#!/usr/bin/env python3
"""The brightness of two runs' shots, block by block: the fork's against the
original's (vibe/docs/DEVELOPMENT.md, shots). Each shot is cut into blocks
(16x16 pixels by default) and each block's mean brightness taken; the fork's
shot first goes through the gamma the original's frames carry (1.5 at the runs'
Brightness). Prints, for each pair of shots, the mean ratio over the region
asked for (the whole frame by default), the ratios' spread, and with --blocks
the grid of ratios.

  shots.py <original run> <fork run> [--gamma 1.5] [--block 16]
           [--region x,y,w,h] [--shot N] [--blocks]

A run is its folder under build/dxcap/runs: the original's shots are the
grabber's Shot0000.png..., the fork's Shot0000.bmp...; a pair is the two of one
number (--shot picks one), the original's missing ones passed over. Needs
ImageMagick (magick)."""
import argparse, os, re, subprocess, sys

def means(path, gamma, block):
    # One pixel per block: ImageMagick's scale averages each block
    cmd = ['magick', path]
    if gamma != 1.0:
        cmd += ['-gamma', str(gamma)]
    cmd += ['-colorspace', 'Gray', '-define', 'filter:blur=1', '-scale', f'{100.0 / block}%', '-depth', '16', 'txt:-']
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    grid = {}
    w = h = 0
    for line in out.splitlines():
        m = re.match(r'(\d+),(\d+): \((\d+)', line)
        if not m:
            continue
        x, y, v = int(m[1]), int(m[2]), int(m[3])
        grid[(x, y)] = v / 65535.0
        w, h = max(w, x + 1), max(h, y + 1)
    return grid, w, h

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('original')
    ap.add_argument('fork')
    ap.add_argument('--gamma', type=float, default=1.5, help="the gamma put on the fork's shot (1.5)")
    ap.add_argument('--block', type=int, default=16, help='the block size in pixels (16)')
    ap.add_argument('--region', help='x,y,w,h in pixels: the region whose mean ratio is given')
    ap.add_argument('--shot', type=int, help='one shot number, else every pair')
    ap.add_argument('--blocks', action='store_true', help='print the grid of ratios')
    a = ap.parse_args()
    numbers = sorted(int(m[1]) for f in os.listdir(a.original) for m in [re.match(r'Shot(\d+)\.png$', f)] if m)
    if a.shot is not None:
        numbers = [n for n in numbers if n == a.shot]
    if not numbers:
        sys.exit('no shots of the original to compare')
    for n in numbers:
        fork = os.path.join(a.fork, 'Shot%04d.bmp' % n)
        if not os.path.exists(fork):
            print('shot %d: the fork has none' % n)
            continue
        og, w, h = means(os.path.join(a.original, 'Shot%04d.png' % n), 1.0, a.block)
        fg, fw, fh = means(fork, a.gamma, a.block)
        if (w, h) != (fw, fh):
            sys.exit('shot %d: %dx%d blocks against %dx%d' % (n, w, h, fw, fh))
        x0, y0, x1, y1 = 0, 0, w, h
        if a.region:
            rx, ry, rw, rh = (int(v) for v in a.region.split(','))
            x0, y0 = rx // a.block, ry // a.block
            x1, y1 = (rx + rw + a.block - 1) // a.block, (ry + rh + a.block - 1) // a.block
        ratios = []
        osum = fsum = 0.0
        for y in range(y0, y1):
            for x in range(x0, x1):
                o, f = og[(x, y)], fg[(x, y)]
                osum += o
                fsum += f
                if o >= 2.0 / 255:
                    ratios.append(f / o)
        ratios.sort()
        if not ratios:
            print('shot %d: the original is black in the region' % n)
            continue
        mid = ratios[len(ratios) // 2]
        print('shot %d: fork/original %.3f over the region (%d blocks of %d; the original %.3f, the fork %.3f); blocks %.2f to %.2f, median %.2f'
              % (n, fsum / osum if osum else 0.0, len(ratios), (x1 - x0) * (y1 - y0), osum / ((x1 - x0) * (y1 - y0)), fsum / ((x1 - x0) * (y1 - y0)), ratios[0], ratios[-1], mid))
        if a.blocks:
            for y in range(y0, y1):
                row = []
                for x in range(x0, x1):
                    o, f = og[(x, y)], fg[(x, y)]
                    row.append('  ..' if o < 2.0 / 255 else '%4.2f' % (f / o))
                print(' '.join(row))

if __name__ == '__main__':
    main()
