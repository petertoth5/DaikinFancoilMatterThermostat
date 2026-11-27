/* mb_console_write.c - `mb write ...` syntax gate and the write lines of `mb status` (spec 019b).
 * Runs in the REPL task. It only turns words into a queued command; the limits that depend on the live
 * registers are checked by write_guard in bus_task. */

#include <stdio.h>
#include <string.h>
#include "bus_task.h"
#include "daikin_regs.h"

#define SP_REG                 231u
#define SEASON_REG             233u
#define TENTHS_PER_UNIT        10u
#define SP_WHOLE_DIGITS_MAX    1000u    /* stops the digit loop long before overflow; the range check follows */

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/* "21" or "21.5": digits, then at most one decimal digit. Result in raw tenths of a degree. */
static bool parse_tenths(const char *text, uint32_t *tenths)
{
    uint32_t whole = 0;
    size_t i = 0;
    for (; is_digit(text[i]); i++) {
        whole = whole * 10u + (uint32_t)(text[i] - '0');
        if (whole > SP_WHOLE_DIGITS_MAX) {
            return false;
        }
    }
    if (i == 0) {
        return false;
    }
    uint32_t fraction = 0;
    if (text[i] == '.') {
        i++;
        if (!is_digit(text[i])) {
            return false;
        }
        fraction = (uint32_t)(text[i++] - '0');
    }
    if (text[i] != '\0') {
        return false;
    }
    *tenths = whole * TENTHS_PER_UNIT + fraction;
    return true;
}

typedef struct {
    const char *word;
    uint32_t code;
} keyword_t;

static bool parse_keyword(const char *text, const keyword_t *table, size_t count, uint32_t *code)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(text, table[i].word) == 0) {
            *code = table[i].code;
            return true;
        }
    }
    return false;
}

static const keyword_t k_onoff[] = { { "on", 1 }, { "off", 0 } };
static const keyword_t k_fan[] = {
    { "auto", WG_FAN_AUTO }, { "silent", WG_FAN_SILENT }, { "night", WG_FAN_NIGHT }, { "max", WG_FAN_MAX },
};

static bool in_register_range(uint16_t reg, uint32_t value)
{
    const dr_reg_t *r = daikin_regs_find(reg);
    return r != NULL && value <= UINT16_MAX && daikin_regs_in_range(r, (uint16_t)value);
}

static bool set_write(bt_cmd_type_t *type, uint32_t *a, uint32_t *b, wg_cmd_type_t cmd, uint32_t arg)
{
    *type = BT_CMD_WRITE;
    *a = cmd;
    *b = arg;
    return true;
}

static bool parse_arm(int argc, char **argv, bt_cmd_type_t *type, uint32_t *a)
{
    uint32_t seconds = WG_ARM_DEFAULT_S;
    if (argc > 4 || (argc == 4 && !mb_console_parse_uint(argv[3], WG_ARM_MIN_S, WG_ARM_MAX_S, &seconds))) {
        return false;
    }
    *type = BT_CMD_WRITE_ARM;
    *a = seconds;
    return true;
}

/* argv[3] is the single argument of the four write commands. */
static bool parse_write_command(const char *name, const char *text, bt_cmd_type_t *type, uint32_t *a, uint32_t *b)
{
    uint32_t value;
    if (strcmp(name, "sp") == 0) {
        return parse_tenths(text, &value) && in_register_range(SP_REG, value) &&
               set_write(type, a, b, WG_CMD_SP, value);
    }
    if (strcmp(name, "standby") == 0) {
        return parse_keyword(text, k_onoff, sizeof(k_onoff) / sizeof(k_onoff[0]), &value) &&
               set_write(type, a, b, WG_CMD_STANDBY, value);
    }
    if (strcmp(name, "fan") == 0) {
        return parse_keyword(text, k_fan, sizeof(k_fan) / sizeof(k_fan[0]), &value) &&
               set_write(type, a, b, WG_CMD_FAN, value);
    }
    if (strcmp(name, "season") == 0) {
        return mb_console_parse_uint(text, 0, UINT16_MAX, &value) && in_register_range(SEASON_REG, value) &&
               set_write(type, a, b, WG_CMD_SEASON, value);
    }
    return false;
}

bool mb_console_write_parse(int argc, char **argv, bt_cmd_type_t *type, uint32_t *a, uint32_t *b)
{
    if (argc < 3) {
        return false;
    }
    *b = 0;
    if (strcmp(argv[2], "arm") == 0) {
        return parse_arm(argc, argv, type, a);
    }
    if (argc == 3 && strcmp(argv[2], "disarm") == 0) {
        *type = BT_CMD_WRITE_DISARM;
        *a = 0;
        return true;
    }
    if (argc == 3 && strcmp(argv[2], "restore") == 0) {
        return set_write(type, a, b, WG_CMD_RESTORE, 0);
    }
    return argc == 4 && parse_write_command(argv[2], argv[3], type, a, b);
}

void mb_console_write_print_status(const wg_status_t *w)
{
    if (w->armed) {
        printf("write: armed %lu s left, ", (unsigned long)w->left_s);
    } else {
        printf("write: disarmed, ");
    }
    printf("budget %u/%d, failures %u/%d%s\n", (unsigned)w->budget_left, WG_BUDGET_FRAMES, (unsigned)w->failures,
           WG_MAX_FAILURES, w->latched ? ", latched" : "");
    if (w->restore_valid) {
        printf("restore set: 201=%04x 233=%04x 231=%04x\n", (unsigned)w->restore_201, (unsigned)w->restore_233,
               (unsigned)w->restore_231);
    } else {
        printf("restore set: none\n");
    }
}
