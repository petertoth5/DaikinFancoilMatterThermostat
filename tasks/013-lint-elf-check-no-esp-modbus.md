# 013 - Bus-access lint update ("no esp-modbus anywhere", own RTU master) and ELF symbol check

Phase: 1. Status: spec ready (new 2026-11-24, user decision E2 on the 002 S3 escalation; replaces part (c) of retired 002 and the ELF check of 002). Depends on: 001, 012 (the tree scan must accept the real `mb_rtu.c`).
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
Unchanged from 001: scan roots and skip rules, suffixes, comments/strings not exempt, continuation splicing, output `path:line: R<n>: message`, exit 1, and rules **R2, R5, R6, R7**. New scan input: files named exactly `idf_component.yml` under `main/` and `components/` (only R3 applies to them; walker test extended). Changed rules (amended 2026-11-24, user decision):

| Rule | Check |
|---|---|
| R1 | No esp-modbus master/controller API anywhere: `\bmbc_(?!slave_\|tcp_)\w*` or `\bmbm_\w*` fails in every scanned file (slave/TCP names stay R2, so each name reports exactly one rule). The allowed-call list is removed. |
| R3 | No esp-modbus header anywhere: `#include` whose basename starts with `esp_modbus_` (except `esp_modbus_slave.h`, which stays R2) or is `mbcontroller.h` fails. In `idf_component.yml`: any line containing `esp-modbus` fails. |
| R4 | `\b(uart_write_bytes\w*\|uart_tx_chars\|uart_set_rts\|uart_set_dtr\|uart_set_loop_back\|uart_set_line_inverse\|_uart_set_pin\w*)\b` fails everywhere, **except** the exact name `uart_write_bytes` in files under `components/rs485_port/src/` (not its `include/`; `uart_write_bytes_with_break` fails there too). |
| R8 | Function-code bytes only in `mb_rtu`, only 0x03 in phase 1: (a) `\bMB_FUNC_\w*` (esp-modbus constants) fails everywhere. (b) `\bMB_RTU_FC_\w*` only in `components/mb_rtu/src/`; there only the names `MB_RTU_FC_READ_HOLDING` and `MB_RTU_FC_READ_HOLDING_EXC`, and each `#define` of them must have the value literally `0x03` resp. `0x83` (hex digits any case, optional `u`/`U` suffix). (c) In `components/mb_rtu/`, `components/mb_link/`, `components/rs485_port/`: a hex integer literal whose value is 0x03 or 0x83 fails unless it is the value of one of the two (b) defines in `components/mb_rtu/src/`; a hex literal whose value is a write function code (0x05, 0x06, 0x0F, 0x10, 0x15, 0x16, 0x17; leading zeros and `uUlL` suffixes allowed) fails. (d) In the same three components: any compound assignment (`\|= &= ^= += -= *= /= %= <<= >>=`) to an identifier or field named `command`, `fc`, `func`, `func_code` or `function_code` (whole identifier, case-insensitive) fails; in `components/mb_link/` and `components/rs485_port/` a simple assignment (`=`, not `==`) to such a name also fails. Comparisons (`== != >= <=`) never match. No `WRITE_FILE` exemption in phase 1 (phase 3 redefines where FC06 lives). |

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

## Out of scope
- `rs485_port`, `mb_link` code (014); `main.c` behaviour (006), except the reverted temporary edits above.
- Setting `DMT_ELF_REQUIRED_SYMBOLS` (006).
- New restrictions beyond the table (e.g. limiting other `uart_*` calls to `rs485_port`); propose them in the report instead.
- Phase-3 write rules.
- The lint/ELF changes a revert to the E1b fallback would need (014 "Fallback E1b"; not implemented, separate user decision; amended 2026-11-24, user decision).
