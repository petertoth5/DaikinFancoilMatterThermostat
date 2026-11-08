# Project components

Rules for every component in this directory:

- Compile with `-Wall -Wextra -Werror`:
  `target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)` in its `CMakeLists.txt`.
- Host tests live in `<component>/test_host/test_*.c`. They are picked up by `test/host/` (see `../README.md`);
  append the component to `REQUIRES` in `test/host/main/CMakeLists.txt`.
- Bus access is restricted by `../tools/check_bus_access.py` (run on every `idf.py build`);
  `test_host/` directories are not scanned by it.
- Dependencies come only from the ESP component registry (`idf_component.yml`).
