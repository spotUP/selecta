#!/bin/sh
# I1237: an IFF SMUS song names its instruments but holds none; the sonix player reads Instruments/<name>.instr and .ss beside
# the song. A remote song cached alone played silent. The nuade CONTENT_COMPANIONS hook names the instrument files so the
# library cache fetches them. Real song: ACE2 (Allister Brimble), fetched by checksum (tests/m3/fixtures/smus_ace2.SOURCE).
# Runs: (a) local song with Instruments beside it, (b) the song alone, (c) OPEN modland:<song> through a local HTTP mirror.
# Asserts: (b) is quieter than (a) by SMUS_RATIO (3); (c) cached Instruments/*.instr (the hook ran: sentinel) and its energy
# is within 1% of (a). Exit 77 = skipped (no network).
# usage: smus_instruments.sh <selecta-cli> <module dir>
set -u
cli=$1
export SELECTA_PLUGIN_PATH=$2
here=$(cd "$(dirname "$0")" && pwd)
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
w=$(mktemp -d) || exit 1
srv=
trap '[ -n "$srv" ] && kill "$srv" 2>/dev/null; rm -rf "$w"' EXIT
mkdir -p "$w/full/Instruments" "$w/alone" "$w/mirror/IFF-SMUS/Brimble/ACE2/Instruments" "$w/cache"
grep '^https' "$here/fixtures/smus_ace2.SOURCE" > "$w/rows.txt"
while read -r url sum; do
  name=$(basename "$url" | sed 's/%20/ /g;s/%28/(/g;s/%29/)/g')
  p=$("$here/fetch_fixture.sh" "$url" "$sum" "smus_$name"); rc=$?
  [ $rc -eq 0 ] || exit $rc
  case "$name" in
    *.smus) cp "$p" "$w/full/ace2.smus"; cp "$p" "$w/alone/ace2.smus"; cp "$p" "$w/mirror/IFF-SMUS/Brimble/ACE2/ace2.smus" ;;
    *) cp "$p" "$w/full/Instruments/$name"; cp "$p" "$w/mirror/IFF-SMUS/Brimble/ACE2/Instruments/$name" ;;
  esac
done < "$w/rows.txt"
frames=441000
run() { # <tag> <key> ; sets E
  rm -f "$w/$1.raw"
  printf '@dump %s\nOPEN %s\nPLAY\n@render %s\nSTOP\n' "$w/$1.raw" "$2" "$frames" > "$w/$1.sel"
  out=$("$cli" script "$w/$1.sel" --db "$w/$1.db" 2>&1) || fail "$1: script failed: $out"
  case "$out" in *ERROR*|*open_failed*) fail "$1: $out" ;; esac
  E=$(energy "$w/$1.raw")
}
run full "$w/full/ace2.smus"; e_full=$E
run alone "$w/alone/ace2.smus"; e_alone=$E
[ "$e_full" -gt 1000000 ] || fail "song with instruments is silent (energy $e_full)"
[ $((e_alone * ${SMUS_RATIO:-3})) -lt "$e_full" ] || fail "song alone not quieter: alone=$e_alone full=$e_full"
port=$(python3 -I -c "import socket;s=socket.socket();s.bind(('127.0.0.1',0));print(s.getsockname()[1])")
(cd "$w/mirror" && exec python3 -I -m http.server "$port" --bind 127.0.0.1 >/dev/null 2>&1) &
srv=$!
sleep 1
export SELECTA_NET_ALLOW=127.0.0.1 SELECTA_CACHE_DIR="$w/cache" SELECTA_MIRRORS_MODLAND="http://127.0.0.1:$port/"
unset SELECTA_MIRROR
run remote "modland:IFF-SMUS/Brimble/ACE2/ace2.smus"; e_remote=$E
n=$(ls "$w/cache/files/modland/IFF-SMUS/Brimble/ACE2/Instruments" 2>/dev/null | grep -c '\.instr$')
[ "$n" -gt 0 ] || fail "no Instruments/*.instr in the cache (the hook did not run)"
d=$((e_remote - e_full)); [ $d -ge 0 ] || d=$((0 - d))
[ $d -le $((e_full / 100)) ] || fail "remote energy $e_remote differs from local $e_full by more than 1%"
echo "smus_instruments_ok instr_fetched=$n full=$e_full alone=$e_alone remote=$e_remote"
