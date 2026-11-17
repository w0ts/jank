# jank mini 2.2: fix plan for review pass 2 and sim pass 2 (on 3cec628)

Stage 1 only: nothing is changed on the board yet and nothing is committed. "Sim" numbers come from Claude's own runs
of the sim pass 2 decks (`~/GitHub/jank/worktrees/sim-pass2/decks`, its models, its worst corner: LDO VO -3 %, RDO
0.5 ohm, the LM66100's VON -250 mV, the Schottkys at VF max, caps -20 %, cell 0.30 ohm), with the change applied.
Costs are per board, from JLC's list prices (Basic and preferred parts carry no type fee; an Extended type costs
$3.09 per order).

## Questions for the user (everything else below is decided or technical)

| # | Question | Options (sim, worst corner) | Recommendation |
|---|---|---|---|
| Q1 | **Firmware "empty" voltage** (user decision 1, firmware only). +3V3 stays at or above 3.0 V with the cell alone (Wi-Fi 340 mA + MIDI + LEDs + 1.3" OLED 30 mA ASSUMED) down to **3.45 V**. But **pulling USB in the middle of a Wi-Fi burst** dips +3V3 for 50 .. 100 us (U15's turn-on) | **(a) 3.5 V cutoff:** steady 3.06 V; a USB unplug during a burst dips to 2.92 V (tON 40 us) / 2.87 V (tON 90 us) at 3.5 V, 3.01 / 2.97 V at 3.7 V. Above the brown-out reset (2.44 V), out of the S3's spec for ~0.1 ms. **(b) 3.8 V cutoff:** everything at or over 3.0 V (3.05 / 3.03 V), but a LiPo at 3.8 V under load is roughly half full (ASSUMED typical curve): about half the runtime is lost. **(c) 3.6 V cutoff + D9 -> a 3 A Schottky (SS34, C8678, Basic, +$0.01; SMA footprint, re-placed):** unplug 3.05 / 3.03 V at 3.6 V, 3.01 / 2.98 V at 3.5 V; small hardware change | **(a)**, with the dip written up as a known limit; (c) if the user wants the spec held at 3.6 V |
| Q2 | **The tantalum bulk vs the LDO's stability plot** (review M2). The LDL1117's plot covers 1 .. 22 uF (nominal, ESR 0.02 .. 10 ohm). C72 / C86 (2 x 100 uF tantalum, battery order) carry +3V3 through U15's 40 us turn-on at the unplug | **(a) keep them** (their ESR, 0.9 .. 1.7 ohm, is inside the window; their capacitance is past the plot: stability UNVERIFIED, a bench load step 10 -> 400 mA on the first boards decides). **(b) remove them:** inside the plot, but the unplug dips to 2.62 V at 3.5 V and still 2.91 V at 3.8 V (worst): Q1's 3.0 V cannot be held at any cutoff | **(a)**, with the bench load step on both orders before anything else is built |
| Q3 | **Board size.** 72 x 72 mm was chosen for the 2.42" module, now dropped | (a) keep 72 x 72 (the layout and the case stay; the freed corner helps L9); (b) shrink (a new placement, a new case: days, not hours) | **(a)** |
| Q4 | **U13's edge current** (sim pass item 11: 62 .. 94 mA peaks, +-50 mA absolute maximum "continuous") | (a) keep R83 22 ohm: RMS 19 .. 26 mA, the peaks last nanoseconds; CLK / RST 4.82 V worst (accepted); (b) R83 33 ohm + R74 / R80 47 k: peaks 51 .. 69 mA, CLK / RST 4.88 V worst / 5.30 V typ, but the outputs' fall into 100 k slows from ~0.1 to ~0.3 ms; (c) R83 33 ohm alone: peaks 49 .. 51 mA, CLK / RST 4.70 V worst | **(a)** (the user accepted the level; the rating is for DC) |

## Decided by the user (2026-11-15)

| Item | Fix | Parts | Re-place / re-route |
|---|---|---|---|
| Sim FAIL 1 / 2 (low cell) | firmware cutoff (Q1), README "Firmware" | none | no |
| Sim FAIL 5 (USB current 556 .. 752 mA while charging in a burst) | **R51 5.1 k -> 12 k: ~83 mA charge** (TP4054: I = 1000 V / R below 150 mA; +-10 % ASSUMED). USB peak in a burst: 83 x 1.1 + 340 + 28.5 + 30 (OLED) = **490 mA worst** (was 556 .. 752). Charge time: ~6.5 h for 550 mAh, ~3.5 h for 300 mAh at CC, plus ~1 h of CV (ASSUMED). The twin RT51 still raises it (12 k // 12 k = 166 mA) for someone on a strong supply | R51: 12 k 0603, C22790 (Basic) | no (a value) |
| Sim FAIL 6 (CLK / RST 4.82 V worst into 10 k) | **accepted**, README note. For the record (sim): R74 / R80 47 k: 4.97 V; the 1 k series pair halved: 5.04 V; both: 5.20 V (with longer fall times, and the halved pair over its rating at -12 V) | none | no |
| **The 2.42" screen dropped** | J10B removed (pads, holes, its net stubs); C7466000 out of cost.py SCREEN_PARTS; the README, screen-study and case lid (s242) references; sim FAIL 7 moot. **C86 / the tantalums re-checked:** C86 was +3V3 bulk for the 2.42"'s boost start AND (since pass 1) half of the unplug ride-through; the second role stays (Q2) | J10B gone | re-place (J10B), re-route |
| **Rocker switch** | RS601HL-1010011BB (C2885782, 3 A, ~10.5 x 15 mm snap-in cutout), wired to J11 as before; the MTS-102 (C55159907) out of cost.py USER_PARTS; README; the case's toggle hole -> a rectangular cutout (case plan; STLs later) | user-soldered | no |

## Board fixes (review pass 2 and the sim pass), all technical

| Item | Fix | Parts (per board) | Re-place / route | Sim before -> after |
|---|---|---|---|---|
| **M1** no VBUS sense (Espressif "self-powered device"); **sim 17** phantom VBUS 2.5 .. 2.7 V (USB 2.0 7.1.5.1 / 7.2.1) | (1) **U5's REF (pin 5) from VUSB to +3V3**: the D+ pull-up's path into VBUS is gone (the data lines keep their clamp; VBUS loses U5's clamp, C27 below damps it). (2) **VBUS sense: R88 22 k / R89 33 k from VBUS (after R63) to GPIO38** (pin 43, the bottom-left corner the LEDs freed; no power-up glitch, Table 2-2): 5 V -> 3.0 V, 4.35 V -> 2.61 V (VIH 2.475 V), 0 V with no host. Firmware: the USB pull-up only while VBUS is high | R88 22 k 0402 C25768 (Basic), R89 33 k 0402 C25779 (Basic): ~$0.002 | re-place (2 parts), re-route | phantom VBUS 2.74 V -> 0 V (no path left; to re-run) |
| **L1** hot-plug overshoot: VSYS 6.30 V (battery, 5.5 V source), VREG 6.46 V (usb) vs the LM66100's 6.0 V abs max / the XC6206's 6.0 V | **A damped bulk on VBUS after R63, both orders: C27 fitted + C91, two 10 uF 16 V tantalums** (ESR 3 ohm each: the damping is their ESR; a 6.3 V part is not safe on a 5 .. 5.5 V bus, so not the TAJB107) | TAJA106K016RNJ, C7171, Basic, x 2: ~$0.35. C27's land 0805 -> case A 3216 | re-place, re-route | 5.5 V source, 1 uH, caps -20 %: VSYS 6.30 -> **5.53 V**, usb VREG 6.46 -> **5.34 V** (700 nH: 5.41 / 5.26 V); one TAJA106 alone: 5.92 V (too thin); a 22 uF / 1 ohm part would give 5.27 V but none is Basic. Cost: ~+100 uC of USB inrush (the known FAIL, accepted) |
| **M2** the LDO's ceramics 49.3 uF nominal vs the plot's 22 uF | C5 22 uF -> **4.7 uF** (0603, C19666, Basic); **C60 (10 uF by J10) removed** (the module has its own; C61 100 nF stays); C17 10 uF kept (Espressif's checklist, VDD3P3). Ceramics: 4.7 + 10 + 3 x 2.2 + 8 x 0.1 = **22.1 uF nominal**. The tantalums: Q2. The README's "stable with ceramics" corrected; theta-JA: Rev 1 56 C/W, Rev 3 120 C/W (both quoted) | -1 part | value + one part out; small re-route | with the smaller ceramics (8.5 uF worst on +3V3) and the tantalums: see Q1 |
| **L2** no cap at U15's output (TI 11.1) | **C88 1 uF VSYS-GND at U15 / D9** | 1 uF 0402 C52923 (Basic) | re-place, re-route | - |
| **L3 / sim 18** GPIO35 floats into U13 at reset | **R87 100 k PUMP_IO to GND** | C25741 (Basic) | re-place, re-route | - |
| **L4 / sim 19** GPIO17 (MIDI_TX2) 60 us low glitch at power-up (a 0xFE / 0xFC byte) | **MIDI_TX2 moves to GPIO40** (pin 45, MTDO; not in Table 2-2's glitch list); GPIO17 freed. A pull-up would not help: the datasheet's "low-level glitch" is the pad driving low | none | re-route (a longer MIDI_TX2) | - |
| **L5** SDA 5.6 mm at 0.39 mm beside LIN_GS | `lin_keep.py`: KEEP_NETS += LIN_GS, LIN_VB | none | re-route step | - |
| **L6** dangling stubs (MIDI_TX2 9.4 mm, CHIP_PU 1.9 mm, ...) | `repair.py`'s stub rule: any dangling track (not only < 5 mm), after the joins; DRC must show 0 track_dangling | none | re-route step | - |
| **L7** LINE IN sleeve unclamped (R39 22 ohm 0402 only) | **D14 SMAJ5.0CA LIN_S to GND** at J7's sleeve leg (the D7 / D8 part) | C19077524 (preferred) ~$0.04 | re-place, re-route | - |
| **L8** crystal CL ~6.5 .. 9 pF vs 8 pF | README: measure the frequency on the first board, trim with CT9 / CT10 | none | no | - |
| **L9** PJ-307C locating holes 0.4 mm from the edge (J2, J3, J7) | J2 / J3 / J7 0.5 mm in (the nose still ~2.5 mm past the edge; the case follows from `place_args.py` later) | none | re-place, re-route | - |
| **L10 / L11** stale texts | parts.py (P-FET path, VBAT 1 M, "powered from VBUS", TWINS C72, the AMS1117 sheet), README (HT7833 500 mA, J10B pads, C86 1206) | none | no | - |
| **sim 12** R15 120 mW in a 125 mW 0805 (tip short, fast corner) | the four sinks to 1206 (0.25 W) if a Basic / preferred 68 ohm 1206 exists; else note | to look up | re-place if changed | - |
| **sim 14** a 5 V MIDI OUT plugged into ours lifts +3V3 to 3.61 V with the board idle (U11 / U12 REF) | note only (the board running draws > 90 mA: 3.30 V) | none | no | - |
| **sim 15 / 20 / 21** ADC calibration, TP4054 termination / 8 mV margin, off drain 30 .. 48 uA (~100 uA at 75 C) | README notes (one-point calibration per board; the off drain corrected) | none | no | - |
| **sim 8 / 13 / 16 / 4** double tip short, tip-ring 30 mA, LINE IN headroom, inrush | known, accepted: notes stay | none | no | - |

## Cost of the fixes (estimates)

New lines: R87, R88, R89, C88, C27 + C91 (two TAJA106), R51's new value (all Basic), D14 (preferred):
**about +$0.45 a board in parts, no new Extended type**; J10B and C60 removed. cost.py
reruns at the end; the real JLC quote decides (real orders have come in ~50 % over cost.py).

## What stage 2 does (after the answers)

1. parts.py / schematic / footprints (the parts above, J10B out, MIDI_TX2 -> GPIO40, VBUS_SENSE -> GPIO38).
2. Placement: the new parts, J10B out, J2 / J3 / J7 0.5 mm in, C27 / D9 footprints.
3. **A full re-route is needed** (new parts and two GPIO moves near the chip): Freerouting ~1.5 h at this load, then
   lin_keep (LIN_GS / LIN_VB added), repair, fatten, checks, DRC, make_fab (basic renders, no STLs), cost.md, a local
   commit, then review pass 3 / sim pass 3.

## The user's answers and what stage 2 did with them (2026-11-15)

- **Q1 (c):** D9 = SS34 (C8678, Basic, SMA); the firmware's empty at **3.6 V** (3.7 V with a 2.42" fitted).
- **Q2 (no bench test possible):** inside the datasheets. U2 = **TI TLV75733PDRVR** (C2868428, Extended, $0.34): its
  recommended operating conditions cover COUT 1 .. 200 uF; +3V3 keeps its 49 uF of ceramics (C5 / C60 unchanged) +
  **one** 100 uF tantalum (C86), ~150 uF; the rest of the ride-through moves to the LDO's input: **C72 + C92, 2 x 100
  uF X5R on VSYS** (C15008, Basic, battery order); **D15** B5819W OUT -> IN (TI's reverse-current note). Sim, the
  worst corner (TLV757P VO -1.5 % / 475 mV; LM66100 VON -250 mV / 125 mohm; the Schottky at VF max, a B5819W standing
  in for the SS34; caps -20 %; the sim pass's load), a 3.6 V cell, USB pulled in a TX burst, tON 40 / 90 us:
  **+3V3 3.086 / 3.024 V** (the LDL1117 with two tantalums: 3.039 / 3.014; the TLV757P with one tantalum and no VSYS
  bulk: 3.008 / 2.980; with no bulk at all: 2.89 / 2.87).
- **Q3:** 72 x 72 kept. **Q4 (a):** U13 / R83 as they are.
- **Screens (the user's change):** the 1.3" dropped everywhere (J10 takes the 1.54"); the 2.42" kept on J10B (pads
  only, free). With the 2.42" on the cell (sim, worst): switched on with Wi-Fi idle 3.07 V and all white in a TX burst
  3.07 V at a 3.7 V cell; switched on during a burst 2.70 .. 2.90 V (3.6 .. 3.8 V): the firmware switches it on with
  Wi-Fi idle and stops at 3.7 V. No part needed.
- C60 is not removed after all (the TLV757P's 200 uF covers the ceramics).

## Added during stage 2 (2026-11-15, the user's decisions relayed by the main session)

- **USB host mode, option A** (battery order only): a current-limited 5 V signal on VBUS for self-powered devices
  (U16 HT7550-1 from CV_RAIL, Q7 / Q8 on GPIO42, R93 2.2 k + D16 BAT54C), Q9 (AO3401A) isolating the board's VBUS in
  R63's place, U17 TUSB320LI (DRP, Rd while unpowered) on I2C + GPIO39. `sim/host.py` / `sim/host.txt`; README
  "Review pass 2".
- **The indicator:** two WS2812B-2020-V6 RGB LEDs on GPIO33, VLED = VREG through 4.7 ohm + 10 uF (the user's filter:
  the usb order's hot-plug peak 5.37 V -> 5.00 V at the LEDs). Known limit: under ~3.8 V of cell a Wi-Fi burst takes
  VLED below the LED's 3.3 V for ~2 ms (firmware: refresh every frame).
- **U1 = ESP32-S3FH4R2** (4 MB flash + 2 MB PSRAM): a 5.7 mm EPAD; the firmware's partitions must shrink to two
  ~1.6 MB OTA slots with the drive streaming into the spare slot.

## The power change and the ground parts (2026-11-15, after POWER-STUDY.md)

- +3V3 from a TPS63802 buck-boost (empty at 3.3 V), a 5 V aux rail from a TPS61023 (4.91 V) with a CJ431 clamp (5.21 V),
  one 0.47 uH type (DFE201612E).
- The charge pump removed; CLK / RST from the aux rail (~4.4 V into 10 k, accepted).
- MIDI OUT as 5 V current loops with a 2N7002 per connector (the UART inverted); fallback lands to the 3.3 V drive.
- The host signal from the aux rail (U16 / pump gone).
- R39 100 ohm, C67 2.2 nF fitted, R120 / R121 47 ohm on the CLK / RST sleeves (twins RT120 / RT121).
- Escape hatches: the aux rail's EN and group links, U2's EN link + the U2B LDO land, MODE's PWM land, the MIDI fallback.

## The module and the BOM study's changes (2026-11-15)

- U1 = ESP32-S3-WROOM-1U-N8R2 (8 MB flash, 2 MB PSRAM, its u.FL takes the lid's antenna): the crystal, RF line, J4 and
  the chip's decoupling gone (4 Extended types fewer); LED_DATA on GPIO35.
- R60 / R61 3.3 k (400 kHz I2C with the 2.42" lead).
- The ESD arrays: Hongjiacheng SRV05-4 (C7420376, preferred), its own land.
- The power path: Q1 AO3401A (AN1149 load sharing) + D9 in the LM66100's place (sim/power.py: +3V3 >= 3.247 V worst).
- LINE IN digital: two matched passive networks into GPIO9 / GPIO8, no op-amp / LDO (sim/linein.py).
