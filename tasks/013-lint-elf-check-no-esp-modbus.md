# 013 - Bus-access lint update ("no esp-modbus anywhere", own RTU master) and ELF symbol check

Phase: 1. Status: done (accepted 2026-11-24 as is: build-verifier PASS except host re-run not run, firmware-reviewer APPROVE WITH MINORS) (amended 2026-11-24, user decision: status line; the review findings A1-A6 and UART rules B are in "Amendment 2026-11-24" below; the open follow-up is at the end of this file). Originally: spec new 2026-11-24, user decision E2 on the 002 S3 escalation; replaces part (c) of retired 002 and the ELF check of 002. Depends on: 001, 012 (the tree scan must accept the real `mb_rtu.c`).
Review: **bus-guarding** (the lint and the ELF check are bus-safety controls). `firmware-reviewer` reviews this task independently before 014 starts.

**Ownership grant (user decision 2026-11-24):** this task may change the task-001-owned files `firmware/tools/check_bus_access.py` and `firmware/CMakeLists.txt` (hook lines only).

## Goal
1. Update `tools/check_bus_access.py` to the E2 design: no esp-modbus anywhere (R1, R3), exactly `uart_write_bytes` (not `_with_break`) allowed only in `components/rs485_port/src/` (R4), function-code bytes only in `mb_rtu` and only 0x03 (R8); extend `--self-test` (incl. the open 001 minor: explicit cases for the compound operators).
2. New `tools/check_elf_symbols.py` (+ `--self-test`), run post-build on the esp32s3 image (D20).
3. Make sure esp-modbus is gone from the local build (manifest, lock file, managed component directory).

## Files / directories owned
- `firmware/tools/check_bus_access.py` (modify; granted)
- `firmware/tools/check_elf_symbols.py` (new)
- `firmware/CMakeLists.txt` (granted: add the ELF hook and the `DMT_ELF_REQUIRED_SYMBOLS` variable; keep the lint hook unchanged)
- `firmware/README.md` (lint/ELF section only: rule summary, ELF check usage, documented limits)

## Wiki pages relied on
- `concepts/firmware-architecture`, section "3. Bus-safety rules" ("Compile-time gate and bus-access lint (D10, D20)"; the R1-R8 summary there is updated by `wiki-keeper` from this spec), "6. Decisions" (D7 reversed 2026-11-24, D20)
- `concepts/dev-environment` (registry-only rule, IDF v6.0.2, toolchain)
- `tasks/001-skeleton-kconfig-lint.md` (scan roots, suffixes, skip rules, splicing, output format, self-test structure: all kept unless changed below)

## Lint rules after this task (paths relative to `firmware/`)
Unchanged from 001: scan roots and skip rules, suffixes, comments/strings not exempt, continuation splicing, output `path:line: R<n>: message`, exit 1, and rules **R2, R6, R7** (R5 extended below, amended 2026-11-24, user decision; R2's include check uses the widened include detection of A1, amended 2026-11-24, user decision). New scan input: files named exactly `idf_component.yml` under `main/` and `components/` (only R3 applies to them; walker test extended). Changed rules (amended 2026-11-24, user decision):

| Rule | Check |
|---|---|
| R1 | No esp-modbus master/controller API anywhere: `\bmbc_(?!slave_\|tcp_)\w*` or `\bmbm_\w*` fails in every scanned file (slave/TCP names stay R2, so each name reports exactly one rule). The allowed-call list is removed. |
| R3 | No esp-modbus header anywhere: an include directive (detection widened by A1: comments around `#`/directive/header, `%:include`, `#import`, `#include_next`; amended 2026-11-24, user decision) whose basename starts with `esp_modbus_` (except `esp_modbus_slave.h`, which stays R2) or is `mbcontroller.h` fails. In `idf_component.yml`: any line matching `esp[-_]modbus` (case-insensitive) fails (amended 2026-11-24, user decision; was: containing `esp-modbus`). |
| R4 | `\b(uart_write_bytes\w*\|uart_tx_chars\|uart_set_rts\|uart_set_dtr\|uart_set_loop_back\|uart_set_line_inverse\|_uart_set_pin\w*)\b` fails everywhere, **except** the exact name `uart_write_bytes` in files under `components/rs485_port/src/` (not its `include/`; `uart_write_bytes_with_break` fails there too). **Also (amended 2026-11-24, user decision B):** `\b(uart_ll_\w*\|uart_hal_\w*\|esp_rom_uart_tx_one_char\|uart_tx_one_char)\b`, raw register access `\bUART[0-2]\s*\.`, and an include directive (A1 detection) whose basename is `uart_struct.h`, `uart_ll.h` or `uart_hal.h` fail **everywhere, including `components/rs485_port/src/`** (architect's choice: 014 plans none of them, the driver API is enough; a need is a 014 escalation "lint relaxation"). Basename match (not the full `soc/`/`hal/` path) is deliberate: stricter, same as R2/R3. |
| R5 | (amended 2026-11-24, user decision B) Unchanged from 001 (setup names only in `components/rs485_port/`; `uart_set_mode` mode argument check everywhere), with the setup list extended by `uart_set_hw_flow_ctrl`, `uart_set_baudrate`, `uart_set_parity`, `uart_set_word_length`, `uart_set_stop_bits`, `uart_set_tx_idle_num`, `uart_enable_tx_intr`, `uart_flush` (whole word: `uart_flush_input` does not match and stays allowed), `uart_intr_config`, `uart_isr_register`. The new names use the same path rule as the existing ones (`components/rs485_port/`, as implemented by 001 and 013 pass 1); see open point P1 in the amendment section. |
| R8 | Function-code bytes only in `mb_rtu`, only 0x03 in phase 1: (a) `\bMB_FUNC_\w*` (esp-modbus constants) fails everywhere. (b) `\bMB_RTU_FC_\w*` only in `components/mb_rtu/src/`; there only the names `MB_RTU_FC_READ_HOLDING` and `MB_RTU_FC_READ_HOLDING_EXC`, and each `#define` of them must have the value literally `0x03` resp. `0x83` (hex digits any case, optional `u`/`U` suffix). (c) In `components/mb_rtu/`, `components/mb_link/`, `components/rs485_port/`: a hex integer literal whose value is 0x03 or 0x83 fails unless it is the value of one of the two (b) defines in `components/mb_rtu/src/`; a hex literal whose value is a write function code (0x05, 0x06, 0x0F, 0x10, 0x15, 0x16, 0x17; leading zeros and `uUlL` suffixes allowed) fails. (d) In the same three components: any compound assignment (`\|= &= ^= += -= *= /= %= <<= >>=`) to an identifier or field named `command`, `fc`, `func`, `func_code` or `function_code` (whole identifier, case-insensitive) fails; in `components/mb_link/` and `components/rs485_port/` a simple assignment (`=`, not `==`) to such a name also fails. Comparisons (`== != >= <=`) never match. **Also (amended 2026-11-24, user decision A3):** in all three components `++`/`--` on such a name fails, postfix (`fc++`, `r.command --`, `(fc)++`) and prefix (`++fc`, `-- r.command`, `++p->func_code`, `++(fc)`, `++a[i].fc`), with optional blanks/comments between operator and operand. The closing-paren limit `{0,8}` of pass 1 is lifted (unbounded), because the gap pattern is now deterministic (A2; architect's choice, amended 2026-11-24, user decision). No `WRITE_FILE` exemption in phase 1 (phase 3 redefines where FC06 lives). |

Remove the now-dead constants (`R1_ALLOWED_CALLS`, `READ_COMMANDS`, `MB_LINK_SRC` if unused) and update the module docstring: rule summary, "the real guard is that FC bytes exist only in `mb_rtu` plus the ELF check (013)", and the documented limits: `#include MACRO`, `##` token pasting, decimal function-code literals, function codes computed at run time or passed through variables, aliases/pointers to an FC field.

### Self-test (`--self-test`)
Keep every existing case whose expectation is still valid; change the ones the rule change flips (list them in the report), and add at least:
- R1: `mbc_master_send_request(h, &r, d);` in `components/mb_link/src/mb_link.c` -> {R1} (was allowed); `mbc_master_set_descriptor(h, d, 1);` -> {R1}; `mbm_rq_read_holding_reg(i, 1, 0, 1, 0);` in `main/main.c` -> {R1}; `mb_link_read(1, 0, 1, v, &e);` and `mb_rtu_build_fc03(1, 0, 1, b);` in `main/main.c` -> pass; `mbc_slave_start(h);` -> {R2} only.
- R3: `#include "esp_modbus_master.h"` in `components/mb_link/src/mb_link.c` -> {R3} (was allowed); `#include <esp_modbus_common.h>` -> {R3}; `#include "mbcontroller.h"` -> {R3}; `#include "esp_modbus_slave.h"` -> {R2} only; virtual `components/mb_link/idf_component.yml` with `  espressif/esp-modbus:\n    version: "==2.1.4"` -> {R3}; a yml with only `idf: ">=6.0"` -> pass.
- R4: `uart_write_bytes(p, b, 8);` in `components/rs485_port/src/rs485_port.c` -> pass; same in `components/rs485_port/include/rs485_port.h` -> {R4}; in `components/mb_link/src/mb_link.c` -> {R4}; in `components/mb_rtu/src/mb_rtu.c` -> {R4}; `uart_write_bytes_with_break(p, b, 8, 2);` in `components/rs485_port/src/rs485_port.c` -> {R4}; `uart_tx_chars(p, b, 1);` there -> {R4} (existing).
- R8 (a)/(b): `x = MB_FUNC_READ_HOLDING_REGISTER;` in `components/mb_link/src/mb_link.c` -> {R8}; `x = MB_FUNC_WRITE_SINGLE_REGISTER;` in `main/main.c` -> {R8} (was allowed in `main/`); `#define MB_RTU_FC_READ_HOLDING 0x03u` and `#define MB_RTU_FC_READ_HOLDING_EXC 0x83u` in `components/mb_rtu/src/mb_rtu.c` -> pass; `#define MB_RTU_FC_READ_HOLDING 0x06u` there -> {R8}; `#define MB_RTU_FC_WRITE_SINGLE 0x06u` there -> {R8}; `#define MB_RTU_FC_READ_HOLDING 0x03u` in `components/mb_rtu/include/mb_rtu.h` -> {R8}; `out[1] = MB_RTU_FC_READ_HOLDING;` in `components/mb_link/src/mb_link.c` -> {R8}.
- R8 (c): in `components/mb_link/src/mb_link.c`: `b = 0x06;`, `b = 0X0f;`, `b = 0x0010u;`, `b = 0x03;`, `b = 0x83;` -> {R8} each; in `components/rs485_port/src/rs485_port.c`: `n = 0x17;` -> {R8}; in `components/mb_rtu/src/mb_rtu.c`: `crc ^= 0xA001u; c = 0xFFFFu; m = 0x80u;` -> pass, `if (rx[1] == 0x03)` -> {R8}; `x = 0x0300;` in `mb_link.c` -> pass (value 0x300); `x = 0x06;` in `components/daikin_regs/src/daikin_regs.c` -> pass (outside the three components); `/* FC 0x06 */` in `mb_link.c` -> {R8} (comments not exempt).
- R8 (d): in `components/mb_rtu/src/mb_rtu.c` one case per operator `|= &= ^= += -= *= /= %= <<= >>=` on `fc` or `r.command` -> {R8} each (this closes the 001 follow-up for `^= -= *= /= %= >>=`); `uint8_t fc = rx[1];` there -> pass; `fc = 3;` and `r.command = 0x03;` in `components/mb_link/src/mb_link.c` -> {R8}; `if (fc == x || fc >= y || fc != z || fc <= w) {}` in `mb_link.c` -> pass.
- Walker: `components/c/idf_component.yml` is scanned (marker content `espressif/esp-modbus` -> R3), `components/c/other.yml` is not; the existing walker marker `uart_write_bytes` stays valid (no walker path under `components/rs485_port/src/`).

## ELF check (`tools/check_elf_symbols.py`)
Python 3 stdlib only. Usage: `check_elf_symbols.py --nm <nm> [--require SYM[,SYM...]] <elf>` and `check_elf_symbols.py --self-test`.
- Runs `<nm> --defined-only <elf>`; fails (exit 1, one line per finding) if a defined symbol matches `^(mbc_|mbm_|mbs_|mb_link_write)`; fails if any `--require` symbol is not defined.
- Fails (never skips) if `--nm` is missing or not executable, the ELF is missing, or `nm` exits non-zero.
- Prints a one-line summary: forbidden 0 found; `required: <list> present` or `required: none configured`.
- `--self-test`: pure parsing/matching on synthetic `nm` output lines: forbidden defined symbol -> fail; forbidden name only undefined (`U`) -> pass; `mb_link_writer_x` -> fail (prefix); `xmbc_foo` -> pass (anchored); required present / missing; empty required list. Exit 0 only if every case gives its expected result.

Hook in `firmware/CMakeLists.txt` (target builds only; the host project has its own top-level file):
```cmake
# Symbols the image must contain. Empty until 006 links the read path (006 sets: mb_link_read,mb_rtu_build_fc03).
set(DMT_ELF_REQUIRED_SYMBOLS "")
```
plus a `POST_BUILD` command on `${CMAKE_PROJECT_NAME}.elf` that runs `--self-test`, then the check with `--nm ${CMAKE_NM}`, `--require` from the variable (omitted when empty) and the ELF path. Verify that `CMAKE_NM` is the xtensa-esp32s3 `nm` in IDF v6.0.2 and report its value; if `add_custom_command(TARGET ... POST_BUILD)` cannot attach to the IDF elf target from this file, use a custom target that depends on the elf and is a dependency of the flashable binary, and say which. A failing check must fail `idf.py build`.

Why the required list starts empty (sequencing): with `--gc-sections`, `mb_link_read` and `mb_rtu_build_fc03` are only in the image once `app_main` reaches them (006). 006 sets the variable and proves the check is not vacuous.

## esp-modbus removal (local tree)
- `grep -rn "esp-modbus" firmware --include=idf_component.yml` is empty (no manifest in our tree references it).
- After `idf.py reconfigure`: `firmware/dependencies.lock` (local, not committed) has no `espressif/esp-modbus` entry, and `firmware/managed_components/espressif__esp-modbus/` does not exist (delete it by hand if the component manager left it; it is a download, not project code). Report both.

## Acceptance checks
1. `python3 firmware/tools/check_bus_access.py --self-test` exits 0 (report the case count); `python3 firmware/tools/check_bus_access.py` exits 0 on the tree (with `mb_rtu` from 012).
2. `python3 firmware/tools/check_elf_symbols.py --self-test` exits 0.
3. `cd firmware && idf.py build` (esp32s3) passes; the build output shows the lint, the ELF self-test and the ELF summary line (`required: none configured`).
4. Guard proofs, each a reverted temporary edit with the failing output reported: (a) `uart_write_bytes(1, "x", 1);` in `main/main.c` -> build fails with R4; (b) a temporary file `components/rs485_port/include/tmp.h` containing `uart_write_bytes` -> R4; (c) `#define MB_RTU_FC_READ_HOLDING 0x06u` in `mb_rtu.c` -> R8; (d) with the lint dependency removed by hand, `void mbc_dummy(void) {}` **called from `app_main`** (an unreferenced function is dropped by `--gc-sections` and would prove nothing) -> ELF check fails; (e) `set(DMT_ELF_REQUIRED_SYMBOLS "dmt_no_such_symbol")` -> ELF check fails.
5. esp-modbus removal items above reported.
6. `firmware/README.md` lint/ELF section matches the rules above.

## Test strategy
Both scripts are pure Python and unit-tested by their `--self-test`, which runs on every build. The real-tree guard proofs (acceptance 4) show the hooks are wired. No hardware.

## Recommended implementer model
sonnet: regex rules with path exceptions where every bad case must report exactly one rule (R1/R2 and R3/R2 overlaps, R8 hex-value parsing, define exemption), CMake post-build wiring on the IDF elf target and the `--gc-sections` pitfall in the guard proof; the 001 lint needed two review rounds for bypasses.

## Amendment 2026-11-24 (user decision on the 013 review; follow-up pass)
Everything in this section is amended 2026-11-24, user decision. The follow-up pass modifies only the four files already owned (`firmware/tools/check_bus_access.py`, `firmware/tools/check_elf_symbols.py`, `firmware/README.md` lint/ELF section; `firmware/CMakeLists.txt` only if needed, which is not expected). Keep all pass-1 behaviour and self-test cases unless an item below changes them; list any flipped case in the report.

### A - review fixes (approved)
**A1 Include detection (R2, R3, R4 headers).** One shared include regex for all include checks. Directive start `#` or the digraph `%:`; directive name `include`, `include_next` or `import`; between start, name and header any mix of blanks/tabs and block comments (a block comment may contain newlines: it is one space in translation phase 3, so `#/* x\n */include` is a real directive). A bare newline does not continue a directive (continuations are spliced before matching, as before). Sketch (implementer may refine; the cases decide):
```python
_CMT = r"/\*(?:[^*]|\*(?!/))*\*/"           # deterministic block comment (A2)
_DIR_GAP = rf"(?:[ \t]|{_CMT})*"
RE_INCLUDE = re.compile(rf"(?:#|%:){_DIR_GAP}(?:include(?:_next)?|import){_DIR_GAP}[<\"]([^>\"]*)[>\"]")
```
`RE_FC_DEFINE` (R8 b) switches to the same deterministic gap. It keeps accepting only `#` (not `%:`): a `%:define MB_RTU_FC_READ_HOLDING 0x03u` is then not an accepted define and its literal is reported by R8 (c), i.e. it fails closed.

**A2 Deterministic comment gaps.** Replace every lazy comment pattern (`/\*.*?\*/` with DOTALL in `_GAP`, `/\*[\s\S]*?\*/` in `_FC_GAP`) by `_CMT` above, so a block comment can only end at its first `*/` and no input can be split into comments in more than one way (the lazy forms let one match span several comments, which makes the nested `*` exponential on a failing match). No other regex may nest an unbounded quantifier over an alternative that can match the same text in two ways (state in the report that you checked each pattern).

**A3 R8 (d) `++`/`--`.** See the R8 row. Sketch: postfix `{_FC_FIELD}(?:\+\+|--)` (with `_FC_FIELD` = name, then `(?:{_FC_GAP}\))*`, then `{_FC_GAP}`); prefix `(?:\+\+|--){_FC_GAP}(?:\({_FC_GAP})*(?:\w+{_FC_GAP}(?:\[[^\]]*\]{_FC_GAP})*(?:\.|->){_FC_GAP})*(?<!\w)(?:fc|func|func_code|function_code|command)(?!\w)`, case-insensitive. Applies in `components/mb_rtu/`, `components/mb_link/`, `components/rs485_port/` (all three, like the compound operators). Message e.g. `'++' on a function-code field is banned`.

**A4 Manifest R3.** `esp[-_]modbus`, case-insensitive (see R3 row).

**A5 ELF sanity (`check_elf_symbols.py`).** New pure function (e.g. `sanity_problems(nm_output)` -> list of messages) used by `check()` after a successful `nm` run and before the forbidden/required evaluation output: fails (exit 1, one `check_elf_symbols: FAIL: ...` line each) if the output yields **zero parsed defined symbols** (`no defined symbols parsed from nm output`) or if **`app_main` is not among the defined symbols** (`app_main not defined (wrong ELF or nm output format)`). `evaluate()` and its existing cases stay unchanged (they use synthetic outputs without `app_main` on purpose). The ok summary line is unchanged (`forbidden 0 found; required: ...`), so 006 acceptance 1 is unaffected.

**A6 Documented limits** (module docstring of `check_bus_access.py` and README, both): keep the pass-1 list and add:
- R8 (d) paren limit: lifted (A3), so no longer a limit; still not detected: casts or dereference in the operand (`++(*p).fc`, `++((T *)p)->fc`), aliases/pointers to an FC field (existing).
- Arithmetic or bit operations on the FC constants inside `components/mb_rtu/src/` (`MB_RTU_FC_READ_HOLDING + 3`, `MB_RTU_FC_READ_HOLDING | 0x80`) are not detected; `mb_rtu/src` is the trusted place, covered by review and the 012 host tests.
- Character and string escapes (`'\x06'`, `'\006'`, `"\x06"`) are not detected as FC literals.
- Decimal, octal and binary FC literals stay undetected (octal/binary bans not adopted, user decision 2026-11-24).
- `CMakeLists.txt`, `*.cmake` and `Kconfig*` files are not scanned (e.g. a compile definition `-DFC=0x06`).
- "Exactly one `uart_write_bytes` call site" cannot be counted by the lint; it is the 014 inspection item 3e.
- Raw UART register access is detected only as `UART0.`/`UART1.`/`UART2.`; `&UART1`, `UART_LL_GET_HW(1)` or a `uart_dev_t *` are not (the `uart_struct.h`/`uart_ll.h`/`uart_hal.h` include ban does not see transitive includes).
- Trigraph `??=include` is not detected (inferred: GCC ignores trigraphs in its GNU language modes unless `-trigraphs` is given; external, GCC manual).
- Known false positives (comments and strings are not exempt): a line comment ending in `--`/`++` directly before a line starting with an FC name (`// ----` then `fc = ...`) and prose such as `... UART1.` in a comment report R8 resp. R4; reword the comment.

### B - stricter UART rules (approved)
See the R4 and R5 rows. Choice stated there: the low-level names, raw registers and the three headers are banned everywhere, including `components/rs485_port/src/`.

**Open point P1 (for the user; does not block the pass):** the decision text says the new R5 names are allowed "where R5 already allows its setup calls, i.e. rs485_port/src". The implemented R5 allows `components/rs485_port/` (including `include/`). This amendment keeps one R5 path for all names (the existing one). If R5 as a whole should be `components/rs485_port/src/` only, it is a one-constant change with no effect on 014 (`rs485_port.h` contains no UART calls); say so and the implementer applies it in the same pass, plus a case `uart_set_pin(...)` in `components/rs485_port/include/rs485_port.h` -> {R5}.

### Self-test additions (`check_bus_access.py --self-test`)
Each bad case must report exactly the listed rule set; "pass" = no finding.
- A1 includes, in `components/mb_link/src/mb_link.c` unless noted: `#include /*c*/ "esp_modbus_master.h"` -> {R3}; `#/**/include <esp_modbus_master.h>` -> {R3}; `# /* a */ include /* b */ <mbcontroller.h>` -> {R3}; `#/* multi\nline */include "esp_modbus_master.h"` -> {R3}; `%:include "esp_modbus_master.h"` -> {R3}; `%: include <mbcontroller.h>` -> {R3}; `#import "esp_modbus_master.h"` -> {R3}; `#include_next /**/ <esp_modbus_common.h>` -> {R3}; `#import <esp_modbus_slave.h>` -> {R2}; `%:include "esp_modbus_slave.h"` -> {R2}; `#include /*c*/ "mb_link.h"` in `main/main.c` -> pass; `%:define MB_RTU_FC_READ_HOLDING 0x03u` in `components/mb_rtu/src/mb_rtu.c` -> {R8} (fails closed).
- A2 timing guard (each case also timed with `time.monotonic()`; a case taking more than 1.0 s is a failure): `"fc" + " /* a */" * 5000 + " ;"` in `mb_link.c` -> pass; `"fc" + " /* a */" * 5000 + " = y;"` in `mb_link.c` -> {R8}; `"r.command" + " /* a */" * 5000 + " |= 1;"` in `mb_rtu.c` -> {R8}; `"#" + "/**/" * 5000 + "include <esp_modbus_master.h>"` -> {R3}; `"#" + "/**/" * 5000 + "x"` -> pass; `"#define" + " /* x */" * 5000 + " MB_RTU_FC_READ_HOLDING 0x03u"` in `mb_rtu.c` -> pass; `"fc" + " /* a */" * 5000 + " ++;"` in `mb_rtu.c` -> {R8}; `"fc" + ")" * 5000 + " ;"` in `mb_link.c` -> pass. Use one name/directive per case (many names in one case make the scan quadratic by construction, which is not the property under test). Optional evidence: the first case does not finish with the pass-1 `_FC_GAP` (do not commit that).
- A3, generated for each of `mb_rtu.c`, `mb_link.c`, `rs485_port.c`: `fc++;`, `fc--;`, `++fc;`, `--fc;`, `r.command ++;`, `++ r.command;`, `--p->func_code;`, `(fc)++;`, `++(fc);`, `fc /* c */ ++;`, `++ /* c */ fc;`, `++a[i].fc;`, `FUNC++;` -> {R8} each. Pass in `mb_rtu.c`: `i++; n--; ++i; fc_len++; ++fcx; --xfc; ++commands;`; `x = fc + +y; z = fc - -1;`; `if (fc == 3) {}` stays pass. Pass outside the three: `fc++; --command;` in `components/daikin_regs/src/daikin_regs.c` and in `main/main.c`.
- A4: virtual `components/mb_link/idf_component.yml` with `  espressif/esp_modbus: "*"` -> {R3}; `  ESPRESSIF/ESP-MODBUS: "*"` -> {R3}; `# Esp_Modbus` -> {R3}; `  espressif/modbus_other: "*"` -> pass.
- B / R4, each -> {R4}: `uart_ll_write_txfifo(h, b, 8);` in `rs485_port.c`, `mb_link.c` and `main/main.c`; `uart_hal_write_txfifo(&h, b, 8, &n);` in `rs485_port.c`; `esp_rom_uart_tx_one_char('x');` in `rs485_port.c` and `main/main.c`; `uart_tx_one_char('x');` in `main/main.c`; `UART1.fifo.val = 0;` in `rs485_port.c`; `UART0 . conf0.val = 0;` in `main/main.c`; `#include "soc/uart_struct.h"` in `rs485_port.c`; `#include <hal/uart_ll.h>` in `rs485_port.c`; `#include "hal/uart_hal.h"` in `mb_link.c`; `#include /*c*/ "hal/uart_ll.h"` in `main/main.c`; `#include "uart_ll.h"` in `rs485_port.c` (basename). Pass: `int p = UART_NUM_1; int q = UART_PIN_NO_CHANGE; int r = UART10;` in `main/main.c`; `#include "driver/uart.h"` and `#include "hal/uart_types.h"` in `rs485_port.c`.
- B / R5: each new name (`uart_set_hw_flow_ctrl(p, UART_HW_FLOWCTRL_DISABLE, 0);`, `uart_set_baudrate(p, 9600);`, `uart_set_parity(p, UART_PARITY_DISABLE);`, `uart_set_word_length(p, UART_DATA_8_BITS);`, `uart_set_stop_bits(p, UART_STOP_BITS_1);`, `uart_set_tx_idle_num(p, 0);`, `uart_enable_tx_intr(p, 1, 0);`, `uart_flush(p);`, `uart_intr_config(p, &c);`, `uart_isr_register(p, f, a, 0, &h);`) in `main/main.c` -> {R5} and in `mb_link.c` -> {R5} (generated), and in `rs485_port.c` -> pass. `uart_flush_input(p);` in `rs485_port.c` -> pass.
- 014 compatibility (pass, in `components/rs485_port/src/rs485_port.c`; one case for the open/transact path, one for the silence check):
  `uart_config_t cfg = { .baud_rate = 9600, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT }; gpio_set_level(de, 0); gpio_set_direction(de, GPIO_MODE_OUTPUT); uart_param_config(p, &cfg); uart_driver_install(p, 256, 0, 0, NULL, 0); uart_set_mode(p, UART_MODE_RS485_HALF_DUPLEX); uart_set_pin(p, tx, rx, de, UART_PIN_NO_CHANGE); uart_set_rx_full_threshold(p, 1); gpio_pullup_en(rx); uart_get_buffered_data_len(p, &n); uart_flush_input(p); n = uart_write_bytes(p, tx, 8); uart_wait_tx_done(p, t); uart_read_bytes(p, b, 1, t); int64_t now = esp_timer_get_time(); uart_driver_delete(p);`
  and `uart_driver_install(p, 256, 0, 8, &q, 0); uart_set_mode(p, UART_MODE_UART); uart_set_pin(p, UART_PIN_NO_CHANGE, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); if (xQueueReceive(q, &ev, t) && (ev.type == UART_DATA || ev.type == UART_FRAME_ERR || ev.type == UART_PARITY_ERR || ev.type == UART_BREAK || ev.type == UART_FIFO_OVF || ev.type == UART_BUFFER_FULL)) { gap_bytes += ev.size; faults++; } uart_driver_delete(p);` -> pass each.
- ELF (`check_elf_symbols.py --self-test`): pure `sanity_problems` cases: empty output -> 1 problem (no symbols); only undefined lines (`         U app_main`) -> no-symbols problem; unparseable lines only (`foo.o:`, blank) -> no-symbols problem; `400d0000 T other` -> app_main problem; `400d0000 T app_main` -> none. `check()`-level cases with a stub `nm` written to the temp dir (executable `#!/bin/sh` script, `chmod 0o755`; the stub accepts and ignores its arguments, like `/bin/true`): stub printing nothing, exit 0 -> `check()` returns 1; stub printing `400d0000 T other` -> 1; positive control, stub printing `400d0000 T app_main` -> 0 (proves the stub mechanism works, so the two failures are not artefacts). The build host is Linux/WSL (IDF), so `/bin/sh` is available; the self-test must fail, not skip, if it is not.

### Acceptance (amendment)
1. Both `--self-test`s exit 0; report the case counts before and after, the wall time of `time python3 firmware/tools/check_bus_access.py --self-test`, and any pass-1 case whose expectation changed (expected: none).
2. `python3 firmware/tools/check_bus_access.py` exits 0 on the current tree (`mb_rtu` from 012 contains no `++`/`--` on an FC name, no `UART<n>.`, no new R5 name; verify).
3. `cd firmware && idf.py build` (esp32s3) passes; output shows lint, ELF self-test and `required: none configured`.
4. Guard proofs, each a reverted temporary edit with the failing output reported. Use line comments so the C compiler never sees the construct and the failure is the lint's (comments are not exempt): (a) `// %:include "esp_modbus_master.h"` in `main/main.c` -> R3; (b) `// #/**/include "hal/uart_ll.h"` in `main/main.c` -> R4; (c) `// UART1.fifo.val = 0;` in `main/main.c` -> R4; (d) `// uart_flush(1);` in `main/main.c` -> R5; (e) `// fc++;` in `components/mb_rtu/src/mb_rtu.c` -> R8. (f) Manual, no edit: `python3 tools/check_elf_symbols.py --nm /bin/true build/daikin_matter_thermostat.elf` -> exit 1 with the no-symbols message.
5. Module docstrings and the README lint/ELF section (rule table R3, R4, R5, R8 rows; ELF paragraph: sanity checks; limits list per A6) match this section.

### Compatibility with 014 and 006 (checked by the architect against their spec text)
- 014's planned calls in `components/rs485_port/src/` (`uart_param_config`, `uart_driver_install`, `uart_set_mode` with `UART_MODE_RS485_HALF_DUPLEX`/`UART_MODE_UART`, `uart_set_pin`, `uart_set_rx_full_threshold`, `uart_get_buffered_data_len`, `uart_flush_input`, `uart_read_bytes`, `uart_write_bytes`, `uart_wait_tx_done`, `uart_driver_delete`, `gpio_*`, `esp_timer_get_time`, event-queue types for the silence check) hit none of the new rules; the 014-compatibility cases above lock this in. 014 uses no `uart_ll_*`/`uart_hal_*`, no raw registers, no ll/hal/struct header, no FC-named counter.
- 006 (`main.c`, `bus_task`) plans no `uart_*` call and no esp-modbus or FC code; its `DMT_ELF_REQUIRED_SYMBOLS` line and guard proof (`dmt_no_such_symbol`) are unaffected; `app_main` is always defined (main component).
- 014 "Fallback E1b" lint items stay valid; a revert would additionally have to re-allow `esp_modbus` spellings in that one manifest (covered by its "R1/R3 re-allow" item).

### Recommended implementer model (amendment)
sonnet: regex determinism (proving no ambiguous nested quantifiers), many exactly-one-rule cases across overlapping rules (R2/R3/R4 include checks, R8 b/c/d), a timing-guarded self-test and stub-based ELF failure modes; too subtle for haiku.

## Out of scope
- `rs485_port`, `mb_link` code (014); `main.c` behaviour (006), except the reverted temporary edits above.
- Setting `DMT_ELF_REQUIRED_SYMBOLS` (006).
- New restrictions beyond the table (the B extensions of R4/R5 are now in the table, amended 2026-11-24, user decision); propose further ones in the report instead. Not adopted (user decision 2026-11-24): octal/binary FC literal bans.
- Phase-3 write rules.
- The lint/ELF changes a revert to the E1b fallback would need (014 "Fallback E1b"; not implemented, separate user decision; amended 2026-11-24, user decision).

## Open follow-up (decided, not implemented) (amended 2026-11-24, user decision)
Source: re-review of 013 after acceptance. Scope: one small lint pass (`firmware/tools/check_bus_access.py`, its docstring and the README lint section), by `firmware-dev` (sonnet: regex with exactly-one-rule cases and a timing guard), then build-verifier and firmware-reviewer. It runs **before 014 starts** (README row "013 follow-up (lint)").

**Conflict check with 014 (architect, against the 014 text):** no conflict. 014 uses none of the new ban names (no `esp_rom_*`, no `UART[0-2]` access, no `REG_WRITE`/`WRITE_PERI_REG`, no `uart_reg.h`, `uart_struct.h`, `uart_ll.h`, `uart_hal.h`). Its `uart_write_bytes` in `components/rs485_port/src/` and its other driver calls (`uart_wait_tx_done`, `uart_get_buffered_data_len`, `uart_flush_input`, `uart_read_bytes`, `uart_set_rx_full_threshold`) are not touched by the new rules. The open minors below touch no 014 call either.

### Decided: new R4 bans, everywhere (including `components/rs485_port/src/`)
| Item | Check (fails everywhere, same path rule as the rest of R4) |
|---|---|
| ROM UART TX routes | `\besp_rom_(?:output\|uart)_\w*` (R4). Covers `esp_rom_output_tx_one_char` and `esp_rom_uart_putc`. The pattern is the whole family, deliberately wider than the TX names (no ROM UART call is needed by this project). The name `esp_rom_uart_tx_one_char` from 013 B is covered too. |
| Pointer register access | `\bUART[0-2]\s*(?:\.\|->)` (R4 raw-register row extended by `->`; `UART_NUM_1`, `UART10` and `&UART1` stay as documented in A6). |
| FIFO register writes | `\b(?:REG_WRITE\|WRITE_PERI_REG)\s*\([^;]*\bUART_FIFO\w*_REG\b`: a write macro whose argument list, up to the next `;`, names a `UART_FIFO*_REG` macro. Writes only; reads are open question Q-b. |
| Include | basename `uart_reg.h` (`soc/uart_reg.h`) added to the R4 include list, with `uart_struct.h`, `uart_ll.h`, `uart_hal.h`; same A1 include detection. |

Self-test cases (each -> {R4} unless marked pass):
- `esp_rom_output_tx_one_char('x');` in `components/rs485_port/src/rs485_port.c` and in `main/main.c`; `esp_rom_uart_putc('x');` in `main/main.c`; `esp_rom_uart_tx_one_char('x');` in `rs485_port.c`.
- `UART1->fifo.val = 0;` in `rs485_port.c`; `UART0 -> conf0.val = 0;` in `main/main.c`.
- `WRITE_PERI_REG(UART_FIFO_REG(1), 0x41);` in `rs485_port.c`; `REG_WRITE(UART_FIFO_AHB_REG(1), 0x41);` in `main/main.c`.
- `#include "soc/uart_reg.h"` in `rs485_port.c`; `#include <soc/uart_reg.h>` in `main/main.c`.
- Pass: `esp_rom_delay_us(10);` in `main/main.c` (not in the family); `int n = UART_NUM_1;` in `main/main.c`.
- No timing case for the FIFO-write rule: `[^;]*` is quadratic for many unterminated `REG_WRITE(` in one statement (same class as minor 4 below). The implementer may bound the gap instead and must report the choice.
- Guard proof (acceptance 4 style, line comment so the compiler never sees it): `// REG_WRITE(UART_FIFO_REG(1), 0);` in `main/main.c` -> R4.

### Open minors (numbering as in the user's list; architect's recommendation; user to confirm)
- **(1) Limit, document (A6 list, docstring and README):** `++`/`--` with nested brackets, e.g. `++a[b[1]].fc`, is not detected: the bracket gap `\[[^\]]*\]` does not nest. Recommendation: document; no fix in phase 1.
- **(3) Docstring "matching is linear" is wrong.** An unterminated `/*` or repeated `/*` makes the comment-gap scan quadratic in the input length (A2 `_CMT`). Recommendation: correct the docstring to the measured behaviour, no behaviour change. Alternative (user choice): cap the comment-gap length, a rule change (a long comment between `fc` and `=` would then pass). Not chosen by the architect.
- **(4) Pre-existing quadratic R5 `_second_argument` (001 code) on repeated `uart_set_mode(`.** Document it in the docstring and README limits. An optional timing case (`"uart_set_mode(" * 5000` in `rs485_port.c`) is allowed; its expected result must be derived from the current R5 logic and reported, not assumed.
- **(5) Self-test timing guard 1.0 s -> 5 s (A2 cases).** Recommendation: raise to 5 s; it avoids flakes on a loaded WSL host. A quadratic or exponential regression still fails it, since such a case runs far longer than 5 s. The implementer reports the measured wall time of each A2 case so the user can check the margin.

### Open questions (not adopted; the user decides, not part of this pass)
- **Q-a (ROM install route):** `esp_rom_install_uart_printf()` (IDF v6.0.2 `components/esp_rom/include/esp_rom_sys.h:113`) and other `esp_rom_install_*` calls can route ROM output to a UART. They are outside `esp_rom_(output|uart)_\w*` and are not banned by this follow-up. Option: add `\besp_rom_install_\w*` to R4. Architect's recommendation: add it (same bus-safety argument; no project use).
- **Q-b (FIFO reads):** `REG_READ`/`READ_PERI_REG` of a `UART_FIFO*_REG` would pull RX bytes around the driver. Not in the decided list; the architect recommends the same ban for reads.
- **Q-c (other TX entry points):** `esp_rom_printf` and similar ROM print calls write to the console UART, not the bus UART; not in scope unless the user wants a blanket ban on `esp_rom_*` output calls.

### Acceptance (follow-up pass)
1. `python3 firmware/tools/check_bus_access.py --self-test` exits 0; report the case count before and after and the wall time of `time python3 firmware/tools/check_bus_access.py --self-test`; the A2 timing case times.
2. `python3 firmware/tools/check_bus_access.py` exits 0 on the current tree (no hit from the new bans; verify with grep).
3. `cd firmware && idf.py build` (esp32s3) passes; lint and ELF summary lines shown.
4. Guard proof: the line-comment case above -> R4, with the output.
5. README lint section and docstring match the new rows and the open minors as decided by the user.
