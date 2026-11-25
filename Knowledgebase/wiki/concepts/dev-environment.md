---
title: Dev environment
type: concept
sources: ["project-brief.md"]
updated: 2026-11-25
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
- ELF check `firmware/tools/check_elf_symbols.py` forbids only `mbc_|mbm_|mbs_|mb_link_write` (line 22; local inspection by the orchestrator, session 2026-11-24). "No Wi-Fi / lwip / Matter linked" in the probe image was a manual `build-verifier` acceptance item, not enforced by tooling. The phase-2a Matter project gets its own separate bus-symbol check ([[concepts/firmware-architecture]] section 3).

## Matter controllers (phase 2a; user statement / user decision, session 2026-11-24; not in raw)
| Item | Fact |
|---|---|
| User setup | Home Assistant **Core** in a Docker container on a Synology NAS; an Apple TV for testing; the target site currently uses a SmartThings hub (user statement) |
| 2a acceptance | chip-tool (reference) + Home Assistant (Core + separate `python-matter-server`) + Apple Home (cross-check); SmartThings acceptance on site later (phase 2b/3), incl. test-device registration (user decision) |
| HA Core note | HA Core has no add-ons, so the Matter Server runs as a separate `python-matter-server` container (host network; IPv6 needed on the NAS) (external, unverified) |
| Apple Home | Shows "Uncertified accessory" for test VID 0xFFF1 devices (external, unverified) |
| SmartThings | Typically requires registering test devices (VID/PID) in the SmartThings Developer Workspace (external, unverified) |
| Multi-admin | Matter multi-admin allows several controllers on one device (external, unverified) |
| Build host risk | CHIP build with about 7 GB RAM (orchestrator `free -g`, session 2026-11-24) may need a parallel-job cap ([[concepts/firmware-architecture]] section 8) |

## Registry facts (fetched from components.espressif.com 2026-11-02; external, not in raw)
- `espressif/esp_matter` 1.6.1, supports all targets incl. esp32s3; page recommends ESP-IDF v6.0.2 for Matter projects; install with `idf.py add-dependency "espressif/esp_matter^1.6.1"`.

## Approved components (user decision 2026-11-07; versions from the registry, external)
- `espressif/esp_matter` ^1.6.1: P2.
- **Removed 2026-11-24:** `espressif/esp-modbus`. The user reversed D7/Q9 and chose an own minimal FC03-only RTU master (E2, user decision, session 2026-11-24, not in raw; [[concepts/firmware-architecture]] section 6 "Task 002 spike decisions"). Reason: the task-002 spike found that 2.1.4 merges slave exception and bad reply into one error and drops the exception code (project evidence, `docs/spikes/002-esp-modbus.md` S3, not a raw source); 2.1.4 was the latest registry version (registry page fetched 2026-11-24, external).
  - *History:* `espressif/esp-modbus` **==2.1.4** exact (pin re-confirmed by the user 2026-11-08, v0.3 Q9): serial master via `mbc_master_send_request` from phase 1 on, only in `components/mb_link/`. *(Superseded 2026-11-07 v0.2 use: P1 only its private `mb_crc16`, P3 its serial master.)*
- **Registry-only rule and own code (user-confirmed interpretation, session 2026-11-24, not in raw):** own project components (e.g. `mb_rtu`, `rs485_port`) are compliant: E2 adds no dependency and removes one.
Details, guards and phases: [[concepts/firmware-architecture]].

## Open questions
1. Flashing/monitoring path from WSL2 (usbipd) or Windows — decide when a board is attached.
2. `espressif/esp_matter` and its (large) dependencies are not yet added; architecture v0.3 approved 2026-11-08, added in phase 2 ([[concepts/firmware-architecture]]). **2026-11-24:** added in the separate project `firmware/matter/` (task 2a-A); step 0 checks it builds on IDF v6.0.2 from the registry only (user decision, session 2026-11-24). **2026-11-25:** step 0 passed (task 015: registry-only, offline configure from lock + cache; build `idf.py reconfigure` + `ninja -C build -j2`, about 26 min clean, no OOM; session fact, [[concepts/firmware-architecture]] section 7). *(Superseded 2026-11-24: "esp-modbus is added in task 002"; esp-modbus is not used, E2.)*
3. `libbsd-dev` for the linux host target: user to install (`sudo apt install libbsd-dev`).
4. User action: prepare the `python-matter-server` container and IPv6 on the NAS (session 2026-11-24).
