#!/usr/bin/env python3
"""The Veranda SID Player's pictures -> src/pics.c (GEOS compacted bitmaps).

    tools/mkpics.py src/pics.c

The transport buttons are 32 x 24 (4 cards, as GEOS icons: DoIcons), the
list's marks and the scroll arrows 8 x 8. A compacted bitmap: 1-127 = the
next byte that many times, 128 + n = n bytes as they are (n <= 64 here:
GEOS's 220-255 are BIGCOUNT).

GPL-3.0-or-later (see COPYING).
"""
import sys


def button(sym):
    """a 32 x 24 button: a rounded frame (0-30 x 0-22), its shadow (x 31,
    y 23), sym (rows of '#' and '.') in the middle"""
    rows = [[0] * 32 for _ in range(24)]
    for x in range(2, 29):
        rows[0][x] = rows[22][x] = 1
    for y in range(2, 21):
        rows[y][0] = rows[y][30] = 1
    for x, y in ((1, 1), (29, 1), (1, 21), (29, 21)):
        rows[y][x] = 1
    for y in range(2, 24):
        rows[y][31] = 1
    for x in range(2, 32):
        rows[23][x] = 1
    sy = (23 - len(sym)) // 2
    for i, line in enumerate(sym):
        sx = (31 - len(line)) // 2
        for j, c in enumerate(line):
            if c == '#':
                rows[sy + i][sx + j] = 1
    return rows


def art(lines):
    return [[1 if c == '#' else 0 for c in l] for l in lines]


SYMS = {
    'prev': [
        "#.....##",
        "#....###",
        "#...####",
        "#..#####",
        "#.######",
        "########",
        "#.######",
        "#..#####",
        "#...####",
        "#....###",
        "#.....##",
    ],
    'play': [
        "##......",
        "####....",
        "######..",
        "########",
        "######..",
        "####....",
        "##......",
    ],
    'pause': [
        "###..###",
        "###..###",
        "###..###",
        "###..###",
        "###..###",
        "###..###",
        "###..###",
        "###..###",
        "###..###",
    ],
    'stop': [
        "#########",
        "#########",
        "#########",
        "#########",
        "#########",
        "#########",
        "#########",
        "#########",
        "#########",
    ],
    'next': [
        "##.....#",
        "###....#",
        "####...#",
        "#####..#",
        "######.#",
        "########",
        "######.#",
        "#####..#",
        "####...#",
        "###....#",
        "##.....#",
    ],
    'minus': [
        "...........",
        "...........",
        "...........",
        "...........",
        "###########",
        "###########",
        "...........",
        "...........",
        "...........",
        "...........",
        "...........",
    ],
    'plus': [
        "....###....",
        "....###....",
        "....###....",
        "....###....",
        "###########",
        "###########",
        "###########",
        "....###....",
        "....###....",
        "....###....",
        "....###....",
    ],
}
# (prev and next: two triangles and a bar each, wider)
SYMS['prev'] = [
    "##.....#.....##"[:15],
    "##....##....###",
    "##...###...####",
    "##..####..#####",
    "##.#####.######",
    "###############",
    "##.#####.######",
    "##..####..#####",
    "##...###...####",
    "##....##....###",
    "##.....#.....##",
]
SYMS['next'] = [r[::-1] for r in SYMS['prev']]

SMALL = {
    'up': [         # ".." : an arrow up
        "...#....",
        "..###...",
        ".#####..",
        "...#....",
        "...#....",
        "...####.",
        "........",
        "........",
    ],
    'folder': [
        "###.....",
        "#..####.",
        "#......#",
        "#......#",
        "#......#",
        "########",
        "........",
        "........",
    ],
    'note': [
        "...####.",
        "...#..#.",
        "...#..#.",
        "...#..#.",
        ".###.##.",
        "####.##.",
        ".##.....",
        "........",
    ],
    'playing': [
        "#.......",
        "###.....",
        "#####...",
        "#######.",
        "#####...",
        "###.....",
        "#.......",
        "........",
    ],
    'arrow_up': [
        "........",
        "...#....",
        "..###...",
        ".#####..",
        "#######.",
        "........",
        "........",
        "........",
    ],
    'arrow_down': [
        "........",
        "........",
        "#######.",
        ".#####..",
        "..###...",
        "...#....",
        "........",
        "........",
    ],
}


def bytes_of(rows):
    out = []
    for r in rows:
        for x in range(0, len(r), 8):
            b = 0
            for i in range(8):
                b = b << 1 | (r[x + i] if x + i < len(r) else 0)
            out.append(b)
    return out


def compact(data):
    out, i = [], 0
    while i < len(data):
        n = 1
        while i + n < len(data) and data[i + n] == data[i] and n < 127:
            n += 1
        if n >= 3:
            out += [n, data[i]]
            i += n
            continue
        j = i
        lit = []
        while j < len(data) and len(lit) < 64:
            k = 1
            while j + k < len(data) and data[j + k] == data[j] and k < 3:
                k += 1
            if k >= 3:
                break
            lit.append(data[j])
            j += 1
        out += [128 + len(lit)] + lit
        i = j
    return out


def main():
    o = ["/* generated by tools/mkpics.py: do not edit */", '#include "sidp.h"', ""]
    for name, sym in SYMS.items():
        d = compact(bytes_of(button(sym)))
        o.append("const unsigned char pic_%s[] = { %s };" % (name, ", ".join("0x%02X" % b for b in d)))
    for name, sym in SMALL.items():
        d = compact(bytes_of(art(sym)))
        o.append("const unsigned char pic_%s[] = { %s };" % (name, ", ".join("0x%02X" % b for b in d)))
    open(sys.argv[1], "w").write("\n".join(o) + "\n")


if __name__ == "__main__":
    main()
