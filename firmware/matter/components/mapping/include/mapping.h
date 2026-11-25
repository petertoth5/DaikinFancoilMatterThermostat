#pragma once

/* mapping.h - register model -> Matter Thermostat values (task 016; pure C99, no IDF).
 * PROVISIONAL: 233 values 3/5, 231 vs 008 and the starred registers wait for the probe log
 * (architecture sections 5, 6). Matter units: 0.01 degC; SystemMode enum Off 0 / Cool 3 / Heat 4 (external). */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "reg_model.h"  /* phase-1 pure C header without its own extern "C" guards */

#define MAP_MODE_OFF 0
#define MAP_MODE_COOL 3
#define MAP_MODE_HEAT 4

/* Published constants (architecture section 5). */
#define MAP_ABS_MIN_CENTI 500
#define MAP_ABS_MAX_CENTI 4000

/* Bits of mapping_changed(): one per attribute group the bridge pushes. */
#define MAP_CH_LIMITS (1u << 0)       /* Min/Max Heat/Cool setpoint limit (4 attributes) */
#define MAP_CH_HEAT_SP (1u << 1)
#define MAP_CH_COOL_SP (1u << 2)
#define MAP_CH_LOCAL_TEMP (1u << 3)
#define MAP_CH_SYSTEM_MODE (1u << 4)
#define MAP_CH_ALL (MAP_CH_LIMITS | MAP_CH_HEAT_SP | MAP_CH_COOL_SP | MAP_CH_LOCAL_TEMP | MAP_CH_SYSTEM_MODE)

typedef struct {
    bool local_temp_valid;      /* false = null */
    int16_t local_temp;
    int16_t heat_sp, cool_sp;
    bool sp_observed;           /* false = still the default, not read from 231 */
    int16_t min_heat, max_heat, min_cool, max_cool;
    uint8_t system_mode;        /* MAP_MODE_* */
    bool season_auto_flag;      /* 233 = 0 without usable 104: mode is the last explicit season (D15) */
    bool limits_conflict;       /* limits unusable (kept) or a setpoint lies outside them */
} map_thermostat_t;

/* Values kept between runs ("keep" in the spec). */
typedef struct {
    map_thermostat_t kept;
    uint8_t last_season;        /* MAP_MODE_HEAT or MAP_MODE_COOL */
} map_state_t;

void mapping_init(map_state_t *st);

/* Maps the model as seen at now_ms and stores the result as the new "kept" values. */
void mapping_thermostat(map_state_t *st, const reg_model_t *m, int64_t now_ms, map_thermostat_t *out);

/* Bitmask of attribute groups whose value differs; first = true returns MAP_CH_ALL.
 * The flags (sp_observed, season_auto_flag, limits_conflict) never set a bit. */
uint32_t mapping_changed(const map_thermostat_t *last_pushed, const map_thermostat_t *cur, bool first);

#ifdef __cplusplus
}
#endif
