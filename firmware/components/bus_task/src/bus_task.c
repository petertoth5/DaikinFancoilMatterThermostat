/* bus_task.c - boot silence check, poll loop over poll_step, snapshot publishing (spec 006).
 * All output goes through the log_out ring buffer; this task never prints directly. */

#include "bus_task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "log_out.h"
#include "txn_log.h"

/* Write image only (spec 019b): the guard hooks and the header suffix. In the read image they vanish. */
#if CONFIG_DMT_BUS_WRITE
#include "bus_write.h"
#define HEADER_WRITE_SUFFIX     " writes=console"
#define disarm_writes(reason)   bus_write_disarm((reason), now_ms())
#else
#define HEADER_WRITE_SUFFIX     ""
#define disarm_writes(reason)   ((void)0)
#endif

#define BUS_TASK_STACK_BYTES   4096
#define BUS_TASK_PRIORITY      10
#define BUS_TASK_CORE          1
#define COMMAND_QUEUE_DEPTH    8
#define MAX_IDLE_WAIT_MS       50       /* longest queue wait, so a due step is noticed (spec 006) */
#define STALE_PERIOD_FACTOR    3        /* reg_model stale threshold = 3 x poll period (spec 006) */
#define MS_PER_S               1000

typedef struct {
    bt_cmd_type_t type;
    uint32_t a, b;
} bt_cmd_t;

typedef struct {
    bus_task_config_t cfg;
    poll_sched_t sched;
    reg_model_t model;
    bool link_ready;            /* mb_link_init succeeded after a clean silence check; mb_link_read allowed */
    bool auto_start_cancelled;  /* a queued `mb poll off` or `mb silence` ran during the boot drain */
    bool log_on;
    bool silence_running;       /* a silence check is in progress (boot or mb silence) */
    bool link_closed;           /* port fault observed since the last successful mb_link_init */
    uint32_t faults_seen;       /* last mb_link_counters_t.faults value observed */
    uint32_t boot_activity;
    uint32_t log_dropped;
    ps_health_t reported_health;
} bt_state_t;

static bt_state_t s;            /* owned by the bus_task task */
static bus_snapshot_t s_snap;   /* guarded by s_mutex */
static StaticSemaphore_t s_mutex_struct;
static SemaphoreHandle_t s_mutex;
static StaticQueue_t s_queue_struct;
static uint8_t s_queue_storage[COMMAND_QUEUE_DEPTH * sizeof(bt_cmd_t)];
static QueueHandle_t s_queue;
static StaticTask_t s_task_tcb;
static StackType_t s_task_stack[BUS_TASK_STACK_BYTES / sizeof(StackType_t)];

static int64_t now_ms(void)
{
    return esp_timer_get_time() / MS_PER_S;
}

static int64_t clock_ms(void *ctx)
{
    (void)ctx;
    return now_ms();
}

/* ---- output (ring buffer only) ---- */

static void emit_line(const char *line)
{
    if (!log_out_emit(line, strlen(line))) {
        s.log_dropped++;
    }
}

static void emitf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void emitf(const char *fmt, ...)
{
    char line[LOG_OUT_MAX_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    emit_line(line);
}

static void emit_header(void)
{
    emitf("# dmt-probe v1 addr=%u baud=%lu fw=%s" HEADER_WRITE_SUFFIX, (unsigned)s.sched.cfg.addr,
          (unsigned long)s.cfg.link.port.baud, esp_app_get_description()->version);
}

static void emit_link_error(const char *call, esp_err_t err)
{
    emitf("# mb_link: %s failed: %s", call, esp_err_to_name(err));
}

static void emit_txn(const ps_txn_t *t)
{
    bool always = t->req.kind == PS_KIND_MANUAL || t->req.kind == PS_KIND_SWEEP || t->req.kind == PS_KIND_PRE ||
                  t->req.kind == PS_KIND_WRITE || t->req.kind == PS_KIND_VERIFY;
    if (!always && !s.log_on) {
        return;
    }
    char line[LOG_OUT_MAX_LINE];
    size_t len = txn_log_format_txn(line, sizeof(line), t);
    if (len == 0 || !log_out_emit(line, len)) {
        s.log_dropped++;
    }
}

static void report_health(bool force)
{
    ps_health_t h = poll_sched_health(&s.sched);
    if (!force && h == s.reported_health) {
        return;
    }
    s.reported_health = h;
    char line[LOG_OUT_MAX_LINE];
    size_t len = txn_log_format_health(line, sizeof(line), now_ms(), h);
    if (len == 0 || !log_out_emit(line, len)) {
        s.log_dropped++;
    }
}

/* ---- snapshot (mutex scope: copy only) ---- */

static void publish_snapshot(void)
{
    mb_link_counters_t counters;
    mb_link_get_counters(&counters);
    uint32_t stack_hwm = (uint32_t)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t));

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_snap.model = s.model;
    poll_sched_get_stats(&s.sched, &s_snap.stats);
    s_snap.health = poll_sched_health(&s.sched);
    s_snap.addr = s.sched.cfg.addr;
    s_snap.boot_activity = s.boot_activity;
    s_snap.log_on = s.log_on;
    s_snap.log_dropped = s.log_dropped;
    s_snap.stack_hwm = stack_hwm;
    s_snap.link_ctr = counters;
    s_snap.link_closed = s.link_closed;
    s_snap.silence_running = s.silence_running;
#if CONFIG_DMT_BUS_WRITE
    bus_write_get_status(now_ms(), &s_snap.write);
#endif
    xSemaphoreGive(s_mutex);
}

void bus_task_get_snapshot(bus_snapshot_t *out)
{
    if (s_mutex == NULL) {
        memset(out, 0, sizeof(*out));
        return;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_snap;
    xSemaphoreGive(s_mutex);
}

/* ---- link adapter for poll_step ---- */

ps_res_t bus_task_map_result(mb_link_res_t r)
{
    switch (r) {
    case MBL_OK:        return PS_RES_OK;
    case MBL_TIMEOUT:   return PS_RES_TIMEOUT;
    case MBL_EXCEPTION: return PS_RES_EXCEPTION;
    case MBL_BAD_FRAME: return PS_RES_BAD_FRAME;
    case MBL_ERROR:
    case MBL_REFUSED:
    default:            return PS_RES_ERROR;
    }
}

/* Only reached through poll_step, and poll_step runs only while s.link_ready. */
static ps_res_t link_read(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc)
{
    (void)ctx;
    return bus_task_map_result(mb_link_read(addr, reg, qty, val, exc));
}

/* ---- boot and on-demand silence check ---- */

static bool is_latched(void)
{
    return poll_sched_health(&s.sched) == PS_LATCHED;
}

static void latch(void)
{
    disarm_writes(WG_DISARM_LATCHED);
    s.link_ready = false;
    poll_sched_latch(&s.sched);
}

/* Silence check, then mb_link_init only after a clean result. Any error or activity latches.
 * The caller has set silence_running and published the snapshot before this call. */
static bool silence_check_then_open(uint32_t ms, uint32_t *activity)
{
    *activity = 0;
    emitf("# silence check: started %lu s", (unsigned long)(ms / MS_PER_S));
    esp_err_t err = mb_link_silence_check(&s.cfg.link, ms, activity);
    s.silence_running = false;
    if (err != ESP_OK) {
        emit_link_error("silence_check", err);
        latch();
        return false;
    }
    if (*activity > 0) {
        emitf("# silence check: activity %lu, latched until reboot", (unsigned long)*activity);
        latch();
        return false;
    }
    err = mb_link_init(&s.cfg.link);
    if (err != ESP_OK) {
        emit_link_error("init", err);
        latch();
        return false;
    }
    s.link_ready = true;
    s.link_closed = false;
    return true;
}

/* ---- commands ---- */

static void command_poll_on(void)
{
    if (is_latched()) {
        emit_line("# mb poll on refused: latched until reboot");
        return;
    }
    poll_sched_start(&s.sched, now_ms());
}

static void command_poll_off(void)
{
    s.auto_start_cancelled = true;
    disarm_writes(WG_DISARM_POLL_OFF);     /* before the stop: a pending VERIFY must not go out */
    poll_sched_stop(&s.sched);
}

static void command_read(uint32_t reg, uint32_t qty)
{
    if (poll_sched_request_manual(&s.sched, (uint16_t)reg, (uint16_t)qty) < 0) {
        emit_line("# mb read refused: latched, register range not allowed, or a read is pending");
    }
}

static void command_probe(void)
{
    if (poll_sched_request_sweep(&s.sched) < 0) {
        emit_line("# mb probe refused: latched or a sweep is running");
    }
}

static void command_addr(uint32_t addr)
{
    disarm_writes(WG_DISARM_ADDR);         /* before the address changes: a VERIFY would read another slave */
    if (poll_sched_set_addr(&s.sched, (uint8_t)addr, now_ms()) < 0) {
        emit_line("# mb addr refused: outside 1..247");
        return;
    }
    reg_model_reset(&s.model);
    emit_header();
}

static void command_log(bool on)
{
    s.log_on = on;
    if (on) {
        emit_header();
    }
}

static void command_silence(uint32_t seconds)
{
    if (is_latched()) {
        emit_line(s.cfg.hold_err != ESP_OK ? "# mb silence refused: hold_receive failed, reboot only"
                                           : "# mb silence refused: latched, reboot only");
        return;
    }
    s.auto_start_cancelled = true;
    disarm_writes(WG_DISARM_SILENCE);      /* before the stop and the link close */
    poll_sched_cancel_manual(&s.sched);     /* a pending read must not go out after the check */
    poll_sched_stop(&s.sched);
    s.link_ready = false;
    s.silence_running = true;
    publish_snapshot();
    esp_err_t err = mb_link_deinit();
    if (err != ESP_OK) {
        emit_link_error("deinit", err);
        s.silence_running = false;
        latch();
        publish_snapshot();
        return;
    }
    uint32_t activity;
    if (silence_check_then_open(seconds * MS_PER_S, &activity)) {
        emitf("# silence check %lus ok, polling off: mb poll on to start", (unsigned long)seconds);
    }
    publish_snapshot();
}

static void handle_command(const bt_cmd_t *c)
{
    switch (c->type) {
    case BT_CMD_POLL_ON:  command_poll_on(); break;
    case BT_CMD_POLL_OFF: command_poll_off(); break;
    case BT_CMD_READ:     command_read(c->a, c->b); break;
    case BT_CMD_PROBE:    command_probe(); break;
    case BT_CMD_ADDR:     command_addr(c->a); break;
    case BT_CMD_SILENCE:  command_silence(c->a); break;
    case BT_CMD_LOG:      command_log(c->a != 0); break;
#if CONFIG_DMT_BUS_WRITE
    case BT_CMD_WRITE_ARM:    bus_write_arm(c->a, now_ms()); break;
    case BT_CMD_WRITE_DISARM: bus_write_disarm(WG_DISARM_USER, now_ms()); break;
    case BT_CMD_WRITE:        bus_write_command((wg_cmd_type_t)c->a, (uint16_t)c->b, now_ms()); break;
#endif
    }
}

/* Commands queued during the silence check run before the first poll_step (spec 006). */
static void drain_commands(void)
{
    bt_cmd_t c;
    while (xQueueReceive(s_queue, &c, 0) == pdTRUE) {
        handle_command(&c);
    }
}

/* Exactly one hint per boot path, chosen from the real state after the drain (rule M1). */
static void emit_state_hint(void)
{
    if (is_latched()) {
        emit_line("# latched until reboot: no transmission");
        return;
    }
    if (poll_sched_health(&s.sched) == PS_STOPPED) {
        emit_line("# silence ok, polling off: mb poll on to start");
        return;
    }
    emit_line("# silence ok, polling on: mb poll off to stop");
}

static void boot_sequence(void)
{
    emit_header();
    s.silence_running = s.cfg.hold_err == ESP_OK;   /* no check runs after a failed hold */
    publish_snapshot();                             /* valid values during the boot window (rule M2) */
    if (s.cfg.hold_err != ESP_OK) {
        /* Fatal: no silence check, no init, no polling until reboot. */
        emit_link_error("hold_receive", s.cfg.hold_err);
        latch();
    } else if (silence_check_then_open(s.cfg.boot_silence_ms, &s.boot_activity)) {
        publish_snapshot();
        drain_commands();
        if (s.link_ready && !s.auto_start_cancelled) {
            poll_sched_start(&s.sched, now_ms());
        }
    }
    emit_state_hint();
    report_health(true);
    publish_snapshot();
}

/* Rule M5: only an observed increase of the fault counter closes the link (mb_link.h contract). */
static void track_link_faults(void)
{
    mb_link_counters_t counters;
    mb_link_get_counters(&counters);
    if (!counters.measured || counters.faults <= s.faults_seen) {
        return;
    }
    s.faults_seen = counters.faults;
    disarm_writes(WG_DISARM_LINK);
    if (s.link_closed) {
        return;
    }
    s.link_closed = true;
    emit_line("# mb_link: port fault, link closed (mb silence or reboot to recover)");
}

/* ---- task ---- */

/* One bus transaction: the write guard in the write image, plain poll_step otherwise. */
static bool step_bus(const ps_env_t *env, ps_txn_t *txn, int64_t *wait_until_ms)
{
#if CONFIG_DMT_BUS_WRITE
    return bus_write_step(env, txn, wait_until_ms);
#else
    return poll_step(&s.sched, &s.model, env, txn, wait_until_ms);
#endif
}

/* The W and A lines of the step, printed after its T line. true: there were some. */
static bool emit_write_events(void)
{
#if CONFIG_DMT_BUS_WRITE
    return bus_write_emit_events();
#else
    return false;
#endif
}

static TickType_t idle_wait_ticks(int64_t wait_until_ms)
{
    int64_t wait_ms = wait_until_ms - now_ms();
    if (wait_ms <= 0) {
        return 0;
    }
    if (wait_ms > MAX_IDLE_WAIT_MS) {
        wait_ms = MAX_IDLE_WAIT_MS;
    }
    return (TickType_t)((wait_ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);   /* round up: no early wake spin */
}

static void bus_task_main(void *arg)
{
    (void)arg;
    boot_sequence();

    const ps_env_t env = { .link = link_read, .link_ctx = NULL, .clock = clock_ms, .clock_ctx = NULL };
    int64_t wait_until_ms = 0;
    for (;;) {
        bt_cmd_t cmd;
        if (xQueueReceive(s_queue, &cmd, idle_wait_ticks(wait_until_ms)) == pdTRUE) {
            handle_command(&cmd);
            report_health(false);
            publish_snapshot();
        }
        if (!s.link_ready) {
            wait_until_ms = INT64_MAX;
            continue;
        }
        ps_txn_t txn;
        bool done = step_bus(&env, &txn, &wait_until_ms);
        if (done) {
            emit_txn(&txn);
        }
        bool events = emit_write_events();
        if (done) {
            track_link_faults();
        }
        if (done || events) {
            report_health(false);
            publish_snapshot();
        }
    }
}

/* ---- public API ---- */

esp_err_t bus_task_start(const bus_task_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_queue != NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    s.cfg = *cfg;
    if (poll_sched_init(&s.sched, &cfg->sched) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    reg_model_init(&s.model, (int64_t)cfg->sched.fast_ms * STALE_PERIOD_FACTOR,
                   (int64_t)cfg->sched.slow_ms * STALE_PERIOD_FACTOR);
#if CONFIG_DMT_BUS_WRITE
    esp_err_t write_err = bus_write_init(&s.sched, &s.model, &cfg->link, emit_line);
    if (write_err != ESP_OK) {
        return write_err;
    }
#endif
    s.log_on = true;    /* default on from power-up (spec 006) */
    s.reported_health = PS_STOPPED;

    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_struct);
    s_queue = xQueueCreateStatic(COMMAND_QUEUE_DEPTH, sizeof(bt_cmd_t), s_queue_storage, &s_queue_struct);
    esp_err_t err = log_out_start();
    if (err != ESP_OK) {
        return err;
    }
    TaskHandle_t task = xTaskCreateStaticPinnedToCore(bus_task_main, "bus_task",
                                                      sizeof(s_task_stack) / sizeof(StackType_t), NULL,
                                                      BUS_TASK_PRIORITY, s_task_stack, &s_task_tcb, BUS_TASK_CORE);
    return task != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bus_task_command(bt_cmd_type_t type, uint32_t a, uint32_t b)
{
    if (s_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    const bt_cmd_t c = { .type = type, .a = a, .b = b };
    return xQueueSend(s_queue, &c, 0) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
}
