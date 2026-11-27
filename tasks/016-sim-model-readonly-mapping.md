# 016 Simulated register model + provisional read-only mapping (phase 2a, part B)

Status: done (committed 03dacf5; feature review and verification recorded in 2f932e7). Plan approved by the user 2026-11-24; amended 2026-11-25 for NONVOLATILE attributes (user decision "Accept + limit", section "NVS persistence"). Depends on 015. Model: **sonnet** (pure logic + host tests + esp_matter glue). After the task: `build-verifier`, then the **one** `firmware-reviewer` review of the phase-2a feature (015 + 016); text-only fixes get no re-review (S3).

## Goal
EP1 attributes follow a simulated `reg_model` through a pure, host-tested read mapping. The bridge reads the model only through one snapshot getter, so phase 2b swaps the source (bus) without touching mapping or bridge. The whole mapping is **PROVISIONAL**: 233 values 3/5, 231 vs 008 and every starred register wait for the probe log.

## Files owned (all new)
`firmware/matter/components/sim_source/`, `firmware/matter/components/mapping/` (pure C99, no IDF includes), `firmware/matter/components/matter_bridge/` (esp_matter glue), `firmware/matter/main/sim_console.c/.h`; edits to `firmware/matter/main/`, `firmware/matter/CMakeLists.txt` (add `components`) and `firmware/matter/sdkconfig.defaults` (one line, see "NVS persistence"; regenerate `sdkconfig`); host project `firmware/matter/test/host/` (linux target, Unity). `firmware/test/host` is **not** reused: adding tests there needs edits to its phase-1 `CMakeLists.txt` files. The new host project lists in `EXTRA_COMPONENT_DIRS` only `daikin_regs`, `reg_model`, `sim_source`, `mapping` (never `matter_bridge`), with `-Wall -Wextra -Werror` on the test sources.

## Wiki pages relied on
`concepts/firmware-architecture` sec. 2 (`mapping`, `matter_bridge`, threading), 3 (stale = 3 x period, NVS wear), 5 (EP1 mapping, D15); `concepts/register-map` (000, 008, 201, 202, 203, 231, 233, 245, 246, 247, 104; scale 0.1); `concepts/flag-registers` (201 bit 7, 104 STAT bits 0/1); `concepts/web-restriction-flags` (247 bit 5). Reuses `reg_model.h`, `daikin_regs.h` unchanged.

## Interfaces (essential only)
```c
/* sim_source.h - pure, time injected */
typedef enum { SIM_LIVE, SIM_STALE, SIM_OFFLINE } sim_state_t;
void sim_init(sim_t *s, int64_t now_ms);           /* reg_model_init(15000, 180000) + seeds */
void sim_tick(sim_t *s, int64_t now_ms);
int  sim_set(sim_t *s, uint16_t reg, uint16_t raw, int64_t now_ms);   /* -1 undocumented */
int  sim_unsupported(sim_t *s, uint16_t reg, int64_t now_ms);         /* exc 02 */
void sim_set_state(sim_t *s, sim_state_t st, int64_t now_ms);
const reg_model_t *sim_model(const sim_t *s);
/* mapping.h - pure; Matter units 0.01 degC, SystemMode enum Off 0 / Cool 3 / Heat 4 (external) */
void mapping_init(map_state_t *st);
void mapping_thermostat(map_state_t *st, const reg_model_t *m, int64_t now_ms, map_thermostat_t *out);
/* bitmask MAP_CH_LIMITS | MAP_CH_HEAT_SP | MAP_CH_COOL_SP | MAP_CH_LOCAL_TEMP | MAP_CH_SYSTEM_MODE;
   first = true -> all bits; flags (sp_observed, season_auto_flag, limits_conflict) never set a bit */
uint32_t mapping_changed(const map_thermostat_t *last_pushed, const map_thermostat_t *cur, bool first);
/* matter_bridge.h - the phase-2b seam */
typedef void (*bridge_snapshot_fn_t)(reg_model_t *out, int64_t *now_ms);
esp_err_t matter_bridge_start(uint16_t endpoint_id, bridge_snapshot_fn_t get_snapshot);
```
`map_thermostat_t`: `local_temp_valid`, `local_temp`, `heat_sp`, `cool_sp`, `sp_observed`, `min_heat/max_heat/min_cool/max_cool`, `system_mode`, `season_auto_flag`, `limits_conflict`.

## Behaviour rules
Sim (pure): seeds 000=215, 008=200, 201=0, 202=160, 203=280, 231=200, 233=3 *(inferred demo values within the p4 ranges)*; 104/245/246/247 unset (like unavailable starred registers). LIVE: every tick re-sets every seeded register OK at `now`; `sim_set` applies at once (LIVE) or on return to LIVE. STALE: no refresh, values age out. OFFLINE: every seeded register `reg_model_set_timeout` (raw and `t_ok` kept).

Mapping ("fresh" = `reg_model_fresh` and not `out_of_range`; "keep" = previous output, initial value in brackets):
- M1 LocalTemperature = 000 raw x 10 if fresh, else **null** (unknown, stale, offline, out of range).
- M2 Both setpoints = 231 raw x 10 if fresh, `sp_observed` = true; else keep [2000, "default, not observed"]. 008 not used (diagnostic).
- M3 Limits (heat = cool): 202/203 fresh and 202 <= 203 -> [202 x 10, 203 x 10]; if 247 fresh with bit 5 and 245/246 fresh -> intersect with [245 x 10, 246 x 10] *(inferred, same rule as the write clamp, sec. 3)*; empty or invalid -> keep + `limits_conflict`. Initial [500, 4000]. AbsMin/AbsMax constant 500/4000.
- M4 SystemMode: 201 fresh with bit 7 -> Off. Else 233 fresh: 3 -> Heat, 5 -> Cool (stored as last explicit season); 0 -> if 104 fresh with exactly one of bit 1 (Heat) / bit 0 (Cool) set, that mode; otherwise last explicit season and `season_auto_flag` (D15; both/neither bits = open wiki item, provisional). 201 or 233 not fresh -> keep [Heat, *inferred*].
- M5 Setpoint outside the published limits: publish anyway, set `limits_conflict` (no clamping of read values).

Bridge: an `esp_timer` every **1 s** posts work into the Matter context (`PlatformMgr().ScheduleWork` or the esp_matter equivalent *(external)*); the work function calls `get_snapshot` into a static buffer, runs `mapping_thermostat`, then updates **only changed** attributes (`mapping_changed` against the last *successfully pushed* value, not against `mapping_init` defaults) in the order limits -> setpoints -> LocalTemperature -> SystemMode, with the own-update flag from 015 set. The first cycle is scheduled at once in `matter_bridge_start` (not after the first 1 s) and runs with `first = true`. A failed update is logged once per value, leaves `last_pushed` unchanged for that attribute and is retried next cycle. The producer (sim glue) never takes the CHIP lock; the bridge never blocks on the sim mutex for longer than the copy. Writes stay rejected (015 Behaviour 4), including those caused by SetpointRaiseLower.

## NVS persistence (user decision 2026-11-25, "Accept + limit")
Fact from the 015 build and the esp_matter 1.6.1 source *(external, `managed_components/espressif__esp_matter`, `data_model/esp_matter_data_model.cpp`)*: `cluster::thermostat::create()` marks OccupiedHeatingSetpoint, OccupiedCoolingSetpoint and SystemMode `ATTRIBUTE_FLAG_NONVOLATILE`; the flag cannot be cleared through the public API. The 8 limit attributes (015) and LocalTemperature are volatile. We accept the flag and limit the writes:
- **Unchanged -> no write.** The bridge rule above (`mapping_changed`). In addition, esp_matter itself returns `ESP_ERR_NOT_FINISHED` from `set_val_internal` for an equal value and `attribute::update` maps it to `ESP_OK` without NVS access *(external, source)*; treat that as success.
- **Deferred persistence.** `matter_bridge_start` calls `esp_matter::attribute::set_deferred_persistence()` (public, `esp_matter_data_model.h`) on the 3 attribute handles (`attribute::get(endpoint_id, Thermostat::Id, <attr>)`) before the first cycle; any failure -> return the error (escalate). The delay is the global Kconfig `CONFIG_ESP_MATTER_DEFERRED_ATTR_PERSISTENCE_TIME_MS` (no per-attribute delay in the API); set **`=60000`** in `sdkconfig.defaults`. Semantics *(external, source)*: on a change the value is written once when a 60 s timer expires; the timer is not restarted by further changes, so each attribute is written at most once per 60 s, holding its latest value. The value is passed as `Milliseconds16`, so it must stay <= 65535. No other attribute in our image uses deferred persistence (grep of esp_matter 1.6.1: none set internally).
- **Why 60 s and not the suggested 3 s:** the persisted value has no use for us (the node is a mirror; the model overwrites it at boot, below), so the only cost of a longer delay is that a reboot within 60 s of a change leaves an older stale value in NVS, which is overwritten anyway. 60 s cuts the worst case 20x against 3 s (estimate below) and is the largest round value under the 65535 ms limit.
- **Boot behaviour:** the stored value is loaded when 015 creates the attribute and overrides the static initial value *(external, source)*, so after reboot the last persisted value (up to 60 s old, from the previous session) is visible from `esp_matter::start()` until the first bridge cycle. 016 handles it by the immediate first cycle with `first = true` (all attributes pushed; equal values cost no NVS write). Without `first`, a stored 2300 against a mapped 2000 that equals the bridge's initial `last_pushed` would never be corrected. The remaining window (start until the first cycle runs in the Matter context, well under 1 s *(inferred)*) is accepted: a controller may briefly read the old value.

**Wear note (estimate, not measured).** Inputs: NVS page 4096 B, 32-byte entries, 126 entries per page, one page kept free for garbage collection, ~100 000 erase cycles per sector, u16/enum8 attribute = 1 entry per write *(inferred, ESP-IDF NVS documentation, not in the wiki; esp_matter uses `nvs_set_u16`/`nvs_set_u8` for these types)*. The 48 KB `nvs` partition = 12 pages, 11 in use, about 1386 entries; Matter fabrics, keys and counters occupy an unknown part, assumed half -> ~600 entries free for churn. NVS writes circularly, so lifetime ~ 600 x 100 000 = 6e7 entry writes. Matter's own NVS writes (counters, fabrics) are not counted.

| Case (3 attributes) | Writes/day | Lifetime |
|---|---|---|
| No deferral, value changes every 1 s bridge cycle (theoretical worst) | 259 200 | ~230 days |
| Deferred 3 s, same worst case | 86 400 | ~1.9 years |
| **Deferred 60 s, same worst case (chosen)** | **4 320** | **~38 years** |
| Realistic 016 sim (setpoint/mode change only by console, say <= 100/day) | <= 100 | not limiting |

Phase 2b (poll period 5 s *(inferred from stale = 3 x period, 15 s)*) is bounded by the same 60 s cap. The worst case needs a value that flips every cycle (e.g. 201 bit 7 or 231 flapping), which the bus does not normally do *(inferred)*.

Console (`main/sim_console.c`, same REPL as 015; owns the `sim_t`, its mutex and a 1 s tick from `esp_timer_get_time()`): `sim set <reg> <raw>`, `sim unsup <reg>`, `sim stale`, `sim offline`, `sim live`, `sim show` (model entries + current mapped output + flags). Bad register or value -> error line, nothing changed.

## Acceptance checks
1. Host project `firmware/matter/test/host`: `idf.py --preview set-target linux && idf.py build && build/*.elf` -> all pass, 0 failures; report the count. Tests cover M1-M5 for **every** mapped attribute: fresh, unknown, stale (15 s boundary: 15000 ms fresh, 15001 ms not), offline, unsupported, out of range (e.g. 000 = 1001, 233 = 4), 202 > 203, 247 bit 5 intersection and empty intersection, 233 = 0 with 104 bit 0 / bit 1 / both / neither / unavailable, 201 bit 7 precedence over 233. Sim: tick refresh, STALE ageing, OFFLINE, LIVE recovery, deferred `sim_set`, undocumented register -> -1. `mapping_changed`: identical outputs -> 0 (no update when unchanged); two consecutive `mapping_thermostat` runs on an unchanged model -> 0; `first = true` -> all 5 bits; each attribute field changed alone -> only its bit; LocalTemperature valid <-> null -> its bit; only a flag changed (`sp_observed`, `season_auto_flag`, `limits_conflict`) -> 0.
2. Target build of `firmware/matter` (capped jobs as in 015): 0 warnings from our files; guard (015) passes; image < 75 % of `factory`.
3. `mapping` and `sim_source` include no IDF/esp_matter headers (grep); `matter_bridge` includes neither `sim_source.h` nor any bus header.
4. Phase-1 unchanged: same check as 015 acceptance 5 (build, lint, ELF check, 139 host tests).
5. NVS persistence: generated `sdkconfig` has `CONFIG_ESP_MATTER_DEFERRED_ATTR_PERSISTENCE_TIME_MS=60000`; `matter_bridge` calls `set_deferred_persistence` for exactly the 3 NONVOLATILE attributes and includes no `private/` esp_matter header (grep); the bridge pushes only bits returned by `mapping_changed`.

## On-device checklist (for the user; chip-tool reference, HA and Apple Home cross-check)
O1 After boot: LocalTemperature 2150, setpoints 2000, limits 1600/2800, SystemMode Heat. O2 `sim set 0 230` -> 2300 within ~2 s on all three controllers. O3 `sim stale` -> LocalTemperature null after 15-17 s; `sim live` -> back. O4 `sim offline` -> null; setpoints/SystemMode keep. O5 `sim set 201 128` -> Off; `sim set 201 0` + `sim set 233 5` -> Cool. O6 `sim set 233 0` -> last season + flag in `sim show`. O7 controller writes (setpoint, SystemMode, setpoint-raise-lower) -> error, values unchanged. O8 reboot -> rejoins, values from the sim seeds again. O9 NVS: `sim set 231 230` -> setpoints 2300; exactly one "Store the deferred attribute" log line per setpoint ~60 s later; repeat `sim set 231 240`, `sim set 231 230` within 60 s -> still at most one such line per attribute per 60 s; idle for 2 min -> no such line. Then reboot: the boot log / an early read may show 2300 (stale NVS, accepted), and within ~2 s of boot all controllers show the seed 2000 again; same for SystemMode after `sim set 233 5` + reboot (-> Heat from the seed 233 = 3).

## Out of scope
Bus source, `bus_task` integration, writes / `write_guard`, Fan endpoint, 008/STAT diagnostics in Matter, scheduling, SmartThings, changes to phase-1 files.

## Escalate (stop, report)
esp_matter rejects our own updates in a way the order rule cannot fix; ScheduleWork-style posting unavailable without taking the CHIP lock in the producer; any need to edit `reg_model` / `daikin_regs`; `set_deferred_persistence` fails or the 3 attributes need a private esp_matter header.
