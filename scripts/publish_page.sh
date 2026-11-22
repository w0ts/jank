#!/bin/bash
# Publishes the site to https://w0ts.github.io/jank/ : About (page/index.html -> index.html), Install (page/install.html ->
# install.html), Device (page/jank.html, the control page the firmware embeds -> device.html), Build (page/build.html -> build.html)
# and page/img/. The gh-pages
# branch holds only pages, built with git plumbing,
# so no local branch, no worktree and no push of main. A secure (https) origin lets Chrome keep the microphone / output
# permission and gives the page Web MIDI, which a file:// copy cannot remember.
#   scripts/publish_page.sh [--dry-run] [--from REF]    the pages as COMMITTED on HEAD (or on REF) -> the root
#   scripts/publish_page.sh [--dry-run] --preview NAME REF
#                                                       the pages of branch/commit REF -> /NAME/ (a preview next to
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
        --from) REF="${2:?--from REF}"; shift 2 ;;
        --preview) PREVIEW="${2:?--preview NAME REF}"; REF="${3:?--preview NAME REF}"; shift 3 ;;
        *) echo "usage: scripts/publish_page.sh [--dry-run] [--preview NAME REF]"; exit 2 ;;
    esac
done
case "$PREVIEW" in *[!a-z0-9_-]*) echo "preview name: lowercase letters, digits, - and _ only"; exit 2 ;; esac

empty_blob=$(git hash-object -w /dev/null)
# the site's files: published name <- path in the repo
site_entries() {   # the "mode type sha<TAB>name" lines of the site, from REF
    local pairs="index.html:page/index.html install.html:page/install.html device.html:page/jank.html build.html:page/build.html" pair
    for pair in $pairs; do
        printf '100644 blob %s\t%s\n' "$(git rev-parse "$REF:${pair#*:}")" "${pair%%:*}"
    done
    printf '040000 tree %s\timg\n' "$(git rev-parse "$REF:page/img")"
}

git fetch --quiet "$REMOTE" "$BRANCH" 2>/dev/null || true
parent=$(git rev-parse --verify --quiet "refs/remotes/$REMOTE/$BRANCH" || true)
kept=""
[ -n "$parent" ] && kept=$(git ls-tree "$parent^{tree}")

keep_except() { printf '%s\n' "$kept" | awk -F'\t' -v skip="$1" 'NF && $2 != skip'; }
keep_not_site() { printf '%s\n' "$kept" | awk -F'\t' 'NF && $2 != "index.html" && $2 != "install.html" && $2 != "device.html" && $2 != "build.html" && $2 != "img" && $2 != ".nojekyll"'; }

if [ -n "$PREVIEW" ]; then
    sub=$(site_entries | git mktree)
    tree=$( { keep_except "$PREVIEW"; printf '040000 tree %s\t%s\n' "$sub" "$PREVIEW"; } | git mktree)
    what="a preview /$PREVIEW/ from $REF"
else
    tree=$( { keep_not_site; site_entries; printf '100644 blob %s\t.nojekyll\n' "$empty_blob"; } | git mktree)
    what="the site from $REF"
fi

if [ -n "$parent" ] && [ "$(git rev-parse "$parent^{tree}")" = "$tree" ]; then
    echo "nothing to publish: $REMOTE/$BRANCH already holds this ($what)"
    exit 0
fi

parent_args=()
[ -n "$parent" ] && parent_args=(-p "$parent")
commit=$(git commit-tree "$tree" "${parent_args[@]}" \
    -m "publish: the jank site ($what)" \
    -m "Only pages: no firmware, no weights. Built by scripts/publish_page.sh.")

echo "$what -> commit ${commit:0:10} on $BRANCH$([ -n "$parent" ] && echo ", on top of ${parent:0:10}")"
git ls-tree "$tree" | awk -F'\t' '{print "  " $2}'
if [ "$DRY" = 1 ]; then
    echo "dry run: nothing pushed"
    exit 0
fi
git push "$REMOTE" "${commit}:refs/heads/$BRANCH"
echo "published: https://w0ts.github.io/jank/${PREVIEW:+$PREVIEW/} (GitHub takes a minute to build)"
