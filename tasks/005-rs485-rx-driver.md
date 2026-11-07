# 005 - `rs485_rx`: RX-only UART driver for the bus tap (hardware layer)

Phase: P1. Status: spec ready. Depends on: 001. Independent of 003/004.

## Goal
Thin hardware component that installs the IDF UART driver on the bus UART in **receive-only** configuration, delivers timestamped byte chunks with an "idle after" flag (hardware RX timeout) and a "discontinuity" flag (overflow / line errors), counts UART events, and supports a clean **stop / start** (driver uninstall and reinstall) so P3 can time-share the UART with esp-modbus (D7b = U2).

## Zero-TX rules (P1/P2, binding; this task is where they are implemented)
- Hardware (documented in 008, assumed here): MAX485-M **DE and /RE hardwired to GND, DI unconnected**; RO reaches the RX GPIO through the level/ground fix from the bring-up checklist. Even if the UART transmitted, the transceiver driver stays disabled.
- `uart_driver_install(port, rx_buf, 0 /* tx_buffer_size */, queue_len, &queue, flags)`.
- Mode stays the driver default `UART_MODE_UART`; **`uart_set_mode` is never called**.
- Exactly one pin call in the whole code base, here: `uart_set_pin(port, UART_PIN_NO_CHANGE, rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)` (5-argument form; TX, RTS, CTS untouched). `uart_config_t.flow_ctrl = UART_HW_FLOWCTRL_DISABLE`.
- Forbidden here and everywhere outside `components/bus_tx/`: `mbc_master_*`, `mbc_slave_*`, `uart_write_bytes*`, `uart_tx_chars`, `uart_set_mode`, `uart_set_rts`, `uart_set_dtr`, `uart_set_loop_back`, `_uart_set_pin4/6`, esp-modbus controller headers. The build lint (001) must pass unchanged; do not weaken it.
- No esp-modbus usage at all in this component.

## Files / directories owned
- `firmware/components/rs485_rx/` (new): `CMakeLists.txt` (`REQUIRES esp_driver_uart esp_timer`, `-Wall -Wextra -Werror`), `include/rs485_rx.h`, `src/rs485_rx.c`

## Wiki pages relied on
- `concepts/firmware-architecture` (sections "rs485_rx", "Zero-TX by construction", "Threading model", "P3 outline (U2 time-share)")
- `concepts/modbus-rs485-link` (9600 8N1)
- `entities/max485` (DE/RE, RO 5 V level), `entities/esp32-s3-wroom-1`
- `concepts/bus-topology` (tap on "+ A B -", D1)

External (ESP-IDF v6.0.2 `esp_driver_uart/include/driver/uart.h`, fetched 2026-11-07): `uart_event_t` has `type`, `size`, `timeout_flag`; `uart_set_rx_timeout(port, tout_thresh)` in UART symbol periods (max 126, 0 = off); `uart_set_always_rx_timeout(port, bool)` exists (esp-modbus 2.1.4 enables it too); `uart_set_pin` is a variadic macro (5 args -> `_uart_set_pin4`); `UART_PIN_NO_CHANGE` = -1.

## Interfaces
```c
#define RS485_RX_CHUNK_MAX 256
typedef struct { int uart_port; int rx_gpio; uint32_t baud; uint8_t idle_chars; } rs485_rx_config_t;
typedef struct {
    int64_t t_us;               /* esp_timer_get_time() when the data event was taken from the queue */
    uint16_t len;
    bool idle_after;            /* event had timeout_flag (line idle for idle_chars symbols) */
    bool discontinuity_before;  /* overflow, buffer full, frame/parity error or break since the previous chunk */
    uint8_t data[RS485_RX_CHUNK_MAX];
} rs485_rx_chunk_t;
typedef struct { uint32_t data_events, bytes, fifo_ovf, buffer_full, frame_err, parity_err, breaks, other, starts, stops; } rs485_rx_stats_t;

esp_err_t rs485_rx_start(const rs485_rx_config_t *cfg);   /* ESP_ERR_INVALID_STATE if running; INVALID_ARG if port is the console UART */
esp_err_t rs485_rx_stop(void);                             /* uart_driver_delete; idempotent (ESP_OK if stopped) */
esp_err_t rs485_rx_read(rs485_rx_chunk_t *out, uint32_t timeout_ms);  /* ESP_OK with len > 0, or ESP_ERR_TIMEOUT */
void rs485_rx_get_stats(rs485_rx_stats_t *out);
void rs485_rx_reset_stats(void);
bool rs485_rx_is_running(void);
```
Rules:
- Single owner: only `bus_task` (006) calls these functions; no internal locking beyond what is needed for `get_stats` from another task (stats copy under a critical section or atomic reads).
- `start`: `uart_param_config` (baud from cfg, 8N1, no flow control) -> `uart_set_pin` (as above) -> `uart_driver_install` (RX buffer >= 2048, TX 0, queue >= 32, IRAM ISR per `CONFIG_UART_ISR_IN_IRAM`) -> `uart_set_rx_timeout(port, idle_chars)` -> `uart_set_always_rx_timeout(port, true)`. Check every return value; on failure undo what was done and return the error.
- `read`: wait on the event queue; for `UART_DATA` take the timestamp first, then `uart_read_bytes` up to `size` without waiting; set flags. For `UART_FIFO_OVF` / `UART_BUFFER_FULL`: `uart_flush_input` + `xQueueReset`, count, mark the next chunk discontinuous. Frame/parity/break: count, mark discontinuous. Keep looping until data or timeout.
- `stop` after `start` must release everything (driver, queue) so that `start` works again and the heap returns to its previous level.
- Console guard in `start`: if the console is on a UART (`CONFIG_ESP_CONSOLE_UART`) and `CONFIG_ESP_CONSOLE_UART_NUM == cfg->uart_port` -> `ESP_ERR_INVALID_ARG` (bus and console must not share a UART). Task 006 adds the matching compile-time `#error`.

## Acceptance checks
1. Target build passes with zero warnings in the component; lint passes; the only `uart_set_pin` call in the tree is here and matches the rule.
2. `grep` in the report: no `uart_write_bytes`, `uart_set_mode`, `tx_io_num` other than `UART_PIN_NO_CHANGE`, no esp-modbus include in `components/rs485_rx/`.
3. Code review checklist in the report: every IDF call's return value checked; no allocation in the read path; timestamp taken before `uart_read_bytes`.
4. Behaviour on hardware cannot be checked (no board). The HIL steps are in 008: RX of live traffic, stats counters, `bus rx restart 100` with heap before/after equal and RX resuming.

## Test strategy
- Hardware layer: not host-tested (UART driver unavailable on `linux`). Kept minimal so that all logic worth testing lives in 003/004.
- Target-side verification is the HIL checklist (008). Optionally add a Unity test for esp32s3 that runs `start`/`stop` 100 times on an unconnected GPIO and checks free heap; it is built but not run until a board is attached.

## Recommended implementer model
sonnet: IDF UART driver details (event handling, overflow recovery, clean uninstall) and the zero-TX constraints need care; small but safety-relevant.

## Out of scope
- Frame splitting/decoding (003), statistics (004), task creation and console (006).
- Any transmit path, RS485 mode, DE control (P3 `bus_tx`).
- Pin choice (Kconfig default is a placeholder; the user confirms it in 008).
