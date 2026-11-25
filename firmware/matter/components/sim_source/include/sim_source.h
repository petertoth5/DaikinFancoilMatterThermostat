#pragma once

/* sim_source.h - simulated register source for phase 2a (task 016; pure C99, no IDF, time injected).
 * Phase 2b replaces it by the bus snapshot; the model stays a reg_model_t. */

#include <stdint.h>

#include "reg_model.h"

typedef enum { SIM_LIVE, SIM_STALE, SIM_OFFLINE } sim_state_t;

typedef struct {
    reg_model_t model;
    sim_state_t state;
    struct {
        bool has_value;   /* false: never set, or made unsupported */
        uint16_t raw;
    } target[RM_REG_COUNT];  /* value the simulated unit reports, same index as model.e */
} sim_t;

/* reg_model_init(15000, 180000), seeds the demo registers and applies them (LIVE). */
void sim_init(sim_t *s, int64_t now_ms);

/* LIVE: re-sets every register with a value OK at now_ms. OFFLINE: sets them to timeout. STALE: nothing. */
void sim_tick(sim_t *s, int64_t now_ms);

/* New value of a register: visible at once when LIVE, otherwise on return to LIVE. -1 if undocumented. */
int sim_set(sim_t *s, uint16_t reg, uint16_t raw, int64_t now_ms);

/* The unit answers exception 02 for reg until sim_set() gives it a value again. -1 if undocumented. */
int sim_unsupported(sim_t *s, uint16_t reg, int64_t now_ms);

void sim_set_state(sim_t *s, sim_state_t st, int64_t now_ms);

const reg_model_t *sim_model(const sim_t *s);
