#pragma once

/* write_guard.h - safety layer for the console-driven register writes of the write image
 * (spec 019; pure C99, no IDF, no hardware). Decides whether a write command may run, drives its
 * PRE -> WRITE -> VERIFY sequence one transaction per write_guard_step, and owns the arm session,
 * the frame budget, the failure latch and the restore set. The FC06 frame itself goes out through an
 * injected wg_write_fn; the bus_task glue (019b) is the only place that supplies a real one.
 *
 * Time is int64_t ms from the injected clock. All rules are the WR1..WR11 of task 019.
 * Every public function that takes a wg_events_t resets it first and fills it with the W/A log
 * events of that call; write_guard_format_event turns one event into its log line.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "poll_sched.h"
#include "reg_model.h"

/* ------------------------------------------------------------ constants (W6, W7, W8) */
#define WG_ARM_MIN_S        30
#define WG_ARM_DEFAULT_S    120
#define WG_ARM_MAX_S        600
#define WG_BUDGET_FRAMES    40      /* FC06 frames per arm session */
#define WG_PACING_MS        5000    /* between write commands, end of last transaction to start of PRE */
#define WG_MAX_FAILURES     3       /* per boot; then writes are latched until reboot */

/* ------------------------------------------------------------ commands */
typedef enum { WG_CMD_SP, WG_CMD_STANDBY, WG_CMD_FAN, WG_CMD_SEASON, WG_CMD_RESTORE } wg_cmd_type_t;

/* Fan codes are the raw bits 0-2 of register 201 (flag-registers: 000/001/010/011). */
enum { WG_FAN_AUTO = 0, WG_FAN_SILENT = 1, WG_FAN_NIGHT = 2, WG_FAN_MAX = 3 };

/* arg: SP raw tenths of a degree; STANDBY 1 = on, 0 = off; FAN a WG_FAN_* code;
 * SEASON the raw value of register 233 (0 auto, 3 winter, 5 summer); RESTORE unused. */
typedef struct { wg_cmd_type_t type; uint16_t arg; } wg_cmd_t;

/* ------------------------------------------------------------ events (WR11) */
typedef enum {
    WG_DISARM_USER, WG_DISARM_TIMEOUT, WG_DISARM_FAILURE, WG_DISARM_BUDGET, WG_DISARM_HEALTH,
    WG_DISARM_POLL_OFF, WG_DISARM_SILENCE, WG_DISARM_ADDR, WG_DISARM_LATCHED, WG_DISARM_LINK,
    WG_DISARM_WEB_FLAGS
} wg_reason_t;

typedef enum {
    WG_REFUSE_LATCHED, WG_REFUSE_HEALTH, WG_REFUSE_DISARMED, WG_REFUSE_BUSY, WG_REFUSE_PACING,
    WG_REFUSE_STALE, WG_REFUSE_WEB_FLAG, WG_REFUSE_LIMIT, WG_REFUSE_UNDEFINED_MODE
} wg_refuse_t;

/* What failed in a command; the log shows pre_/write_/verify_<RES>, mismatch or verify_lost. */
typedef enum { WG_FAIL_PRE, WG_FAIL_WRITE, WG_FAIL_VERIFY, WG_FAIL_MISMATCH, WG_FAIL_VERIFY_LOST } wg_fail_t;

typedef enum {
    WG_EV_ARMED, WG_EV_EXTENDED, WG_EV_RESTORE_SET, WG_EV_ARM_REFUSED, WG_EV_DISARMED, WG_EV_LATCHED,
    WG_EV_CMD, WG_EV_REFUSED, WG_EV_SKIP, WG_EV_ABORTED, WG_EV_DONE, WG_EV_FAIL
} wg_ev_kind_t;

typedef struct {
    wg_ev_kind_t kind;
    int64_t t_ms;
    uint16_t seconds;        /* ARMED, EXTENDED: seconds left */
    uint8_t budget;          /* ARMED, EXTENDED, DONE: frames left */
    wg_cmd_type_t cmd;       /* CMD, REFUSED */
    uint16_t arg;            /* CMD */
    wg_refuse_t refuse;      /* ARM_REFUSED, REFUSED */
    uint16_t detail;         /* REFUSED/ARM_REFUSED: stale register or web flag number; REFUSED pacing: ms left */
    uint16_t lo, hi;         /* REFUSED limit: allowed raw range */
    wg_reason_t reason;      /* DISARMED, ABORTED */
    uint16_t reg;            /* SKIP, ABORTED, DONE, FAIL */
    uint16_t old_raw, new_raw;   /* DONE old -> new; SKIP and FAIL mismatch: the value in old_raw */
    wg_fail_t fail;          /* FAIL */
    ps_res_t res;            /* FAIL pre_/write_/verify_<RES> */
    uint8_t failures;        /* FAIL: count after this failure */
    uint16_t restore_201, restore_233, restore_231;   /* RESTORE_SET */
} wg_event_t;

#define WG_EVENTS_MAX 8
typedef struct { wg_event_t e[WG_EVENTS_MAX]; size_t n; } wg_events_t;

/* ------------------------------------------------------------ state */
typedef enum { WG_PHASE_IDLE, WG_PHASE_PRE, WG_PHASE_WRITE, WG_PHASE_VERIFY } wg_phase_t;

/* Defined here for static allocation; fields are private by convention. */
typedef struct write_guard {
    bool armed;
    int64_t deadline_ms;
    uint8_t budget_left;
    uint8_t failures;
    bool latched;
    wg_reason_t last_disarm;
    bool restore_valid;
    uint16_t restore_201, restore_233, restore_231;
    bool restore_active;
    uint8_t restore_next;           /* index into the restore order 201, 233, 231 */
    bool have_cmd_end;
    int64_t cmd_end_ms;             /* end of the last write-sequence transaction */
    wg_phase_t phase;               /* the transaction the running command needs next */
    wg_cmd_type_t op_cmd;
    uint16_t op_reg, op_mask, op_target;
    uint16_t op_pre, op_new;
    uint8_t op_addr;   /* slave address of the PRE; WRITE and VERIFY go to it only */
    bool op_write_failed;
    ps_res_t op_write_res;
} write_guard_t;

typedef struct {
    bool armed;
    uint32_t left_s;                /* rounded up; 0 when disarmed */
    uint8_t budget_left;
    uint8_t failures;
    bool latched;
    bool restore_valid;
    uint16_t restore_201, restore_233, restore_231;
} wg_status_t;

/* The injected FC06: ps_res_t as for a read (OK = echo OK), *exc set for PS_RES_EXCEPTION. */
typedef ps_res_t (*wg_write_fn)(void *ctx, uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc);
typedef struct { ps_env_t ps; wg_write_fn write; void *write_ctx; } wg_env_t;

/* ------------------------------------------------------------ API */
void write_guard_init(write_guard_t *w);   /* disarmed, budget 40, no failures, no restore set */

/* Arm or extend (WR2). false: refused (event ARM_REFUSED) or seconds outside WG_ARM_MIN_S..WG_ARM_MAX_S
 * (no event; the console prints the usage). Re-arm never shortens the deadline and never resets the budget. */
bool write_guard_arm(write_guard_t *w, const poll_sched_t *s, const reg_model_t *m, uint32_t seconds,
                     int64_t now_ms, wg_events_t *ev);

/* Accept or refuse a command (WR5 step 1). true: accepted, the transactions follow in write_guard_step.
 * false: refused (event REFUSED), or an invalid arg (SP is range-checked as a limit; STANDBY > 1, FAN > 3,
 * SEASON not 0/3/5): no event, the console prints the usage. */
bool write_guard_command(write_guard_t *w, const poll_sched_t *s, const reg_model_t *m, const wg_cmd_t *c,
                         int64_t now_ms, wg_events_t *ev);

/* At most one transaction (*txn filled, true); otherwise delegates to poll_step (WR10) and returns its
 * result. *wait_until_ms is written only when false is returned: min(poll wait, deadline, next write op). */
bool write_guard_step(write_guard_t *w, poll_sched_t *s, reg_model_t *m, const wg_env_t *env,
                      ps_txn_t *txn, wg_events_t *ev, int64_t *wait_until_ms);

/* End the session for the given reason; no effect on a disarmed guard except WG_DISARM_ADDR, which
 * also clears the restore set. Never clears the failure latch. */
void write_guard_disarm(write_guard_t *w, wg_reason_t reason, int64_t now_ms, wg_events_t *ev);

/* WR10: armed, frames left, not latched, health ONLINE. */
bool write_guard_may_transmit(const write_guard_t *w, const poll_sched_t *s);

void write_guard_get_status(const write_guard_t *w, int64_t now_ms, wg_status_t *out);

/* Format one event as its W or A log line (no trailing newline). Returns the length; 0 if n is
 * too small (buf is then NUL-terminated if n > 0) or the event is malformed. */
size_t write_guard_format_event(char *buf, size_t n, const wg_event_t *e);
