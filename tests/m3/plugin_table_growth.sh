#!/bin/sh
# I1133: the module table must hold every module a plugin folder offers. 400 copies of one real module (more than the
# old 192-slot table plus the built-ins) load through SELECTA_PLUGIN_PATH; none may be rejected with "module table full".
# The copies share one plugin id, so each is rejected as a duplicate id, which proves the loader reached all of them.
set -u
cli="$1"
mods="$2"
src=$(ls "$mods"/sel_null.* 2>/dev/null | head -n 1)
[ -n "$src" ] || { echo "no sel_null module in $mods"; exit 77; }
ext=${src##*.}
d=$(mktemp -d)
trap 'rm -rf "$d"' EXIT
i=0
while [ $i -lt 400 ]; do
  cp "$src" "$d/sel_zz_copy_$i.$ext"
  i=$((i + 1))
done
out=$(SELECTA_PLUGIN_PATH="$d" "$cli" plugins 2>&1)
full=$(printf '%s\n' "$out" | grep -c 'module table full')
dups=$(printf '%s\n' "$out" | grep -c 'duplicate plugin id')
echo "table_full=$full duplicates=$dups"
[ "$full" -eq 0 ] || { echo "FAIL: modules rejected with module table full"; exit 1; }
[ "$dups" -ge 399 ] || { echo "FAIL: expected 399 duplicate rejections (every copy reached)"; exit 1; }
echo "plugin_table_growth_ok"
