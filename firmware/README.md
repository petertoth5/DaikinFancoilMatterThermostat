# Firmware

ESP-IDF v6.0.2 only. Activate per shell: `. ~/esp/esp-idf-v6.0.2/export.sh`.

## Build (esp32s3)
    cd firmware
    idf.py set-target esp32s3
    idf.py build

`sdkconfig.defaults` is applied on set-target. Kconfig menu "Daikin bus (phase 1 read probe)":
`idf.py menuconfig`. `CONFIG_DMT_BUS_WRITE=y` fails CMake (phase 3 is not specified).

## Bus-access lint
`tools/check_bus_access.py` (rules R1-R8) and `tools/check_bus_access.py --self-test` run on every
`idf.py build` (CMake target `bus_access_lint`); a violation or failing self-test fails the build.
Manual run: `python3 tools/check_bus_access.py [--self-test]` from `firmware/`.
Out of scope for the lint: `#include MACRO`, `##` token pasting, positional struct initializers and
aliases for R8. R8 is a heuristic; the real guard is that the FC06 constant lives only in WRITE_FILE,
plus the ELF check in task 002. Backslash-newline continuations are spliced before matching.
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
