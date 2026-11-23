#!/usr/bin/env python3
"""The Veranda SID Player's voices panel read back from a VRAM dump.

    check_ui.py <dump.bin> <rom.bin> shadow     the known shadow (tests/tunes/shadow.s)
    check_ui.py <dump.bin> <rom.bin> show       print the panel's text and bars
    check_ui.py <dump.bin> <rom.bin> chrome     Veranda Look 2's bar and window

The look (chrome): palette entries 0-15 Look 2's light ones (no settings
file: the Veranda theme, Veranda DECISIONS V74); the kernel's menu bar
(card rows 0-1: the mark's two cards cyan, the rest chrome; the mark's and
the program's name's ink; its line at y 15); the window's chrome (vrwin.h,
V79: the title band, card rows 3-4, on the accent, its line at y 39 and the
close glyph at its right; the status bar, rows 57-58, chrome, its line at
y 456; the frame's sides; the dither shadow right and below); the desk's
colour round it; the panels' chrome heads; the body paper.

The text in 8 x 8 cards (PutCards with the ROM's font, src/draw.c) is read
back by matching each card of the 1 bpp bitmap against the font's glyphs
(plain or reversed: a set flag is drawn reversed); the bars are the colour
map's cards (a card's paper colour, Veranda DECISIONS V30). The scopes: the
trace's rows in each scope's bitmap.

GPL-3.0-or-later (see COPYING).
"""
import sys

VDUMP = 132878
VRAM, COMPOSER, PALETTE = 131072, 256, 512
STRIDE = 80
SMILEY = bytes([0x7E, 0x81, 0xA5, 0x81, 0xBD, 0x99, 0x81, 0x7E])

# the layout (src/sidp.h)
V_HEAD, V_SCOPE, V_WAVE, V_ENV, V_PW, V_TEXT = 35, 37, 43, 45, 46, 48
SCOPE_H = 40
F_ROW1, F_ROW2 = 52, 54
COL = [2, 28, 54]
C_ENV_OFF = 2          # (CHROME: Look 2's palette, src/sidp.h)
# Look 2's palette (light), entries 0-15, and the theme Veranda's colours
LOOK2 = [0xFFF, 0x223, 0xDDE, 0xEEF, 0x9CF, 0xACE, 0xFD5, 0xF87,
         0x8D9, 0xB9F, 0xFA5, 0x6CE, 0x36A, 0x8BE, 0x5A7, 0xDAC]
CHROME, TITLE, CYAN, SKY2 = 2, 5, 11, 13
WIN_X, WIN_Y, WIN_W, WIN_H = 1, 3, 78, 56


def font(rom):
    """glyphs 32-127 of the ROM's 8 x 8 font (ROM bank 6, found by its smiley)"""
    b = rom[6 * 16384:7 * 16384]
    for a in range(0, 0x4000, 0x800):
        if b[a + 8:a + 16] == SMILEY:
            return {bytes(b[a + 8 * c:a + 8 * c + 8]): chr(c) for c in range(32, 128)}
    raise SystemExit("check_ui: no font in the ROM")


class Screen:
    def __init__(self, dump, rom):
        d = open(dump, "rb").read()
        self.v = d[len(d) - VDUMP:]
        self.glyphs = font(open(rom, "rb").read())
        lay = [self.v[VRAM + COMPOSER + PALETTE + 7 * i:][:7] for i in range(2)]
        bm = 1 if self.v[VRAM] & 0x20 and lay[1][0] & 4 else 0
        self.base = (lay[bm][2] >> 2) << 11
        self.cmap = None
        if bm == 1 and self.v[VRAM] & 0x10:
            cfg = lay[0]
            self.cmap = (32 << ((cfg[0] >> 4) & 3), cfg[1] << 9)

    def card(self, x, y, py=None):
        top = 8 * y if py is None else py
        return bytes(self.v[self.base + (top + i) * STRIDE + x] for i in range(8))

    def char(self, x, y, py=None):
        """(character, reversed) of card x, y (or at pixel row py); '?' if no
        glyph matches"""
        c = self.card(x, y, py)
        if c in self.glyphs:
            return self.glyphs[c], False
        inv = bytes(b ^ 0xFF for b in c)
        if inv in self.glyphs:
            return self.glyphs[inv], True
        return "?", False

    def text(self, x, y, n, py=None):
        return "".join(self.char(x + i, y, py)[0] for i in range(n))

    def rev(self, x, y, n):
        """the text's reversed characters (a set flag) as letters, the rest as '.'"""
        return "".join(ch if r else "." for ch, r in (self.char(x + i, y) for i in range(n)))

    def colour(self, x, y):
        if not self.cmap:
            return 0
        w, base = self.cmap
        return self.v[base + 2 * (y * w + x) + 1] >> 4

    def lit(self, x, y, n, off=C_ENV_OFF):
        """how many of the n cards from x are lit (not the unlit colour)"""
        return sum(1 for i in range(n) if self.colour(x + i, y) not in (off, 0))

    def ink(self, x, y, w, h):
        """ink pixels in cards x .. x + w - 1, pixel rows y .. y + h - 1 (their
        inner part: the frame's columns left out)"""
        n = 0
        for yy in range(y, y + h):
            for i in range(w):
                b = self.v[self.base + yy * STRIDE + x + i]
                if i == 0:
                    b &= 0x3F
                if i == w - 1:
                    b &= 0xFC
                n += bin(b).count("1")
        return n

    def scope_rows(self, v):
        """the rows of voice v's scope with trace pixels (its inner 46 rows, the
        middle line's dots left out)"""
        rows = set()
        for r in range(1, SCOPE_H - 1):
            y = V_SCOPE * 8 + r
            bits = 0
            for i in range(24):
                b = self.v[self.base + y * STRIDE + COL[v] + i]
                if i == 0:
                    b &= 0x7F
                if i == 23:
                    b &= 0xFE
                if r == SCOPE_H // 2:
                    b &= ~0x22 & 0xFF
                bits |= b
            if bits:
                rows.add(r)
        return rows


def show(s):
    for v in range(3):
        x = COL[v]
        print("voice %d: head %r" % (v + 1, s.text(x + 20, 0, 3, V_HEAD * 8 + 4)))
        print("   %r  %r" % (s.text(x, V_WAVE, 24), s.rev(x, V_WAVE, 24)))
        for row in range(V_TEXT, V_TEXT + 3):
            print("   %r" % s.text(x, row, 12))
        print("   env %d cards lit, pw %d; scope rows %s" % (s.lit(x + 4, V_ENV, 20), s.lit(x + 4, V_PW, 20),
                                                        sorted(s.scope_rows(v))))
    print("filter %r" % s.text(2, F_ROW1, 51))
    print("       %r" % s.text(2, F_ROW2, 51))
    print("       %r" % s.rev(2, F_ROW2, 51))
    print("cutoff bar %d, volume bar %d" % (s.lit(20, F_ROW1, 24), s.lit(43, F_ROW2, 8)))


def check_shadow(s):
    bad = []

    def want(what, got, exp):
        if got != exp:
            bad.append("%s: %r, want %r" % (what, got, exp))

    for v, (adsr, pw, freq, flags, pwn) in enumerate([
            ("ADSR 09A0", "PW    800", "1D45 440.1Hz", "........PUL.....GT......", 11),
            ("ADSR 22F4", "PW    400", "22CE 523.3Hz", "....SAW.........GT.SY...", 6),
            ("ADSR 00F0", "PW    C00", "2BDA 659.4Hz", "............NOI.GT......", 16)]):
        x = COL[v]
        want("voice %d note" % (v + 1), s.text(x + 20, 0, 3, V_HEAD * 8 + 4), ["A-4", "C-5", "E-5"][v])
        want("voice %d ADSR" % (v + 1), s.text(x, V_TEXT, 12).rstrip(), adsr)
        want("voice %d pulse width" % (v + 1), s.text(x, V_TEXT + 1, 12).rstrip(), pw)
        want("voice %d frequency" % (v + 1), s.text(x, V_TEXT + 2, 12).rstrip(), freq)
        want("voice %d waveform text" % (v + 1), s.text(x, V_WAVE, 24), "TRI SAW PUL NOI GT SY RM")
        want("voice %d waveform bits set" % (v + 1), s.rev(x, V_WAVE, 24), flags)
        want("voice %d pulse width bar" % (v + 1), s.lit(x + 4, V_PW, 20), pwn)
        if not s.ink(x + 13, V_TEXT * 8 - 3, 11, 26):
            bad.append("voice %d: no ADSR graph" % (v + 1))
    env = [s.lit(COL[v] + 4, V_ENV, 20) for v in range(3)]
    want("voice 1 envelope (sustain $A)", env[0], 13)
    want("voice 2 envelope (sustain $F)", env[1], 20)
    if env[2] < 18:
        bad.append("voice 3 envelope (ENV3 read back): %d cards, want 18-20" % env[2])
    want("filter row 1", " ".join(s.text(2, F_ROW1, 51).split()), "FILTER CUTOFF 7FF RES F")
    want("filter row 2", " ".join(s.text(2, F_ROW2, 51).split()),
         "MODE LP BP HP 3OFF ROUTE 1 2 3 EXT VOL F")
    want("filter bits set", s.rev(2, F_ROW2, 51).replace(".", " ").split(), ["LP", "BP", "1", "2"])
    want("cutoff bar ($7FF: 23 of 24)", s.lit(20, F_ROW1, 24), 23)
    want("volume bar (15: 8 of 8)", s.lit(43, F_ROW2, 8), 8)
    if not s.ink(53, F_ROW1 * 8 + 1, 24, 20):
        bad.append("no filter graph")
    # the scopes: voice 1 a pulse (two levels), voice 2 a saw (many rows), voice 3 noise
    for v in range(3):
        rows = s.scope_rows(v)
        if len(rows) < 8:
            bad.append("voice %d's scope: trace on %d rows" % (v + 1, len(rows)))
    for b in bad:
        print("FAIL  " + b)
    return not bad


def check_chrome(s):
    bad = []

    def want(what, got, exp):
        if got != exp:
            bad.append("%s: %r, want %r" % (what, got, exp))

    pal = [s.v[0x1FA00 + 2 * i] | s.v[0x1FA01 + 2 * i] << 8 for i in range(16)]
    want("the palette", ["%03X" % c for c in pal], ["%03X" % c for c in LOOK2])
    if not s.cmap:
        bad.append("no colour map")
        for b in bad:
            print("FAIL  " + b)
        return False

    def cards(x0, x1, y0, y1):
        return {s.colour(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}

    def row(y, x0, x1):
        return [s.v[s.base + y * STRIDE + x] for x in range(x0, x1 + 1)]

    def inked(x0, x1, y0, y1):
        return sum(bin(b).count("1") for y in range(y0, y1) for b in row(y, x0, x1))

    r = WIN_X + WIN_W - 1
    b = WIN_Y + WIN_H - 1
    want("the menu bar's mark (cards 0-1, rows 0-1)", cards(0, 1, 0, 1), {CYAN})
    want("the menu bar (cards 2-79, rows 0-1)", cards(2, 79, 0, 1), {CHROME})
    want("the menu bar's line (y 15)", set(row(15, 0, 79)), {0xFF})
    if inked(0, 1, 2, 14) < 20:
        bad.append("no Veranda mark in the bar")
    if inked(3, 10, 2, 14) < 60:
        bad.append("no program name in the bar")
    want("the desk (column 0, rows 2-59)", cards(0, 0, 2, 59), {SKY2})
    want("the desk (column 79, rows 2-59)", cards(79, 79, 2, 59), {SKY2})
    want("the title band (rows 3-4)", cards(WIN_X, r, WIN_Y, WIN_Y + 1), {TITLE})
    want("the title band's line (y 39)", set(row(39, WIN_X, r)), {0xFF})
    want("the status bar (rows 57-58)", cards(WIN_X, r, b - 1, b), {CHROME})
    want("the status bar's line (y 456)", set(row(456, WIN_X, r)), {0xFF})
    want("the panels' heads (rows 6-7)", cards(2, 41, 6, 7) | cards(43, 77, 6, 7), {CHROME})
    want("the body (card 30, row 20)", s.colour(30, 20), 0)
    if inked(r - 1, r, 8 * WIN_Y + 4, 8 * WIN_Y + 12) < 10:
        bad.append("no close glyph at the title band's right")
    left = {s.v[s.base + y * STRIDE + WIN_X] & 0x80 for y in range(8 * WIN_Y, 8 * (b + 1))}
    rgt = {s.v[s.base + y * STRIDE + r] & 0x01 for y in range(8 * WIN_Y, 8 * (b + 1))}
    want("the frame's sides", (left, rgt), ({0x80}, {0x01}))
    sh = inked(r + 1, r + 1, 8 * WIN_Y + 8, 8 * (b + 1))
    if not 300 < sh < 1400:
        bad.append("the shadow right of the window: %d ink pixels" % sh)
    for b_ in bad:
        print("FAIL  " + b_)
    if not bad:
        print("      the look: Look 2's palette, the menu bar (mark, name, chrome), the title band, "
              "the close glyph, the status bar, the frame, the shadow, the desk, the panels' heads")
    return not bad


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    s = Screen(sys.argv[1], sys.argv[2])
    if sys.argv[3] == "show":
        show(s)
    elif sys.argv[3] == "chrome":
        if not check_chrome(s):
            sys.exit(1)
    elif not check_shadow(s):
        show(s)
        sys.exit(1)


if __name__ == "__main__":
    main()
