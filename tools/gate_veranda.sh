#!/bin/bash
# The Veranda SID Player's gate: the player in Veranda in the emulator with an
# emulated X16-SID card (make gate).
#
#   tools/gate_veranda.sh       EMU: x16emu with the card model and -mousescript
#                               (default: tools/mkemu.sh's build/emu/x16emu, from
#                               ~/Projects/x16-emulator-sidcard); ROM: R49's
#                               release image; VSDK: Veranda's SDK (make vsdk)
#
# The card: the emulator's -sidcard is the CommanderSID project's own card model
# (its emulator patches, with its firmware core: the mailbox, the PSID player,
# the C64 mode, reSIDfp for the SID), so the player talks to the card's real
# firmware code. -sidlog logs every access to the card's window ("W/R cycle reg
# value") and the card's own SID writes ("P"); tests/sidlog.py decodes the
# mailbox's frames from it. The log build of the player (-DSIDP_LOG) writes its
# "SIDP" lines to the console (LogText). The card is at IO6 except in the io5
# session: at IO5 it covers the emulator's own registers ($9FB0-$9FBF), so
# Veranda's LogText is silent there and the release build runs.
#
# Sessions (each a run of the emulator; a mouse script, -mousescript, plays the
# clicks and the keys through the emulated PS/2 mouse and keyboard):
#   play     the host folder: the list (folders first, .sid only), DEMO.SID by a
#            double click; SONGLEN.IDX (made by tools/mksongidx.py) gives its songs
#            3 and 2 s, TEST.SID's 2 and 1.5: auto-advance song by song, tune by
#            tune, stop at the end; MORE/ and back. The bytes stored on the card
#            are the files', MD5 asked once a tune, the ARM lease never lapses.
#   shadow   SHADOW.SID writes a known set of registers once: the voices' notes,
#            waveform bits, envelopes, ADSR, pulse widths, frequencies, the
#            filter and the bars read back from VRAM (tests/check_ui.py); and
#            Veranda Look 2's look (check_ui.py chrome): Look 2's palette, the
#            kernel's menu bar (the mark, the name, chrome cards), the window's
#            chrome (vrwin.h: the title band on the accent with the close
#            glyph, the status bar, the frame, the shadow), the desk's colour.
#   tracker  DEMO.SID, the tracker view (T), pause and resume (Space).
#   cpu      what a pass of the main loop costs while DEMO.SID plays (the log
#            build's statistics from the emulator's cycle counter): under 70 %
#            of the CPU, no pass over 4 frames.
#   menus    SID Player > about, file > open... (Veranda's file chooser into MORE/,
#            TEST.SID, Open), play > repeat, view > tracker; the lease holds
#            under the dialog boxes. about: the about box (a picture).
#   nocard   no card: the box "No X16-SID card found", then a tune: the message,
#            nothing hangs.
#   io5      the card at IO5, the release build.
#   rsid     RSIDTEST.SID on the card's C64: C64ROMS.BIN first (a stand-in made
#            here, 20 KB of a pattern: real ROMs are never copied), the tune at
#            $5000, C64_SID; a PSID over them; the RSID again uploads them again.
#   norom    an RSID without C64ROMS.BIN: the message, nothing stored.
#   mouse    an SD card image (/VERANDA: the system, the shell; /VERANDA/APPS:
#            the player; /TUNES), R49: BASIC starts Veranda, the shell starts the player by
#            its icon, the playlist goes to /TUNES, DEMO.SID; then every transport
#            icon (song +, song -, pause, play, next, previous, stop, play), the
#            close glyph; then the shell opens the tune itself (documents = SID):
#            its name is 38 characters long, so the player finds it by
#            dataLongName (dataFileName holds its 16-character short form, V82).
#   hvsc     (when ~/Projects/CommanderSID/sidfiles/HVSC is there) a folder of Rob
#            Hubbard's tunes with the real SONGLEN.IDX: a picture.
# Each dump's display registers are checked (Veranda's tools/check_vera.py); the
# pictures are build/gate/*.png.
set -e -o pipefail
cd "$(dirname "$0")/.."
export CC65_HOME=${CC65_HOME:-$HOME/.local/share/cc65}
export PATH=$CC65_HOME/bin:$PATH
ROM=${ROM:-$HOME/Downloads/Release.R49.ROM.Image/rom.bin}
VSDK=${VSDK:-build/vsdk}
VERANDA=${VERANDA:-$HOME/Projects/geos}
HVSC=${HVSC:-$HOME/Projects/CommanderSID/sidfiles}
OUT=build/gate
PY="python3 -I -B"
ok() { echo "ok    $*"; }
fail() { echo "FAIL  $*"; echo "GATE veranda-sidplay: FAIL"; exit 1; }
[ -f "$ROM" ] || fail "no ROM ($ROM)"
[ -f "$VSDK/bin/VERANDA.PRG" ] && [ -f "$VSDK/bin/SHELL.cvt" ] || fail "no Veranda SDK in $VSDK (make vsdk)"
EMU=${EMU:-$(tools/mkemu.sh)}
{ "$EMU" -help 2>&1 || true; } | grep -q -- -mousescript || fail "$EMU has no -mousescript"
{ "$EMU" -help 2>&1 || true; } | grep -q -- -sidcard || fail "$EMU has no -sidcard"
EMU=$(cd "$(dirname "$EMU")" && pwd)/$(basename "$EMU")
ROM=$(cd "$(dirname "$ROM")" && pwd)/$(basename "$ROM")
rm -rf "$OUT"
mkdir -p "$OUT"
echo "      ROM: $ROM"
echo "      emulator: $EMU"

# the player (release and log builds), the test tunes
make -s all >"$OUT/build.log" 2>&1 || { cat "$OUT/build.log"; fail "the player's build"; }
grep -q "^OVERLAY1" build/app/app.map && grep -q "^OVERLAY2" build/app/app.map && grep -q "^OVERLAY3" build/app/app.map || fail "no overlay"
ok "the player builds: $(grep -E '^(CODE|RODATA|DATA|BSS|OVERLAY1|OVERLAY2|OVERLAY3) ' build/app/app.map | awk '{printf "%s $%s-$%s  ", $1, substr($2,3), substr($3,3)}')"
T=$OUT/tunes
mkdir -p "$T"
for t in testtune rsidtune shadow demo; do
	ca65 -o "$T/$t.o" tests/tunes/$t.s && ld65 -C tests/tunes/psid.cfg -o "$T/$t.sid" "$T/$t.o" || fail "tune $t"
done
ok "the test tunes assemble"

# run <name> <io5|io6|none> <log|app> [sd]: $OUT/<name>/ (its files there already),
# $OUT/<name>.steps the steps (tools/msc.py); host folder: the player is AUTOSTART.cvt
run() {
	local n=$1 card=$2 b=$3 sd=$4 d=$OUT/$1 sc=() boot=()
	[ "$card" != none ] && sc=(-sidcard "$card" -sidrev b -sidlog w.log)
	if [ -n "$sd" ]; then
		$PY tools/msc.py --boot < "$OUT/$n.steps" > "$d/mouse.txt"
		boot=(-ram 2048 -sdcard sd.img)
	else
		cp "$VSDK/bin/VERANDA.PRG" "$VSDK/bin/VERANDA.SYS" "$d/"
		cp "build/$b/SID Player.cvt" "$d/AUTOSTART.cvt"
		$PY tools/msc.py < "$OUT/$n.steps" > "$d/mouse.txt"
		boot=(-prg VERANDA.PRG -run)
	fi
	(cd "$d" && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout -s INT 600 "$EMU" -rom "$ROM" "${boot[@]}" \
		-warp -sound none -echo -dump V -mousescript mouse.txt "${sc[@]}" > console.txt 2>&1) || true
	grep -a -q "the mouse script's exit" "$d/console.txt" && [ -f "$d/dump.bin" ] \
		|| { tail -c 800 "$d/console.txt"; fail "$n: the run did not reach its end"; }
	$PY "$VERANDA/tools/check_vera.py" "$d/dump.bin" >/dev/null || fail "$n: VERA's display registers"
	$PY "$VSDK/tools/vram2png.py" --color "$d/dump.bin" "$OUT/$n.png" >/dev/null
}
# the playlist's rows (src/sidp.h): y 64 + 16 r, x 60; the transport icons at
# y 176-199, x (46 + 5 i) * 8 + 16; the close glyph at the title band's right
# (vrwin.h) at 621, 32. The menu bar (Veranda Look 2, DECISIONS V74, V79: the
# kernel's geometry in the UI font, Veranda's tests/look_menu.py): the items'
# middles SID Player 48, File 112, Play 148, View 188; an open menu's items 16
# lines apart (a sub menu puts the pointer on its first item)
ROW0="60 72"; ROW1="60 88"; ROW2="60 104"
PREV="384 188"; PLAY="424 188"; STOP="464 188"; NEXT="504 188"; SMINUS="544 188"; SPLUS="584 188"
CLOSE="621 32"; OKBOX="320 272"

# ---- play: auto-advance by SONGLEN.IDX
d=$OUT/play
mkdir -p "$d/MORE"
cp "$T/demo.sid" "$d/DEMO.SID"
cp "$T/testtune.sid" "$d/TEST.SID"
cp "$T/demo.sid" "$d/MORE/C.SID"
echo hello > "$d/README.TXT"
$PY - "$d" <<'EOF'
import hashlib, sys
sys.path.insert(0, "tools")
import mksongidx
d = sys.argv[1]
md5 = lambda f: hashlib.md5(open(d + "/" + f, "rb").read()).hexdigest()
txt = "[Database]\n; /DEMO.SID\n%s=0:03 0:02\n; /TEST.SID\n%s=0:02 0:01.5\n" % (md5("DEMO.SID"), md5("TEST.SID"))
open(d + "/SONGLEN.IDX", "wb").write(mksongidx.build(mksongidx.parse(txt.splitlines())))
EOF
printf 'wait 300\ndbl %s\nwait 760\ndbl %s\nwait 120\ndbl %s\nwait 120\n' "$ROW1" "$ROW0" "$ROW0" > "$OUT/play.steps"
run play io6 log
$PY tests/check_session.py play "$d" || fail "play"
ok "play: auto-advance by SONGLEN.IDX, the stored bytes, the lease"

# ---- shadow: the UI from a known register shadow
d=$OUT/shadow
mkdir -p "$d"
cp "$T/demo.sid" "$d/DEMO.SID"
cp "$T/shadow.sid" "$d/SHADOW.SID"
printf 'wait 300\ndbl %s\nwait 240\n' "$ROW1" > "$OUT/shadow.steps"
run shadow io6 log
$PY tests/check_ui.py "$d/dump.bin" "$ROM" shadow || fail "shadow: the voices' panel"
ok "shadow: notes, waveforms, envelopes, ADSR, pulse widths, frequencies, filter, bars, scopes"
$PY tests/check_ui.py "$d/dump.bin" "$ROM" chrome || fail "shadow: Veranda Look 2's bar and window"
ok "the look: Look 2's palette, the menu bar, the window's chrome (title band, close glyph, status bar, frame, shadow), the desk"

# ---- tracker, pause
d=$OUT/tracker
mkdir -p "$d"
cp "$T/demo.sid" "$d/DEMO.SID"
printf 'wait 300\ndbl %s\nwait 200\nkey t\nwait 120\nkey \\X20\nwait 60\nkey \\X20\nwait 240\n' "$ROW0" > "$OUT/tracker.steps"
run tracker io6 log
$PY tests/check_session.py tracker "$d" || fail "tracker"
ok "tracker: the view, pause and resume"

# ---- cpu: what a pass of the main loop costs while a tune plays (the log
# build's pass statistics at the quit; a frame is 133,333 cycles at 8 MHz; a pass
# runs at most every second frame)
d=$OUT/cpu
mkdir -p "$d"
cp "$T/demo.sid" "$d/DEMO.SID"
cp "$VSDK/bin/SHELL.cvt" "$d/"           # (the quit goes to the shell)
printf 'wait 300\ndbl %s\nwait 600\nkey t\nwait 600\nkey q\nwait 120\n' "$ROW0" > "$OUT/cpu.steps"
run cpu io6 log
$PY - "$d/console.txt" <<'EOF2' || fail "cpu"
import re, sys
t = open(sys.argv[1], "rb").read().decode("latin-1")
v = {k: int(n) for k, n in re.findall(r"SIDP (PASSES|PASS CYCLES AVERAGE|PASS CYCLES MAX|FRAMES) (\d+)", t)}
if len(v) != 4:
    sys.exit("FAIL  no pass statistics")
share = v["PASSES"] * v["PASS CYCLES AVERAGE"] / (v["FRAMES"] * 133333.0)
print("      %d passes in %d frames; a pass %d cycles on average, at most %d (%.1f frames): %.0f %% of the CPU"
      % (v["PASSES"], v["FRAMES"], v["PASS CYCLES AVERAGE"], v["PASS CYCLES MAX"],
         v["PASS CYCLES MAX"] / 133333.0, 100 * share))
if share > 0.7 or v["PASS CYCLES MAX"] > 4 * 133333:
    sys.exit("FAIL  the passes take too long")
EOF2
ok "cpu: the main loop's passes while a tune plays"

# ---- menus: SID Player > about (its box), file > open... (Veranda's Open dialog,
# vrchoose.h, by its keys: M and Return into MORE, T and Return opens TEST.SID), play > repeat, view > tracker. (A sub menu puts
# the pointer on its first item: "rel" moves from there.)
d=$OUT/menus
mkdir -p "$d/MORE"
cp "$T/demo.sid" "$d/DEMO.SID"
cp "$T/testtune.sid" "$d/MORE/TEST.SID"
cat > "$OUT/menus.steps" <<STEPS
wait 300
click 48 7
wait 30
rel 0 1
wait 60
click $OKBOX
wait 30
click 112 7
wait 30
rel 0 1
wait 100
key m
wait 30
key \\n
wait 100
key t
wait 30
key \\n
wait 300
click 148 7
wait 30
rel 0 112
wait 60
click 188 7
wait 30
rel 0 16
wait 120
STEPS
run menus io6 log
$PY tests/check_session.py menus "$d" || fail "menus"
mkdir -p "$OUT/about"
printf 'wait 300\nclick 48 7\nwait 30\nrel 0 1\nwait 60\n' > "$OUT/about.steps"
run about io6 log
ok "menus: about, open... (the file chooser), repeat, the tracker"

# ---- nocard
d=$OUT/nocard
mkdir -p "$d"
cp "$T/demo.sid" "$d/DEMO.SID"
printf 'wait 400\n' > "$OUT/nocard-box.steps"
mkdir -p "$OUT/nocard-box" && cp "$T/demo.sid" "$OUT/nocard-box/DEMO.SID"
run nocard-box none log
printf 'wait 400\nclick %s\nwait 60\ndbl %s\nwait 120\n' "$OKBOX" "$ROW0" > "$OUT/nocard.steps"
run nocard none log
$PY tests/check_session.py nocard "$d" || fail "nocard"
ok "nocard: \"No X16-SID card found\", no hang"

# ---- io5, the release build
d=$OUT/io5
mkdir -p "$d"
cp "$T/demo.sid" "$d/DEMO.SID"
printf 'wait 300\ndbl %s\nwait 240\n' "$ROW0" > "$OUT/io5.steps"
run io5 io5 app
$PY tests/check_session.py io5 "$d" || fail "io5"
ok "io5: the card at IO5 (the release build)"

# ---- rsid: the card's C64 (a stand-in for C64ROMS.BIN: no ROM is copied)
d=$OUT/rsid
mkdir -p "$d"
cp "$T/rsidtune.sid" "$d/RSIDTEST.SID"
cp "$T/demo.sid" "$d/DEMO.SID"
$PY -c "import sys; open(sys.argv[1], 'wb').write(bytes((i * 7 + (i >> 8)) & 255 for i in range(20480)))" "$d/C64ROMS.BIN"
printf 'wait 300\ndbl %s\nwait 240\ndbl %s\nwait 120\ndbl %s\nwait 240\n' "$ROW1" "$ROW0" "$ROW1" > "$OUT/rsid.steps"
run rsid io6 log
$PY tests/check_session.py rsid "$d" || fail "rsid"
d=$OUT/norom
mkdir -p "$d"
cp "$T/rsidtune.sid" "$d/RSIDTEST.SID"
printf 'wait 300\ndbl %s\nwait 120\n' "$ROW0" > "$OUT/norom.steps"
run norom io6 log
$PY tests/check_session.py norom "$d" || fail "norom"
ok "rsid: C64ROMS.BIN then the tune at \$5000, C64_SID; without it: the message"

# ---- mouse: an SD card image, BASIC, the shell, the transport icons, a document
d=$OUT/mouse
mkdir -p "$d/card/VERANDA/APPS" "$d/card/TUNES"
cp "$VSDK/bin/VERANDA.PRG" "$VSDK/bin/VERANDA.SYS" "$VSDK/bin/SHELL.cvt" "$d/card/VERANDA/"
cp "build/log/SID Player.cvt" "$d/card/VERANDA/APPS/"
cp "$T/demo.sid" "$d/card/TUNES/Another demo tune with a long name.sid"     # (38 characters: V82)
cp "$T/testtune.sid" "$d/card/TUNES/TEST.SID"
$PY "$VSDK/tools/vcard.py" make --mb 64 "$d/sd.img" "$d/card" >/dev/null || fail "vcard make"
# the card's layout (Veranda DECISIONS V71): the player in /VERANDA/APPS; the
# shell (Look 2, V77) starts in /DOCUMENTS (made), window 0: the sidebar's
# places at x 56 (Apps y 112, the card y 184), the icons' tiles 72 apart from
# x 152, y 96 (in APPS: "..", the player). The player starts in /DOCUMENTS
# (the shell's curDirName): "..", then the top folder's DOCUMENTS, TUNES,
# VERANDA; back in the shell: the card's place, TUNES, the tune (after "..")
cat > "$OUT/mouse.steps" <<EOF
wait 1500
click 56 112
wait 200
dbl 224 96
wait 400
dbl $ROW0
wait 120
dbl $ROW1
wait 120
dbl $ROW1
wait 240
click $SPLUS
wait 120
click $SMINUS
wait 120
click $PLAY
wait 60
click $PLAY
wait 60
click $NEXT
wait 120
click $PREV
wait 120
click $STOP
wait 60
click $PLAY
wait 120
click $CLOSE
wait 600
click 56 184
wait 200
dbl 224 96
wait 200
dbl 224 96
wait 600
EOF
run mouse io6 log sd
$PY tests/check_session.py mouse "$d" || fail "mouse"
ok "mouse: R49, an SD card image, the shell, the transport icons, a .sid of 38 characters opened from the shell (dataLongName)"

# ---- hvsc: a picture with real tunes (not committed: the user's own HVSC copy)
H=$HVSC/HVSC/MUSICIANS/H/Hubbard_Rob
if [ -d "$H" ] && [ -f "$HVSC/SONGLEN.IDX" ]; then
	d=$OUT/hvsc
	mkdir -p "$d/Hubbard"
	for t in Action_Biker Battle_of_Britain Chimera Commando Crazy_Comets Delta Flash_Gordon \
		International_Karate Lightforce Monty_on_the_Run Sanxion Thing_on_a_Spring Zoids; do
		[ -f "$H/$t.sid" ] && cp "$H/$t.sid" "$d/Hubbard/"
	done
	cp "$HVSC/SONGLEN.IDX" "$d/"
	printf 'wait 300\ndbl %s\nwait 100\ndbl 60 152\nwait 900\n' "$ROW0" > "$OUT/hvsc.steps"
	run hvsc io6 log
	grep -a -q "SIDP TITLE Crazy Comets" "$d/console.txt" || fail "hvsc: Crazy Comets did not play"
	grep -a -q "Song lengths\|SONG 1" "$d/console.txt" || true
	ok "hvsc: Rob Hubbard's Crazy Comets with HVSC's song length (a picture)"
else
	echo "skip  hvsc: no $H"
fi

echo "      pictures: $(cd "$OUT" && ls *.png | tr '\n' ' ')"
echo "GATE veranda-sidplay: PASS"
