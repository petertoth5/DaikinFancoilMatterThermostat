/* reg_model.c - per-register value/age/availability store (pure C99). */

#include "reg_model.h"
#include "daikin_regs.h"
#include <string.h>

/* Entry index = table index of the register in daikin_regs, or -1. */
static int entry_index(uint16_t reg)
{
    const dr_reg_t *r = daikin_regs_find(reg);
    if (r == NULL) {
        return -1;
    }
    return (int)(r - daikin_regs_at(0));
}

void reg_model_reset(reg_model_t *m)
{
    memset(m->e, 0, sizeof(m->e));
    for (size_t i = 0; i < daikin_regs_count() && i < RM_REG_COUNT; i++) {
        m->e[i].reg = daikin_regs_at(i)->reg;
        m->e[i].avail = RM_UNKNOWN;
    }
}

void reg_model_init(reg_model_t *m, int64_t stale_fast_ms, int64_t stale_slow_ms)
{
    m->stale_fast_ms = stale_fast_ms;
    m->stale_slow_ms = stale_slow_ms;
    reg_model_reset(m);
}

int reg_model_set_ok(reg_model_t *m, uint16_t reg, uint16_t raw, int64_t now_ms)
{
    int i = entry_index(reg);
    if (i < 0) {
        return -1;
    }
    rm_entry_t *e = &m->e[i];
    e->avail = RM_OK;
    e->has_value = true;
    e->raw = raw;
    e->out_of_range = !daikin_regs_in_range(daikin_regs_find(reg), raw);
    e->t_ok_ms = now_ms;
    e->t_attempt_ms = now_ms;
    e->exc_code = 0;
    e->ok_count++;
    return 0;
}

int reg_model_set_unsupported(reg_model_t *m, uint16_t reg, uint8_t exc, int64_t now_ms)
{
    int i = entry_index(reg);
    if (i < 0) {
        return -1;
    }
    m->e[i].avail = RM_UNSUPPORTED;
    m->e[i].exc_code = exc;
    m->e[i].t_attempt_ms = now_ms;
    m->e[i].fail_count++;
    return 0;
}

int reg_model_set_timeout(reg_model_t *m, uint16_t reg, int64_t now_ms)
{
    int i = entry_index(reg);
    if (i < 0) {
        return -1;
    }
    m->e[i].avail = RM_TIMEOUT;
    m->e[i].exc_code = 0;
    m->e[i].t_attempt_ms = now_ms;
    m->e[i].fail_count++;
    return 0;
}

int reg_model_set_exception(reg_model_t *m, uint16_t reg, uint8_t exc, int64_t now_ms)
{
    int i = entry_index(reg);
    if (i < 0) {
        return -1;
    }
    m->e[i].avail = RM_TIMEOUT;
    m->e[i].exc_code = exc;
    m->e[i].t_attempt_ms = now_ms;
    m->e[i].fail_count++;
    return 0;
}

bool reg_model_get(const reg_model_t *m, uint16_t reg, rm_entry_t *out)
{
    int i = entry_index(reg);
    if (i < 0) {
        return false;
    }
    *out = m->e[i];
    return true;
}

bool reg_model_fresh(const reg_model_t *m, uint16_t reg, int64_t now_ms)
{
    int i = entry_index(reg);
    if (i < 0) {
        return false;
    }
    const rm_entry_t *e = &m->e[i];
    if (!e->has_value || e->avail == RM_UNSUPPORTED) {
        return false;
    }
    int64_t threshold = (daikin_regs_find(reg)->group == DR_GROUP_FAST) ? m->stale_fast_ms
                                                                       : m->stale_slow_ms;
    return now_ms - e->t_ok_ms <= threshold;
}
