# SPDX-License-Identifier: GPL-3.0-only
"""A snapshot of every LCSC part in parts.py as JLCPCB's parts search shows it today (no login, reads only): library type
(base = Basic, expand = Extended), whether it is a "preferred" Extended part, stock, price breaks and the loss JLC adds.
Writes fab/parts-snapshot.json; cost.py and the README read it. Prices and stock change daily: a snapshot, not a quote.

  python3 jlc_parts.py
"""
import datetime
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
import parts as P

URL = "https://jlcpcb.com/api/overseas-pcb-order/v1/shoppingCart/smtGood/selectSmtComponentList"


def lookup(code):
    body = json.dumps({"keyword": code, "currentPage": 1, "pageSize": 5}).encode()
    req = urllib.request.Request(URL, body, {"Content-Type": "application/json"})
    rows = json.load(urllib.request.urlopen(req, timeout=30))["data"]["componentPageInfo"]["list"] or []
    for c in rows:
        if c["componentCode"] == code:
            return {"code": code, "mpn": c["componentModelEn"], "brand": c.get("componentBrandEn", ""),
                    "package": c.get("componentSpecificationEn", ""),
                    "type": {"base": "Basic", "expand": "Extended"}.get(c["componentLibraryType"], c["componentLibraryType"]),
                    "preferred": bool(c.get("preferredComponentFlag")), "stock": c["stockCount"],
                    "loss": c.get("lossNumber") or 0,
                    "prices": [(p["startNumber"], p["productPrice"]) for p in c.get("componentPrices") or []]}
    raise SystemExit(f"{code}: not found at JLCPCB")


def main():
    codes = sorted({p[4] for p in P.PARTS if p[4]})
    snap = {"date": datetime.date.today().isoformat(), "source": URL, "parts": {c: lookup(c) for c in codes}}
    json.dump(snap, open(os.path.join(HERE, "fab", "parts-snapshot.json"), "w"), indent=1)
    for c in codes:
        s = snap["parts"][c]
        print(f"{c:>10} {s['type']:8} {'pref' if s['preferred'] else '    '} stock {s['stock']:>8}  ${s['prices'][0][1]:<8} {s['mpn']}")


if __name__ == "__main__":
    main()
