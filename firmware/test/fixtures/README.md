# Probe-log fixtures

Replay fixtures for the end-to-end host tests (`host/main/test_e2e_poll.c`). The fake slave
(`host/main/fake_slave.c`) loads one with `fake_load_log` and answers each request from it.

## Format

`txn_log` format v1, normative in `tasks/010-reg-model-poll-sched.md` (section "`txn_log.h`"):
one `T <t_start_ms> <dur_ms> <addr> <kind> <reg> <qty> <res>[ <data>]` line per transaction.
`H` lines and any other line are ignored by the replay.

Fixture header (extends the 010 header, a comment to the parser):

    # dmt-probe v1 addr=<a> baud=<b> fw=<x> source=synthetic|capture origin=<free text>

Replay rule: per `(addr, reg, qty)` the recorded results are served in order, then the last one
repeats; a key that is not recorded times out. `dur_ms` is the response latency.

## Naming

- `<what>-synthetic.log`: hand-written (`healthy-synthetic.log`, `starred-timeout-synthetic.log`);
  one line per documented register, values from the `daikin_regs` defaults (000 = `00d7`, 008 = `00c8`).
- `probe-<date>-capture.log`: a real capture from site (`source=capture`), `<date>` = `YYYY-MM-DD`.

## Turning an on-site serial log (task 008) into a fixture

1. Save the serial console output of the probe run to a file.
2. Keep every `T ...` line (and `H ...` lines if wanted); other lines (ESP log output, prompts) may stay,
   the replay ignores them. No conversion is needed.
3. Make the first line the header with `source=capture` and `origin=` describing the site and date; replace
   the `fw=` and `baud=` values if the capture lacks them.
4. Name it `probe-<date>-capture.log`, put it in this directory and check that it loads
   (a malformed `T`/`H` line makes `fake_load_log` return -1 and print its line number).
