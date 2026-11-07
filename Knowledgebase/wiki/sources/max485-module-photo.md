---
title: Photo of the MAX485-M RS485 breakout module (user)
type: source
sources: ["20261107_192127.jpg"]
updated: 2026-11-07
---

# MAX485-M module photo

Raw: `20261107_192127.jpg` — user photo, 1949x1259 JPEG, added 2026-11-07. Facts below were extracted **visually** from the image by `datasheet-reader` (no text extraction possible). Confidence markers are kept as reported.

## What the photo shows
- Top (component) side of a small blue PCB on a breadboard; a red/white twisted pair is attached at the right. Underside not shown. Glare and blur limit legibility.
- The board is a **breakout module**, not a bare chip.
- The layout matches the common low-cost "MAX485 TTL-to-RS485 module" *(inferred, general recognition, not sourced)*.

## Components
- **U1:** SO-8 package. **Top marking illegible**, so chip type (MAX485 / MAX3485 / SP485 ...) and manufacturer are **unconfirmed**. SO-8 fits the MAX485 pinout in [[sources/max485-datasheet]] *(inferred)*.
- **Left 4-pin header**, labels clearly visible: **RO, RE, DE, DI** (no overbar visible on RE). DE and RE are **separate pins**. No auto-direction circuitry visible on the top side (no transistor or second IC) *(inferred from absence)*.
- **Right 4-pin header:** only **"GND"** readable, the other labels are hidden; that the rest is VCC/A/B is *(inferred)*.
- **Green 2-pole screw terminal:** labels not visible; A/B *(inferred)*.
- **R7:** code read as "121" (= 120 Ω), medium-high confidence; originally *inferred* to be a termination across A-B (trace not followable). **Superseded, see conflict below.**
- **R5, R6, R8:** codes illegible; possibly A/B bias resistors *(inferred, unverified)*.
- **R1–R4** (next to RO/RE/DE/DI): codes illegible, function unknown.
- **C1, C2:** values unknown. **D1:** possibly a power LED *(inferred)*.
- **Silkscreen:** "C25B" (board ID/revision, *inferred*); no product name; **no 5V/3.3V marking visible**, so the supply voltage is **not determined** from the photo.

## Conflict: R7 vs. user statement on termination
- **User (session 2026-11-07, not in raw):** "I had to buy a 120 ohm separately, it is not on the board by default." So the module has **no on-board 120 Ω termination**; the user has a separate 120 Ω resistor.
- **Photo inference (above):** R7 "121" = fixed 120 Ω across A-B.
- **Resolution for the termination question:** the user statement wins; the module is treated as **unterminated**. R7's actual role, or whether the "121" reading is correct, is **unexplained** (not guessed). An A–B resistance measurement on the unpowered module would settle it.

## Open items (need another photo or a multimeter)
- U1 top marking.
- Right header and screw-terminal labels; resistor codes; underside of the board.
- R7: role / code reading unexplained (see conflict above); A–B resistance measurement would clarify. A–VCC and B–GND for bias resistors.
- Does D1 light when powered?

## Related
[[entities/max485]], [[concepts/bus-topology]], [[sources/max485-datasheet]].
