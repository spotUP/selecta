#!/bin/sh
# T71 source-local (rescan and watcher part, I160): after the first scan a file added to ONE subdirectory
# of a fixture copy in the build dir is picked up by rescanning only that directory.
#   --watch: desktop folder watcher (FSEvents / inotify): watch_events >= 1, rescanned_dirs == 1
#   --poll : directory mtime poll (the Amiga path): rescanned_dirs == 1, watch_events == 0
#   rescan : the manual entry, flat per directory
# Deterministic: the CLI prints watch_ready=1 once armed, the wait for it is bounded (10 s), the watch
# itself is bounded (8 s); the asserts run after the process exits.
# usage: source_watch.sh <selecta-cli> <manifest> <mod> <mod>
set -eu
cli=$1
manifest=$2
modA=$3
modB=$4
pid=0
root=$(mktemp -d "${TMPDIR:-/tmp}/sel-t71w.XXXXXX")
trap 'rm -rf "$root"; if [ "$pid" -gt 0 ]; then kill "$pid" 2>/dev/null; fi; true' EXIT
mkdir -p "$root/tree/sub" "$root/tree/other"
cp "$modA" "$root/tree/one.mod"
cp "$modB" "$root/tree/sub/two.mod"
expect=$(grep -c . "$manifest")
field() { echo "$1" | tr ' ' '\n' | sed -n "s/^$2=//p"; }

# run_mode <watch|poll> <file to add> <expected db_songs afterwards>
run_mode() {
  mode=$1; newfile=$2; songs=$3
  log="$root/$mode.log"
  : > "$log"
  "$cli" library scan --db "$root/lib.db" "--$mode" 8000 "$root/tree" > "$log" 2>&1 &
  pid=$!
  n=0
  until grep -q '^watch_ready=1' "$log"; do
    n=$((n + 1))
    [ "$n" -le 100 ] || { echo "FAIL $mode never became ready"; exit 1; }
    sleep 0.1
  done
  cp "$modA" "$newfile"
  wait "$pid" || { echo "FAIL $mode exit"; cat "$log"; exit 1; }
  pid=0
  out=$(grep 'rescanned_dirs=' "$log")
  echo "$mode: $out"
  [ "$(field "$out" rescanned_dirs)" = 1 ] || { echo "FAIL $mode rescanned_dirs != 1"; exit 1; }
  [ "$(field "$out" rescan_files)" -ge 1 ] || { echo "FAIL $mode rescan_files"; exit 1; }
  [ "$(field "$out" db_songs)" = "$songs" ] || { echo "FAIL $mode db_songs != $songs"; exit 1; }
}

run_mode watch "$root/tree/sub/three.mod" $((expect + 1))
out=$(grep 'rescanned_dirs=' "$root/watch.log")
[ "$(field "$out" watch_events)" -ge 1 ] || { echo "FAIL watch_events"; exit 1; }
run_mode poll "$root/tree/other/four.mod" $((expect + 2))
out=$(grep 'rescanned_dirs=' "$root/poll.log")
[ "$(field "$out" watch_events)" = 0 ] || { echo "FAIL poll watch_events"; exit 1; }

cp "$modB" "$root/tree/sub/five.mod"
out=$("$cli" library rescan --db "$root/lib.db" "$root/tree/sub")
echo "rescan: $out"
[ "$(field "$out" rescanned_dirs)" = 1 ] || { echo "FAIL rescan rescanned_dirs"; exit 1; }
[ "$(field "$out" db_songs)" = $((expect + 3)) ] || { echo "FAIL rescan db_songs"; exit 1; }
echo "source_watch_ok"
