# 014 - `rs485_port` (UART, DE, silence check, FC03 transaction) and `mb_link` read path over own RTU codec

Phase: 1. Status: spec ready (new 2026-11-24, user decision E2 on the 002 S3 escalation; replaces part (b) of retired 002; amended 2026-11-24, user decision: no fake-port host test, seam rule with own `mb_link.h` types, known 100 Hz limit, fallback E1b documented; amended 2026-11-24, reviewer minor on 012: reply-length adapter bounds, non-NULL `val`, acceptance 3k). Depends on: 009 (read whitelist), 012 (`mb_rtu`), 013 (lint allows `uart_write_bytes` in `rs485_port/src/`; R8 for FC bytes).
Review: **bus-touching** (first code that transmits). `firmware-reviewer` reviews this task independently, including the IDF evidence list, before 006 starts.

## Goal
1. `rs485_port` (HW): owns the bus UART and the DE GPIO: receive-hold, silence check (D17), open/close in RS485 half-duplex with DE via RTS (D8), and one blocking request/reply transaction with the end-of-frame rule below. Modbus-agnostic: no function codes, no CRC.
2. `mb_link` (logic over `rs485_port` + `mb_rtu`): the only hardware-facing interface above the port; API of retired spec 002 unchanged (user decision 2026-11-24): guards, build, transact, classify. Not host-tested (D12 unchanged: `mb_link` is not built for linux); the refusal order is verified by code inspection (amended 2026-11-24, user decision).

No esp-modbus (E2; D7 and Q9 reversed, user decision 2026-11-24). E1b (esp-modbus) is a documented, not implemented fallback; see "Fallback E1b" below (amended 2026-11-24, user decision).

## Seam rule (normative; amended 2026-11-24, user decision)
`mb_link.h` and its result semantics (`mb_link_res_t`, `exc_code`, counters) are the **single seam** between the link layer and everything above it. Nothing outside `components/mb_link/` and `components/rs485_port/` may include `mb_rtu.h` or `rs485_port.h`, use their types, constants or functions, or rely on how `mb_link` frames, times or classifies (e.g. no `MB_RTU_*` constant, no `rs485_port_*` type in `bus_task`). `mb_link.h` therefore declares its own types and does not include `rs485_port.h`. Reason: a revert to E1b must touch only `components/mb_link/src/` (plus the lint/ELF/build-config items listed in "Fallback E1b").

## Bus-safety rules (binding; architecture section 3, as amended 2026-11-24)
- FC03 only: the request bytes come only from `mb_rtu_build_fc03` (012). Inside `mb_link_read`, **before any `rs485_port` call**: `val`/`exc_code` non-NULL, `addr` in 1..247 (never 0/broadcast, D21) and `daikin_regs_read_allowed(reg, qty)` (qty 1..4); else `MBL_REFUSED` (addr/whitelist) or `MBL_ERROR` (NULL) without touching the port.
- One transaction at a time, no internal retries (retry policy is `poll_sched`'s, D21). `rs485_port_transact` accepts exactly 8 TX bytes in phase 1 (`ESP_ERR_INVALID_ARG` otherwise, no bus access).
- Transceiver in receive (DE low) whenever no request is being transmitted: DE GPIO is a plain GPIO output at level 0 whenever the UART driver is not installed (boot, silence check, after close); while installed, DE is the UART RTS signal, low except while our 8 bytes are shifted out. 10 kOhm pull-down covers reset/boot (D8).
- DE order rules (user-approved 2026-11-24, from the 002 spike S5, `docs/spikes/002-esp-modbus.md`): `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)` **before** `uart_set_pin` (a routed RTS in plain UART mode sits high); after `uart_driver_delete` and in every hold: `gpio_set_level(de, 0)` **before** the output is enabled (`gpio_output_enable` via `gpio_set_direction`; IDF `esp_driver_gpio/src/gpio.c:228` resets the matrix to the GPIO register and enables the output, so a stale level 1 would drive DE high). Never call `gpio_reset_pin` on DE (it enables the pull-up).
- Safe fallback on a port fault (short write, `uart_wait_tx_done` timeout, read error): `rs485_port` closes itself (driver delete, hold receive) and returns an error; `mb_link` marks itself not initialised; every later `mb_link_read` returns `MBL_ERROR` without bus access until `mb_link_init` (006: only via `mb silence` after a clean check, or reboot). No automatic re-open.
- No logging in either component: no `ESP_LOG*`, no `ESP_ERROR_CHECK`, no `abort`/`assert` (`_Static_assert` is fine). Failures are return values and counters; `bus_task`/`txn_log` report them (no log line per failed read; 006 rule "no printing from `bus_task` except via the ring buffer").
- Only `bus_task` (006) calls `mb_link_*` (after `app_main`'s first hold call); only `mb_link` calls `rs485_port_*`.

## Files / directories owned
- `firmware/components/rs485_port/` (new): `CMakeLists.txt` (`REQUIRES` none public; `PRIV_REQUIRES esp_driver_uart esp_driver_gpio esp_timer freertos`), `include/rs485_port.h`, `src/rs485_port.c`
- `firmware/components/mb_link/` (new): `CMakeLists.txt` (no public `REQUIRES`; `PRIV_REQUIRES rs485_port daikin_regs mb_rtu`; amended 2026-11-24, user decision: seam rule), `include/mb_link.h`, `src/mb_link.c`
- No host-test files (amended 2026-11-24, user decision: the fake-`rs485_port` host test is dropped; `firmware/test/host/` is not touched by this task)

No `idf_component.yml`, no new dependency. All components `-Wall -Wextra -Werror`. Do not touch `main/` (006 owns it); both components are compiled in the target build because IDF builds every project component (verify in the build log).

## Wiki pages relied on
- `concepts/firmware-architecture`, sections "2. Components and threading" (Modbus layer as amended 2026-11-24), "3. Bus-safety rules" (Timing D16, D17, D21, Fallbacks), "4. Hardware wiring" (DE + /RE tied to RTS, 10 kOhm pull-down, RO level shift), "6. Decisions"
- `concepts/modbus-rs485-link` (9600 8N1), `concepts/device-addressing`, `entities/max485` (DE high = transmit, /RE low = receive, datasheet p7/p10; RO 5 V output)
- `docs/spikes/002-esp-modbus.md` S5 (IDF facts for RTS in half-duplex mode on esp32s3: `uart_ll.h:773-783`, `uart.c:1190-1191`, `uart.c:1493-1506`, `uart.c:958-960`, `uart.c:2164-2168`; external, IDF v6.0.2 source) and S10 (driver delete releases pins, `uart.c:2105-2142`). Re-check each against the installed IDF and cite file:line in the report.

## Interfaces
```c
/* rs485_port.h (used only by mb_link); includes <stdint.h>, <stddef.h>, "esp_err.h" only */
typedef struct { int uart_port, tx_gpio, rx_gpio, de_gpio; uint32_t baud; } rs485_port_cfg_t;
#define RS485_LEN_MORE    ((size_t)0)   /* reply length not known yet */
#define RS485_LEN_UNKNOWN SIZE_MAX      /* not determinable: end at the first silence window */
typedef size_t (*rs485_frame_len_fn)(const uint8_t *rx, size_t n, void *ctx);
typedef struct { uint32_t gap_bytes, faults, max_byte_interval_us; } rs485_port_counters_t; /* since boot, saturating */

esp_err_t rs485_port_hold_receive(const rs485_port_cfg_t *c);  /* DE: level 0, then output enable; pull-up off */
/* Precondition: port not open (else ESP_ERR_INVALID_STATE). Plain RX-only UART (UART_MODE_UART, TX/RTS/CTS =
   UART_PIN_NO_CHANGE, TX buffer 0, event queue); counts received bytes plus frame/parity/break/FIFO-overflow/
   buffer-full events for ms; deletes the driver; holds receive. DE stays a plain GPIO at 0 throughout. */
esp_err_t rs485_port_silence_check(const rs485_port_cfg_t *c, uint32_t ms, uint32_t *activity);
/* hold receive; uart_param_config (8N1, baud, flow control off); uart_driver_install (RX 256, TX 0, no queue);
   uart_set_mode(UART_MODE_RS485_HALF_DUPLEX); uart_set_pin(tx, rx, de as RTS, UART_PIN_NO_CHANGE);
   uart_set_rx_full_threshold(1); RX pull-up (see below). Any failure: close path, error returned.
   Refused (ESP_ERR_INVALID_ARG, no UART call) if uart_port is not 1..2 (UART0 is the console) or the three GPIOs are not distinct;
   ESP_ERR_INVALID_STATE if already open. The same cfg checks apply to hold_receive and silence_check. */
esp_err_t rs485_port_open(const rs485_port_cfg_t *c);
esp_err_t rs485_port_close(void);   /* if installed: uart_driver_delete; then always hold receive; safe if not open */
/* Blocking FC-agnostic transaction (rules below). ESP_OK with *rx_len (0 = nothing received before the deadline);
   ESP_ERR_INVALID_STATE (not open) / ESP_ERR_INVALID_ARG (bad args, tx_len != 8) without bus access;
   ESP_FAIL / ESP_ERR_TIMEOUT on a port fault, after which the port is closed and held. */
esp_err_t rs485_port_transact(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_cap, size_t *rx_len,
                              rs485_frame_len_fn len_fn, void *len_ctx,
                              uint32_t resp_timeout_ms, uint32_t silence_us);
void rs485_port_get_counters(rs485_port_counters_t *out);

/* mb_link.h: includes "esp_err.h", <stdint.h>, <stdbool.h> only (amended 2026-11-24, user decision: seam rule,
   no rs485_port.h). API of spec 002 unchanged in use (field names as before); counters added. */
typedef enum { MBL_OK, MBL_TIMEOUT, MBL_EXCEPTION, MBL_BAD_FRAME, MBL_ERROR, MBL_REFUSED } mb_link_res_t;
typedef struct { int uart_port, tx_gpio, rx_gpio, de_gpio; uint32_t baud; } mb_link_port_cfg_t; /* copied field by field into rs485_port_cfg_t */
typedef struct { mb_link_port_cfg_t port; uint32_t resp_timeout_ms; } mb_link_config_t;
esp_err_t mb_link_hold_receive(const mb_link_config_t *cfg);                          /* first call in app_main */
esp_err_t mb_link_silence_check(const mb_link_config_t *cfg, uint32_t ms, uint32_t *activity); /* not while initialised: ESP_ERR_INVALID_STATE */
esp_err_t mb_link_init(const mb_link_config_t *cfg);    /* = rs485_port_open; resp_timeout_ms 200..2000, baud 1200..115200 else ESP_ERR_INVALID_ARG; twice: ESP_ERR_INVALID_STATE */
esp_err_t mb_link_deinit(void);                         /* = rs485_port_close; ESP_OK no-op if not initialised */
/* Blocking, <= TX frame time + resp_timeout_ms + 2 silence windows + 1 tick. exc_code = exception code, else 0. */
mb_link_res_t mb_link_read(uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc_code);
/* Own type (amended 2026-11-24, user decision: seam rule). E2: measured = true, values = port counters.
   E1b fallback: measured = false and all three values 0 (not measurable through esp-modbus). */
typedef struct { bool measured; uint32_t gap_bytes, faults, max_byte_interval_us; } mb_link_counters_t;
void mb_link_get_counters(mb_link_counters_t *out);    /* bus_task only; copy of the port counters (approved 2026-11-24, user decision) */
```
`mb_link.c` includes only `esp_err.h`, standard headers, `mb_link.h`, `rs485_port.h`, `mb_rtu.h`, `daikin_regs.h` (no FreeRTOS, `esp_log`, driver headers): the link logic stays separate from the driver code (amended 2026-11-24, user decision: no host build, D12). `_Static_assert(RS485_LEN_MORE == MB_RTU_LEN_MORE && RS485_LEN_UNKNOWN == MB_RTU_LEN_UNKNOWN)`.

### `mb_link_read` (normative order)
1. `*exc_code = 0` if non-NULL. 2. `val == NULL || exc_code == NULL` -> `MBL_ERROR`. 3. `addr` not 1..247 or `!daikin_regs_read_allowed(reg, qty)` -> `MBL_REFUSED`. 4. Not initialised -> `MBL_ERROR`. 5. `mb_rtu_build_fc03` (!= 8 -> `MBL_REFUSED`, unreachable). 6. `rs485_port_transact(req, 8, rx, MB_RTU_MAX_REPLY_LEN + 1, &n, adapter -> mb_rtu_fc03_reply_len(qty, ...), resp_timeout_ms, mb_rtu_t35_us(baud))`; non-OK -> `rs485_port_close()`, not initialised, `MBL_ERROR`. 7. `mb_rtu_parse_fc03_reply(addr, qty, rx, n, val, exc_code)`: OK -> `MBL_OK`, EXCEPTION -> `MBL_EXCEPTION`, BAD_FRAME -> `MBL_BAD_FRAME`, TIMEOUT -> `MBL_TIMEOUT`. The RX buffer is one byte longer than the longest valid reply so that trailing bytes are always visible to the classifier. `val` is non-NULL here (step 2 guarantees it): `mb_link` never passes a NULL `val` to `mb_rtu_parse_fc03_reply`, which returns BAD_FRAME for NULL `val` (reviewer note on 012) and would be misreported as a bad frame on the bus.

### Reply-length adapter (normative; amended 2026-11-24, reviewer minor on 012)
- Only `mb_link` calls `mb_rtu_fc03_reply_len`. `rs485_port` never calls it; it only calls the `len_fn` it is given.
- The adapter in `mb_link.c` is the `len_fn` of step 6. It takes `qty` only from the `mb_link_read` argument, and `mb_link_read` reaches step 6 only after `qty` is validated (`daikin_regs_read_allowed`, qty 1..4 = 1..`MB_RTU_MAX_QTY`). So `mb_rtu_fc03_reply_len` is only ever called with qty in 1..`MB_RTU_MAX_QTY`.
- Result handling: `r = mb_rtu_fc03_reply_len(qty, rx, n)`. If `r == MB_RTU_LEN_UNKNOWN` or `r > MB_RTU_MAX_REPLY_LEN`, the adapter returns `RS485_LEN_MORE`, not a clamped length (a clamped length would end the frame early instead of at the deadline). Otherwise it returns `r`. `RS485_LEN_MORE` means that rule 5 applies: silence never ends the frame, only the deadline does, and the existing deadline + one silence window hard stop (rule 5) bounds the drain. Rule 5's generic `RS485_LEN_UNKNOWN` branch is unchanged and is not used on this path.
- Buffer bound: the RX buffer is `uint8_t rx[MB_RTU_MAX_REPLY_LEN + 1]` and `rx_cap` is `MB_RTU_MAX_REPLY_LEN + 1` (13 + 1 trailing byte). `rs485_port_transact` never stores into `rx[i]` for i >= `rx_cap` (bytes beyond are counted and discarded, rule 5) and passes `len_fn` an `n <= rx_cap`. Any `len_fn` result, including an out-of-range one, therefore cannot overrun the buffer.

### `rs485_port_transact` (normative)
1. Arguments checked (open, pointers, `tx_len == 8`, `rx_cap >= 1`, `len_fn`, `resp_timeout_ms` 1..2000, `silence_us > 0`) before any UART call.
2. **Gap bytes:** `uart_get_buffered_data_len` is added to `gap_bytes` (bytes that arrived since the previous transaction: late replies or foreign traffic), then `uart_flush_input`. Phase 1: count and discard, the request is still sent (amended 2026-11-24, user decision; no bus-busy skip, no latch).
3. `uart_write_bytes(port, tx, 8)` (the only TX call site in the tree); result != 8 -> fault. `uart_wait_tx_done(port, ticks for the 8-byte frame time at baud + 20 ms, rounded up)`; not `ESP_OK` -> fault. Fault = `faults++`, close, return error.
4. Deadline = time after `uart_wait_tx_done` returned + `resp_timeout_ms` (`esp_timer_get_time`). Silence window = `silence_us` rounded **up** to FreeRTOS ticks **plus one tick** (a 1-tick wait can expire at once; with the default 100 Hz tick the window is 10-20 ms at 9600; longer than t3.5 is allowed, shorter is not). **Known limit** (amended 2026-11-24, user decision): `CONFIG_FREERTOS_HZ` stays at the default 100 Hz, so every transaction carries a 10-20 ms end-of-frame wait (about 2.5-5 x t3.5 at 9600); this lengthens the logged transaction time (008 D.8) and is accepted.
5. **End-of-frame rule (fixes the split-frame risk from the spike review):** bytes are read as they arrive (RX FIFO threshold 1, so each byte reaches the ring buffer at once) and time-stamped; `max_byte_interval_us` tracks the longest interval between consecutive bytes of one reply. After each byte, `len_fn` gives the expected length.
   - **Silence before the expected length is reached never ends a frame; only the deadline does.** A reply with inter-byte gaps is therefore collected as one frame as long as it is complete before the deadline; there is no early return that could let the rest of the reply overlap our next request.
   - Expected length reached, or `RS485_LEN_UNKNOWN`: wait for one full silence window without a byte (>= t3.5, user decision); bytes inside it are trailing bytes, collected and returned (the classifier makes them BAD_FRAME), and each restarts the window.
   - Deadline reached with 0 bytes: return `*rx_len = 0` at once (TIMEOUT). Deadline reached with a short frame: drain as above.
   - Hard stop for the silence/drain phase: deadline + one silence window. Bytes beyond `rx_cap` are counted for the stop rule and discarded; `*rx_len` saturates at `rx_cap`.
   - Not checked (documented limit, user decision): t1.5 inter-character timeout; a gap inside a frame is tolerated, not flagged (it is visible in `max_byte_interval_us`).
6. Residual risk (documented): a reply that starts after the deadline + window is not seen; its bytes arrive in the idle time and are counted as `gap_bytes` before our next request (>= `gap_ms` later). A reply that is still arriving when our next request starts could collide; the 100 ms gap and the counter make this visible, the 008 inter-byte check measures it.

### RX line while transmitting
DE and /RE are tied, so the MAX485 receiver output RO is high-impedance while we transmit (`entities/max485`) and the RX GPIO may float. State from the v6.0.2 source whether `uart_set_pin` enables the RX pull-up (file:line); if not, `rs485_port_open` enables it (`gpio_pullup_en(rx_gpio)`) after `uart_set_pin`. No RX flush after TX (it could eat the start of a fast reply); garbage during TX would show up as BAD_FRAME instead of TIMEOUT on the bench without a slave (008 B.4).

## Escalation (stop and report to the architect; the user decides)
- With TX buffer 0, the half-duplex RTS assertion is not on the `uart_write_bytes` path, or DE would be high at any time outside our 8-byte transmit (between `uart_set_mode` and `uart_set_pin`, after TX done, during or after delete).
- `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)` cannot precede `uart_set_pin`, or the RTS level after `uart_set_pin` is not low.
- `uart_set_rx_full_threshold(port, 1)` is unavailable or bytes cannot be time-stamped within about one character time (the end-of-frame rule would need another design).
- Any need for `uart_set_line_inverse`, `uart_set_rts`, an inverter or a lint relaxation.

## Acceptance checks
1. `cd firmware && idf.py build` (esp32s3) passes, zero warnings in owned code; lint (013 rules) passes; ELF check passes with `required: none configured` (the read path is linked from 006); report `librs485_port.a` / `libmb_link.a` in the build log.
2. Existing host tests (`firmware/test/host`, after `idf.py reconfigure`) still pass, exit 0; this task adds none (amended 2026-11-24, user decision; D12).
3. Code inspection checklist in the report, each with file:line in our code and, where stated, in IDF v6.0.2:
   a. `mb_link_read` (amended 2026-11-24, user decision: replaces the host-test evidence "refusals make zero port calls"): file:line of the `addr` 1..247 check and of the `daikin_regs_read_allowed` check, each returning `MBL_REFUSED`, and of the NULL and not-initialised checks returning `MBL_ERROR`, all before the first `rs485_port_*` call in the function (file:line of that call); same for `mb_link_init`'s timeout/baud checks before `rs485_port_open`, and `mb_link_silence_check`'s initialised check before `rs485_port_silence_check`.
   b. `rs485_port_open` order: hold -> `uart_param_config` -> `uart_driver_install` -> `uart_set_mode(RS485_HALF_DUPLEX)` -> `uart_set_pin` -> RX threshold -> RX pull-up; no call between `uart_set_mode` and `uart_set_pin` sets RTS active (IDF file:line).
   c. Close and every hold: `gpio_set_level(de, 0)` before the output-enable call (IDF `gpio.c:228` cited); hold after `uart_driver_delete` regardless of its result; no `gpio_reset_pin` on DE.
   d. Silence check: `UART_MODE_UART`, TX/RTS/CTS `UART_PIN_NO_CHANGE`, refused while open, driver deleted and receive held afterwards.
   e. Exactly one `uart_write_bytes` call site, preceded by gap-byte count + flush, followed by `uart_wait_tx_done`; the half-duplex RTS assert/release points on this path (IDF file:line: `uart.c` TX path with TX buffer 0, TX_DONE ISR).
   f. End-of-frame implementation matches rule 5 (point to the loop); duration bound holds by construction.
   g. Fault paths close and hold; no `ESP_LOG*`, `ESP_ERROR_CHECK`, `abort`, `assert` in both components (grep output).
   h. `grep -rn "rs485_port_" firmware/components firmware/main` shows calls only in `mb_link/src/` (and the port itself); no FC literal outside `mb_rtu` (lint R8).
   i. Seam rule (amended 2026-11-24, user decision): `grep -rnE 'mb_rtu\.h|rs485_port\.h|rs485_port_|MB_RTU_|RS485_' firmware/components firmware/main` lists matches only under `components/mb_rtu/`, `components/mb_link/src/`, `components/rs485_port/` (report the output); `mb_link.h` includes no project header.
   j. `mb_link_get_counters` sets `measured = true` and copies the three port counters.
   k. Reply-length adapter bounds (amended 2026-11-24, reviewer minor on 012): report file:line of (i) the `qty` validation in `mb_link_read` before the `rs485_port_transact` call; (ii) the adapter in `mb_link.c` mapping `MB_RTU_LEN_UNKNOWN` and `> MB_RTU_MAX_REPLY_LEN` to `RS485_LEN_MORE` (and no clamp); (iii) the `rx` declaration of size `MB_RTU_MAX_REPLY_LEN + 1` and the `rx_cap` argument of that size; (iv) in `rs485_port.c` the write guard showing no store to `rx[i]` with i >= `rx_cap`; (v) the `mb_rtu_parse_fc03_reply` call in `mb_link_read` after the step-2 check that `val` is non-NULL. Also `grep -rn 'mb_rtu_fc03_reply_len' firmware/components` must list only the adapter call in `mb_link/src/mb_link.c` (and the definition in `mb_rtu`).
4. Guard proofs, each a reverted temporary edit with the failing output: (a) `uart_write_bytes` call added to `mb_link.c` -> lint R4; (b) `uart_set_mode(p, UART_MODE_RS485_APP_CTRL)` in `rs485_port.c` -> lint R5. (The former host-test guard proof (c) is dropped, amended 2026-11-24, user decision.) Plus: `grep -n '#include' firmware/components/mb_link/src/mb_link.c` and `firmware/components/mb_link/include/mb_link.h` list only the allowed headers (report both).
5. No dependency added (local `dependencies.lock` unchanged apart from IDF itself; no esp-modbus).

## Test strategy
- No host tests in this task (amended 2026-11-24, user decision: fake-`rs485_port` host test rejected; D12 unchanged, `mb_link` is not built for linux). The logic `mb_link` delegates to is host-tested where it lives: frame building, reply length, classification and t3.5 in `mb_rtu` (012), the read whitelist in `daikin_regs` (009), retry/health on `ps_res_t` results at the `poll_sched` seam (010, 011).
- `mb_link`: verified by code inspection (acceptance 3a, 3i, 3j: refusal order, `mb_rtu_parse_fc03_reply` -> `MBL_*` mapping, fault path closes and marks not initialised, `rx_cap` = `MB_RTU_MAX_REPLY_LEN + 1`, `silence_us = mb_rtu_t35_us(baud)`), by the 006 build (ELF required symbols) and on the bench/site through 008.
- `rs485_port`: target-only (IDF UART driver); verified by the code inspection list, the guard proofs and on the bench/site through 008 (DE low, TIMEOUT without slave, `mb silence` cycles, inter-byte interval).

## Fallback E1b (not implemented; amended 2026-11-24, user decision)
E2 (this spec) is the implementation. E1b is kept documented so a revert stays cheap; **nothing of E1b is built now**.
- **Trigger:** a separate user decision, e.g. framing or timing problems with the own RTU master on site that cannot be resolved, or E2 proving too hard to debug. No agent reverts on its own.
- **E1b behaviour:** esp-modbus `==2.1.4` serial master inside `components/mb_link/src/`; `mb_link.h` unchanged. Result mapping: every `ESP_ERR_INVALID_RESPONSE` -> `MBL_BAD_FRAME` with `exc_code` 0 (esp-modbus does not distinguish a slave exception from a bad CRC/frame, spike S3); `poll_sched` then retries once and counts it toward DEGRADED/OFFLINE (010 rules, unchanged). `MBL_EXCEPTION` is never returned. Counters: `measured = false`, all values 0 (006 prints `n/a`).
- **What a revert changes** (facts from `docs/spikes/002-esp-modbus.md`):
  - `components/mb_link/src/` internals and its `idf_component.yml` (`espressif/esp-modbus: "==2.1.4"`): one-entry dummy descriptor table for `mbc_master_start` (S3 findings); `uart_set_mode(UART_MODE_RS485_HALF_DUPLEX)` before `uart_set_pin` (S5); DE re-driven level-before-enable after `uart_driver_delete` (S5, S10). `rs485_port` keeps hold-receive and silence check; whether `rs485_port_open`/`transact` and `mb_rtu` stay in the tree is decided with the revert.
  - Lint (013): R1/R3 re-allow esp-modbus master API and headers only in `components/mb_link/src/` (incl. `mbc_master_set_descriptor`) and the esp-modbus line in `components/mb_link/idf_component.yml`; R4/R8 back to the 001 rules for that path (esp-modbus request struct `command`, `MB_FUNC_*`).
  - ELF check (013): drop the `mbc_`/`mbm_` ban, keep `mbs_`/`mbc_slave_`/`mbc_tcp_`/`mb_link_write`; write handlers are present but unreachable (S11). `DMT_ELF_REQUIRED_SYMBOLS` (006's line in `firmware/CMakeLists.txt`): drop `mb_rtu_build_fc03` if `mb_rtu` is no longer linked.
  - Nothing else above `mb_link` (seam rule).
- **What is lost:** D21 exception-02 -> unsupported detection; the exception codes behind U2/U3/U5 (010); 008 probe exit items 4 (exception + code per register) and 5 (qty > 1 failure cause) are weakened to "BAD_FRAME, cause unknown"; the 008 link-counter checks (B.4, D.8a) become n/a. The 010 rules for `PS_RES_EXCEPTION` become unreachable but stay correct.

## Recommended implementer model
sonnet: safety-ordered driver code (DE level before output enable, mode before pins), a timing loop with a subtle end-of-frame rule, IDF source evidence with file:line, and a seam (`mb_link.h` own types, no `rs485_port.h`; amended 2026-11-24, user decision) that must stay clean for the E1b fallback; not mechanical.

## Out of scope
- `bus_task`, console, `app_main`, setting `DMT_ELF_REQUIRED_SYMBOLS` (006).
- Any write code (phase 3), other function codes, broadcast, address scan.
- Acting on gap bytes beyond counting (user decision 2026-11-24), t1.5 checking, collision detection, `CONFIG_FREERTOS_HZ` changes, a bench slave simulator (amended 2026-11-24, user decision: neither extra).
- Any E1b code, manifest or lint change (documented fallback only).
- Host tests for `mb_link` or `rs485_port` (user decision 2026-11-24; D12).
- Workarounds for an unexpected DE level (inverter, `uart_set_line_inverse`): escalate instead.
