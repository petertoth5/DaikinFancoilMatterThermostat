# Firmware

ESP-IDF v6.0.2 only. Activate per shell: `. ~/esp/esp-idf-v6.0.2/export.sh`.

## Build (esp32s3)
    cd firmware
    idf.py set-target esp32s3
    idf.py build

`sdkconfig.defaults` is applied on set-target. Kconfig menu "Daikin bus (phase 1 read probe)":
`idf.py menuconfig`.

### Build variants (spec 019)
| Image | Build dir | Config | Content |
|---|---|---|---|
| read (default) | `build/` | `CONFIG_DMT_BUS_WRITE=n` | phase-1 read probe, no write code compiled |
| write | `build_write/` | `CONFIG_DMT_BUS_WRITE=y` (`sdkconfig.defaults.write`) | read probe plus the `mb write` console commands, inert until armed |

Write image (own sdkconfig in `build_write/`, the read image's `sdkconfig` is not touched):

    cd firmware
    idf.py -B build_write -D SDKCONFIG=build_write/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.write" set-target esp32s3   # first time only
    idf.py -B build_write -D SDKCONFIG=build_write/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.write" build
    idf.py -B build_write -p /dev/ttyACM0 flash

CMake refuses a mismatch: `CONFIG_DMT_BUS_WRITE=y` in a build dir not named `build_write`, and
`CONFIG_DMT_BUS_WRITE=n` in `build_write/`. The ELF check runs in the matching mode (below).

Write-image console commands (`mb write ...`; absent from the read image). They only queue; `bus_task`
decides. Syntax errors print the usage and queue nothing. Disarmed, every command is refused
(`W <t> REFUSED <cmd> disarmed`).

| Command | Effect |
|---|---|
| `mb write arm [30-600]` | arm for the given seconds (default 120); re-arm extends, never resets the 40-frame budget |
| `mb write disarm` | end the session (stops writes, not reads) |
| `mb write sp <5.0-40.0>` | setpoint, register 231, at most one decimal |
| `mb write standby on\|off` | register 201 bit 7 (read-modify-write) |
| `mb write fan auto\|silent\|night\|max` | register 201 bits 0-2 = 0/1/2/3 (read-modify-write) |
| `mb write season 0\|3\|5` | register 233 |
| `mb write restore` | write back the values captured at the first arm (201, 233, 231, only those that differ) |

`mb status` shows `write: ...` and `restore set: ...` (read image: `write: compiled out`); `mb stats`
appends ` pre <n> write <n> verify <n>` to the `kinds:` line; the log header ends ` writes=console`.
`W`/`A` log lines and the `T` kinds PRE/WRITE/VERIFY: `docs/phase1-probe/checklist.md`.

## Bus-access lint and ELF symbol check
`tools/check_bus_access.py` (rules R1-R9) and its `--self-test` run on every `idf.py build` (CMake
target `bus_access_lint`); a violation or failing self-test fails the build. Manual run:
`python3 tools/check_bus_access.py [--self-test]` from `firmware/`. It scans C/C++/assembly sources
and files named `idf_component.yml` (R3 only) under `main/` and `components/`; comments and strings
are not exempt, backslash-newline continuations are spliced.

| Rule | Check |
|---|---|
| R1 | no esp-modbus master/controller API (`mbc_*`, `mbm_*`) anywhere |
| R2 | no esp-modbus slave/TCP API (`mbc_slave_*`, `mbs_*`, `mbc_tcp_*`), no `esp_modbus_slave.h` |
| R3 | no `esp_modbus_*.h` / `mbcontroller.h` include (`#` or `%:`, `include`/`include_next`/`import`, block comments allowed between the tokens); no line matching `esp[-_]modbus` (case-insensitive) in an `idf_component.yml` |
| R4 | `uart_write_bytes*`, `uart_tx_chars`, `uart_set_rts/dtr/loop_back/line_inverse`, `_uart_set_pin*` banned; only the exact `uart_write_bytes` is allowed, in `components/rs485_port/src/`. Banned everywhere, also in `rs485_port/src/`: `uart_ll_*`, `uart_hal_*`, `uart_tx_one_char`, the ROM families `esp_rom_output_*`, `esp_rom_uart_*`, `esp_rom_install_*` (other `esp_rom_*` such as `esp_rom_delay_us` are allowed), raw register access `UART0.`/`UART1.`/`UART2.` and the pointer form `UART0->`, `REG_WRITE`/`WRITE_PERI_REG`/`REG_READ`/`READ_PERI_REG` whose arguments (within 200 characters, before the next `;`) name a `UART_FIFO*_REG`, includes of `uart_struct.h`, `uart_ll.h`, `uart_hal.h`, `uart_reg.h` (by basename) |
| R5 | UART setup calls only in `components/rs485_port/` (`uart_set_pin`, `uart_set_mode`, `uart_driver_install/delete`, `uart_param_config`, `uart_read_bytes`, `uart_set_hw_flow_ctrl`, `uart_set_baudrate`, `uart_set_parity`, `uart_set_word_length`, `uart_set_stop_bits`, `uart_set_tx_idle_num`, `uart_enable_tx_intr`, `uart_flush` (not `uart_flush_input`), `uart_intr_config`, `uart_isr_register`); `uart_set_mode` only with `UART_MODE_RS485_HALF_DUPLEX` or `UART_MODE_UART` |
| R6 | `CONFIG_DMT_BUS_TX_GPIO` / `CONFIG_DMT_BUS_DE_GPIO` only in `components/rs485_port/` and `main/` |
| R7 | `mb_link_write*` only in `mb_link_write.c/.h` and `components/bus_task/src/bus_write.c` |
| R8 | function-code bytes only in `mb_rtu`: `MB_FUNC_*` banned; `MB_RTU_FC_*` only in `components/mb_rtu/src/` (names `MB_RTU_FC_READ_HOLDING` = `0x03` and `MB_RTU_FC_READ_HOLDING_EXC` = `0x83` in `mb_rtu.c`; the write single register code and its exception variant as the two defines in `mb_rtu_write.c`); in `mb_rtu`, `mb_link`, `rs485_port` the hex literals 0x03/0x83 (except the read defines), 0x06/0x86 (except the two write defines) and the other write codes (0x05, 0x0F, 0x10, 0x15, 0x16, 0x17) are banned (also in `mb_link_write.c`), as are compound assignments to `command`/`fc`/`func`/`func_code`/`function_code` (and, in `mb_link`/`rs485_port`, any assignment to them) and `++`/`--` on them, prefix or postfix (all three components) |
| R9 | the FC06 codec names `mb_rtu_build_fc06`, `mb_rtu_parse_fc06`, `mb_rtu_fc06_*` only in `mb_rtu_write.c/.h` and `mb_link_write.c` |

Documented limits (not detected): `#include MACRO`, `##` token pasting, decimal/octal/binary
function-code literals, function codes computed at run time or passed through variables,
aliases/pointers to an FC field, casts or dereference in a `++`/`--` operand (`++(*p).fc`,
`++((T *)p)->fc`), nested brackets in a prefix operand (`++a[b[1]].fc`), arithmetic or bit operations on the FC constants inside `components/mb_rtu/src/`
(`MB_RTU_FC_READ_HOLDING + 3`; trusted place, covered by review and host tests), character/string
escapes (`'\x06'`, `'\006'`, `"\x06"`), `CMakeLists.txt`/`*.cmake`/`Kconfig*` files (e.g.
`-DFC=0x06`), the count "exactly one `uart_write_bytes` call site" (014 inspection item 3e), raw
UART access other than `UART0.`..`UART2.` (`&UART1`, `UART_LL_GET_HW(1)`, a `uart_dev_t *`) and
transitive includes of the four banned UART headers, a `UART_FIFO*_REG` more than 200 characters
after the `(` of the register macro or reached through another macro/variable, trigraph `??=include` (GCC ignores trigraphs
in GNU modes, inferred). Known false positives (comments and strings are not exempt): a line comment
ending in `--`/`++` directly before a line starting with an FC name (`// ----` then `fc = ...`) and
prose such as `... UART1.` in a comment report R8 resp. R4; reword the comment. R8 is a heuristic;
the real guard is that FC bytes exist only in `mb_rtu` plus the ELF check. Matching time: no
exponential backtracking; linear for closed comments, quadratic for an unterminated or many repeated
`/*` (each rescans to the end of the input; no cap on comment length). The R5 `uart_set_mode`
argument parser (001 code) is also quadratic on many repeated `uart_set_mode(` without `)`. The
self-test also times every case (limit 5.0 s) so a regex that backtracks badly fails the build.

`tools/check_elf_symbols.py --nm <nm> [--mode read|write] [--require SYM[,SYM...]] <elf>` runs after every link
(`POST_BUILD` on the `.elf` target, `--nm ${CMAKE_NM}`, `--mode` chosen from `CONFIG_DMT_BUS_WRITE` in
`CMakeLists.txt`, preceded by its `--self-test`). It fails if the image defines a symbol starting with
`mbc_`, `mbm_` or `mbs_`, if a `--require` symbol is not defined, or if nm/the ELF is missing or nm fails
(it never skips). Mode `read` (default, the read image) additionally forbids the write path: prefixes
`mb_link_write`, `mb_rtu_build_fc06`, `mb_rtu_parse_fc06`, `mb_rtu_fc06_`, `write_guard_`. Mode `write`
(the write image) additionally requires `mb_link_write`, `mb_rtu_build_fc06` and `write_guard_step`. It also
fails when the nm output yields no defined symbols or lacks `app_main` (wrong ELF or nm format), so an
empty output cannot pass vacuously.
Required symbols are set in `DMT_ELF_REQUIRED_SYMBOLS` in `CMakeLists.txt` (empty until task 006).
Manual run: `python3 tools/check_elf_symbols.py --nm <xtensa-esp32s3-elf-nm> [--mode write] build/daikin_matter_thermostat.elf`
(`build_write/...` for the write image, with `--mode write`).
esp-clang/clang-tidy is not installed (`idf.py clang-check`: "Espressif Clang was not found").

## Host tests (IDF `linux` target, Unity)
    cd firmware/test/host
    idf.py --preview set-target linux
    idf.py build
    ./build/host_tests.elf; echo $?

Needs the system package `libbsd-dev` (IDF linux target requirement; `sudo apt install libbsd-dev`).
Exit code 0 means all tests passed. Tests use `TEST_CASE(...)` with `unity_run_all_tests()`
(works on the linux target in v6.0.2, see `main/test_main.c`).
Sources: `test/host/main/test_*.c` and `components/*/test_host/test_*.c` (globbed). A component with
host tests is added to `REQUIRES` in `test/host/main/CMakeLists.txt`. `FIXTURE_DIR` is defined as the
absolute path of `firmware/test/fixtures` (created by task 011).
After adding a new `test_*.c`, run `idf.py reconfigure` in `test/host` (plain `file(GLOB)`, no `CONFIGURE_DEPENDS`).
