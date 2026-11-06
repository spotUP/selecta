#!/bin/sh
# Regenerates vt_player.h (I710ai). Needs dasm 2.20.14.1 (github.com/dasm-assembler/dasm, commit 09270863f5f70e5200c30d2778f654ff7a40adba, GPL 2; a
# build tool only, nothing of it is shipped) and the player sources musiced/player.asm + playerdata.asm of VIC-TRACKER 2.0 (Daniel Kahlin, BSD,
# http://www.kahlin.net/daniel/victracker/vt-2.0/victracker-2.0.tar.gz sha256 d06c726fbde199cf00d66d1945317446cbf3f43c9e5e928ad5bd501d7c75da85).
# Same steps as bbmp tools/build-retro.sh. usage: gen_vt_player.sh <dasm> <dir with player.asm and playerdata.asm> > vt_player.h
set -e
dasm=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
cd "$2"
cat > vt_player.asm <<'ASM'
	processor 6502
VERSION_MAJOR	EQU	2
VERSION_MINOR	EQU	0
	include	"playerdata.asm"
	seg	code
	org	$6000
	include	"player.asm"
ASM
"$dasm" vt_player.asm -f3 -ovt_player.bin -svt_player.sym > dasm.out || { cat dasm.out >&2; exit 1; }
python3 -I - <<'PY'
blob = open('vt_player.bin', 'rb').read()
syms = {}
for line in open('vt_player.sym'):
    parts = line.split()
    if len(parts) >= 2 and parts[0].startswith('pl_'):
        syms[parts[0]] = int(parts[1], 16)
print('// generated from VIC-TRACKER 2.0 musiced/player.asm (BSD licence)')
print('#define VT_PLAYER_ORG 0x6000')
for name in ('pl_Init', 'pl_Play', 'pl_ThisSong', 'pl_Step', 'pl_PatternPos', 'pl_ThisEndStep'):
    print('#define VT_%s 0x%04x' % (name, syms[name]))
print('static const unsigned char vt_player[] = {')
for i in range(0, len(blob), 16):
    print(' ' + ''.join(' 0x%02x,' % b for b in blob[i:i + 16]))
print('};')
PY
