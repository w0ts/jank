# jank mini 2.2: the MIDI outputs under single faults (midi_faults.py; the datasheet's figures and the models in its docstring)

Limits asked: the GPIO within 40 mA (Table 5-4's IOH; no per-pin maximum is stated), each 0402 within 1/16 W in a sustained short, +3V3 up, and >= 5 mA into a standard receiver at 3.0 V and 3.3 V.

### THE BOARD, the jacks (stage 2, the coordinator's decision: source three 100 ohm 1206 = 33.3 ohm, 0.75 W; sink 68 ohm 0805, 0.125 W): source 33.3333 ohm, sink 68 ohm

| fault (sustained, +3V3 3.47 V) | GPIO mA | P sink W | P source W |
|---|---|---|---|
| tip-GND, GPIO high | 41 | 0.113 |  |
| ring-GND |  |  | 0.361 |
| tip-ring, GPIO low | 31 | 0.064 | 0.031 |
| the twin output, tip short, GPIO high: its receiver's 'off' current | pin high sags to 2.78 V | the twin's receiver gets 0.00 mA while it should be off | |

| receiver | 3.0 V | 3.3 V |
|---|---|---|
| 220 ohm + 1.2 V opto | 5.35 mA | 6.23 mA |
| 220 ohm + 1.5 V opto | 4.68 mA | 5.55 mA |
| 220 ohm + 1.5 V opto + series diode | 2.87 mA | 3.70 mA |

### THE BOARD, the DINs (source two 100 ohm 1206 = 50 ohm, 0.5 W; sink 68 ohm 0805, 0.125 W): source 50 ohm, sink 68 ohm

| fault (sustained, +3V3 3.47 V) | GPIO mA | P sink W | P source W |
|---|---|---|---|
| tip-GND, GPIO high | 41 | 0.113 |  |
| ring-GND |  |  | 0.241 |
| tip-ring, GPIO low | 27 | 0.048 | 0.036 |
| the twin output, tip short, GPIO high: its receiver's 'off' current | pin high sags to 2.78 V | the twin's receiver gets 0.00 mA while it should be off | |

| receiver | 3.0 V | 3.3 V |
|---|---|---|
| 220 ohm + 1.2 V opto | 5.11 mA | 5.94 mA |
| 220 ohm + 1.5 V opto | 4.47 mA | 5.30 mA |
| 220 ohm + 1.5 V opto + series diode | 2.75 mA | 3.53 mA |

Every sustained single fault stays inside each part's rating on the board's values (a tip short 0.113 W in a 0.125 W sink; a ring or DIN pin-4 short 0.36 / 0.24 W shared by three / two 0.25 W 1206s); the GPIO's tip-short current is at the datasheet's IOH figure (40 mA typ, PAD_DRIVER 3: Table 5-4). The receivers get 5.5 mA (jacks) / 5.1 mA (DINs) at 3.3 V and 4.7 / 4.1 mA at 3.0 V into a 220 ohm + 1.5 V opto: accepted (the coordinator). **Firmware:** the MIDI pins at drive strength 2 (20 mA nominal): the pin's own driver then limits a short further; NOT modelled (the datasheet gives the drive per strength only as nominal currents, no output resistance): unverified.

### stage 1f (CA-033's values: source 33.3 / 33 ohm, sink 10 ohm 0603): source 33.3 ohm, sink 10 ohm

| fault (sustained, +3V3 3.47 V) | GPIO mA | P sink W | P source W |
|---|---|---|---|
| tip-GND, GPIO high | 129 | 0.165 |  |
| ring-GND |  |  | 0.362 |
| tip-ring, GPIO low | 63 | 0.039 | 0.131 |
| the twin output, tip short, GPIO high: its receiver's 'off' current | pin high sags to 1.29 V | the twin's receiver gets 3.76 mA while it should be off | |

| receiver | 3.0 V | 3.3 V |
|---|---|---|
| 220 ohm + 1.2 V opto | 6.44 mA | 7.50 mA |
| 220 ohm + 1.5 V opto | 5.63 mA | 6.68 mA |
| 220 ohm + 1.5 V opto + series diode | 3.42 mA | 4.41 mA |

### protection by resistors alone, 0402 (the smallest that keep every fault inside: source >= 174 ohm for a ring short, sink >= 138 ohm for a tip short at 1/16 W and <= 40 mA): source 180 ohm, sink 150 ohm

| fault (sustained, +3V3 3.47 V) | GPIO mA | P sink W | P source W |
|---|---|---|---|
| tip-GND, GPIO high | 21 | 0.065 |  |
| ring-GND |  |  | 0.067 |
| tip-ring, GPIO low | 10 | 0.015 | 0.019 |
| the twin output, tip short, GPIO high: its receiver's 'off' current | pin high sags to 3.12 V | the twin's receiver gets 0.00 mA while it should be off | |

| receiver | 3.0 V | 3.3 V |
|---|---|---|
| 220 ohm + 1.2 V opto | 3.23 mA | 3.74 mA |
| 220 ohm + 1.5 V opto | 2.83 mA | 3.35 mA |
| 220 ohm + 1.5 V opto + series diode | 1.79 mA | 2.28 mA |

Resistors alone into 0402s: **no set meets every limit.** Keeping 0402s inside 1/16 W in a ring or tip short needs a source >= 174 ohm and a sink >= 138 ohm; 5 mA into a 220 ohm + 1.5 V receiver at 3.0 V needs source + sink <= 68 ohm (1.5 V / 5 mA - 220 - 12): off by about 4.6 x. With a receiver that has a series diode (2.2 V in all), even 0 ohm gives only 3.6 mA at 3.0 V.

