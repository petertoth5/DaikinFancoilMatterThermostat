# Phase-1 probe measurement log

Copy this file, fill it in, and save it with the complete serial output as described at the end. Field ids refer to `checklist.md`. Units are stated in each header. Cells already filled are pre-filled answers: do not overwrite them with a pass. Indirect evidence is not a measurement.

## Header

| Field | Value |
|---|---|
| Date | |
| Git commit of the image (`B3.commit`) | |
| `build-verifier`: lint and ELF check passed y/n (`B3.verifier`) | |
| `fw=` of first console line (`B3.fw`) | |
| TX / RX / DE GPIO (`B1.gpio`) | / / |
| U1 marking (`B1.u1`) | |
| Module supply, V (`B1.supply`) | |
| Address in `# dmt-probe` line (`B3.addr`), baud (`B3.baud`) | |
| Address used on site | |
| Termination fitted y/n (and where) | |
| Mains-isolation / GND connected y/n (`C4.skip` reason if skipped) | |

## Bench

| Field | Value | Unit |
|---|---|---|
| `B2.ab` A-B resistance, module unpowered | | Ohm |
| `B2.derel` DE to /RE continuity y/n | | |
| `B2.degnd` DE to GND | | Ohm |
| `B4.activity` boot silence activity (no slave) | | count |
| `B4.gap` `gap_bytes` | | bytes |
| `B4.maxint` `max_byte_interval_us` | | us |
| `B4.faults` `faults since boot` ("n/a" if `mb status` shows `link counters: n/a`: apply the checklist B.4.5 `n/a` stop) | | count |
| `B4.de_v` DE level after `mb poll off` (multimeter, DC; indirect) | | V |
| `B4.results` all `T` lines TIMEOUT y/n; any BADFRAME y/n (STOP if y) | | |
| `B7.result` RX pull-up on: all TIMEOUT, no BADFRAME, `gap_bytes` 0 y/n | | |
| `B8.console` console used (UART0 / USB-Serial-JTAG) | | |
| `B8.bus` `mb status` first line shows `bus: uart 1` y/n | | |
| `B9.seen` "queue free spaces" line seen y/n | | |
| `B10.after_boot` bus_task stack high-water, after first `H` line | | bytes |
| `B10.after_silence` bus_task stack high-water, after `mb silence 1` | | bytes |
| `B11.t_lines` abort path: any `T` line before next command y/n | | |
| `B11.status` abort path: `addr: kconfig <k> current <a>` (a not 0), `health: STOPPED`, `txns 0` y/n | | |
| `B13.result` pending read cancelled (no `MANUAL 000` line) y/n | | |
| `B14.result` latched run (lines, refusals, DE low) or "not done" | | |
| `B15.*` port-fault line(s) and `mb status` before/after, if any | | |
| `B16.activity` B.16 RO direct: boot silence activity per boot (latch: reboot, at most 2 repeats; e.g. `1, 0`) | | count |
| `B16.results` B.16: all `T` lines TIMEOUT y/n; any BADFRAME y/n (STOP if y) | | |
| `B16.gap` B.16 `gap_bytes` (> 0: STOP, back to the architect) | | bytes |

### B.5 silence cycles

| Field | Value |
|---|---|
| `B5.faults_before` `faults since boot` before ("n/a": B.4.5 stop) | |
| `B5.run1` `# silence check 5s ok, polling off: mb poll on to start` y/n, no `# mb_link: ... failed` (a `# mb_link: ... failed` line: record, STOP, back to the architect) | |
| `B5.run2` | |
| `B5.run3` | |
| `B5.run4` | |
| `B5.run5` | |
| `B5.faults_after` `faults since boot` after (changed: STOP, back to the architect) | |
| `B5.probe` `mb probe` no slave: all SWEEP TIMEOUT, health STOPPED y/n | |
| `B5.de_v` DE level after each `mb silence 5`, V (indirect) | |
| `B5.heap` free heap over five cycles | not checked (no heap output in the 006 image) |

### B.6 DE timing

Not decided in phase 1. Entries below are pre-filled; none is a pass.

| Item | Result |
|---|---|
| DE low at boot without a glitch | not measured (no instrument, Q-HW1) |
| No glitch when the port opens (`uart_set_pin`, after the silence check) | not measured (no instrument, Q-HW1) |
| Rising edge before the first start bit of each request | not measured (no instrument, Q-HW1) |
| Falling edge within about 100 us after the last stop bit | not measured (no instrument, Q-HW1) |
| Worst-case DE-fall delay | not measured (no instrument, Q-HW1) |
| No DE pulse during `mb silence 5` and the close before it | not measured (no instrument, Q-HW1) |
| DE with concurrent flash/NVS activity | not done in phase 1, repeat at phase-2 bring-up (Q-HW2) |

Indirect evidence (multimeter DC readings average the level; they cannot show a glitch or an edge time):

| Field | Value | Unit |
|---|---|---|
| `B6.de_boot` DE level during the 5 s boot window | | V |
| `B6.de_poll_off` DE level after `mb poll off` | | V |
| `B6.de_silence` DE level after each `mb silence 5` | | V |
| `B6.health_before` health before the sweep (`mb status`; must be STOPPED) | | |
| `B6.de_probe_scrolling` reading taken while `T ... SWEEP` lines scrolled y/n | | |
| `B6.de_probe` averaged DE level during `mb probe`, no slave (healthy about 0.05 V; close to 3.3 V: hard stop, unplug USB at once) | | V |
| `D8.ok` / `D8.timeout` / `D8.badframe` counts from `mb stats` on site (indirect only) | / / | count |

## On site

| Field | Value | Unit |
|---|---|---|
| `C1.photo` photo file names | | |
| `C2.aa` continuity A-A | | |
| `C2.bb` continuity B-B | | |
| Chosen A and B terminal | | |
| `C3.ab` A-B resistance at panel port | | Ohm |
| `C3.cp` "-" to CP resistance (shared "-"?) | | Ohm |
| `C4.minus` "-" vs node GND, DC | | V |
| `C4.a` A vs node GND, DC | | V |
| `C4.b` B vs node GND, DC | | V |
| `C4.ab` A-B voltage at panel | | V |
| `C4.skip` readings skipped (A.4) y/n, node GND left unconnected | | |
| `C5.addr` address shown on the panel | | |
| `D2.activity` boot silence activity on the real bus | | count |
| `D2a.ab_idle` (optional, A.4 conditions) A-B DC at the panel after a clean boot; >= 0.2 V = panel biases | | V |
| `D5.*` `mb read 200 4`, `000 2`, `242 4`: result lines | | |
| `D6.*` panel display anomalies when polled (e.g. WEB icon) | | |
| `D9.*` panel/fancoil behaviour with polling off > 300 s, duration | | s |
| `D9.regs` 105 bit 0 / 104 bit 12 after resume | / | |
| `D10.result` `mb silence 60` result line | | |
| `D11.*` panel and fancoil normal after disconnect; "+ A B -" untouched y/n | | |

### D.8 soak: `mb stats` and link counters

Note for the latency columns: `dur_ms` is the whole transaction (request + slave reply time + end-of-frame wait of about 20-30 ms), not the slave's response time. `gap_bytes` is a lower bound; `max_byte_interval_us` is approximate (includes task wake-up latency).

| Time since D.2 (min) | txns | ok | timeout | exception | bad_frame | error | error rate % | offline entries | latency min / median / max (ms) | `max_byte_interval_us` | `gap_bytes` | `faults since boot` |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | | | | | | | | | | | | |
| 10 | | | | | | | | | | | | |
| 60 | | | | | | | | | | | | |

"n/a" in the last three columns is allowed only with the `n/a` stop rule of the link-counter log below.

BADFRAME line times (`T` lines), if any:

### D.8b turnaround

| Field | Value |
|---|---|
| Pure slave turnaround (end of request to first reply start bit) | unknown, no instrument (Q-HW1) |
| Cross-check of `max_byte_interval_us` against a real reply's longest inter-byte gap | not measured (no instrument, Q-HW1) |
| `D8b.dur_min` minimum `dur_ms` of `OK` qty-1 `T` lines | ms |
| `D8b.dur_max` maximum `dur_ms` of `OK` qty-1 `T` lines | ms |
| Turnaround at most about min `dur_ms` - 34 ms (using min / max above) | / ms, upper bound, derived, not measured |
| `D8b.small` any `OK` qty-1 `dur_ms` < about 36 ms y/n (if y: record, an assumption is wrong) | |

### Panel actions (D.7)

| Wall-clock time | Action | Registers changed | Old -> new |
|---|---|---|---|
| | | | |
| | | | |
| | | | |
| | | | |
| | | | |
| | | | |

### Link-counter log (D.8a, B.4)

If `mb status` shows `link counters: n/a`, write "n/a" in the three counter columns (no `faults since boot` line is printed then) and apply the checklist B.4.5 `n/a` stop: type `mb status` again; still `n/a` after the first `H` line: record and STOP, back to the architect.

| Time | `max_byte_interval_us` (us) | `gap_bytes` | `faults since boot` | Note |
|---|---|---|---|---|
| | | | | |
| | | | | |
| | | | | |

## Exit items 1-11

Answer each, or "unknown + reason". B.6 and D.8b are not exit items; their pre-filled entries block none of them.

| # | Question | Answer (value / OK / unknown + reason) | Verified by user (items 1, 2, 7, 8, 9, 10, 11) |
|---|---|---|---|
| 1 | Panel slave address (on the panel `C5.addr`; by a bus reply) | | |
| 2 | A/B polarity: valid replies at 9600 8N1 (swapped y/n) | | |
| 3 | Silence check saw no foreign traffic (boot and `mb silence 60`) | | n/a |
| 4 | Every documented register: OK + raw/scaled, exception + code, or timeout (attach `mb regs` output) | | n/a |
| 5 | FC03 qty > 1 works | | n/a |
| 6 | Response time as whole-transaction `dur_ms` min / median / max over >= 1000 txns; derived turnaround upper bound (D.8b); pure turnaround | | n/a |
| 7 | >= 1 h polling, error rate < 1 %, no panel anomaly | | |
| 8 | Keypress correlation 231/008, 201 bits 0-2, 233, 201 bit 7 | | |
| 9 | Panel shows anything when polled | | |
| 10 | Panel behaviour when polling stops > 300 s and resumes | | |
| 11 | A-A/B-B continuity, A-B resistance, "-" vs node GND, U1/supply/RO level, "-" shared with CP | | |

Termination decision (C.3) and result by error rate:

## Unexpected observations

(free text)

## Finish

Save the filled log and the complete serial output to `Knowledgebase/raw/` (e.g. `raw/phase1-probe-<date>.md`, `raw/phase1-probe-<date>.log`) and ask `wiki-keeper` to run /capture. The `.log` can be copied to `firmware/test/fixtures/probe-<date>-capture.log` (011).
