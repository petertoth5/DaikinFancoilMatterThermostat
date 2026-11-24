#pragma once

/* bus_task.h - the single owner of mb_link: boot silence check, poll loop, snapshots (spec 006).
 * Only the bus_task task calls mb_link_* (after app_main's first hold call). */

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "mb_link.h"
#include "poll_sched.h"
#include "reg_model.h"

typedef struct {
    mb_link_config_t link;
    ps_config_t sched;
    uint32_t boot_silence_ms;
    esp_err_t hold_err;     /* result of app_main's mb_link_hold_receive(); not ESP_OK = fatal until reboot */
} bus_task_config_t;

typedef struct {
    reg_model_t model;
    ps_stats_t stats;
    ps_health_t health;
    uint8_t addr;
    uint32_t boot_activity;
    bool log_on;
    uint32_t log_dropped;
    uint32_t stack_hwm;     /* bytes */
    mb_link_counters_t link_ctr;
    bool link_closed;       /* a port fault was observed since the last successful mb_link_init (rule M5) */
    bool silence_running;   /* a silence check (boot or mb silence) is in progress (rule M2) */
} bus_snapshot_t;

typedef enum {
    BT_CMD_POLL_ON, BT_CMD_POLL_OFF, BT_CMD_READ, BT_CMD_PROBE, BT_CMD_ADDR, BT_CMD_SILENCE, BT_CMD_LOG
} bt_cmd_type_t;

/* Copies cfg, creates the queue, mutex, log_out task and the bus_task task. Call once. */
esp_err_t bus_task_start(const bus_task_config_t *cfg);

/* Copy under mutex. All zero before bus_task_start. */
void bus_task_get_snapshot(bus_snapshot_t *out);

/* Queued, executed by bus_task. ESP_ERR_TIMEOUT if the queue is full.
 * a, b: READ reg, qty; ADDR address; SILENCE seconds; LOG 1 = on, 0 = off; others unused. */
esp_err_t bus_task_command(bt_cmd_type_t type, uint32_t a, uint32_t b);

/* Registers the `mb` console command (call after the REPL is created, before it starts).
 * Keeps a copy of cfg for `mb status`. */
esp_err_t mb_console_register(const bus_task_config_t *cfg);
