#!/bin/sh
# Fetch a game-ROM test fixture (data we must not commit to the public repo) into a git-ignored cache.
# usage: fetch_fixture.sh <url> <sha256> <local-name>   -> prints the cached file path on stdout
# Cache: ${FIXTURE_CACHE:-tests/m3/fixtures/.cache} (git-ignored). Every use re-checks the sha256.
# Exit codes: 0 ok, 77 download failed (ctest SKIP_RETURN_CODE 77), 1 checksum mismatch (a real failure, never a skip).
set -u
url=$1
want=$2
name=$3
cache=${FIXTURE_CACHE:-$(dirname "$0")/fixtures/.cache}
mkdir -p "$cache" || { echo "FAIL: cannot create $cache" >&2; exit 1; }
dest="$cache/$name"
sum() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }
if [ -f "$dest" ] && [ "$(sum "$dest")" = "$want" ]; then printf '%s\n' "$dest"; exit 0; fi
rm -f "$dest"
tmp="$dest.part.$$"
if ! curl -fsSL --retry 2 --max-time 120 -o "$tmp" "$url" 2>/dev/null; then
  rm -f "$tmp"; echo "SKIP: cannot download $url" >&2; exit 77
fi
got=$(sum "$tmp")
if [ "$got" != "$want" ]; then
  rm -f "$tmp"; echo "FAIL: sha256 mismatch for $url: got $got, want $want" >&2; exit 1
fi
mv "$tmp" "$dest" && printf '%s\n' "$dest"
