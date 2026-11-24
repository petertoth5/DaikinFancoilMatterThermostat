#pragma once

/* poll_sched.h - pure scheduling layer between bus_task and mb_link
 * (pure C99, no IDF). Decides which FC03 read goes on the bus and when, tracks
 * link health (D21), statistics, and offers poll_step as the test/bus_task seam.
 * Time is int64_t ms from an injected clock (no wrap handling).
 */

#include <stdbool.h>
#include <stdint.h>
#include "reg_model.h"

typedef enum { PS_STOPPED, PS_ONLINE, PS_DEGRADED, PS_OFFLINE, PS_LATCHED } ps_health_t;
typedef enum { PS_RES_OK, PS_RES_TIMEOUT, PS_RES_EXCEPTION, PS_RES_BAD_FRAME, PS_RES_ERROR } ps_res_t;
typedef enum { PS_KIND_POLL, PS_KIND_RETRY, PS_KIND_HEALTH, PS_KIND_SWEEP, PS_KIND_MANUAL } ps_kind_t;

typedef struct { uint8_t addr; uint16_t reg, qty; ps_kind_t kind; } ps_req_t;
typedef struct {
    ps_req_t req; ps_res_t res; uint8_t exc_code; uint16_t val[4];
    int64_t t_start_ms, t_end_ms;
} ps_txn_t;
typedef struct {
    uint8_t addr; uint32_t gap_ms, fast_ms, slow_ms, offline_probe_ms;
    uint8_t retries, offline_after;
} ps_config_t;

#define PS_LAT_BUCKET_MS 10
#define PS_LAT_BUCKETS   201          /* 0..2000 ms */
typedef struct {
    uint32_t txns, ok, timeout, exception, bad_frame, error, retries, health_probes, sweeps, manual,
             offline_entries;
    uint32_t lat_min_ms, lat_max_ms, lat_hist[PS_LAT_BUCKETS];   /* answered transactions only (OK or EXCEPTION) */
} ps_stats_t;

/* Defined here for static allocation; fields are private by convention. */
typedef struct poll_sched {
    ps_config_t cfg;
    ps_health_t health;
    bool have_last;                 /* a transaction has completed since init */
    int64_t last_end_ms;
    int64_t due_ms[RM_REG_COUNT];   /* per table index; used for polled registers only */
    bool unsupported[RM_REG_COUNT];
    uint32_t failures;              /* consecutive counted failures (rule 7) */
    bool retry_pending;
    uint16_t retry_reg;
    bool manual_pending;
    uint16_t manual_reg, manual_qty;
    bool sweep_active;
    uint8_t sweep_idx;              /* table index of the next sweep read */
    int64_t next_health_ms;
    ps_stats_t stats;
} poll_sched_t;

int  poll_sched_init(poll_sched_t *s, const ps_config_t *cfg); /* -1 outside the 001 Kconfig ranges or slow_ms < fast_ms; state STOPPED */
void poll_sched_start(poll_sched_t *s, int64_t now_ms);        /* STOPPED -> ONLINE, counter 0, all polled regs due now; no effect in any other state */
void poll_sched_stop(poll_sched_t *s);                         /* -> STOPPED unless LATCHED; pending RETRY/SWEEP dropped */
void poll_sched_latch(poll_sched_t *s);                        /* -> LATCHED, permanent until re-init (reboot) */
int  poll_sched_set_addr(poll_sched_t *s, uint8_t addr, int64_t now_ms); /* 1..247 else -1; clears unsupported marks, counter, pending work; DEGRADED/OFFLINE -> ONLINE, all polled regs due now; STOPPED/LATCHED kept */
int  poll_sched_request_manual(poll_sched_t *s, uint16_t reg, uint16_t qty); /* -1: LATCHED, !daikin_regs_read_allowed, or one pending */
int  poll_sched_request_sweep(poll_sched_t *s);                /* -1: LATCHED or a sweep running (from request until its last read is issued) */
bool poll_sched_next(poll_sched_t *s, int64_t now_ms, ps_req_t *out, int64_t *wait_until_ms);
void poll_sched_complete(poll_sched_t *s, const ps_txn_t *t);
ps_health_t poll_sched_health(const poll_sched_t *s);
bool poll_sched_is_unsupported(const poll_sched_t *s, uint16_t reg);
void poll_sched_get_stats(const poll_sched_t *s, ps_stats_t *out);
uint32_t poll_sched_lat_median_ms(const ps_stats_t *st);       /* upper edge of the median bucket; 0 if no samples */

typedef ps_res_t (*ps_link_fn)(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc);
typedef int64_t  (*ps_clock_fn)(void *ctx);
typedef struct { ps_link_fn link; void *link_ctx; ps_clock_fn clock; void *clock_ctx; } ps_env_t;
/* true if a transaction was performed (*txn filled); false: nothing before *wait_until_ms (INT64_MAX = idle).
 * *wait_until_ms is written only when false is returned. */
bool poll_step(poll_sched_t *s, reg_model_t *m, const ps_env_t *env, ps_txn_t *txn, int64_t *wait_until_ms);
