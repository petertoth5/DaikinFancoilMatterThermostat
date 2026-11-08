---
title: Dev environment
type: concept
sources: ["project-brief.md"]
updated: 2026-11-09
---

# Dev environment

## Rules (from [[sources/project-brief]])
- Framework: **ESP-IDF**, target `esp32s3` ([[entities/esp32-s3-wroom-1]]).
- Every module is consumed through the **IDF component manager** (`idf-component-manager`, PyPI) and must exist in the **ESP component registry** (components.espressif.com). Nothing else (no git submodules, no vendored third-party code, no Arduino libs).

## Installed setup (done 2026-11-02, local; not from raw)
- Host: WSL2 Ubuntu, Python 3.12.3, 12 cores, ~7 GB RAM.
- Previous broken EIM install (`~/.espressif`, EIM caches, PATH blocks in `~/.bashrc`/`~/.profile`) was removed at the user's request; the old `esp-matter` git clone went with it (git clones of esp-matter are not allowed by the registry-only rule).
- **ESP-IDF v6.0.2** cloned (shallow, recursive) to `~/esp/esp-idf-v6.0.2`; toolchain + Python venv installed with `./install.sh esp32s3` into `~/.espressif` (recreated by the installer; `idf-component-manager` 3.1.2 comes with the venv).
- Activate per shell: `. ~/esp/esp-idf-v6.0.2/export.sh` (not added to shell rc on purpose).
- Skeleton project in `firmware/` (`main/idf_component.yml` declares only `idf >=6.0.2`); `idf.py set-target esp32s3 && idf.py build` succeeds. `build/`, `managed_components/`, `sdkconfig*`, `dependencies.lock` are git-ignored.
- No serial device attached; flashing path from WSL2 (usbipd-win vs Windows flashing) deferred until a board is available.

## Host build facts (task 001, 2026-11-09; session implementation facts from verified build/test output, not in raw)
- The linux host target (host tests) is selected with `idf.py --preview set-target linux` in IDF v6.0.2.
- The linux host target needs `libbsd-dev`: **user action pending**, `sudo apt install libbsd-dev` (until then the session used a scratchpad `CPATH`/`LIBRARY_PATH` workaround).
- esp-clang / clang-tidy are not installed.
- The bus-access lint `firmware/tools/check_bus_access.py` (with its self-test) is stdlib-only Python and runs on every build ([[concepts/firmware-architecture]] section 3).

## Registry facts (fetched from components.espressif.com 2026-11-02; external, not in raw)
- `espressif/esp_matter` 1.6.1, supports all targets incl. esp32s3; page recommends ESP-IDF v6.0.2 for Matter projects; install with `idf.py add-dependency "espressif/esp_matter^1.6.1"`.

## Approved components (user decision 2026-11-07; versions from the registry, external)
- `espressif/esp-modbus` **==2.1.4** exact (pin re-confirmed by the user 2026-11-08, v0.3 Q9): serial master via `mbc_master_send_request` from phase 1 on, only in `components/mb_link/`. *(Superseded 2026-11-07 v0.2 use: P1 only its private `mb_crc16`, P3 its serial master.)*
- `espressif/esp_matter` ^1.6.1: P2.
Details, guards and phases: [[concepts/firmware-architecture]].

## Open questions
1. Flashing/monitoring path from WSL2 (usbipd) or Windows — decide when a board is attached.
2. `espressif/esp_matter` and its (large) dependencies are not yet added; architecture v0.3 approved 2026-11-08, added in phase 2 ([[concepts/firmware-architecture]]). esp-modbus is added in task 002.
3. `libbsd-dev` for the linux host target: user to install (`sudo apt install libbsd-dev`).
