#!/usr/bin/env python3
"""A master server's side of an uplink, on this machine: it takes a server's
heartbeats on UDP and asks the query port each names what a master asks --
\\secure\\ with a challenge, then \\basic\\ and \\info\\ -- printing each
heartbeat and each answer, so either engine's uplink and query answerer can
be diffed (vibe/docs/DEVELOPMENT.md, scripted runs: net tests). A run's
server announces itself to it with DXCAP_UPLINK=127.0.0.1:27900.

    vibe/tools/dxcap/fakemaster.py [port] [seconds] [challenge]

The port defaults to 27900, the game's master servers'; it listens 120 s.
"""
import socket
import sys
import time


def ask(port, text, wait=1.5):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(wait)
    s.sendto(text.encode('latin1'), ('127.0.0.1', port))
    replies = []
    end = time.time() + wait
    while time.time() < end:
        try:
            data, _ = s.recvfrom(4096)
            replies.append(data.decode('latin1', 'replace'))
        except socket.timeout:
            break
    s.close()
    return replies


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 27900
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 120.0
    challenge = sys.argv[3] if len(sys.argv) > 3 else 'ABCDEF'
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('127.0.0.1', port))
    s.settimeout(0.5)
    start = time.time()
    print(f'listening on 127.0.0.1:{port} for {seconds:.0f} s', flush=True)
    while time.time() - start < seconds:
        try:
            data, addr = s.recvfrom(4096)
        except socket.timeout:
            continue
        text = data.decode('latin1', 'replace')
        print(f'{time.time() - start:6.1f} s from {addr[0]}:{addr[1]}: {text!r}', flush=True)
        parts = text.split('\\')
        if len(parts) > 2 and parts[1] == 'heartbeat':
            qport = int(parts[2])
            for q in (f'\\secure\\{challenge}', '\\basic\\', '\\info\\'):
                print(f'  {qport} {q!r}: {ask(qport, q)!r}', flush=True)


if __name__ == '__main__':
    main()
