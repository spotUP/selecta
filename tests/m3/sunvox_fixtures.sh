#!/bin/sh
# I706c: the SunVox test songs are not committed (no licence line for the tune). Fetch the original from the public SunVox
# engine source release (pinned commit, sha256 checked by fetch_fixture.sh) and derive the modern-format twin by
# re-saving it with the SunVox Library (sunvox_resave.c). Origin notes: fixtures/sunvox_tune.sunvox.SOURCE.
# usage: sunvox_fixtures.sh <outdir> [<sunvox library file>]   -> prints "<old.sunvox>" (and "<modern.sunvox>" with a library)
# Exit 77 when the download fails (ctest SKIP), 1 on a checksum mismatch or a failed re-save.
# $SUNVOX_TUNE_URL / $SUNVOX_TUNE_SHA256 override the pins (red proofs only).
set -u
here=$(dirname "$0")
out=$1
lib=${2:-}
url=${SUNVOX_TUNE_URL:-https://raw.githubusercontent.com/warmplace/sunvox_sources/e5e38200984567e37a4615048f198977ca7a07a6/sunvox_api_examples/interactive_sound/make/tune.sunvox}
sha=${SUNVOX_TUNE_SHA256:-4cb6e4080c68718535833fdd98b062ef07ec01cd0c83b2153b2183642c258b02}
old=$("$here/fetch_fixture.sh" "$url" "$sha" sunvox_tune.sunvox) || exit $?
printf '%s\n' "$old"
[ -n "$lib" ] || exit 0
mkdir -p "$out" || exit 1
cc -O1 -o "$out/sunvox_resave" "$here/sunvox_resave.c" || { echo "FAIL: cannot build sunvox_resave" >&2; exit 1; }
"$out/sunvox_resave" "$lib" "$old" "$out/sunvox_tune_modern.sunvox" || { echo "FAIL: re-save with the SunVox Library failed" >&2; exit 1; }
printf '%s\n' "$out/sunvox_tune_modern.sunvox"
