# 006 - `bus_task` glue, diagnostics console, `app_main`

Phase: P1. Status: spec ready. Depends on: 003, 004, 005.

## Goal
Wire the P1 pipeline on the target: `rs485_rx` -> `rtu_sniff` -> `bus_monitor`, in one high-priority task that is the **single owner of the bus UART**, plus an `esp_console` REPL with diagnostic commands and a raw-chunk dump in the replay-fixture format, so the user can do the P1 measurements (008). `app_main` starts both.

## Zero-TX rules (P1/P2, binding)
- Hardware: MAX485-M DE and /RE hardwired to GND, DI unconnected.
- The bus UART is touched only through `rs485_rx` (`UART_MODE_UART`, RX pin only, TX/RTS/CTS = `UART_PIN_NO_CHANGE`). No console command may transmit on the bus or change UART mode/pins.
- Forbidden outside `components/bus_tx/`: `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers. Console output goes through `printf`/`esp_console` on the console port, never on the bus UART. Build lint and ELF check must pass unchanged.
- No esp-modbus controller instance (ELF check from 002).

## Files / directories owned
- `firmware/components/bus_task/` (new): `CMakeLists.txt` (`REQUIRES rs485_rx rtu_sniff bus_monitor console esp_timer freertos`, `-Wall -Wextra -Werror`), `include/bus_task.h`, `src/bus_task.c`, `src/bus_console.c`
- `firmware/components/bus_fmt/` (new, pure; see Test strategy)
- `firmware/main/main.c`, `firmware/main/CMakeLists.txt` (REQUIRES `bus_task`)
- `firmware/test/host/main/CMakeLists.txt` (add `bus_fmt` to `REQUIRES`)

## Wiki pages relied on
- `concepts/firmware-architecture` (sections "Threading model", "Console commands", "Replay fixture format", "P1 measurement goals")
- `concepts/register-map` (names/multipliers via `daikin_regs`), `concepts/flag-registers` (flag registers printed in hex)
- `concepts/bus-topology` (open items P1 answers)

## Interfaces
```c
typedef struct { rs485_rx_config_t rx; int64_t resp_timeout_us; } bus_task_config_t;
esp_err_t bus_task_start(const bus_task_config_t *cfg);
/* Thread-safe snapshot accessors for the console (and later P2): copy under the monitor mutex. */
void bus_task_get_stats(rs485_rx_stats_t *rx, rtu_sniff_stats_t *sniff, bus_monitor_stats_t *mon);
size_t bus_task_copy_regs(bus_reg_entry_t *out, size_t max);
esp_err_t bus_task_request(bus_task_cmd_t cmd, uint32_t arg);   /* RESET_STATS, RESET_ALL, RX_RESTART(n) - executed by bus_task itself */
void bus_task_set_dump(bool chunks, bool frames);
```
Threading (normative):
- `bus_task`: priority 20, pinned to core 1, static stack (start at 4096, report the high-water mark). Loop: `rs485_rx_read(&chunk, 100)`; on data `rtu_sniff_feed`; frame callback -> `bus_monitor_on_frame` under the mutex; every loop `bus_monitor_tick(esp_timer_get_time())`; then execute at most one pending console command. Only this task calls `rs485_rx_*` (single UART owner; P3's `bus_tx` will run in this context too).
- Dump/print path must never block `bus_task`: chunk and frame dump lines go into a FreeRTOS queue or ring buffer drained by a low-priority `bus_dump` task (priority 2); when full, drop and count `dump_dropped`.
- Monitor mutex held only for copy operations; printing happens after release.
- `main.c`: build `bus_task_config_t` from Kconfig (`CONFIG_DMT_BUS_*`), `#error` if the console is a UART with the same number as `CONFIG_DMT_BUS_UART_PORT`, start `bus_task`, start the REPL (UART or USB-Serial-JTAG according to the console Kconfig, as in IDF console examples). No NVS, Wi-Fi or Matter.

Console commands (all read-only with respect to the bus):
- `bus status` - config (port, RX GPIO, baud, idle chars, timeout), uptime, `rs485_rx` stats, `rtu_sniff` stats, monitor counters, `dump_dropped`, bus_task stack high-water mark.
- `bus regs` - mirror table: addr, reg (3 digits), name or `undocumented`, raw dec and hex, scaled value for 0.1-multiplier registers (signed for `signed_inferred`, marked `(inferred)`), flags in hex/binary, age ms, reads, writes, min/max interval ms, last source.
- `bus timing` - gap, idle_gap and latency: count, min, avg, max, histogram.
- `bus addrs` - slave addresses seen with request counts; exception table.
- `bus dump chunks on|off`, `bus dump frames on|off`.
- `bus reset [all]` - stats (and mirror with `all`).
- `bus rx restart [n]` - `n` (default 1, max 1000) stop/start cycles executed by `bus_task`; prints free heap before/after and the result codes.

Dump line formats (normative, shared with 007):
- Chunk: `RX <t_us> <idle 0|1> <disc 0|1> <hex bytes, lowercase, space separated>`
- Frame: `FR <t_end_us> <kind> <hex bytes>` (kind = enum name without the `RTU_FRAME_` prefix)

## Acceptance checks
1. Target build passes, zero warnings in owned code; lint and ELF check pass.
2. `idf.py size` reported; no Wi-Fi/Matter components linked.
3. Report shows the console command list (`help` output cannot be captured without a board: list the registered commands from the code instead).
4. Review checklist in the report: single UART owner respected (grep `rs485_rx_` callers), no blocking printing in `bus_task`, mutex scope minimal.
5. HIL behaviours are verified by the user through 008, not here.

## Test strategy
- Glue and console: target-only, no host tests (thin layer over host-tested 003/004).
- The dump line formatters are pure and host-tested, because 007 parses the same format: put them in a small pure component `firmware/components/bus_fmt/` (owned by this task; `REQUIRES rtu_sniff`; `include/bus_fmt.h` with `size_t bus_fmt_chunk_line(char *buf, size_t n, int64_t t_us, bool idle, bool disc, const uint8_t *data, uint16_t len)` and `size_t bus_fmt_frame_line(char *buf, size_t n, const rtu_frame_t *f)`; `test_host/test_bus_fmt.c`), add `bus_fmt` to the host-test `REQUIRES`. Test: exact output strings, a 256-byte chunk, truncation when `n` is too small (returns 0, buffer NUL-terminated).

## Recommended implementer model
sonnet: FreeRTOS task/queue/mutex design with a non-blocking dump path and the IDF console API.

## Out of scope
- Any transmit or polling, Matter, Wi-Fi, NVS, OTA.
- Persisting captures to flash.
- Parsing dumps (007).
