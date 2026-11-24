# Phase-1 probe: bench and on-site checklist

For the person at the bench and on site. Task spec: `tasks/008-p1-bringup-hil-checklist.md`. Fill in `measurement-log-template.md` as you go (field ids such as `B4.x` point to its rows). Wiki short names used below: arch = `concepts/firmware-architecture` (section number after "s"), topo = `concepts/bus-topology`, ekw = `entities/ekwhctrl1`, addr = `concepts/device-addressing`, max485 = `entities/max485`, flags = `concepts/flag-registers`, web = `concepts/web-restriction-flags`. Facts marked "external" are not in the wiki. Facts marked "inferred" are inferences, not facts. Image: phase-1 firmware, commit b8ad00d (task 006).

Tick each box when done. If a step says STOP: write down what you saw in the log ("unexpected observations") and go back to the architect; do not continue.

## Console commands used (all exist in the image)

`mb status`, `mb regs`, `mb read <reg> [qty 1-4]`, `mb probe`, `mb poll on|off`, `mb silence [seconds 1-600]`, `mb stats`, `mb addr <1-247>`, `mb log on|off`. The prompt is `dmt>`.

Line formats printed by the image:
- `T <t_start_ms> <dur_ms> <addr> <kind> <reg> <qty> <result> [values | exception code]`; kind is POLL, RETRY, HEALTH, SWEEP or MANUAL; result is `OK`, `TIMEOUT`, `EXC`, `BADFRAME` or `ERROR`; values are 4-digit hex words (raw, unscaled), an exception code is 2 hex digits.
- `H <t_ms> <health>`; health is STOPPED, ONLINE, DEGRADED, OFFLINE or LATCHED.
- `mb status` prints, in this order: `bus: uart ...`, `addr: kconfig <k> current <a>`, `pacing: ...`, `periods: ...`, `health: <h>`, `boot silence: <ms> ms, activity <n|running>`, `log: ...`, `bus_task stack high-water (minimum free): <n> bytes`, `write: compiled out`, then either `link counters: gap_bytes <n> max_byte_interval_us <n>` followed by `faults since boot: <n>` (and `link closed: mb silence or reboot to recover` only after a port fault), or just `link counters: n/a`. **No `faults since boot` line follows `n/a`.** `n/a` is expected only in the first moments after reset (inferred). Type `mb status` again; if it still shows `n/a` after the first `H` line, record it and STOP, back to the architect (the "`n/a` stop", B.4.5).
- `mb stats` prints `txns <n>: ok .. timeout .. exception .. bad_frame .. error ..`, a `kinds:` line, `error rate (timeout+bad_frame+error)/txns: <x.y> %`, `offline entries <n>`, `latency ms (answered): min <n> median <n> max <n>`.

---

## A. Safety (read first, tick before any wiring step)

- [ ] A.1 The fancoil and the wall panel are mains powered. Open housings, connect or disconnect wires and measure resistance or continuity **only with mains off**, following the appliance manual.
- [ ] A.2 **Never touch the "+ A B -" block** (panel to fancoil) **and never the CP terminal** of the "A A B B -" block: do not loosen, measure on or reconnect them. Only one A, one B and "-" of "A A B B -" are used.
- [ ] A.3 Connect node GND to "-" **only after** step C.4 showed "-", A and B within -7..+12 V of node GND (arch s4, RS485 common-mode range, external). The node runs on normal USB power. Whether the panel's low-voltage side is isolated from mains is not stated in the wiki (arch s8).
- [ ] A.4 Live voltage readings (C.4) only on the low-voltage terminals with mains parts covered. If that is not possible, skip them, leave node GND unconnected and say so in the log (`C4.skip`).
- [ ] A.5 **Abort** (unexpected panel or fancoil behaviour, error code, icon, smell, heat): type `mb poll off`, unplug the node's USB power (DE is then held low by the 10 kOhm pull-down and the MAX485 is unpowered), mains off before touching any wire. Record what happened. Unplugging USB alone also stops all transmission.
- [ ] A.6 **The node transmits on its own.** After a clean boot silence check, polling starts automatically and the first frame goes out about 5 s after power-up. Therefore: connect or disconnect A/B only with the node unpowered and mains off (A.1); open the serial console before powering the node while it is wired, ready to type `mb poll off`. `mb poll off` typed within the first 5 s prevents the first frame; later it stops polling after the current transaction.

---

## B. Bench (before going on site)

Pacing rule (B.12): while `mb silence N` runs the node executes no command. Wait N s, until its result line, before typing more. The command queue holds 8 entries; extra commands print `command not queued: ESP_ERR_TIMEOUT`.

**B.1 Wiring** (arch s4; max485)
1. [ ] TX GPIO to DI; RO through a level shift or divider to RX GPIO (RO is a 5 V output unless U1 is a 3.3 V part); DE and /RE tied to the DE GPIO, with 10 kOhm from DE to GND.
2. [ ] Write the GPIO numbers into `B1.gpio`. They must equal `CONFIG_DMT_BUS_TX_GPIO` / `RX_GPIO` / `DE_GPIO` of the flashed image (defaults 5 / 4 / 6 are placeholders). Record U1 marking and module supply (`B1.u1`, `B1.supply`).

**B.2 Unpowered module** (no USB): measure A-B resistance, DE to /RE continuity, DE to GND (expect 10 kOhm). Record `B2.*`.

**B.3 Flash and identify**
1. [ ] Flash the phase-1 image. Record the git commit (`B3.commit`) and that `build-verifier` reported lint and ELF check passed (`B3.verifier`).
2. [ ] Open the console. The first console line is `# dmt-probe v1 addr=<a> baud=<b> fw=<version>`. Record `a`, `b` and `fw` (`B3.addr`, `B3.baud`, `B3.fw`). Save the complete serial output from here on.

**B.4 Powered, bus not connected, no slave**
1. [ ] **[TRANSMITS]** (polling starts by itself after a clean 5 s check, A.6). Power-cycle. Within the 5 s check: `mb status` shows `boot silence: 5000 ms, activity running` (the ms value is the configured one).
2. [ ] **[TRANSMITS]** After about 5 s the output shows `# silence check: started 5 s`, then `# silence ok, polling on: mb poll off to stop` and `H <t> ONLINE`. Without a slave then `H <t> DEGRADED` and, at the 5th consecutive failure, `H <t> OFFLINE`.
3. [ ] **[TRANSMITS]** (polling still running until step 4) `mb status` shows `boot silence: <ms> ms, activity 0`, `write: compiled out`, `link counters: gap_bytes 0 ...` and `faults since boot: 0` (if it shows `link counters: n/a` instead: the B.4.5 `n/a` stop). Record `B4.activity`, `B4.gap`, `B4.maxint`, `B4.faults`.
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

---

## C. On site, mains off, node not connected

1. [ ] Photograph both terminal blocks before any change (`C1.photo`).
2. [ ] Continuity A-A and B-B of the doubled terminals (`C2.aa`, `C2.bb`); choose one A and one B (ekw, topo item 3).
3. [ ] Resistance A-B at the panel port (`C3.ab`). Termination rule (arch s4): about 120 Ohm: fit your 120 Ohm at the node end; high or open: start without and judge by the error rate of exit item 7. Resistance "-" to the CP terminal (`C3.cp`, shared "-"?), measured without touching the CP wiring.
4. [ ] Mains on, node powered but not wired to the panel: with the meter between node GND and each panel terminal, DC voltage of "-", A, B (`C4.minus`, `C4.a`, `C4.b`); then A-B at the panel (`C4.ab`). Mains off. If unsure whether the panel low-voltage side is isolated, skip C.4 (A.4).
5. [ ] Confirm the panel address on the panel itself: standby, hold AUTO 5 s, read the value, wait 10 s without changing anything (addr, p6). Record `C5.addr`. If it differs from the image's address (`B3.addr`), use `mb addr <n>` in D.2 (never scan).

---

## D. Connect and probe

1. [ ] Mains off, node unpowered: connect node A/B (and GND to "-" only if A.3 allows) with short leads, no star; fit 120 Ohm only if C.3 says so. Mains on.
2. [ ] **[TRANSMITS]** (automatic start after a clean silence check, and `mb addr` / `mb poll on` below). Open the serial console and start saving the complete serial output, then power the node. The boot silence check runs 5 s; afterwards `mb status` must show `boot silence: <ms> ms, activity 0` (exit item 3; `D2.activity`).
   - **If activity > 0 the node latches and will not transmit** (`# silence check: activity <n>, latched until reboot`, `# latched until reboot: no transmission`, `H <t> LATCHED`): do not reboot to retry. Abort and record; foreign traffic or an unbiased line, both go to the architect.
   - With activity 0 polling starts by itself (`# silence ok, polling on: mb poll off to stop`, `H <t> ONLINE`): watch the panel from the first frame (exit item 9).
   - If C.5 gave an address other than the image's: always `mb poll off`, then `mb addr <n>`, then `mb poll on` (a bare `mb addr` keeps polling at the wrong address). Type `mb poll off` within the 5 s window if possible, otherwise at once.
3. [ ] **[TRANSMITS]** Logging is on from power-up, so the saved output holds every `T` line from the first frame. Type `mb log off` only if the console is too noisy (then `mb log on` before D.7). Then `mb probe` and `mb regs` (exit items 1, 2, 4). If `H <t> OFFLINE` appears before or during `mb probe`, nothing answers: go to D.4.
4. [ ] **[TRANSMITS]** (repeats D.2-D.3). If nothing answers: `mb poll off`, unplug the node, mains off, swap A/B on the node side only, mains on, repeat D.2-D.3 (exit item 2). Still nothing: `mb poll off`, stop and report.
5. [ ] **[TRANSMITS]** Multi-register test: `mb read 200 4`, `mb read 000 2`, `mb read 242 4` (exit item 5; `D5.*`).
6. [ ] **[TRANSMITS]** Polling has run since D.2 (`mb poll on` only if it was stopped). Watch the panel display for anything new, e.g. a WEB icon (exit item 9; web, flag 0). Record `D6.*`.
7. [ ] **[TRANSMITS]** (polling keeps running) Keypress correlation with logging on (default; `mb log on` only if switched off in D.3). Do each action and write its wall-clock time in the panel-action table: setpoint up, setpoint down (231 vs 008), each fan mode (201 bits 0-2 order), season change (233 values 3/5), standby on/off (201 bit 7) (exit item 8).
8. [ ] **[TRANSMITS]** Soak >= 1 h of polling; `mb stats` at 1, 10 and 60 min. Targets: `txns` >= 1000; `error rate (timeout+bad_frame+error)/txns` < 1 %; `offline entries 0`; panel normal: display, icons, error codes, local keys (exit item 7). Record `latency ms (answered): min / median / max` (exit item 6). Note for item 6: the logged duration (`dur_ms` of the `T` lines and the `mb stats` latency) is the whole transaction (8-byte request, about 8 ms at 9600, plus the slave's reply time plus the end-of-frame silence wait of about 20-30 ms with the default tick: 20 ms window measured from the last byte, (ceil(t3.5/tick)+1) x tick at 9600 / 100 Hz, plus up to one tick; 014), not the slave's pure response time.
   - 8a. [ ] At 1, 10 and 60 min also note from `mb status`: `max_byte_interval_us`, `gap_bytes` (line `link counters: ...`) and `faults since boot` (if `mb status` shows `link counters: n/a`: the B.4.5 `n/a` stop). One character lasts about 1042 us at 9600 8N1 (external); an interval well above about 2600 us (one character plus t1.5, external, approximate) means the answering slave pauses inside its replies. The node tolerates that (014) but records it. `gap_bytes` > 0 means bytes arrived between transactions (late replies or foreign traffic). Resolution is limited by task wake-up latency (inferred, tens of us). Which device answers on "A A B B -" is the panel per the wiki (inferred, ekw); record what is observed, do not conclude more. If any `BADFRAME` lines appear, note their times next to these values. State next to the values: `gap_bytes` is a lower bound (bytes dropped by the driver are not counted) and `max_byte_interval_us` is approximate (time-stamped when the task reads the byte, so it includes wake-up latency) (014 M3).
   - 8b. Slave turnaround: pure turnaround is **"unknown, no instrument (Q-HW1)"** (pre-filled); `max_byte_interval_us` stays unconfirmed and approximate. Indirect evidence, derived and not a measurement: from the saved `T` lines take the minimum and maximum `dur_ms` of `OK` lines with qty 1 (`D8b.dur_min`, `D8b.dur_max`). A qty-1 transaction contains the 8-byte request (about 8.3 ms), the turnaround, the 7-byte reply (about 7.3 ms; external) and the end-of-frame wait W of at least 20 ms (014), plus task latency, so the turnaround is **at most about `dur_ms` - 34 ms** (inferred, approximate). Log it always as "upper bound, derived, not measured". Derivation (inferred): 9600 8N1 (one character about 1.042 ms, external), 8-byte request about 8.33 ms, 7-byte `OK` qty-1 reply about 7.29 ms, W >= 20 ms, timestamps truncated to 1 ms (true duration < `dur_ms` + 1 ms), the reply starts after the request ended; hence T < `dur_ms` + 1 - (8.33 + 7.29 + 20) ms, about `dur_ms` - 34 ms. Sanity check: an `OK` qty-1 line should show `dur_ms` >= about 36 ms; a smaller value: record it (`D8b.small`), an assumption is wrong.
   - 8c. Known limit, no action: with the default 100 Hz tick every transaction ends with an end-of-frame wait of about 20-30 ms (20 ms window measured from the last byte, (ceil(t3.5/tick)+1) x tick at 9600 / 100 Hz, plus up to one tick), so the logged transaction time is longer than the slave's turnaround. Accepted; record the transaction times and the derived upper bound; do not treat the difference as a fault.
9. [ ] **[TRANSMITS]** (`mb poll on`) `mb poll off` for > 300 s (e.g. 10 min): watch panel and fancoil; `mb poll on`; `mb regs` for 105 bit 0 and, if readable, 104 bit 12 (exit item 10; flags).
10. [ ] **No TX; UART closed and re-opened, DE glitch risk not measured (B.6.d).** `mb silence 60` with the panel connected; wait 60 s for the result line (B.12). Expected `# silence check 60s ok, polling off: mb poll on to start`, i.e. activity 0 (exit item 3; the panel is a slave, inferred). Activity shows as `# silence check: activity <n>, latched until reboot`; judge by these lines, not by `mb status` (its `boot silence` line keeps the boot check's value). Polling stays off afterwards.
11. [ ] `mb poll off`, unplug the node, mains off, disconnect the node, photograph, mains on; confirm panel and fancoil work as before and that "+ A B -" was untouched (`D11.*`).

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
| 11 | A-A/B-B continuity, A-B resistance, "-" vs node GND voltage, U1/supply/RO level, "-" shared with CP | B.1, C.2-C.4 | 3, 3a, 7 | `B1.*`, `C2.*`..`C4.*`, exit table |

You personally verify exit items 1, 2, 7, 8, 9, 10, 11 and that "+ A B -" was untouched (arch s1).

B.6 (DE timing) and D.8b (slave turnaround) are **not exit items**. Their entries "not measured (no instrument, Q-HW1)" and "not done in phase 1, repeat at phase-2 bring-up (Q-HW2)" do not block any of items 1-11. Item 6 is answered as whole-transaction time (D.8 note) with the derived turnaround upper bound of D.8b; the slave's pure response time stays unknown, reason "no instrument, Q-HW1". DE timing stays a residual risk carried into phase 2 (arch s8); the phase-2 DE re-check method is an open question for the phase-2 specs.

## Finish

Save the filled log and the complete serial output to `Knowledgebase/raw/` (e.g. `raw/phase1-probe-<date>.md`, `raw/phase1-probe-<date>.log`) and ask `wiki-keeper` to run /capture. The `.log` can be copied to `firmware/test/fixtures/probe-<date>-capture.log` (011).
