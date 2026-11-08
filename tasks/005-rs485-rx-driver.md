# 005 - `rs485_rx` RX-only UART driver

Status: retired (v0.3, 2026-11-08). Not to be implemented.

Why: the RX-only tap (DE and /RE on GND, no TX) belonged to the v0.2 listen-only sniffer on "+ A B -". In v0.3 the node transmits FC03 requests as sole master on "A A B B -"; the UART is owned by esp-modbus in RS485 half-duplex mode, DE + /RE tied to RTS with a 10 kOhm pull-down (Q1).

Replaced by: 002 (`rs485_port`: receive-hold, boot/on-demand silence check, pin and mode setup). The v0.2 text is in git history (commit 7519040).
