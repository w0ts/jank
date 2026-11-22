# Source this: the project's own ESP-IDF, kept in the scratch folder. Nothing in the home folder.
JANK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export JANK_SCRATCH="${JANK_SCRATCH:-$JANK_ROOT/worktrees/jank-scratch}"
export IDF_COMPONENT_CACHE_PATH="${IDF_COMPONENT_CACHE_PATH:-$JANK_SCRATCH/component-cache}"
# a content hash for build stamps: sha256sum (Linux, CI) or shasum (macOS)
jank_sum() { if command -v sha256sum > /dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
if [ -n "${JANK_IDF_FROM_ENV:-}" ]; then
    # CI (.github/workflows/release.yml): the container's own ESP-IDF (espressif/idf), at $IDF_PATH
    mkdir -p "$JANK_SCRATCH"
    . "$IDF_PATH/export.sh" > "$JANK_SCRATCH/export.log"
else
    export IDF_TOOLS_PATH="$JANK_SCRATCH/espressif"
    export PATH="$JANK_SCRATCH/pybin:$PATH"
    . "$JANK_SCRATCH/esp-idf/export.sh" > "$JANK_SCRATCH/export.log"
fi
