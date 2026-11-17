# jank mini 2.2: power-section study (stage 1, 2026-11-15) and the ground strategy

**Implemented the same day (the user's decisions):** B + C, CLOCK / RESET from the 5 V aux rail (the pump removed),
MIDI as a true 5 V loop, the host signal from the aux rail, the ground parts (R39 100 ohm, C67 fitted, 47 ohm CLK / RST
sleeves with twins). Changed from this study: the boost is set to 4.91 V (not 5.0) and the clamp is a CJ431 at 5.21 V
(Basic), so the clamp never conducts at the boost's tolerance; see the README's "Review pass 2" rows and `sim/`.

Study only. Nothing below is placed or routed. The datasheets were read this session (three reading agents). Their text
sits in `~/GitHub/jank/worktrees/mini2-2-scratch/pb/ds/`, and every figure here comes from those files. Behavioural
simulations are marked as such. ASSUMED marks a guess.

## 1. Recommendation

**Do not adopt an all-in-one power-bank IC.** None of the twelve candidates meets four basics:
- a 5 V rail that never switches itself off;
- a charge current that can be held under USB's 500 mA;
- a USB-C role that the board controls, never the chip's own Try.SRC;
- device mode and charging that survive a dead part.

Instead, keep today's charging path (TP4054, LM66100, TUSB320) and add two converters:

- **B. A 5 V aux rail.** A TI TPS61023 boost (C919459, $0.23, SOT-563) runs from VSYS.
  - It feeds the RGB LEDs, the MIDI loops (a true 5 V current loop), the host VBUS (real 5 V through a current-limited
    switch) and optionally CLK / RST.
  - Its datasheet (SLVSF14B): 0.5 .. 5.5 V in, 1 MHz with auto PFM at light load, 20 uA from VOUT, true disconnect
    when off, pass-through when VIN > VOUT, TJ -40 .. 125 C.
- **C. 3.3 V from a buck-boost.** A TI TPS63802 (C2845237, $0.91, VSON-10) replaces the TLV75733P, so the fee is
  unchanged.
  - Its datasheet (SLVSEU9D): 1.3 .. 5.5 V in, 11 uA, 2 A at VO 3.3 V, TJ -40 .. 125 C.
  - +3V3 then stays flat through every dip of VSYS (Wi-Fi bursts, the USB unplug), down to about 2.3 V of input.
  - That ends the dropout rules: the 3.6 V empty point, the 2.42" switch-on rule and the RGB LEDs' supply dips. The
    cell's empty point can drop to about 3.2 .. 3.3 V under load.
- **One inductor type for both.** 0.47 uH is inside both datasheets' ranges (TPS63802 0.37 .. 0.57 uH, TPS61023
  0.37 .. 2.9 uH). Candidate: TDK VLS252012HBX-R47M-1 (C88537, 1008, $0.06); its saturation current is not checked.

The charger, the power path and the USB roles stay as built and simulated. A dead boost or buck-boost does not stop
charging. A dead buck-boost does stop the chip, so it gets its own escape hatch (§6).

## 2. The candidates (from their datasheets)

| Part (LCSC) | Charger | 5 V out | Light-load auto-off | Power path | Temp | USB-C roles | Verdict |
|---|---|---|---|---|---|---|---|
| IP5306 (C181692) | switching 2.1 A, not settable | 2.1 A, 500 kHz | **< 45 mA for 32 s, no disable**; key starts boost | "charge while discharge", no timing | **0 .. 70 C** | none | no |
| IP5306-I2C (C488349) | 0.05 .. 3.15 A over I2C | 2.4 A | 8 .. 64 s over I2C, **cannot be disabled**; needs a key double-click to sleep | as IP5306 | 0 .. 70 C | none | no |
| IP5305T (C2982924) | 1.2 A, not settable | 1.0 A, 350 kHz | **< 45 mA (20 .. 80 mA) for 32 s** | stated, no timing | -20 .. 70 C | none | no |
| IP5407 (C2890537) | ~2 A, adapter-adaptive, **no 500 mA setting** | 2.1 A, 950 kHz | output stays at 5 V ("low-power mode") | stated, no timing | TJ only | none | closest Injoinic part; fails the 500 mA budget, and its own circuit adds an external protector |
| IP5310 (C191176) | 2.3 .. 3.2 A, not settable | 3.1 A | < 45 mA for 32 s | VIN -> VOUT PMOS | not given | **DRP with Try.SRC: it would source 5 V into a computer**; no forced role; I2C only on a custom part | no |
| IP5385 (C42411072) | | | | | | | **2..4S only** |
| ETA9640 (C7465502) | linear, 1000/Rset (settable) | 1 A, 1.8 MHz, EN pin | none | **none**: the load always runs from the cell through the boost; termination is confused by the load | -40 .. 85 C | none | workable, but no pass-through |
| ETA9740 (C7465512) | switching, Riset 2 A / 3 A only | 2.4 A, 650 kHz | none | one bidirectional OUT pin; load while charging not covered | -40 .. 85 C | none | no |
| ETA9697 (C7435201) | linear 1000/Rset | **0.4 A** | none | none | -40 .. 85 C | none | too weak; no undervoltage stop |
| TP5400 (C2891434) | linear, settable | 1 A, VFM (variable frequency near audio) | none | none; needs 47 uF tantalum + TVS | -40 .. 85 C | none | no |

Common gaps: no datasheet gives the plug / unplug handover timing of its 5 V output. None states that an external cell
protector can go (IP5306-I2C and IP5407 show one in their own circuits), so the **XB5353A stays** in every variant.

## 3. What each idea from the brief becomes (with B + C)

- **(a) +3V3 from a rail with no dropout.** Yes, through C (the buck-boost from VSYS, not "LDO from 5 V").
  - An LDO from 5 V would waste a third of the energy.
  - The cell's empty point goes from 3.6 V to about 3.2 .. 3.3 V under load. Its exact value should come from the cell
    and the XB5353A's 2.4 .. 3.0 V cutoff; I have not computed it here.
- **(b) RGB LEDs on 5 V.** Yes: VLED from the aux rail. R98 / C98 can stay as the filter. VIH 0.55 x 5 V = 2.75 V, which
  GPIO33 still drives.
- **(c) CLK / RST straight from 5 V.** Possible, but a **regression against the user's "5 V+ into 10 k"**.
  - 5.0 V through the 1 k pair gives 4.55 V into 10 k (today's pump: 5.33 V) and 4.95 V into 100 k.
  - Keeping 5 V into 10 k needs about 220 ohm series, i.e. ~55 mA into a -12 V fault: no.
  - **Recommended: keep the pump (U13 and its Extended fee) for CLK / RST**, or relax the target to "5 V into 100 k"
    (the usual Eurorack input). The user's call.
- **(d) MIDI as a true 5 V loop.** Each connector gets a 2N7002 low-side switch (Basic, on the BOM; the UART's TX
  inverted in firmware: `uart_set_line_inverse`), 220 ohm source, 220 ohm sink.
  - Receiver current (calculation):

    | aux rail | opto 1.2 V | opto 1.5 V | opto 1.7 V |
    |---|---|---|---|
    | 4.75 V | 5.30 mA | 4.85 mA | 4.55 mA |
    | 5.00 V | 5.68 mA | 5.23 mA | 4.92 mA |
    | 5.25 V | 6.06 mA | 5.61 mA | 5.30 mA |

  - Today: 4.0 .. 5.8 mA, and the GPIO carries the loop.
  - The GPIO then carries no loop current, which **ends the drive-strength trade-off**.
  - A tip shorted to GND puts 23.9 mA into the 220 ohm sink (0.13 W: an 0805 or 1206, not 0402).
  - Four more FETs and resistors per connector pair: a re-layout of the MIDI blocks.
- **(e) Real 5 V on VBUS in host mode.** The aux rail feeds an SY6280AAC current-limited switch (C55136, Extended,
  $0.09; limit set by a resistor), which replaces U16 / Q7 / Q8 / R93 / D16.
  - Whether to power small gear becomes the user's choice: a firmware enable, and the limit resistor.
  - Q9 and U17 stay.
  - Cell cost: 0.5 A at 5 V is ~0.75 A from the cell (88 % ASSUMED). The sim below shows what that does to VSYS.
- **(f) A simpler power path.** Not much simpler: B + C keep the path and add two converters. In exchange they remove
  the HT7550 signal chain and (if (c) is accepted) the pump.

## 4. Simulations

**A 5 V aux load on VSYS at the USB unplug** (the review-pass-2 worst corner: TLV757P -1.5 %, LM66100 VON -250 mV; the
board's sim-pass load + 30 mA OLED).
- The TPS61023 is modelled behaviourally: P_in = P_out / 0.88 (ASSUMED; the sheet's curves are not digitised).
- Script: `mini2-2-scratch/pb/sim/aux5.py`.

| 5 V aux load | +3V3 min, cell 3.7 V | +3V3 min, cell 3.6 V | VSYS min, cell 3.6 V |
|---|---|---|---|
| 0 | 3.080 V | 3.009 V | 3.24 V |
| 50 mA | 3.065 | 2.993 | 3.22 |
| 100 mA | 3.047 | 2.978 | 3.19 |
| 200 mA | 3.014 | 2.943 | 3.13 |
| 500 mA (host power) | 2.877 | **2.760** | 2.97 |

- **With B alone (the LDO kept), the aux rail eats the unplug margin.** At 3.6 V and 50 mA, +3V3 already falls under
  3.0 V. The firmware would have to shed the 5 V loads at a low cell, or raise the empty point.
- **With C, it does not matter.** +3V3 is regulated while VSYS stays above ~2.3 V (the TPS63802's input range with
  margin). The lowest VSYS in the table is 2.97 V with 500 mA of host power.
  - The +3V3 traces with the buck-boost were not simulated; this follows from its 1.3 .. 5.5 V input range.

**Battery life** (calculation; the load and the cell's capacity split are ASSUMED):
- The load: 80 mA + 280 mA x 20 % = 136 mA average at 3.3 V = 0.45 W.

| Supply of +3V3 | Efficiency | Power from the cell | Runtime vs today | Usable capacity |
|---|---|---|---|---|
| today: LDO from VSYS (3.7 V mean) | 89 % | 0.50 W | 1.00 | empty at 3.6 V |
| all-in-one IC: boost to 5 V + LDO to 3.3 V | 0.88 x 0.66 = 58 % | 0.77 W | **~0.65** | + the 3.6 .. 3.2 V band |
| boost + buck 5 -> 3.3 V (TLV62569) | 0.88 x 0.9 = 79 % | 0.57 W | ~0.88 | + the band |
| **C: buck-boost from the cell** | ~90 % (ASSUMED; its sheet's curve not read off) | 0.50 W | **~1.0** | + the band |

- The band between 3.6 V and 3.2 V under load holds ~10 .. 15 % of a LiPo's energy at this rate (ASSUMED, a typical
  curve). So C is ~1.1 .. 1.15 x today's runtime and the all-in-one ~0.7 x.
- The aux rail adds only what its loads draw: LEDs at one colour 24 mA, MIDI 4 x 5 mA while sending, CLK µA.

**Boost ripple, LINE IN and RF** (calculation and layout rules; nothing coupled is simulated):
- The aux rail feeds no analog part. LINE IN's AVDD stays on U7 (XC6206), and U7 moves from VREG to +3V3 or stays on
  VREG.
- The coupling paths are the boost's input current on VSYS (1 MHz plus PFM bursts at light load, which can fall in the
  audio band) and its ground return.
- Keep-outs to give the placer:
  - the TPS61023 and TPS63802 hot loops (SW node, inductor, output caps) at least 10 mm from every LIN_* net, J7 and
    the LIN_GS sense node;
  - their ground on a local pour island tied to the main GND at the VSYS bulk caps (C72 / C92), not under the LINE IN
    block or the RF line;
  - no LINE IN or RF track over an inductor;
  - both converters in the power block at the lower left; the LINE IN front end is at the top middle, about 30 mm away.
- PFM in the audio band could be avoided by putting the TPS63802 in forced PWM (its MODE pin), at a higher light-load
  current. The TPS61023 has no PWM-only mode in its feature list.

## 5. Cost (battery order of 10; fees at $3.09 each; parts at the snapshot's prices)

| Change | Extended fee | Parts per board |
|---|---|---|
| + TPS61023 (C919459) | +1 | +0.23 |
| + 0.47 uH inductor, one type for both converters | +1 | +0.12 (two) |
| TPS63802 (C2845237) in place of the TLV75733P (C2868428) | 0 | +0.57 |
| + SY6280AAC host switch (C55136), - U16 HT7550 (preferred), Q7 / Q8 / R93 / D16 | +1 | -0.07 |
| MIDI: + 4 x 2N7002 + 0805 resistors (Basic) | 0 | +0.08 |
| CLK / RST from 5 V: - U13 (C23654) and the pump's parts | -1 | -0.20 |
| **B + C with CLK / RST from 5 V** | **+2 = +$6.18** | **+$0.73 (x 10 = $7.30)** |
| B + C keeping the pump | +3 = +$9.27 | +$0.93 ($9.30) |

- Estimated total on today's $148.76 (cost.md, battery x 10): +$13.5 .. 18.6, so **~$162 .. 167**.
- Real orders came in ~50 % above `cost.py`.
- The all-in-one alternative (IP5407 or ETA9640 + inductor; - LM66100, - TP4054, - SS34 / D3) would save one fee net,
  but fails the requirements in §2.

## 6. Escape hatches for B + C

- **The aux rail:** its EN to VSYS through a fitted 0 ohm (lift it: aux off) and a 0 ohm link into each consumer group
  (LEDs, MIDI, host switch). A dead or shorted TPS61023 takes only the 5 V features with it; charging and device mode do
  not touch it.
- **The buck-boost (the chip's only supply):**
  - its EN through a fitted 0 ohm;
  - beside it, a DNP SOT-23-5 land for a 3.3 V LDO (ME6211 / AP2112 / XC6220 pinout) from VSYS to +3V3.
  - If the TPS63802 fails: lift its EN link, solder the LDO. That is today's circuit, with today's 3.6 V empty point.
- **MIDI:** a 0 ohm land from each loop's sink to its GPIO (as today, DNP), so a dead FET falls back to today's 3.3 V
  drive.
- **Host switch:** lift the SY6280's output link; Q9 / R63 as today.

## 7. Ground strategy (the user's question: jank must not bridge ground loops)

`mini2-2-scratch/pb/gnd/{gl,earth,clk}.py` uses the LINE IN front end of `sim/linein.lib` (generic models, its
docstring).

**LINE IN (the difference input):**
- The front end already works as a true difference input: LIN_GS feeds k = 5.6 / 15.6 of the sleeve voltage into the
  bias return, matching the summer's gain.
- The sweeps:
  - R39 barely changes the rejection (the sense compensates). It sets the loop current.
  - C67 2.2 nF across R66 (matching C32) is what fixes 1 .. 10 kHz.
  - C33 220 nF is already the 50 Hz optimum: 180 n gives 1.05 mV, 270 n 1.30 mV, against 0.65 mV at 220 n.
- Residual at the ADC pin from 100 mV of sleeve-to-GND difference, R39 100 ohm, C67 2.2 nF. Rejection is against the
  sleeve on GND, which puts 53.4 / 53.7 / 34.1 mV on the pin:

| Parts | 50 Hz | 1 kHz | 10 kHz |
|---|---|---|---|
| nominal | 0.65 mV, **38 dB** | 0.28 mV, **46 dB** | 1.73 mV, 26 dB |
| 1 % resistors, worst corner | 1.91 mV, 29 dB | 1.43 mV, 31.5 dB | 2.01 mV, 24.6 dB |
| 0.5 % | 1.25 mV, 32.6 dB | 0.78 mV, 36.8 dB | 1.87 mV, 25.2 dB |
| 0.1 % | 0.76 mV, 36.9 dB | 0.33 mV, 44.3 dB | 1.76 mV, 25.7 dB |
| caps +-10 % + R65 / R66 1 % opposed | 1.71 mV, 29.9 dB | 0.80 mV, 36.5 dB | 1.83 mV, 25.4 dB |

- **Achievable with Basic 1 % parts: ~38 dB typical at 50 Hz, ~29 .. 30 dB worst. 46 / ~31 dB at 1 kHz.**
- 40 dB at 50 Hz is the circuit's own ceiling: C30 / C31 / C33's time constants. Even 0.1 % resistors stop at 37 dB.
- Going further would need the AC coupling after a DC-coupled difference stage: a redesign of the front end, not
  proposed now.
- **Proposed:** R39 22 -> **100 ohm** (C25076, Basic) and **fit C67 2.2 nF** (C1531, Basic).
  - Both are already lands. RT39 (its twin) still grounds the sleeve solid.
  - D14 (SMAJ5.0CA) stays across the sleeve to GND, so ESD still clamps.

**The worst case** (`earth.py`): an earthed mixer on LINE IN, an earthed Eurorack case on CLK, an earthed computer on
USB.
- 50 Hz earth differences of 100 mV and 1 V rms between each pair, with the case in phase with the mixer or opposed.
- Cable shields 0.05 / 0.2 ohm, the USB ground 0.1 ohm. CLK high (5 V through 1 k) into the module's 100 k.
- Results:

| Sleeves | Earth difference | LINE IN loop | CLK loop | ADC hum 50 Hz | 1 kHz | 10 kHz | CLK hum at the module |
|---|---|---|---|---|---|---|---|
| hard (both 0.03 ohm) | 100 mV | 0.23 .. 1.25 A | same | -27 .. -41 dBFS | -27 .. -41 | -31 .. -45 | 28 .. 99 mV |
| today (R39 22, CLK hard) | 100 mV | 2 .. 7 mA | 0.3 .. 0.56 A | -58 .. -70 | -64 .. -79 | -52 .. -63 | 44 .. 69 mV |
| **proposed (R39 100, CLK 47 ohm)** | 100 mV | **1.0 mA** | **2.1 mA** | **-63 .. -64** | **-71** | **-55** | 99 mV |
| hard | 1 V | 2.3 .. 12.5 A | same | -7 .. -21 | -7 .. -21 | -11 .. -25 | 0.28 .. 0.99 V |
| today | 1 V | 20 .. 71 mA | 3 .. 5.6 A | -38 .. -50 | -44 .. -59 | -32 .. -43 | 0.44 .. 0.69 V |
| **proposed** | 1 V | **10 mA** | **21 mA** | **-43** | **-51** | **-35** | 0.99 V |

- **The lifted sleeves cut the loop currents through jank's ground about 100 .. 1000 times** (amps to milliamps).
- The ADC hum is then set by the input's own rejection, not by the cables. The uncorrected part, the shield's IR drop,
  is 0.05 .. 2 mV.
- **CLK / RST:** the module's input is single-ended to its own earth, so the earth difference lands on top of our
  gate, whatever the sleeve.
  - A hard sleeve only splits it with the USB ground (69 % remains at 0.2 ohm shields). A lifted one passes it whole.
  - At 100 mV: ±0.14 V on a 0 / 4.95 V gate. Harmless.
  - **At 1 V: ±1.4 V peak on the low level. A module with a ~1 V threshold could false-trigger.**
  - No sleeve value fixes that while USB ties jank to a different earth. Swept 0.03 / 10 / 47 / 100 ohm: 0.69 .. 0.99 V
    at the module, loop current 3 A / 97 / 21 / 10 mA.
- **Proposed for CLK / RST:** 47 ohm 0603 (Basic) per sleeve, with a twin land (a 0 ohm there grounds it solid: the
  escape hatch).
  - A -12 V fault (17 mA, `sim/cv.txt`) puts 0.8 V and 14 mW on it.
  - The gate level into 100 k changes by 2.5 mV.
- **Is USB as the one solid ground enough?** For the usual tens to hundreds of mV: yes, everything stays in the table's
  "proposed" rows.
  - A ferrite on VBUS / GND does nothing at 50 Hz (it only helps at RF). A bigger CLK sleeve resistor does not help
    either.
  - For large earth differences, the fix is to break the USB earth: run jank on the cell with Wi-Fi / Link or the jank
    radio, or use a USB isolator. The README says so (below).

## 8. For the user to decide

1. **The power section:** B + C (recommended), B alone (and live with the 3.6 V empty point and the unplug margin), or
   keep today's. B + C cost about +$14 .. 19 on the 10-board order (estimate).
2. **CLK / RST:** keep the pump (5.33 V into 10 k), or take them from the 5 V aux (4.55 V into 10 k, 4.95 V into 100 k)
   and drop U13's fee.
3. **MIDI:** a true 5 V loop with four FETs (a re-layout of the MIDI blocks), or keep today's 3.3 V drive.
4. **Host VBUS:** real 5 V with a set limit (SY6280), or keep the 2 mA "signal".
5. **The ground parts** (cheap, Basic; can go in this round independent of 1-4): R39 100 ohm, C67 2.2 nF fitted, CLK /
   RST sleeve resistors 47 ohm with twin lands.
