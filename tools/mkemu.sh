#!/bin/bash
# The emulator for the gates: x16emu with the X16-SID card model (CommanderSID's
# emulator patches and firmware core) and -mousescript, from the sidcard branch
# (EMUSRC, default ~/Projects/x16-emulator-sidcard, SIDCARD.md there), built out
# of its tree into build/emu; reSIDfp 1.2.3 into build/deps.
#
#   tools/mkemu.sh          prints the emulator's path
set -e -o pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
EMUSRC=${EMUSRC:-$HOME/Projects/x16-emulator-sidcard}
VER=1.2.3
if [ ! -f build/deps/lib/libresidfp.a ]; then
	mkdir -p build/dl
	TGZ=build/dl/libresidfp-$VER.tar.gz
	[ -f "$TGZ" ] || cp "$HOME/Projects/CommanderSID/build/dl/libresidfp-$VER.tar.gz" "$TGZ" 2>/dev/null \
		|| curl -sSL -o "$TGZ" "https://github.com/libsidplayfp/libresidfp/releases/download/v$VER/libresidfp-$VER.tar.gz"
	rm -rf build/dl/libresidfp-$VER
	tar xzf "$TGZ" -C build/dl
	(cd build/dl/libresidfp-$VER && ./configure --prefix="$ROOT/build/deps" --disable-shared --enable-static >/dev/null \
		&& make -j"$(nproc)" >/dev/null && make install >/dev/null)
fi
export PKG_CONFIG_PATH="$ROOT/build/deps/lib/pkgconfig"
cmake -S "$EMUSRC" -B build/emu -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build/emu -j"$(nproc)" >/dev/null
echo "$ROOT/build/emu/x16emu"
