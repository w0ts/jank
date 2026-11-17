"""The robust chains by name: the firmware chain (tuned-l16-causal, tempo-changes/final.json) and its variants.

A sweep file maps names to {"base": name, "coast": {...}, "memory": {...}, "ratio": {...}, "agree": {...}}: a variant of
an earlier name, its dicts merged key by key (None in a sweep entry turns a change off).
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "tempo-changes"))
sys.path.insert(0, str(HERE))

import configs as CF
from rchain import RConfig

FINAL = HERE.parent / "tempo-changes" / "final.json"
FIRMWARE = "tuned-l16-causal"
CHANGES = ("coast", "memory", "ratio", "agree", "skip")


def firmware() -> RConfig:
    base = CF.named([FIRMWARE], json.loads(FINAL.read_text()))[0]
    return RConfig("fw", base)


def variant(base: RConfig, name: str, spec: dict) -> RConfig:
    kw = {"name": name, "base": base.base, "dbn": spec.get("dbn", base.dbn)}
    for c in CHANGES:
        old = getattr(base, c)
        if c not in spec:
            kw[c] = old
        elif spec[c] is None:
            kw[c] = None
        else:
            kw[c] = {**(old or {}), **spec[c]}
    return RConfig(**kw)


def registry(extra: dict | None = None) -> dict[str, RConfig]:
    reg = {"fw": firmware()}
    for name, spec in (extra or {}).items():
        spec = dict(spec)
        reg[name] = variant(reg[spec.pop("base", "fw")], name, spec)
    return reg


def named(names: list[str], extra: dict | None = None) -> list[RConfig]:
    reg = registry(extra)
    return [reg[n] for n in names]
