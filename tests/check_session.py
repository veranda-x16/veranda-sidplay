#!/usr/bin/env python3
"""The gate's sessions checked (tools/gate_veranda.sh): the console's "SIDP"
lines (the log build, src/main.c logs) and the emulated card's log (-sidlog,
tests/sidlog.py).

    check_session.py <kind> <run folder> [files...]

GPL-3.0-or-later (see COPYING).
"""
import hashlib
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sidlog  # noqa: E402

S = 8_000_000.0
bad = []


def fail(msg):
    bad.append(msg)


def lines(run):
    t = open(os.path.join(run, "console.txt"), "rb").read().decode("latin-1")
    t = t.replace("\r", "\n")
    return [m.group(1).rstrip() for m in re.finditer(r"SIDP ([^\n\\]*)", t)]


def want_in_order(got, want, what):
    """each line of want starts some line of got, in this order"""
    i = 0
    for w in want:
        while i < len(got) and not got[i].startswith(w):
            i += 1
        if i == len(got):
            fail("%s: no %r (after the ones before it); the lines: %s" % (what, w, " | ".join(got)))
            return
        i += 1


def plays(reqs):
    """[(seconds, length, song)] of the PLAY requests"""
    return [(c / S, int.from_bytes(p[4:8], "little"), p[8]) for c, cmd, p, _ in reqs if cmd == 0x0B]


def common(run, starts=1):
    """every session with a card: frames with good CRCs, the lease renewed
    while the player runs (from its MB_RESET to its DISARM at the quit)"""
    log = sidlog.Log(os.path.join(run, "w.log"))
    reqs = log.requests()
    if not all(ok for *_, ok in reqs):
        fail("a request with a bad CRC")
    cmds = log.commands()
    gap, last, on = 0, None, False
    for c, v in cmds:
        if v == 0x80:
            on, last = True, c
        elif v == 0x85:
            on = False
        elif v == 0x84 and on:
            gap = max(gap, c - last)
            last = c
    if gap / S > 0.5:
        fail("the ARM lease lapsed for %.2f s (the card disarms after 1 s)" % (gap / S))
    if [v for c, v in cmds].count(0x80) != starts:
        fail("MB_RESET ($80) sent %d times, want %d" % ([v for c, v in cmds].count(0x80), starts))
    if not reqs or reqs[0][1] != 0x02:
        fail("the first request is not INFO")
    return log, reqs


def check_store(reqs, at, data, what):
    img, got = sidlog.store_image(reqs)
    if bytes(img[at:at + len(data)]) != data or not all(at + i in got for i in range(len(data))):
        fail("%s: the store at $%04X does not hold the file's %d bytes" % (what, at, len(data)))


def k_play(run, files):
    """auto-advance by song lengths: DEMO (0:03, 0:02), TEST (0:02, 0:01.5), the end"""
    got = lines(run)
    log, reqs = common(run)
    demo, test = (open(os.path.join(run, f), "rb").read() for f in ("DEMO.SID", "TEST.SID"))
    want_in_order(got, ["CARD X16-SID rev B", "ENTRY DMORE", "ENTRY SDEMO.SID", "ENTRY STEST.SID", "RUNNING"],
                  "the start")
    entries = [g for g in got if g.startswith("ENTRY")]
    if entries[:3] != ["ENTRY DMORE", "ENTRY SDEMO.SID", "ENTRY STEST.SID"] or any("README" in g for g in got):
        fail("the root's list: %s (folders first, .sid files only)" % entries[:4])
    want_in_order(got, ["UPLOADED 470", "SONG 1", "TITLE Veranda Demo", "SONG END", "SONG 2", "SONG END",
                        "UPLOADED 231", "SONG 1", "TITLE X16-SID test tune", "SONG END", "SONG 2", "SONG END",
                        "STOP", "ENTRY U..", "ENTRY SC.SID", "FOLDER /MORE", "FOLDER /"], "the session")
    p = plays(reqs)
    if [(n, s) for _, n, s in p] != [(470, 0), (470, 2), (231, 0), (231, 2)]:
        fail("PLAY requests %s" % [(n, s) for _, n, s in p])
    else:
        gaps = [b[0] - a[0] for a, b in zip(p, p[1:])]
        stop = [c / S for c, cmd, *_ in reqs if cmd == 0x0C]
        gaps.append(stop[-1] - p[-1][0] if stop else 0)
        for g, w in zip(gaps, (3, 2, 2, 2)):
            if not w - 0.1 < g < w + 0.6:
                fail("a song lasted %.2f s, want %d (SONGLEN.IDX)" % (g, w))
        print("      songs lasted %s s (SONGLEN.IDX: 3, 2, 2, 1.5 -> 2)" % ", ".join("%.2f" % g for g in gaps))
    md5 = [p for c, cmd, p, _ in reqs if cmd == 0x14]
    if [(int.from_bytes(m[:4], "little"), int.from_bytes(m[4:], "little")) for m in md5] != [(0, 470), (0, 231)]:
        fail("MD5 requests %s" % md5)
    idx = [i for i, r in enumerate(reqs) if r[1] == 0x0B]
    if len(idx) == 4:
        check_store(reqs[:idx[0]], 0, demo, "DEMO.SID")
        check_store(reqs[idx[1]:idx[2]], 0, test, "TEST.SID")
    if len(log.p) < 100:
        fail("the card's player wrote the SID %d times" % len(log.p))
    print("      %d requests, %d player writes, the list and the order as expected" % (len(reqs), len(log.p)))


def k_tracker(run, files):
    got = lines(run)
    log, reqs = common(run)
    want_in_order(got, ["UPLOADED 470", "SONG 1", "VIEW TRACKER", "PAUSE", "RESUME"], "the session")
    pauses = [p[0] for c, cmd, p, _ in reqs if cmd == 0x15]
    if pauses != [1, 0]:
        fail("PAUSE requests %s, want [1, 0]" % pauses)
    print("      the tracker shown, pause and resume (PAUSE 1, 0)")


def k_nocard(run, files):
    got = lines(run)
    want_in_order(got, ["CARD No X16-SID card found", "RUNNING", "DOUBLE CLICK", "ERROR No X16-SID card found"],
                  "the session")
    if any(g.startswith("UPLOADED") for g in got):
        fail("something was uploaded without a card")
    print("      no card: the box, the message, no hang")


def k_io5(run, files):
    log, reqs = common(run)
    demo = open(os.path.join(run, "DEMO.SID"), "rb").read()
    check_store(reqs, 0, demo, "DEMO.SID")
    if [(n, s) for _, n, s in plays(reqs)] != [(470, 0)]:
        fail("PLAY requests %s" % plays(reqs))
    if len(log.p) < 100:
        fail("the card's player wrote the SID %d times" % len(log.p))
    print("      IO5: INFO, STORE, MD5, PLAY; the tune plays (%d player writes)" % len(log.p))


def k_rsid(run, files):
    got = lines(run)
    log, reqs = common(run)
    roms = open(os.path.join(run, "C64ROMS.BIN"), "rb").read()
    rsid = open(os.path.join(run, "RSIDTEST.SID"), "rb").read()
    want_in_order(got, ["ROMS C64ROMS.BIN", "UPLOADED %d" % len(rsid), "SONG 1", "UPLOADED 470", "SONG 1",
                        "ROMS C64ROMS.BIN", "UPLOADED %d" % len(rsid)], "the session")
    starts = [p for c, cmd, p, _ in reqs if cmd == 0x16]
    want = bytes(4) + (0x5000).to_bytes(4, "little") + len(rsid).to_bytes(4, "little") + bytes(1)
    if starts != [want, want]:
        fail("C64_SID requests %s, want twice %s" % ([s.hex() for s in starts], want.hex()))
    stores = [p for c, cmd, p, _ in reqs if cmd == 0x06]
    n = sum(len(p) - 4 for p in stores)
    if n != 2 * (len(roms) + len(rsid)) + 470:
        fail("%d bytes stored, want the ROMs and the tune twice, the PSID once" % n)
    check_store(reqs, 0x5000, rsid, "RSIDTEST.SID")
    stops = [cmd for c, cmd, p, _ in reqs if cmd in (0x0C, 0x0E)]
    if stops[:1] != [0x0E]:
        fail("an RSID is stopped with C64_STOP ($0E): %s" % stops)
    print("      ROMs once per RSID after a PSID, the tune at $5000, C64_SID (0, $5000, %d, 0)" % len(rsid))


def k_norom(run, files):
    got = lines(run)
    log, reqs = common(run)
    want_in_order(got, ["DOUBLE CLICK", "ERROR RSID tune: needs C64ROMS.BIN"], "the session")
    if any(cmd in (0x06, 0x16) for c, cmd, p, _ in reqs):
        fail("something was stored or started without C64ROMS.BIN")
    print("      an RSID without C64ROMS.BIN: the message, nothing stored")


def k_mouse(run, files):
    got = lines(run)
    log, reqs = common(run, starts=2)
    if [v for c, v in log.commands()].count(0x85) != 1:
        fail("the quit did not let the card go (DISARM $85)")
    want_in_order(got, ["CARD X16-SID", "RUNNING", "FOLDER /", "FOLDER /TUNES", "UPLOADED 470", "SONG 1",
                        "ICON SONG+", "SONG 2", "ICON SONG-", "SONG 1", "ICON PLAY", "PAUSE", "ICON PLAY",
                        "RESUME", "ICON NEXT", "TITLE X16-SID test tune", "ICON PREV", "TITLE Veranda Demo",
                        "ICON STOP", "STOP", "ICON PLAY", "SONG 1", "QUIT",
                        "DOCUMENT Another demo tune with a long name.sid", "UPLOADED 470", "SONG 1", "TITLE Veranda Demo"], "the session")
    p = [(n, s) for _, n, s in plays(reqs)]
    want = [(470, 0), (470, 2), (470, 1), (231, 0), (470, 0), (470, 0), (470, 0)]
    if p != want:
        fail("PLAY requests %s, want %s" % (p, want))
    pauses = [q[0] for c, cmd, q, _ in reqs if cmd == 0x15]
    if pauses != [1, 0]:
        fail("PAUSE requests %s" % pauses)
    print("      the shell, the folders, every transport icon, the close glyph, a long-named .sid opened from the shell")


def k_menus(run, files):
    got = lines(run)
    log, reqs = common(run)
    want_in_order(got, ["RUNNING", "ABOUT", "ABOUT CLOSED", "OPEN BOX", "FOLDER /MORE", "UPLOADED 231", "SONG 1",
                        "TITLE X16-SID test tune", "REPEAT FOLDER", "VIEW TRACKER"], "the session")
    print("      the about box, the file chooser into MORE/, play > repeat, view > tracker")


def main():
    kind, run, *files = sys.argv[1:]
    globals()["k_" + kind](run, files)
    for b in bad:
        print("FAIL  " + b)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
