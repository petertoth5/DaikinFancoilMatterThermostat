# 002 - esp-modbus `==2.1.4` master spike, `rs485_port`, `mb_link` read path, ELF check

Status: retired (2026-11-24, user decision on escalation S3). Not to be implemented further.

Why: the spike (`docs/spikes/002-esp-modbus.md`) stopped at S3: `mbc_master_send_request` returns the same `ESP_ERR_INVALID_RESPONSE` for a slave exception and a CRC/malformed reply and drops the exception code, so D21 (exception 02 -> unsupported; retry on timeout/CRC but not on exception) cannot be implemented on top of it. Further findings: `mbc_master_start` needs a descriptor table (uninitialised field otherwise), write handlers cannot be compiled out. The user chose **E2** (2026-11-24): no esp-modbus at all, own minimal FC03-only RTU master; D7 and Q9 reversed.

Replaced by (amended 2026-11-24, user decision):
- 012 `mb_rtu`: pure CRC / FC03 builder / reply classifier, host-tested.
- 013 bus-access lint update (no esp-modbus anywhere, `uart_write_bytes` only in `rs485_port/src/`, FC bytes only in `mb_rtu`) and the ELF symbol check (was goal 5 here).
- 014 `rs485_port` + `mb_link` over the UART, API of this spec unchanged (incl. silence check and hold-receive semantics, D8, D17).

Still valid from the spike and used by 014: S5 (DE = RTS, low idle, high while transmitting; `uart_set_mode` before `uart_set_pin`) and S10 (driver delete releases the pins). The v0.3 text of this spec is in git history (last version before commit of this note).

Fallback (amended 2026-11-24, user decision): the esp-modbus route is kept as the documented, not implemented fallback E1b; see `014-rs485-port-mb-link.md`, section "Fallback E1b".
