#!/bin/sh
# I710al1: mutation smoke test of the GoatTracker loader. Fixed-seed byte mutations of a GT1 ("GTS!") and a GT2 ("GTS2") song are rendered
# through `selecta-cli render`; no mutant may crash (signal exit, status >= 128) or run past the time budget. SELECTA_GT_MAX_SECONDS (set by
# ctest, default 3 here) caps the loop-detection scan so a mutant that never repeats ends fast. A mutant may be refused or played.
# Songs are fetched by checksum (tests/m3/fixtures/goattracker.SOURCE); exit 77 = skipped.
# usage: goattracker_mutate.sh <selecta-cli>
set -u
cli=$1
here=$(dirname "$0")
fail() { echo "FAIL: $*"; exit 1; }
fetch() { "$here/fetch_fixture.sh" "$1" "$2" "$3" || exit $?; }
SELECTA_GT_MAX_SECONDS=${SELECTA_GT_MAX_SECONDS:-3}
export SELECTA_GT_MAX_SECONDS
s1=$(fetch "https://modland.com/pub/modules/GoatTracker%202/Cadaver/dojo.sng" 0dc659f11a7fbf1c0199d42e4b6827a3cf9bd67b6bc1f8930ecdace3aea330c5 dojo.sng) || exit $?
s3=$(fetch "https://modland.com/pub/modules/GoatTracker/Aeuk/metal%20warrior%204%20-%20streets.sng" 34da331cf5ecacc7150caca940e5a631daf3fd871a02acee5e4758c2fa64a4ad streets.sng) || exit $?
n=0
for src in "$s1" "$s3"; do
  size=$(wc -c < "$src" | tr -d ' ')
  seed=1
  while [ "$seed" -le 30 ]; do
    cp "$src" mut.sng
    # Five fixed-seed LCG offsets and values per mutant, written in place.
    awk -v seed="$seed" -v size="$size" 'BEGIN { x = seed * 7919; for (i = 0; i < 5; i++) { x = (x * 1103515245 + 12345) % 2147483648; o = x % size; x = (x * 1103515245 + 12345) % 2147483648; printf "%d %d\n", o, int(x / 256) % 256 } }' > muts.txt
    while read -r off val; do
      printf "$(printf '\\%03o' "$val")" | dd of=mut.sng bs=1 seek="$off" conv=notrunc 2>/dev/null
    done < muts.txt
    start=$(date +%s)
    "$cli" render mut.sng --output mut.wav --seconds 5 --rate 22050 >/dev/null 2>&1
    rc=$?
    [ "$rc" -lt 128 ] || fail "$(basename "$src") seed $seed: render died with status $rc (mut.sng kept)"
    [ $(($(date +%s) - start)) -le 60 ] || fail "$(basename "$src") seed $seed: render hung past 60 s"
    n=$((n + 1))
    seed=$((seed + 1))
  done
done
rm -f mut.sng mut.wav muts.txt
echo "mutants=$n"
echo goattracker_mutate_ok
