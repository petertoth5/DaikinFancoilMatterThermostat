# 012 - `mb_rtu`: pure FC03 RTU frame codec (CRC, request builder, reply classifier)

Phase: 1. Status: spec ready (new 2026-11-24, user decision E2 on the 002 S3 escalation; replaces part (a) of retired 002). Depends on: 001 (host-test runner).
Review: **bus-guarding** (it builds the only request frame and decides how replies are classified). `firmware-reviewer` reviews this task independently before 013 starts.

## Goal
Pure C component, host-testable, with all Modbus RTU knowledge the phase-1 node needs:
1. CRC-16 (Modbus): polynomial 0xA001 (reflected), init 0xFFFF, transmitted low byte first *(external, Modbus over Serial Line spec)*.
2. `mb_rtu_build_fc03`: the **only** request builder; the function code is fixed inside (FC03), never a parameter.
3. Reply classifier: OK + values / EXCEPTION + code / BAD_FRAME / TIMEOUT.
4. Two helpers the port layer needs without knowing Modbus: expected reply length from the first bytes, and the t3.5 silence time for a baud rate.

No esp-modbus (user decision 2026-11-24: E2, no esp-modbus at all; D7 and Q9 reversed). No IDF, FreeRTOS or `esp_log` includes.

## Bus-safety rules (binding)
- Function-code bytes exist only in `components/mb_rtu/src/mb_rtu.c`, as exactly two private macros: `#define MB_RTU_FC_READ_HOLDING 0x03u` and `#define MB_RTU_FC_READ_HOLDING_EXC 0x83u`. Not in the public header. No other hex literal with value 0x03 or 0x83 and no hex literal with a write function-code value (0x05, 0x06, 0x0F, 0x10, 0x15, 0x16, 0x17) anywhere in the component's `include/` and `src/` (lint R8 from 013 checks this; decimal literals are a documented lint limit, do not use them for function codes either).
- No variable or field named `command`, `fc`, `func`, `func_code` or `function_code` is the target of a compound assignment (lint R8, 013). Simple reads such as `uint8_t fc = rx[1];` are fine.
- The builder refuses address 0, addresses > 247 and qty outside 1..4 (defence in depth; the real guard is `mb_link_read`, 014).

## Files / directories owned
- `firmware/components/mb_rtu/` (new): `CMakeLists.txt` (no `REQUIRES`; `-Wall -Wextra -Werror`), `include/mb_rtu.h`, `src/mb_rtu.c`, `test_host/test_mb_rtu.c`
- `firmware/test/host/main/CMakeLists.txt` (append `mb_rtu` to `REQUIRES`)

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "3. Bus-safety rules" (Timing D16: t3.5 about 4.0 ms at 9600 with 11-bit characters, external; Read errors and health D21), "6. Decisions" (D7 as amended 2026-11-24 by wiki-keeper, D21: addresses 1..247)
- `concepts/modbus-rs485-link` (9600 8N1, FC03 read; qty > 1 not stated)
- Not from the wiki, marked external in code comments: frame layout, CRC, exception frame (function code | 0x80), t3.5 rule (fixed 1.75 ms above 19200 baud), all from the Modbus Application Protocol and Modbus over Serial Line specifications.

## Interfaces (`mb_rtu.h`)
```c
#include <stdint.h>
#include <stddef.h>

#define MB_RTU_MAX_QTY        4u
#define MB_RTU_REQ_LEN        8u                               /* addr, fc, reg hi/lo, qty hi/lo, crc lo/hi */
#define MB_RTU_MAX_REPLY_LEN  (5u + 2u * MB_RTU_MAX_QTY)       /* 13 */
#define MB_RTU_LEN_MORE       ((size_t)0)                      /* length not known yet: need more bytes */
#define MB_RTU_LEN_UNKNOWN    SIZE_MAX                         /* not determinable (unknown function byte) */

typedef enum { MB_RTU_OK, MB_RTU_EXCEPTION, MB_RTU_BAD_FRAME, MB_RTU_TIMEOUT } mb_rtu_res_t;

uint16_t mb_rtu_crc16(const uint8_t *buf, size_t len);

/* 8 on success; -1 (out untouched) if addr not 1..247, qty not 1..MB_RTU_MAX_QTY, or out == NULL. */
int mb_rtu_build_fc03(uint8_t addr, uint16_t reg, uint16_t qty, uint8_t out[MB_RTU_REQ_LEN]);

/* Expected length of the reply to an FC03 request of qty registers, from the bytes received so far:
   n < 2 -> MB_RTU_LEN_MORE; rx[1] == FC03 -> 5 + 2*qty (from the request, not from rx[2]);
   rx[1] == FC03 | 0x80 -> 5; any other function byte -> MB_RTU_LEN_UNKNOWN. Does not look at the address. */
size_t mb_rtu_fc03_reply_len(uint16_t qty, const uint8_t *rx, size_t n);

/* Classify a complete received byte sequence (everything the port collected for this transaction).
   *exc_code is set to 0 on entry (if non-NULL) and to the code only for MB_RTU_EXCEPTION;
   val[0..qty-1] (host order) is written only for MB_RTU_OK. */
mb_rtu_res_t mb_rtu_parse_fc03_reply(uint8_t addr, uint16_t qty, const uint8_t *rx, size_t len,
                                     uint16_t *val, uint8_t *exc_code);

/* t3.5 in microseconds: baud <= 19200 -> ceil(3.5 * 11 * 1e6 / baud); baud > 19200 -> 1750 (external). 0 baud -> 0. */
uint32_t mb_rtu_t35_us(uint32_t baud);
```

### Request frame (normative)
`out = { addr, 0x03, reg >> 8, reg & 0xFF, qty >> 8, qty & 0xFF, crc & 0xFF, crc >> 8 }`, `crc = mb_rtu_crc16(out, 6)`. The 0x03 comes from `MB_RTU_FC_READ_HOLDING`.

### Classifier rules (normative, evaluated in this order; the first matching rule decides)
1. `len == 0` -> `MB_RTU_TIMEOUT` (no byte received).
2. `rx == NULL`, `addr` not 1..247 or `qty` not 1..4 -> `MB_RTU_BAD_FRAME` (caller error; never expected).
3. `len < 5` -> `BAD_FRAME` (short).
4. `rx[0] != addr` -> `BAD_FRAME` (wrong address).
5. `rx[1] == FC03 | 0x80` (exception):
   - `len != 5` -> `BAD_FRAME` (trailing bytes);
   - CRC of `rx[0..2]` != `rx[3] | rx[4] << 8` -> `BAD_FRAME`;
   - `rx[2] == 0` -> `BAD_FRAME` (0 is not an exception code and would collide with "no code"; approved 2026-11-24, user decision);
   - else `MB_RTU_EXCEPTION`, `*exc_code = rx[2]` (any code 1..255 is reported as is; 01..04 are the ones the Modbus spec defines for FC03, external).
6. `rx[1] == FC03`:
   - `rx[2] != 2 * qty` -> `BAD_FRAME` (byte-count mismatch, even if the frame is internally consistent);
   - `len < 5 + 2*qty` -> `BAD_FRAME` (short); `len > 5 + 2*qty` -> `BAD_FRAME` (trailing bytes);
   - CRC of `rx[0 .. 2+2*qty]` != last two bytes (low first) -> `BAD_FRAME`;
   - else `MB_RTU_OK`, `val[i] = rx[3+2i] << 8 | rx[4+2i]`.
7. Any other function byte -> `BAD_FRAME` (wrong function code).

## Acceptance checks
1. `firmware/test/host`: `idf.py build` (after `idf.py reconfigure`, the test GLOB is evaluated at configure time), run `build/*.elf`: all tests pass, exit 0.
2. `cd firmware && idf.py build` (esp32s3) passes with zero warnings; the bus-access lint (current 001 rules) passes. The component is not referenced by the image yet (linked from 014/006).
3. `grep -rE '#include *[<"](esp_|freertos|driver/)' firmware/components/mb_rtu` is empty.
4. `grep -rniE '0x0*(3|83|5|6|f|10|15|16|17)[uUlL]*\b' firmware/components/mb_rtu/include firmware/components/mb_rtu/src` lists only the two `#define` lines (report the output).
5. One temporarily broken expectation (e.g. the 0x1241 vector) makes the host run fail; report, revert.

## Test strategy (host, Unity; `test_host/test_mb_rtu.c`)
Every BAD_FRAME fixture carries a **valid CRC** unless the CRC itself is the fault, so each test fails for the stated reason only (build fixtures with `mb_rtu_crc16` in the test, plus the fixed vectors below).
- CRC vectors *(external)*: `"123456789"` -> 0x4B37 (CRC-16/MODBUS check value, CRC catalogue); `02 07` -> 0x1241 (example in the Modbus over Serial Line specification V1.02, CRC section; the architect re-computed it by hand; the implementer cites the section if the PDF is at hand, else marks it "external, not re-checked against the PDF"); empty buffer -> 0xFFFF; residue: CRC over a frame including its two CRC bytes (low first) == 0.
- Builder: addr 1 reg 0 qty 1 -> bytes 0..5 `01 03 00 00 00 01` and bytes 6..7 = CRC low/high; reg 0x00F2 (242) qty 4 -> `.. 00 F2 00 04 ..`; addr 247 accepted; addr 0, 248, 255, qty 0, qty 5 -> -1 with `out` unchanged (pre-filled pattern); `out == NULL` -> -1.
- Reply length: n = 0, 1 -> MORE; fc 0x03 with qty 1..4 -> 7, 9, 11, 13 (independent of a wrong `rx[2]`); fc 0x83 -> 5; fc 0x04, 0x06, 0x90 -> UNKNOWN.
- Classifier OK: qty 1, 2, 3, 4 with distinct values (incl. 0x0000, 0xFFFF, 0x8000), values in host order; `exc_code` is 0 after OK.
- EXCEPTION: codes 01, 02, 03, 04 -> EXCEPTION with that code, `val` untouched (pre-filled pattern).
- BAD_FRAME, one test each: bad CRC (flip one data bit; separately flip one CRC byte) for normal and exception frames; wrong address (normal and exception); wrong function code (0x04, 0x06, 0x84 with valid CRC); byte-count mismatch (qty 1 request, valid-CRC reply with byte count 4 and 4 data bytes); short (lengths 1..4, and a qty-2 reply missing its last byte); trailing bytes (valid qty-1 frame + 1 byte; valid exception frame + 1 byte); exception code 0x00; `val`/`exc_code` behaviour as specified on each.
- TIMEOUT: `len == 0` (also with `rx == NULL`).
- t3.5: 9600 -> 4011, 19200 -> 2006, 1200 -> 32084, 38400 -> 1750, 115200 -> 1750, 0 -> 0.

## Recommended implementer model
sonnet: small and pure, but bus-guarding; the tests must build valid-CRC fault frames so each case fails for one reason only, and the rule order (length before CRC, byte count from the request) is easy to get subtly wrong. haiku would be acceptable only for the code; the fixture discipline is the reason for sonnet.

## Out of scope
- Any UART, timing or I/O (014); the end-of-frame rule (014).
- Other function codes, writes (phase 3), broadcast, ASCII mode, t1.5 checking.
- Lint changes (013).
