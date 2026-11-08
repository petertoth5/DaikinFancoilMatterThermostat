# Task specs

Written by `architect` from architecture **v0.3**, approved by the user on 2026-11-08 (Q1-Q10 answered with the recommendations; wiki: `concepts/firmware-architecture`). Spec-level choices answered by the user on 2026-11-09: automatic polling start after a clean boot silence check, normal USB power on site, all other listed choices accepted as written; follow-up answers the same day: commands typed during the boot silence check run before the first poll, polling stays off after an on-demand `mb silence` until `mb poll on`, `mb log` default on (006, 008). Implement one task per `firmware-dev` pass; after each: `build-verifier`, then `firmware-reviewer`. Bus-touching tasks (marked below) get an **independent `firmware-reviewer` review** of the spec result before the next task starts.

## Gate
- Released: **phase 1 (on-site read probe)** only, as a gate exception (Q8).
- Phase 2 (Matter read-only mirror) and phase 3 (Matter writes behind `CONFIG_DMT_BUS_WRITE`) specs are written **only after the filled phase-1 probe log** (task 008, exit items 1-11) is in `Knowledgebase/raw/` and captured into the wiki.

## Order (phase 1)
| # | Task | Depends on | Model | Bus-touching | Status |
|---|---|---|---|---|---|
| 001 | [Skeleton, Kconfig, bus-access lint, host-test runner](001-skeleton-kconfig-lint.md) | - | sonnet | yes (lint) | spec ready |
| 009 | [`daikin_regs` register table, scaling, whitelists](009-daikin-regs.md) | 001 | sonnet | yes (read whitelist) | spec ready |
| 010 | [`reg_model` + `poll_sched`, health, `txn_log`](010-reg-model-poll-sched.md) | 009 | sonnet | yes (pacing) | spec ready |
| 002 | [esp-modbus `==2.1.4` master spike, `rs485_port`, `mb_link` read, ELF check](002-esp-modbus-master-spike-mb-link.md) | 001, 009 | sonnet | yes | spec ready |
| 006 | [`bus_task` poll loop, probe console, `app_main`](006-bus-task-console.md) | 002, 010 | sonnet | yes | spec ready |
| 011 | [Fake slave, probe-log fixtures, end-to-end host tests](011-fake-slave-host-tests.md) | 010 | sonnet | yes (evidence) | spec ready |
| 008 | [On-site probe checklist + measurement log (document)](008-p1-bringup-hil-checklist.md) | 006, 002 | sonnet | yes (procedure) | spec ready |

Parallelism: 009 and 010 may run in parallel with 002 (002 needs only `daikin_regs_read_allowed` from 009). 011 may start as soon as 010 is done. 008 may be drafted early and is finalised after 006 and the 002 spike report.

Escalations that stop the pipeline (user decision): 002 S3 (timeout vs exception not distinguishable: options E1/E2), 002 S5/S6/S12 deviations; 008 boot silence activity on the bench or on site.

## Retired (v0.3, 2026-11-08; files kept as short notes so numbering and history stay clear)
- 003 `rtu_sniff` -> replaced by 002 (`mb_link` over esp-modbus)
- 004 `bus_monitor` -> replaced by 010
- 005 `rs485_rx` -> replaced by 002 (`rs485_port`)
- 007 replay fixtures -> replaced by 011
- `002-esp-modbus-crc-adapter-spike.md` is a pointer to the renamed 002 file; remove it with `git rm` after the rename is committed.

## Bus-safety rules, restated (architecture section "3. Bus-safety rules")
1. Node = sole Modbus master on EKWHCTRL1 "A A B B -"; "+ A B -" and CP are never touched.
2. FC03 only in phase 1, addresses 1..247, never broadcast; registers from the `daikin_regs` read whitelist (qty 1 automatic, 1-4 manual); no write source exists (`CONFIG_DMT_BUS_WRITE` default n, build fails if set).
3. Boot silence check (5 s) and on-demand `mb silence` (default 60 s): any activity -> latched refusal to transmit until reboot. After a clean boot check polling starts automatically (first frame about 5 s after power-up; user decision 2026-11-09); `mb poll off` / `mb poll on` stop and resume it.
4. Pacing: one transaction at a time, >= 100 ms gap, 500 ms timeout, 1 retry on timeout/CRC (none on exception), exception 02 -> register unsupported until reboot, DEGRADED on 1-4 and OFFLINE after 5 consecutive failures, then only reg 000 every 30 s; `mb poll off` stops all automatic traffic.
5. Transceiver held in receive (DE GPIO low, 10 kOhm pull-down) whenever esp-modbus is not running.
6. Enforcement: `tools/check_bus_access.py` (+ `--self-test`) on every build, `tools/check_elf_symbols.py` post-build.
