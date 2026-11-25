/* sim_source.c - simulated register source (task 016). */

#include "sim_source.h"

#include "daikin_regs.h"

/* Same thresholds as the bus poller: 3 x 5 s fast period, 3 x 60 s slow period (architecture section 3). */
#define SIM_STALE_FAST_MS 15000
#define SIM_STALE_SLOW_MS 180000

#define EXC_ILLEGAL_FUNCTION 0x02  /* "unsupported register" answer, see reg_model_set_unsupported */

/* Demo values inside the p4 ranges (inferred): 000 T1 21.5, 008 SP 20.0, 201 PRG 0, 202 SPL 16.0,
 * 203 SPH 28.0, 231 SP 20.0, 233 Man 3 (winter). 104/245/246/247 stay unset like unavailable starred registers. */
static const struct { uint16_t reg; uint16_t raw; } k_seeds[] = {
    {0, 215}, {8, 200}, {201, 0}, {202, 160}, {203, 280}, {231, 200}, {233, 3},
};

/* Table index of a documented register, -1 otherwise (same indexing as reg_model entries). */
static int index_of(uint16_t reg)
{
    const dr_reg_t *r = daikin_regs_find(reg);
    return r ? (int)(r - daikin_regs_at(0)) : -1;
}

static void apply_target(sim_t *s, int i, int64_t now_ms)
{
    if (!s->target[i].has_value) {
        return;
    }
    uint16_t reg = s->model.e[i].reg;
    if (s->state == SIM_LIVE) {
        reg_model_set_ok(&s->model, reg, s->target[i].raw, now_ms);
    } else if (s->state == SIM_OFFLINE) {
        reg_model_set_timeout(&s->model, reg, now_ms);
    }
}

void sim_init(sim_t *s, int64_t now_ms)
{
    reg_model_init(&s->model, SIM_STALE_FAST_MS, SIM_STALE_SLOW_MS);
    s->state = SIM_LIVE;
    for (int i = 0; i < RM_REG_COUNT; i++) {
        s->target[i].has_value = false;
        s->target[i].raw = 0;
    }
    for (size_t k = 0; k < sizeof(k_seeds) / sizeof(k_seeds[0]); k++) {
        sim_set(s, k_seeds[k].reg, k_seeds[k].raw, now_ms);
    }
}

void sim_tick(sim_t *s, int64_t now_ms)
{
    for (int i = 0; i < RM_REG_COUNT; i++) {
        apply_target(s, i, now_ms);
    }
}

int sim_set(sim_t *s, uint16_t reg, uint16_t raw, int64_t now_ms)
{
    int i = index_of(reg);
    if (i < 0) {
        return -1;
    }
    s->target[i].has_value = true;
    s->target[i].raw = raw;
    if (s->state == SIM_LIVE) {
        apply_target(s, i, now_ms);
    }
    return 0;
}

int sim_unsupported(sim_t *s, uint16_t reg, int64_t now_ms)
{
    int i = index_of(reg);
    if (i < 0) {
        return -1;
    }
    s->target[i].has_value = false;
    return reg_model_set_unsupported(&s->model, reg, EXC_ILLEGAL_FUNCTION, now_ms);
}

void sim_set_state(sim_t *s, sim_state_t st, int64_t now_ms)
{
    s->state = st;
    sim_tick(s, now_ms);
}

const reg_model_t *sim_model(const sim_t *s)
{
    return &s->model;
}
