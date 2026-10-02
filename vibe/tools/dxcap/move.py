#!/usr/bin/env python3
"""Reads MoveConsole runs: does each pawn move as the original's does?

    move.py <reference run dir> <run dir> [<run dir> ...]

Each run's DXMOVE lines -- MoveConsole's, every two seconds: a pawn's state,
orders, move target, place, and the distance it moved since the line before
-- are read from its log (engine.log or DeusEx.log). The first run is the
reference, normally the original's, and each other is laid beside it, pawn by
pawn:

- distance: the whole run's distance moved, and whether it is within
  tolerance of the reference's (15% of it, or 200 units, whichever is more);
- stalls: the longest run of lines over which a pawn moved under 5 units
  while its orders were to go somewhere (anything but Standing, Sitting,
  Dancing or Idle), flagged where it is 3 lines or more longer than the
  reference's longest;
- targets: the move targets it reached, as each line names its mt, and those
  the reference reached that it never did.

The summary counts the pawns within tolerance and the stalls the reference
does not have; a pawn missing from either run is listed apart.
"""
import os
import re
import sys

LINE = re.compile(r"DXMOVE: (\d+) (\S+) (\S+) state (\S+) orders (\S+) mt (\S+) dest \S+ vel \S+ at \S+ moved (\d+)")
STILL_ORDERS = {"Standing", "Sitting", "Dancing", "Idle", "None"}


def read_run(run):
    """Each pawn's lines, in order: (time, state, orders, move target, moved)."""
    for name in ("engine.log", "DeusEx.log"):
        path = os.path.join(run, name)
        if os.path.exists(path):
            break
    else:
        sys.exit(f"{run}: no engine.log or DeusEx.log")
    pawns = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if not m:
                continue
            t, pawn, cls, state, orders, mt, moved = m.groups()
            pawns.setdefault(pawn, {"class": cls, "lines": []})["lines"].append((int(t), state, orders, mt, int(moved)))
    return pawns


def longest_stall(lines):
    """The longest run of lines moving under 5 units while ordered to go."""
    best = run = 0
    for _, _, orders, _, moved in lines:
        if moved < 5 and orders not in STILL_ORDERS:
            run += 1
            best = max(best, run)
        else:
            run = 0
    return best


def targets(lines):
    return {mt for _, _, _, mt, _ in lines if mt != "None"}


def within(dist, ref):
    return abs(dist - ref) <= max(0.15 * ref, 200)


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    ref_run = sys.argv[1]
    ref = read_run(ref_run)
    for run in sys.argv[2:]:
        other = read_run(run)
        print(f"{os.path.basename(run)} against {os.path.basename(ref_run)}:")
        names = sorted(set(ref) & set(other))
        ok = 0
        new_stalls = []
        for pawn in names:
            r_lines, o_lines = ref[pawn]["lines"], other[pawn]["lines"]
            r_dist = sum(l[4] for l in r_lines)
            o_dist = sum(l[4] for l in o_lines)
            r_stall, o_stall = longest_stall(r_lines), longest_stall(o_lines)
            missed = sorted(targets(r_lines) - targets(o_lines))
            good = within(o_dist, r_dist)
            ok += good
            stalled = o_stall >= r_stall + 3
            if stalled:
                new_stalls.append(pawn)
            flags = []
            if not good:
                flags.append("DISTANCE")
            if stalled:
                flags.append(f"STALL {o_stall} lines (reference {r_stall})")
            if missed:
                flags.append("never reached " + " ".join(missed))
            mark = "  " if not flags else "! "
            print(f"  {mark}{pawn:22s} {ref[pawn]['class']:18s} moved {o_dist:6d} vs {r_dist:6d}"
                  + (("  " + "; ".join(flags)) if flags else ""))
        only_ref = sorted(set(ref) - set(other))
        only_other = sorted(set(other) - set(ref))
        print(f"  {ok} of {len(names)} pawns' distance within tolerance; "
              f"{len(new_stalls)} stalling where the reference does not"
              + (f" ({', '.join(new_stalls)})" if new_stalls else ""))
        if only_ref:
            print(f"  only in the reference: {', '.join(only_ref)}")
        if only_other:
            print(f"  only in this run: {', '.join(only_other)}")
        print()


if __name__ == "__main__":
    main()
