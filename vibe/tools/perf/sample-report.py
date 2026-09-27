#!/usr/bin/env python3
"""Report on the engine's own CPU samples (SURREAL_PERF_SAMPLE, a hook in
vibe/tools/perf/perf-instrumentation.patch), for devices without perf.

  vibe/tools/perf/sample-report.py <samples> <engine binary> [--root NAME] [--callers NAME]
                                   [--skip N] [--top N] [--sysroot DIR]

<samples> is the file the engine wrote, with <samples>.maps beside it; the
engine binary is the unstripped build that ran -- that exact build: every
rebuild moves the addresses (Port Ex Machina's dx.sh profile keeps it as <samples>.engine).
Each block records the main thread's CPU time over it, so shares come out
as ms per frame too (a sample is 1 ms, or a scheduler tick where the kernel
checks CPU timers only that often: 4 ms on the Smart Pro). --root keeps only
samples with NAME in their stack (ULevel::Tick: the game tick); the stacks
come from frame pointers, which the hooks' build has. --callers adds the
three frames above each sample whose own function is NAME. --skip drops the
first N blocks (default 1: the one with the level's loading).

Symbols come from nm: NM=<path> for a cross build (the port's toolchain nm);
libraries are looked up in deps/sysroots/<port>/lib when --sysroot names it.
"""
import argparse
import bisect
import collections
import os
import re
import struct
import subprocess
import sys


def read_blocks(path):
    blocks = []
    with open(path, "rb") as f:
        data = f.read()
    pos = 0
    while pos + 28 <= len(data):
        magic, index, frames, count, dropped, cpu_ns = struct.unpack_from("<5IQ", data, pos)
        if magic != 0x42535844:
            sys.exit(f"{path}: not a sample block at byte {pos}")
        pos += 28
        samples = []
        for _ in range(count):
            if pos + 8 > len(data):
                break
            (depth,) = struct.unpack_from("<Q", data, pos)
            pos += 8
            pcs = struct.unpack_from(f"<{depth}Q", data, pos)
            pos += 8 * depth
            samples.append(pcs)
        blocks.append((index, frames, dropped, cpu_ns, samples))
    return blocks


def read_maps(path):
    # The executable is the first file mapped: the lowest address.
    maps = []   # (start, end, path)
    bases = {}  # path -> load address (its mapping at file offset 0)
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 6:
                continue
            start, end = (int(x, 16) for x in parts[0].split("-"))
            offset = int(parts[2], 16)
            name = parts[5]
            maps.append((start, end, name))
            if offset == 0 and name not in bases:
                bases[name] = start
    maps.sort()
    exe = next((m[2] for m in maps if m[2].startswith("/")), "")
    return maps, bases, exe


class Symbols:
    def __init__(self, path, nm, dynamic=False):
        self.addrs, self.names = [], []
        args = [nm, "-C", "-n", "--defined-only"] + (["-D"] if dynamic else []) + [path]
        try:
            out = subprocess.run(args, capture_output=True, text=True, check=True).stdout
        except (OSError, subprocess.CalledProcessError) as e:
            print(f"warning: {' '.join(args)}: {e}", file=sys.stderr)
            return
        for line in out.splitlines():
            m = re.match(r"([0-9a-f]+) [tTwW] (.*)", line)
            if m:
                self.addrs.append(int(m.group(1), 16))
                self.names.append(m.group(2))

    def lookup(self, addr):
        i = bisect.bisect_right(self.addrs, addr) - 1
        return self.names[i] if i >= 0 else None


def short(name):
    # Drop argument lists and template noise: enough to tell functions apart.
    name = re.sub(r"\(.*\)( const)?$", "", name)
    return name


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("samples")
    ap.add_argument("binary")
    ap.add_argument("--root", default="")
    ap.add_argument("--callers", default="")
    ap.add_argument("--skip", type=int, default=1)
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--sysroot", default="")
    args = ap.parse_args()

    nm = os.environ.get("NM", "nm")
    blocks = read_blocks(args.samples)
    maps, bases, exe = read_maps(args.samples + ".maps")
    starts = [m[0] for m in maps]
    engine = Symbols(args.binary, nm)
    libs = {}

    def symbolise(addr, is_return):
        if is_return:
            addr -= 1  # a return address is just past its call
        i = bisect.bisect_right(starts, addr) - 1
        if i < 0 or addr >= maps[i][1]:
            return "[unknown]"
        path = maps[i][2]
        rel = addr - bases.get(path, maps[i][0])
        base = os.path.basename(path)
        if path == exe:
            return short(engine.lookup(rel) or "[" + base + "]")
        if args.sysroot:
            if base not in libs:
                lib = os.path.join(args.sysroot, "lib", base)
                libs[base] = Symbols(lib, nm, dynamic=True) if os.path.exists(lib) else None
            if libs[base]:
                name = libs[base].lookup(rel)
                if name:
                    return short(name) + " [" + base + "]"
        return "[" + base + "]"

    cache = {}
    self_c, incl_c, callers_c = collections.Counter(), collections.Counter(), collections.Counter()
    frames = matched = total = dropped = cpu_ns = 0
    for index, nframes, ndropped, ncpu, samples in blocks[args.skip:]:
        frames += nframes
        dropped += ndropped
        cpu_ns += ncpu
        for pcs in samples:
            total += 1
            names = []
            for j, pc in enumerate(pcs):
                key = (pc, j > 0)
                if key not in cache:
                    cache[key] = symbolise(pc, j > 0)
                names.append(cache[key])
            if args.root and not any(args.root in n for n in names):
                continue
            matched += 1
            self_c[names[0]] += 1
            for n in set(names):
                incl_c[n] += 1
            if args.callers and args.callers in names[0]:
                callers_c[" <- ".join(names[1:4])] += 1

    if not frames or not total:
        sys.exit("no samples after --skip")
    ms = cpu_ns / 1e6 / total  # CPU time a sample stands for
    print(f"{len(blocks) - args.skip} blocks, {frames} frames, {total} samples of {ms:.2f} ms "
          f"({cpu_ns / 1e6 / frames:.1f} ms of main-thread CPU a frame), {dropped} dropped")
    if args.root:
        print(f"under {args.root}: {matched} samples, {matched * ms / frames:.1f} ms a frame")
    denom = max(matched, 1)
    print(f"\n-- self: % of {'samples under ' + args.root if args.root else 'all samples'}, ms a frame --")
    for name, n in self_c.most_common(args.top):
        print(f"{100.0 * n / denom:6.2f}%  {n * ms / frames:6.2f}  {name}")
    print("\n-- inclusive --")
    for name, n in incl_c.most_common(args.top):
        print(f"{100.0 * n / denom:6.2f}%  {n * ms / frames:6.2f}  {name}")
    if args.callers:
        print(f"\n-- callers of {args.callers} --")
        for name, n in callers_c.most_common(args.top):
            print(f"{100.0 * n / denom:6.2f}%  {n * ms / frames:6.2f}  {name}")


if __name__ == "__main__":
    main()
