# 017 Station-only Wi-Fi: no SoftAP (phase 2a fix)

Status: done (committed 6b10c93; completion and STA-only bench result recorded in d63e73b). Approved by user 2026-11-25 (re-analysis requested). Re-analysis 2026-11-25: the root cause was corrected (it is not old NVS content). The fix (early `esp_wifi_init`, force `WIFI_MODE_STA` before `esp_matter::start`) is unchanged. Only the log lines in Behaviour 2 changed: they now also name the mode in the non-warning cases, so D5 shows the driver default. Alternative C was rejected by the user 2026-11-25. Depends on 015, 016 (commit 276e056). Model: **sonnet** (small change, but it runs before `esp_matter::start` and must match the CHIP Wi-Fi init order exactly; a wrong order breaks boot, so this is judgement, not mechanical). After the task: `build-verifier`; `firmware-reviewer` only if the diff goes beyond the two files below.

## Finding (bench, user-confirmed 2026-11-25)
The user ran `idf.py -p /dev/ttyACM0 erase-flash` (full chip erase) right before the first flash of image 276e056. No other image has run since, and there has been no BOOT factory reset. So the Wi-Fi NVS was **empty at first boot**. Boot log of the commissioned board (2 fabrics, connected as station), about 2.19 s after boot, right after `chip[DL]: Starting ESP WiFi layer`: `wifi:mode : sta (1c:db:d4:46:ec:98) + softAP (1c:db:d4:46:ec:99)`. The user's phone sees an extra network near the board. Its SSID is **not** `ESP_...`-style (exact name pending, Q1).

`sdkconfig`: `CONFIG_ENABLE_WIFI_STATION=y` (line 4408). `CONFIG_ENABLE_WIFI_AP` is absent from `sdkconfig` and from `build/config/sdkconfig.h`. `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=y` (2811). `CONFIG_ESP_WIFI_NVS_ENABLED=y` (2765).

## Root cause (from source; esp_matter 1.6.1, IDF v6.0.2)
Paths below are under `managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/` unless stated.

1. **No code in this image asks for an AP.**
   - `CHIP_DEVICE_CONFIG_ENABLE_WIFI_AP` defaults to 0 (`src/include/platform/CHIPDeviceConfig.h:542-543`). The generated `build/esp-idf/espressif__esp_matter/platform/CHIPDeviceBuildConfig.h` does not override it.
   - `src/platform/ESP32/` contains no AP code at all: no `WIFI_AP` config symbol, no `esp_wifi_set_config(WIFI_IF_AP, ...)`, no AP netif.
   - `esp_matter::start` has `SetWiFiAPMode` commented out (`components/esp_matter/esp_matter_core.cpp:322`).
   - The only `esp_wifi_set_mode` in the compiled stack is `ESP32Utils::EnableStationMode` (`src/platform/ESP32/ESP32Utils.cpp:126`). The esp_matter console `wifi` command sets `WIFI_MODE_STA` only (`components/esp_matter_console/esp_matter_console_wifi.cpp:36`).
   - `firmware/matter/main` and `components` contain no Wi-Fi or netif calls.
2. **Old NVS content (the previous hypothesis) is ruled out** by the erase-flash fact above.
3. **The only code path to STA+AP is `EnableStationMode` itself.** If the current mode is `WIFI_MODE_AP`, it sets `WIFI_MODE_APSTA` (`ESP32Utils.cpp:122-126`) and logs `Changing ESP WiFi mode: AP -> STA+AP` (`:124`). The call order is:
   - `esp_matter::start`
   - `InitWiFiStack`: `esp_wifi_init`, no mode set (`esp_matter_core.cpp:378`; `ESP32Utils.cpp:285-293`)
   - `chip_init`, then `InitChipStack` (`esp_matter_core.cpp:319`)
   - `ConnectivityManagerImpl::_Init`, then `InitWiFi` (`ConnectivityManagerImpl.cpp:65`)
   - `EnableStationMode` (`ConnectivityManagerImpl_WiFi.cpp:350`)
   - later `DriveStationState`, then `StartWiFiLayer`: `Starting ESP WiFi layer`, then `esp_wifi_start` (`ESP32Utils.cpp:91-93`). The driver prints `mode : ...` here, which matches the log position.

   With `CONFIG_ESP_WIFI_NVS_ENABLED=y`, the new mode is written to NVS and survives reboots. After that, `EnableStationMode` sees `APSTA`, treats it as "station enabled" and keeps it (`:116-120`). No code sets the mode after `esp_wifi_start`. `DriveStationState` only calls `EnableStationMode` again (`ConnectivityManagerImpl_WiFi.cpp:493`), and that call is a no-op for STA or APSTA.
4. **So the AP must come from the closed Wi-Fi driver's mode right after `esp_wifi_init` with empty NVS** *(inferred; the driver is the binary `libnet80211.a`, so this cannot be checked in source)*. Ranked:
   - **H1 (leading):** the driver's initial mode is `WIFI_MODE_AP`. CHIP then turns it into `STA+AP` (point 3) and writes it to NVS. The first-boot log would contain `Changing ESP WiFi mode: AP -> STA+AP`.
   - **H2:** the driver's initial mode is already `WIFI_MODE_APSTA`. CHIP keeps it, and there is no `Changing ...` line.
   - Conflict, recorded: the IDF header says "The default mode is station mode" (`esp-idf-v6.0.2/components/esp_wifi/include/esp_wifi.h:382`). With STA or NULL as the default, CHIP would end in STA (`ESP32Utils.cpp:116, 122`) and no AP would exist. The observed log contradicts the header. Either the header is wrong for an empty NVS, or the beacon is not explained by this image (H4/H5 below).
   - A factory reset calls `esp_wifi_restore()` and then reboots (`src/platform/ESP32/ConfigurationManagerImpl.cpp:495, 514`). That resets the mode to the driver default, so under H1/H2 **the AP comes back after every factory reset** unless our guard runs.

### SSID mapping (to discriminate with Q1)
| Hypothesis | SSID the phone would show | BSSID |
|---|---|---|
| H1/H2 driver-default AP, default AP config | `ESP_46EC99` (`ESP_` plus the last 3 bytes of the SoftAP MAC), open, no lock *(external: IDF Wi-Fi guide, `docs/en/api-guides/wifi-driver/overview.rst:537`)* | `1c:db:d4:46:ec:99` |
| H3 CHIP SoftAP code | `MATTER-` plus a suffix (`CHIP_DEVICE_CONFIG_WIFI_AP_SSID_PREFIX`, `CHIPDeviceConfig.h:552-553`). **Not compiled in this image** (point 1). Seeing it would mean another binary is on the board. | `...:99` |
| H4 not this board's beacon | any name (a neighbour's AP, printer, TV, another device that happens to be nearby) | not `...:99` |
| H5 not Wi-Fi at all | a BLE or "nearby accessory" entry (for example `MATTER-xxxx` or a vendor name) in the phone's Bluetooth or Home UI, not in the Wi-Fi list | none |
| H6 driver default AP config differs in IDF 6.0.2 | a non-`ESP_` name. Cannot be checked in source (closed driver). | `...:99` |

The user said the SSID is not `ESP_...`. So the BSSID decides between H6 (board, `...:99`) and H4/H5 (not the board's AP). Q1 asks for it. The `+ softAP` log line itself proves the board runs an AP interface, whatever the phone shows.

CHIP creates no AP netif, so a client that joins gets no DHCP or IP stack. The Matter node cannot be reached through the AP *(inferred)*. Still, it is an unwanted, probably open beacon. It also uses airtime and power next to BLE coexistence. Side effect of today's state: in STA+AP mode the primary MAC is the AP MAC (`ConfigurationManagerImpl.cpp:343-346`), and DNS-SD derives the hostname from it (`src/app/server/Dnssd.cpp:193`).

### Does the fix defeat the cause? Yes, unchanged
Our guard runs after `esp_wifi_init` and before `InitChipStack`. If it sets `WIFI_MODE_STA`, `EnableStationMode` returns early (`ESP32Utils.cpp:116`), so the AP-to-STA+AP promotion at `:122` never runs. No code path sets the mode after `esp_wifi_start` (point 3), so a "set before start" fix is enough. The guard also runs on the boot right after a factory reset (`esp_wifi_restore` and then `esp_restart`), so the driver default never reaches `esp_wifi_start`.

## Goal
The node runs Wi-Fi in station mode only, on every boot, whatever mode the driver starts with or NVS holds. This uses public IDF APIs only. No change to managed_components, no new dependency, no Kconfig change, fabrics kept.

## Files owned
`firmware/matter/main/app_main.cpp`, `firmware/matter/main/CMakeLists.txt` (add `esp_wifi esp_netif esp_event` to `REQUIRES`). Nothing else.

## Wiki pages relied on
`concepts/dev-environment` (registry-only rule, IDF v6.0.2); `concepts/firmware-architecture` sec. 5 (Commissioning). The Wi-Fi driver and CHIP behaviour above are **external** (source references listed).

## Behaviour
1. In `app_main`, after `nvs_flash_init` and **before** `esp_matter::start`, make the same calls in the same order as `ESP32Utils::InitWiFiStack`:
   1. `esp_netif_init()` (idempotent, `esp_netif_lwip.c:552-554`).
   2. `esp_event_loop_create_default()`. Accept `ESP_OK` or `ESP_ERR_INVALID_STATE`, as `esp_matter_core.cpp:372-376` does.
   3. `esp_netif_create_default_wifi_sta()`, only if `esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")` is NULL.
   4. `wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&cfg)` (same form as `ESP32Utils.cpp:288-289`).

   CHIP then skips its own netif creation and `esp_wifi_init` (`ESP32Utils.cpp:275, 286`). Wi-Fi is **not** started here.
2. Call `esp_wifi_get_mode`, then:
   - `WIFI_MODE_AP` or `WIFI_MODE_APSTA`: log once at warning level `Wi-Fi mode after init <AP|STA+AP>, forcing STA`, then `esp_wifi_set_mode(WIFI_MODE_STA)`. This writes NVS, so the warning normally appears once per board and again after each factory reset. The check stays as a guard.
   - `WIFI_MODE_NULL`: log at info level `Wi-Fi mode after init NULL, setting STA`, then set STA.
   - `STA`: log at info level `Wi-Fi mode after init STA`, nothing else.

   The log text names the mode the driver reported, so D1 and D5 show the real default (H1 vs H2).
3. Any failure in 1-2 is logged and aborts `app_main` through the existing `ok()` pattern (a node that might beacon an AP must not start).
4. No other change: no STA config, connect, power-save, country or Kconfig change. BLE, commissioning, reset button, sim console and bridge are unchanged.

BLE commissioning side effect: none expected. BLE (NimBLE, `CONFIG_USE_BLE_ONLY_FOR_COMMISSIONING=y`) does not depend on the Wi-Fi mode. CHIP's network commissioning sets STA credentials through `EnableStationMode`, which accepts `WIFI_MODE_STA` (`ESP32Utils.cpp:116`). Expected changes:
- Coexistence airtime improves slightly.
- The DNS-SD hostname changes once, from the AP MAC (`...EC99`) to the STA MAC (`...EC98`). Controllers resolve the operational instance name, not the hostname, so rejoin should work *(inferred; on-device check D3)*.

## Alternatives (not chosen)
- **A. Matter factory reset (BOOT >= 5 s) or `idf.py erase-flash`:** **disproven as a cure.** Erase-flash was done before the first boot, and the AP still appeared. A factory reset (`esp_wifi_restore`) returns the mode to the same driver default (H1/H2). Both also lose the fabrics.
- **B. `esp_wifi_set_mode(WIFI_MODE_STA)` after `esp_matter::start`** (or on `kServerReady`): 3 lines. But `EnableStationMode` has already promoted the mode and Wi-Fi may already be started. The AP then beacons briefly on the first boot and after every factory reset, and the mode switches while the STA is connecting (STA drop behaviour not verified).
- **C. `CONFIG_ESP_WIFI_SOFTAP_SUPPORT=n`:** **rejected by the user 2026-11-25. Do not add it.**

## Acceptance checks
1. Capped build of `firmware/matter` (same command as 015/016) passes, with 0 warnings from our files. The bus guard (`check_no_bus.py` self-test, `--sources`, `--nm`) passes.
2. Scope:
   - `git diff --stat` touches only the two owned files.
   - `git status --porcelain firmware | grep -v '^?? firmware/matter/'` is empty (phase-1 unchanged).
   - No `managed_components` edits, no new entries in `dependencies.lock`, no `sdkconfig`/`sdkconfig.defaults` change (`CONFIG_ESP_WIFI_SOFTAP_SUPPORT` stays `y`).
3. Code inspection:
   - The Wi-Fi init calls come before `start(...)`, in the order of Behaviour 1.
   - Behaviour 2 covers all four modes.
   - No `esp_wifi_start`/`esp_wifi_connect` in our code.
   - No private CHIP headers: `platform/ESP32/ESP32Utils.h` is **not** included. The key `"WIFI_STA_DEF"` is the IDF default (`esp_netif_defaults.h:52`).
4. Host tests in `firmware/matter/test/host` still pass (no change expected).

## On-device checks (for the user)
- **D1** First boot after flashing the commissioned board: the warning `Wi-Fi mode after init STA+AP, forcing STA` appears once. NVS holds the mode that CHIP wrote under H1, or the driver default under H2. The boot log shows `mode : sta (...)` with no `+ softAP`. If there is no warning but `+ softAP` stays, escalate.
- **D2** Phone scan: the extra network has gone. If it is still there with the board running station-only, it was not the board (H4/H5).
- **D3** The board rejoins Wi-Fi and both fabrics without re-commissioning (HA and Apple Home or chip-tool read EP1).
- **D4** Second reboot: info `Wi-Fi mode after init STA`, no warning, `mode : sta` only.
- **D5** BOOT >= 5 s factory reset, then the boot right after it:
  - Record the guard line. `AP` confirms H1, `STA+AP` confirms H2, `NULL`/`STA` means the driver default is harmless (then the original AP cause is unexplained; report it).
  - `mode : sta` only.
  - Commission again over BLE: works.

## Test strategy
No host test: the change is IDF glue with no pure logic. It is proven by D1-D5 on the device and by code inspection (acceptance 3).

## Out of scope
Kconfig changes of any kind (alternative C rejected by the user), Wi-Fi power save, country code, any change to commissioning, BLE, mapping, bridge or phase-1 files, scheduling.

## Escalate (stop, report)
- `esp_wifi_init` or `esp_matter::start` fails after the pre-init (order or double-init conflict).
- D1 shows no warning but `+ softAP` stays (root cause wrong).
- STA cannot rejoin after the mode change (D3).
- Any need to include CHIP platform headers, edit managed_components or change Kconfig.

## Open questions for the user
- **Q1** Exact SSID shown by the phone, whether it has a lock icon, and if possible its BSSID (for example from a Wi-Fi analyser app). A BSSID of `1c:db:d4:46:ec:99` means it is the board's AP (H1/H2 with a non-default name, H6). Any other BSSID means it is not the board (H4). Also: does it disappear while the board is unpowered?
- **Q2** (optional) Is the serial log of the very first boot after erase-flash still available? `Changing ESP WiFi mode: AP -> STA+AP` before `Starting ESP WiFi layer` confirms H1 now. Its absence points to H2. D5 answers the same question after the fix if the log is gone.
- Q3 (previous: alternative C follow-up): closed, rejected by the user 2026-11-25.
