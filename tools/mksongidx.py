#!/usr/bin/env python3
# Copied from the CommanderSID project (tools/songlengths/mksongidx.py, commit 03007ec), GPL-3.0-or-later.
"""HVSC's Songlengths.md5 -> SONGLEN.IDX, the song length index SIDPLAY.PRG reads on the X16.

Songlengths.md5 keys every tune by the MD5 of its whole .sid file:
    ; /MUSICIANS/H/Hubbard_Rob/Commando.sid
    9fb3bd6e4c4c3ca0c45a8d6d4b0e0a5f=3:34 0:13.5 0:09
Scanning that 3+ MB text file on the X16 for each tune would take seconds, so this tool sorts it
into fixed-size records the X16 binary-searches with DOS seeks (the card computes the MD5).

SONGLEN.IDX, little endian:
    0   "SLIX"           magic
    4   u8  version (1)
    5   u8  key length K (6: the first 6 bytes of the MD5)
    6   u16 0
    8   u32 count        tunes
    12  u32 data         where the length lists start
    16  count x (K bytes key, u24 offset from `data`), sorted by key
    data: per tune  u8 songs, songs x u16 seconds (rounded up; 0 = unknown)
"""
import argparse
import re
import struct
import sys

KEY = 6
LINE = re.compile(r"^([0-9a-fA-F]{32})=(.*)$")
TIME = re.compile(r"^(\d+):(\d+)(?:\.(\d+))?")


def seconds(tok):
    """'3:34', '0:13.5', '1:02.500(G)' -> whole seconds, rounded up; None if not a time."""
    m = TIME.match(tok)
    if not m:
        return None
    s = int(m.group(1)) * 60 + int(m.group(2))
    if m.group(3) and int(m.group(3)):
        s += 1
    return min(s, 0xFFFF)


def parse(lines):
    """{md5 bytes: [seconds per song]} (a duplicate MD5 keeps its first entry)."""
    tunes = {}
    for line in lines:
        m = LINE.match(line.strip())
        if not m:
            continue
        key = bytes.fromhex(m.group(1))
        if key in tunes:
            continue
        songs = [seconds(t) for t in m.group(2).split()]
        tunes[key] = [s if s is not None else 0 for s in songs][:255]
    return tunes


def build(tunes):
    keys = sorted(tunes)
    short = [k[:KEY] for k in keys]
    for a, b in zip(short, short[1:]):
        if a == b:
            raise ValueError(f"two tunes share the key {a.hex()}: raise KEY")
    data = bytearray()
    records = bytearray()
    for k in keys:
        off = len(data)
        if off >= 1 << 24:
            raise ValueError("length lists too big for a 24-bit offset")
        records += k[:KEY] + struct.pack("<I", off)[:3]
        songs = tunes[k]
        data += bytes([len(songs)]) + b"".join(struct.pack("<H", s) for s in songs)
    head = b"SLIX" + struct.pack("<BBHII", 1, KEY, 0, len(keys), 16 + len(records))
    return head + records + data


def lookup(idx, md5):
    """What the X16 does: binary search, then the length list. None if the tune is not listed."""
    count, data = struct.unpack_from("<II", idx, 8)
    rec = KEY + 3
    lo, hi = 0, count
    key = md5[:KEY]
    while lo < hi:
        mid = (lo + hi) // 2
        k = idx[16 + mid * rec:16 + mid * rec + KEY]
        if k == key:
            off = int.from_bytes(idx[16 + mid * rec + KEY:16 + (mid + 1) * rec], "little")
            n = idx[data + off]
            return list(struct.unpack_from(f"<{n}H", idx, data + off + 1))
        if k < key:
            lo = mid + 1
        else:
            hi = mid
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("songlengths", help="HVSC's DOCUMENTS/Songlengths.md5")
    ap.add_argument("-o", "--out", default="SONGLEN.IDX")
    a = ap.parse_args()
    with open(a.songlengths, encoding="latin-1") as f:
        tunes = parse(f)
    idx = build(tunes)
    with open(a.out, "wb") as f:
        f.write(idx)
    print(f"{a.out}: {len(tunes)} tunes, {len(idx)} bytes", file=sys.stderr)


if __name__ == "__main__":
    main()
