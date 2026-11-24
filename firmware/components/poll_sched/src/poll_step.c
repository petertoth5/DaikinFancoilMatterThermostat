/* poll_step.c - one schedule -> link -> complete -> model step (pure C99). */

#include "poll_sched.h"
#include <string.h>

#define EXC_ILLEGAL_ADDR 2u   /* exception 02: register not supported (rule 6) */

/* Model update for a finished transaction (spec "poll_step model updates"). */
static void update_model(reg_model_t *m, const ps_txn_t *t, bool final_failure)
{
    int64_t now = t->t_end_ms;
    if (t->res == PS_RES_OK) {
        for (uint16_t k = 0; k < t->req.qty; k++) {
            reg_model_set_ok(m, (uint16_t)(t->req.reg + k), t->val[k], now);
        }
        return;
    }
    if (t->req.qty != 1) {
        return;   /* failures of qty > 1 requests do not touch the model */
    }
    if (t->res == PS_RES_EXCEPTION) {
        /* Same condition as rule 6 (HEALTH excluded); other exceptions are failed reads. */
        if (t->exc_code == EXC_ILLEGAL_ADDR && t->req.kind != PS_KIND_HEALTH) {
            reg_model_set_unsupported(m, t->req.reg, t->exc_code, now);
        } else {
            reg_model_set_exception(m, t->req.reg, t->exc_code, now);
        }
    } else if (final_failure) {
        reg_model_set_timeout(m, t->req.reg, now);
    }
}

bool poll_step(poll_sched_t *s, reg_model_t *m, const ps_env_t *env, ps_txn_t *txn, int64_t *wait_until_ms)
{
    ps_req_t req;
    if (!poll_sched_next(s, env->clock(env->clock_ctx), &req, wait_until_ms)) {
        return false;
    }
    memset(txn, 0, sizeof(*txn));
    txn->req = req;
    txn->t_start_ms = env->clock(env->clock_ctx);
    txn->res = env->link(env->link_ctx, req.addr, req.reg, req.qty, txn->val, &txn->exc_code);
    txn->t_end_ms = env->clock(env->clock_ctx);
    poll_sched_complete(s, txn);
    if (req.addr != s->cfg.addr) {
        return true;   /* rule 12: address changed during the call, model untouched */
    }
    /* A failed POLL that the scheduler queued for retry is not final yet. */
    bool retry_queued = req.kind == PS_KIND_POLL && s->retry_pending;
    update_model(m, txn, !retry_queued);
    return true;
}
