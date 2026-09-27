#!/usr/bin/env python3
"""Keeps the marked frames of a screen grab.

Reads an X display's screen five times a second (ImageMagick's import) until
killed. A frame whose view carries CaptureConsole's mark -- a magenta block,
then the shot's number in eight black or white blocks of 12 pixels -- is
written as ShotNNNN.ppm to the output folder, cropped to the view from the
mark's corner; the last one of each number wins, the steadiest.

    grab.py <out dir> <display> <width> <height> <view width> <view height>
"""
import os
import subprocess
import sys
import time

BLOCK = 12


def is_magenta(p):
    return p[0] > 200 and p[1] < 60 and p[2] > 200


def find_mark(frame, width, height):
    """The top-left corner of a magenta block in the frame's top-left area."""
    for y in range(0, min(height, 120)):
        row = y * width * 3
        for x in range(0, min(width, 200)):
            i = row + x * 3
            if is_magenta(frame[i:i + 3]):
                # Its full block, not a stray pixel.
                j = row + (x + BLOCK - 1) * 3 + (BLOCK - 1) * width * 3
                if j + 3 <= len(frame) and is_magenta(frame[j:j + 3]):
                    return x, y
                return None
    return None


def read_number(frame, width, x, y):
    number = 0
    for bit in range(8):
        cx = x + BLOCK + bit * BLOCK + BLOCK // 2
        cy = y + BLOCK // 2
        i = (cy * width + cx) * 3
        if sum(frame[i:i + 3]) > 3 * 200:
            number |= 1 << bit
    return number


def main():
    out, display = sys.argv[1], sys.argv[2]
    width, height, vw, vh = (int(a) for a in sys.argv[3:7])
    os.makedirs(out, exist_ok=True)
    size = width * height * 3
    env = dict(os.environ, DISPLAY=display)
    while True:
        started = time.monotonic()
        grab = subprocess.run(['import', '-window', 'root', '-depth', '8', 'rgb:-'],
                              env=env, capture_output=True)
        frame = grab.stdout
        time.sleep(max(0.0, 0.2 - (time.monotonic() - started)))
        if len(frame) < size:
            continue
        mark = find_mark(frame, width, height)
        if not mark:
            continue
        x, y = mark
        number = read_number(frame, width, x, y)
        w, h = min(vw, width - x), min(vh, height - y)
        rows = [frame[((y + r) * width + x) * 3:((y + r) * width + x + w) * 3] for r in range(h)]
        with open(os.path.join(out, 'Shot%04d.ppm' % number), 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (w, h))
            for r in rows:
                f.write(r)


if __name__ == '__main__':
    main()
