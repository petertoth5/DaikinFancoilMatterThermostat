# 007 - Replay fixtures and end-to-end host test (`rtu_sniff` + `bus_monitor`)

Phase: P1. Status: spec ready. Depends on: 003, 004, 006 (dump format and `bus_fmt`).

## Goal
A host-side replay harness that reads capture files in the `RX ...` dump format (006), feeds them chunk by chunk into `rtu_sniff` -> `bus_monitor`, and asserts on the resulting mirror and statistics. Ship synthetic fixtures now; real P1 captures pasted from `idf.py monitor` logs must work as fixtures later without conversion.

## Zero-TX rules (P1/P2, binding)
- Host-only test code; nothing here is linked into the firmware. It must still not introduce any transmit/controller identifiers (`mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, ...), so that test code can never be copied into firmware with a TX path.
- Firmware rules unchanged: DE and /RE to GND, DI unconnected, `UART_MODE_UART`, RX only, no esp-modbus controller.

## Files / directories owned
- `firmware/test/fixtures/` (new): `README.md` (format, provenance rule), `synthetic_*.rtu.txt`
- `firmware/test/host/main/replay.h`, `firmware/test/host/main/replay.c` (fixture loader, host-only)
- `firmware/test/host/main/test_replay.c`
- `firmware/test/host/main/CMakeLists.txt` (only: add `replay.c` to the sources)

## Wiki pages relied on
- `concepts/firmware-architecture` (sections "Replay fixture format", "Test strategy")
- `concepts/register-map`, `concepts/device-addressing` (registers and addresses used in synthetic frames)
- `concepts/modbus-rs485-link` (FC 03/06, 9600 8N1)

## Interfaces
Fixture format (normative, = 006 dump):
```
# dmt-capture v1
# baud=9600 idle_chars=3 source=synthetic|capture origin=<free text>
RX <t_us> <idle 0|1> <disc 0|1> <hex bytes>
```
- Lines not starting with `RX ` are ignored (so a raw `idf.py monitor` log with log prefixes on other lines can be used as is); a `RX` line with an ESP log prefix in front is also accepted (match `RX ` anywhere, parse from there).
- Header lines `# baud=` and `idle_chars=` are optional; defaults 9600 / 3.
- Loader API (host only): `int replay_run(const char *path, rtu_sniff_t *s)`; returns the number of chunks fed, or -1 on a malformed `RX` line (report line number on stderr). Fixture paths come from the `FIXTURE_DIR` compile definition (001).

Provenance rule (write it into `fixtures/README.md`): synthetic fixtures must say `source=synthetic` and use only documented function codes (03, 06), documented registers from `concepts/register-map`, address 1 (wiki default) unless the test is about addresses; real captures say `source=capture` with date and setup and are copied unmodified from the log. Synthetic frames are not bus facts.

Synthetic fixtures (minimum):
1. `synthetic_poll_cycle.rtu.txt`: 3 cycles of FC03 reads of 000 (T1), 008 (SP), 105 (ALR_STAT) from address 1 with responses, realistic gaps (e.g. 20-200 ms; arbitrary, label as such).
2. `synthetic_write.rtu.txt`: FC06 write of 231 with echo, then an FC03 read-back of 231.
3. `synthetic_merged_chunks.rtu.txt`: request and response in one chunk (`idle 0`), and a frame split over chunks.
4. `synthetic_errors.rtu.txt`: one CRC error, one exception response (FC 0x83), one unanswered request, one `disc 1` chunk.
CRC bytes must be correct; compute them with `rtu_crc16` (e.g. a throwaway host program that is not committed) or a standard CRC-16/MODBUS tool, and let the tests prove them (every frame that is meant to be valid must decode as valid).

## Acceptance checks
1. Host tests pass (exit 0); target build and lint unaffected.
2. A log excerpt with ESP log prefixes around `RX` lines (add one small `synthetic_logwrapped.rtu.txt`) loads and gives the same result as the clean version.
3. Malformed line -> `replay_run` returns -1 with line number.

## Test strategy
Host (Unity, `linux`), in `test_replay.c`:
- Per fixture, assert exact mirror content (addr, reg, value, source, read/write counts) and the relevant counters (matched, unanswered, crc_errors, exceptions, discontinuities).
- Timing: idle_gap and latency stats for `synthetic_poll_cycle` within +-1 character time of the values constructed in the file.
- Loader unit tests: header parsing, ignored lines, log-prefixed lines, malformed hex, odd number of hex digits, more than 256 bytes on one line (reject).

## Recommended implementer model
sonnet: mostly mechanical, but fixture authoring with correct CRCs and exact expected statistics needs care; haiku would likely get the timing expectations wrong.

## Out of scope
- Real captures (they come from the user after 008 and are added later as plain files).
- Any firmware changes; changes to the dump format (owned by 006).
