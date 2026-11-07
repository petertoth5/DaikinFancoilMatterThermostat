# 003 - `rtu_sniff`: passive Modbus RTU frame splitter and decoder (pure)

Phase: P1. Status: spec ready. Depends on: 002 (CRC adapter and the host-CRC route H0/H1 chosen there).

## Goal
Pure C module that turns timestamped RX byte chunks (as delivered by `rs485_rx`, task 005) into classified Modbus RTU frames, using the idle-gap flag and the documented frame shapes of FC 03 and FC 06, validated by `rtu_crc_frame_ok()` (esp-modbus `mb_crc16`, D7a). Plus decode helpers for the documented frame types. No IDF, FreeRTOS or hardware calls.

## Zero-TX rules (P1/P2, binding)
- This module is receive-side only and must not contain any transmit, UART or esp-modbus controller call.
- Forbidden everywhere outside `components/bus_tx/`: `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers. `mbcrc.h` only in `src/rtu_crc.c` (task 002). Enforced by the build lint.
- Hardware stays DE and /RE to GND, DI unconnected; UART stays `UART_MODE_UART`, RX only (task 005).

## Files / directories owned
- `firmware/components/rtu_sniff/include/rtu_sniff.h` (new)
- `firmware/components/rtu_sniff/src/rtu_sniff.c` (new)
- `firmware/components/rtu_sniff/test_host/test_rtu_sniff.c` (new)
- `firmware/components/rtu_sniff/CMakeLists.txt` (only: add `src/rtu_sniff.c` to SRCS)

## Wiki pages relied on
- `concepts/firmware-architecture` (sections "rtu_sniff", "Timing estimates")
- `concepts/modbus-rs485-link` (9600 8N1; FC 03 read, FC 06 write are the only documented codes)
- `concepts/device-addressing` (addresses 1-255)

External (Modbus Application Protocol / Serial Line specification, not in the wiki; marked external in the architecture): frame = address, function code, data, CRC (low byte first); FC03 request 8 bytes (addr, 0x03, start hi/lo, count hi/lo, CRC); FC03 response 5 + N bytes (addr, 0x03, byte count N, N data bytes big-endian per register, CRC), N even, N <= 250; FC06 request and normal response both 8 bytes and identical (addr, 0x06, reg hi/lo, value hi/lo, CRC); exception response 5 bytes (addr, fc | 0x80, code, CRC); max RTU frame 256 bytes; address 0 = broadcast (no response); 8N1 = 10 bit times per character.

## Interfaces
```c
#define RTU_MAX_FRAME 256
#define RTU_SNIFF_MAX_LEAD_SKIP 2   /* bytes of leading garbage tolerated before a frame at an idle boundary */

typedef enum {
    RTU_FRAME_FC03_REQ, RTU_FRAME_FC03_RESP, RTU_FRAME_FC06,      /* FC06: request or echo, undecidable here */
    RTU_FRAME_EXCEPTION, RTU_FRAME_OTHER_FC,                      /* OTHER_FC: CRC ok, undocumented function code */
    RTU_FRAME_CRC_ERROR, RTU_FRAME_OVERSIZE
} rtu_frame_kind_t;

typedef struct {
    rtu_frame_kind_t kind;
    int64_t t_end_us;            /* estimated end of last byte (see timing rule) */
    uint16_t len;
    uint8_t bytes[RTU_MAX_FRAME];
} rtu_frame_t;

typedef void (*rtu_frame_cb_t)(const rtu_frame_t *frame, void *ctx);

typedef struct { uint32_t frames[7]; uint32_t dropped_bytes; uint32_t skipped_lead_bytes; uint32_t discontinuities; } rtu_sniff_stats_t;

typedef struct rtu_sniff rtu_sniff_t;   /* define the struct in the header so callers can allocate it statically; no malloc */

void rtu_sniff_init(rtu_sniff_t *s, uint32_t baud, uint8_t idle_chars, rtu_frame_cb_t cb, void *ctx);
void rtu_sniff_feed(rtu_sniff_t *s, const uint8_t *data, uint16_t len, int64_t t_us, bool idle_after, bool discontinuity_before);
void rtu_sniff_get_stats(const rtu_sniff_t *s, rtu_sniff_stats_t *out);
void rtu_sniff_reset_stats(rtu_sniff_t *s);
uint32_t rtu_char_time_us(uint32_t baud);      /* 10 bit times (8N1), rounded to nearest us */

/* Decode helpers: return false if kind/shape does not match. Register values big-endian on the wire. */
bool rtu_decode_fc03_req(const rtu_frame_t *f, uint8_t *addr, uint16_t *start, uint16_t *count);
bool rtu_decode_fc03_resp(const rtu_frame_t *f, uint8_t *addr, uint16_t *n_regs, uint16_t *values, uint16_t max_values);
bool rtu_decode_fc06(const rtu_frame_t *f, uint8_t *addr, uint16_t *reg, uint16_t *value);
bool rtu_decode_exception(const rtu_frame_t *f, uint8_t *addr, uint8_t *fc, uint8_t *code);
```
Behaviour (normative):
1. `discontinuity_before` (UART overflow / error reported by `rs485_rx`): drop buffered bytes (count `dropped_bytes`, `discontinuities`), then process the chunk.
2. Append the chunk to the buffer. Extraction loop at buffer start: candidate lengths by `bytes[1]`: 0x03 -> 8 and, if `bytes[2]` is even and 2..250, `5 + bytes[2]`; 0x06 -> 8; `bytes[1] & 0x80` -> 5. Try candidates in ascending length; the first whose bytes are all present and pass `rtu_crc_frame_ok` is emitted (kind by shape: 0x03 len 8 = `FC03_REQ`, else `FC03_RESP`) and removed; repeat. If no candidate can be complete yet, wait for more bytes.
3. At `idle_after == true` (end of an idle-delimited block): run step 2 once more; for a non-empty remainder try leading skips of 1..`RTU_SNIFF_MAX_LEAD_SKIP` bytes (count `skipped_lead_bytes` if one leads to an emitted frame and continue extraction); else if the whole remainder passes `rtu_crc_frame_ok` emit `OTHER_FC`; else emit `CRC_ERROR` with the remainder bytes. Clear the buffer.
4. Buffer growing past `RTU_MAX_FRAME` without an extractable frame: emit `OVERSIZE` (first `RTU_MAX_FRAME` bytes), drop the rest until the next idle boundary.
5. Timing rule: `c = rtu_char_time_us(baud)`; the last byte of a chunk ends at `t_us - (idle_after ? idle_chars * c : 0)`; a frame followed by k further bytes in the same chunk ends k * c earlier. Callers derive the start as `t_end_us - len * c`.
6. The callback is invoked synchronously from `rtu_sniff_feed`; `frame` is valid only during the call.

## Acceptance checks
1. Target build and lint pass; host tests pass (exit 0).
2. No `#include` of IDF/FreeRTOS headers in `rtu_sniff.h`/`rtu_sniff.c` (grep in the report).
3. No dynamic allocation.

## Test strategy
Host unit tests (Unity, `linux`) in `test_host/test_rtu_sniff.c`. Build test frames with a helper that appends `rtu_crc16()` low byte first (the CRC itself is covered by 002). Required cases:
- Each kind from a single idle-terminated chunk: FC03 request, FC03 response (1 and 2 registers), FC06, exception (FC 0x83 and 0x86), OTHER_FC (e.g. FC 0x10 with valid CRC), CRC_ERROR.
- Request and response concatenated in one chunk without idle (task latency case) -> two frames.
- A frame split over 2 and over 3 chunks (`idle_after` only on the last).
- Leading garbage of 1 and 2 bytes before a valid frame at an idle boundary -> frame emitted, `skipped_lead_bytes` counted; 3 bytes -> CRC_ERROR.
- FC03 response with odd byte count -> not a response.
- Oversize: 300 bytes of noise without idle -> one OVERSIZE, then recovery on the next idle-delimited valid frame.
- `discontinuity_before` drops a partial frame.
- Timing: with baud 9600 (`c` = 1042 us) and idle_chars 3, `t_end_us` of single and concatenated frames match the rule exactly.
- Decode helpers: field extraction incl. big-endian values 0x0000, 0x7FFF, 0x8000, 0xFFFF; addr 0 and 255; wrong-kind returns false.
- Stats counters and reset.

## Recommended implementer model
sonnet: a stateful splitter with several edge cases and an exact timing rule; needs careful test design.

## Out of scope
- Request/response pairing, register mirror, timing statistics (004).
- Any UART access (005), printing/console (006).
- Function codes other than 03/06 beyond OTHER_FC classification.
- Inter-character (1.5 char) gap detection (not observable through the UART driver; documented limitation).
