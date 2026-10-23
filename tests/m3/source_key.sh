#!/bin/sh
# T72 source-modland-key: song key and Modland URL round trip through selecta-cli library key.
# usage: source_key.sh <selecta-cli>
set -eu
cli=$1
out=$("$cli" library key modland "pub/modules/Protracker/Some Artist/some song.mod")
echo "$out"
echo "$out" | grep -q '^key=modland:pub/modules/Protracker/Some Artist/some song.mod$' || { echo "FAIL modland key"; exit 1; }
echo "$out" | grep -q '^url=https://modland.com/pub/modules/Protracker/Some%20Artist/some%20song.mod$' || { echo "FAIL modland url"; exit 1; }
echo "$out" | grep -q '^roundtrip=1$' || { echo "FAIL modland roundtrip"; exit 1; }
out=$("$cli" library key local "Work:Music/a.mod")
echo "$out"
echo "$out" | grep -q '^key=local:Work:Music/a.mod$' || { echo "FAIL local key"; exit 1; }
echo "$out" | grep -q '^roundtrip=1$' || { echo "FAIL local roundtrip"; exit 1; }
if "$cli" library key nosuch "x/y" >/dev/null 2>&1; then echo "FAIL unknown source accepted"; exit 1; fi
echo "source_key_ok"
