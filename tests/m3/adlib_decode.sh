#!/bin/sh
# I710as: bbmp's Ad Lib helper formats (Pixel Painters .fmf/.ftf, SIG2 .sig, Lemmings .lem, First Samurai .mus, Shadowlands .cal, Vibrants .vib, Sagitta FM Tracker .fmt, EdLib Packed .edl)
# play through `selecta-cli render` as the RENDER-kind replayer bbmp_adlib (GPL-3.0-or-later bbmp csrc/adlib on AdPlug's Nuked OPL3).
# Sentinel: the `replayer=bbmp_adlib` line of render. Negatives first (they need no song): for every extension, a file that is not laid out like the format
# (zeros, a text pattern, a short file) must not be claimed. Then the positives from tests/m3/fixtures/bbmp_adlib.manifest, one song per line:
#   <url> <sha256> <local name> <minimum seconds>
# fetched by checksum into the git-ignored cache, never committed (tests/m3/fixtures/bbmp_adlib.SOURCE). Per song it asserts the plugin ran, the WAV is not
# silent (ADLIB_ENERGY_MIN raises the floor for red proofs) and the frames match the song length (ADLIB_MIN_SECONDS replaces the floor, for red proofs).
# The manifest lists one Modland song per claimed extension (see the SOURCE file); the minimum seconds are measured renders. Exit 77 = skipped (download failed).
# usage: adlib_decode.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
# The module loads through the user path and registers as a replayer (kind=1), even with no song to play.
"$cli" plugins 2>/dev/null | grep -q '^bbmp_adlib kind=1 ' || fail "bbmp_adlib module not loaded (SELECTA_PLUGIN_PATH)"
for ext in fmf ftf sig lem mus cal vib fmt edl; do
  head -c 8192 /dev/zero > "neg_zero.$ext"
  head -c 8192 /dev/zero | tr '\0' 'A' > "neg_text.$ext"
  head -c 5 /dev/zero > "neg_short.$ext"
  for f in "neg_zero.$ext" "neg_text.$ext" "neg_short.$ext"; do
    rm -f fd_adlib.wav
    out=$("$cli" render "$f" --output fd_adlib.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
    [ "$(field "$out" replayer)" != "bbmp_adlib" ] || fail "bbmp_adlib claimed $f"
    rm -f "$f"
  done
done
rm -f fd_adlib.wav
# Davey Taylor'"'"'s FM Tracker (also .fmt, libopenmpt Load_fmt, magic "FMTracker\001\001") is a different format: bbmp_adlib must leave it alone.
{ printf 'FMTracker\001\001'; head -c 8192 /dev/zero; } > neg_openmpt_fmtracker.fmt
rm -f fd_adlib.wav
out=$("$cli" render neg_openmpt_fmtracker.fmt --output fd_adlib.wav --seconds 1 --rate "$rate" 2>/dev/null) || true
[ "$(field "$out" replayer)" != "bbmp_adlib" ] || fail "bbmp_adlib claimed a libopenmpt-style FMTracker .fmt"
rm -f neg_openmpt_fmtracker.fmt fd_adlib.wav
# A damaged EdLib Packed file (bbmp itself marks 87 of 179 Modland .edl files damaged) must fail the probe cleanly, not crash or hang: gooath.edl cut off
# inside the implode stream, and with its first block overwritten, are each refused (render exits non-zero, no bbmp_adlib claim, no signal).
manifest="$here/fixtures/bbmp_adlib.manifest"
edl_good=$(awk '$3 ~ /\.edl$/ { print $1, $2, $3; exit }' "$manifest" 2>/dev/null)
if [ -n "$edl_good" ]; then
  set -- $edl_good
  edl_song=$("$here/fetch_fixture.sh" "$1" "$2" "$3") || exit $?
  head -c 1200 "$edl_song" > neg_cut.edl
  { head -c 600 "$edl_song"; head -c 600 /dev/zero; tail -c +1201 "$edl_song"; } > neg_zeroed.edl
  for f in neg_cut.edl neg_zeroed.edl; do
    rm -f fd_adlib.wav
    out=$("$cli" render "$f" --output fd_adlib.wav --seconds 1 --rate "$rate" 2>/dev/null); rc=$?
    [ "$rc" -lt 128 ] || fail "$f: render died with signal ($rc)"
    [ "$(field "$out" replayer)" != "bbmp_adlib" ] || fail "$f: bbmp_adlib claimed a damaged .edl"
  done
  rm -f neg_cut.edl neg_zeroed.edl fd_adlib.wav
fi
echo "negatives ok"
songs=0
if [ -f "$manifest" ]; then
  while read -r url sum name min; do
    case "$url" in ''|'#'*) continue ;; esac
    song=$("$here/fetch_fixture.sh" "$url" "$sum" "$name") || exit $?
    rm -f fd_adlib.wav
    out=$("$cli" render "$song" --output fd_adlib.wav --seconds 600 --rate "$rate" 2>/dev/null) || fail "render failed: $name"
    [ "$(field "$out" replayer)" = "bbmp_adlib" ] || fail "$name: replayer=$(field "$out" replayer), want bbmp_adlib"
    got=$(field "$out" rendered_frames)
    want=${ADLIB_MIN_SECONDS:-$min}
    [ "$got" -ge $((rate * want)) ] || fail "$name: rendered_frames=$got, want at least $((rate * want))"
    [ "$got" -le $((rate * (want + 120))) ] || fail "$name: rendered_frames=$got, far beyond the song length"
    e=$(energy fd_adlib.wav)
    [ "$e" -gt "${ADLIB_ENERGY_MIN:-1000000}" ] || fail "$name: silent output (energy $e)"
    echo "$name frames=$got energy=$e"
    rm -f fd_adlib.wav
    songs=$((songs + 1))
  done < "$manifest"
fi
if [ "$songs" -eq 0 ]; then echo "SKIP: no Ad Lib fixture song listed"; exit 77; fi
echo adlib_decode_ok
