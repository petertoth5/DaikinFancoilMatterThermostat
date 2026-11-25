/* sim_console.c - simulated register source owner and console commands (task 016). */

#include "sim_console.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_console.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "mapping.h"
#include "sim_source.h"

#define TICK_PERIOD_US 1000000   /* spec: 1 s tick */
#define MS_PER_US_DIV 1000
#define REG_MAX 0xFFFFu          /* register addresses and raw values are 16 bit */
#define CONSOLE_PROMPT "sim> "
#define CONSOLE_MAX_CMDLINE 128

static const char *TAG = "sim_console";

static struct {
    SemaphoreHandle_t mutex;
    sim_t sim;
    map_state_t map;
    map_thermostat_t mapped;  /* result of the last tick, shown by "sim show" */
    bool mapped_valid;        /* false until the first tick */
} s;

/* Copy of everything "sim show" prints, taken under the mutex so printing never blocks the tick or the bridge. */
typedef struct {
    sim_state_t state;
    reg_model_t model;
    int64_t now;
    bool mapped_valid;
    map_thermostat_t mapped;
} show_copy_t;

static show_copy_t s_show;  /* static: too big for the console task stack; only the console task uses it */

static int64_t now_ms(void)
{
    return esp_timer_get_time() / MS_PER_US_DIV;
}

static void tick(void *arg)
{
    (void)arg;
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    int64_t now = now_ms();
    sim_tick(&s.sim, now);
    mapping_thermostat(&s.map, sim_model(&s.sim), now, &s.mapped);
    s.mapped_valid = true;
    xSemaphoreGive(s.mutex);
}

void sim_console_snapshot(reg_model_t *out, int64_t *now)
{
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    *out = *sim_model(&s.sim);
    *now = now_ms();
    xSemaphoreGive(s.mutex);
}

/* Whole string must be a decimal number 0..REG_MAX. */
static bool parse_u16(const char *text, uint16_t *value)
{
    char *end = NULL;
    unsigned long v = strtoul(text, &end, 10);
    if (text[0] == '\0' || *end != '\0' || v > REG_MAX) {
        return false;
    }
    *value = (uint16_t)v;
    return true;
}

static const char *avail_name(rm_avail_t a)
{
    switch (a) {
    case RM_OK: return "ok";
    case RM_UNSUPPORTED: return "unsupported";
    case RM_TIMEOUT: return "timeout";
    default: return "unknown";
    }
}

static const char *mode_name(uint8_t mode)
{
    switch (mode) {
    case MAP_MODE_OFF: return "Off";
    case MAP_MODE_COOL: return "Cool";
    case MAP_MODE_HEAT: return "Heat";
    default: return "?";
    }
}

static const char *state_name(sim_state_t st)
{
    switch (st) {
    case SIM_LIVE: return "LIVE";
    case SIM_STALE: return "STALE";
    default: return "OFFLINE";
    }
}

static void show_copy_take(show_copy_t *c)
{
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    c->state = s.sim.state;
    c->model = *sim_model(&s.sim);
    c->now = now_ms();
    c->mapped_valid = s.mapped_valid;
    c->mapped = s.mapped;
    xSemaphoreGive(s.mutex);
}

static void print_mapped(const map_thermostat_t *t)
{
    if (t->local_temp_valid) {
        printf("LocalTemperature %d\n", t->local_temp);
    } else {
        printf("LocalTemperature null\n");
    }
    printf("Setpoint heat=%d cool=%d observed=%d\n", t->heat_sp, t->cool_sp, t->sp_observed);
    printf("Limits heat=[%d,%d] cool=[%d,%d]\n", t->min_heat, t->max_heat, t->min_cool, t->max_cool);
    printf("SystemMode %s\n", mode_name(t->system_mode));
    printf("flags: season_auto=%d limits_conflict=%d\n", t->season_auto_flag, t->limits_conflict);
}

static int cmd_show(void)
{
    show_copy_t *c = &s_show;
    show_copy_take(c);
    printf("state %s\n", state_name(c->state));
    for (int i = 0; i < RM_REG_COUNT; i++) {
        const rm_entry_t *e = &c->model.e[i];
        if (e->avail == RM_UNKNOWN) {
            continue;
        }
        printf("  %03u raw=%u %s age=%" PRId64 "ms fresh=%d out_of_range=%d\n", e->reg, e->raw, avail_name(e->avail),
               c->now - e->t_ok_ms, reg_model_fresh(&c->model, e->reg, c->now), e->out_of_range);
    }
    if (c->mapped_valid) {
        print_mapped(&c->mapped);
    } else {
        printf("mapped: n/a (no tick yet)\n");
    }
    return 0;
}

static int cmd_set_state(sim_state_t st)
{
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    sim_set_state(&s.sim, st, now_ms());
    xSemaphoreGive(s.mutex);
    return 0;
}

static int cmd_set(const char *reg_text, const char *raw_text)
{
    uint16_t reg, raw;
    if (!parse_u16(reg_text, &reg) || !parse_u16(raw_text, &raw)) {
        printf("error: <reg> and <raw> must be decimal 0..65535\n");
        return 1;
    }
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    int rc = sim_set(&s.sim, reg, raw, now_ms());
    xSemaphoreGive(s.mutex);
    if (rc != 0) {
        printf("error: register %u is not documented\n", reg);
    }
    return rc != 0;
}

static int cmd_unsupported(const char *reg_text)
{
    uint16_t reg;
    if (!parse_u16(reg_text, &reg)) {
        printf("error: <reg> must be decimal 0..65535\n");
        return 1;
    }
    xSemaphoreTake(s.mutex, portMAX_DELAY);
    int rc = sim_unsupported(&s.sim, reg, now_ms());
    xSemaphoreGive(s.mutex);
    if (rc != 0) {
        printf("error: register %u is not documented\n", reg);
    }
    return rc != 0;
}

static int usage(void)
{
    printf("usage: sim set <reg> <raw> | sim unsup <reg> | sim stale | sim offline | sim live | sim show\n");
    return 1;
}

static bool is_cmd(char **argv, int argc, const char *name, int args)
{
    return argc == args + 2 && strcmp(argv[1], name) == 0;
}

static int cmd_sim(int argc, char **argv)
{
    if (argc < 2) {
        return usage();
    }
    if (is_cmd(argv, argc, "set", 2)) {
        return cmd_set(argv[2], argv[3]);
    }
    if (is_cmd(argv, argc, "unsup", 1)) {
        return cmd_unsupported(argv[2]);
    }
    if (is_cmd(argv, argc, "stale", 0)) {
        return cmd_set_state(SIM_STALE);
    }
    if (is_cmd(argv, argc, "offline", 0)) {
        return cmd_set_state(SIM_OFFLINE);
    }
    if (is_cmd(argv, argc, "live", 0)) {
        return cmd_set_state(SIM_LIVE);
    }
    if (is_cmd(argv, argc, "show", 0)) {
        return cmd_show();
    }
    return usage();
}

static esp_err_t start_tick(void)
{
    const esp_timer_create_args_t args = {.callback = tick, .name = "sim_tick"};
    esp_timer_handle_t timer;
    esp_err_t err = esp_timer_create(&args, &timer);
    if (err != ESP_OK) {
        return err;
    }
    return esp_timer_start_periodic(timer, TICK_PERIOD_US);
}

static esp_err_t start_repl(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = CONSOLE_PROMPT;
    repl_config.max_cmdline_length = CONSOLE_MAX_CMDLINE;
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_uart(&uart_config, &repl_config, &repl);
    if (err != ESP_OK) {
        return err;
    }
    const esp_console_cmd_t cmd = {
        .command = "sim",
        .help = "Simulated registers: set <reg> <raw> | unsup <reg> | stale | offline | live | show (show prints the console's own mapping state, which may differ from what the bridge published)",
        .hint = NULL,
        .func = cmd_sim,
    };
    err = esp_console_cmd_register(&cmd);
    if (err != ESP_OK) {
        return err;
    }
    return esp_console_start_repl(repl);
}

esp_err_t sim_console_start(void)
{
    s.mutex = xSemaphoreCreateMutex();
    if (!s.mutex) {
        return ESP_ERR_NO_MEM;
    }
    sim_init(&s.sim, now_ms());
    mapping_init(&s.map);
    esp_err_t err = start_tick();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "tick timer: %s", esp_err_to_name(err));
        return err;
    }
    return start_repl();
}
