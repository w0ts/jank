# SPDX-License-Identifier: GPL-3.0-only
"""An estimate of what 5 or 10 assembled jank chip ("jank mini") boards cost at JLCPCB, Economic assembly (one side), from the parts
snapshot (fab/parts-snapshot.json, jlc_parts.py) and the assembly summary (fab/summary.json, make_assembly.py). Writes
fab/cost.md. Nothing here is a quote: every fee says where it was seen. Shipping is left out: it is shared with other
boards in one JLC order.

  python3 cost.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
USD_EUR = (0.86, "USD to EUR, an ASSUMED rate (not read on the day): check it")
FEES = {   # USD; where each figure comes from
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
    rows = {
        f"PCB (bare, {n})": FEES["pcb_5" if n <= 5 else "pcb_10"][0],
        "Economic setup": FEES["econ_setup"][0],
        "Economic stencil": FEES["econ_stencil"][0],
        "nitrogen reflow": FEES["nitrogen"][0],
        f"Extended part types ({len(ext)} x {FEES['extended'][0]})": len(ext) * FEES["extended"][0],
        f"solder joints ({summary['joints']} a board)": summary["joints"] * n * FEES["joint"][0],
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


def main():
    snap = json.load(open(os.path.join(HERE, "fab", "parts-snapshot.json")))
    s = json.load(open(os.path.join(HERE, "fab", "summary.json")))
    eur = USD_EUR[0]
    w, h = s["board_mm"]
    runs = {n: estimate(s, snap, n) for n in (5, 10)}
    out = [f"# jank chip (jank mini): cost estimate (generated by cost.py; parts snapshot {snap['date']})", "",
           f"{w:g} x {h:g} mm, 2 layers, {s['parts']} parts on {s['lines']} lines, {s['joints']} joints, top side only "
           "(JLC Economic). Shipping (shared with other boards in one order), taxes and the case are not included. Every fee is "
           "an estimate (sources below).", "", "| | 5 boards, USD | 10 boards, USD |", "|---|---|---|"]
    keys5, keys10 = list(runs[5][0]), list(runs[10][0])
    for k5, k10 in zip(keys5, keys10):
        out.append(f"| {k5.replace('(bare, 5)', '(bare)')} | {runs[5][0][k5]:.2f} | {runs[10][0][k10]:.2f} |")
    t5, t10 = runs[5][1], runs[10][1]
    out += [f"| **total** | **{t5:.2f}** (EUR {t5 * eur:.2f}) | **{t10:.2f}** (EUR {t10 * eur:.2f}) |",
            f"| **per board** | **{t5 / 5:.2f}** (EUR {t5 / 5 * eur:.2f}) | **{t10 / 10:.2f}** (EUR {t10 / 10 * eur:.2f}) |", "",
            "## Parts (5 boards)", "", "| LCSC | part | type | stock | a board | ordered (with loss) | unit USD | USD |",
            "|---|---|---|---|---|---|---|---|"]
    for code, sp, per, qty, price, cost in runs[5][2]:
        typ = sp["type"] + (" (preferred)" if sp["preferred"] else "")
        out.append(f"| {code} | {sp['mpn']} | {typ} | {sp['stock']} | {per} | {qty} | {price:.4f} | {cost:.2f} |")
    out += ["", f"Extended types that pay the fee: {', '.join(sorted(runs[5][3]))}", ""]
    out += tht_routes(s, snap, runs)
    out += ["## Sources", ""]
    out += [f"- {k}: USD {v:g}, {why}" for k, (v, why) in FEES.items()] + [f"- {USD_EUR[1]}: {eur}"]
    open(os.path.join(HERE, "fab", "cost.md"), "w").write("\n".join(out) + "\n")
    print("\n".join(out[:16]))


if __name__ == "__main__":
    main()
