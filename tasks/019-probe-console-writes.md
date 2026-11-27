# 019 Console-driven writes in a phase-1 write variant (OS6 control test)

Status: 019a done (committed 2d8ec44 incl. A6; amendments in 0075304); 019b done (committed f65f465; fixes B1-B4, firmware-reviewer GO, build-verifier all pass, host tests 201/0/0). Acceptance pending: bench B.W run with the write image, then the on-site E run. It implements the architect's console-write design of 2026-11-27 (W1-W14) with the user decisions of 2026-11-27, relayed by the orchestrator. W4 and W8 were changed by the user. wiki-keeper records the decisions in `concepts/firmware-architecture` section 6 (OS6).

**See "Amendments 2026-11-27 (019a review)" at the end. It overrides the text above where they differ, and it adds a 019a fix-up (A6) that must land before the 019a commit.**

Gate exception W2: this task is specified and built **before** the probe log and before the bus-topology answers. The bench runs with A/B open. The first real FC06 frame goes out on site in checklist section E. It is gated at run time (fresh OK reads, health ONLINE) and by checklist E.0. Depends on 006, 010, 011, 012, 013, 014. Bus-touching, so after 019b: `build-verifier`, then a **full independent `firmware-reviewer`** review of 019a + 019b together (S3).

**Split (decided now):** two `firmware-dev` passes, two commits.
- **019a** is pure and host-testable: FC06 codec, `write_guard`, poll_sched kinds, txn_log, fake slave, lint and ELF script logic. It lands first. `build-verifier` checks it.
- **019b** is target glue, builds, the P4 race backport and the docs.

Model **sonnet** for both. Why: safety logic with many interacting rules (019a), and dual-build CMake plus boot order (019b). This is judgement work, not mechanical.

## Goal
- **One image for the trip (W4).** The phase-1 project gets a second build variant, the **write image**: `CONFIG_DMT_BUS_WRITE=y`, overlay `sdkconfig.defaults.write`, build dir `build_write/`. It is the phase-1 probe unchanged plus `mb write ...` console commands for setpoint, standby, fan mode and season, plus restore.
- The write image is flashed on the bench after B.W + B.16 pass. It is used on site for C/D (kept disarmed) and E. There is no flashing on site.
- **Disarmed, the write code is inert:** no path to an FC06 frame exists without `mb write arm`.
- The **read image** (default build) stays: no write code is compiled, the ELF check runs in read mode, and its behaviour is unchanged.
- The Matter project stays write-free until phase 3 (W1). Its `FATAL_ERROR` on `CONFIG_DMT_BUS_WRITE` stays (W3).
- The console-init race fix is backported to `firmware/main/main.c` (P4, W11). 018 Part B is dropped.

## Files allowed to change
**019a**
- New:
  - `firmware/components/mb_rtu/src/mb_rtu_write.c`, `include/mb_rtu_write.h`, `test_host/test_mb_rtu_write.c`;
  - `firmware/components/write_guard/` (`CMakeLists.txt`, `include/write_guard.h`, `src/write_guard.c`, `src/wg_format.c`, `test_host/test_write_guard.c`);
  - `firmware/test/host/main/test_e2e_write.c`.
- Edited:
  - `firmware/components/mb_rtu/CMakeLists.txt`;
  - `firmware/components/poll_sched/{include/poll_sched.h, include/txn_log.h, src/poll_sched.c, src/txn_log.c, test_host/test_poll_sched.c, test_host/test_txn_log.c}`;
  - `firmware/test/host/main/{fake_slave.c, fake_slave.h, CMakeLists.txt}`;
  - `firmware/tools/check_bus_access.py`, `firmware/tools/check_elf_symbols.py`.

**019b**
- New:
  - `firmware/components/mb_link/src/mb_link_write.c`, `include/mb_link_write.h`;
  - `firmware/components/bus_task/src/bus_write.c`, `src/bus_write.h`, `src/mb_console_write.c`;
  - `firmware/sdkconfig.defaults.write`.
- Edited:
  - `firmware/components/mb_link/CMakeLists.txt`;
  - `firmware/components/bus_task/{CMakeLists.txt, include/bus_task.h, src/bus_task.c, src/mb_console.c}`;
  - `firmware/main/main.c`, `firmware/main/CMakeLists.txt`;
  - `firmware/main/Kconfig.projbuild` (only the help text of `DMT_BUS_WRITE`);
  - `firmware/CMakeLists.txt`;
  - `docs/phase1-probe/checklist.md`, `docs/phase1-probe/measurement-log-template.md`. Decision: **`firmware-dev` applies the docs edits.** They quote console strings that this task creates, so the same pass keeps them in sync. The reviewer checks them (I13).
- `build*/` is already in `firmware/.gitignore`.

**Not allowed:**
- `components/rs485_port/**` (FC06 request and echo are 8 bytes, so `rs485_port_transact` fits unchanged), `daikin_regs`, `reg_model`, `log_out`, `mb_link.c`, `mb_link.h`.
- `firmware/matter/**`, `tasks/018-*`.
- Both `dependencies.lock`, `managed_components`.
- `Knowledgebase/**`.

## Wiki pages relied on
- `concepts/firmware-architecture`:
  - s1 phase 3 and OS6;
  - s2 components (`write_guard`);
  - s3 timing D16, health D21, "Writes (P3 only)" D9/D18/D22, gate/lint D10/D20;
  - s4 DE;
  - s6 OS1-OS6, 006 T4, the console race;
  - s8.
- `concepts/register-map`: 231 range, 233 values 0/3/5, 202/203/245/246, "limit values must be respected", "write 231".
- `concepts/flag-registers`: 201 bits 0-2 000/001/010/011, bit 4 LOCK, bit 7 Stby, bits 3/5/6 undocumented, 8-15 reserved "do not change".
- `concepts/web-restriction-flags`: 247 flags 1-8, persistence.
- `concepts/modbus-rs485-link`: FC 03/06 documented, port not named.
- `concepts/bus-topology`: items 1, 1b, 2 open.
- Specs 006, 010, 011, 012, 013, 014.

## Write rules (normative; host-tested in `write_guard` unless marked)
- **WR1. Write set (W5, D9).** Only these four, and nothing else:

  | Command | Register | Effect |
  |---|---|---|
  | `mb write sp <5.0-40.0>` | 231 | raw = tenths |
  | `mb write standby on\|off` | 201 | bit 7 via RMW |
  | `mb write fan auto\|silent\|night\|max` | 201 | bits 0-2 = 000/001/010/011 via RMW |
  | `mb write season 0\|3\|5` | 233 | 0 auto, 3 winter, 5 summer |

  Never written: 247, 200, 202, 203, 245, 246, any other register, and 201 bits 3-6 and 8-15 (W9).
- **WR2. Arming (W6).**
  - `mb write arm [30-600]`, default 120 s.
  - Arming is refused unless all of these hold:
    - health is ONLINE;
    - the write latch is clear;
    - 201, 231, 233, 202, 203 and 247 are *fresh OK* (avail RM_OK, `reg_model_fresh`, not `out_of_range`);
    - if 247 bit 5 is set, 245 and 246 are fresh OK too;
    - 247 flags (bit n = flag n, *inferred*) 1, 2, 3, 6, 7 and 8 are all 0 (W9).
  - Re-arm while armed: deadline = max(old deadline, now + s). It never shortens. **The budget is not reset.**
  - `mb write disarm` ends the session.
- **WR3. Restore set (W6, W13).**
  - Captured at the first successful arm per boot and per address (`mb addr` clears it): raw 201, 233 and 231 from the model.
  - `mb write restore` (armed only) writes back, in the order **201 -> 233 -> 231**, each register whose current fresh value differs. For 201 only the bits 0-2 and 7 are restored. Each sub-step is a full write command (WR5, WR6).
- **WR4. Budget (W8).**
  - At most **40 FC06 frames per arm session**. A frame counts when it is sent, whatever the result.
  - A session starts with `mb write arm` while disarmed and ends at any disarm.
  - A new arm after a disarm gives a fresh 40. No per-boot cap.
  - When the 40th frame's command has finished: disarm, reason `budget`.
- **WR5. Command sequence (D10, W7).** A command runs in this order, one transaction per `write_guard_step`:
  1. Check armed, not busy, pacing, latch, the 247 flags and the needed limits fresh OK.
  2. **PRE**: FC03 qty 1 of the target register.
  3. Compute the new value. For 201: `(pre & ~0x0087) | (target & 0x0087)`, and refuse if `pre` bits 0-2 are 100-111 (undefined). If new == pre: `SKIP`, no FC06.
  4. Re-check armed, budget and health. If disarmed: `ABORTED`, no FC06.
  5. **WRITE**: one FC06.
  6. **VERIFY**: FC03 qty 1. It always runs after a sent FC06, even after a WRITE failure or a disarm.
  7. Success = WRITE echo OK **and** VERIFY OK **and** the value matches (201: masked 0x0087).

  **No automatic resend of FC06** (W7). PRE and VERIFY are not retried either.
- **WR6. Pacing (W8, D16).**
  - Every transaction respects the poll_sched gap (>= 100 ms end-to-start).
  - The next write command (or restore sub-step) starts its PRE >= **5000 ms** after the last transaction of the previous accepted command, SKIP included.
  - Earlier commands are **refused, not queued** (`pacing <ms>`). One command at a time (`busy`).
  - Write transactions go before due polls. Polls continue between commands.
- **WR7. Setpoint clamp (D9, s3).**
  - Allowed = [202, 203] ∩ [245, 246] (if 247 bit 5) ∩ [50, 400] raw.
  - If 247 bit 4 is set, 50 and 400 are refused too ("5/40 °C ends disabled", *inferred* reading).
  - If any needed limit is not fresh OK: refused. Out of range: refused with the allowed range. The value is never clamped silently.
- **WR8. Failures and latch (W7).**
  - Failure = a PRE that is not OK, a WRITE that is not echo-OK, a VERIFY that is not OK, a mismatch, or a VERIFY lost because the link closed.
  - Every failure disarms (`failure`) and counts toward a **per-boot** counter. At 3: writes are latched until reboot. `mb addr`, `mb silence` and re-arm do not clear it.
  - Refusals and ABORTED are not failures.
- **WR9. Disarm triggers (not failures):**
  - timeout;
  - `mb write disarm`;
  - health leaves ONLINE;
  - `mb poll off`, `mb silence`, `mb addr`;
  - the bus latch (`LATCHED`) or a port fault (link closed);
  - a poll that shows 247 flag 1/2/3/6/7/8 set (`web_flags`).
- **WR10. Inertness (W4).** While disarmed, `write_guard_step` never calls the write function and starts no PRE/WRITE/VERIFY transaction. It only delegates to `poll_step`.
  - The only `mb_link_write` call site (`bus_write.c`) is reached only from a WRITE op.
  - Right before the call, `write_guard_may_transmit()` (armed, budget > 0, not latched, ONLINE) is checked again.
- **WR11. Log (W10).** All W and A lines are printed whatever the `mb log` setting.
  - `T` lines: kinds `PRE|WRITE|VERIFY`. A WRITE OK line carries the written value as `hhhh`.
  - Format (no trailing newline, `t` = ms, `hhhh` = 4-digit lowercase hex):
    ```
    A <t> ARMED <s> s budget <n>
    A <t> EXTENDED <s> s left budget <n>
    A <t> RESTORE_SET 201=<hhhh> 233=<hhhh> 231=<hhhh>
    A <t> REFUSED arm <latched|health|silence|stale <reg>|web_flag <n>>
    A <t> DISARMED <user|timeout|failure|budget|health|poll_off|silence|addr|latched|link|web_flags>
    A <t> LATCHED failures 3
    W <t> CMD <sp|standby|fan|season|restore> <arg>
    W <t> REFUSED <cmd> <disarmed|latched|busy|pacing <ms>|stale <reg>|web_flag <n>|limit <lo>-<hi>|undefined_mode|budget>
    W <t> SKIP <reg> unchanged <hhhh>
    W <t> ABORTED <reg> <reason>
    W <t> DONE <reg> <old hhhh> -> <new hhhh> budget <n>
    W <t> FAIL <reg> <pre_|write_|verify_><RES>|mismatch <hhhh>|verify_lost failures <k>/3
    ```
  - `<arg>` is the raw value for `sp`, the keyword otherwise. Syntax errors print the usage via `printf` (console), no W line.
- **WR12. Bus-safety, unchanged.**
  - FC03 polling, gap, timeout, retries, health, silence check, latch, address range and read whitelist stay exactly as phase 1 (D6, D16, D17, D19, D21).
  - The node is the sole master (D1). "+ A B -" and CP are never touched.
  - Never broadcast: `mb_link_write` refuses addr outside 1..247.

## 019a design (pure)
1. **`mb_rtu_write` (FC06 codec).** The function code is fixed inside it: `MB_RTU_FC_WRITE_SINGLE` 0x06 and `_EXC` 0x86 are defined only in `mb_rtu_write.c`.
   - `int mb_rtu_build_fc06(uint8_t addr, uint16_t reg, uint16_t value, uint8_t out[8])` returns 8, or -1 for addr outside 1..247 or NULL.
   - `size_t mb_rtu_fc06_reply_len(const uint8_t *rx, size_t n)`: 8 for echo, 5 for exception, `MB_RTU_LEN_UNKNOWN` otherwise.
   - `mb_rtu_res_t mb_rtu_parse_fc06_reply(const uint8_t req[8], const uint8_t *rx, size_t len, uint8_t *exc_code)`:
     - len 0 -> TIMEOUT;
     - exact 8-byte echo with valid CRC -> OK *(external: Modbus FC06 echoes the request)*;
     - `addr, 0x86, code, crc` with code != 0 -> EXCEPTION;
     - anything else, including code 0, -> BAD_FRAME.
   - CRC through `mb_rtu_crc16`.
   - CMake: the source is compiled only if `CONFIG_DMT_BUS_WRITE` or `IDF_TARGET STREQUAL "linux"`.
2. **`write_guard`** (pure C99, REQUIRES poll_sched, reg_model, daikin_regs; same compile condition). All public symbols are prefixed `write_guard_`. Sketch; `firmware-dev` may refine names but not the semantics:
   ```c
   typedef ps_res_t (*wg_write_fn)(void *ctx, uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc);
   typedef struct { ps_env_t ps; wg_write_fn write; void *write_ctx; } wg_env_t;
   void write_guard_init(write_guard_t *w);
   void write_guard_command(write_guard_t *w, const poll_sched_t *s, const reg_model_t *m,
                            const wg_cmd_t *c, int64_t now_ms, wg_events_t *ev);       /* accept or refuse */
   bool write_guard_step(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env,
                         ps_txn_t *txn, wg_events_t *ev, int64_t *wait_until_ms);     /* <= 1 txn; else poll_step */
   void write_guard_disarm(write_guard_t *w, wg_reason_t r, int64_t now_ms, wg_events_t *ev); /* hooks; ADDR also clears the restore set */
   bool write_guard_may_transmit(const write_guard_t *w, const poll_sched_t *s, int64_t now_ms);
   void write_guard_get_status(const write_guard_t *w, int64_t now_ms, wg_status_t *out);
   size_t write_guard_format_event(char *buf, size_t n, const wg_event_t *e);         /* WR11 lines */
   ```
   - `write_guard_step` handles housekeeping (timeout, health, 247 flags) every call. `wait_until_ms` = min(poll wait, deadline, next ready write op).
   - PRE and VERIFY OK update `reg_model` (`set_ok`). Failures do not touch the model.
   - Constants (W6, W7, W8): 120 / 30 / 600 s, 40 frames, 5000 ms, 3 failures. No Kconfig.
3. **`poll_sched`.**
   - `ps_kind_t` gains `PS_KIND_PRE`, `PS_KIND_WRITE`, `PS_KIND_VERIFY`.
   - `poll_sched_complete` for these kinds: stats (txns and results, new counters `pre`, `write`, `verify`) and `last_end_ms`. No health, schedule, retry or unsupported effect (like MANUAL, rule 10).
   - New `int64_t poll_sched_ready_ms(const poll_sched_t *s)` returns `last_end + gap`, or `INT64_MIN` before the first transaction.
   - `txn_log` formats and parses the three kinds. W and A lines parse as "ignored" (0).
4. **Fake slave.**
   - `fake_write` (`wg_write_fn`) shares the clock, journal and the 100 ms gap rule with `fake_link`, and is journalled as kind WRITE.
   - Script per register: `OK` (store + echo), `SILENT_LANDED` (store, TIMEOUT), `SILENT_LOST` (TIMEOUT, no store), `EXC <code>`, `BAD_ECHO`, `CLAMP <v>` (stores v, echoes the request).
   - It fails the test on FC06 to a register outside {201, 231, 233} or to an address other than its own.
   - Helper `fake_write_count()`, plus a journal check that FC06 frames are >= 5000 ms apart.
5. **Lint** (`check_bus_access.py`, self-test cases for each rule):
   - R7: `mb_link_write*` is allowed in `mb_link_write.c/.h` **and** `components/bus_task/src/bus_write.c` only.
   - R8: the write FC names and literals 0x06/0x86 are allowed only as the two defines in `mb_rtu_write.c`. 0x86 joins the banned literals. 0x05/0x0F/0x10/0x15-0x17 stay banned everywhere.
   - New R9: `mb_rtu_build_fc06|mb_rtu_parse_fc06|mb_rtu_fc06_` only in `mb_rtu_write.c/.h` and `mb_link_write.c`.
   - Report the new self-test count.
6. **ELF check.** `check_elf_symbols.py --mode read|write` (default `read`, so the current CMake call still works in 019a).

   | Mode | Forbidden prefixes | Required (defined) |
   |---|---|---|
   | `read` | `mbc_`, `mbm_`, `mbs_`, `mb_link_write`, `mb_rtu_build_fc06`, `mb_rtu_parse_fc06`, `mb_rtu_fc06_`, `write_guard_` | as `--require` |
   | `write` | `mbc_`, `mbm_`, `mbs_` | `--require` + `mb_link_write`, `mb_rtu_build_fc06`, `write_guard_step` |

   Self-tests cover both modes.

## 019b design (target)
1. **Build variants.**
   - `sdkconfig.defaults.write` holds `CONFIG_DMT_BUS_WRITE=y`.
   - `main/CMakeLists.txt`: the `FATAL_ERROR` is removed.
   - `firmware/CMakeLists.txt` after `project()` adds two guards:
     - `CONFIG_DMT_BUS_WRITE` set while the binary dir is not named `build_write` -> `FATAL_ERROR`;
     - not set while it is named `build_write` -> `FATAL_ERROR`.
   - The ELF POST_BUILD passes `--mode write` or `--mode read`, chosen from `CONFIG_DMT_BUS_WRITE`.
   - `mb_link` and `bus_task` CMake: write sources only `if(CONFIG_DMT_BUS_WRITE)`. REQUIRES `write_guard` unconditionally; that component is header-only in the read build. If IDF v6.0.2 rejects this, say how it was solved.
   - Write build command:
     ```
     idf.py -B build_write -D SDKCONFIG=build_write/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.write" build
     ```
2. **`mb_link_write.c/.h`.** `mb_link_res_t mb_link_write(uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc_code)`.
   - Refused (MBL_REFUSED) **before any port call**:
     - addr outside 1..247;
     - `daikin_regs_write_mask(reg) == 0`;
     - 231 outside 50..400;
     - 233 not in {0, 3, 5}.
   - Not initialised -> MBL_ERROR.
   - Otherwise one `rs485_port_transact` (8 TX bytes, `mb_rtu_fc06_reply_len`, same timeout and t3.5), then `mb_rtu_parse_fc06_reply`.
   - Port fault handling is identical to `mb_link_read`. No retries.
3. **`bus_write.c`** (bus_task context only).
   - Owns the `write_guard_t`. Adapters: `wg_write_fn` -> `mb_link_write`, with the `may_transmit` re-check right before the call.
   - Handles the new queued commands. The `bt_cmd_type_t` values are added under `#if CONFIG_DMT_BUS_WRITE`.
   - Formats events through `log_out`.
   - Hooks: `command_poll_off`, `command_silence`, `command_addr`, `latch()` and `track_link_faults` call `write_guard_disarm`.
   - `bus_task.c` changes are limited to those hooks, to `write_guard_step` replacing `poll_step` in the write build, to the header suffix ` writes=console` (write build only), and to a `wg_status_t` in `bus_snapshot_t` (write build only).
4. **Console.**
   - `mb_console_write.c` registers the `mb write` subcommands of WR1/WR2 plus `restore` and `disarm`. Parsing:
     - `sp` takes at most one decimal digit;
     - arm seconds 30..600;
     - anything else prints the usage.
   - `mb status` in the write build replaces `write: compiled out` with two lines:
     - `write: <disarmed|armed <s> s left>, budget <n>/40, failures <k>/3[, latched]`;
     - `restore set: 201=hhhh 233=hhhh 231=hhhh` (or `restore set: none`).
   - `mb stats` in the write build appends ` pre <n> write <n> verify <n>` to the `kinds:` line.
   - The read build output is unchanged.
5. **`main.c` (P4).** New order:
   1. hold, as the first statement;
   2. the console-UART check;
   3. `uart` log WARN;
   4. `create_repl` + help + `mb_console_register` (+ the write subcommands in the write build);
   5. `bus_task_start`;
   6. `esp_console_start_repl`.

   Any failure before step 5: log, return, `bus_task` is not started.
6. **Docs.** Apply the appendix text to `checklist.md` and `measurement-log-template.md`. Adjust only the quoted console strings, and only to match the implementation exactly.

## Acceptance checks
019a (host only):
1. `firmware/test/host` passes with 0 failures. Report the count (was 139). Required units:
   - **`mb_rtu_write`:**
     - builder: addr 0/248 rejected, CRC checked against one independently computed vector (name the tool);
     - `reply_len` for echo, exception and an unknown byte;
     - parse: OK, EXC, code 0 -> BADFRAME, wrong addr/reg/value/CRC, short frame, len 0 -> TIMEOUT.
   - **`write_guard` with the fake slave (`test_e2e_write.c`).** The 100 ms gap rule is active and the FC06 >= 5 s check runs at the end of every scenario:
     - (a) **inertness:** each command type while disarmed during 10 simulated minutes of polling -> refused, `fake_write_count() == 0`, no PRE/WRITE/VERIFY in the journal;
     - (b) arm preconditions, table-driven: health STOPPED/DEGRADED/OFFLINE/LATCHED, each needed register stale/unsupported, each of the 247 flags 1/2/3/6/7/8, latch -> refused; flags 0/4/5 -> allowed; 245/246 needed only with bit 5;
     - (c) setpoint happy path: PRE/WRITE/VERIFY order, value, model fresh, DONE line;
     - (d) clamp: boundaries of 202/203, bit 5 intersection, bit 4 ends, stale limit;
     - (e) 201 RMW: bits 3-6 and 8-15 and LOCK preserved; standby on/off; the four fan codes; undefined mode bits refused;
     - (f) 233 values 0/3/5, others refused;
     - (g) unchanged value -> SKIP, no FC06, budget unchanged;
     - (h) failures, each counting 1, disarming and never resending: SILENT_LANDED, SILENT_LOST, EXC, BAD_ECHO, CLAMP mismatch, VERIFY timeout, PRE timeout;
     - (i) latch at the 3rd failure across sessions and after `mb addr`;
     - (j) budget: 40 frames, then disarm; re-arm while armed keeps the budget and extends the deadline (never shortens); disarm + arm gives a fresh 40;
     - (k) pacing: 4999 ms refused, 5000 ms accepted;
     - (l) timeout at 120 s; a command in flight at the deadline finishes VERIFY;
     - (m) each WR9 trigger disarms; a disarm between PRE and WRITE -> ABORTED and no FC06; a lost VERIFY counts as a failure;
     - (n) restore: set captured at the first arm only, recaptured after `mb addr`; order 201 -> 233 -> 231; only differing registers; 201 restores bits 0-2/7 only; sub-steps >= 5 s apart;
     - (o) WR11 strings exact for every event; txn_log round trip of PRE/WRITE/VERIFY lines;
     - (p) poll_sched: the new kinds change no health, schedule or unsupported state; `ready_ms`.
2. **Mutation proofs** (revert before hand-back, report the outputs). Each of these must make at least one test fail:
   - the armed check removed from the step;
   - 5000 -> 4999;
   - 40 -> 41;
   - mask 0x0087 -> 0xFFFF;
   - the VERIFY-after-failure removed.
3. **Lint and ELF self-tests** pass. Report the counts. Deliberate cases are caught:
   - `mb_link_write(` in `mb_console.c` (R7);
   - `0x06` in `mb_link_write.c` (R8);
   - `0x10` anywhere (R8);
   - `mb_rtu_build_fc06` in `bus_task.c` (R9);
   - `write_guard_x` and `mb_rtu_build_fc06` defined in read mode;
   - `mb_link_write` missing in write mode.
4. `idf.py build` (read image) passes, unchanged in behaviour (019a adds no target source to the read build).

019b:

5. **Read build.** `idf.py build` passes with 0 own warnings, lint and ELF `--mode read`. Report the image size. `nm` shows no `mb_link_write`, `mb_rtu_build_fc06` or `write_guard_` symbol.
6. **Write build** (command above) passes with 0 own warnings, lint and ELF `--mode write`. Report the image size.
7. **Failure proofs** (use a scratch copy or revert, report the outputs):
   - (a) read build with a referenced `mb_link_write_dummy()` -> POST_BUILD fails;
   - (b) `check_elf_symbols.py --mode write` on the read ELF fails, and `--mode read` on the write ELF fails;
   - (c) `CONFIG_DMT_BUS_WRITE=y` in `build/` -> CMake FATAL; `=n` in `build_write/` -> FATAL;
   - (d) `CONFIG_DMT_BUS_WRITE=y` in `firmware/matter` still fails CMake.
8. **Grep checks:**
   - `mb_link_write(` is called only in `bus_write.c`;
   - `mb_link_write.h` is included only by `mb_link_write.c` and `bus_write.c`;
   - `mb_rtu_write.h` only by `mb_rtu_write.c`, `mb_link_write.c` and tests;
   - `write_guard.h` only by `bus_write.c`, `bus_write.h` and tests;
   - every `#if CONFIG_DMT_BUS_WRITE` in `bus_task.c` / `mb_console.c` is a hook listed in 019b design 3/4;
   - `git diff` is empty for `rs485_port`, `daikin_regs`, `reg_model`, `mb_link.c/.h` and `firmware/matter/`.
9. Host tests still pass, and the count is unchanged from 019a.
10. No `dependencies.lock` change, no `managed_components` edit.

## Code-inspection checklist (`firmware-reviewer`)
- I1. `main.c`: the hold is still the first statement, the order is that of 019b design 5, and every failure path before `bus_task_start` returns.
- I2. WR10: there is exactly one `mb_link_write` call site, reachable only from a WRITE op, with the `may_transmit` re-check directly before it. Disarmed or latched, no path leads to it.
- I3. `mb_link_write` refuses addr, register and value before any `rs485_port_*` call. Port fault path = `mb_link_read`. No retry.
- I4. FC06 parse: exact 8-byte echo compare, 0x86 exception, code 0 -> BADFRAME. Function-code bytes appear only in `mb_rtu_write.c`.
- I5. Pacing: every write transaction passes through `poll_sched_ready_ms` / `poll_sched_complete` (gap). 5 s between commands. Budget counted at send. No resend anywhere (WR5, WR6).
- I6. 201 RMW: PRE directly before WRITE (no other transaction between them), mask 0x0087, bit 4 and bits 8-15 preserved, undefined mode refused. The residual race with a local keypress is accepted (s3).
- I7. WR8/WR9: every trigger disarms, the latch is per boot, and nothing clears it but a reboot.
- I8. Never written: 247, 200, 202/203, 245/246, 201 bits 3-6/8-15 (WR1). The `daikin_regs_write_mask` whitelist is used in both `write_guard` and `mb_link_write`.
- I9. Read build: no write source is compiled (CMake conditions), `write: compiled out`, the header is unchanged, ELF read mode passes.
- I10. Threading: writes execute only in `bus_task`, the console only queues, `bus_task` takes no new lock. The snapshot copy holds the mutex for the copy only.
- I11. Stack: no large locals on the write path. On the bench, B.W.7 >= 1024 B.
- I12. Lint/ELF modes and the build-dir guard come from Kconfig, not hard-coded.
- I13. The docs strings match the implementation, and the unverified assumptions (below) are stated in the checklist.

## Unverified assumptions about the port and the slave (state them in the report and in checklist E)
- The panel accepts FC06 on "A A B B -". The manual lists 03/06 but does not name the port (`modbus-rs485-link`).
- The FC06 reply is an exact echo *(external, Modbus spec)*.
- The FC06 reply time fits the 500 ms timeout.
- The panel applies and displays a write at once.
- Write endurance and EEPROM storage are not stated (D22 note).
- Whether the panel enforces 202/203/245/246 is unknown.
- 247 flag n = bit n *(inferred)*. Bit 4 = 5.0/40.0 refused *(inferred)*.
- **No bench slave exists:** the first FC06 frame on real hardware is sent on site. Host tests with the fake slave are the only pre-trip evidence (optional B.W.9).

## Test strategy
- Pure logic is host-tested: the codec, `write_guard`, the poll_sched kinds, txn_log, the scripts (019a, acceptance 1-3).
- `mb_link_write`, `bus_write`, the console and `main.c` are IDF-bound. They are covered by inspection I1-I13, both builds, the ELF proofs, bench B.W and site E.
- `mb_link` stays untested on the host (D12).

## Out of scope
- Matter writes, Matter-side `write_guard` use, D22 debounce and hourly/daily caps (phase 3).
- Fan EP2, the 018 revision.
- Changes to `rs485_port`, timing, address or the read whitelist.
- Automatic restore on disarm or timeout.
- Persisting anything (NVS).
- A firmware-enforced E.0 (error-rate gate).
- Writing 200, 247 or the limit registers.
- Address scan, scheduling, MAX3485 docs.

## Escalate (stop, report)
- `rs485_port` needs a change.
- Conditional sources or REQUIRES do not work on IDF v6.0.2.
- The ELF/lint mode cannot be read from Kconfig.
- Any host scenario shows a gap < 100 ms, two FC06 frames < 5 s apart, or an FC06 while disarmed.
- The read build's behaviour would change beyond the stats counters.

## Appendix: docs text (019b applies it; strings adjusted to the implementation)
**checklist.md:**
- Header "Image" line: the trip image is the **write image** (`fw=<B.W fw>`, header suffix `writes=console`). The read image remains the default build.
- A.6 hard rule: "phase-1 probe image (read or write variant)".
- Console section: add the `mb write` commands, the W/A line formats and the T kinds PRE/WRITE/VERIFY.
- B.4.3: the `write:` line is `write: compiled out` (read image) or `write: disarmed, budget 40/40, failures 0/3` (write image).

New **B.W Write image** (bench, A/B open, no slave; after B.16):
1. [ ] `build-verifier` passed both builds, lint and ELF (read and write mode) (`BW.verifier`). Flash with `idf.py -B build_write -p /dev/ttyACM0 flash`. `mb log on`: the header ends with `writes=console` (`BW.fw`).
2. [ ] **[TRANSMITS]** Repeat B.16 with this image (all `T` lines TIMEOUT, no BADFRAME, activity 0, `gap_bytes` 0; latch rule as B.16) (`BW.b16`).
3. [ ] `mb status`: `write: disarmed, budget 40/40, failures 0/3`, `restore set: none` (`BW.status`).
4. [ ] Disarmed refusals: `mb write sp 21.0`, `mb write standby on`, `mb write fan max`, `mb write season 3` and `mb write restore` each print `W <t> REFUSED <cmd> disarmed`. No `T ... PRE|WRITE|VERIFY` line (`BW.refused`).
5. [ ] `mb write arm` (health OFFLINE without a slave) prints `A <t> REFUSED arm health`. The same after `mb poll off` (STOPPED). Syntax: `mb write arm 29`, `mb write arm 601`, `mb write sp 4.9`, `mb write sp 40.1`, `mb write sp 21.55` and `mb write season 4` each print the usage (`BW.arm`).
6. [ ] `mb stats` `kinds:` line ends `pre 0 write 0 verify 0`. DE low on the multimeter (`BW.de_v`).
7. [ ] Stack high-water >= 1024 B after the steps above (`BW.stack`). Less: STOP, back to the architect.
8. [ ] The write image stays on the board. **No flashing until after D.11.**
9. (optional) Only with a USB-RS485 adapter and a PC Modbus slave simulator (not in the BOM): run E.2-E.7 against it. Else write "not done" (`BW.slave`).

New **E. Control test** (after D.10, before D.11; same write image, no re-flash):
- **E.0 Gate.** All of the following must hold. If any fails: skip E and record why (`E0.*`).
  1. D.1-D.10 done without a STOP. `mb poll on` after D.10. Health ONLINE.
  2. Two `mb stats` >= 10 min apart with polling running: (Δtimeout + Δbad_frame + Δerror) / Δtxns < 1 % and Δbad_frame = 0.
  3. `mb regs`: 201, 231, 233, 202, 203 and 247 `ok` and `fresh`. 247 flags 1, 2, 3, 6, 7, 8 = 0. If flag 5 is set: 245/246 `ok fresh`.
  4. The panel is normal. You stand at it and know how to set setpoint, fan mode, season and standby back by hand.
  5. Write down the panel state and the raw values of 201/231/233/008 (`E0.initial`).
- **E.1** No flashing. E starts from the running image and the running boot. Do not reboot during E: a reboot clears the arm state and the restore set.
- **E.2** **[TRANSMITS FC06 from here]** `mb write arm`. Expect `A <t> ARMED 120 s budget 40`, then `A <t> RESTORE_SET ...` equal to E0.initial. `A ... REFUSED arm ...`: record it; retry once after 1 min, else skip E. Re-arm (`mb write arm`) whenever `mb status` shows < 60 s left. Re-arming extends the time and does not reset the budget (40 frames per session; E needs about 13).
- **E.3** `mb write sp <initial+0.5>`. Expect `T` PRE/WRITE/VERIFY 231 and `W ... DONE 231 ...`. Record the panel display and 008 at the next poll. After >= 5 s: `mb write sp <initial>`.
- **E.4** `mb write standby on`: panel and fancoil go to standby (watch up to 60 s). Then `mb write standby off`.
- **E.5** `mb write fan silent`, `night`, `max`, `auto`, >= 5 s apart (if `pacing` is printed, wait and retype). Record the display and the fan sound for each. End at the initial mode.
- **E.6** (last) `mb write season <the other of 3/5>`: observe for at most 60 s, then `mb write season <initial>`. 0 is written only to restore an initial 0.
- **E.7** `mb write restore` (it writes only the differing registers, 201 -> 233 -> 231, >= 5 s apart). Then `mb write disarm` -> `A <t> DISARMED user`. `mb regs` and the panel must equal E0.initial (`E7.*`).
- **E.8** (optional) Change the setpoint by one step at the panel. The next poll shows it in 231/008 (bus is truth). Set it back by hand.
- **Failures:**
  - `W ... FAIL ...` disarms the node: record it and check the panel. Re-arm **once**, only for `mb write restore`. A second FAIL: stop E and restore by hand.
  - `A ... LATCHED failures 3`: stop E, restore by hand, no reboot to retry.
  - `A ... DISARMED health|link|web_flags`: stop E and record it. Restore once health is ONLINE again.
  - Panel anomaly, error code or unexpected fancoil behaviour: `mb write disarm`, `mb poll off`, restore by hand, A.5.
- E is not a phase-1 exit item. Its results feed phase 3.

**measurement-log-template.md:**
- Bench rows: `BW.verifier`, `BW.fw`, `BW.b16`, `BW.status`, `BW.refused`, `BW.arm`, `BW.de_v` (V), `BW.stack` (bytes), `BW.slave`.
- On-site rows: `E0.stats1`, `E0.stats2`, `E0.rate` (%), `E0.badframe`, `E0.regs`, `E0.web` (247 flags), `E0.initial` (panel + raw 201/231/233/008), `E2.arm` (A lines), `E7.final` (raw + panel), `E.fail` (any FAIL/LATCHED lines).
- A "Control actions (E)" table: wall-clock time | command | W/A result line | panel display | fancoil behaviour.

## Roadmap to production (for the user; no specs yet)
1. Trip: run the write image (019) for C/D/E. The probe log and capture go to `raw/`, then `/capture`.
2. One combined task set "2b+3" revises 018. One Matter BUS image contains:
   - the bus stack;
   - the real mapping from the log (2b-B);
   - Fan EP2;
   - **Matter writes reusing `write_guard` from 019**, plus the D22 debounce/caps and a Matter -> bus queue;
   - the timing re-tune from the log.
3. 2b-C, the DE MCPWM check (Q-HW2), runs inside the same bench pass with a diagnostic build, not as a separate trip.
4. Deployment hardware: the MAX3485 module, power and enclosure (BOM). After that, the production image is flashed once and the probe images retire.
5. This is about 2-3 implementation passes and one more on-device flash after the trip.

## Amendments 2026-11-27 (019a review)
These are the architect's rulings on the deviations `firmware-dev` reported for 019a. Where they differ from the text above, they win.

- **A1. Accessors.** `poll_sched_addr()` and `txn_log_res_name()` are accepted. They are read-only, pure, and needed so the code does not duplicate state or strings.
- **A2. `write_guard_may_transmit(w, s)`** has no `now_ms`. Accepted. The deadline is enforced by the housekeeping in the same `write_guard_step` call, and nothing blocks between that and the call. The signature in 019a design 2 is changed to match.
- **A3. Invalid arguments.** `write_guard_command` returns false with no event for season not in {0, 3, 5}, standby > 1 or fan > 3. This is accepted as defence in depth. The console stays the syntax gate. **019b:** the console rejects `sp` outside 5.0..40.0, more than one decimal, arm seconds outside 30..600 and `season` not in {0, 3, 5} with the usage text, and it never queues them (B.W.5 relies on this). A syntactically valid setpoint outside the live limits gives `W ... REFUSED sp limit <lo>-<hi>`.
- **A4. Unreachable reasons are removed from WR11.**
  - `REFUSED arm silence` is gone: a silence check leaves health STOPPED, so the reason is `health`.
  - `REFUSED <cmd> budget` is gone: the 40th frame's command disarms, so a later command gets `disarmed`.
  - 019b does not add them back. The `budget_left > 0` term in `may_transmit` stays as defence in depth.
- **A5. Log format** is accepted as implemented, and the docs (019b) quote it exactly:
  - registers as `%03u`;
  - `limit <lo>-<hi>` in raw tenths, like `CMD sp <raw>`;
  - `W <t> CMD restore` with no argument;
  - `ABORTED <reg> <reason>`;
  - `FAIL <reg> mismatch <hhhh>`;
  - one FAIL line per command with the precedence write_ > verify_ > mismatch. The VERIFY value is still in the `T ... VERIFY` line.
- **A6. Lost VERIFY. The reason split is confirmed. The mechanism must change: 019a fix-up, before the 019a commit.**
  - Confirmed: no VERIFY, counted as `verify_lost` (a failure), when the disarm reason is `link`, `latched`, `poll_off`, `silence` or `addr`.
    - After `mb poll off`, `mb silence` or the latch, the operator and D19/D21 expect no more TX.
    - After a link fault, the port is closed.
    - After `mb addr`, a VERIFY would read a different slave.
    - The outcome is unknown, so it counts as a failure.
  - Confirmed: the VERIFY still runs (WR5.6) after `user`, `timeout`, `budget`, `web_flags` and `health` (DEGRADED/OFFLINE). Polling and the offline probes keep sending FC03 in these states. The VERIFY is one FC03 at the normal gap, with no retry. It is the only evidence of what a sent FC06 did. No FC06 can go out in these states (`may_transmit`).
  - **Change (bus safety must not depend on hook order).** Right now TX is suppressed only when a hook passes a bus-gone reason. `housekeeping` maps STOPPED to `health` and returns early once disarmed. So "user disarm, then `mb poll off`" without the hook would send the VERIFY while STOPPED. Required:
    1. `write_guard_step` starts no PRE, WRITE or VERIFY while `poll_sched_health` is STOPPED or LATCHED, whether armed or not. A pending VERIFY then ends as `verify_lost`, and a pending PRE/WRITE as ABORTED.
    2. While armed, `housekeeping` maps STOPPED to the disarm reason `poll_off` and LATCHED to `latched`.
    3. PRE captures the slave address. WRITE and VERIFY use that captured address. If `poll_sched_addr()` differs by then: no TX, ABORTED before the WRITE, `verify_lost` at the VERIFY.
  - New host scenarios in `test_e2e_write.c`, with no hook called and a VERIFY pending (armed, and after a user disarm):
    - `poll_sched_stop` -> no TX, `verify_lost`;
    - `poll_sched_latch` -> no TX, `verify_lost`;
    - `poll_sched_set_addr` -> no frame to either address, `verify_lost`;
    - DEGRADED -> the VERIFY runs.
  - New mutation proof: removing check 1 makes a test fail.
  - Files: `write_guard/src/write_guard.c`, `write_guard/include/write_guard.h` (only if the state struct needs the address) and `test_e2e_write.c`. `build-verifier` re-runs 019a. Report the new test count.
  - **019b hooks** call `write_guard_disarm` *before* the action takes effect, and also when already disarmed (a VERIFY may be pending):
    - `command_poll_off` before `poll_sched_stop`;
    - `command_silence` before `poll_sched_stop` / `mb_link_deinit`;
    - `command_addr` before `poll_sched_set_addr`;
    - `latch()` and `track_link_faults` on entry.
  - **Docs (019b):** state in the checklist that `mb write disarm` stops writes but not reads, and that `mb poll off` stops all TX. A command in flight then counts `verify_lost`.
- **A7. Restore** is accepted as implemented:
  - a fresh register equal to the stored value is skipped silently (no PRE, no pacing);
  - a non-fresh register goes through PRE (SKIP if equal);
  - if nothing differs, only the `CMD restore` line is printed;
  - a refused sub-step prints `REFUSED restore <reason>` and ends the restore.
  - E.7 confirms the result with `mb regs`. The docs say this.
- **A8. Flag-4 upper clamp.** Accepted: the low end is tested only. 203 cannot exceed 350 raw without being `out_of_range` (not fresh OK). The clamp code stays as defence in depth.
- **A9. Lint scope of the function-code literals.** Accepted as implemented: 0x05/0x06/0x0F/0x10/0x15-0x17/0x86 are banned in `components/mb_rtu/`, `components/mb_link/` (including `mb_link_write.c`) and `components/rs485_port/`. Only the two defines in `mb_rtu_write.c` are allowed. "Anywhere" in WR/acceptance 3 is replaced by this scope. Reason: 0x10 is a common non-FC literal elsewhere, and frames can only be built in these components (the R1-R9 call-site rules).
- **A10. `firmware/README.md`** is added to the 019b files allowed to change. Only these parts may change: the build variants and write build command, lint R7-R9, the ELF `--mode read|write`, and the write-image console commands. Each part must match the implementation.

## Amendments 2026-11-27 (019b review)
Rulings after `firmware-reviewer` (GO) and `build-verifier` (all pass except 7(d)). Where they differ from the text above, they win.

- **B1. W3 and acceptance 7(d) are replaced.** `firmware/matter` never had a `CONFIG_DMT_BUS_WRITE` guard. The symbol does not exist in that project, and the project compiles no bus or write source. So a Kconfig FATAL there would prove nothing. The Matter-side guard is the existing ELF check:
  - `firmware/matter/tools/check_no_bus.py` `FORBIDDEN_PREFIXES` gains `"write_guard_"` and `"bus_write_"`. `mb_link_write*` and `mb_rtu_*_fc06*` are already covered by `mb_link_` and `mb_rtu_`.
  - Goal line W3 now reads: "The Matter project stays write-free until phase 3 (W1). Its POST_BUILD `check_no_bus.py` rejects any write symbol (W3)."
  - New 7(d): `check_no_bus.py --nm` on the phase-1 **write** ELF fails and lists `write_guard_` and `bus_write_` symbols. On the Matter ELF it passes.
  - Exception to "Not allowed `firmware/matter/**`" and to acceptance 8: only the `FORBIDDEN_PREFIXES` tuple in `check_no_bus.py` may change.
- **B2. Fixed now:**
  - checklist E.0.3 and E.3 (the setpoint limits);
  - checklist line 7;
  - the `gap_bytes` record after a `write_TIMEOUT`;
  - the ABORTED reason;
  - the indentation in `write_guard.h`;
  - the stale comment in `sdkconfig.defaults` (comment only; this file is added to the allowed files for that purpose);
  - a note in E about lines lost when the log ring drops.
- **B3. Left as is:**
  - `command_addr` disarms before the address is validated. This is unreachable, because the console limits the address to 1..247. Disarming early is the safe direction.
  - `strtoul` accepts `+3`/`03` for season. The value is still gated to {0, 3, 5} (A3).
  - Build commands (deviation 5). The two-step `set-target` + `build` form in the README is verified, so it stays. `flash` without `-D SDKCONFIG` reuses the build dir cache, and the CMake guard stops a wrong build.
  - `log_out` ring drop: accepted, with the docs note from B2.
  - B.W.7 (stack high-water >= 1024 B with the write image) stays a bench condition. It must pass before the trip.
- **B4. Review rule (S3).** Text-only fixes need no re-review. The code nits (ABORTED reason, indentation, prefixes, comment) are checked by `build-verifier` only.
