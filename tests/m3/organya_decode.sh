#!/bin/sh
# I703: an Organya song (.org) plays through `selecta-cli render` as a RENDER-kind replayer (organya module).
# One reachability test through the product entry point; the sentinel is the `replayer=organya` line of render.
# Asserts: the plugin ran, the WAV is not silent (ORG_ENERGY_MIN raises the floor for red proofs) and the song lasts at
# least ORG_MIN_SECONDS (default 20: one pass up to repeat_end, 20 s in the fixture; the song loops, so --seconds is far above).
# Companion bank: the song is rendered again with wave100.wdb beside it (the fixture bank, square waves, silent drums).
# The two renders must differ: energy must change by at least ORG_BANK_RATIO (default 1.5) in either direction, so a bank
# that is not read fails. Red proof: stage no bank beside the second copy.
# Fixtures: tests/m3/fixtures/org_test.org, org_bank_test.wdb (generated, see org_test.org.SOURCE).
# usage: organya_decode.sh <selecta-cli> <fixture.org> <bank.wdb>
set -u
cli=$1
f=$2
bank=$3
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }
energy() { od -An -v -t d2 -j 44 "$1" | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }'; }
rate=44100
rm -rf fd_org fd_org_bank fd_org.wav fd_org_bank.wav
mkdir fd_org fd_org_bank && cp "$f" fd_org/ && cp "$f" fd_org_bank/ || fail "cannot stage $f"
cp "$bank" fd_org_bank/${ORG_BANK_NAME:-wave100.wdb} || fail "cannot stage $bank"
out=$("$cli" render "fd_org/$(basename "$f")" --output fd_org.wav --seconds 300 --rate "$rate" 2>/dev/null) || fail "render failed: $f"
[ "$(field "$out" replayer)" = "organya" ] || fail "$f: replayer=$(field "$out" replayer), want organya"
got=$(field "$out" rendered_frames)
[ "$got" -ge $((rate * ${ORG_MIN_SECONDS:-20})) ] || fail "$f: rendered_frames=$got, want at least $((rate * ${ORG_MIN_SECONDS:-20}))"
e0=$(energy fd_org.wav)
[ "$e0" -gt "${ORG_ENERGY_MIN:-1000000}" ] || fail "$f: silent output (energy $e0)"
"$cli" render "fd_org_bank/$(basename "$f")" --output fd_org_bank.wav --seconds 300 --rate "$rate" >/dev/null 2>&1 || fail "render with bank failed"
e1=$(energy fd_org_bank.wav)
r=${ORG_BANK_RATIO:-1.5}
awk -v a="$e0" -v b="$e1" -v r="$r" 'BEGIN { exit !(a >= b * r || b >= a * r) }' || fail "$f: energy without bank $e0 and with bank $e1 differ by less than ${r}x (bank not read)"
rm -rf fd_org fd_org_bank fd_org.wav fd_org_bank.wav
echo organya_decode_ok
