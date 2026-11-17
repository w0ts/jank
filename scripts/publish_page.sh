#!/bin/bash
# Publishes the control page (page/jank.html as COMMITTED on HEAD) to https://w0ts.github.io/jank/ : the gh-pages branch
# holds only index.html and .nojekyll, built with git plumbing, so no local branch, no worktree and no push of main.
# A secure (https) origin lets Chrome keep the microphone / output permission and gives the page Web MIDI, which a
# file:// copy cannot remember.
# Usage: scripts/publish_page.sh [--dry-run]    (make publish-page)
set -euo pipefail
cd "$(dirname "$0")/.."
REMOTE="${PAGES_REMOTE:-origin}"
BRANCH=gh-pages
DRY=0
[ "${1:-}" = "--dry-run" ] && DRY=1

page_blob=$(git rev-parse HEAD:page/jank.html)
empty_blob=$(git hash-object -w /dev/null)
tree=$(printf '100644 blob %s\t.nojekyll\n100644 blob %s\tindex.html\n' "$empty_blob" "$page_blob" | git mktree)

git fetch --quiet "$REMOTE" "$BRANCH" 2>/dev/null || true
parent=$(git rev-parse --verify --quiet "refs/remotes/$REMOTE/$BRANCH" || true)
if [ -n "$parent" ] && [ "$(git rev-parse "$parent^{tree}")" = "$tree" ]; then
    echo "nothing to publish: $REMOTE/$BRANCH already holds this page ($(git rev-parse --short "$page_blob"))"
    exit 0
fi

parent_args=()
[ -n "$parent" ] && parent_args=(-p "$parent")
commit=$(git commit-tree "$tree" "${parent_args[@]}" \
    -m "publish: the jank control page (page/jank.html of main $(git rev-parse --short HEAD))" \
    -m "Only the page: no firmware, no weights. Built by scripts/publish_page.sh.")

echo "page ${page_blob:0:10} -> commit ${commit:0:10} on $BRANCH$([ -n "$parent" ] && echo ", on top of ${parent:0:10}")"
if [ "$DRY" = 1 ]; then
    echo "dry run: nothing pushed"
    exit 0
fi
git push "$REMOTE" "${commit}:refs/heads/$BRANCH"
echo "published: https://w0ts.github.io/jank/ (GitHub takes a minute to build)"
