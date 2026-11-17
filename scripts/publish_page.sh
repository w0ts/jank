#!/bin/bash
# Publishes the control page to https://w0ts.github.io/jank/ : the gh-pages branch holds only pages, built with git plumbing,
# so no local branch, no worktree and no push of main. A secure (https) origin lets Chrome keep the microphone / output
# permission and gives the page Web MIDI, which a file:// copy cannot remember.
#   scripts/publish_page.sh [--dry-run]                 page/jank.html as COMMITTED on HEAD -> the root (index.html)
#   scripts/publish_page.sh [--dry-run] --preview NAME REF
#                                                       page/jank.html of branch/commit REF -> /NAME/ (a preview next to
#                                                       the main page, e.g. https://w0ts.github.io/jank/lcd2/)
# Every other path already on gh-pages is kept. (make publish-page)
set -euo pipefail
cd "$(dirname "$0")/.."
REMOTE="${PAGES_REMOTE:-origin}"
BRANCH=gh-pages
DRY=0
PREVIEW=""
REF=HEAD
while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run) DRY=1; shift ;;
        --preview) PREVIEW="${2:?--preview NAME REF}"; REF="${3:?--preview NAME REF}"; shift 3 ;;
        *) echo "usage: scripts/publish_page.sh [--dry-run] [--preview NAME REF]"; exit 2 ;;
    esac
done
case "$PREVIEW" in *[!a-z0-9_-]*) echo "preview name: lowercase letters, digits, - and _ only"; exit 2 ;; esac

page_blob=$(git rev-parse "$REF:page/jank.html")
empty_blob=$(git hash-object -w /dev/null)

git fetch --quiet "$REMOTE" "$BRANCH" 2>/dev/null || true
parent=$(git rev-parse --verify --quiet "refs/remotes/$REMOTE/$BRANCH" || true)
kept=""
[ -n "$parent" ] && kept=$(git ls-tree "$parent^{tree}")

keep_except() { printf '%s\n' "$kept" | awk -F'\t' -v skip="$1" 'NF && $2 != skip'; }

if [ -n "$PREVIEW" ]; then
    sub=$(printf '100644 blob %s\tindex.html\n' "$page_blob" | git mktree)
    tree=$( { keep_except "$PREVIEW"; printf '040000 tree %s\t%s\n' "$sub" "$PREVIEW"; } | git mktree)
    what="preview /$PREVIEW/ from $REF"
else
    tree=$( { keep_except index.html | awk -F'\t' '$2 != ".nojekyll"'; printf '100644 blob %s\t.nojekyll\n100644 blob %s\tindex.html\n' "$empty_blob" "$page_blob"; } | git mktree)
    what="the main page from $REF"
fi

if [ -n "$parent" ] && [ "$(git rev-parse "$parent^{tree}")" = "$tree" ]; then
    echo "nothing to publish: $REMOTE/$BRANCH already holds this ($what)"
    exit 0
fi

parent_args=()
[ -n "$parent" ] && parent_args=(-p "$parent")
commit=$(git commit-tree "$tree" "${parent_args[@]}" \
    -m "publish: the jank control page ($what, page ${page_blob:0:10})" \
    -m "Only pages: no firmware, no weights. Built by scripts/publish_page.sh.")

echo "$what -> commit ${commit:0:10} on $BRANCH$([ -n "$parent" ] && echo ", on top of ${parent:0:10}")"
git ls-tree "$tree" | awk -F'\t' '{print "  " $2}'
if [ "$DRY" = 1 ]; then
    echo "dry run: nothing pushed"
    exit 0
fi
git push "$REMOTE" "${commit}:refs/heads/$BRANCH"
echo "published: https://w0ts.github.io/jank/${PREVIEW:+$PREVIEW/} (GitHub takes a minute to build)"
