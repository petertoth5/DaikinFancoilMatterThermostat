#pragma once

/* bus_write.h - write-image glue between bus_task and write_guard (spec 019b). bus_task context only:
 * these functions are not thread-safe. The event lines of every call go out through the emit function. */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "mb_link.h"
#include "poll_sched.h"
#include "reg_model.h"
#include "write_guard.h"

typedef void (*bus_write_emit_fn)(const char *line);

/* mb_link_res_t to ps_res_t; defined in bus_task.c and shared with the FC06 adapter. */
ps_res_t bus_task_map_result(mb_link_res_t r);

/* sched and model are the bus_task's own; link is the config given to mb_link_init. Call once, before
 * the task starts. */
esp_err_t bus_write_init(poll_sched_t *sched, reg_model_t *model, const mb_link_config_t *link,
                         bus_write_emit_fn emit);

/* Replaces poll_step: at most one transaction (true, *txn filled); the events of the step wait for
 * bus_write_emit_events so that the T line of the transaction is printed first. */
bool bus_write_step(const ps_env_t *env, ps_txn_t *txn, int64_t *wait_until_ms);
/* Prints the events of the last bus_write_step; true if there were any. */
bool bus_write_emit_events(void);

void bus_write_arm(uint32_t seconds, int64_t now_ms);
void bus_write_command(wg_cmd_type_t type, uint16_t arg, int64_t now_ms);
/* Safe on a disarmed guard (a VERIFY may be pending). */
void bus_write_disarm(wg_reason_t reason, int64_t now_ms);

void bus_write_get_status(int64_t now_ms, wg_status_t *out);
