#pragma once

/* reg_model.h - last known value, age and availability per documented register
 * (pure C99, no IDF, no locking; the snapshot mutex lives in bus_task).
 * Time is int64_t ms from an injected clock.
 */

#include <stdbool.h>
#include <stdint.h>

/* Number of documented registers in daikin_regs (one entry each, table order). */
#define RM_REG_COUNT 32

typedef enum { RM_UNKNOWN, RM_OK, RM_UNSUPPORTED, RM_TIMEOUT } rm_avail_t;

typedef struct {
    uint16_t reg;
    rm_avail_t avail;
    bool has_value;
    uint16_t raw;
    bool out_of_range;          /* !daikin_regs_in_range() of the last OK value */
    int64_t t_ok_ms, t_attempt_ms;
    uint8_t exc_code;
    uint32_t ok_count, fail_count;
} rm_entry_t;

typedef struct {
    rm_entry_t e[RM_REG_COUNT];
    int64_t stale_fast_ms, stale_slow_ms;
} reg_model_t;

void reg_model_init(reg_model_t *m, int64_t stale_fast_ms, int64_t stale_slow_ms);
void reg_model_reset(reg_model_t *m);   /* back to init, thresholds kept */

/* Return 0, or -1 if reg is not documented. Failures keep raw and t_ok_ms. */
int reg_model_set_ok(reg_model_t *m, uint16_t reg, uint16_t raw, int64_t now_ms);
int reg_model_set_unsupported(reg_model_t *m, uint16_t reg, uint8_t exc, int64_t now_ms);
int reg_model_set_timeout(reg_model_t *m, uint16_t reg, int64_t now_ms);
/* Failed read answered by an exception other than 02: avail TIMEOUT, exc_code = exc. */
int reg_model_set_exception(reg_model_t *m, uint16_t reg, uint8_t exc, int64_t now_ms);
/* exc_code is the code of the last attempt: set_ok and set_timeout clear it to 0. */

/* false if reg is not documented. */
bool reg_model_get(const reg_model_t *m, uint16_t reg, rm_entry_t *out);

/* fresh = has_value && avail != UNSUPPORTED && now - t_ok <= stale threshold of
 * the register's group (SLOW threshold for group NONE). */
bool reg_model_fresh(const reg_model_t *m, uint16_t reg, int64_t now_ms);
