/* bus_write.c - owns the write_guard of the write image and is the only caller of mb_link_write
 * (spec 019b, WR10). Runs in the bus_task task only. */

#include "bus_write.h"
#include "log_out.h"
#include "mb_link_write.h"

static struct {
    write_guard_t guard;
    wg_events_t events;     /* static: bus_task's stack is small */
    poll_sched_t *sched;
    reg_model_t *model;
    bus_write_emit_fn emit;
} s_bw;

/* The only FC06 call site. write_guard checked the same rule one statement ago; the repeat is the
 * defence that no path leads to the bus while disarmed or latched. */
static ps_res_t link_write(void *ctx, uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc)
{
    (void)ctx;
    if (!write_guard_may_transmit(&s_bw.guard, s_bw.sched)) {
        return PS_RES_ERROR;
    }
    return bus_task_map_result(mb_link_write(addr, reg, value, exc));
}

esp_err_t bus_write_init(poll_sched_t *sched, reg_model_t *model, const mb_link_config_t *link,
                         bus_write_emit_fn emit)
{
    s_bw.sched = sched;
    s_bw.model = model;
    s_bw.emit = emit;
    write_guard_init(&s_bw.guard);
    return mb_link_write_configure(link);
}

bool bus_write_emit_events(void)
{
    char line[LOG_OUT_MAX_LINE];
    for (size_t i = 0; i < s_bw.events.n; i++) {
        if (write_guard_format_event(line, sizeof(line), &s_bw.events.e[i]) > 0) {
            s_bw.emit(line);
        }
    }
    bool any = s_bw.events.n > 0;
    s_bw.events.n = 0;
    return any;
}

bool bus_write_step(const ps_env_t *env, ps_txn_t *txn, int64_t *wait_until_ms)
{
    const wg_env_t wg_env = { .ps = *env, .write = link_write, .write_ctx = NULL };
    return write_guard_step(&s_bw.guard, s_bw.sched, s_bw.model, &wg_env, txn, &s_bw.events, wait_until_ms);
}

void bus_write_arm(uint32_t seconds, int64_t now_ms)
{
    write_guard_arm(&s_bw.guard, s_bw.sched, s_bw.model, seconds, now_ms, &s_bw.events);
    bus_write_emit_events();
}

void bus_write_command(wg_cmd_type_t type, uint16_t arg, int64_t now_ms)
{
    const wg_cmd_t cmd = { .type = type, .arg = arg };
    write_guard_command(&s_bw.guard, s_bw.sched, s_bw.model, &cmd, now_ms, &s_bw.events);
    bus_write_emit_events();
}

void bus_write_disarm(wg_reason_t reason, int64_t now_ms)
{
    write_guard_disarm(&s_bw.guard, reason, now_ms, &s_bw.events);
    bus_write_emit_events();
}

void bus_write_get_status(int64_t now_ms, wg_status_t *out)
{
    write_guard_get_status(&s_bw.guard, now_ms, out);
}
