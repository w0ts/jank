# place2 vs buf-try placement (2026-11-16)

Measured with placer/measure.py (the same model, both new terms at their definitions in placer/rcost.py).

| | old (buf-try place.json) | place2 (k1 c0.5 t3 s2) | place2q (k1 c1.0 t1.5 s1) |
|---|---|---|---|
| ratsnest crossings | 240 | 237 | 234 |
| KEEP (non-LINE-IN airwire mm inside LINE IN's keep-out) | 255.1 | 39.9 | 47.6 |
| CONGESTION (sum of squared over-demand, 2 mm cells) | 373.7 | 71.0 | 59.9 |
| hard pulls over their distance | 0 | 0 | 0 |
| airwire length, no GND (mm) | 2198 | 2195 | 2227 |
| weighted length + pulls | 2990 + 550 | 3018 + 499 | 3024 + 518 |
| legal (placer rules) | JP8/JP9 0.150 mm apart (rounding) | yes | yes |
| make_board + check_board / check_nets | (buf-try) | 0 / 0 problems | 0 / 0 problems |

## How the new terms are defined
- **KEEP**: the region is make_board.inner_planes()'s rule-area formula: the LINE IN net pads (LIN_*, LINE_IN, LINE_SENSE;
  not LIN_DET, not U1), grouped 5 mm apart, each group's bounding box + 1.5 mm, recomputed as the parts move. An airwire's
  path is the mean of its two L-shaped paths.
- **CONGESTION**: 2 mm cells, demand along both L paths (0.5 each, power nets 2x), horizontal and vertical apart.
  Capacity is 2 layers x 2.5 tracks (half of the 0.4 mm pitch: a judgement, not measured), minus pads, floor 1 track
  per layer. Cost = sum of max(0, demand - capacity)^2.

## Hottest cells (over capacity; cell corner in mm)
- old: H (8,56) +5.4 BAT_P/VBUS/VLED; V (28,40) +4.5 and V (28,38) +4.1 +3V3/OLED/SDA/SCL/V5C/V5M; H (20,58) +3.9;
  H (38,6) +3.3 CC/VUSB; H (22,44) +3.1
- place2: H (36,6) and H (34,6) +2.5 (CC1/CC2/UDN/UDP/VUSB at the USB-C); H (18,44) +2.1; H (16,44) +1.9
- place2q: H (10,50) +3.0 (VREG/VSYS); V and H (22,50) +2.0 (+3V3/V3_FB/V3_MODE); H (36,6) and H (34,6) +2.0 (USB-C)

## How they were made
- 16 anneals on gpufarm from place.json (run.py 400k moves + polish): K_W 1|2, C_W 0.5|1.0, T0 1.5|3.0, 2 seeds.
- Legalised on the Mac with FIX_TOUCH=1 fix.py (two passes for these two).
- Weights in the cost: X_W 1, L_W 0.12, B_W 0.25 as before, plus K_W 1 per mm and C_W as named.

## Renders
- place2/render-old-top.png, place2/render-old-bottom.png
- place2/render-new-top.png, place2/render-new-bottom.png
- ../place2q/render-new-top.png, ../place2q/render-new-bottom.png
