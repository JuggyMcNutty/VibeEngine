#!/usr/bin/env bash
# Upstream Surreal Engine and the fork's deusex branch. Upstream's commits
# come in only when someone decides to (vibe/docs/ENGINE.md, upgrading).
#
#   vibe/tools/upgrade.sh status          how far upstream is past the fork
#   vibe/tools/upgrade.sh [<ref>]         merge upstream in: its latest, or <sha|tag|branch>
#   vibe/tools/upgrade.sh --continue      after resolving conflicts (git add each file)
#   vibe/tools/upgrade.sh --abort         leave the fork as it was
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
BRANCH=deusex
UPSTREAM="https://github.com/dpjudas/SurrealEngine.git"
UPSTREAM_REF=upstream/master        # upstream's own branch, as the clone fetches it

say() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
eng() { git -C "$REPO" "$@"; }
commits() { if [ "$1" = 1 ]; then echo "1 commit"; else echo "$1 commits"; fi; }
ensure_upstream() { eng remote get-url upstream >/dev/null 2>&1 || eng remote add upstream "$UPSTREAM"; }

finish() {
    say "merged. Next:"
    say "  build and run linux-x86_64; check the Vulkan validation layer;"
    say "  vibe/tools/perf/perf.sh on (then save if the hooks moved); profile on the devices;"
    say "  bring vibe/docs/ENGINE.md up to date, push $BRANCH, then pin it in the workspace"
    say "  (Port Ex Machina's scripts/engine.sh pin)."
}

ensure_upstream
g="$(eng rev-parse --absolute-git-dir)"
case "${1:-}" in
    status)
        eng fetch --quiet --force --tags upstream
        printf 'upstream  %s  (%s)\n' "$(eng log -1 --format='%h  %cs  %s' "$UPSTREAM_REF")" "$UPSTREAM_REF"
        n=$(eng rev-list --count "$(eng merge-base "$UPSTREAM_REF" "$BRANCH")..$UPSTREAM_REF")
        if [ "$n" = 0 ]; then
            echo "the fork has all of upstream"
        else
            echo "upstream is $(commits "$n") past the fork -- vibe/tools/upgrade.sh merges them in"
        fi
        [ ! -f "$g/MERGE_HEAD" ] || echo "a merge is in progress -- vibe/tools/upgrade.sh --continue or --abort"
        exit 0 ;;
    --continue)
        [ -f "$g/MERGE_HEAD" ] || die "no merge in progress"
        eng -c core.editor=true merge --continue ||
            die "still conflicts -- resolve them (git add each file), then vibe/tools/upgrade.sh --continue"
        finish
        exit 0 ;;
    --abort)
        [ -f "$g/MERGE_HEAD" ] || die "no merge in progress"
        eng merge --abort
        say "upgrade abandoned: $BRANCH is as it was"
        exit 0 ;;
    -*) die "upgrade.sh status | [<upstream ref>] | --continue | --abort" ;;
esac
[ ! -f "$g/MERGE_HEAD" ] || die "a merge is in progress -- vibe/tools/upgrade.sh --continue or --abort"
[ "$(eng rev-parse --abbrev-ref HEAD)" = "$BRANCH" ] || die "the clone is not on $BRANCH"
[ -z "$(eng status --porcelain --untracked-files=no)" ] ||
    die "uncommitted changes in the engine tree (the profiling hooks? vibe/tools/perf/perf.sh off)"

eng fetch --quiet --force --tags upstream
ref="${1:-$UPSTREAM_REF}"
target="$(eng rev-parse --verify --quiet "$ref^{commit}")" || die "no such commit in the clone: $ref"
if eng merge-base --is-ancestor "$target" HEAD; then
    say "the fork already has $ref ($(eng rev-parse --short "$target"))"
    exit 0
fi
say "merging $(commits "$(eng rev-list --count "HEAD..$target")") of upstream into $BRANCH"
if ! eng merge -m "Merge upstream SurrealEngine: $(eng log -1 --format=%s "$target")" "$target"; then
    say "upstream conflicts with the fork: resolve the files (git add each one),"
    say "then vibe/tools/upgrade.sh --continue -- or --abort to leave everything as it was"
    exit 1
fi
finish
