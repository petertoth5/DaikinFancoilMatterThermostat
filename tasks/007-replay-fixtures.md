# 007 - Replay fixtures (`dmt-capture v1`) and sniffer end-to-end test

Status: retired (v0.3, 2026-11-08). Not to be implemented.

Why: the capture format and the end-to-end test were built around the passive decoder (003) and monitor (004), both retired with the move to sole master on "A A B B -" (user decision 2026-11-08).

Replaced by: 011 (scripted fake slave, end-to-end through `poll_sched`/`reg_model`, fixtures in the `txn_log` v1 probe-log format from 010, so on-site probe logs become fixtures directly). The v0.2 text is in git history (commit 7519040).
