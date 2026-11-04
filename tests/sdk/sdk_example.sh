#!/bin/sh
# I427c (design 2026-10-30-plugin-loader-sdk D7): the installed SDK builds an out-of-tree replayer and the product loads it.
# Reachability through the product entry point (selecta-cli): the module is found on $SELECTA_PLUGIN_PATH (`plugins` lists
# its path as the origin), then `render` plays a .sine file through it (replayer=sine), the WAV is not silent and the frame
# count matches. Sentinel: the same render with the module folder left out of the path must fail, so the pass above can
# only come from the example build.
# usage: sdk_example.sh <selecta-cli> <cmake> <repo-root> <build-root> <work-dir>
set -u
cli=$1
cmake=$2
src=$3
build=$4
work=$5
field() { printf '%s\n' "$1" | tr ' ' '\n' | sed -n "s/^$2=//p" | head -1; }
fail() { echo "FAIL: $*"; exit 1; }

rm -rf "$work"
mkdir -p "$work" || fail "mkdir $work"
cd "$work" || fail "cd $work"

"$cmake" --install "$build" --prefix "$work/prefix" >install.log 2>&1 || { tail -n 20 install.log; fail "install"; }
[ -f "$work/prefix/include/selecta/plugin.h" ] || fail "plugin.h not installed"
[ -f "$work/prefix/lib/cmake/SelectaSDK/SelectaSDKConfig.cmake" ] || fail "package config not installed"

"$cmake" -S "$src/examples/plugin-sine" -B "$work/example" -DCMAKE_PREFIX_PATH="$work/prefix" \
    -DCMAKE_BUILD_TYPE=Release >configure.log 2>&1 || { tail -n 20 configure.log; fail "configure example"; }
"$cmake" --build "$work/example" >example.log 2>&1 || { tail -n 20 example.log; fail "build example"; }
mod=$(ls "$work"/example/modules/sel_sine.* 2>/dev/null | head -1)
[ -n "$mod" ] || fail "no sel_sine module built"

printf 'SINE 440 2000\n' >"$work/tone.sine"
rate=44100

# Sentinel: without the module on the path the song has no replayer.
if SELECTA_PLUGIN_PATH="$work/empty" "$cli" render "$work/tone.sine" --output sentinel.wav --seconds 1 --rate "$rate" >sentinel.out 2>&1; then
    if grep -q 'replayer=sine' sentinel.out; then fail "sentinel: sine played without the module path"; fi
fi
SELECTA_PLUGIN_PATH="$work/empty" "$cli" plugins >sentinel.list 2>&1
if grep -q '^sine ' sentinel.list; then fail "sentinel: sine listed without the module path"; fi

export SELECTA_PLUGIN_PATH="$work/example/modules"
"$cli" plugins >plugins.out 2>&1 || fail "plugins failed"
line=$(grep '^sine ' plugins.out)
[ -n "$line" ] || fail "sine not listed by selecta-cli plugins"
case $line in *"origin=$mod"*) ;; *) fail "origin of sine is not the built module: $line" ;; esac

out=$("$cli" render "$work/tone.sine" --output tone.wav --seconds 1 --rate "$rate" 2>/dev/null) || fail "render failed"
[ "$(field "$out" replayer)" = "sine" ] || fail "replayer=$(field "$out" replayer), want sine"
got=$(field "$out" rendered_frames)
awk -v g="$got" -v w="$rate" 'BEGIN { d = g - w; if (d < 0) d = -d; exit !(d <= w * 0.01) }' ||
    fail "rendered_frames=$got, want $rate +-1%"
e=$(od -An -v -t d2 -j 44 tone.wav | tr -s " " "\n" | awk '$1 != "" { s += $1 * $1 } END { printf "%d", s }')
[ "$e" -gt 1000000 ] || fail "silent output (energy $e)"
echo sdk_example_ok
