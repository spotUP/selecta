#!/bin/sh
# I704c: qsf-real-decode fetches the Capcom game set (not redistributable, not in git) via fetch_fixture.sh, then runs
# qsf_decode.sh. Sources and checksums: tests/m3/fixtures/mm1_title.miniqsf.SOURCE. Exit 77 (SKIP) when offline.
# usage: qsf_real_decode.sh <selecta-cli>
set -u
here=$(dirname "$0")
base='https://modland.antarctica.no/pub/modules/Capcom%20Q-Sound%20Format/Hideki%20OK/Megaman%201%20-%20The%20Power%20Battle'
song=$("$here/fetch_fixture.sh" "$base/01%20-%20title.miniqsf" 6bd52eadd9f60041e1489687b48c431d353560f722438475d4dc8ec2761fc098 mm1_title.miniqsf) || exit $?
lib=$("$here/fetch_fixture.sh" "$base/Mega%20Man%20-%20The%20Power%20Battle.qsflib" 6c95fbf2f1beecd8dd59ce30c725e21352c714fc5dc1e1c5c8b1e0dc1dcbd481 'Mega Man - The Power Battle.qsflib') || exit $?
QSF_LIB=$lib QSF_MIN_SECONDS=21 QSF_MAX_SECONDS=22 QSF_ENERGY_MIN=1000000000000 exec sh "$here/qsf_decode.sh" "$1" "$song"
