# SPDX-License-Identifier: GPL-3.0-only
"""jank mini 2.2 fresh floorplan: the board's parts as movable units (a part, or a resistor with its twin), the
fixed parts and keep-outs, and the cost (ratsnest crossings + weighted airwire length + pulls + block cohesion).
Coordinates: board mm, (0, 0) top left, 72 x 72. Rotation: KiCad's (degrees, counter-clockwise on screen)."""
import collections, json, math, os

HERE = os.path.dirname(os.path.abspath(__file__))
W = H = 72.0
EDGE = 0.6                      # courtyards this far inside the board edge
GAP = 0.15                      # courtyard to courtyard (make_board.auto_place's)
IRON = 0.8                      # every through-hole pad's copper + 0.8: no top part (make_board.IRON)
SCREEN = (16.8, 13.4, 59.2, 51.4)   # the 1.54" module on J10's 2.5 mm header (case/mini_case.scad SCREENS "s154")
TALL = {"U1", "D3", "D9"}   # >= 2.0 mm: not under the screen (the module 3.2 mm, the SMA diodes 2.0 .. 2.3)
LID_FEET = [(2.0, 70.0), (2.0, 65.5), (70.4, 67.6), (67.0, 71.0)]   # mini_case.scad lid_pads (d 2.4, Y = H - y)
BTN_C = (61.5, 62.2)
PIVOT_R = 1.5 + 0.3
REGION = {"J11": (0.6, 40.0, 14.0, 70.0)}   # (the beat LEDs' row at y ~54.6 since 2026-11-15: J11 above or below it)
if os.environ.get("J11_RIGHT"):              # (the "all MIDI on one wall" variant: J11 by the right wall)
    REGION = {"J11": (58.0, 36.0, 71.4, 66.0)}   # (hard: J11, the toggle's two wires, stays by the left wall)
# the module's escape corridors (its rot 0 frame: pins 1 .. 14 on the left at x -8.75, 15 .. 26 along the bottom at
# y 8.89, 27 .. 40 on the right at x 8.75; its courtyard (-9.695, -10.555, 9.695, 9.835)): no other part there, but
# the parts that belong at the pins (decoupling, the EN RC, the ADC cap, the USB resistors)
MOD_CORR = [(-7.6, 9.835, 7.6, 12.6), (-11.4, -9.6, -9.695, 8.4), (9.695, -9.6, 11.4, 8.4)]
MOD_EXEMPT = {"C16", "C17", "C7", "R8", "C8", "R24", "R25"}
MOD_X_MIN = 39.6   # (the module's centre: its body (18 mm) >= 4 mm from LED4's (x 25.5, 2.2 mm): the lid's light hole)
MOD_Y = 61.4                    # the module's centre y: its courtyard 51.57 .. 71.96 (under the screen's 51.4)

FIXED = {"U5", "J1", "J2", "J3", "J5", "J6", "J7", "J8", "J9", "J10", "J10B", "J13", "J14", "LED1", "LED2", "LED3", "LED4",
         "SW1", "SW3", "SW4", "SW5"}
SKIP = {"CT8"}                  # (C8's bottom twin: make_board puts it under C8)
TWINS = {"RT15": "R15", "RT55": "R55", "RT20": "R20", "RT57": "R57", "RT33": "R33", "RT34": "R34", "RT36": "R36",
         "RT37": "R37", "RT38": "R38", "RT51": "R51", "RT10": "R10", "RT11": "R11", "RT60": "R60", "RT61": "R61",
         "RT39": "R39", "RT120": "R120", "RT121": "R121", "RT93": "R93"}

BLOCKS = {
    "usb": ["R3", "R4", "JP3", "JP4", "R63", "R94"],
    "host": ["U17", "C95", "JP5", "JP6", "JP7", "R91", "R92", "R93", "D16", "Q9", "JP13"],
    "screen": ["JP1", "JP2", "C60", "R60", "R61"],
    "midi1j": ["U11", "R14", "R15", "Q10", "R124", "R126"],
    "midi1d": ["R54", "R55", "Q11", "R127"],
    "midi2j": ["U12", "R21", "R20", "Q12", "R125", "R128"],
    "midi2d": ["R56", "R57", "Q13", "R129"],
    "clk": ["U21", "C105", "D10", "R75", "R71", "C89", "R120"],
    "rst": ["D11", "R81", "R77", "C90", "R121"],
    "linfe": ["D7", "D14", "R39", "R36", "R37", "R38"],
    "linA": ["R30", "R31", "C30", "C31", "C32", "R32"],
    "linB": ["R65", "R66", "C65", "C66", "C67", "R67"],
    "linbias": ["C33", "R33", "R34", "R153"],
    "module": ["U1", "C16", "C17", "C7", "R8", "R16", "C82", "R24", "R25", "C8", "R10", "R11", "R88", "R89"],
    "recovery": ["J12"],
    "batt": ["U9", "C42", "R52", "Q2"],
    "charger": ["U8", "R51", "C40", "C41", "R86", "C27", "C91", "D3", "R151", "R152"],
    "path": ["J11", "Q1", "R50", "D9", "C72", "C92", "R62"],
    "conv3": ["U2", "L4", "C3", "C5", "JP12", "R140", "R141", "R146", "R142", "JP11", "R144", "U2B"],
    "conv5": ["U18", "L5", "C99", "C100", "C101", "R131", "JP10", "R134", "R135", "R154", "R136", "U19", "R138", "R139",
              "JP8", "R123", "JP9", "R145"],
    "ui": ["C96", "C102", "R40", "C98", "R98"],
    "buttons": ["C80", "C81", "C83"],
}
BLOCK_OF = {r: b for b, rs in BLOCKS.items() for r in rs}
LINE_IN = set(BLOCKS["linfe"]) | set(BLOCKS["linA"]) | set(BLOCKS["linB"]) | set(BLOCKS["linbias"])
CONVERTERS = {"U2", "L4", "U18", "L5"}
LIN_GAP = 10.0                  # the LINE IN networks >= 10 mm from both converters (the user's)

# airwire weight per net (default 1): the switch nodes and the converters' feeds short, the high-impedance LINE IN
# nodes and the USB pair short
NET_W = {"V3_L1": 8, "V3_L2": 8, "V5_SW": 8, "VREG": 3, "V5A": 2, "VSYS": 2, "BAT_P": 2, "VBUS": 2, "+3V3": 1.5,
         "UDP": 3, "UDN": 3, "USB_DP": 3, "USB_DN": 3, "LINE_IN": 4, "LINE_SENSE": 4, "LIN_A1": 3, "LIN_A2": 3,
         "LIN_B1": 3, "LIN_B2": 3, "LIN_VB": 2, "V3_FB": 4, "V5_FB": 4, "V5_FBL": 3, "V5_CL": 2, "VBAT_ADC": 3,
         "CHIP_PU": 2, "VUSB": 2, "CLK_N": 2, "RST_N": 2, "V3_EN": 1, "V5_EN": 1}
# pad to pad pulls (weight per mm): decoupling at its pin, the hatch lands by what they bypass
PULLS = [("C17.1", "U1.2", 4), ("C16.1", "U1.2", 5), ("C7.1", "U1.3", 3), ("C8.1", "U1.4", 4),
         ("C3.1", "U2.10", 6), ("C3.2", "U2.8", 4), ("C5.1", "U2.6", 6), ("C5.2", "U2.8", 4),
         ("C99.1", "U18.3", 6), ("C99.2", "U18.4", 3), ("C100.1", "U18.6", 6), ("C100.2", "U18.4", 4),
         ("C101.1", "U18.6", 4), ("C95.1", "U17.12", 4), ("C42.1", "U9.3", 3), ("C96.1", "LED1.4", 3),
         ("C102.1", "LED3.4", 3), ("C60.1", "JP1.3", 2), ("C40.1", "U8.4", 4),
         ("C41.1", "R86.2", 3), ("R123.2", "JP8.2", 3), ("U2B.1", "U2.10", 2), ("U2B.5", "U2.6", 2),
         ("D9.2", "Q1.3", 3), ("D9.1", "Q1.2", 3), ("C33.1", "R32.2", 2)]
X_W, L_W, B_W = 1.0, 0.12, 0.25   # per crossing, per weighted mm, per mm from the block's centroid


# the converter loops as rigid templates (the fix after round 1 left both switch nodes open, 2026-11-15): the IC at 0, 0
# (rotation 0 of its footprint), each part (ref, x, y, rotation) in its frame. The datasheets' layout rules: the
# inductor straight across the switch pins, the input and output capacitors at their pins with their ground ends
# together, the power pins facing the parts, not the board edge (the template's own parts sit there).
# TPS63802 (VSON-10, U2 at rotation 0: pins 6 VOUT, 7 L2, 8 PGND, 9 L1, 10 VIN down its right side, x +0.82; 1 EN,
#  2 MODE, 3 GND, 4 FB on the left): L4 upright across 9 / 7, C3 (CIN) over pin 10, C5 / C6 (COUT) under pin 6.
# TPS61023 (SOT-563, U18 at rotation 0: 4 GND, 5 SW, 6 VOUT down its left side x -0.75; 3 VIN, 2 EN, 1 FB down its
#  right): L5 straight out of pin 5, C100 / C101 (COUT) under pin 6 and its GND, C99 (CIN) on L5's input end.
# (Rotation: the parts' KiCad orientation, as Unit.gpads applies it.)
TEMPLATES = {"U2": [("L4", 3.05, 0.0, 270), ("C3", 1.3, -3.25, 0), ("C5", 1.3, 3.25, 0)],
             "U18": [("L5", -3.5, 0.0, 0), ("C100", -2.7, 2.9, 180), ("C101", -2.7, 5.25, 180), ("C99", -7.35, 0.0, 180)]}
OWNER = {m: a for a, mem in TEMPLATES.items() for m, *_ in mem}
FINE_PITCH = {"U17", "U18", "U2"}          # (a free 1 mm ring round the IC's own courtyard)
LOOP_PADS = {"L4.1", "L4.2", "C3.1", "C5.1", "L5.1", "L5.2", "C99.1", "C100.1", "C101.1"}   # (>= 2 ways out)
NO_ACCESS = {"U1", "J1", "U5"}             # (the module: its corridors; the USB-C and U5: drawn copper)
CONN_LANES = {"J2", "J3", "J5", "J6", "J7", "J13", "J14"}   # (their signal pins keep a lane into the board)


# the module's GPIO pins that may change function (the option "pin swaps"): the ADC nets stay on ADC1 (GPIO1 .. 10,
# not the strapping GPIO3); the digital nets go to any free GPIO but the strapping pins (GPIO3 / 45 / 46: pins 15, 26,
# 16), BOOT (GPIO0, 27), USB (13 / 14), UART0 (36 / 37). Pin -> GPIO: the WROOM-1U datasheet v1.8, Table 3-1
PIN_GPIO = {4: 4, 5: 5, 6: 6, 7: 7, 8: 15, 9: 16, 10: 17, 11: 18, 12: 8, 13: 19, 14: 20, 15: 3, 16: 46, 17: 9, 18: 10,
            19: 11, 20: 12, 21: 13, 22: 14, 23: 21, 24: 47, 25: 48, 26: 45, 27: 0, 28: 35, 29: 36, 30: 37, 31: 38,
            32: 39, 33: 40, 34: 41, 35: 42, 38: 2, 39: 1}
ADC_NETS = {"VBAT_ADC", "LINE_IN", "LINE_SENSE"}
ADC_PINS = {39, 38, 4, 5, 6, 7, 12, 17, 18}
DIG_NETS = {"RST_IO", "CLK_IO", "BTN_UP", "BTN_YES", "BTN_DOWN", "SCL", "SDA", "LIN_DET", "MIDI_TX", "MIDI_TX2",
            "LED_DATA", "VBUS_SENSE", "CC_INT", "HOST_EN"}
DIG_PINS = {4, 5, 6, 7, 8, 9, 10, 11, 12, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30, 31, 32, 33, 35, 38, 39}
# (pin 34, GPIO41: the 5 V boost's enable, fixed. The outputs (CLOCK, RESET, both MIDI loops, the host signal, the
#  LED data) only on pins with no power-up glitch (S3 datasheet v2.2 Table 2-2: GPIO1 .. 20) and no reset pull (GPIO39:
#  Table 2-1 note 7))
OUT_NETS = {"RST_IO", "CLK_IO", "MIDI_TX", "MIDI_TX2", "HOST_EN", "LED_DATA"}
OUT_PINS = {23, 24, 25, 28, 29, 30, 31, 33, 35}   # (GPIO21, 47, 48, 35..38, 40, 42: no power-up glitch, no reset pull)
NO_PULL_NETS = {"VBUS_SENSE"}                      # (its divider: not on GPIO39's reset pull-up)


def rot(dx, dy, a):
    t = math.radians(a)
    c, s = math.cos(t), math.sin(t)
    return (dx * c + dy * s, -dx * s + dy * c)


def rbox(b, a):
    pts = [rot(x, y, a) for x in (b[0], b[2]) for y in (b[1], b[3])]
    return (min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts))


class Unit:
    def __init__(self, ref, x, y, a, pads, boxes, fixed, side):
        self.ref, self.x, self.y, self.a = ref, x, y, a
        self.pads = pads            # [(name "REF.num", net, dx, dy)] local (a = 0)
        self.boxes = boxes          # [local box]
        self.fixed, self.side = fixed, side
        self.twin = None
        self.block = BLOCK_OF.get(ref)
        self.psize = {}             # name: (w, h, attr) local (a = 0)
        self.members = []           # (ref, dx, dy, rot): a template's parts, in this unit's frame

    def gpads(self, x=None, y=None, a=None, pads=None):
        x, y, a = (self.x if x is None else x), (self.y if y is None else y), (self.a if a is None else a)
        out = []
        for name, n, dx, dy in (self.pads if pads is None else pads):
            px, py = rot(dx, dy, a)
            out.append((name, n, x + px, y + py))
        return out

    def gboxes(self, x=None, y=None, a=None):
        x, y, a = (self.x if x is None else x), (self.y if y is None else y), (self.a if a is None else a)
        out = []
        for b in self.boxes:
            r = rbox(b, a)
            out.append((r[0] + x, r[1] + y, r[2] + x, r[3] + y))
        return out


def load(path=os.path.join(HERE, os.environ.get("CUR", "board.json"))):
    d = json.load(open(path))
    units, obst_c, obst_b = {}, [], []
    for ref, v in d.items():
        if ref in SKIP or ref in TWINS:
            continue
        fixed = ref in FIXED
        x, y, a = v["x"], v["y"], v["rot"]
        pads = []
        psz = {}
        for p in v["pads"]:
            num, net, px, py, attr, w, h, sz, drill = p
            if v["side"] == "B" or fixed:
                lx, ly = px - x, py - y
                if not fixed:
                    raise SystemExit("a movable bottom part: " + ref)
            else:
                lx, ly = rot(px - x, py - y, -a)
            if net:
                pads.append((f"{ref}.{num}", net, lx, ly))
            sw = (h, w) if (not fixed and v["side"] == "T" and round(a) % 180 == 90) else (w, h)
            key = f"{ref}.{num}"
            while key in psz:                    # (pads sharing a number, the USB-C's shell legs: each one kept)
                key += "'"
            psz[key] = (*sw, attr, net, lx, ly)
            if v["side"] == "B" or (attr != "smd" and fixed):
                if attr == "pth":
                    obst_c.append((px, py, sz / 2 + IRON))
                elif attr == "npth":
                    obst_c.append((px, py, drill / 2 + 0.5))
        b = v["box"]
        lb = (b[0] - x, b[1] - y, b[2] - x, b[3] - y) if fixed else rbox((b[0] - x, b[1] - y, b[2] - x, b[3] - y), -a)
        u = Unit(ref, x, y, 0.0 if fixed else a, pads, [lb] if v["side"] == "T" else [], fixed, v["side"])
        u.allpads = psz             # name: (w, h, attr, net, lx, ly), every pad (unconnected too)
        if v["side"] == "B":
            u.boxes = []
            u.bbox_bottom = b
        units[ref] = u
        if ref == "U1":
            ep = [rot(p[2] - x, p[3] - y, -a) for p in v["pads"] if p[0] == "41"]
            u.ep_local = (min(q[0] for q in ep) - 1.6, min(q[1] for q in ep) - 1.6, max(q[0] for q in ep) + 1.6,
                          max(q[1] for q in ep) + 1.6)
            u.pins = {}
            for p in v["pads"]:
                if p[0].isdigit() and int(p[0]) in PIN_GPIO:
                    u.pins[int(p[0])] = rot(p[2] - x, p[3] - y, -a)
            u.pin_of = {f"U1.{p[0]}": int(p[0]) for p in v["pads"] if p[1] and p[0].isdigit() and int(p[0]) in PIN_GPIO}
    for t, r in TWINS.items():      # the twin rides with its resistor: its own pads at its current offset, in r's frame
        v, m = d[t], d[r]
        u = units[r]
        dx, dy = rot(v["x"] - m["x"], v["y"] - m["y"], -m["rot"])      # (its current place, in r's frame: exact)
        for p in v["pads"]:
            num, net, px, py = p[:4]
            lx, ly = rot(px - m["x"], py - m["y"], -m["rot"])
            u.pads.append((f"{t}.{num}", net, lx, ly))
            w, h = p[5], p[6]
            u.allpads[f"{t}.{num}"] = ((h, w) if round(m["rot"]) % 180 == 90 else (w, h)) + (p[4], net, lx, ly)
        b = v["box"]
        u.boxes.append(rbox((b[0] - m["x"], b[1] - m["y"], b[2] - m["x"], b[3] - m["y"]), -m["rot"]))
        u.twin_rot = v["rot"] - m["rot"]
        u.twin = (t, dx, dy)
    # (the checks' rules the courtyards miss: J11's wire pads are through-hole (their iron room, 0.8 round the copper);
    #  a twin's copper 1.0 mm from other parts' pads (make_board: 0.35 between courtyards); C8's bottom twin's via, 2.0
    #  mm out past its pad 1 (make_board.bottom_twin))
    j = units["J11"]                             # (its courtyard grown to its pads' copper + the iron's 0.8)
    xs = [(dx - max(w, h) / 2 - IRON, dx + max(w, h) / 2 + IRON) for w, h, _a, _n, dx, _dy in j.allpads.values()]
    ys = [(dy - max(w, h) / 2 - IRON, dy + max(w, h) / 2 + IRON) for w, h, _a, _n, _dx, dy in j.allpads.values()]
    b0 = j.boxes[0]
    j.boxes = [(min([b0[0]] + [x[0] for x in xs]), min([b0[1]] + [y[0] for y in ys]),
                max([b0[2]] + [x[1] for x in xs]), max([b0[3]] + [y[1] for y in ys]))]
    for u in units.values():
        if u.twin:
            b = u.boxes[1]
            u.boxes[1] = (b[0] - 0.2, b[1] - 0.2, b[2] + 0.2, b[3] + 0.2)
    units["C8"].boxes.append((-2.5, -0.5, -1.5, 0.5))
    for anchor, mem in TEMPLATES.items():      # (the converter loops: one rigid unit each)
        a = units[anchor]
        for m, ox, oy, mr in mem:
            mu = units.pop(m)
            for nm, n, dx, dy in mu.pads:
                px, py = rot(dx, dy, mr)
                a.pads.append((nm, n, px + ox, py + oy))
            for nm, (w, h, attr, n, dx, dy) in mu.allpads.items():
                px, py = rot(dx, dy, mr)
                a.allpads[nm] = ((h, w) if round(mr) % 180 == 90 else (w, h)) + (attr, n, px + ox, py + oy)
            for b in mu.boxes:
                q = rbox(b, mr)
                a.boxes.append((q[0] + ox, q[1] + oy, q[2] + ox, q[3] + oy))
            a.members.append((m, ox, oy, mr))
    fixed_boxes = [b for u in units.values() if u.fixed and u.side == "T" for b in u.gboxes()]
    circles = obst_c + [(x, y, 1.2 + 0.12) for x, y in LID_FEET] + [(BTN_C[0], BTN_C[1], PIVOT_R)]
    hard_pulls(units)
    return units, fixed_boxes, circles


# hard pulls (2026-11-15, the user's "a part whose only non-GND nets go to one IC pin sits at that pin"): pad to pad
# within HARD_MM, checked in legal(). The rule: a two-pad R / C whose non-GND nets reach the pins of ONE IC (U*, Q*)
# and no other IC: each of its pads on such a net within HARD_MM of that pin; a part on a net with no IC that joins a
# pulled part (a divider's upper leg): within CHAIN_MM of it; plus the explicit list (decoupling on shared rails, the
# USB series resistors, the CC resistors at J1, the LEDs' caps)
HARD_MM, CHAIN_MM = 6.0, 4.0   # (the general rule; the explicit list below at HARD_EXPLICIT: 3 mm)
HARD_EXPLICIT = 3.5
BULK = {"C72", "C92"}   # (the 100 uF reservoirs on VSYS: within BULK_MM of their pin, not decoupling)
BULK_MM = 10.0
GND_RING = 0.8   # (mm round each converter template: room for its ground pour and stitching vias)
HARD_EXTRA = [("C16.1", "U1.2"), ("C17.1", "U1.2"), ("R8.2", "U1.3"), ("R24.2", "U1.14"), ("R25.2", "U1.13"), ("C40.1", "U8.4"),
              ("R86.1", "U8.3"), ("C41.1", "R86.2"), ("R3.1", "J1.A5"), ("R4.1", "J1.B5"),
              ("C96.1", "LED1.4"), ("C102.1", "LED3.4"), ("C42.1", "U9.3")]
HARD = []


def hard_pulls(units):
    owner = {}
    net_pads = collections.defaultdict(list)
    for r, u in units.items():
        for nm, (_w, _h, _a, net, _dx, _dy) in u.allpads.items():
            owner[nm.rstrip("'")] = r
            if net:
                net_pads[net].append(nm.rstrip("'"))
    ic = lambda nm: nm.split(".")[0].startswith(("U", "Q"))
    del HARD[:]
    done = set()
    for r, u in units.items():
        parts = {}
        for nm, (_w, _h, _a, net, _dx, _dy) in u.allpads.items():
            ref = nm.split(".")[0]
            if ref.startswith(("RT", "CT")) or not ref.startswith(("R", "C")):
                continue
            parts.setdefault(ref, []).append((nm.rstrip("'"), net))
        for ref, pads in parts.items():
            if len(pads) != 2:
                continue
            sig = [(nm, n) for nm, n in pads if n and n != "GND"]
            pins = {n: [q for q in net_pads[n] if ic(q) and q.split(".")[0] != ref] for _nm, n in sig}
            ics = {q.split(".")[0] for qs in pins.values() for q in qs}
            if len(ics) != 1:
                continue
            if any(q.split(".")[0].startswith("SW") for _nm, n in sig for q in net_pads[n]):
                continue                               # (its net also goes to a button: it sits by the button)
            for nm, n in sig:
                for q in pins[n]:
                    if owner[q] != owner[nm]:          # (not inside one template: its own geometry holds)
                        HARD.append((nm, q, HARD_MM))
                        done.add(ref)
    for _pass in range(3):                             # (the chains: a net with no IC, two parts, one of them pulled)
        for net, pads in net_pads.items():
            if net == "GND" or any(ic(q) for q in pads) or len(pads) != 2:
                continue
            a, b = pads
            ra, rb = a.split(".")[0], b.split(".")[0]
            if not (ra.startswith(("R", "C")) and rb.startswith(("R", "C"))):
                continue
            if (ra in done) != (rb in done) and owner[a] != owner[b]:
                HARD.append((a, b, CHAIN_MM) if ra in done else (b, a, CHAIN_MM))   # (the pulled part first)
                done |= {ra, rb}
    refs = owner_refs(units)
    tight = {"R140", "JP11", "R142", "R146", "R134", "R135", "C95", "C42", "C7", "R8", "C8"}   # (EN, MODE, FB, decoupling)
    for i, (a, b, m) in enumerate(HARD):
        if a.split(".")[0] in tight and m != CHAIN_MM:
            HARD[i] = (a, b, HARD_EXPLICIT)
    for i, (a, b, m) in enumerate(HARD):
        if a.split(".")[0] in BULK:
            HARD[i] = (a, b, BULK_MM)
    for a, b in HARD_EXTRA:
        if a.split(".")[0] in refs and b.split(".")[0] in refs:
            HARD.append((a, b, HARD_EXPLICIT))
    MOD_EXEMPT.update(a.split(".")[0] for a, b, _m in HARD if b.startswith("U1."))


def owner_refs(units):
    out = set()
    for u in units.values():
        out |= {nm.split(".")[0] for nm in u.allpads}
    return out


def box_dist(a, b):
    return math.hypot(max(a[0] - b[2], b[0] - a[2], 0), max(a[1] - b[3], b[1] - a[3], 0))


def overlap(a, b):
    return a[0] < b[2] and b[0] < a[2] and a[1] < b[3] and b[1] < a[3]


def box_circle(b, c):
    return math.hypot(max(b[0] - c[0], 0, c[0] - b[2]), max(b[1] - c[1], 0, c[1] - b[3])) < c[2]


def mst(pts):
    """the nets' airwires as xeval.py draws them: Prim from the first pad"""
    if len(pts) < 2:
        return []
    done, rest, out = [pts[0]], list(pts[1:]), []
    best = {i: (math.dist(pts[0], p), 0) for i, p in enumerate(rest)}
    while rest:
        i = min(best, key=lambda k: best[k][0])
        d, j = best.pop(i)
        p = rest[i]
        out.append((done[j], p))
        done.append(p)
        rest[i] = None
        for k in best:
            q = rest[k]
            dd = math.dist(p, q)
            if dd < best[k][0]:
                best[k] = (dd, len(done) - 1)
        if not best:
            break
    return out


def cross(a, b, c, d):
    o = lambda p, q, r: (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    return o(a, b, c) * o(a, b, d) < 0 and o(c, d, a) * o(c, d, b) < 0
