#!/usr/bin/env bash
# The frame-time profiling hooks: perf-instrumentation.patch beside this
# script, applied to the engine tree for a profile and reverted after it --
# never committed. vibe/docs/ENGINE.md, the profiling hooks.
#
#   vibe/tools/perf/perf.sh on      apply the hooks
#   vibe/tools/perf/perf.sh off     revert them
#   vibe/tools/perf/perf.sh save    rewrite the patch from the tree (after re-basing the hooks)
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(git -C "$HERE" rev-parse --show-toplevel)"
PATCH="$HERE/perf-instrumentation.patch"

say() { printf '%s\n' "$*" >&2; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
usage() { sed -n '2,8p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 2; }
eng() { git -C "$REPO" "$@"; }

# The hooks are a patch against the fork's head, so a fork commit that
# touches the same lines moves them. "on" falls back to a three-way merge;
# after one (and after resolving any conflict it leaves), "save" rewrites the
# patch from the tree so "off" and the next "on" apply cleanly. vibe/, where
# the patch itself lives, is never part of it.
case "${1:-}" in
    on)
        if eng apply "$PATCH" 2>/dev/null; then
            say "profiling hooks applied -- never commit them (vibe/tools/perf/perf.sh off)"
            exit 0
        fi
        # A three-way merge needs the index to match the tree: it applies
        # nothing over uncommitted changes to a file the hooks touch.
        rc=0
        eng apply --3way "$PATCH" || rc=$?
        eng reset -q
        conflicts="$(eng diff --name-only --diff-filter=U; eng grep -l '^<<<<<<< ' -- SurrealEngine 2>/dev/null || true)"
        [ -z "$conflicts" ] || die "the hooks conflict with the fork in: $(echo $conflicts) -- resolve, then vibe/tools/perf/perf.sh save"
        eng grep -q 'TEMPORARY DEBUG TOOL' -- SurrealEngine 2>/dev/null ||
            die "the hooks did not apply (git apply --3way: $rc) -- commit the engine's changes first, then perf.sh on"
        say "profiling hooks applied by a three-way merge -- vibe/tools/perf/perf.sh save to re-base the patch"
        ;;
    off) eng apply -R "$PATCH" && say "profiling hooks removed" ;;
    save)
        ! eng grep -q '^<<<<<<< \|^>>>>>>> ' -- SurrealEngine 2>/dev/null ||
            die "conflict markers in the engine tree -- resolve them before perf.sh save"
        # New files (PerfLog.h) are untracked: record them for the diff only.
        mapfile -t new < <(eng ls-files --others --exclude-standard -- SurrealEngine)
        [ ${#new[@]} -eq 0 ] || eng add -N -- "${new[@]}"
        eng diff -- . ':(exclude)vibe' > "$PATCH"
        [ ${#new[@]} -eq 0 ] || eng reset -q -- "${new[@]}"
        grep -q '^+.*TEMPORARY DEBUG TOOL' "$PATCH" || die "no TEMPORARY DEBUG TOOL lines in the tree -- are the hooks applied?"
        say "wrote vibe/tools/perf/perf-instrumentation.patch from the engine tree"
        ;;
    *) usage ;;
esac
