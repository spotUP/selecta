#!/bin/sh
# T36 helper: writes its arguments, one per line, to the file named by ARGDUMP_OUT.
: > "$ARGDUMP_OUT"
for a in "$@"; do
  printf '%s\n' "$a" >> "$ARGDUMP_OUT"
done
