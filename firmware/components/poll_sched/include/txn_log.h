#pragma once

/* txn_log.h - one-line text format v1 for transactions and health changes
 * (probe log, fixtures). Pure C99, no IDF.
 *
 *   # dmt-probe v1 addr=<a> baud=<b> fw=<version>   (header, written by bus_task; any '#' line is a comment)
 *   T <t_start_ms> <dur_ms> <addr> <kind> <reg> <qty> <res>[ <data>]
 *   H <t_ms> <STOPPED|ONLINE|DEGRADED|OFFLINE|LATCHED>
 * kind POLL|RETRY|HEALTH|SWEEP|MANUAL|PRE|WRITE|VERIFY (the last three: write sequence, spec 019; a WRITE OK line
 * carries the written value); reg 3-digit zero-padded decimal; res OK (+qty 4-digit
 * lowercase hex values), EXC (+2-digit hex code), TIMEOUT, BADFRAME, ERROR.
 * Formatted lines carry no trailing newline.
 */

#include <stddef.h>
#include <stdint.h>
#include "poll_sched.h"

typedef struct {
    enum { TL_TXN, TL_HEALTH } type;
    ps_txn_t txn;
    int64_t t_ms;
    ps_health_t health;
} tl_rec_t;

/* Return the line length; 0 if n is too small (buf is then NUL-terminated if n > 0). */
size_t txn_log_format_txn(char *buf, size_t n, const ps_txn_t *t);
/* Name of a result as used in T lines ("OK", "TIMEOUT", "EXC", "BADFRAME", "ERROR"); "?" if out of range. */
const char *txn_log_res_name(ps_res_t res);
size_t txn_log_format_health(char *buf, size_t n, int64_t t_ms, ps_health_t h);

/* 1 parsed, 0 ignored (not starting with "T " or "H "; so also the W and A lines of the write guard), -1 malformed. Accepts a trailing "\r\n" or "\n". */
int txn_log_parse(const char *line, tl_rec_t *out);
