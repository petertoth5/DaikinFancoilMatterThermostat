#pragma once

/* fake_slave.h - scripted Modbus slave behind the poll_sched link seam (task 011).
 * Test-only (host); never linked into the firmware. Plugs in as ps_link_fn / ps_clock_fn,
 * advances a simulated clock instead of sleeping, and fails the running Unity test on any
 * request the real bus must never carry.
 */

#include <stddef.h>
#include <stdint.h>
#include "poll_sched.h"

#define FAKE_REG_SPACE   256     /* register addresses 0..255 (documented ones end at 247) */
#define FAKE_JOURNAL_MAX 2048    /* requests kept for assertions */
#define FAKE_REPLAY_MAX  512     /* recorded transactions kept in replay mode */
#define FAKE_DEFAULT_LATENCY_MS 25
#define FAKE_DEFAULT_GAP_MS     100   /* wiki firmware-architecture section 3, D16: end of one request to start of the next */

typedef enum { FAKE_REG_UNSET, FAKE_REG_OK, FAKE_REG_EXC, FAKE_REG_SILENT } fake_reg_mode_t;

typedef struct {
    fake_reg_mode_t mode;
    uint16_t raw;
    uint32_t latency_ms;
    uint8_t exc_code;
} fake_reg_t;

typedef struct {
    ps_txn_t txn;       /* recorded result; dur = t_end - t_start is the replay latency */
    bool consumed;
} fake_replay_t;

typedef struct fake_slave {
    uint8_t addr;
    int64_t *clock_ms;
    uint32_t resp_timeout_ms;
    fake_reg_t regs[FAKE_REG_SPACE];
    int64_t dead_from_ms, dead_to_ms;
    uint32_t fail_every_n;
    uint32_t request_count;
    int64_t last_end_ms;       /* end of the previous request; valid once journal_n > 0 */
    ps_txn_t journal[FAKE_JOURNAL_MAX];
    size_t journal_n;
    bool replaying;
    fake_replay_t replay[FAKE_REPLAY_MAX];
    size_t replay_n;
} fake_slave_t;

void fake_init(fake_slave_t *f, uint8_t addr, int64_t *clock_ms, uint32_t resp_timeout_ms); /* advances *clock_ms */
/* An unset register answers with exception 02 (illegal data address). */
void fake_set_reg(fake_slave_t *f, uint16_t reg, uint16_t raw, uint32_t latency_ms);
void fake_set_exception(fake_slave_t *f, uint16_t reg, uint8_t code);
void fake_set_silent(fake_slave_t *f, uint16_t reg);                 /* no response -> TIMEOUT */
void fake_set_dead(fake_slave_t *f, int64_t from_ms, int64_t to_ms); /* every request TIMEOUT in [from, to) */
void fake_fail_every(fake_slave_t *f, uint32_t n);                   /* every n-th request TIMEOUT, 0 = off */
int  fake_load_log(fake_slave_t *f, const char *path);               /* replay mode; -1 on parse error (line number printed) */
ps_res_t fake_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc); /* ps_link_fn */
int64_t  fake_clock(void *ctx);                                     /* ps_clock_fn */
size_t   fake_journal(const fake_slave_t *f, const ps_txn_t **out); /* every request with timing, for assertions */
/* The link seam does not carry the request kind: the caller tells the fake the kind of the
 * request it just made (poll_step's txn). Stored in the journal; qty > 1 with kind != MANUAL fails the test. */
void fake_tag_last(fake_slave_t *f, ps_kind_t kind);
