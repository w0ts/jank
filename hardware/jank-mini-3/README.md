# jank-mini-3: the LCD-2 add-on (schematic stage, 2026-11-17; not placed, not routed, not ordered)

A removable add-on board for a Waveshare ESP32-S3-LCD-2 (sku 30306). The LCD-2 plugs in on its own two 1 x 14
male headers into J15 / J16 and is held by 4 x M2 screws from below. Nothing is soldered to the LCD-2. The plan,
cost, sketches and bench tests are in `worktrees/lcd2-addon-scratch/` (README.md there).

| file | what |
|---|---|
| `parts.py` | the one parts table: every part, value, LCSC code, pad -> net, the GPIO map, the LCD-2's rows |
| `make_schematic.py` | writes `lcd2.kicad_sch`, `lib/chip.kicad_sym`, `fab/intended-nets.json` (mini 2.2's generator) |
| `check_nets.py` | the KiCad netlist against parts.py, plus the pin rules (LCD-2 rows, ADC1, glitch / pull rules, pull-downs, boost < clamp) |
| `sim/` | `power.py`, `clamp.py`, `host.py`, `midi_cv.py`, `linein_esd.py` and their `.txt` results |
| `lcd2-schematic.pdf` | the sheet (one label per pin: it reads as a netlist) |

Rebuild and check (KiCad 10, ngspice 47, Python with numpy):

```bash
cd hardware/jank-mini-3 && python3 make_schematic.py && kicad-cli sch erc --severity-all -o fab/erc.rpt lcd2.kicad_sch && kicad-cli sch export netlist --format kicadxml -o fab/lcd2.net.xml lcd2.kicad_sch && python3 check_nets.py fab/lcd2.net.xml fab/intended-nets.json
```

```bash
cd hardware/jank-mini-3/sim && for s in power clamp host midi_cv linein_esd; do python3 $s.py > /dev/null || echo FAIL $s; done
```

USB (mots, 2026-11-17): the LCD-2's own USB-C sits at the case's side wall and is used directly for a computer or a
charger. For host mode a standard short cable outside the box joins it to J1 (IN, USB-C, VBUS and CC open); D+ / D-
run straight to J4 (OUT, USB-A female), whose VBUS carries only the host signal (R93 / D16 from U21).

OUT gives hosted gear only a 680 ohm host signal on VBUS, never power (mots, 2026-11-17: the small cell is not drained). Bus-powered gear needs its own power or a powered hub.

## jank-mini-3 layout (mots, 2026-11-18: placed, not routed)

Board 93 x 72 mm with a cut-out for the LCD-2 (its PCB level with ours, LCD-2 centred). Two bridge strips (small
PCBs, two 1 x 14 female sockets 3.81 mm apart, not drawn yet) join its pins to our two 1 x 14 male rows J15 / J16,
mounted under the board 3.81 mm outside its rows. No screws: 4 pegs under the lid press its corners down. The 2 cells
lie flat on the case floor under it (2.3 mm spare under the strips). Under the board, hand-soldered by mots: the
DINs, the jacks, J15 / J16, the cell sockets J18 / J19 (side-entry PH2.0, part not picked). On top, by JLC: the
USB-Cs and every SMD part. Walls: MIDI (2 DINs + 2 TRS) top, our USB-Cs left (J4 above, J1 below the LCD-2's own),
CLOCK / RESET right, LINE IN bottom.

```bash
cd hardware/jank-mini-3 && BOARD=$PWD/.t/mini3.kicad_pcb /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 make_board.py && /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 tools/mini3_check.py .t/mini3.kicad_pcb
```

Not done yet: the bridge strip board, the case (cut-out, lid pegs, lower wall openings), routing.

## Cells (proposal, 2026-11-18, waiting for mots's pick)

mots's 1S LiPo cells (3.7 V, 4.2 V full; 450 or 550 mAh, 6 x 17 x 60 mm, PH2.0 lead), 2 of them standing on their long edge (12 x 17 tall x 60) along the bottom wall.
They plug into 2 PH2.0 sockets on this board (J18, J19; always both cells, never 1: the LCD-2 charges at 2 A), each through its own 0.33 ohm 2 W,
joined to VBAT (the LCD-2's header pin; its J4 stays empty). One DW01A + FS8205A protects the joined cells (sim/cells.txt).
- **Never plug in cells at different voltages.** Charge them to the same state first, all full or all on the unit. The 0.33 ohm per socket only limits the current between cells (~1.5 A at a 1 V difference) if this is forgotten.
- The protector cuts at 2.5 V, 4.3 V and ~1.6-2 A out; the firmware still stops first, at a healthier cell voltage. The cells' minus (B-) is not GND.
- The LCD-2's charger stops at 4.2 V (a match for these cells) and charges at up to 2 A (ISET 82k), shared by the cells.
