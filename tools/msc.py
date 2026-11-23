#!/usr/bin/env python3
"""Steps -> an x16emu -mousescript (the emulator's src/mousescript.c).

    tools/msc.py [--boot basic] < steps > mouse.txt

Steps, a line each ('#' a comment):
    click x y | dbl x y     a click, a double click at x, y (the pointer pushed
                            to 0, 0 first, then moved there)
    rel dx dy               a click after a relative move (in an open menu)
    key text                keys ("type": \\X1D right, \\X1B Esc, \\n Return ...)
    wait n                  n frames
    waitmem addr val [mask] until the byte at addr (hex) AND mask is val
--boot basic: start Veranda from BASIC first (an SD card image: CD, LOAD, RUN).
GPL-3.0-or-later (see COPYING).
"""
import sys

out = ["0 id 3"]
f = 30
if "--boot" in sys.argv:
    out.append('150 type DOS"CD:VERANDA"\\nLOAD"VERANDA.PRG"\\nRUN\\n')
    f = 900
for line in sys.stdin:
    t = line.split()
    if not t or t[0].startswith("#"):
        continue
    if t[0] in ("click", "dbl"):
        out += ["%d move -2000 -2000" % f, "%d move %s %s" % (f + 5, t[1], t[2])]
        f += 20
        out += ["%d down" % f, "%d up" % (f + 9)]
        f += 15
        if t[0] == "dbl":
            out += ["%d down" % f, "%d up" % (f + 6)]
            f += 10
    elif t[0] == "rel":
        out += ["%d move %s %s" % (f, t[1], t[2])]
        f += 20
        out += ["%d down" % f, "%d up" % (f + 9)]
        f += 15
    elif t[0] == "key":
        out.append("%d type %s" % (f, line.split(None, 1)[1].rstrip("\n")))
        f += 10
    elif t[0] == "wait":
        f += int(t[1])
    elif t[0] == "waitmem":
        out.append("wait " + " ".join(t[1:]))
        f = 1
out.append("%d exit" % f)
print("\n".join(out))
