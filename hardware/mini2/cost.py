# SPDX-License-Identifier: GPL-3.0-only
"""An estimate of what 5 or 10 assembled jank mini 2.2 boards cost at JLCPCB, STANDARD assembly (one side: every ESP32-S3
module at JLC is "Standard only"), anchored on the first real JLCONE quote (2026-11-15), from the parts
snapshot (fab/parts-snapshot.json, jlc_parts.py) and the assembly summary (fab/summary.json, make_assembly.py). Writes
fab/cost.md. Nothing here is a quote: every fee says where it was seen. Shipping is left out: it is shared with other
boards in one JLC order.

  python3 cost.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
USD_EUR = (0.86, "USD to EUR, an ASSUMED rate (not read on the day): check it")
QUOTE = (233.20, "JLCONE, 2026-11-15 17:58: the dummy board (placed, no tracks), 10 pcs, battery order, top side, "
         "Standard; 57 part types: PCB 6.40, setup 25.75, stencil 8.27, parts 94.56, feeders loading 88.35, SMT 9.36, "
         "packaging 0.51; shipping and the hand-soldered parts not included")
STD = {   # USD, Standard PCBA, read off that quote
    "setup": (25.75, "Standard setup, the JLCONE quote 2026-11-15"),
    "stencil": (8.27, "Standard stencil, same quote"),
    "feeder": (1.55, "feeder loading per part type, every type on Standard: 88.35 / 57 in that quote (JLC's price page: ~1.53)"),
    "joint": (9.36 / 4440, "SMT per joint: 9.36 for 10 x 444 joints in that quote"),
    "packaging": (0.51, "packaging, same quote"),
    "pcb_10": (6.40, "bare 2-layer 72 x 72 mm board, 10 pcs: the JLCONE quote 2026-11-15 (place2 is 2 layers; "
                      "4 layers were 19.30, JLCONE 2026-11-16)"),
}
FEES = {   # USD; where each figure comes from (Economic, the older tier: kept for the history notes)
    "econ_setup": (8.24, "Economic setup fee, JLCONE test quote with the bare ESP32-S3FN8 (2026-11-10)"),
    "econ_stencil": (1.55, "Economic stencil, same quote"),
    "nitrogen": (0.91, "nitrogen reflow, the jank-pcb-mod quote 2026-11-09 (JLC added it; may be optional)"),
    "extended": (3.09, "per unique Extended part type, same test quote; 'preferred' Extended parts are exempt"),
    "joint": (0.0017, "per solder joint (JLCPCB's published Economic rate)"),
    "pcb_5": (3.30, "bare 2-layer 1.6 mm board, 5 pieces, as in the 2026-11-09 quote (87 x 79.5 mm; a smaller board is not dearer)"),
    "pcb_10": (5.00, "bare 2-layer 1.6 mm board, 10 pieces: an ESTIMATE, not seen in a quote"),
    # mini 2.2: the through-hole parts (the jacks, the encoder), soldered by hand: JLCPCB help "PCB Assembly Cost: What
    # Does the Price Include?" (jlcpcb.com/help/article/pcb-assembly-price, "Last updated on Oct 22, 2026", read 2026-11-14)
    "manual_joint": (0.0164, "manual assembly per joint (1..10,000 joints), Economic and Standard alike, JLCPCB help page updated 2026-10-22"),
    "hand_labor": (3.58, "hand-soldering labour per order, same page"),
    "std_setup": (25.56, "Standard PCBA setup, single side, same page (if JLC will not take through-hole parts on Economic)"),
    "std_stencil": (8.21, "Standard PCBA stencil, single side, same page"),
}


# stage 2: the parts the user solders (DNP on the board), bought from LCSC with the JLC order, not assembled:
# LCSC code: (a board, what, which build needs it). LCSC's unit price at the quantity; no JLC loss; shipping shared.
USER_PARTS = {
    "C16684": (3, "PJ-307C switched TRS jack (MIDI OUT 1, MIDI OUT 2, LINE IN)", "every board"),
    "C2939346": (2, "DIN-504B-M10 5-pin DIN socket", "the DIN option"),
    "C131337": (2, "JST B2B-PH-K-S vertical PH2.0 socket (the cells, J8 / J9 on the bottom)", "the battery order"),
    "C2691448": (1, "1 x 4 2.54 mm header (the screen)", "the screen option"),
    "C1693673": (1, "HJ-2.4G-IPEX1-L=80mm FPC antenna, u.FL lead (its size ASSUMED ~40 x 10)", "every board"),
    "C2885782": (1, "RS601HL-1010011BB snap-in rocker, black, 3 A (the power switch, in the case wall, ~10.5 x 15 mm cut-out; "
                    "two wires to J11; the user's choice 2026-11-15, was the MTS-102 toggle)", "the battery order"),
    "C18185602": (2, "PJ-320A 3.5 mm jack (CLOCK and RESET out)", "every board"),
}
SCREEN = (6.97, "the 1.54 inch module, HS154L03W2C01 (C7465999), LCSC's 1-piece price on the snapshot's day")
# 72 x 72 (the user's "one board, different screens"; the 0.96 in is out: unreadable): the screen modules, one per board, the user's choice; bought with
# the order, plugged into J10 (0.96 / 1.3 / 1.54) or on a lead to J10B (2.42); hardware/mini2/screen-study.md
SCREEN_PARTS = {
    "C7465999": ("1.54 in, SSD1309, HS154L03W2C01 (J10)", "screen 1.54"),
    "C7466000": ("2.42 in, SSD1309, HS242L03B2C01 (J10B, a 4-wire lead; colour unclear: screen-study.md)", "screen 2.42"),
}


def unit_price(prices, qty):
    best = prices[0][1]
    for start, p in prices:
        if qty >= start:
            best = p
    return best


def estimate(summary, snap, n):
    lines = []
    for code, per_board in sorted(summary["qty"].items()):
        s = snap["parts"][code]
        qty = per_board * n + s["loss"]
        price = unit_price(s["prices"], qty)
        lines.append((code, s, per_board, qty, price, qty * price))
    ext = [c for c in summary["qty"] if snap["parts"][c]["type"] == "Extended" and not snap["parts"][c]["preferred"]]
    types = len(summary["qty"])
    rows = {
        f"PCB (bare, {n})": FEES["pcb_5"][0] if n <= 5 else STD["pcb_10"][0],
        "Standard setup": STD["setup"][0],
        "Standard stencil": STD["stencil"][0],
        f"feeder loading ({types} part types x {STD['feeder'][0]})": types * STD["feeder"][0],
        f"SMT ({summary['joints']} joints a board)": summary["joints"] * n * STD["joint"][0],
        "packaging": STD["packaging"][0],
        "parts (with JLC's loss)": sum(x[5] for x in lines),
    }
    return rows, sum(rows.values()), lines, ext


def tht_routes(summary, snap, runs):
    """mini 2.2: the through-hole parts (summary "tht", from make_assembly.py) under the two ways of soldering them. The
    table above counts their joints at the SMD rate and their Extended types at the fee, as for every part; here: (A) JLC
    solders them by hand: their joints at the manual rate plus the labour per order (and, if JLC moves the order to
    Standard for them, Standard's setup and stencil); (B) the user solders them: they leave the JLC order (parts, fees,
    joints) and are bought separately at the same unit price (no JLC loss; shipping not counted)"""
    tht, tj = summary.get("tht", {}), summary.get("tht_joints", 0)
    if not tht:
        return []
    names = ", ".join(f"{c} x{q} ({snap['parts'][c]['mpn']})" for c, q in sorted(tht.items()))
    out = ["## Through-hole parts: who solders them", "", f"Through-hole parts a board: {names}; {tj} joints a board.", "",
           "| | 5 boards, USD | 10 boards, USD |", "|---|---|---|"]
    rows = {}
    for n in (5, 10):
        _rows, total, lines, ext = runs[n]
        a = total + tj * n * (FEES["manual_joint"][0] - FEES["joint"][0]) + FEES["hand_labor"][0]
        a_std = a + FEES["std_setup"][0] - FEES["econ_setup"][0] + FEES["std_stencil"][0] - FEES["econ_stencil"][0]
        parts_tht = sum(x[5] for x in lines if x[0] in tht)
        fees_tht = sum(FEES["extended"][0] for c in ext if c in tht)
        b = total - parts_tht - fees_tht - tj * n * FEES["joint"][0]
        buy = sum(unit_price(snap["parts"][c]["prices"], q * n) * q * n for c, q in tht.items())
        rows[n] = (total, a, a_std, b, buy)
    labels = ["the table above (THT joints at the SMD rate)", "**A: JLC hand-solders them** (Economic; manual joints + labour)",
              "A, if JLC moves the order to Standard (+ setup and stencil; Standard's feeder fees not counted)",
              "**B: the user solders them**: the JLC order without them", "B: the through-hole parts bought separately (+ shipping)"]
    for i, label in enumerate(labels):
        out.append(f"| {label} | {rows[5][i]:.2f} | {rows[10][i]:.2f} |")
    out.append(f"| **B in all** (+ shipping) | **{rows[5][3] + rows[5][4]:.2f}** | **{rows[10][3] + rows[10][4]:.2f}** |")
    out += ["", "Whether JLC's Economic assembly takes through-hole parts is not settled: JLC's prototype-assembly page lists "
            "\"SMT, through-hole and mixed-technology assembly\" for both kinds, users report the opposite; the online quote decides. "
            "Whether a through-hole part pays the Extended fee is not stated either (counted here, as above).", ""]
    return out


VARIANTS = ("usb", "battery")   # (mini 2.2: parts.VARIANTS; make_assembly.py writes fab/<variant>/summary.json)


def main():
    snap = json.load(open(os.path.join(HERE, "fab", "parts-snapshot.json")))
    eur = USD_EUR[0]
    sums = {v: json.load(open(os.path.join(HERE, "fab", v, "summary.json"))) for v in VARIANTS}
    allruns = {v: {n: estimate(s, snap, n) for n in (5, 10)} for v, s in sums.items()}
    w, h = sums[VARIANTS[0]]["board_mm"]
    out = [f"# jank mini 2.2: cost estimate, both orders (generated by cost.py; parts snapshot {snap['date']}, newer parts"
           " read later: their 'read' field)", "",
           f"{w:g} x {h:g} mm, 2 layers (1.6 mm), top side only, **JLC Standard** (every ESP32-S3 module at JLC is Standard only). "
           f"**Anchor: {QUOTE[1]}: USD {QUOTE[0]:.2f}.** The table below re-prices today's BOM with that quote's fees; "
           "re-quote before ordering. One board, two orders: **usb** (USB power only)"
           " and **battery** (the cell circuit too; both orders the TPS63802 buck-boost and the TPS61023 5 V aux boost). Shipping: not included; JLC's checkout shows it. Taxes and the case are not included either. Every fee is an"
           " estimate (sources below).", "", "| | " + " | ".join(f"{v}: 5 boards | {v}: 10 boards" for v in VARIANTS) + " |",
           "|---|" + "---|" * (2 * len(VARIANTS))]
    for label, f in (("parts / lines / joints a board", lambda s, r: f"{s['parts']} / {s['lines']} / {s['joints']}"),
                     ("part types (feeder fee each)", lambda s, r: str(len(s["qty"]))),
                     ("**total, USD**", lambda s, r: f"**{r[1]:.2f}**"),
                     ("per board, USD", lambda s, r: None)):
        cells = []
        for v in VARIANTS:
            for n in (5, 10):
                c = f(sums[v], allruns[v][n])
                cells.append(c if c is not None else f"{allruns[v][n][1] / n:.2f}")
        out.append(f"| {label} | " + " | ".join(cells) + " |")
    out.append("")
    for v in VARIANTS:
        out += [f"## The {v} order", ""] + detail(sums[v], snap, allruns[v], eur)
    out += user_parts(snap, allruns, eur)
    out += ["## Sources", ""]
    out += [f"- {k}: USD {v:g}, {why}" for k, (v, why) in STD.items()]
    out += [f"- (Economic, history) {k}: USD {v:g}, {why}" for k, (v, why) in FEES.items()] + [f"- {USD_EUR[1]}: {eur}"]
    open(os.path.join(HERE, "fab", "cost.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:9]))


def user_parts(snap, allruns, eur):
    """the parts bought with the order (the user solders them) and the full-options cost a board"""
    out = ["## Bought with the order, the user solders (not assembled by JLC)", "",
           "| LCSC | part | for | a board | 5 boards: qty, USD | 10 boards: qty, USD |", "|---|---|---|---|---|---|"]
    tot = {}
    for code, (per, what, need) in USER_PARTS.items():
        s = snap["parts"][code]
        row = []
        for n in (5, 10):
            q = per * n
            usd = q * unit_price(s["prices"], q)
            tot[(code, n)] = usd
            row.append(f"{q}, {usd:.2f}")
        out.append(f"| {code} | {what} | {need} | {per} | " + " | ".join(row) + " |")
    out += ["", "### The screen modules (one per board, the user's choice; LCSC unit price at the quantity)", "",
            "| LCSC | module | 5 boards: USD each | 10 boards: USD each | vs the 1.54 in, 10 |", "|---|---|---|---|---|"]
    base10 = unit_price(snap["parts"]["C7465999"]["prices"], 10)
    for code, (what, _k) in SCREEN_PARTS.items():
        pr = snap["parts"][code]["prices"]
        out.append(f"| {code} | {what} | {unit_price(pr, 5):.2f} | {unit_price(pr, 10):.2f} | {unit_price(pr, 10) - base10:+.2f} |")
    out += ["", f"The full-options rows below take the 1.54 in ({SCREEN[1]}).", "",
            "### Full options, a board (USD): the JLC order + every part above + the screen module", "",
            "| | usb: 5 | usb: 10 | battery: 5 | battery: 10 |", "|---|---|---|---|---|"]
    def opt(v, n, codes, screen):
        return (allruns[v][n][1] + sum(tot[(c, n)] for c in codes) + (SCREEN[0] * n if screen else 0)) / n
    base_codes = ["C16684", "C18185602", "C1693673"]
    rows = (("JLC board only", lambda v, n: allruns[v][n][1] / n),
            ("+ the five jacks (3 PJ-307C, 2 PJ-320A) and the FPC antenna (battery: + 2 PH2.0 and the rocker): the minimum to use it",
             lambda v, n: opt(v, n, base_codes + (["C131337", "C2885782"] if v == "battery" else []), False)),
            ("+ the DINs", lambda v, n: opt(v, n, base_codes + ["C2939346"] + (["C131337", "C2885782"] if v == "battery" else []), False)),
            ("+ the DINs + the screen (header, module)",
             lambda v, n: opt(v, n, base_codes + ["C2939346", "C2691448"] + (["C131337", "C2885782"] if v == "battery" else []), True)))
    for label, f in rows:
        out.append(f"| {label} | " + " | ".join(f"{f(v, n):.2f}" for v in ("usb", "battery") for n in (5, 10)) + " |")
    out += ["", "(The battery order's cells are not included: the user's own.)", ""]
    out += split_builds(snap, allruns)
    return out


def split_builds(snap, allruns):
    """the coordinator's question: 10 boards, 5 full (cells, toggle, CLK / RST) and 5 MIDI-only (USB power, no cells, no
    toggle: R62's pads bridged by hand, "USB ONLY: BRIDGE"; no CV jacks): one battery order of 10, or two orders of 5"""
    full = {"C16684": 3, "C1693673": 1, "C18185602": 2, "C131337": 2, "C2885782": 1}
    midi = {"C16684": 3, "C1693673": 1}
    qty = {c: 5 * full.get(c, 0) + 5 * midi.get(c, 0) for c in full}
    buy = sum(q * unit_price(snap["parts"][c]["prices"], q) for c, q in qty.items() if q)
    one = allruns["battery"][10][1]
    two = allruns["usb"][5][1] + allruns["battery"][5][1]
    out = ["### 10 boards: 5 full + 5 MIDI-only (USD; the DINs and the screen left out of both)", "",
           "| | JLC | bought with the order (the user solders) | total | a board |", "|---|---|---|---|---|",
           f"| **one battery order of 10**, 5 of them finished by hand as MIDI-only (no cells, no toggle: R62's pads "
           f"bridged, no CV jacks; their charger, cell parts and CLK / RST drivers fitted but unused) | {one:.2f} | {buy:.2f} | "
           f"**{one + buy:.2f}** | {(one + buy) / 10:.2f} |",
           f"| **two orders**: usb x 5 (MIDI-only) + battery x 5 (full) | {two:.2f} | {buy:.2f} | **{two + buy:.2f}** | "
           f"{(two + buy) / 10:.2f} |", "",
           f"One order saves {two - one:.2f} USD (one set of setup / stencil / feeder fees instead of two). The "
           "MIDI-only boards from the battery order run from USB through the power path and the TPS63802 (no cell needed); "
           "R62 is a DNP 0402 land in that order: a blob or a 0 ohm 0402 bridges it. Shipping: one parcel either way "
           "(not included; JLC's checkout shows it).", ""]
    return out


def detail(s, snap, runs, eur):
    """one order's table, its parts (5 boards), its Extended types, its through-hole routes"""
    out = [f"{s['parts']} parts on {s['lines']} lines, {s['joints']} joints a board.", "",
           "| | 5 boards, USD | 10 boards, USD |", "|---|---|---|"]
    keys5, keys10 = list(runs[5][0]), list(runs[10][0])
    for k5, k10 in zip(keys5, keys10):
        out.append(f"| {k5.replace('(bare, 5)', '(bare)')} | {runs[5][0][k5]:.2f} | {runs[10][0][k10]:.2f} |")
    t5, t10 = runs[5][1], runs[10][1]
    out += [f"| **total** | **{t5:.2f}** (EUR {t5 * eur:.2f}) | **{t10:.2f}** (EUR {t10 * eur:.2f}) |",
            f"| **per board** | **{t5 / 5:.2f}** (EUR {t5 / 5 * eur:.2f}) | **{t10 / 10:.2f}** (EUR {t10 / 10 * eur:.2f}) |", "",
            "### Parts (5 boards)", "", "| LCSC | part | type | stock | a board | ordered (with loss) | unit USD | USD |",
            "|---|---|---|---|---|---|---|---|"]
    for code, sp, per, qty, price, cost in runs[5][2]:
        typ = sp["type"] + (" (preferred)" if sp["preferred"] else "")
        out.append(f"| {code} | {sp['mpn']} | {typ} | {sp['stock']} | {per} | {qty} | {price:.4f} | {cost:.2f} |")
    out += ["", f"Extended types that pay the fee: {', '.join(sorted(runs[5][3]))}", ""]
    out += [line.replace("## Through-hole", "### Through-hole") for line in tht_routes(s, snap, runs)]
    return out


if __name__ == "__main__":
    main()
