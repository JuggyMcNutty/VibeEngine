#!/usr/bin/env python3
"""Keeps the marked frames of a window's grabs.

Reads the window of that name on an X display five times a second
(xdotool's search, ImageMagick's import) until killed. A frame whose view
carries CaptureConsole's mark -- a magenta block, then the shot's number in
eight black or white blocks of 12 pixels -- is written as ShotNNNN.ppm to the
output folder, cropped to the view from the mark's corner; the last one of
each number wins, the steadiest.

    grab.py <out dir> <display> <window name pattern> <view width> <view height>
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


def parse_ppm(data):
    """Width, height and pixels of a binary PPM (import's, no comments)."""
    if not data.startswith(b'P6'):
        return None
    fields, i = [], 2
    while len(fields) < 3:
        while i < len(data) and data[i:i + 1].isspace():
            i += 1
        j = i
        while j < len(data) and not data[j:j + 1].isspace():
            j += 1
        if j == i:
            return None
        fields.append(int(data[i:j]))
        i = j
    width, height, _ = fields
    pixels = data[i + 1:i + 1 + width * height * 3]
    if len(pixels) < width * height * 3:
        return None
    return width, height, pixels


def grab(env, name):
    found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--name', name],
                           env=env, capture_output=True, text=True)
    ids = found.stdout.split()
    if not ids:
        return None
    shot = subprocess.run(['import', '-window', ids[0], '-depth', '8', 'ppm:-'],
                          env=env, capture_output=True)
    return parse_ppm(shot.stdout)


def main():
    out, display, name = sys.argv[1], sys.argv[2], sys.argv[3]
    vw, vh = int(sys.argv[4]), int(sys.argv[5])
    os.makedirs(out, exist_ok=True)
    env = dict(os.environ, DISPLAY=display)
    env.pop('WAYLAND_DISPLAY', None)
    while True:
        started = time.monotonic()
        image = grab(env, name)
        time.sleep(max(0.0, 0.2 - (time.monotonic() - started)))
        if not image:
            continue
        width, height, frame = image
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
