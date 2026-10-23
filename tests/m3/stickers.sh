#!/bin/sh
# T82 stickers driver (I176): round trip, name order, delete, restart. Prints stickers_ok.
# usage: stickers.sh <selecta-cli> <catalogue.tsv> <stickers.sel> <stickers2.sel>
set -u
cli=$1
rm -f stickers.db
out1=$("$cli" script "$3" --catalogue "$2" --db stickers.db 2>&1) || { echo "$out1"; exit 1; }
# The value 4 replaced 5; LIST shows both stickers in name order (mood before rating); the deleted name is gone.
for want in 'rating' 'calm' 'no_such_sticker' 'sticker_ops=8'; do
    printf '%s\n' "$out1" | grep -q "$want" || { echo "missing '$want':"; printf '%s\n' "$out1"; exit 1; }
done
mood=$(printf '%s\n' "$out1" | grep -n 'SRC_NAME=mood' | head -1 | cut -d: -f1)
rating=$(printf '%s\n' "$out1" | grep -n 'SRC_NAME=rating' | sed -n 2p | cut -d: -f1)
[ "$mood" -lt "$rating" ] || { echo "LIST not in name order:"; printf '%s\n' "$out1"; exit 1; }
out2=$("$cli" script "$4" --catalogue "$2" --db stickers.db 2>&1) || { echo "$out2"; exit 1; }
printf '%s\n' "$out2" | grep -q 'sticker_ops=2' || { echo "counter 2"; printf '%s\n' "$out2"; exit 1; }
printf '%s\n' "$out2" | grep -q 'rating' || { echo "sticker lost over restart:"; printf '%s\n' "$out2"; exit 1; }
printf '%s\n' "$out2" | grep -q 'mood' && { echo "deleted sticker came back:"; printf '%s\n' "$out2"; exit 1; }
rm -f stickers.db
echo stickers_ok
