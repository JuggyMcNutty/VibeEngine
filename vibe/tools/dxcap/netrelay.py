#!/usr/bin/env python3
"""A UDP relay between a net test's client and its server, which reads what
passes: tcpdump cannot capture here (no CAP_NET_RAW).

    netrelay.py [--listen <port>] [--server <host:port>] [--log <file>] [--secs <s>]

It listens on --listen (7790, where a client opens 127.0.0.1:7790) and
forwards each client's datagrams to --server (127.0.0.1:7792: the server run
with DXCAP_PORT=7792), and the server's answers back. Every datagram is
decoded as the original's packets are (dx-reverse-info network.md,
packets): its number, its acks, and each bunch's header -- control, open,
close and reliable bits, the channel's index, a reliable one's sequence, the
channel's type when reliable or opening, the data's length in bits. --log
writes a line for each datagram; every second a row per direction counts
packets, bytes, acks and bunches by the channel's type (control, actor,
file, as its opening bunch named it) and reliability. A packet that does not
decode is counted apart. It exits after --secs (300), or on Ctrl-C.
"""
import argparse
import select
import socket
import sys
import time

MAX_PACKET = 512
MAX_PACKET_ID = 16384
MAX_CHANNELS = 1023
MAX_CH_SEQUENCE = 1024
MAX_CH_TYPE = 8
TYPES = {1: "control", 2: "actor", 3: "file"}


class Bits:
    """The packet's bits, first bit lowest; ReadInt as FBitReader's."""

    def __init__(self, data, num):
        self.data, self.num, self.pos = data, num, 0

    def bit(self):
        if self.pos >= self.num:
            raise ValueError("past the end")
        b = (self.data[self.pos >> 3] >> (self.pos & 7)) & 1
        self.pos += 1
        return b

    def int(self, value_max):
        value, mask = 0, 1
        while value + mask < value_max and mask:
            if self.bit():
                value |= mask
            mask <<= 1
        return value

    def skip(self, n):
        if self.pos + n > self.num:
            raise ValueError("bunch past the end")
        self.pos += n


def decode(data):
    """(packet id, acks, bunches) of a datagram; each bunch a dict."""
    if not data or data[-1] == 0:
        raise ValueError("no trailing bit")
    last, num = data[-1], len(data) * 8 - 1
    while not last & 0x80:
        last = (last << 1) & 0xFF
        num -= 1
    r = Bits(data, num)
    packet = r.int(MAX_PACKET_ID)
    acks, bunches = [], []
    while r.pos < r.num:
        if r.bit():
            acks.append(r.int(MAX_PACKET_ID))
            continue
        b = {"control": r.bit()}
        b["open"] = r.bit() if b["control"] else 0
        b["close"] = r.bit() if b["control"] else 0
        b["reliable"] = r.bit()
        b["ch"] = r.int(MAX_CHANNELS)
        b["seq"] = r.int(MAX_CH_SEQUENCE) if b["reliable"] else None
        b["type"] = r.int(MAX_CH_TYPE) if (b["reliable"] or b["open"]) else None
        b["bits"] = r.int(MAX_PACKET * 8)
        r.skip(b["bits"])
        bunches.append(b)
    return packet, acks, bunches


class Direction:
    """One way's channel types (from their opening bunches) and counts."""

    def __init__(self, name):
        self.name = name
        self.types = {}
        self.reset()

    def reset(self):
        self.packets = self.bytes = self.acks = self.bad = 0
        self.counts = {}

    def take(self, data, log, now):
        self.packets += 1
        self.bytes += len(data)
        try:
            packet, acks, bunches = decode(data)
        except ValueError as e:
            self.bad += 1
            if log:
                log.write("%.4f %s %d bytes: undecoded (%s)\n" % (now, self.name, len(data), e))
            return
        self.acks += len(acks)
        parts = []
        for b in bunches:
            if b["type"] is not None:
                self.types[b["ch"]] = b["type"]
            if b["close"]:
                kind = TYPES.get(self.types.pop(b["ch"], b["type"]), "?")
            else:
                kind = TYPES.get(self.types.get(b["ch"], b["type"]), "?")
            key = (kind, "rel" if b["reliable"] else "unrel")
            self.counts[key] = self.counts.get(key, 0) + 1
            flags = ("O" if b["open"] else "") + ("C" if b["close"] else "") + ("R" if b["reliable"] else "U")
            parts.append("ch%d:%s:%s:%d%s" % (b["ch"], kind, flags, b["bits"], "" if b["seq"] is None else "#%d" % b["seq"]))
        if log:
            log.write("%.4f %s %d bytes pk %d acks %d %s\n" % (now, self.name, len(data), packet, len(acks), " ".join(parts)))

    def row(self, second):
        kinds = " ".join("%s %s %d" % (k[0], k[1], n) for k, n in sorted(self.counts.items()))
        line = "%4d %s %3d packets %6d bytes %3d acks %s%s" % (
            second, self.name, self.packets, self.bytes, self.acks, kinds, " (%d undecoded)" % self.bad if self.bad else "")
        self.reset()
        return line


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--listen", type=int, default=7790)
    ap.add_argument("--server", default="127.0.0.1:7792")
    ap.add_argument("--log")
    ap.add_argument("--secs", type=float, default=300.0)
    args = ap.parse_args()
    host, port = args.server.rsplit(":", 1)
    server = (host, int(port))

    front = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    front.bind(("0.0.0.0", args.listen))
    log = open(args.log, "w") if args.log else None
    upstream = {}   # client address -> its socket toward the server
    clients = {}    # that socket -> the client address
    c2s, s2c = Direction("c2s"), Direction("s2c")
    start = time.monotonic()
    second = 0
    print("relay: %d -> %s:%d" % (args.listen, server[0], server[1]), flush=True)
    while True:
        now = time.monotonic() - start
        if now >= args.secs:
            break
        if now >= second + 1:
            for d in (c2s, s2c):
                if d.packets:
                    print(d.row(second), flush=True)
                else:
                    d.reset()
            second = int(now)
            if log:
                log.flush()
        ready, _, _ = select.select([front] + list(clients), [], [], 0.05)
        for s in ready:
            data, addr = s.recvfrom(65536)
            now = time.monotonic() - start
            if s is front:
                up = upstream.get(addr)
                if up is None:
                    up = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    up.bind(("127.0.0.1", 0))
                    upstream[addr] = up
                    clients[up] = addr
                    print("relay: client %s:%d" % addr, flush=True)
                c2s.take(data, log, now)
                up.sendto(data, server)
            else:
                s2c.take(data, log, now)
                front.sendto(data, clients[s])
    if log:
        log.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
