/* mapping.c - register model -> Matter Thermostat values (task 016, PROVISIONAL, see mapping.h). */

#include "mapping.h"

#include "daikin_regs.h"

#define TENTHS_TO_CENTI 10            /* registers hold 0.1 degC, Matter 0.01 degC */
#define DEFAULT_SETPOINT_CENTI 2000   /* "default, not observed" (architecture section 5) */

#define REG_T1 0
#define REG_STAT 104
#define REG_PRG 201
#define REG_SPL 202
#define REG_SPH 203
#define REG_SP 231
#define REG_MAN 233
#define REG_SPL_W 245
#define REG_SPH_W 246
#define REG_WEB 247

#define PRG_STBY_BIT 0x0080u          /* 201 bit 7 (flag-registers) */
#define STAT_COOL_BIT 0x0001u         /* 104 bit 0 "Mod. Raff." (inferred) */
#define STAT_HEAT_BIT 0x0002u         /* 104 bit 1 "Mod. Risc." (inferred) */
#define WEB_SP_RESTRICT_BIT 0x0020u   /* 247 bit 5 (web-restriction-flags) */
#define MAN_WINTER 3                  /* 233: 0 = seasonal auto, 3 = winter, 5 = summer (inferred) */
#define MAN_SUMMER 5

/* Fresh and inside the fixed register range; *raw is set only then. */
static bool read_fresh(const reg_model_t *m, uint16_t reg, int64_t now_ms, uint16_t *raw)
{
    rm_entry_t e;
    if (!reg_model_fresh(m, reg, now_ms) || !reg_model_get(m, reg, &e) || e.out_of_range) {
        return false;
    }
    *raw = e.raw;
    return true;
}

static int32_t centi(uint16_t raw_tenths)
{
    return (int32_t)raw_tenths * TENTHS_TO_CENTI;
}

static void map_local_temp(const reg_model_t *m, int64_t now_ms, map_thermostat_t *out)
{
    uint16_t raw;
    out->local_temp_valid = read_fresh(m, REG_T1, now_ms, &raw);
    out->local_temp = out->local_temp_valid ? (int16_t)centi(raw) : 0;
}

static void map_setpoints(const reg_model_t *m, int64_t now_ms, map_thermostat_t *out)
{
    uint16_t raw;
    if (!read_fresh(m, REG_SP, now_ms, &raw)) {
        return;  /* keep */
    }
    out->heat_sp = out->cool_sp = (int16_t)centi(raw);
    out->sp_observed = true;
}

/* A limit register value in 0.01 degC; false when unusable (not fresh, out of range, or outside the Abs limits
 * because 202/203 have only one fixed bound). */
static bool read_limit(const reg_model_t *m, uint16_t reg, int64_t now_ms, int32_t *centi_out)
{
    uint16_t raw;
    if (!read_fresh(m, reg, now_ms, &raw)) {
        return false;
    }
    *centi_out = centi(raw);
    return *centi_out >= MAP_ABS_MIN_CENTI && *centi_out <= MAP_ABS_MAX_CENTI;
}

/* 247 bit 5 set and both WEB limits usable: narrows [lo, hi] to [245, 246] (inferred, same rule as the write clamp). */
static void intersect_web_limits(const reg_model_t *m, int64_t now_ms, int32_t *lo, int32_t *hi)
{
    uint16_t web;
    int32_t web_lo, web_hi;
    if (!read_fresh(m, REG_WEB, now_ms, &web) || !(web & WEB_SP_RESTRICT_BIT)) {
        return;
    }
    if (!read_limit(m, REG_SPL_W, now_ms, &web_lo) || !read_limit(m, REG_SPH_W, now_ms, &web_hi)) {
        return;
    }
    *lo = web_lo > *lo ? web_lo : *lo;
    *hi = web_hi < *hi ? web_hi : *hi;
}

/* Returns false (limits unusable) when 202/203 are unusable, 202 > 203 or the intersection is empty. */
static bool compute_limits(const reg_model_t *m, int64_t now_ms, int32_t *lo, int32_t *hi)
{
    if (!read_limit(m, REG_SPL, now_ms, lo) || !read_limit(m, REG_SPH, now_ms, hi) || *lo > *hi) {
        return false;
    }
    intersect_web_limits(m, now_ms, lo, hi);
    return *lo <= *hi;
}

/* Returns true when the limits were updated; otherwise they are kept. */
static bool map_limits(const reg_model_t *m, int64_t now_ms, map_thermostat_t *out)
{
    int32_t lo, hi;
    if (!compute_limits(m, now_ms, &lo, &hi)) {
        return false;
    }
    out->min_heat = out->min_cool = (int16_t)lo;
    out->max_heat = out->max_cool = (int16_t)hi;
    return true;
}

static bool setpoint_outside_limits(const map_thermostat_t *t)
{
    return t->heat_sp < t->min_heat || t->heat_sp > t->max_heat ||
           t->cool_sp < t->min_cool || t->cool_sp > t->max_cool;
}

/* 233 = 0 (seasonal auto): the active mode from STAT 104 when exactly one of its mode bits is set. */
static bool mode_from_stat(const reg_model_t *m, int64_t now_ms, uint8_t *mode)
{
    uint16_t stat;
    if (!read_fresh(m, REG_STAT, now_ms, &stat)) {
        return false;
    }
    bool heat = (stat & STAT_HEAT_BIT) != 0;
    bool cool = (stat & STAT_COOL_BIT) != 0;
    if (heat == cool) {
        return false;  /* both or neither: open wiki item, treated as unusable (provisional) */
    }
    *mode = heat ? MAP_MODE_HEAT : MAP_MODE_COOL;
    return true;
}

static void map_season(map_state_t *st, const reg_model_t *m, int64_t now_ms, uint16_t man, map_thermostat_t *out)
{
    out->season_auto_flag = false;
    if (man == MAN_WINTER || man == MAN_SUMMER) {
        st->last_season = (man == MAN_WINTER) ? MAP_MODE_HEAT : MAP_MODE_COOL;
        out->system_mode = st->last_season;
    } else if (!mode_from_stat(m, now_ms, &out->system_mode)) {  /* man == MAN_AUTO (range-checked by read_fresh) */
        out->system_mode = st->last_season;
        out->season_auto_flag = true;
    }
}

static void map_system_mode(map_state_t *st, const reg_model_t *m, int64_t now_ms, map_thermostat_t *out)
{
    uint16_t prg, man;
    if (!read_fresh(m, REG_PRG, now_ms, &prg)) {
        return;  /* keep */
    }
    if (prg & PRG_STBY_BIT) {
        out->system_mode = MAP_MODE_OFF;
        out->season_auto_flag = false;
        return;
    }
    if (!read_fresh(m, REG_MAN, now_ms, &man)) {
        return;  /* keep */
    }
    map_season(st, m, now_ms, man, out);
}

void mapping_init(map_state_t *st)
{
    map_thermostat_t *k = &st->kept;
    k->local_temp_valid = false;
    k->local_temp = 0;
    k->heat_sp = k->cool_sp = DEFAULT_SETPOINT_CENTI;
    k->sp_observed = false;
    k->min_heat = k->min_cool = MAP_ABS_MIN_CENTI;
    k->max_heat = k->max_cool = MAP_ABS_MAX_CENTI;
    k->system_mode = MAP_MODE_HEAT;  /* inferred default, architecture section 5 */
    k->season_auto_flag = false;
    k->limits_conflict = false;
    st->last_season = MAP_MODE_HEAT;
}

void mapping_thermostat(map_state_t *st, const reg_model_t *m, int64_t now_ms, map_thermostat_t *out)
{
    map_thermostat_t *t = &st->kept;
    map_local_temp(m, now_ms, t);
    map_setpoints(m, now_ms, t);
    bool limits_ok = map_limits(m, now_ms, t);
    map_system_mode(st, m, now_ms, t);
    t->limits_conflict = !limits_ok || setpoint_outside_limits(t);
    *out = *t;
}

static bool limits_differ(const map_thermostat_t *a, const map_thermostat_t *b)
{
    return a->min_heat != b->min_heat || a->max_heat != b->max_heat ||
           a->min_cool != b->min_cool || a->max_cool != b->max_cool;
}

static bool local_temp_differs(const map_thermostat_t *a, const map_thermostat_t *b)
{
    if (a->local_temp_valid != b->local_temp_valid) {
        return true;
    }
    return a->local_temp_valid && a->local_temp != b->local_temp;
}

uint32_t mapping_changed(const map_thermostat_t *last_pushed, const map_thermostat_t *cur, bool first)
{
    if (first) {
        return MAP_CH_ALL;
    }
    uint32_t bits = 0;
    if (limits_differ(last_pushed, cur)) {
        bits |= MAP_CH_LIMITS;
    }
    if (last_pushed->heat_sp != cur->heat_sp) {
        bits |= MAP_CH_HEAT_SP;
    }
    if (last_pushed->cool_sp != cur->cool_sp) {
        bits |= MAP_CH_COOL_SP;
    }
    if (local_temp_differs(last_pushed, cur)) {
        bits |= MAP_CH_LOCAL_TEMP;
    }
    if (last_pushed->system_mode != cur->system_mode) {
        bits |= MAP_CH_SYSTEM_MODE;
    }
    return bits;
}
