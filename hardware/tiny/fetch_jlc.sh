#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Fetches JLC's own footprints (EasyEDA, via easyeda2kicad 1.0.1) for every LCSC code in parts.py into lib/jlc.pretty.
# They are committed; run this only to refresh them. Needs network and easyeda2kicad (EASYEDA2KICAD=/path/to/it).
# make_footprints.py then copies them into lib/tiny.pretty (3D model references dropped, courtyards redrawn).
set -eu
cd "$(dirname "$0")"
E="${EASYEDA2KICAD:-easyeda2kicad}"
T="$(mktemp -d "$PWD/.fetch.XXXXXX")"
trap 'rm -rf "$T"' EXIT
for c in $(python3 -c "import parts; print(' '.join(sorted({p[4] for p in parts.PARTS if p[4]})))"); do
    "$E" --footprint --lcsc_id "$c" --output "$T/x" --overwrite >/dev/null
done
cp "$T"/x.pretty/*.kicad_mod lib/jlc.pretty/
ls lib/jlc.pretty
