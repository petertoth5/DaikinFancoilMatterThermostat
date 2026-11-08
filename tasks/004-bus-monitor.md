# 004 - `bus_monitor` pairing, mirror, statistics

Status: retired (v0.3, 2026-11-08). Not to be implemented.

Why: request/response pairing of sniffed third-party traffic is meaningless when the node is the sole master on "A A B B -" (user decision 2026-11-08, `concepts/bus-topology`); the node knows its own requests.

Replaced by: 010 (`reg_model` register state, `poll_sched` statistics and link health). The v0.2 text is in git history (commit 7519040).
