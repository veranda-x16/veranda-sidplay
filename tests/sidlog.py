#!/usr/bin/env python3
"""The emulated X16-SID card's log (x16emu -sidlog, the sidcard branch) decoded.

A line: "W <cycle> <reg> <val>" (the X16 wrote the card's window), "R ..." (it
read), "P ..." (the card's own player wrote the SID), "C ..." (the card's C64).
The X16's mailbox frames are its writes to $1D: LEN_LO LEN_HI CMD PAYLOAD CRC-8
(poly $07 over CMD and the payload); the card's replies are its reads of $1D.
(After the CommanderSID project's sim/e2e/test_e2e.py, mb_requests.)

GPL-3.0-or-later (see COPYING).
"""
import sys

CMDS = {0x02: "INFO", 0x06: "STORE", 0x0B: "PLAY", 0x0C: "STOP", 0x0E: "C64_STOP",
        0x14: "MD5", 0x15: "PAUSE", 0x16: "C64_SID"}
CYCLES = 8_000_000


def crc8(data):
    c = 0
    for b in data:
        c ^= b
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


class Log:
    def __init__(self, path):
        self.w, self.r, self.p, self.c = [], [], [], []
        for line in open(path):
            t = line.split()
            if len(t) != 4:
                continue
            row = (int(t[1]), int(t[2], 16), int(t[3], 16))
            {"W": self.w, "R": self.r, "P": self.p, "C": self.c}.get(t[0], []).append(row)

    def requests(self):
        """[(cycle, cmd, payload, crc_ok)] from the X16's writes to $1D"""
        mb = [(c, v) for c, r, v in self.w if r == 0x1D]
        out, i = [], 0
        while i + 4 <= len(mb):
            n = mb[i][1] | mb[i + 1][1] << 8
            cmd = mb[i + 2][1]
            body = bytes(v for _, v in mb[i + 3:i + 3 + n])
            crc = mb[i + 3 + n][1] if i + 3 + n < len(mb) else None
            out.append((mb[i][0], cmd, body, crc == crc8(bytes([cmd]) + body)))
            i += 4 + n
        return out

    def commands(self, reg=0x1F):
        """the X16's writes to STATUS: [(cycle, value)] (ARM $84, DISARM $85, RESET $80)"""
        return [(c, v) for c, r, v in self.w if r == reg]


def store_image(reqs, size=0x10000):
    """the card's store as the STORE requests wrote it: (bytes, set of written offsets)"""
    img, got = bytearray(size), set()
    for _, cmd, p, _ in reqs:
        if cmd == 0x06:
            off = int.from_bytes(p[:4], "little")
            img[off:off + len(p) - 4] = p[4:]
            got.update(range(off, off + len(p) - 4))
    return img, got


def main():
    log = Log(sys.argv[1])
    for c, cmd, p, ok in log.requests():
        name = CMDS.get(cmd, "$%02X" % cmd)
        extra = ""
        if cmd == 0x06:
            extra = "offset $%04X, %d bytes" % (int.from_bytes(p[:4], "little"), len(p) - 4)
        else:
            extra = p.hex()
        print("%8.3f s  %-8s %s%s" % (c / CYCLES, name, extra, "" if ok else "  BAD CRC"))
    print("player writes: %d, C64 writes: %d" % (len(log.p), len(log.c)))


if __name__ == "__main__":
    main()
