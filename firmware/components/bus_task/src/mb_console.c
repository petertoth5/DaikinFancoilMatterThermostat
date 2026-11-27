/* mb_console.c - the `mb` console command (spec 006). Runs in the REPL task: reads snapshots and queues
 * commands for bus_task; it never touches the bus and never reaches mb_link. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bus_task.h"
#include "daikin_regs.h"
#include "esp_console.h"
#include "esp_timer.h"

#define MS_PER_S            1000
#define PERMILLE            1000
#define FLAG_BITS           16
#define MANUAL_QTY_MIN      1
#define MANUAL_QTY_MAX      4
#define ADDR_MIN            1       /* D19 */
#define ADDR_MAX            247
#define SILENCE_S_MIN       1
#define SILENCE_S_MAX       600
#define SILENCE_S_DEFAULT   60      /* D17 */
#define REG_ADDR_MAX        0xFFFFu
#define VALUE_TEXT_BYTES    24

static bus_task_config_t s_cfg;

typedef struct {
    const char *name;
    int (*run)(int argc, char **argv);
    const char *usage;
} subcommand_t;

static int usage_error(const char *usage)
{
    printf("usage: mb %s\n", usage);
    return 1;
}

bool mb_console_parse_uint(const char *text, uint32_t min, uint32_t max, uint32_t *out)
{
    if (text[0] == '\0' || text[0] == '-') {
        return false;
    }
    char *end = NULL;
    errno = 0;
    unsigned long v = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || v < min || v > max) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static int queue_command(bt_cmd_type_t type, uint32_t a, uint32_t b)
{
    esp_err_t err = bus_task_command(type, a, b);
    if (err != ESP_OK) {
        printf("command not queued: %s\n", esp_err_to_name(err));
        return 1;
    }
    return 0;
}

static const char *health_name(ps_health_t h)
{
    static const char *const names[] = { "STOPPED", "ONLINE", "DEGRADED", "OFFLINE", "LATCHED" };
    return (unsigned)h < sizeof(names) / sizeof(names[0]) ? names[h] : "?";
}

static const char *avail_name(rm_avail_t a)
{
    static const char *const names[] = { "unknown", "ok", "unsupported", "timeout" };
    return (unsigned)a < sizeof(names) / sizeof(names[0]) ? names[a] : "?";
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / MS_PER_S;
}

/* ---- mb status ---- */

static int cmd_status(int argc, char **argv)
{
    (void)argv;
    if (argc != 2) {
        return usage_error("status");
    }
    bus_snapshot_t snap;
    bus_task_get_snapshot(&snap);
    const bus_task_config_t *c = &s_cfg;

    printf("bus: uart %d tx gpio %d rx gpio %d de gpio %d baud %lu 8N1\n", c->link.port.uart_port,
           c->link.port.tx_gpio, c->link.port.rx_gpio, c->link.port.de_gpio, (unsigned long)c->link.port.baud);
    printf("addr: kconfig %u current %u\n", (unsigned)c->sched.addr, (unsigned)snap.addr);
    printf("pacing: gap %lu ms timeout %lu ms retries %u offline_after %u\n", (unsigned long)c->sched.gap_ms,
           (unsigned long)c->link.resp_timeout_ms, (unsigned)c->sched.retries, (unsigned)c->sched.offline_after);
    printf("periods: fast %lu s slow %lu s offline probe %lu s\n", (unsigned long)(c->sched.fast_ms / MS_PER_S),
           (unsigned long)(c->sched.slow_ms / MS_PER_S), (unsigned long)(c->sched.offline_probe_ms / MS_PER_S));
    printf("health: %s\n", health_name(snap.health));
    if (c->hold_err != ESP_OK) {
        printf("mb_link: hold_receive failed: %s (fatal until reboot)\n", esp_err_to_name(c->hold_err));
    }
    if (snap.silence_running) {     /* a stale activity value could be misread as a passed check */
        printf("boot silence: %lu ms, activity running\n", (unsigned long)c->boot_silence_ms);
    } else {
        printf("boot silence: %lu ms, activity %lu\n", (unsigned long)c->boot_silence_ms,
               (unsigned long)snap.boot_activity);
    }
    printf("log: %s, dropped %lu\n", snap.log_on ? "on" : "off", (unsigned long)snap.log_dropped);
    printf("bus_task stack high-water (minimum free): %lu bytes\n", (unsigned long)snap.stack_hwm);
#if CONFIG_DMT_BUS_WRITE
    mb_console_write_print_status(&snap.write);
#else
    printf("write: compiled out\n");
#endif
    if (!snap.link_ctr.measured) {
        printf("link counters: n/a\n");
        return 0;
    }
    printf("link counters: gap_bytes %lu max_byte_interval_us %lu\n",
           (unsigned long)snap.link_ctr.gap_bytes, (unsigned long)snap.link_ctr.max_byte_interval_us);
    printf("faults since boot: %lu\n", (unsigned long)snap.link_ctr.faults);
    if (snap.link_closed) {
        printf("link closed: mb silence or reboot to recover\n");
    }
    return 0;
}

/* ---- mb regs ---- */

static void format_flags(char *buf, size_t n, uint16_t raw)
{
    size_t len = 0;
    for (int bit = FLAG_BITS - 1; bit >= 0 && len + 1 < n; bit--) {
        buf[len++] = (raw >> bit) & 1u ? '1' : '0';
    }
    buf[len] = '\0';
}

static void format_value(char *buf, size_t n, const dr_reg_t *r, uint16_t raw)
{
    int32_t v = daikin_regs_value(r, raw);
    switch (r->kind) {
    case DR_KIND_FLAGS:
        format_flags(buf, n, raw);
        break;
    case DR_KIND_MLT01:
        snprintf(buf, n, "%s%ld.%ld", v < 0 ? "-" : "", labs((long)v) / 10, labs((long)v) % 10);
        break;
    default:
        snprintf(buf, n, "%ld", (long)v);
        break;
    }
}

static void print_reg(const bus_snapshot_t *snap, const dr_reg_t *r, int64_t now)
{
    rm_entry_t e;
    if (!reg_model_get(&snap->model, r->reg, &e)) {
        return;
    }
    char value[VALUE_TEXT_BYTES] = "-";
    char raw_text[VALUE_TEXT_BYTES] = "-";
    char age[VALUE_TEXT_BYTES] = "-";
    if (e.has_value) {
        format_value(value, sizeof(value), r, e.raw);
        snprintf(raw_text, sizeof(raw_text), "%u/0x%04X", (unsigned)e.raw, (unsigned)e.raw);
        snprintf(age, sizeof(age), "%lld", (long long)(now - e.t_ok_ms));
    }
    printf("%03u %-8s%c %-11s raw %-12s val %-18s %-5s age_ms %-8s ok %lu fail %lu exc %02X%s\n",
           (unsigned)r->reg, r->name, r->starred ? '*' : ' ', avail_name(e.avail), raw_text, value,
           reg_model_fresh(&snap->model, r->reg, now) ? "fresh" : "stale", age, (unsigned long)e.ok_count,
           (unsigned long)e.fail_count, (unsigned)e.exc_code, e.out_of_range ? " !range" : "");
}

static int cmd_regs(int argc, char **argv)
{
    (void)argv;
    if (argc != 2) {
        return usage_error("regs");
    }
    bus_snapshot_t snap;
    bus_task_get_snapshot(&snap);
    int64_t now = now_ms();
    for (size_t i = 0; i < daikin_regs_count(); i++) {
        print_reg(&snap, daikin_regs_at(i), now);
    }
    return 0;
}

/* ---- request commands ---- */

static int cmd_read(int argc, char **argv)
{
    static const char usage[] = "read <reg> [qty 1-4]";
    uint32_t reg, qty = MANUAL_QTY_MIN;
    if (argc < 3 || argc > 4 || !mb_console_parse_uint(argv[2], 0, REG_ADDR_MAX, &reg) ||
        (argc == 4 && !mb_console_parse_uint(argv[3], MANUAL_QTY_MIN, MANUAL_QTY_MAX, &qty))) {
        return usage_error(usage);
    }
    return queue_command(BT_CMD_READ, reg, qty);
}

static int cmd_probe(int argc, char **argv)
{
    (void)argv;
    return argc == 2 ? queue_command(BT_CMD_PROBE, 0, 0) : usage_error("probe");
}

static int cmd_poll(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[2], "on") == 0) {
        return queue_command(BT_CMD_POLL_ON, 0, 0);
    }
    if (argc == 3 && strcmp(argv[2], "off") == 0) {
        return queue_command(BT_CMD_POLL_OFF, 0, 0);
    }
    return usage_error("poll on|off");
}

static int cmd_silence(int argc, char **argv)
{
    uint32_t seconds = SILENCE_S_DEFAULT;
    if (argc > 3 || (argc == 3 && !mb_console_parse_uint(argv[2], SILENCE_S_MIN, SILENCE_S_MAX, &seconds))) {
        return usage_error("silence [seconds 1-600]");
    }
    return queue_command(BT_CMD_SILENCE, seconds, 0);
}

static int cmd_addr(int argc, char **argv)
{
    uint32_t addr;
    if (argc != 3 || !mb_console_parse_uint(argv[2], ADDR_MIN, ADDR_MAX, &addr)) {
        return usage_error("addr <1-247>");
    }
    return queue_command(BT_CMD_ADDR, addr, 0);
}

static int cmd_log(int argc, char **argv)
{
    if (argc == 3 && strcmp(argv[2], "on") == 0) {
        return queue_command(BT_CMD_LOG, 1, 0);
    }
    if (argc == 3 && strcmp(argv[2], "off") == 0) {
        return queue_command(BT_CMD_LOG, 0, 0);
    }
    return usage_error("log on|off");
}

#if CONFIG_DMT_BUS_WRITE
static int cmd_write(int argc, char **argv)
{
    bt_cmd_type_t type;
    uint32_t a, b;
    if (!mb_console_write_parse(argc, argv, &type, &a, &b)) {
        return usage_error(MB_CONSOLE_WRITE_USAGE);
    }
    return queue_command(type, a, b);
}
#endif

/* ---- mb stats ---- */

static int cmd_stats(int argc, char **argv)
{
    (void)argv;
    if (argc != 2) {
        return usage_error("stats");
    }
    bus_snapshot_t snap;
    bus_task_get_snapshot(&snap);
    const ps_stats_t *st = &snap.stats;
    uint32_t failed = st->timeout + st->bad_frame + st->error;
    unsigned long rate_permille = st->txns > 0 ? (unsigned long)((uint64_t)failed * PERMILLE / st->txns) : 0;

    printf("txns %lu: ok %lu timeout %lu exception %lu bad_frame %lu error %lu\n", (unsigned long)st->txns,
           (unsigned long)st->ok, (unsigned long)st->timeout, (unsigned long)st->exception,
           (unsigned long)st->bad_frame, (unsigned long)st->error);
    printf("kinds: retries %lu health_probes %lu sweeps %lu manual %lu", (unsigned long)st->retries,
           (unsigned long)st->health_probes, (unsigned long)st->sweeps, (unsigned long)st->manual);
#if CONFIG_DMT_BUS_WRITE
    printf(" pre %lu write %lu verify %lu", (unsigned long)st->pre, (unsigned long)st->write,
           (unsigned long)st->verify);
#endif
    printf("\n");
    printf("error rate (timeout+bad_frame+error)/txns: %lu.%lu %%\n", rate_permille / 10, rate_permille % 10);
    printf("offline entries %lu\n", (unsigned long)st->offline_entries);
    printf("latency ms (answered): min %lu median %lu max %lu\n", (unsigned long)st->lat_min_ms,
           (unsigned long)poll_sched_lat_median_ms(st), (unsigned long)st->lat_max_ms);
    return 0;
}

/* ---- dispatch ---- */

static const subcommand_t s_subcommands[] = {
    { "status",  cmd_status,  "status" },
    { "regs",    cmd_regs,    "regs" },
    { "read",    cmd_read,    "read <reg> [qty 1-4]" },
    { "probe",   cmd_probe,   "probe" },
    { "poll",    cmd_poll,    "poll on|off" },
    { "silence", cmd_silence, "silence [seconds 1-600]" },
    { "stats",   cmd_stats,   "stats" },
    { "addr",    cmd_addr,    "addr <1-247>" },
    { "log",     cmd_log,     "log on|off" },
#if CONFIG_DMT_BUS_WRITE
    { "write",   cmd_write,   MB_CONSOLE_WRITE_USAGE },
#endif
};

static int cmd_mb(int argc, char **argv)
{
    for (size_t i = 0; argc >= 2 && i < sizeof(s_subcommands) / sizeof(s_subcommands[0]); i++) {
        if (strcmp(argv[1], s_subcommands[i].name) == 0) {
            return s_subcommands[i].run(argc, argv);
        }
    }
    printf("usage: mb <subcommand>\n");
    for (size_t i = 0; i < sizeof(s_subcommands) / sizeof(s_subcommands[0]); i++) {
        printf("  mb %s\n", s_subcommands[i].usage);
    }
    return 1;
}

esp_err_t mb_console_register(const bus_task_config_t *cfg)
{
    if (cfg == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_cfg = *cfg;
    const esp_console_cmd_t cmd = {
        .command = "mb",
        .help = "Daikin bus probe: status|regs|read|probe|poll|silence|stats|addr|log",
        .hint = NULL,
        .func = cmd_mb,
    };
    return esp_console_cmd_register(&cmd);
}
