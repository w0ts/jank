#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Fetches JLC's own footprints and 3D models (EasyEDA, via easyeda2kicad 1.0.1) for every LCSC code in parts.py into
# lib/jlc.pretty and lib/3d (.wrl only). They are committed; run this only to refresh them. Needs network and uv
# (uvx), or EASYEDA2KICAD=/path/to/easyeda2kicad. make_footprints.py then copies them into lib/chip.pretty.
set -eu
cd "$(dirname "$0")"
T="$(mktemp -d "$PWD/.fetch.XXXXXX")"
trap 'rm -rf "$T"' EXIT
for c in $(python3 -c "import parts; print(' '.join(sorted({p[4] for p in parts.PARTS if p[4]})))"); do
    if [ -n "${EASYEDA2KICAD:-}" ]; then
        "$EASYEDA2KICAD" --footprint --3d --lcsc_id "$c" --output "$T/x" --overwrite >/dev/null
    else
        uvx --from easyeda2kicad==1.0.1 easyeda2kicad --footprint --3d --lcsc_id "$c" --output "$T/x" --overwrite >/dev/null
    fi
done
mkdir -p lib/jlc.pretty lib/3d
cp "$T"/x.pretty/*.kicad_mod lib/jlc.pretty/
cp "$T"/x.3dshapes/*.wrl lib/3d/
ls lib/jlc.pretty lib/3d
