#!/bin/sh
# T81 cues driver (I175): CUE_SET / CUE_GOTO over the cues table across a restart.
# Process 1: SEEK 5000, 441 frames, CUE_SET 1, SEEK 0, CUE_GOTO 1 -> SEEKED reports the cue position (5010 ms);
# CUE_GOTO 9 -> ERROR no_such_cue. Process 2 (same --db): CUE_GOTO 1 gives the same SEEKED. Prints cues_ok.
# usage: cues.sh <selecta-cli> <catalogue.tsv> <cues.sel> <cues2.sel>
set -u
cli=$1
rm -f cues.db
out1=$("$cli" script "$3" --catalogue "$2" --db cues.db 2>&1) || { echo "$out1"; exit 1; }
seeked=$(printf '%s\n' "$out1" | grep '^SEEKED')
n=$(printf '%s\n' "$seeked" | wc -l | tr -d ' ')
[ "$n" = 3 ] || { echo "process 1: $n SEEKED lines"; printf '%s\n' "$out1"; exit 1; }
first=$(printf '%s\n' "$seeked" | sed -n 1p)
zero=$(printf '%s\n' "$seeked" | sed -n 2p)
goto=$(printf '%s\n' "$seeked" | sed -n 3p)
want="SEEKED 220941" # SEEK 5000 ms = 220500 frames, plus the 441 frames rendered before CUE_SET
[ "$first" = "SEEKED 220500" ] || { echo "SEEK 5000 reported: $first"; exit 1; }
[ "$goto" = "$want" ] || { echo "CUE_GOTO reported $goto, want $want"; exit 1; }
[ "$zero" != "$goto" ] || { echo "SEEK 0 equals cue position"; exit 1; }
printf '%s\n' "$out1" | grep -q 'no_such_cue' || { echo "no no_such_cue"; exit 1; }
printf '%s\n' "$out1" | grep -q '^USERDATA cue_sets=1 cue_gotos=1 sticker_ops=0' || { echo "counters 1"; printf '%s\n' "$out1" | grep USERDATA; exit 1; }
out2=$("$cli" script "$4" --catalogue "$2" --db cues.db 2>&1) || { echo "$out2"; exit 1; }
[ "$(printf '%s\n' "$out2" | grep '^SEEKED')" = "$want" ] || { echo "restart position differs"; printf '%s\n' "$out2"; exit 1; }
printf '%s\n' "$out2" | grep -q '^USERDATA cue_sets=0 cue_gotos=1 sticker_ops=0' || { echo "counters 2"; exit 1; }
rm -f cues.db
echo cues_ok
