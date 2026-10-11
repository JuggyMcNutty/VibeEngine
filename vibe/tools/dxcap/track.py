#!/usr/bin/env python3
"""Reads WadeConsole, FlightConsole and SlideConsole runs: does a pawn move
as the original's does?

    track.py [--tol <units>] <reference run dir> <run dir>

Each run's tick lines -- "<PREFIX>: <scenario> tick <n> t <t> loc <x,y,z>
vel <x,y,z> accel <x,y,z> rot <p,y,r> physics <n> state <name> ..." -- are
read from its log (engine.log or DeusEx.log), for whichever of DXWADE,
DXFLIGHT and DXSLIDE it has. The first run is the reference, normally the
original's. Scenario by scenario, the other run's place at each reference
line's t is interpolated between its two lines around it:

- the largest distance between the two and its time, and the distance at
  the reference's last line;
- the scenario agrees when the largest distance is within the tolerance (16
  units, or 5% of the way the reference went, whichever is more; --tol
  another floor) and both pass through the same physics and states in the
  same order;
- each run's changes of physics or state, with their times;
- each run's fastest speed and its pitch's range (FlightConsole's stock
  gull, whose flight is random, is judged by these alone);
- the console's other lines in each run (its events), in order.
"""
import math
import os
import re
import sys

TICK = re.compile(r"(DXWADE|DXFLIGHT|DXSLIDE): (\S+) tick (\d+) t (\S+) loc (\S+) vel (\S+) accel (\S+) rot (\S+) physics (\d+) state (\S+)")
OTHER = re.compile(r"(DXWADE|DXFLIGHT|DXSLIDE): (.*)")
PHYSICS = ["None", "Walking", "Falling", "Swimming", "Flying", "Rotating", "Projectile", "Rolling", "Interpolating",
           "MovingBrush", "Spider", "Trailer"]


def vec(s):
    return tuple(float(x) for x in s.split(","))


def read_run(run):
    """Each scenario's lines in order, and the console's other lines."""
    for name in ("engine.log", "DeusEx.log"):
        path = os.path.join(run, name)
        if os.path.exists(path):
            break
    else:
        sys.exit(f"{run}: no engine.log or DeusEx.log")
    scenarios, others = {}, []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = TICK.search(line)
            if m:
                _, scen, _, t, loc, vel, accel, rot, phys, state = m.groups()
                scenarios.setdefault(scen, []).append(
                    {"t": float(t), "loc": vec(loc), "vel": vec(vel), "rot": vec(rot), "physics": int(phys), "state": state})
                continue
            m = OTHER.search(line)
            if m:
                others.append(m.group(2).strip())
    return scenarios, others


def at(lines, t):
    """The place at t, between the two lines around it (held past the ends)."""
    if t <= lines[0]["t"]:
        return lines[0]["loc"]
    for a, b in zip(lines, lines[1:]):
        if a["t"] <= t <= b["t"]:
            f = (t - a["t"]) / (b["t"] - a["t"]) if b["t"] > a["t"] else 0.0
            return tuple(p + (q - p) * f for p, q in zip(a["loc"], b["loc"]))
    return lines[-1]["loc"]


def changes(lines):
    """(time, physics, state) at the start and at each change of either."""
    out, last = [], None
    for l in lines:
        key = (PHYSICS[l["physics"]] if l["physics"] < len(PHYSICS) else str(l["physics"]), l["state"])
        if key != last:
            out.append((l["t"], key[0], key[1]))
            last = key
    return out


def path_length(lines):
    return sum(math.dist(a["loc"], b["loc"]) for a, b in zip(lines, lines[1:]))


def pitch(p):
    """A rotator's pitch, -32768..32767."""
    p = int(p) & 0xFFFF
    return p - 0x10000 if p >= 0x8000 else p


def main():
    args = sys.argv[1:]
    floor = 16.0
    if len(args) >= 2 and args[0] == "--tol":
        floor = float(args[1])
        args = args[2:]
    if len(args) != 2:
        sys.exit(__doc__)
    ref_run, run = args
    ref, ref_other = read_run(ref_run)
    oth, oth_other = read_run(run)
    print(f"{os.path.basename(run)} against {os.path.basename(ref_run)}:")
    agreed = 0
    names = [s for s in ref if s in oth]
    for scen in names:
        r, o = ref[scen], oth[scen]
        worst, worst_t = 0.0, 0.0
        for l in r:
            d = math.dist(l["loc"], at(o, l["t"]))
            if d > worst:
                worst, worst_t = d, l["t"]
        final = math.dist(r[-1]["loc"], at(o, r[-1]["t"]))
        tol = max(floor, 0.05 * path_length(r))
        rc, oc = changes(r), changes(o)
        same_path = [c[1:] for c in rc] == [c[1:] for c in oc]
        good = worst <= tol and same_path
        agreed += good
        print(f"  {'  ' if good else '! '}{scen}: largest {worst:.1f} at {worst_t:.2f} s, final {final:.1f} "
              f"(tolerance {tol:.1f}; the reference went {path_length(r):.0f}, this run {path_length(o):.0f})"
              + ("" if same_path else "; physics or states differ"))
        for label, ch, lines in (("reference", rc, r), ("this run ", oc, o)):
            speed = max(math.hypot(*l["vel"]) for l in lines)
            pitches = [pitch(l["rot"][0]) for l in lines]
            print(f"      {label}: " + ", ".join(f"{t:.2f} {p} {s}" for t, p, s in ch)
                  + f"; fastest {speed:.0f}, pitch {min(pitches)} to {max(pitches)}")
    only = [s for s in ref if s not in oth] + [s for s in oth if s not in ref]
    print(f"  {agreed} of {len(names)} scenarios agree" + (f"; in one run only: {', '.join(only)}" if only else ""))
    for label, other in (("reference", ref_other), ("this run", oth_other)):
        print(f"  {label}'s other lines:")
        for line in other:
            print(f"    {line}")


if __name__ == "__main__":
    main()
