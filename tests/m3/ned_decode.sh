#!/bin/sh
# I1223c: NerdTracker II songs (.ned) play through `selecta-cli render` as a RENDER-kind replayer (bbmp_ned module, GPL-3.0-or-later bbmp
# csrc/retro ned engine over Game Music Emu). The replay routine is two I710ga system files (nt2replay.bin, nt2replay.lbl), giftware for
# non-commercial use: never in the repo or the pack. The mirror does not hold them yet, so this test builds them the way third_party/bbmp_ned/
# VERSION.txt says (nedplay.zip fetched by sha256, ca65/ld65 from cc65 2.19) and checks the result against the sha256 the plugin declares.
# Two songs are fetched by checksum, never committed (tests/m3/fixtures/bbmp_ned.SOURCE). Exit 77 = skipped (no network or no cc65).
# Asserts: the plugin ran (replayer=bbmp_ned sentinel), 2 s of non-silent PCM (NED_ENERGY_MIN raises the floor, for red proofs), no system
# files -> system_file_missing, a truncated song fails cleanly and is not played.
# usage: ned_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
M="https://modland.com/pub/modules/NerdTracker%202/Memblers"
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }
rate=44100
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
command -v ca65 >/dev/null 2>&1 && command -v ld65 >/dev/null 2>&1 || { echo "skip: cc65 (ca65, ld65) not installed"; exit 77; }
zip=$(fetch "https://www.nesdev.org/nt2/download/nedplay.zip" 84602a2164280c3a10b080c21fd43f3cac7b6e41e6a53e94faf859c596b4e76f nedplay.zip) || exit $?
rm -rf nt2b nedcfg && mkdir nt2b nedcfg
(cd nt2b && unzip -o -q "$zip" nt2-22.asm DPCMCODE.INC) || fail "cannot unpack nedplay.zip"
: > nt2b/TEMP.DTT
: > nt2b/temp.dmc
i=1
while [ $i -le 17 ]; do echo "ned$i = \$FFFF"; i=$((i + 1)); done > nt2b/songs.asm
cat > nt2b/blob.cfg <<'CFG'
MEMORY {
    ZP:  start = $00,   size = $100,  type = rw;
    RAM: start = $200,  size = $600,  type = rw;
    HDR: start = $7F80, size = $80,   type = ro, file = "";
    PRG: start = $8000, size = $4000, type = ro, file = %O;
    SMP: start = $C080, size = $3F80, type = ro, file = "";
}
SEGMENTS {
    NSFHEADER: load = HDR, type = ro;
    RODATA:    load = PRG, type = ro;
    CODE:      load = PRG, type = ro;
    SAMPLES:   load = SMP, type = ro;
    DATA:      load = RAM, type = bss;
    ZEROPAGE:  load = ZP,  type = zp;
    BSS:       load = RAM, type = bss;
}
CFG
(cd nt2b && ca65 -g nt2-22.asm -o nt2-22.o && ld65 -C blob.cfg -Ln nt2replay.lbl -o nt2replay.bin nt2-22.o) >/dev/null 2>&1 || fail "cannot assemble the replay routine"
[ "$(sha nt2b/nt2replay.bin)" = 8ac3e74346e8374bf9ad522bdae0a21cf1f248c254b563acf4ea363b63be0d21 ] || { echo "skip: assembler output differs from the declared nt2replay.bin (cc65 version?)"; exit 77; }
[ "$(sha nt2b/nt2replay.lbl)" = daca7169d17c5390a01eeff155011dc68c6fa49692f5904b4b86f5c250b83fe7 ] || { echo "skip: assembler output differs from the declared nt2replay.lbl (cc65 version?)"; exit 77; }
cp nt2b/nt2replay.bin nt2b/nt2replay.lbl nedcfg/
cfg=$PWD/nedcfg
check() { # <song path>
  rm -f fd_ned.wav
  out=$("$cli" render "$1" --output fd_ned.wav --seconds 2 --rate "$rate" --config-dir "$cfg" 2>/dev/null) || fail "render failed: $1"
  [ "$(field "$out" replayer)" = "bbmp_ned" ] || fail "$1: replayer=$(field "$out" replayer), want bbmp_ned"
  got=$(field "$out" rendered_frames)
  [ "$got" -ge $((rate * 2)) ] || fail "$1: rendered_frames=$got, want at least $((rate * 2))"
  e=$(energy fd_ned.wav)
  [ "$e" -gt "${NED_ENERGY_MIN:-1000000}" ] || fail "$1: silent output (energy $e)"
  echo "$1 frames=$got energy=$e"
  rm -f fd_ned.wav
}
s1=$(fetch "$M/gradius.ned" 203403d84c5bd32a6a54ee87f15dcb0fe2ba51b9e539c8a626a1e3e73a003ecb gradius.ned) || exit $?
s2=$(fetch "$M/butl1.ned" eaa1919a2311fe23a975e086e5961d4d147ad9d67c4241ed1d8395c0c1be0edd butl1.ned) || exit $?
check "$s1"
check "$s2"
# Without the replay routine the song is not played: the host reports system_file_missing.
mkdir -p nedempty
out=$("$cli" render "$s1" --output fd_ned.wav --seconds 1 --rate "$rate" --config-dir "$PWD/nedempty" 2>&1) || true
case "$out" in *system_file_missing*) ;; *) fail "no system_file_missing without the replay routine: $out" ;; esac
[ "$(field "$out" replayer)" != "bbmp_ned" ] || fail "bbmp_ned played a song with no replay routine"
# A truncated song fails cleanly: no crash, no playback.
head -c 1500 "$s1" > cut.ned
out=$("$cli" render cut.ned --output fd_ned.wav --seconds 1 --rate "$rate" --config-dir "$cfg" 2>/dev/null)
rc=$?
[ $rc -lt 128 ] || fail "truncated song crashed the renderer (exit $rc)"
[ "$(field "$out" replayer)" != "bbmp_ned" ] || [ $rc -ne 0 ] || fail "bbmp_ned rendered a truncated song"
# A file that is not a NerdTracker song is not claimed, whatever its name says.
head -c 5000 /dev/zero | tr '\0' 'A' > notned.ned
out=$("$cli" render notned.ned --output fd_ned.wav --seconds 1 --rate "$rate" --config-dir "$cfg" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_ned" ] || fail "bbmp_ned claimed a file that is not a NerdTracker song"
rm -rf cut.ned notned.ned fd_ned.wav nt2b nedcfg nedempty
echo ned_decode_ok
