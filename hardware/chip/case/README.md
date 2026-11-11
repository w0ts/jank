# jank mini case

Printed case for the jank chip board (`../chip.kicad_pcb`, 44.0 x 51.5 x 1.6 mm), in two versions from one
parametric file: **without** the DIN sockets (3.5 mm jacks only) and **with** J5 / J6 (`din = true`). Ideas taken from
the big jank box (`hardware/box` on the `hardware` branch): DIN plates in slots in the wall (front cheek, back cheek, no
wall behind the body), a stepped wall (the lid's skirt holds the jacks, the base the DINs), a cap through the lid for
the button.

> **Stage: designed and checked in OpenSCAD only. Nothing printed, no part measured.** See "Untested".

![open](renders/open_din.png)

## Files

| File | What |
|---|---|
| `mini_case.scad` | The case (`part` and `din` pick the output) and its checks |
| `make_stl.sh` | Exports the STLs, runs the collision checks for both versions, checks every STL is one closed solid, renders the PNGs |
| `place_args.py` | Reads SW1 and LED1 from `../make_board.py` (`PLACE`) for the scad |
| `stl/mini_base.stl` / `stl/mini_base_din.stl` | The base, without / with the DINs (print orientation, floor down) |
| `stl/mini_lid.stl` | The lid, shared by both versions (print orientation, roof down) |
| `stl/mini_button_cap.stl`, `stl/mini_led_pipe.stl` | The button cap and the light pipe (print 1 each) |
| `renders/closed.png`, `closed_din.png`, `open.png`, `open_din.png` | Closed, and the board in the base with the lid lifted |
| `renders/section_din.png`, `section_jack.png` | Sections through a DIN's axis (y = 20) and through the jacks' axis (y = 5.8), DIN version |

`chip_case.scad`, `stl/chip_case_*.stl` and `assembly.png` belong to the older tiny board and are **superseded** (stale positions).

```
sh hardware/chip/case/make_stl.sh        # about 40 s; BOARD_PY=<make_board.py> to read SW1 / LED1 from another copy
```

## Size

| | Outer (X x Y x Z) | Parts |
|---|---|---|
| **No DIN** | **47.6 x 55.1 x 17.3 mm** | base 4.2 high, lid 13.9 high (they overlap 0.9 in the lap) |
| **DIN** | **47.6 x 55.1 x 35.4 mm** | base 22.3 high, same lid |

The jacks' bushings stand 0.45 mm proud of the side faces (the USB-C is recessed behind a 13 mm opening in the top
face). The board edge is 1.8 mm inside the outer face on every side (0.4 side play + 1.4 wall).

## How the board is held

No screw, no insert, no metal: the antenna (bottom edge) needs about 10 mm of air round it, and the board has no mounting holes.
- **Under:** eight small pads in the base (0.05 mm under the board's underside, along the four edges, none under any
  part). With the DINs, the plates hang from the board and stand 0.35 mm over the floor; the DIN base's ribs and back cheeks stop 0.35 mm under the board.
- **Over:** four feet on the lid (2.4 mm round, bottom corners and sides, clear of every part) 0.1 mm over the board's top.
  The board is held between them with 0.1 mm of vertical play and 0.4 mm each side.
- **The lid** is a cap over the board: a half-lap joint (the lid's 0.7 mm skirt outside, the base's 0.55 mm lap inside, 0.15 mm
  play) and six snap bumps (0.35 mm high, 6 mm wide, on the base's lap, into pockets in the skirt: 2 on the top/bottom
  edges each, 1 on each side). If they do not hold, a drop of glue on the lap or a turn of tape closes it.
- **Antenna:** the base's pads at the antenna edge are plastic, 17 mm from the antenna (x 3..9 and 35..41); the wall there is 1.4 mm PLA/PETG. No metal anywhere.
- **Jacks:** J2 / J3's front blocks (6.0 x 5.5 mm, 2.2 mm past the edge) go through slots in the lid's side walls, open downwards (the lid is
  put on from above); the body's front pushes on the wall beside the slot (0.75 mm lip each side).
- **DINs:** each plate (20.9 x 19.6 x 1.8) slides down into a slot in the base between a front cheek (1.4 mm, in the
  wall; its window leaves 18.3 x 17.4 mm of the face open) and a back cheek (2.0 mm, a partition with an 18.6 mm opening for the body).
  Pull-out loads the front cheek, push-in the back cheek, the floor takes the weight.
- **USB-C:** a 13 x 6.9 mm opening (open downwards, also through the base's wall) for a plug's overmold.
- **Button:** a cap with a 3.4 mm stem through the roof (3.8 mm hole), a 4.6 mm flange 0.2 mm under the roof, resting 0.4 mm over the switch's plunger.
- **LED:** the lid has a 3.4 mm chimney down to 3.3 mm over the board and a 1.95 mm bore; the **light pipe** (1.8 mm,
  its head in a recess flush with the roof) sits 0.5 mm over the LED. A 1.75 mm filament offcut works instead.

## Print

PLA or PETG, 0.2 mm layers, 0.4 mm nozzle, 3 perimeters, 20 % infill, **no supports**:
- **Base:** floor down as exported. The DIN version's front window is bridged over its top (18 mm): turn the cooling up.
- **Lid:** roof down as exported (the slots and the USB opening face up). The chimney and the feet grow upward.
- **Button cap:** flange down. **Light pipe:** head down; a translucent / natural filament lights better. Both are small: print a spare.
- Print the two caps with the case, expect to sand the stem of the cap and the light pipe to fit the first time.

## Assembly

**Without DINs:** 1. Drop the board into the base (top up; the USB-C end at the top, the antenna at the pads' end). 2. Put the button cap, the stem up, into the lid's hole from inside and the light pipe from outside (head in its recess).
3. Put the lid on from above, the skirt over the base's lap, until the bumps click.

**With DINs:** 1. Hand-solder J5 / J6 (README "DIN sockets" of `../README.md`): their pins stand 2.4 mm up through the top side. 2. Drop the board with the sockets into the base: each plate goes down its slot, the bodies through the back cheek's opening. 3. As above.

## Checks (`make_stl.sh`, both versions, last run on the board of aaf494b)

The board model is `../mockup.stl` (block per courtyard, JLC-origin heights), plus the jacks' front blocks, the USB-C legs / jack pegs under the board (assumed 1.5 mm), and each DIN (plate, body and pin tails from `lib/3d/CONN-TH_DIN-504B-M10.wrl`: plate 20.9 x 19.6 x 1.8, body 17.9 x 17.6, 15.3 deep from the face, pins 4.0 up).

| Check | Must be | Result |
|---|---|---|
| base vs the board and every part, 0.3 mm clearance | empty | empty (tightest 0.30 to 0.35: the board's side edge, the jack pegs) |
| lid (without its feet) vs the board and every part, 0.3 mm | empty | empty (tightest 0.30 to 0.35: the jacks' slots) |
| button cap vs the board 0.3 / vs the lid 0.1; light pipe vs the board 0.3 / vs the lid 0.05 | empty | empty (cap 0.4 over the switch, pipe 0.5 over the LED) |
| lid vs base (lap, bumps, pockets) 0.05 | empty | empty |
| base pads vs the board 0.0 / lid feet vs the board 0.0 | empty | empty |
| base pads / lid feet vs the board 0.12 | **not** empty | not empty (they hold the board within 0.12) |
| every STL | one closed solid | 1 body, 0 open / non-manifold edges |

Clearances are found by growing the board model by a ball of the stated size and intersecting (`-D clr_check=0.4` finds a tighter spot).

## Untested

- **Nothing printed.** The fit of the lap, the bumps (0.35 mm: may be too stiff or too loose), the 0.4 mm board play, the cap and pipe press-fit all need a print. Print the lid's lap and a base strip first.
- **The DINs:** body depth 15.3 mm, 17.9 x 17.6 mm body, pin positions come from the 3D model (the HOOYA drawing gives plate 20.9 x 19.6 and 14.8 mm to the farthest pins; the jank box's scaled 13.1 mm body depth does not match the model); no real part was measured. 0.35 mm side/floor play, 1.3 mm side rims, 2.2 mm far rim.
- **Under the board:** tails 1.5 mm assumed (3D models: USB-C legs 0.9, jack pegs 0.55); the floor is 1.9 mm under the board without DINs.
- **The jacks' front blocks** (6.0 x 5.5 x 2.2) come from the 3D model of the PJ-320D, not the part. Bushings stand only 0.45 mm proud of the face: a plug with a wide collar may need the wall thinned there.
- **USB-C:** the plug's overmold is assumed <= 12.5 x 6.5 mm. The switch's plunger height (1.5 mm) is from the footprint's model.
- **The button and LED positions** (SW1 11.8, 37.8; LED1 11.2, 31.0) are those of commit aaf494b. If `make_board.py` moves them, run `make_mockup.py`, then `make_stl.sh` (it reads them from `PLACE`); the lid's chimney and button hole follow, the checks rerun.
- Nothing about the antenna was measured: the case is plastic only, 1.4 mm wall, at the antenna's edge.
