#!/bin/sh
# I862: a .vgz (gzip) and a .vgm file must never end as open_failed. libgme's gme_identify_header names "VGM"/"VGZ" for
# the "Vgm " magic and the gzip magic even when the VGM emulator is not compiled in, so the gme probe said YES and
# gme_open_data then failed. Reachability test through `selecta-cli render`: the verdict is either a replayer that
# plays the file (libvgm, I431) or no_replayer; open_failed is the defect.
# usage: vgz_no_open_failed.sh <selecta-cli> <vgz-file> <vgm-file>
set -u
cli=$1
for f in "$2" "$3"; do
    out=$("$cli" render "$f" --output fd_vgz.wav --seconds 1 2>&1)
    case $out in
    *open_failed*) echo "FAIL: $f: $out"; exit 1 ;;
    esac
done
echo vgz_no_open_failed_ok
