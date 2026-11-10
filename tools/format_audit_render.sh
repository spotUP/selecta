#!/bin/sh
# I1242g: one render of the format audit. Every audit render passes --config-dir (replayers that declare system files, e.g. bbmp_cybertracker,
# bbmp_shroom, stonetracker, bbmp_ned, report system_file_missing without it and the row looks like a dead format).
# usage: format_audit_render.sh <song> <out.wav> [seconds]
# env: SELECTA_CLI (default build/host/apps/selecta-cli/selecta-cli), AUDIT_CONFIG_DIR (default ~/.config/selecta; read-only use: point it at a
# scratch copy when the cli may download system files into it). Prints the cli output and "peak N" of the WAV.
set -u
cli=${SELECTA_CLI:-build/host/apps/selecta-cli/selecta-cli}
cfg=${AUDIT_CONFIG_DIR:-$HOME/.config/selecta}
song=$1
out=$2
secs=${3:-6}
"$cli" render "$song" --output "$out" --seconds "$secs" --resample linear --config-dir "$cfg" 2>&1 | tail -3
[ -f "$out" ] && od -An -v -t d2 -j 44 "$out" | tr -s ' ' '\n' | awk '$1 != "" { a = $1 < 0 ? -$1 : $1; if (a > m) m = a } END { print "peak", m + 0 }'
exit 0
