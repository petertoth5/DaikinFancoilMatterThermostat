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

#define FAKE_WRITE_MIN_SPACING_MS 5000   /* spec 019 WR6: FC06 frames at least this far apart (start to start) */

typedef enum { FAKE_REG_UNSET, FAKE_REG_OK, FAKE_REG_EXC, FAKE_REG_SILENT } fake_reg_mode_t;

/* How the slave answers an FC06 to one register (spec 019, default OK). */
typedef enum {
    FAKE_WRITE_OK,             /* store the value, echo it */
    FAKE_WRITE_SILENT_LANDED,  /* store the value, no reply (TIMEOUT) */
    FAKE_WRITE_SILENT_LOST,    /* no reply, nothing stored */
    FAKE_WRITE_EXC,            /* exception reply, nothing stored */
    FAKE_WRITE_BAD_ECHO,       /* store the value, reply that is no echo (BADFRAME) */
    FAKE_WRITE_CLAMP           /* store a different value, echo the request */
} fake_write_mode_t;

typedef struct {
    fake_reg_mode_t mode;
    uint16_t raw;
    uint32_t latency_ms;
    uint8_t exc_code;
    fake_write_mode_t write_mode;
    uint16_t write_arg;        /* exception code (EXC) or the stored value (CLAMP) */
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
    uint32_t fail_at_request;  /* one request, by its number (request_count after counting it), TIMEOUT; 0 = off */
    uint32_t request_count;    /* reads and writes */
    uint32_t write_count;      /* FC06 frames received */
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
void fake_fail_request(fake_slave_t *f, uint32_t request_number);    /* that one request TIMEOUT (read or write) */
void fake_set_write(fake_slave_t *f, uint16_t reg, fake_write_mode_t mode, uint16_t arg);
int  fake_load_log(fake_slave_t *f, const char *path);               /* replay mode; -1 on parse error (line number printed) */
ps_res_t fake_link(void *ctx, uint8_t addr, uint16_t reg, uint16_t qty, uint16_t *val, uint8_t *exc); /* ps_link_fn */
/* wg_write_fn: FC06. Shares clock, journal (kind WRITE, val[0] = written value) and the 100 ms gap rule with
 * fake_link; fails the running test for a register outside {201, 231, 233} or an address that is not the slave's. */
ps_res_t fake_write(void *ctx, uint8_t addr, uint16_t reg, uint16_t value, uint8_t *exc);
uint32_t fake_write_count(const fake_slave_t *f);
void     fake_check_write_spacing(const fake_slave_t *f);           /* every two FC06 frames >= 5000 ms apart */
int64_t  fake_clock(void *ctx);                                     /* ps_clock_fn */
size_t   fake_journal(const fake_slave_t *f, const ps_txn_t **out); /* every request with timing, for assertions */
/* The link seam does not carry the request kind: the caller tells the fake the kind of the
 * request it just made (poll_step's txn). Stored in the journal; qty > 1 with kind != MANUAL fails the test. */
void fake_tag_last(fake_slave_t *f, ps_kind_t kind);
