#!/bin/sh
# Regenerates mgb_driver.h (NOT run in the build; the generated header is committed). Mirrors bbmp tools/build-retro.sh lines 118-147.
# Usage: regen.sh <dir with rgbds-0.6.1/{rgbasm,rgblink} and GBSoundSystem/ at c0c6a6f3>  (see VERSION.txt for sha256 and commit).
# rgbds 0.6.1 needs bison >= 3.6 (macOS: PATH=/opt/homebrew/opt/bison/bin:$PATH make rgbasm rgblink CFLAGS="-O2 -std=gnu17" Q=).
set -e
SRC=$1; HERE=$(cd "$(dirname "$0")" && pwd)
OBJ=$(mktemp -d); trap 'rm -rf "$OBJ"' EXIT
cp "$HERE/mgb_SoundSystem.def" "$OBJ/SoundSystem.def"
( cd "$OBJ"
  "$SRC/rgbds-0.6.1/rgbasm" -i "$SRC/GBSoundSystem/Driver/" -o driver.o "$SRC/GBSoundSystem/Driver/SoundSystem.asm" > /dev/null
  "$SRC/rgbds-0.6.1/rgbasm" -o entry.o "$HERE/mgb_gbs.asm"
  "$SRC/rgbds-0.6.1/rgblink" -l "$HERE/mgb_gbs.link" -o driver.gb -n driver.sym driver.o entry.o
  python3 -I - "$HERE/mgb_driver.h" <<'PY'
import sys
d = open('driver.gb', 'rb').read()
assert len(d) == 0x4000 and d[0x400] == 0xc3 and d[0x403] == 0xc3
blob = d[0x400:].rstrip(b'\x00')
with open(sys.argv[1], 'w') as f:
    f.write('// generated: the SoundSystem driver and the GBS entry, from $0400\n')
    f.write('static const unsigned char mgb_driver[] = {\n')
    for i in range(0, len(blob), 16):
        f.write(' ' + ''.join(' 0x%02x,' % b for b in blob[i:i + 16]) + '\n')
    f.write('};\n')
PY
)
