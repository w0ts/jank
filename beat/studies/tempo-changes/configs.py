"""The chains compared (chain.Config), by name. Constants as {"module.NAME": value} (chain.MODULES)."""
from __future__ import annotations

from chain import Config

TODAY = Config("today")
M2I = Config("m2i", follow=True, conf="lines")
M2I_RATE = Config("m2i+rate", follow=True, conf="lines", rate=True)
WEAK = Config("weak+rate", follow=True, conf="weak", rate=True, weak={})

BASE = {c.name: c for c in (TODAY, M2I, M2I_RATE, WEAK)}


def variant(base: Config, name: str, **changes) -> Config:
    """base with some fields replaced; dict fields (est_kw, consts, weak) are merged."""
    kw = dict(base.__dict__)
    for k, v in changes.items():
        kw[k] = {**(kw.get(k) or {}), **v} if isinstance(v, dict) and isinstance(kw.get(k), dict) else v
    kw["name"] = name
    return Config(**kw)


def named(names: list[str], extra: dict | None = None) -> list[Config]:
    """Configs by name; `extra` maps further names to {"base": name, field: value, ...} (a sweep file)."""
    reg = dict(BASE)
    for name, spec in (extra or {}).items():
        spec = dict(spec)
        reg[name] = variant(reg[spec.pop("base")], name, **spec)
    return [reg[n] for n in names]
