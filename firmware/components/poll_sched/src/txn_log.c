/* txn_log.c - format and parse of the probe-log line format v1 (pure C99). */

#include "txn_log.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_LINE_LEN     159u   /* longest valid T line is well below this */
#define MAX_TOKENS       12u    /* T line with 4 data words has 12 tokens */
#define T_FIXED_TOKENS   8u     /* T t dur addr kind reg qty res */
#define H_TOKENS         3u
#define MAX_QTY          4u
#define HEX_WORD_DIGITS  4u
#define HEX_BYTE_DIGITS  2u

/* Indexed by the enum values; the order is part of the format. */
static const char *const KIND_NAMES[] = { "POLL", "RETRY", "HEALTH", "SWEEP", "MANUAL", "PRE", "WRITE", "VERIFY" };
static const char *const RES_NAMES[] = { "OK", "TIMEOUT", "EXC", "BADFRAME", "ERROR" };
static const char *const HEALTH_NAMES[] = { "STOPPED", "ONLINE", "DEGRADED", "OFFLINE", "LATCHED" };
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------------ format */

typedef struct { char *buf; size_t n; size_t len; bool overflow; } writer_t;

static void put(writer_t *w, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

static void put(writer_t *w, const char *fmt, ...)
{
    if (w->overflow) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(w->buf + w->len, w->n - w->len, fmt, ap);
    va_end(ap);
    if (r < 0 || (size_t)r >= w->n - w->len) {
        w->overflow = true;
        return;
    }
    w->len += (size_t)r;
}

static size_t finish(writer_t *w)
{
    if (w->overflow) {
        w->buf[0] = '\0';
        return 0;
    }
    return w->len;
}

size_t txn_log_format_txn(char *buf, size_t n, const ps_txn_t *t)
{
    if (n == 0) {
        return 0;
    }
    if ((unsigned)t->req.kind >= COUNT_OF(KIND_NAMES) || (unsigned)t->res >= COUNT_OF(RES_NAMES)) {
        buf[0] = '\0';
        return 0;
    }
    writer_t w = { buf, n, 0, false };
    put(&w, "T %" PRId64 " %" PRId64 " %u %s %03u %u %s", t->t_start_ms, t->t_end_ms - t->t_start_ms,
        (unsigned)t->req.addr, KIND_NAMES[t->req.kind], (unsigned)t->req.reg, (unsigned)t->req.qty,
        RES_NAMES[t->res]);
    if (t->res == PS_RES_OK) {
        for (uint16_t k = 0; k < t->req.qty && k < MAX_QTY; k++) {
            put(&w, " %04x", (unsigned)t->val[k]);
        }
    } else if (t->res == PS_RES_EXCEPTION) {
        put(&w, " %02x", (unsigned)t->exc_code);
    }
    return finish(&w);
}

const char *txn_log_res_name(ps_res_t res)
{
    return (unsigned)res < COUNT_OF(RES_NAMES) ? RES_NAMES[res] : "?";
}

size_t txn_log_format_health(char *buf, size_t n, int64_t t_ms, ps_health_t h)
{
    if (n == 0) {
        return 0;
    }
    if ((unsigned)h >= COUNT_OF(HEALTH_NAMES)) {
        buf[0] = '\0';
        return 0;
    }
    writer_t w = { buf, n, 0, false };
    put(&w, "H %" PRId64 " %s", t_ms, HEALTH_NAMES[h]);
    return finish(&w);
}

/* ------------------------------------------------------------ parse */

/* Split on single spaces; an empty token (double or trailing space) is kept so that
 * the field parsers reject it. Returns the token count, or 0 if there are too many. */
static size_t split(char *line, char *tok[MAX_TOKENS])
{
    size_t count = 0;
    char *p = line;
    for (;;) {
        if (count == MAX_TOKENS) {
            return 0;
        }
        tok[count++] = p;
        p = strchr(p, ' ');
        if (p == NULL) {
            return count;
        }
        *p++ = '\0';
    }
}

static bool parse_dec(const char *s, int64_t max, int64_t *out)
{
    if (*s == '\0') {
        return false;
    }
    int64_t v = 0;
    for (; *s != '\0'; s++) {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10 + (*s - '0');
        if (v > max) {
            return false;
        }
    }
    *out = v;
    return true;
}

static bool parse_hex(const char *s, size_t digits, uint16_t *out)
{
    if (strlen(s) != digits) {
        return false;
    }
    uint16_t v = 0;
    for (; *s != '\0'; s++) {
        int d;
        if (*s >= '0' && *s <= '9') {
            d = *s - '0';
        } else if (*s >= 'a' && *s <= 'f') {
            d = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'F') {
            d = *s - 'A' + 10;
        } else {
            return false;
        }
        v = (uint16_t)(v * 16 + d);
    }
    *out = v;
    return true;
}

static int lookup(const char *s, const char *const *names, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(s, names[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool parse_result_data(char *tok[], size_t ntok, ps_txn_t *t)
{
    if (t->res == PS_RES_OK) {
        if (ntok != T_FIXED_TOKENS + t->req.qty) {
            return false;
        }
        for (uint16_t k = 0; k < t->req.qty; k++) {
            if (!parse_hex(tok[T_FIXED_TOKENS + k], HEX_WORD_DIGITS, &t->val[k])) {
                return false;
            }
        }
        return true;
    }
    if (t->res == PS_RES_EXCEPTION) {
        uint16_t code;
        if (ntok != T_FIXED_TOKENS + 1 || !parse_hex(tok[T_FIXED_TOKENS], HEX_BYTE_DIGITS, &code)) {
            return false;
        }
        t->exc_code = (uint8_t)code;
        return true;
    }
    return ntok == T_FIXED_TOKENS;
}

static bool parse_txn(char *tok[], size_t ntok, ps_txn_t *t)
{
    int64_t start, dur, addr, reg, qty;
    int kind, res;
    if (ntok < T_FIXED_TOKENS ||
        !parse_dec(tok[1], INT64_MAX / 10 - 1, &start) || !parse_dec(tok[2], INT64_MAX / 10 - 1, &dur) ||
        !parse_dec(tok[3], UINT8_MAX, &addr) || !parse_dec(tok[5], UINT16_MAX, &reg) ||
        !parse_dec(tok[6], MAX_QTY, &qty) || qty < 1) {
        return false;
    }
    kind = lookup(tok[4], KIND_NAMES, COUNT_OF(KIND_NAMES));
    res = lookup(tok[7], RES_NAMES, COUNT_OF(RES_NAMES));
    if (kind < 0 || res < 0) {
        return false;
    }
    t->t_start_ms = start;
    t->t_end_ms = start + dur;
    t->req.addr = (uint8_t)addr;
    t->req.reg = (uint16_t)reg;
    t->req.qty = (uint16_t)qty;
    t->req.kind = (ps_kind_t)kind;
    t->res = (ps_res_t)res;
    return parse_result_data(tok, ntok, t);
}

static bool parse_health(char *tok[], size_t ntok, tl_rec_t *rec)
{
    int64_t t_ms;
    if (ntok != H_TOKENS || !parse_dec(tok[1], INT64_MAX / 10 - 1, &t_ms)) {
        return false;
    }
    int h = lookup(tok[2], HEALTH_NAMES, COUNT_OF(HEALTH_NAMES));
    if (h < 0) {
        return false;
    }
    rec->t_ms = t_ms;
    rec->health = (ps_health_t)h;
    return true;
}

int txn_log_parse(const char *line, tl_rec_t *out)
{
    bool is_txn = strncmp(line, "T ", 2) == 0;
    bool is_health = strncmp(line, "H ", 2) == 0;
    if (!is_txn && !is_health) {
        return 0;
    }
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        len--;
    }
    if (len > MAX_LINE_LEN) {
        return -1;
    }
    char copy[MAX_LINE_LEN + 1];
    memcpy(copy, line, len);
    copy[len] = '\0';

    char *tok[MAX_TOKENS];
    size_t ntok = split(copy, tok);
    if (ntok == 0) {
        return -1;
    }
    tl_rec_t rec;
    memset(&rec, 0, sizeof(rec));
    rec.type = is_txn ? TL_TXN : TL_HEALTH;
    bool ok = is_txn ? parse_txn(tok, ntok, &rec.txn) : parse_health(tok, ntok, &rec);
    if (!ok) {
        return -1;
    }
    *out = rec;
    return 1;
}
