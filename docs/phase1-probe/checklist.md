# Phase-1 probe: bench and on-site checklist

For the person at the bench and on site. Task spec: `tasks/008-p1-bringup-hil-checklist.md`. Fill in `measurement-log-template.md` as you go (field ids such as `B4.x` point to its rows). Wiki short names used below: arch = `concepts/firmware-architecture` (section number after "s"), topo = `concepts/bus-topology`, ekw = `entities/ekwhctrl1`, addr = `concepts/device-addressing`, max485 = `entities/max485`, flags = `concepts/flag-registers`, web = `concepts/web-restriction-flags`. Facts marked "external" are not in the wiki. Facts marked "inferred" are inferences, not facts. Image: phase-1 firmware, app `fw=6654669-dirty` as flashed (phase-1 code = commit b8ad00d, task 006). **The trip image is the write image** (task 019: `fw=<B.W fw>`, console header ends `writes=console`, built with `idf.py -B build_write ...`, see `firmware/README.md`); it is the phase-1 probe plus the `mb write` commands, inert until `mb write arm`. The read image (default build, no write code) remains available.

**On-site run 2026-11 with MAX485-M** (D23 bench-only rule overridden by the user 2026-11-27, arch s6 "On-site phase-1 decisions"): VCC 3.3 V from the ESP 3V3 pin (outside the 5 V rating if U1 is a 5 V part), R7 120 Ohm fitted, weak bias (idle A-B ~10 mV), DE pull-down single 10 kOhm (D8 unchanged; boot-window risk accepted by the user), RX: RO direct to GPIO4 with 20 kOhm pull-up to 3.3 V. Image fw 6654669-dirty (phase-1 code = b8ad00d).

**Known console defect (console-init race; fixed in the 019 images, where the REPL is created before `bus_task` starts; still present in the image flashed as `fw=6654669-dirty`):** the boot header `# dmt-probe v1 addr=<a> baud=<b> fw=<fw>` (write image: the line ends ` writes=console`) and the first `# silence check: started <n> s` line may be lost (only `# dmt-` appears). Type `mb log on` after boot to reprint the header. The bus is not affected.

Tick each box when done. If a step says STOP: write down what you saw in the log ("unexpected observations") and go back to the architect; do not continue.

## Console commands used (all exist in the image)

`mb status`, `mb regs`, `mb read <reg> [qty 1-4]`, `mb probe`, `mb poll on|off`, `mb silence [seconds 1-600]`, `mb stats`, `mb addr <1-247>`, `mb log on|off`. The prompt is `dmt>`.

**Write image only** (`mb write ...`; the read image has no such command). Syntax errors print `usage: mb write arm [30-600] | disarm | sp <5.0-40.0> | standby on|off | fan auto|silent|night|max | season 0|3|5 | restore` and send nothing:
- `mb write arm [30-600]` arm for that many seconds (default 120); `mb write disarm`.
- `mb write sp <5.0-40.0>` (at most one decimal, register 231), `mb write standby on|off` (201 bit 7), `mb write fan auto|silent|night|max` (201 bits 0-2 = 0/1/2/3), `mb write season 0|3|5` (233), `mb write restore`.
- Disarmed, `sp`, `standby`, `fan`, `season` and `restore` print `W <t> REFUSED <cmd> disarmed`. At most 40 FC06 frames per arm session; at most one command at a time and one per 5 s.

Line formats printed by the image:
- `T <t_start_ms> <dur_ms> <addr> <kind> <reg> <qty> <result> [values | exception code]`; kind is POLL, RETRY, HEALTH, SWEEP or MANUAL (write image also PRE, WRITE, VERIFY: the three transactions of a write command; the `WRITE` line of an `OK` result carries the written value as the 4-digit hex word, PRE and VERIFY carry the value read; PRE, WRITE and VERIFY lines are printed whatever `mb log` says); result is `OK`, `TIMEOUT`, `EXC`, `BADFRAME` or `ERROR`; values are 4-digit hex words (raw, unscaled), an exception code is 2 hex digits.
- `H <t_ms> <health>`; health is STOPPED, ONLINE, DEGRADED, OFFLINE or LATCHED.
- Write image only, the `A` (arm session) and `W` (write command) lines, all printed whatever `mb log` says, `t` in ms, `hhhh` a 4-digit lowercase hex word, registers 3 digits:
  - `A <t> ARMED <s> s budget <n>`, `A <t> EXTENDED <s> s left budget <n>`, `A <t> RESTORE_SET 201=<hhhh> 233=<hhhh> 231=<hhhh>` (printed once at the first arm per boot and address);
  - `A <t> REFUSED arm <latched|health|stale <reg>|web_flag <n>>`;
  - `A <t> DISARMED <user|timeout|failure|budget|health|poll_off|silence|addr|latched|link|web_flags>`, `A <t> LATCHED failures 3`;
  - `W <t> CMD <sp|standby|fan|season|restore> <arg>` (`sp`: raw tenths, e.g. `sp 215`; `standby`: `on`/`off`; `fan`: the keyword; `season`: 0/3/5; `restore`: no argument);
  - `W <t> REFUSED <cmd> <disarmed|latched|busy|pacing <ms>|stale <reg>|web_flag <n>|limit <lo>-<hi>|undefined_mode>` (`limit` in raw tenths: the allowed setpoint range; `pacing <ms>`: milliseconds still to wait);
  - `W <t> SKIP <reg> unchanged <hhhh>` (the register already had the value: no FC06 sent), `W <t> ABORTED <reg> <disarm reason>` (session ended before the FC06: nothing sent);
  - `W <t> DONE <reg> <old hhhh> -> <new hhhh> budget <n>`;
  - `W <t> FAIL <reg> <pre_|write_|verify_><RESULT>|mismatch <hhhh>|verify_lost failures <k>/3` (for example `W <t> FAIL 231 verify_TIMEOUT failures 1/3`).
- `mb status` prints, in this order: `bus: uart ...`, `addr: kconfig <k> current <a>`, `pacing: ...`, `periods: ...`, `health: <h>`, `boot silence: <ms> ms, activity <n|running>`, `log: ...`, `bus_task stack high-water (minimum free): <n> bytes`, `write: compiled out` (read image) or, in the write image, `write: disarmed, budget 40/40, failures 0/3` (armed: `write: armed <s> s left, budget <n>/40, failures <k>/3`, with `, latched` appended after 3 failures) followed by `restore set: none` or `restore set: 201=<hhhh> 233=<hhhh> 231=<hhhh>`, then either `link counters: gap_bytes <n> max_byte_interval_us <n>` followed by `faults since boot: <n>` (and `link closed: mb silence or reboot to recover` only after a port fault), or just `link counters: n/a`. **No `faults since boot` line follows `n/a`.** `n/a` is expected only in the first moments after reset (inferred). Type `mb status` again; if it still shows `n/a` after the first `H` line, record it and STOP, back to the architect (the "`n/a` stop", B.4.5).
- `mb stats` prints `txns <n>: ok .. timeout .. exception .. bad_frame .. error ..`, a `kinds:` line (write image: ends ` pre <n> write <n> verify <n>`), `error rate (timeout+bad_frame+error)/txns: <x.y> %`, `offline entries <n>`, `latency ms (answered): min <n> median <n> max <n>`.

---

## A. Safety (read first, tick before any wiring step)

- [ ] A.1 The fancoil and the wall panel are mains powered. Open housings, connect or disconnect wires and measure resistance or continuity **only with mains off**, following the appliance manual.
- [ ] A.2 **Never touch the "+ A B -" block** (panel to fancoil) **and never the CP terminal** of the "A A B B -" block: do not loosen, measure on or reconnect them. Only one A, one B and "-" of "A A B B -" are used.
- [ ] A.3 Connect node GND to "-" **only after** step C.4 showed "-", A and B within -7..+12 V of node GND (arch s4, RS485 common-mode range, external). The node runs on normal USB power. Whether the panel's low-voltage side is isolated from mains is not stated in the wiki (arch s8).
- [ ] A.4 Live voltage readings (C.4) only on the low-voltage terminals with mains parts covered. If that is not possible, skip them, leave node GND unconnected and say so in the log (`C4.skip`).
- [ ] A.5 **Abort** (unexpected panel or fancoil behaviour, error code, icon, smell, heat): type `mb poll off`, unplug the node's USB power (the MAX485 is then unpowered), mains off before touching any wire. Record what happened. With this MAX485-M, DE is **not** guaranteed low by the 10 kOhm pull-down while the ESP is in reset or its GPIO is unpowered/undriven (the module's DE / /RE pull-ups win); unplugging USB still removes all power and stops all transmission.
- [ ] A.6 **The node transmits on its own.** After a clean boot silence check, polling starts automatically and the first frame goes out about 5 s after power-up. Therefore: connect or disconnect A/B only with the node unpowered and mains off (A.1); open the serial console before powering the node while it is wired, ready to type `mb poll off`. `mb poll off` typed within the first 5 s prevents the first frame; later it stops polling after the current transaction. With A/B wired: no flashing, no BOOT button, never hold RST (the driver stays enabled while held, accepted risk). Each serial-monitor open that auto-resets the chip is one more ~0.3 s boot window. **Hard rule (user 2026-11-27):** with A/B wired only the phase-1 probe image (read or write variant) (or later the 2b-A bus image, `mb_link_hold_receive` first in `app_main`) may run. Never run an image without the bus code's early DE hold (currently the phase-2a Matter image, which never drives GPIO6) while A/B are wired: with the MAX485-M pull-ups its transmitter would stay enabled the whole time. Re-flash the Matter image only after A/B are disconnected (D.11).

---

## B. Bench (before going on site)

Pacing rule (B.12): while `mb silence N` runs the node executes no command. Wait N s, until its result line, before typing more. The command queue holds 8 entries; extra commands print `command not queued: ESP_ERR_TIMEOUT`.

**B.1 Wiring** (arch s4; max485)
1. [ ] TX GPIO to DI; DE and /RE tied to the DE GPIO, with 10 kOhm from DE to GND. RO handling depends on the module supply: module / VCC at 3.3 V: RO direct to the RX GPIO with the 20 kOhm (2 x 10 kOhm) pull-up to 3.3 V; VCC 5 V: never direct (diode + pull-up as on the 2026-11-27 bench; the divider is rejected, arch s4). Name the module VCC source (on site: ESP 3V3 pin).
2. [ ] Write the GPIO numbers into `B1.gpio`. They must equal `CONFIG_DMT_BUS_TX_GPIO` / `RX_GPIO` / `DE_GPIO` of the flashed image (defaults 5 / 4 / 6 are placeholders). Record U1 marking and module supply (`B1.u1`, `B1.supply`).

**B.2 Unpowered module** (no USB): measure A-B resistance, DE to /RE continuity, DE to GND (expect 10 kOhm). Also DE-VCC, /RE-VCC, RO-VCC, DI-VCC (each with both lead polarities), A-VCC, B-GND, A-B, VCC-GND. Note: DE-GND 10 kOhm holds DE low only when the module has no pull-ups on DE / /RE. Record `B2.*`.

**B.3 Flash and identify**
1. [ ] Flash the phase-1 image. Record the git commit (`B3.commit`) and that `build-verifier` reported lint and ELF check passed (`B3.verifier`).
2. [ ] Open the console. The first console line is `# dmt-probe v1 addr=<a> baud=<b> fw=<version>`. Record `a`, `b` and `fw` (`B3.addr`, `B3.baud`, `B3.fw`). Save the complete serial output from here on.

**B.4 Powered, bus not connected, no slave**
1. [ ] **[TRANSMITS]** (polling starts by itself after a clean 5 s check, A.6). Power-cycle. Within the 5 s check: `mb status` shows `boot silence: 5000 ms, activity running` (the ms value is the configured one).
2. [ ] **[TRANSMITS]** After about 5 s the output shows `# silence check: started 5 s`, then `# silence ok, polling on: mb poll off to stop` and `H <t> ONLINE`. Without a slave then `H <t> DEGRADED` and, at the 5th consecutive failure, `H <t> OFFLINE`.
3. [ ] **[TRANSMITS]** (polling still running until step 4) `mb status` shows `boot silence: <ms> ms, activity 0`, `write: compiled out` (read image) or `write: disarmed, budget 40/40, failures 0/3` (write image), `link counters: gap_bytes 0 ...` and `faults since boot: 0` (if it shows `link counters: n/a` instead: the B.4.5 `n/a` stop). Record `B4.activity`, `B4.gap`, `B4.maxint`, `B4.faults`.
4. [ ] Type `mb poll off` (`H <t> STOPPED` appears). Measure the DE pin with the multimeter (DC, to node GND): must be **low**. Record `B4.de_v`.
5. [ ] Every `T` line without a slave must be `TIMEOUT`.
   - STOP if any `T` line is `BADFRAME`, or `gap_bytes` is not 0, or `faults since boot` is not 0: bytes were received during or after our own transmission (RX line floating while /RE is high, 014). Record and go back to the architect.
   - STOP ("`n/a` stop") if `mb status` shows `link counters: n/a` and still does so after typing `mb status` again after the first `H` line (see Console commands). Record and go back to the architect.
   - STOP if activity is > 0 with A/B open: the receiver sees an unbiased floating line (bias resistors on the module unverified, arch s8). Record. This goes to the architect, not to a relaxed firmware setting.

**B.5 Abort path and silence cycles**
`mb probe` behaviour (006, spec 010 rule 8): it transmits from STOPPED and from ONLINE/DEGRADED (at lowest priority). While health is OFFLINE it is accepted without a message but suspended until a HEALTH read succeeds. A second `mb probe` while one runs prints `# mb probe refused: latched or a sweep is running`. `mb poll off`, `mb silence` and `mb addr` cancel a running sweep.
1. [ ] **[TRANSMITS]** only if the 5 s window is missed (A.6). Reboot and type `mb poll off` within the first 5 s. `mb stats` shows `txns 0`; `mb status` shows `health: STOPPED`.
2. [ ] **[TRANSMITS]** `mb probe` from STOPPED (no slave, polling off; see the `mb probe` note above): every `T ... SWEEP ... TIMEOUT`; `mb status` health stays STOPPED. Do **not** type `mb poll off` while it runs (it cancels the sweep).
3. [ ] **No TX; UART closed and re-opened, DE glitch risk not measured (B.6.d).** Note `faults since boot` (`B5.faults_before`). Then run `mb silence 5` five times, waiting for the result line each time (B.12). Each run prints `# silence check: started 5 s` and then `# silence check 5s ok, polling off: mb poll on to start`; no `# mb_link: ... failed` line. Tick the five results in `B5.run1..5`. If a `# mb_link: ... failed` line appears: record, STOP, back to the architect. If `mb status` shows `link counters: n/a`: the B.4.5 `n/a` stop.
4. [ ] `faults since boot` after the five runs (`B5.faults_after`) must equal the value before. (This is the UART driver delete/re-install cycle, 014; spike S10.) If it changed: record, STOP, back to the architect. If `mb status` shows `link counters: n/a` (no `faults since boot` line): the B.4.5 `n/a` stop.
5. [ ] Free heap: not checked (no heap output in the 006 image). Leave `B5.heap` as pre-filled.
6. [ ] DE pin low again afterwards (multimeter, `B5.de_v`).
7. Note: `boot silence ... activity` in `mb status` is the **boot** check's value; the result of a `mb silence` run is only in its result line.

**B.6 DE timing: no instrument, indirect evidence only** (Q-HW1, Q-HW2; arch s8)
There is no oscilloscope or logic analyser. These items are **"not measured (no instrument, Q-HW1)"** and are pre-filled that way in the log: DE low at boot without a glitch; no glitch when the port opens after the silence check; rising edge before the first start bit of each request; falling edge within about 100 us after the last stop bit; worst-case DE-fall delay; no DE pulse during `mb silence 5` and the close that precedes it. DE with concurrent flash/NVS activity: **"not done in phase 1, repeat at phase-2 bring-up (Q-HW2)"**.

The following is indirect evidence. It does **not** replace a measurement and never counts as a pass.
- [ ] a. Static DE level (multimeter, DC, DE to node GND): low during the 5 s boot window, after `mb poll off` and after each `mb silence 5`. A multimeter shows an average; it cannot show a glitch or an edge time. Record in `B6.de_*`.
- [ ] b. **[TRANSMITS]** Averaged DE level while `mb probe` runs without a slave (bench only, multimeter DC).
  - Precondition: health is STOPPED after B.5: type `mb status` and check `health: STOPPED` (`B6.health_before`). Reason: while health is OFFLINE the sweep does not transmit, and the reading would be a false pass.
  - Read the meter while the `T ... SWEEP` lines are scrolling, during the about 20 s of the sweep (32 x (500 ms + 100 ms), defaults; inferred). Do not type `mb poll off` while it runs (it cancels the sweep). Record whether the reading was taken while the lines scrolled (`B6.de_probe_scrolling`).
  - Expected if healthy: about 0.05 V (about 1.4 % duty, inferred). DE is high only while the 8-byte request is sent (about 8 ms at 9600, external) and transactions are at least 100 ms apart (arch s3).
  - **STOP (hard stop) if the reading is close to the 3.3 V GPIO level**: DE stays high after the request. Unplug USB at once (A.5), record, and go back to the architect.
- c. On site, in D.8: DE and /RE are tied, so while DE is high the receiver output is high-impedance (max485); a complete `OK` reply therefore implies that DE was high for the whole request and fell before the reply's first byte arrived (inferred). The `OK`, `TIMEOUT` and `BADFRAME` counts of `mb stats`, `gap_bytes` and `faults since boot` from D.8 are the indirect evidence. They do not bound the fall delay.
- d. No console evidence exists for a glitch at boot, at port open, or during the close and `mb silence`. These stay undecided (residual risk, arch s8).

**B.7 RX pull-up on, no slave** [TRANSMITS] (extends B.4): every `T` line is `TIMEOUT`, never `BADFRAME`; `gap_bytes 0`. A `BADFRAME` means the RX line picks up garbage while /RE is high despite the pull-up: record `B7.result` and STOP, back to the architect.

**B.8 Ports**: record which console is used, UART0 or USB-Serial-JTAG (`B8.console`), and that the bus is on UART1: `mb status` first line starts `bus: uart 1` (`B8.bus`; `CONFIG_DMT_BUS_UART_PORT` default 1). Any other combination: STOP, back to the architect. (If the console were on the bus UART the image logs `console UART is the bus UART; bus_task and console not started` and there is no `mb` command.)

**B.9 No uart INFO line**: in the saved boot output and after each `mb silence`, the IDF line containing "queue free spaces" must not appear. If it appears, record `B9.seen`; this is not a stop condition.

**B.10 `bus_task` stack** [TRANSMITS] (polling is running after boot; `mb silence 1` itself: no TX, UART closed and re-opened, DE glitch risk not measured (B.6.d)): `mb status` line `bus_task stack high-water (minimum free): <n> bytes`, once after boot (after the first `H` line, `B10.after_boot`) and once after one `mb silence 1` (`B10.after_silence`). Both >= about 1024 bytes. Less: STOP, back to the architect.

**B.11 Abort-path evidence** (extends B.5)
1. [ ] **[TRANSMITS]** only if the 5 s window is missed (A.6). Reboot; type `mb poll off` in the 5 s window. The saved output has **no `T` line** at all before your next command.
2. [ ] After the window: `# silence ok, polling off: mb poll on to start` and `H <t> STOPPED`.
3. [ ] `mb status` typed during and after the window shows `addr: kconfig <k> current <a>` with `<a>` not 0 and `health: STOPPED` (during the window also `boot silence: <ms> ms, activity running`). `mb stats` shows `txns 0`. Record `B11.*`.

**B.12 Pacing**: see the rule at the top of this section.

**B.13 Pending read cancelled by `mb silence`**
1. [ ] **[TRANSMITS]** (the read is queued; a missed window or a failed cancel would transmit). Reboot. Within the 5 s boot window type `mb read 000 1`, then `mb silence 1`.
2. [ ] Expected: no `T ... MANUAL 000` line at all; `# silence check: started 1 s`, then `# silence check 1s ok, polling off: mb poll on to start`, then `# silence ok, polling off: mb poll on to start` and `H <t> STOPPED`. Polling stays off.
3. A `MANUAL` line after the silence result: record `B13.result` and STOP, back to the architect.

**B.14 Latched run** (bench only, never on site). Only if a second RS485 or USB-UART adapter is at hand to put bytes on A/B or the RX side during the 5 s boot window; otherwise write "not done" in `B14.result`.
1. [ ] Put bytes on the line during the boot window. Expected lines: `# silence check: started 5 s`, `# silence check: activity <n>, latched until reboot`, `# latched until reboot: no transmission`, `H <t> LATCHED`; no `T` line.
2. [ ] `mb status` shows `health: LATCHED` and `boot silence: <ms> ms, activity <n>` with n > 0. DE pin low.
3. [ ] `mb poll on`, `mb probe`, `mb silence` are refused with `# mb poll on refused: latched until reboot`, `# mb probe refused: latched or a sweep is running`, `# mb silence refused: latched, reboot only`.
4. [ ] **[TRANSMITS]** (a clean boot starts polling after 5 s, A.6). Reboot without the adapter afterwards.

**B.15 Port fault** (informational, only if one occurs): exactly one line `# mb_link: port fault, link closed (mb silence or reboot to recover)`. `mb status` then shows `faults since boot` and `link closed: mb silence or reboot to recover` until a passed `mb silence`; afterwards `link closed` disappears and `faults since boot` keeps its value. Record `B15.*`.

**B.16 Re-check with RO direct** (bench, before leaving, A/B open, wiring as in the header) [TRANSMITS]: one boot with polling: every `T` line `TIMEOUT`, no `BADFRAME`, boot `activity 0`, `gap_bytes 0` (repeat of B.4.5 / B.7). Record `B16.*`. A latch (boot `activity` > 0): reboot and repeat, at most 2 times (3 boots total, same as D.2 / OS3), log each boot's activity. A `BADFRAME` or `gap_bytes` > 0: record, STOP, back to the architect.

**B.W Write image** (bench, A/B open, no slave; after B.16). The write image is flashed here, after B.16 passed with the read image. It stays on the board for C, D and E: no flashing on site.
1. [ ] `build-verifier` passed both builds, lint and ELF (read and write mode) (`BW.verifier`). Flash with `idf.py -B build_write -p /dev/ttyACM0 flash`. `mb log on`: the header ends with `writes=console` (`BW.fw`).
2. [ ] **[TRANSMITS]** Repeat B.16 with this image (all `T` lines TIMEOUT, no BADFRAME, activity 0, `gap_bytes` 0; latch rule as B.16) (`BW.b16`).
3. [ ] `mb status`: `write: disarmed, budget 40/40, failures 0/3`, `restore set: none` (`BW.status`).
4. [ ] Disarmed refusals: `mb write sp 21.0`, `mb write standby on`, `mb write fan max`, `mb write season 3` and `mb write restore` each print `W <t> REFUSED <cmd> disarmed`. No `T ... PRE|WRITE|VERIFY` line (`BW.refused`).
5. [ ] `mb write arm` (health OFFLINE without a slave) prints `A <t> REFUSED arm health`. The same after `mb poll off` (STOPPED). Syntax: `mb write arm 29`, `mb write arm 601`, `mb write sp 4.9`, `mb write sp 40.1`, `mb write sp 21.55` and `mb write season 4` each print the usage (`usage: mb write arm [30-600] | disarm | ...`) and send nothing (`BW.arm`).
6. [ ] `mb stats` `kinds:` line ends `pre 0 write 0 verify 0`. DE low on the multimeter (`BW.de_v`).
7. [ ] Stack high-water >= 1024 B after the steps above (`BW.stack`). Less: STOP, back to the architect.
8. [ ] The write image stays on the board. **No flashing until after D.11.**
9. (optional) Only with a USB-RS485 adapter and a PC Modbus slave simulator (not in the BOM): run E.2-E.7 against it. Else write "not done" (`BW.slave`).

---

## C. On site, mains off, node not connected

1. [ ] Photograph both terminal blocks before any change (`C1.photo`).
2. [ ] Continuity A-A and B-B of the doubled terminals (`C2.aa`, `C2.bb`); choose one A and one B (ekw, topo item 3).
3. [ ] Resistance A-B at the panel port (`C3.ab`). Termination rule (arch s4, MAX485-M on site): the MAX485-M has R7 = 120 Ohm: **never fit the separate 120 Ohm**. About 120 Ohm: two terminations (60 Ohm), fine. High/open: fine. kOhm range: possibly panel bias, record. < 100 Ohm or unstable: STOP, back to the architect. Resistance "-" to the CP terminal (`C3.cp`, shared "-"?), measured without touching the CP wiring.
4. [ ] Mains on, node powered but not wired to the panel: with the meter between node GND and each panel terminal, DC voltage of "-", A, B (`C4.minus`, `C4.a`, `C4.b`); then A-B at the panel (`C4.ab`). Mains off. If unsure whether the panel low-voltage side is isolated, skip C.4 (A.4).
5. [ ] Confirm the panel address on the panel itself: standby, hold AUTO 5 s, read the value, wait 10 s without changing anything (addr, p6). Record `C5.addr`. If it differs from the image's address (`B3.addr`), use `mb addr <n>` in D.2 (never scan).

---

## D. Connect and probe

1. [ ] Mains off, node unpowered: connect node A/B (and GND to "-" only if A.3 allows) with short leads, no star. No external 120 Ohm. Mains on.
2. [ ] **[TRANSMITS]** (automatic start after a clean silence check, and `mb addr` / `mb poll on` below). Open the serial console and start saving the complete serial output, then power the node. The boot silence check runs 5 s; afterwards `mb status` must show `boot silence: <ms> ms, activity 0` (exit item 3; `D2.activity`).
   - **If activity > 0 the node latches and will not transmit** (`# silence check: activity <n>, latched until reboot`, `# latched until reboot: no transmission`, `H <t> LATCHED`). Relaxed rule (user 2026-11-27): activity 1-7: power-cycle via USB (unplug / replug), at most 2 retries (3 boots total), log each boot's activity. Activity >= 8 or a 3rd latch: abort and report (foreign traffic or an unbiased line, both go to the architect). D.10 `mb silence 60` still covers slow masters.
   - D.2a [ ] (optional, only under A.4 conditions) After a clean boot: A-B DC at the panel (`D2a.ab_idle`); >= 0.2 V means the panel biases the line.
   - With activity 0 polling starts by itself (`# silence ok, polling on: mb poll off to stop`, `H <t> ONLINE`): watch the panel from the first frame (exit item 9).
   - If C.5 gave an address other than the image's: always `mb poll off`, then `mb addr <n>`, then `mb poll on` (a bare `mb addr` keeps polling at the wrong address). Type `mb poll off` within the 5 s window if possible, otherwise at once.
3. [ ] **[TRANSMITS]** Logging is on from power-up, so the saved output holds every `T` line from the first frame. Type `mb log off` only if the console is too noisy (then `mb log on` before D.7). Then `mb probe` and `mb regs` (exit items 1, 2, 4). If `H <t> OFFLINE` appears before or during `mb probe`, nothing answers: go to D.4.
4. [ ] **[TRANSMITS]** (repeats D.2-D.3). If nothing answers: `mb poll off`, unplug the node, mains off, swap A/B on the node side only, mains on, repeat D.2-D.3 (exit item 2). Still nothing: `mb poll off`, stop and report. Note: with A/B swapped, the ~0.3 s boot window (DE enabled by the module pull-ups) is a break at the panel; expected harmless (inferred).
5. [ ] **[TRANSMITS]** Multi-register test: `mb read 200 4`, `mb read 000 2`, `mb read 242 4` (exit item 5; `D5.*`).
6. [ ] **[TRANSMITS]** Polling has run since D.2 (`mb poll on` only if it was stopped). Watch the panel display for anything new, e.g. a WEB icon (exit item 9; web, flag 0). Record `D6.*`.
7. [ ] **[TRANSMITS]** (polling keeps running) Keypress correlation with logging on (default; `mb log on` only if switched off in D.3). Do each action and write its wall-clock time in the panel-action table: setpoint up, setpoint down (231 vs 008), each fan mode (201 bits 0-2 order), season change (233 values 3/5), standby on/off (201 bit 7) (exit item 8).
8. [ ] **[TRANSMITS]** Soak >= 1 h of polling; `mb stats` at 1, 10 and 60 min. Targets: `txns` >= 1000; `error rate (timeout+bad_frame+error)/txns` < 1 %; `offline entries 0`; panel normal: display, icons, error codes, local keys (exit item 7). Record `latency ms (answered): min / median / max` (exit item 6). With an undefined idle (MAX485-M, R7, weak bias) BADFRAME, `gap_bytes` and TIMEOUT may come from line noise; error rate >= 1 %: record and answer exit item 7 as "X % with MAX485-M + R7 at 3.3 V, conditional; repeat with MAX3485"; do not change firmware settings. Note for item 6: the logged duration (`dur_ms` of the `T` lines and the `mb stats` latency) is the whole transaction (8-byte request, about 8 ms at 9600, plus the slave's reply time plus the end-of-frame silence wait of about 20-30 ms with the default tick: 20 ms window measured from the last byte, (ceil(t3.5/tick)+1) x tick at 9600 / 100 Hz, plus up to one tick; 014), not the slave's pure response time.
   - 8a. [ ] At 1, 10 and 60 min also note from `mb status`: `max_byte_interval_us`, `gap_bytes` (line `link counters: ...`) and `faults since boot` (if `mb status` shows `link counters: n/a`: the B.4.5 `n/a` stop). One character lasts about 1042 us at 9600 8N1 (external); an interval well above about 2600 us (one character plus t1.5, external, approximate) means the answering slave pauses inside its replies. The node tolerates that (014) but records it. `gap_bytes` > 0 means bytes arrived between transactions (late replies or foreign traffic). Resolution is limited by task wake-up latency (inferred, tens of us). Which device answers on "A A B B -" is the panel per the wiki (inferred, ekw); record what is observed, do not conclude more. If any `BADFRAME` lines appear, note their times next to these values. State next to the values: `gap_bytes` is a lower bound (bytes dropped by the driver are not counted) and `max_byte_interval_us` is approximate (time-stamped when the task reads the byte, so it includes wake-up latency) (014 M3).
   - 8b. Slave turnaround: pure turnaround is **"unknown, no instrument (Q-HW1)"** (pre-filled); `max_byte_interval_us` stays unconfirmed and approximate. Indirect evidence, derived and not a measurement: from the saved `T` lines take the minimum and maximum `dur_ms` of `OK` lines with qty 1 (`D8b.dur_min`, `D8b.dur_max`). A qty-1 transaction contains the 8-byte request (about 8.3 ms), the turnaround, the 7-byte reply (about 7.3 ms; external) and the end-of-frame wait W of at least 20 ms (014), plus task latency, so the turnaround is **at most about `dur_ms` - 34 ms** (inferred, approximate). Log it always as "upper bound, derived, not measured". Derivation (inferred): 9600 8N1 (one character about 1.042 ms, external), 8-byte request about 8.33 ms, 7-byte `OK` qty-1 reply about 7.29 ms, W >= 20 ms, timestamps truncated to 1 ms (true duration < `dur_ms` + 1 ms), the reply starts after the request ended; hence T < `dur_ms` + 1 - (8.33 + 7.29 + 20) ms, about `dur_ms` - 34 ms. Sanity check: an `OK` qty-1 line should show `dur_ms` >= about 36 ms; a smaller value: record it (`D8b.small`), an assumption is wrong.
   - 8c. Known limit, no action: with the default 100 Hz tick every transaction ends with an end-of-frame wait of about 20-30 ms (20 ms window measured from the last byte, (ceil(t3.5/tick)+1) x tick at 9600 / 100 Hz, plus up to one tick), so the logged transaction time is longer than the slave's turnaround. Accepted; record the transaction times and the derived upper bound; do not treat the difference as a fault.
9. [ ] **[TRANSMITS]** (`mb poll on`) `mb poll off` for > 300 s (e.g. 10 min): watch panel and fancoil; `mb poll on`; `mb regs` for 105 bit 0 and, if readable, 104 bit 12 (exit item 10; flags).
10. [ ] **No TX; UART closed and re-opened, DE glitch risk not measured (B.6.d).** `mb silence 60` with the panel connected; wait 60 s for the result line (B.12). Expected `# silence check 60s ok, polling off: mb poll on to start`, i.e. activity 0 (exit item 3; the panel is a slave, inferred). Activity shows as `# silence check: activity <n>, latched until reboot`; judge by these lines, not by `mb status` (its `boot silence` line keeps the boot check's value). Polling stays off afterwards.
11. [ ] (after E, or straight after D.10 if E was skipped) `mb poll off`, unplug the node, mains off, disconnect the node, photograph, mains on; confirm panel and fancoil work as before and that "+ A B -" was untouched (`D11.*`). Disconnect A/B **before** re-flashing the Matter image (A.6 hard rule); never flash or run the Matter image with A/B wired.

---

## E. Control test (write image; between D.10 and D.11)

Task 019 (OS6). Not a phase-1 exit item: its results feed phase 3. The first FC06 frame ever sent to the panel goes out in E.2. Do E after D.10 and before D.11, with the same image and no re-flash. Until E.2 nobody types `mb write arm`: C and D run with the write image disarmed.

**Unverified assumptions (state them in the log, do not treat them as facts):** the panel accepts FC06 on "A A B B -" (the manual lists FC 03 and 06 but does not name the port); the FC06 reply is an exact echo of the request (external, Modbus specification); the reply arrives within the 500 ms timeout; the panel applies and displays a write at once; write endurance and EEPROM storage are not stated; whether the panel enforces the limit registers 202/203/245/246 is unknown; 247 flag n = bit n and "bit 4 = 5.0/40.0 refused" are inferred. **No bench slave exists: the host tests with a fake slave are the only evidence before this step.**

**Stop and disarm commands:** `mb write disarm` stops writes only; polling and reads continue. `mb poll off` (and `mb silence`, `mb addr`) stops **all** transmission at once, also the VERIFY of a command in flight; that command is then counted as `W <t> FAIL <reg> verify_lost failures <k>/3` (a failure, it counts toward the 3). Prefer `mb write disarm` unless the bus must be silent.

**E.0 Gate.** All of the following must hold. If any fails: skip E and record why (`E0.*`).
1. [ ] D.1-D.10 done without a STOP. `mb poll on` after D.10. Health ONLINE.
2. [ ] Two `mb stats` >= 10 min apart with polling running: (Δtimeout + Δbad_frame + Δerror) / Δtxns < 1 % and Δbad_frame = 0 (`E0.stats1`, `E0.stats2`, `E0.rate`, `E0.badframe`).
3. [ ] `mb regs`: 201, 231, 233, 202, 203 and 247 show `ok` and `fresh` (no `!range`). 247 flags 1, 2, 3, 6, 7, 8 = 0 (a set flag refuses arm: `A <t> REFUSED arm web_flag <n>`). If flag 5 is set: 245/246 `ok` and `fresh`. If flag 4 is set: the setpoint ends 5.0 and 40.0 are refused (`limit` in E.3). The initial 231 lies within [202, 203] and, if flag 5 is set, also within [245, 246] (else the write clamp refuses E.3/E.7). (`E0.regs`, `E0.web`).
4. [ ] The panel is normal. You stand at it and know how to set setpoint, fan mode, season and standby back by hand.
5. [ ] Write down the panel state, the raw values of 201/231/233/008 and the `log: on, dropped <n>` value of `mb status` (`E0.initial`).

**E.1** No flashing. E starts from the running image and the running boot. Do not reboot during E: a reboot clears the arm state and the restore set.

**E.2** [ ] **[TRANSMITS FC06 from here]** `mb write arm`. Expect `A <t> ARMED 120 s budget 40`, then `A <t> RESTORE_SET ...` equal to `E0.initial` (`E2.arm`). `A <t> REFUSED arm ...`: record it; retry once after 1 min, else skip E. `mb status` shows `write: armed <s> s left, budget <n>/40, failures 0/3` (the seconds are refreshed with each bus transaction, so up to a few seconds old). Re-arm (`mb write arm`) whenever `mb status` shows < 60 s left. Re-arming extends the time (`A <t> EXTENDED <s> s left budget <n>`) and does not reset the budget (40 frames per session; E needs about 13).

**E.3** [ ] `mb write sp <initial+0.5>` (decimal, e.g. `mb write sp 21.5`) if that value lies within the limits from E.0.3, else `mb write sp <initial-0.5>`; if neither lies within the limits, skip E.3 and record `E3.skip`. The value is never clamped: outside the live limits (202/203, with flag 5 also 245/246, with flag 4 the ends 5.0/40.0 too) it is refused with `W <t> REFUSED sp limit <lo>-<hi>` (`lo` and `hi` in raw tenths), nothing is sent, and you pick a value inside the range; a stale limit register is refused as `W <t> REFUSED sp stale <reg>`. Expect `W <t> CMD sp <raw>`, the `T` lines PRE, WRITE and VERIFY for 231, then `W <t> DONE 231 <old hhhh> -> <new hhhh> budget <n>`. Record the panel display and 008 at the next poll. After >= 5 s: `mb write sp <initial>`.

**E.4** [ ] `mb write standby on`: panel and fancoil go to standby (watch up to 60 s). Then `mb write standby off`.

**E.5** [ ] `mb write fan silent`, `night`, `max`, `auto`, >= 5 s apart (if `W <t> REFUSED fan pacing <ms>` is printed, wait and retype). Record the display and the fan sound for each. End at the initial mode. A `W <t> SKIP 201 unchanged <hhhh>` means the register already had that mode (no frame sent).

**E.6** [ ] (last) `mb write season <the other of 3/5>`: observe for at most 60 s, then `mb write season <initial>`. 0 is written only to restore an initial 0.

**E.7** [ ] `mb write restore`: it writes only the registers that differ from the `RESTORE_SET`, in the order 201, 233, 231, >= 5 s apart (nothing differs: only `W <t> CMD restore` is printed). Then `mb write disarm` -> `A <t> DISARMED user`. `mb regs` and the panel must equal `E0.initial` (`E7.final`, `E7.*`).

**E.8** [ ] (optional) Change the setpoint by one step at the panel. The next poll shows it in 231/008 (bus is truth). Set it back by hand.

**E.9** [ ] Leave the node disarmed (`mb status`: `write: disarmed, ...`). Check `log: on, dropped <n>`: `<n>` must equal the value at E.0.5. The log ring drops lines when it is full (`emit` writes only to the ring): a dropped `T`, `W` or `A` line is missing from the console and from the saved output. If `<n>` rose, record `E9.dropped` and mark the saved log and the "Control actions (E)" table as incomplete. Continue with D.11 (disconnect A/B before any re-flash). Save the complete serial output and fill the "Control actions (E)" table.

**STOP E.**
- `W <t> FAIL ...` disarms the node: record it (`E.fail`) and check the panel. Re-arm **once**, only for `mb write restore`. A second FAIL: stop E and restore by hand.
- `W <t> FAIL <reg> write_TIMEOUT` (or `pre_TIMEOUT`, `verify_TIMEOUT`) followed by `gap_bytes` > 0 in `mb status` (`link counters: gap_bytes <n> ...`): a late reply arrived between transactions (D.8a). Record `E.gap` with `faults since boot: <n>`, do not re-arm, stop E and go back to the architect.
- `A <t> LATCHED failures 3`: stop E, restore by hand, no reboot to retry.
- `A <t> DISARMED health|link|web_flags`: stop E and record it. Restore once health is ONLINE again.
- `A <t> DISARMED timeout` between commands is normal: re-arm.
- Panel anomaly, error code or unexpected fancoil behaviour: `mb write disarm`, `mb poll off`, restore by hand, A.5.

---

## Exit items (arch s1)

Phase 1 is done when each item has an answer or "unknown + reason".

| # | Item (short) | Steps | Topo item | Log fields |
|---|---|---|---|---|
| 1 | Panel slave address: on the panel and by a bus reply | C.5, D.3 | 1 | exit table, `C5.addr` |
| 2 | A/B polarity: valid replies at 9600 8N1 | D.3, D.4 | 5, 1b | exit table |
| 3 | Silence check saw no foreign traffic | D.2, D.10 | 1 | `D2.activity`, exit table |
| 4 | Every documented register: OK + raw/scaled, exception + code, or timeout | D.3 | 4 | exit table |
| 5 | FC03 qty > 1 works? | D.5 | 1b | `D5.*`, exit table |
| 6 | Response time min/median/max over >= 1000 transactions as whole-transaction time `dur_ms` (+ inter-byte interval, D.8a; turnaround upper bound about `dur_ms` - 34 ms "upper bound, derived, not measured", D.8b; pure turnaround "unknown, no instrument (Q-HW1)"). Decidable in phase 1 | D.8, D.8a, D.8b | 2 | `D8.*`, `D8b.*`, exit table |
| 7 | >= 1 h polling, error rate < 1 %, no panel anomaly (+ `gap_bytes`, `faults since boot`) | D.8, D.8a | 2 | `D8.*`, link-counter table, exit table |
| 8 | Keypress correlation 231/008, 201 bits 0-2, 233, 201 bit 7 | D.7 | - | panel-action table, exit table |
| 9 | Panel shows anything when polled (e.g. WEB icon) | D.2, D.6 | - | `D6.*`, exit table |
| 10 | Panel behaviour when polling stops > 300 s and resumes | D.9 | 2 | `D9.*`, exit table |
| 11 | A-A/B-B continuity, A-B resistance, "-" vs node GND voltage, U1/supply/RO level, "-" shared with CP; record module type (MAX485-M), VCC 3.3 V, R7 fitted, single 10 kOhm DE pull-down, RX direct | B.1, C.2-C.4 | 3, 3a, 7 | `B1.*`, `C2.*`..`C4.*`, exit table |

You personally verify exit items 1, 2, 7, 8, 9, 10, 11 and that "+ A B -" was untouched (arch s1).

B.6 (DE timing) and D.8b (slave turnaround) are **not exit items**. Their entries "not measured (no instrument, Q-HW1)" and "not done in phase 1, repeat at phase-2 bring-up (Q-HW2)" do not block any of items 1-11. Item 6 is answered as whole-transaction time (D.8 note) with the derived turnaround upper bound of D.8b; the slave's pure response time stays unknown, reason "no instrument, Q-HW1". DE timing stays a residual risk carried into phase 2 (arch s8); the phase-2 DE re-check method is an open question for the phase-2 specs.

## Finish

Save the filled log and the complete serial output to `Knowledgebase/raw/` (e.g. `raw/phase1-probe-<date>.md`, `raw/phase1-probe-<date>.log`) and ask `wiki-keeper` to run /capture. The `.log` can be copied to `firmware/test/fixtures/probe-<date>-capture.log` (011).
