# Task specs

Written by `architect` from architecture **v0.3**, approved by the user on 2026-11-08 (Q1-Q10 answered with the recommendations; wiki: `concepts/firmware-architecture`). Spec-level choices answered by the user on 2026-11-09: automatic polling start after a clean boot silence check, normal USB power on site, all other listed choices accepted as written; follow-up answers the same day: commands typed during the boot silence check run before the first poll, polling stays off after an on-demand `mb silence` until `mb poll on`, `mb log` default on (006, 008). **Amended 2026-11-24, user decision E2** on the 002 S3 escalation: no esp-modbus at all, own minimal FC03-only RTU master (D7 and Q9 reversed); 002 split into 012/013/014. Implement one task per `firmware-dev` pass; after each: `build-verifier`, then `firmware-reviewer`. Bus-touching tasks (marked below) get an **independent `firmware-reviewer` review** of the spec result before the next task starts.

## Gate
- Released: **phase 1 (on-site read probe)** only, as a gate exception (Q8).
- Phase 2 (Matter read-only mirror) and phase 3 (Matter writes behind `CONFIG_DMT_BUS_WRITE`) specs are written **only after the filled phase-1 probe log** (task 008, exit items 1-11) is in `Knowledgebase/raw/` and captured into the wiki.

## Order (phase 1)
| # | Task | Depends on | Model | Bus-touching | Status |
|---|---|---|---|---|---|
| 001 | [Skeleton, Kconfig, bus-access lint, host-test runner](001-skeleton-kconfig-lint.md) | - | sonnet | yes (lint) | done (implemented and accepted 2026-11-09: firmware-reviewer accept after two rounds, build-verifier all checks pass); lint R1/R3/R4/R8 redefined by 013 |
| 009 | [`daikin_regs` register table, scaling, whitelists](009-daikin-regs.md) | 001 | sonnet | yes (read whitelist) | done (accepted 2026-11-24, committed) |
| 010 | [`reg_model` + `poll_sched`, health, `txn_log`](010-reg-model-poll-sched.md) | 009 | sonnet | yes (pacing) | done (accepted 2026-11-24 after amendment U1-U6: build-verifier PASS, firmware-reviewer APPROVE) |
| 012 | [`mb_rtu` pure FC03 frame codec (CRC, builder, classifier)](012-mb-rtu-frame-codec.md) | 001 | sonnet | yes (frame builder, classifier) | done (accepted 2026-11-24: build-verifier PASS, firmware-reviewer APPROVE WITH MINORS, minors applied) |
| 013 | [Lint update "no esp-modbus", ELF symbol check](013-lint-elf-check-no-esp-modbus.md) | 001, 012 | sonnet | yes (lint, ELF) | done (accepted 2026-11-24 as is: build-verifier PASS except host re-run not run, firmware-reviewer APPROVE WITH MINORS) (amended 2026-11-24, user decision: status line) |
| 013 follow-up (lint) | [013 open follow-up: ROM/register UART TX bans, open minors](013-lint-elf-check-no-esp-modbus.md#open-follow-up-done-committed-9ca4517-amended-2026-11-24-user-decision) | 013 | sonnet | yes (lint) | done (accepted 2026-11-24 as is: build-verifier PASS, firmware-reviewer APPROVE WITH MINORS; committed 9ca4517; doc minors open) (amended 2026-11-24, user decision) |
| 014 | [`rs485_port` + `mb_link` read path over the UART](014-rs485-port-mb-link.md) | 009, 012, 013, 013 follow-up | sonnet | yes (first TX code) | done; firmware-reviewer APPROVE WITH MINORS; done (accepted 2026-11-24: build-verifier PASS incl. M2 guard proof, firmware-reviewer APPROVE; committed 987fbd1), remaining items moved to 006 and 008 (amended 2026-11-24, user decision: 014 review) |
| 006 | [`bus_task` poll loop, probe console, `app_main`](006-bus-task-console.md) | 014, 010 | sonnet | yes | done (accepted 2026-11-24: build-verifier PASS, firmware-reviewer APPROVE; committed b8ad00d) (amended 2026-11-24, user decision: 006 accepted) |
| 011 | [Fake slave, probe-log fixtures, end-to-end host tests](011-fake-slave-host-tests.md) | 010 | sonnet | yes (evidence) | spec ready (unchanged) |
| 008 | [On-site probe checklist + measurement log (document)](008-p1-bringup-hil-checklist.md) | 006, 014 | sonnet | yes (procedure) | spec ready (amended 2026-11-24) |

Next (amended 2026-11-24, user decision: 006 accepted): 011 and 008. 008 needs the user's answers to Q-HW1 and Q-HW2 (wiki `concepts/firmware-architecture` section 8) before it is finalised.

Order and why (amended 2026-11-24): 012 first (pure, no lint impact); 013 next, because the current lint bans `uart_write_bytes` everywhere and 014 cannot build before it, and its tree scan should see the real `mb_rtu.c`; 014 after both; 006 last among the firmware tasks. The ELF check's required-symbol list starts empty in 013 and is filled by 006, because `--gc-sections` drops `mb_link_read` until `app_main` reaches it. 011 may run any time (needs only 010). 008 may be drafted early and is finalised after 006 and 014.

Escalations that stop the pipeline (user decision): 014 DE/RTS deviations (RTS not asserted on the TX path, DE high outside our transmit, mode-before-pin order impossible), no per-byte RX timing (end-of-frame rule), any need for line inversion or a lint relaxation; 008 boot silence activity on the bench or on site, `BADFRAME` without a slave on the bench (B.4).

## Decisions 2026-11-24 on 012/013/014 (amended 2026-11-24, user decision)
- Gap bytes before a request (014 rule 2): counted and discarded, the request is still sent (default kept).
- `mb_link_get_counters` (`gap_bytes`, `faults`, `max_byte_interval_us`) shown in `mb status`: approved.
- No host test of `mb_link` with a fake `rs485_port` (D12 unchanged); refusal order is a code-inspection item in 014.
- Exception code 0x00 -> `BAD_FRAME` (012): approved.
- No `CONFIG_FREERTOS_HZ` change and no bench simulator task; the 10-20 ms end-of-frame wait at 100 Hz is a documented known limit (014 rule 4).
- E2 stays the implementation; E1b (esp-modbus `==2.1.4`) is a documented fallback only, nothing of it is built: see 014 "Fallback E1b". A revert is a separate user decision.

## Seam rule (normative; amended 2026-11-24, user decision)
`mb_link.h` and its result semantics are the single seam of the link layer. Nothing above `mb_link` (`bus_task`, `main`, later Matter code) may include `mb_rtu.h` or `rs485_port.h` or depend on their types, constants or behaviour; `mb_link.h` declares its own config and counter types. Checked by grep in 014 (3i) and 006 (acceptance 2). This keeps an E1b revert confined to `components/mb_link/src/` plus the lint/ELF/build-config items listed in 014.

## Retired (files kept as short notes so numbering and history stay clear)
- 002 esp-modbus master spike / `mb_link` (2026-11-24, user decision E2 on escalation S3) -> replaced by 012 (`mb_rtu`), 013 (lint + ELF check), 014 (`rs485_port` + `mb_link`); spike evidence in `docs/spikes/002-esp-modbus.md`; the esp-modbus route survives only as the documented fallback E1b in 014 (amended 2026-11-24, user decision)
- 003 `rtu_sniff` (v0.3, 2026-11-08) -> was replaced by 002, now by 012/014
- 004 `bus_monitor` (v0.3) -> replaced by 010
- 005 `rs485_rx` (v0.3) -> was replaced by 002 (`rs485_port`), now by 014
- 007 replay fixtures (v0.3) -> replaced by 011

## Bus-safety rules, restated (architecture section "3. Bus-safety rules", amended 2026-11-24, user decision E2)
1. Node = sole Modbus master on EKWHCTRL1 "A A B B -"; "+ A B -" and CP are never touched.
2. FC03 only in phase 1: the function code is fixed inside `mb_rtu_build_fc03` and function-code bytes exist only in `mb_rtu` (012); addresses 1..247, never broadcast, checked in `mb_link_read` before any port access; registers from the `daikin_regs` read whitelist (qty 1 automatic, 1-4 manual); no write source exists (`CONFIG_DMT_BUS_WRITE` default n, build fails if set).
3. Boot silence check (5 s) and on-demand `mb silence` (default 60 s): any activity -> latched refusal to transmit until reboot. After a clean boot check polling starts automatically (first frame about 5 s after power-up; user decision 2026-11-09); `mb poll off` / `mb poll on` stop and resume it.
4. Pacing: one transaction at a time, >= 100 ms gap, 500 ms timeout, 1 retry on timeout/CRC (none on exception), exception 02 -> register unsupported until reboot, DEGRADED on 1-4 and OFFLINE after 5 consecutive failures, then only reg 000 every 30 s; `mb poll off` stops all automatic traffic. The link layer has no retries of its own; a port fault closes the link until `mb silence` or reboot (014).
5. Transceiver held in receive: DE GPIO a plain output at 0 whenever the UART driver is not installed (level written before the output is enabled; never `gpio_reset_pin`); while installed DE = RTS, high only while our 8-byte request is sent (`uart_set_mode` before `uart_set_pin`); 10 kOhm pull-down covers reset/boot (D8).
6. Enforcement: `tools/check_bus_access.py` (+ `--self-test`) on every build: no esp-modbus identifiers, headers or manifests anywhere; `uart_write_bytes` only in `components/rs485_port/src/`; FC bytes only in `mb_rtu`. `tools/check_elf_symbols.py` (+ `--self-test`) post-build: no `mbc_` / `mbm_` / `mbs_` / `mb_link_write` symbols; `mb_link_read` and `mb_rtu_build_fc03` required from 006 on.
