# Veranda SID Player (README.md)
#   make            the application: build/SID Player.cvt (and the log build, build/log/)
#   make vsdk       Veranda's SDK, kernel and shell, built from a copy of $(VERANDA)
#                   (git archive of its HEAD into build/veranda-src: nothing is built there)
#   make emu        the emulator with the X16-SID card model and -mousescript
#                   ($(EMUSRC), branch sidcard; reSIDfp into build/deps)
#   make gate       tools/gate_veranda.sh
export CC65_HOME ?= $(HOME)/.local/share/cc65
export PATH := $(CC65_HOME)/bin:$(PATH)
VERANDA ?= $(HOME)/Projects/geos
EMUSRC  ?= $(HOME)/Projects/x16-emulator-sidcard
VSDK    ?= build/vsdk
SDKABS  := $(abspath $(VSDK))
CC      := cl65 -t none --cpu 65C02 -O -I $(SDKABS)/include --asm-include-dir $(SDKABS)/include -Wl -D,__STACKSIZE__=0x0600
SRC     := src/main.c src/list.c src/tune.c src/ui.c src/viz.c src/draw.c src/pics.c src/card.s src/viz.s
NAME    := SID Player

all: build/app/app.cvt build/log/app.cvt

# the Open dialog (vrchoose.h, Veranda DECISIONS V79): the SDK's source
# compiled into overlay 2 (banked overlays; its BSS in main RAM): main RAM
# has no room for it (main.c's choose maps it)
build/vrchoose.o: $(SDKABS)/src/vrchoose.c $(SDKABS)/include/vrchoose.h
	@mkdir -p build
	$(CC) --code-name OVERLAY2 --rodata-name OVL2RO -c -o $@ $<

src/pics.c: tools/mkpics.py
	python3 -I -B tools/mkpics.py $@

# the window's chrome (vrwin.h, V79): the SDK's source in overlay 3 (main.c's
# win_* map it)
build/vrwin.o: $(SDKABS)/src/vrwin.c $(SDKABS)/include/vrwin.h
	@mkdir -p build
	$(CC) --code-name OVERLAY3 --rodata-name OVL3RO -c -o $@ $<

build/app/app.bin: $(SRC) src/sidp.h build/vrchoose.o build/vrwin.o $(SDKABS)/lib/veranda.lib
	@mkdir -p build/app
	$(CC) -C $(SDKABS)/cfg/app-c-ovl.cfg -m build/app/app.map -o $@ $(SRC) build/vrchoose.o build/vrwin.o $(SDKABS)/lib/veranda.lib

build/log/app.bin: $(SRC) src/sidp.h build/vrchoose.o build/vrwin.o $(SDKABS)/lib/veranda.lib
	@mkdir -p build/log
	$(CC) -DSIDP_LOG -C $(SDKABS)/cfg/app-c-ovl.cfg -m build/log/app.map -o $@ $(SRC) build/vrchoose.o build/vrwin.o $(SDKABS)/lib/veranda.lib

build/%/app.cvt: build/%/app.bin src/app.hdr src/icon.pbm src/tile.txt
	cd src && python3 -I -B $(SDKABS)/tools/vpack.py --load 0x0801 -o "../build/$*/$(NAME).cvt" app.hdr ../build/$*/app.bin ../build/$*/app.bin.1 ../build/$*/app.bin.2 ../build/$*/app.bin.3
	cp "build/$*/$(NAME).cvt" $@

vsdk:
	rm -rf build/veranda-src && mkdir -p build/veranda-src
	git -C $(VERANDA) archive HEAD | tar -x -C build/veranda-src
	$(MAKE) -C build/veranda-src sdk shell SDKDIR=$(SDKABS)
	cp build/veranda-src/shell/SHELL.cvt $(VSDK)/bin/

emu:
	tools/mkemu.sh

gate: all
	tools/gate_veranda.sh

clean:
	rm -rf build/app build/log

.PHONY: all vsdk emu gate clean
