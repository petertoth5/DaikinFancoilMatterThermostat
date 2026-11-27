/* wg_format.c - the W and A log lines of the write guard (spec 019 WR11; pure C99).
 * Format (t in ms, hhhh = 4-digit lowercase hex, registers 3-digit decimal as in the T lines):
 *   A <t> ARMED <s> s budget <n>            A <t> EXTENDED <s> s left budget <n>
 *   A <t> RESTORE_SET 201=<hhhh> 233=<hhhh> 231=<hhhh>
 *   A <t> REFUSED arm <latched|health|stale <reg>|web_flag <n>>
 *   A <t> DISARMED <reason>                 A <t> LATCHED failures <k>
 *   W <t> CMD <cmd> [<arg>]                 W <t> REFUSED <cmd> <reason>
 *   W <t> SKIP <reg> unchanged <hhhh>       W <t> ABORTED <reg> <disarm reason>
 *   W <t> DONE <reg> <old> -> <new> budget <n>
 *   W <t> FAIL <reg> <detail> failures <k>/3
 */

#include "write_guard.h"
#include <stdio.h>
#include "txn_log.h"

#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* Indexed by the enum values; the order is part of the format. */
static const char *const CMD_NAMES[] = { "sp", "standby", "fan", "season", "restore" };
static const char *const FAN_NAMES[] = { "auto", "silent", "night", "max" };
static const char *const REASON_NAMES[] = { "user", "timeout", "failure", "budget", "health", "poll_off",
                                            "silence", "addr", "latched", "link", "web_flags" };
static const char *const REFUSE_NAMES[] = { "latched", "health", "disarmed", "busy", "pacing", "stale",
                                            "web_flag", "limit", "undefined_mode" };

static int format_arg(char *buf, size_t n, const wg_event_t *e)
{
    switch (e->cmd) {
    case WG_CMD_STANDBY: return snprintf(buf, n, " %s", e->arg != 0u ? "on" : "off");
    case WG_CMD_FAN:     return snprintf(buf, n, " %s", e->arg < COUNT_OF(FAN_NAMES) ? FAN_NAMES[e->arg] : "?");
    case WG_CMD_SP:
    case WG_CMD_SEASON:  return snprintf(buf, n, " %u", (unsigned)e->arg);
    case WG_CMD_RESTORE: return snprintf(buf, n, "%s", "");
    }
    return -1;
}

/* What follows the refusal name: the stale register, flag number, ms left or the allowed range. */
static int format_refusal_detail(char *buf, size_t n, const wg_event_t *e)
{
    switch (e->refuse) {
    case WG_REFUSE_STALE:    return snprintf(buf, n, " %03u", (unsigned)e->detail);
    case WG_REFUSE_WEB_FLAG: return snprintf(buf, n, " %u", (unsigned)e->detail);
    case WG_REFUSE_PACING:   return snprintf(buf, n, " %u", (unsigned)e->detail);
    case WG_REFUSE_LIMIT:    return snprintf(buf, n, " %u-%u", (unsigned)e->lo, (unsigned)e->hi);
    default:                 return snprintf(buf, n, "%s", "");
    }
}

static int format_fail_detail(char *buf, size_t n, const wg_event_t *e)
{
    switch (e->fail) {
    case WG_FAIL_PRE:         return snprintf(buf, n, "pre_%s", txn_log_res_name(e->res));
    case WG_FAIL_WRITE:       return snprintf(buf, n, "write_%s", txn_log_res_name(e->res));
    case WG_FAIL_VERIFY:      return snprintf(buf, n, "verify_%s", txn_log_res_name(e->res));
    case WG_FAIL_MISMATCH:    return snprintf(buf, n, "mismatch %04x", (unsigned)e->old_raw);
    case WG_FAIL_VERIFY_LOST: return snprintf(buf, n, "verify_lost");
    }
    return -1;
}

/* Two-part lines (command or failure detail first) are built in a small scratch buffer. */
#define PART_MAX 48

static int format_line(char *buf, size_t n, const wg_event_t *e)
{
    char part[PART_MAX];
    long long t = (long long)e->t_ms;
    switch (e->kind) {
    case WG_EV_ARMED:
        return snprintf(buf, n, "A %lld ARMED %u s budget %u", t, (unsigned)e->seconds, (unsigned)e->budget);
    case WG_EV_EXTENDED:
        return snprintf(buf, n, "A %lld EXTENDED %u s left budget %u", t, (unsigned)e->seconds, (unsigned)e->budget);
    case WG_EV_RESTORE_SET:
        return snprintf(buf, n, "A %lld RESTORE_SET 201=%04x 233=%04x 231=%04x", t, (unsigned)e->restore_201,
                        (unsigned)e->restore_233, (unsigned)e->restore_231);
    case WG_EV_ARM_REFUSED:
        if ((unsigned)e->refuse >= COUNT_OF(REFUSE_NAMES) || format_refusal_detail(part, sizeof(part), e) < 0) {
            return -1;
        }
        return snprintf(buf, n, "A %lld REFUSED arm %s%s", t, REFUSE_NAMES[e->refuse], part);
    case WG_EV_DISARMED:
        return (unsigned)e->reason < COUNT_OF(REASON_NAMES)
                   ? snprintf(buf, n, "A %lld DISARMED %s", t, REASON_NAMES[e->reason]) : -1;
    case WG_EV_LATCHED:
        return snprintf(buf, n, "A %lld LATCHED failures %u", t, (unsigned)e->failures);
    case WG_EV_CMD:
        if ((unsigned)e->cmd >= COUNT_OF(CMD_NAMES) || format_arg(part, sizeof(part), e) < 0) {
            return -1;
        }
        return snprintf(buf, n, "W %lld CMD %s%s", t, CMD_NAMES[e->cmd], part);
    case WG_EV_REFUSED:
        if ((unsigned)e->cmd >= COUNT_OF(CMD_NAMES) || (unsigned)e->refuse >= COUNT_OF(REFUSE_NAMES) ||
            format_refusal_detail(part, sizeof(part), e) < 0) {
            return -1;
        }
        return snprintf(buf, n, "W %lld REFUSED %s %s%s", t, CMD_NAMES[e->cmd], REFUSE_NAMES[e->refuse], part);
    case WG_EV_SKIP:
        return snprintf(buf, n, "W %lld SKIP %03u unchanged %04x", t, (unsigned)e->reg, (unsigned)e->old_raw);
    case WG_EV_ABORTED:
        return (unsigned)e->reason < COUNT_OF(REASON_NAMES)
                   ? snprintf(buf, n, "W %lld ABORTED %03u %s", t, (unsigned)e->reg, REASON_NAMES[e->reason]) : -1;
    case WG_EV_DONE:
        return snprintf(buf, n, "W %lld DONE %03u %04x -> %04x budget %u", t, (unsigned)e->reg,
                        (unsigned)e->old_raw, (unsigned)e->new_raw, (unsigned)e->budget);
    case WG_EV_FAIL:
        if (format_fail_detail(part, sizeof(part), e) < 0) {
            return -1;
        }
        return snprintf(buf, n, "W %lld FAIL %03u %s failures %u/%u", t, (unsigned)e->reg, part,
                        (unsigned)e->failures, (unsigned)WG_MAX_FAILURES);
    }
    return -1;
}

size_t write_guard_format_event(char *buf, size_t n, const wg_event_t *e)
{
    if (n == 0) {
        return 0;
    }
    int len = format_line(buf, n, e);
    if (len < 0 || (size_t)len >= n) {
        buf[0] = '\0';
        return 0;
    }
    return (size_t)len;
}
