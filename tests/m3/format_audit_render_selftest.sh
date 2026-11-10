#!/bin/sh
# I1242g: the audit render helper always hands --config-dir to the cli (default ~/.config/selecta, AUDIT_CONFIG_DIR overrides).
# A fake cli records its arguments; the check fails when the option is dropped from tools/format_audit_render.sh.
# usage: format_audit_render_selftest.sh [tool]   (tool defaults to tools/format_audit_render.sh)
set -u
here=$(cd "$(dirname "$0")" && pwd)
tool=${1:-$here/../../tools/format_audit_render.sh}
d=$(mktemp -d) || exit 1
trap 'rm -rf "$d"' EXIT
printf '#!/bin/sh\nprintf "%%s\\n" "$*" > "$FAKE_ARGS"\n' > "$d/fakecli"
chmod +x "$d/fakecli"
export SELECTA_CLI="$d/fakecli" FAKE_ARGS="$d/args"
fail() { echo "FAIL: $*"; exit 1; }
unset AUDIT_CONFIG_DIR
sh "$tool" song.x "$d/o.wav" 1 >/dev/null
grep -q -- "--config-dir $HOME/.config/selecta" "$d/args" || fail "default --config-dir missing: $(cat "$d/args")"
AUDIT_CONFIG_DIR=/x/y sh "$tool" song.x "$d/o.wav" 1 >/dev/null
grep -q -- "--config-dir /x/y" "$d/args" || fail "AUDIT_CONFIG_DIR not passed: $(cat "$d/args")"
echo "format_audit_render_selftest: ok"
