#!/usr/bin/env python3
"""Reads SkipConsole runs: does a skipped line's speech stop, or play on?

    skip.py <run dir> [<run dir> ...]

The run's audio.wav is laid against its log (engine.log or DeusEx.log) by its
two beeps -- the one at the conversation's start and the one after its end,
a known time apart: the offset that puts both on loud frames is the
alignment, and after the conversation's end only the beep is loud, so a
loud line under one beep time cannot stand in for the pair.

With one run, each skipped line's tail window is measured -- from 0.1 s
after the skip to the same speaker's next line (which replaces the channel)
or the line's would-be end -- as mean energy in dB, with the run's speech
level and its ambient for scale. A level says little on its own: what plays
in the window besides the tail differs (the next speaker's line, choices,
nothing), so the verdict needs runs to compare.

With two or more, the same windows are read side by side and differenced
per tail: the same conversation, the same skips, so a window's difference
is the tail itself. A fork with the StopSound bug shows every NPC line's
tail a dB or more louder than a fixed fork's, and the player's own tails
level -- the player's lines stopped even unfixed, as the player is both the
StopSound caller and the sound's actor, which is what the old match
required. Absolute levels are not comparable between the engines -- the
original's audio passes through wine's mixer -- so only differences between
runs are read.

Levels are dB of full scale.
"""
import array
import math
import os
import re
import sys
import wave

HOP = 0.005  # seconds per energy frame


class Recording:
    def __init__(self, path):
        w = wave.open(path)
        if w.getsampwidth() != 2:
            sys.exit('%s: not 16-bit' % path)
        self.rate = w.getframerate()
        self.channels = w.getnchannels()
        data = array.array('h', w.readframes(w.getnframes()))
        if sys.byteorder != 'little':
            data.byteswap()
        env = []
        step = int(self.rate * HOP) * self.channels
        for i in range(0, len(data) - step + 1, step):
            chunk = data[i:i + step]
            env.append(sum(x * x for x in chunk) / len(chunk))
        self.env = env
        self.dbs = [self.db(e) for e in env]

    def db(self, energy):
        return 10 * math.log10(max(energy, 1e-3) / (32768.0 * 32768.0))

    def frame(self, t):
        return int(round(t / HOP))

    def energy_db(self, start, end):
        """Mean energy as dB between two times."""
        frames = self.env[max(0, self.frame(start)):self.frame(end)]
        if not frames:
            return None
        return self.db(sum(frames) / len(frames))


def read_log(run):
    for name in ('engine.log', 'DeusEx.log'):
        path = os.path.join(run, name)
        if os.path.exists(path):
            return open(path, encoding='latin1').read()
    sys.exit('%s: no engine.log or DeusEx.log' % run)


def parse(log):
    """The run's shape: (start beep, end beep, the tails).

    A tail is (speaker, skip time, remaining seconds, window end): the
    window ends at the same speaker's next line, which replaces the
    channel, or at the line's would-be end, whichever comes first. The
    events are read in log order, so a line that starts a few milliseconds
    after the skip is the next line, not the skipped one."""
    events = []     # (time, kind, ...) in log order
    startbeep = endbeep = None
    for line in log.splitlines():
        if 'DXSKIP:' not in line:
            continue
        m = re.search(r' at (-?\d+\.\d+)', line)
        if not m:
            continue
        t = float(m.group(1))
        what = line.split('DXSKIP:', 1)[1]
        if 'beep' in what:
            if startbeep is None:
                startbeep = t
            else:
                endbeep = t
            continue
        mm = re.match(r'\s*at -?\d+\.\d+: (\w+): .*\(sound \d+, ([\d.]+) s\)', what)
        if mm:
            events.append((t, 'line', mm.group(1), float(mm.group(2))))
            continue
        mm = re.search(r'skipping sound \d+ ([\d.]+) s into a ([\d.]+) s line', what)
        if mm:
            events.append((t, 'skip', float(mm.group(1)), float(mm.group(2))))
    if startbeep is None or endbeep is None:
        sys.exit('no beeps in the log')

    # The conversation's span, for the speech level.
    times = [e[0] for e in events if e[1] == 'line']
    lens = {e[0]: e[3] for e in events if e[1] == 'line'}
    first, last = min(times), max(times) + lens[max(times)]

    # Each skip's speaker: the line most recently logged before it.
    tails = []
    lines = [e for e in events if e[1] == 'line']
    for i, e in enumerate(events):
        if e[1] != 'skip':
            continue
        t, into, total = e[0], e[2], e[3]
        speaker, start = None, None
        for j in range(i - 1, -1, -1):
            if events[j][1] == 'line':
                speaker, start = events[j][2], events[j][0]
                break
        if speaker is None:
            continue
        remaining = total - into
        end = t + remaining
        for le in lines:
            if le[0] > t and le[2] == speaker:
                end = min(end, le[0])
                break
        if end - t < 0.6:
            continue    # too short to read
        tails.append((speaker, t, remaining, end))
    if not tails:
        sys.exit('no skips in the log')
    return startbeep, endbeep, first, last, tails


def align(rec, startbeep, endbeep):
    """The offset that puts both beeps on loud frames; its score is the
    quieter of the two beeps' peaks."""
    best, off = -99.0, None
    lo, hi = 0.5, len(rec.dbs) * HOP - (endbeep - startbeep) - 0.5
    f = lo
    while f < hi:
        o = f - startbeep
        f1, f2 = rec.frame(f), rec.frame(f + (endbeep - startbeep))
        s = min(max(rec.dbs[max(0, f1 - 1):f1 + 2]), max(rec.dbs[max(0, f2 - 1):f2 + 2]))
        if s > best:
            best, off = s, o
        f += HOP
    return off, best


def main(runs):
    if not runs:
        sys.exit(__doc__)
    parsed = []
    for run in runs:
        startbeep, endbeep, first, last, tails = parse(read_log(run))
        rec = Recording(os.path.join(run, 'audio.wav'))
        off, peak = align(rec, startbeep, endbeep)
        name = os.path.basename(run.rstrip('/'))
        parsed.append((name, rec, off, first, last, tails))
        speech = rec.energy_db(first + off, last + off)
        amb = rec.energy_db(endbeep + 0.5 + off, endbeep + 1.5 + off)
        print(f'{name}: beeps {startbeep:.2f}..{endbeep:.2f}, wav offset {off:+.3f} s '
              f'(quieter beep peak {peak:.1f} dB)')
        print(f'  speech {speech:.1f} dB, ambient after the end beep {amb:.1f} dB')
        for speaker, t, remaining, end in tails:
            lvl = rec.energy_db(t + 0.1 + off, end + off)
            print(f'  {speaker:19s} tail {remaining:5.2f} s [{t + 0.1:.2f}, {end:.2f}]: {lvl:6.1f} dB')

    if len(parsed) < 2:
        return
    # The same windows across runs: the difference is the tail itself.
    base = parsed[0]
    print(f'\ntails over {base[0]}:')
    for other in parsed[1:]:
        print(f'  against {other[0]}:')
        for k, tail in enumerate(base[5]):
            a = base[1].energy_db(tail[1] + 0.1 + base[2], tail[3] + base[2])
            ot = other[5][k] if k < len(other[5]) else None
            if ot is None or (len(other[5]) != len(base[5]) and abs(ot[1] - tail[1]) > 0.5):
                print(f'    tail {k + 1}: no matching window')
                continue
            b = other[1].energy_db(ot[1] + 0.1 + other[2], ot[3] + other[2])
            print(f'    {tail[0]:19s} tail {tail[2]:5.2f} s: {b - a:+5.1f} dB')


if __name__ == '__main__':
    main(sys.argv[1:])
