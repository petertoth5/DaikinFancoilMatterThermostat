---
title: MAX481/483/485/487-491/1487 datasheet (Maxim, via Hestore)
type: source
sources: ["MAX485-M_EN_10035508.pdf"]
updated: 2026-11-07
---

# MAX485 family datasheet

Raw: `MAX485-M_EN_10035508.pdf` — 18 PDF pages: page 1 is a Hestore cover, pages 2-18 are the Maxim datasheet 19-0122 Rev 10 (9/14). **Printed page N = PDF page N+1.** It is the generic chip-family datasheet; it says nothing about the "MAX485-M" breakout/module board (see gaps in [[entities/max485]]). Read visually (images), not via text extraction.

## Key facts for the MAX485 variant (printed pages)
- Half-duplex RS-485/RS-422 transceiver, one driver + one receiver, up to **2.5 Mbps**, **not** slew-rate limited, no shutdown mode, has DE and /RE; **32 receivers** on the bus (12 kΩ input = 1 unit load) (p1 selection table, p2).
- **Supply 5 V ±5 %** (4.75-5.25 V); abs. max 12 V (p2, p7).
- Logic inputs DE, DI, /RE: V_IH ≥ 2.0 V, V_IL ≤ 0.8 V (p2) → a 3.3 V logic level satisfies the input thresholds (inferred).
- Receiver output RO: V_OH ≥ 3.5 V at I = -4 mA, V_CC = 5 V (p2) → RO is a 5 V-level signal; the ESP32-S3 GPIO is 3.3 V logic, so RO needs level shifting/divider (inferred, ESP32-S3 datasheet not in raw).
- Receiver threshold ±0.2 V, hysteresis 70 mV; **fail-safe**: RO = high with open inputs (p2, p10 Table 2, p1).
- Bus pins A/B abs. max -8 V to +12.5 V (p2).
- Pinout, 8-pin DIP/SO: 1 RO, 2 /RE, 3 DE, 4 DI, 5 GND, 6 A (non-inverting), 7 B (inverting), 8 V_CC (p7, Fig. 1).
- Direction control: DE high enables the driver (DI high → Y/A high); /RE low enables the receiver. Tie DE and /RE together for send/receive switching (inferred from Tables 1-2, p10). Driver enable/disable ≈ 40-70 ns (p3).
- Applications (p13): the line is terminated at both ends in its characteristic impedance (the **120 Ω** value is from the figures); keep stubs as short as possible; standard covers up to 4000 ft.
- Driver has foldback current limit and thermal shutdown (p11).

## Related
[[entities/max485]], [[concepts/bus-topology]], [[concepts/modbus-rs485-link]]. Module board photo: [[sources/max485-module-photo]].
