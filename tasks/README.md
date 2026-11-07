# Task specs

Written by `architect` from the architecture approved by the user on 2026-11-07 (wiki: `concepts/firmware-architecture`). Implement one task per `firmware-dev` pass, in order; after each: `build-verifier` + `firmware-reviewer`.

Gate: only **P1 (listen-only)** tasks are released (D2 exception approved by the user). P2 (Matter read-only mirror) specs follow after P1 is done; P3 (guarded TX, U2 time-share) is specified only after the P1 measurements.

| # | Task | Depends on | Model | Status |
|---|---|---|---|---|
| 001 | [Skeleton, Kconfig, zero-TX lint, host-test runner](001-skeleton-kconfig-lint.md) | - | sonnet | spec ready |
| 002 | [esp-modbus `==2.1.4` pin, `rtu_crc` over `mb_crc16`, guards, build spike](002-esp-modbus-crc-adapter-spike.md) | 001 | sonnet | spec ready |
| 003 | [`rtu_sniff` frame splitter/decoder (pure)](003-rtu-sniff-decoder.md) | 002 | sonnet | spec ready |
| 004 | [`bus_monitor` pairing, register mirror, stats (pure)](004-bus-monitor.md) | 003 | sonnet | spec ready |
| 005 | [`rs485_rx` RX-only UART driver, stop/start](005-rs485-rx-driver.md) | 001 | sonnet | spec ready |
| 006 | [`bus_task` glue, console, `bus_fmt`, `app_main`](006-bus-task-console.md) | 003, 004, 005 | sonnet | spec ready |
| 007 | [Replay fixtures + end-to-end host test](007-replay-fixtures.md) | 003, 004, 006 | sonnet | spec ready |
| 008 | [P1 bring-up HIL checklist + measurement log (document for the user)](008-p1-bringup-hil-checklist.md) | 006 | sonnet | spec ready |

Notes
- 005 may run in parallel with 003/004. 008 may be drafted in parallel and finalised after 006.
- The esp-modbus build spike of the v0.2 outline is merged into 002 because esp-modbus is a P1 dependency now (D7a = B) and the host-CRC route decided there determines how 003 is tested.
- The v0.2 task "RO fan-out check" was dropped (D7b = U2).
- If 002 finds that `mb_crc16` cannot be built for host tests (H0 and H1 fail), stop: the fallback (F1/F2) is a user decision.

## Zero-TX rules (P1/P2), restated in every bus-related spec
1. Hardware: MAX485-M DE and /RE hardwired to GND, DI unconnected.
2. Bus UART in `UART_MODE_UART` (never `uart_set_mode`), RX pin only: the single `uart_set_pin` call lives in `rs485_rx` with TX/RTS/CTS = `UART_PIN_NO_CHANGE`; TX buffer 0.
3. Forbidden outside `components/bus_tx/` (absent in P1/P2): `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`; architect additions: `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, includes of esp-modbus controller headers. `mbcrc.h` only in `rtu_sniff/src/rtu_crc.c`. Enforced by `tools/check_zero_tx.py` on every build.
4. esp-modbus is linked only for `mb_crc16`; no controller instance; a post-build ELF check fails if controller symbols are in the image.
