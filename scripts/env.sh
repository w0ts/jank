# Source this: the project's own ESP-IDF, kept in the scratch folder. Nothing in the home folder.
JANK_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export JANK_SCRATCH="${JANK_SCRATCH:-$JANK_ROOT/../worktrees/jank-scratch}"
export IDF_TOOLS_PATH="$JANK_SCRATCH/espressif"
export IDF_COMPONENT_CACHE_PATH="$JANK_SCRATCH/component-cache"
export PATH="$JANK_SCRATCH/pybin:$PATH"
. "$JANK_SCRATCH/esp-idf/export.sh" > "$JANK_SCRATCH/export.log"
