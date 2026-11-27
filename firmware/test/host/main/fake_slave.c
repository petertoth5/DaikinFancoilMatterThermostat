/* fake_slave.c - scripted Modbus slave for the poll_sched link seam (task 011, host only). */

#include "fake_slave.h"
#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "daikin_regs.h"
#include "txn_log.h"

#define EXC_ILLEGAL_ADDR 2u   /* Modbus exception 02, answered for registers nobody configured */
#define ADDR_MIN 1u           /* valid slave addresses on the bus */
#define ADDR_MAX 247u
#define LOG_LINE_MAX 256

typedef struct {
    ps_res_t res;
    uint8_t exc_code;
    uint16_t val[4];
    uint32_t latency_ms;
} fake_answer_t;

void fake_init(fake_slave_t *f, uint8_t addr, int64_t *clock_ms, uint32_t resp_timeout_ms)
{
    memset(f, 0, sizeof(*f));
    f->addr = addr;
    f->clock_ms = clock_ms;
    f->resp_timeout_ms = resp_timeout_ms;
}

static fake_reg_t *reg_slot(fake_slave_t *f, uint16_t reg)
{
    TEST_ASSERT_TRUE_MESSAGE(reg < FAKE_REG_SPACE, "fake: register outside FAKE_REG_SPACE");
    return &f->regs[reg];
}

void fake_set_reg(fake_slave_t *f, uint16_t reg, uint16_t raw, uint32_t latency_ms)
{
    fake_reg_t *r = reg_slot(f, reg);
    r->mode = FAKE_REG_OK;
    r->raw = raw;
    r->latency_ms = latency_ms;
}

void fake_set_exception(fake_slave_t *f, uint16_t reg, uint8_t code)
{
    fake_reg_t *r = reg_slot(f, reg);
    r->mode = FAKE_REG_EXC;
    r->exc_code = code;
    r->latency_ms = FAKE_DEFAULT_LATENCY_MS;
}

void fake_set_silent(fake_slave_t *f, uint16_t reg)
{
    reg_slot(f, reg)->mode = FAKE_REG_SILENT;
}

void fake_fail_request(fake_slave_t *f, uint32_t request_number)
{
    f->fail_at_request = request_number;
}

void fake_set_write(fake_slave_t *f, uint16_t reg, fake_write_mode_t mode, uint16_t arg)
{
    fake_reg_t *r = reg_slot(f, reg);
    r->write_mode = mode;
    r->write_arg = arg;
}

void fake_set_dead(fake_slave_t *f, int64_t from_ms, int64_t to_ms)
{
    f->dead_from_ms = from_ms;
    f->dead_to_ms = to_ms;
}

void fake_fail_every(fake_slave_t *f, uint32_t n)
{
    f->fail_every_n = n;
}

int fake_load_log(fake_slave_t *f, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("fake_load_log: cannot open %s\n", path);
        return -1;
    }
    char line[LOG_LINE_MAX];
    int line_no = 0;
    int rc = 0;
    while (rc == 0 && fgets(line, sizeof(line), fp) != NULL) {
        line_no++;
        tl_rec_t rec;
        int parsed = txn_log_parse(line, &rec);
        if (parsed < 0 || (parsed > 0 && rec.type == TL_TXN && f->replay_n >= FAKE_REPLAY_MAX)) {
            printf("fake_load_log: %s line %d: %s\n", path, line_no, parsed < 0 ? "malformed" : "replay table full");
            rc = -1;
        } else if (parsed > 0 && rec.type == TL_TXN) {
            f->replay[f->replay_n].txn = rec.txn;
            f->replay[f->replay_n].consumed = false;
            f->replay_n++;
        }
    }
    fclose(fp);
    f->replaying = (rc == 0);
    return rc;
}

/* ------------------------------------------------------------ answers */

static fake_answer_t timeout_answer(const fake_slave_t *f)
{
    fake_answer_t a;
    memset(&a, 0, sizeof(a));
    a.res = PS_RES_TIMEOUT;
    a.latency_ms = f->resp_timeout_ms;
    return a;
}

static bool in_dead_window(const fake_slave_t *f, int64_t now_ms)
{
    return now_ms >= f->dead_from_ms && now_ms < f->dead_to_ms;
}

static bool is_scripted_failure(const fake_slave_t *f)
{
    return (f->fail_every_n != 0 && f->request_count % f->fail_every_n == 0) ||
           (f->fail_at_request != 0 && f->request_count == f->fail_at_request);
}

static fake_answer_t answer_from_regs(fake_slave_t *f, uint8_t addr, uint16_t reg, uint16_t qty, int64_t now_ms)
{
    if (addr != f->addr || in_dead_window(f, now_ms) || is_scripted_failure(f)) {
        return timeout_answer(f);
    }
    fake_answer_t a;
    memset(&a, 0, sizeof(a));
    a.res = PS_RES_OK;
    a.latency_ms = reg_slot(f, reg)->latency_ms;
    for (uint16_t k = 0; k < qty; k++) {
        const fake_reg_t *r = reg_slot(f, (uint16_t)(reg + k));
        if (r->mode == FAKE_REG_SILENT) {
            return timeout_answer(f);
        }
        if (r->mode != FAKE_REG_OK) {   /* EXC or UNSET */
            a.res = PS_RES_EXCEPTION;
            a.exc_code = r->mode == FAKE_REG_EXC ? r->exc_code : EXC_ILLEGAL_ADDR;
            a.latency_ms = r->mode == FAKE_REG_EXC ? r->latency_ms : FAKE_DEFAULT_LATENCY_MS;
            return a;
        }
        a.val[k] = r->raw;
    }
    return a;
}

/* Next unused recording of the key, else the last one, else TIMEOUT. */
static fake_answer_t answer_from_replay(fake_slave_t *f, uint8_t addr, uint16_t reg, uint16_t qty)
{
    fake_replay_t *last = NULL;
    fake_replay_t *next = NULL;
    for (size_t i = 0; i < f->replay_n; i++) {
        fake_replay_t *e = &f->replay[i];
        if (e->txn.req.addr != addr || e->txn.req.reg != reg || e->txn.req.qty != qty) {
            continue;
        }
        last = e;
        if (!e->consumed && next == NULL) {
            next = e;
        }
    }
    if (last == NULL) {
        return timeout_answer(f);
    }
    fake_replay_t *use = next != NULL ? next : last;
    use->consumed = true;
    fake_answer_t a;
    memset(&a, 0, sizeof(a));
    a.res = use->txn.res;
    a.exc_code = use->txn.exc_code;
    memcpy(a.val, use->txn.val, sizeof(a.val));
    a.latency_ms = (uint32_t)(use->txn.t_end_ms - use->txn.t_start_ms);
    return a;
}

/* Common start of a request: bus rules, new journal entry. */
static ps_txn_t *begin_request(fake_slave_t *f, uint8_t addr, uint16_t reg, uint16_t qty)
{
    TEST_ASSERT_TRUE_MESSAGE(addr >= ADDR_MIN && addr <= ADDR_MAX, "fake: request address outside 1..247");
    TEST_ASSERT_TRUE_MESSAGE(f->journal_n < FAKE_JOURNAL_MAX, "fake: journal full");
    if (f->journal_n > 0) {   /* the first request after init has no predecessor */
        TEST_ASSERT_GREATER_OR_EQUAL_INT64_MESSAGE(FAKE_DEFAULT_GAP_MS, *f->clock_ms - f->last_end_ms,
                                                   "fake: bus gap < 100 ms between two requests");
    }
    ps_txn_t *t = &f->journal[f->journal_n++];
    memset(t, 0, sizeof(*t));
    t->req.addr = addr;
    t->req.reg = reg;
    t->req.qty = qty;
    t->t_start_ms = *f->clock_ms;
    f->request_count++;
    return t;
}

ps_res_t fake_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc)
{
    fake_slave_t *f = ctx;
    TEST_ASSERT_TRUE_MESSAGE(daikin_regs_read_allowed(reg, qty), "fake: read of an undocumented register or qty outside 1..4");
    ps_txn_t *t = begin_request(f, addr, reg, qty);
    fake_answer_t a = f->replaying ? answer_from_replay(f, addr, reg, qty)
                                   : answer_from_regs(f, addr, reg, qty, t->t_start_ms);
    *f->clock_ms += a.latency_ms;

    t->res = a.res;
    t->exc_code = a.exc_code;
    memcpy(t->val, a.val, sizeof(t->val));
    t->t_end_ms = *f->clock_ms;
    f->last_end_ms = t->t_end_ms;

    *exc = a.exc_code;
    memcpy(val, a.val, qty * sizeof(uint16_t));
    return a.res;
}

/* ------------------------------------------------------------ FC06 */

#define WRITABLE_REGS_N 3
static const uint16_t k_writable_regs[WRITABLE_REGS_N] = { 201, 231, 233 };   /* spec 019 WR1 */

static bool is_writable_reg(uint16_t reg)
{
    for (size_t i = 0; i < WRITABLE_REGS_N; i++) {
        if (k_writable_regs[i] == reg) {
            return true;
        }
    }
    return false;
}

/* A register that would not answer a read (dead window, scripted failure, silent, exception, unset)
 * does not answer a write either; otherwise the register's write script decides. */
static fake_answer_t answer_write(fake_slave_t *f, uint16_t reg, uint16_t value, int64_t now_ms)
{
    fake_answer_t a = answer_from_regs(f, f->addr, reg, 1, now_ms);
    if (a.res != PS_RES_OK) {
        return a;
    }
    fake_reg_t *r = reg_slot(f, reg);
    switch (r->write_mode) {
    case FAKE_WRITE_OK:            r->raw = value; break;
    case FAKE_WRITE_SILENT_LANDED: r->raw = value; return timeout_answer(f);
    case FAKE_WRITE_SILENT_LOST:   return timeout_answer(f);
    case FAKE_WRITE_EXC:           a.res = PS_RES_EXCEPTION; a.exc_code = (uint8_t)r->write_arg; break;
    case FAKE_WRITE_BAD_ECHO:      r->raw = value; a.res = PS_RES_BAD_FRAME; break;
    case FAKE_WRITE_CLAMP:         r->raw = r->write_arg; break;
    }
    return a;
}

ps_res_t fake_write(void *ctx, uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc)
{
    fake_slave_t *f = ctx;
    TEST_ASSERT_TRUE_MESSAGE(is_writable_reg(reg), "fake: FC06 to a register outside {201, 231, 233}");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(f->addr, addr, "fake: FC06 to an address other than the slave's");
    ps_txn_t *t = begin_request(f, addr, reg, 1);
    t->req.kind = PS_KIND_WRITE;
    f->write_count++;
    fake_answer_t a = answer_write(f, reg, value, t->t_start_ms);
    *f->clock_ms += a.latency_ms;
    t->res = a.res;
    t->exc_code = a.exc_code;
    t->val[0] = value;
    t->t_end_ms = *f->clock_ms;
    f->last_end_ms = t->t_end_ms;
    *exc = a.exc_code;
    return a.res;
}

uint32_t fake_write_count(const fake_slave_t *f)
{
    return f->write_count;
}

void fake_check_write_spacing(const fake_slave_t *f)
{
    int64_t previous_start_ms = 0;
    bool have_previous = false;
    for (size_t i = 0; i < f->journal_n; i++) {
        const ps_txn_t *t = &f->journal[i];
        if (t->req.kind != PS_KIND_WRITE) {
            continue;
        }
        if (have_previous) {
            TEST_ASSERT_GREATER_OR_EQUAL_INT64_MESSAGE(FAKE_WRITE_MIN_SPACING_MS, t->t_start_ms - previous_start_ms,
                                                       "fake: two FC06 frames < 5000 ms apart");
        }
        previous_start_ms = t->t_start_ms;
        have_previous = true;
    }
}

int64_t fake_clock(void *ctx)
{
    return *((fake_slave_t *)ctx)->clock_ms;
}

size_t fake_journal(const fake_slave_t *f, const ps_txn_t **out)
{
    *out = f->journal;
    return f->journal_n;
}

void fake_tag_last(fake_slave_t *f, ps_kind_t kind)
{
    TEST_ASSERT_TRUE_MESSAGE(f->journal_n > 0, "fake: no request to tag");
    ps_txn_t *t = &f->journal[f->journal_n - 1];
    t->req.kind = kind;
    TEST_ASSERT_TRUE_MESSAGE(t->req.qty == 1 || kind == PS_KIND_MANUAL, "fake: qty > 1 outside a MANUAL read");
}
