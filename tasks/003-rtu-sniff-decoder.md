# 003 - `rtu_sniff` passive frame decoder

Status: retired (v0.3, 2026-11-08). Not to be implemented.

Why: v0.2 listened passively on the "+ A B -" fancoil link and needed its own frame splitter. Since 2026-11-08 the node is the sole Modbus master on "A A B B -" (user decision, `concepts/bus-topology`); framing, CRC and response parsing are done by the esp-modbus master (Q9, `==2.1.4`).

Replaced by: 002 (`mb_link` read path over esp-modbus). The v0.2 text is in git history (commit 7519040).
