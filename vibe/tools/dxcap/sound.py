#!/usr/bin/env python3
"""Reads a SoundConsole run: the recording against the moves in its log.

    sound.py <run dir>

The run's audio.wav is laid against its log (engine.log or DeusEx.log) by
each scenario's three beeps, half a second apart, which SoundConsole plays
before the scenario starts: the console's clock and the recording's differ by
the time a map takes to load, so each scenario is laid by its own beeps.

  wall:   the level and the brightness of the ambient sound heard from the
          open spot and from the spot behind a wall;
  reverb: for each shot in the reverb zone and outside it, its peak, how long
          it rings (until 60 dB under its peak), and its tail: the energy
          from 0.5 to 1.5 s after the peak against the first 0.45 s, the
          shot's own length -- what the zone adds;
  pan:    each channel's peak for beeps from a path node to the right, to
          the left and ahead.

Levels are dB of full scale. Brightness is the frequency of a sine with the
same share of first-difference energy -- a low-pass filter lowers it.
"""
import array
import math
import os
import re
import sys
import wave

HOP = 0.005  # seconds per envelope frame


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
        self.data = data
        self.frames = len(data) // self.channels
        self.hop = int(self.rate * HOP)
        # The energy envelope, both channels together, and each channel's.
        env = []
        per = [[] for _ in range(self.channels)]
        step = self.hop * self.channels
        for i in range(0, len(data) - step + 1, step):
            chunk = data[i:i + step]
            env.append(sum(x * x for x in chunk) / len(chunk))
            for c in range(self.channels):
                one = chunk[c::self.channels]
                per[c].append(sum(x * x for x in one) / len(one))
        self.env = env
        self.per = per

    def channel_peaks(self, start, end):
        """Each channel's loudest frame (dB) between two recording times."""
        a, b = max(0, self.frame(start)), self.frame(end)
        return [self.db(max(env[a:b], default=0.0)) for env in self.per]

    def db(self, energy):
        return 10 * math.log10(max(energy, 1e-3) / (32768.0 * 32768.0))

    def frame(self, t):
        return int(round(t / HOP))

    def level(self, start, end):
        """Median level (dB) and brightness (Hz) between two recording times."""
        frames = sorted(self.env[max(0, self.frame(start)):self.frame(end)])
        a = max(0, int(start * self.rate)) * self.channels
        b = min(self.frames, int(end * self.rate)) * self.channels
        c = self.channels
        chunk = self.data[a:b]
        if not frames or len(chunk) <= 2 * c:
            return None, None
        e = sum(x * x for x in chunk)
        d = sum((chunk[i] - chunk[i - c]) ** 2 for i in range(c, len(chunk)))
        ratio = min(4.0, d / max(e, 1.0))
        bright = self.rate / math.pi * math.asin(math.sqrt(ratio) / 2)
        return self.db(frames[len(frames) // 2]), bright


def read_log(run):
    for name in ('engine.log', 'DeusEx.log'):
        path = os.path.join(run, name)
        if os.path.exists(path):
            return open(path, encoding='latin1').read()
    sys.exit('%s: no engine.log or DeusEx.log' % run)


def events(log):
    """(time, what) for each timed DXCAP line, in order."""
    out = []
    for m in re.finditer(r'DXCAP: (.*?) at (-?\d+\.\d+)', log):
        out.append((float(m.group(2)), m.group(1)))
    return out


def onsets(rec):
    """Onset strength per frame: its rise over the lowest of the 50 ms before."""
    dbs = [rec.db(e) for e in rec.env]
    out = [0.0] * len(dbs)
    for i in range(10, len(dbs)):
        out[i] = max(0.0, dbs[i] - min(dbs[i - 10:i]))
    return out


def align(rec, strength, beeps, lo, hi):
    """The recording time of console time 0 that puts all the beeps on onsets.

    An offset scores its weakest beep's onset, so a single loud onset -- the
    sound starting out of silence -- cannot stand in for three beeps.
    """
    best, best_offset = -1.0, None
    steps = int((hi - lo) / HOP)
    for s in range(steps):
        offset = lo + s * HOP
        score = None
        for t in beeps:
            f = rec.frame(t + offset)
            if f < 3 or f + 3 >= len(strength):
                score = -1.0
                break
            onset = max(strength[f - 3:f + 4])
            score = onset if score is None else min(score, onset)
        if score is not None and score > best:
            best, best_offset = score, offset
    return best_offset, best


def scenario(evts, first, last):
    """The events from the one whose text starts with first to last's."""
    out, on = [], False
    for t, what in evts:
        if what.startswith(first):
            on = True
        if on:
            out.append((t, what))
            if what.startswith(last):
                break
    return out


def beeps_after(evts, t0):
    b = [t for t, what in evts if what == 'beep' and t >= t0]
    return b[:3]


def report_wall(rec, strength, evts):
    """Returns the scenario's offset, where the next one's search starts."""
    wall = scenario(evts, 'open', 'wall scenario ends')
    if not wall:
        print('wall: not in the log')
        return -5.0
    beeps = beeps_after(wall, wall[0][0])
    # The recording starts a few seconds before the level (6 s at most so
    # far): a wider search can land on a louder triple of onsets further on.
    offset, score = align(rec, strength, beeps, -5.0, 15.0)
    print('wall: beeps found at %+.3f s from the log (onset %.1f dB)' % (offset, score))
    moves = [(t, what) for t, what in wall if what in ('open', 'wall', 'wall scenario ends')]
    results = {'open': [], 'wall': []}
    for (t, what), (t_next, _) in zip(moves, moves[1:]):
        start = t + 0.5
        if beeps and start < beeps[-1] + 0.5 and t_next > beeps[-1]:
            start = beeps[-1] + 0.5
        lvl, bright = rec.level(start + offset, t_next - 0.2 + offset)
        if lvl is None:
            continue
        results[what].append((lvl, bright))
        print('  %-4s %6.2f-%6.2f s  %6.1f dB  %5.0f Hz' % (what, start, t_next - 0.2, lvl, bright))
    if results['open'] and results['wall']:
        mean = lambda xs: sum(xs) / len(xs)
        o = mean([l for l, _ in results['open']])
        w = mean([l for l, _ in results['wall']])
        ob = mean([b for _, b in results['open']])
        wb = mean([b for _, b in results['wall']])
        print('  behind the wall: %+.1f dB, brightness %.0f -> %.0f Hz' % (w - o, ob, wb))
    return offset


def shot(rec, t):
    """Peak (dB), ring (s) and tail (dB) of a shot fired at recording time t."""
    env = rec.env
    f0 = rec.frame(t - 0.05)
    f1 = rec.frame(t + 0.3)
    if f0 < 0 or f1 + rec.frame(2.8) >= len(env):
        return None, None, None
    peak_f = max(range(f0, f1), key=lambda f: env[f])
    peak = rec.db(env[peak_f])
    # Up to 2.8 s: the next shot comes 3 s after.
    ring = max((f for f in range(peak_f, peak_f + rec.frame(2.8)) if rec.db(env[f]) > peak - 60), default=peak_f)
    body = sum(env[peak_f:peak_f + rec.frame(0.45)])
    tail = sum(env[peak_f + rec.frame(0.5):peak_f + rec.frame(1.5)])
    return peak, (ring - peak_f) * HOP, 10 * math.log10(max(tail, 1e-3) / max(body, 1e-3))


def report_reverb(rec, strength, evts, after):
    """after: the earlier scenario's offset; a map's load only adds to it.

    Returns the scenario's offset, which the pan that follows it shares."""
    rev = scenario(evts, 'in the zone', 'shot outside')
    rest = [(t, w) for t, w in evts if rev and t >= rev[0][0]]
    if not rev:
        print('reverb: not in the log')
        return None, []
    beeps = beeps_after(rest, rev[0][0])
    offset, score = align(rec, strength, beeps, after - 0.5, after + 20.0)
    print('reverb: beeps found at %+.3f s from the log (onset %.1f dB)' % (offset, score))
    results = {'in': [], 'out': []}
    for t, what in rest:
        if not what.startswith('shot'):
            continue
        where = 'in' if 'in the zone' in what else 'out'
        peak, ring, tail = shot(rec, t + offset)
        if peak is None:
            print('  %-3s %6.2f s  not in the recording' % (where, t))
            continue
        results[where].append((peak, ring, tail))
        print('  %-3s %6.2f s  peak %6.1f dB  rings %.2f s  tail %+6.1f dB' % (where, t, peak, ring, tail))
    if results['in'] and results['out']:
        # Medians: a stray sound can fall in one shot's window.
        median = lambda xs: sorted(xs)[len(xs) // 2]
        i = [median([r[k] for r in results['in']]) for k in range(3)]
        o = [median([r[k] for r in results['out']]) for k in range(3)]
        print('  in the zone: peak %.1f dB, rings %.2f s, tail %.1f dB' % tuple(i))
        print('  outside:     peak %.1f dB, rings %.2f s, tail %.1f dB' % tuple(o))
    return offset, rest


def report_pan(rec, offset, rest):
    pans = [(t, what) for t, what in rest if what.startswith('pan ')]
    if offset is None or not pans:
        print('pan: not in the log')
        return
    print('pan: left and right channel peaks')
    for t, what in pans:
        peaks = rec.channel_peaks(t + offset - 0.05, t + offset + 0.4)
        if len(peaks) != 2:
            print('  %-5s not stereo' % what[4:])
            continue
        print('  %-5s %6.2f s  L %6.1f dB  R %6.1f dB  R-L %+5.1f dB' % (
            what[4:], t, peaks[0], peaks[1], peaks[1] - peaks[0]))


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    run = sys.argv[1]
    rec = Recording(os.path.join(run, 'audio.wav'))
    evts = events(read_log(run))
    strength = onsets(rec)
    print('%s: %.1f s of audio' % (run, rec.frames / rec.rate))
    offset = report_wall(rec, strength, evts)
    offset, rest = report_reverb(rec, strength, evts, offset)
    report_pan(rec, offset, rest)


if __name__ == '__main__':
    main()
