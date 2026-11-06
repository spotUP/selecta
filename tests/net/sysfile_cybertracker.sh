#!/bin/sh
# I710ga7: fetch the git-ignored fixtures by url + sha256 (exit 77 offline = ctest SKIP), then run sysfile_cybertracker.cmake.
# usage: sysfile_cybertracker.sh <cmake> <selecta> <work> <fixdir> <plugins> <net dir> <m3 dir>
set -u
cmake=$1 selecta=$2 work=$3 fixdir=$4 plugins=$5 netdir=$6 m3=$7
zip=$("$m3/fetch_fixture.sh" "https://csdb.dk/getinternalfile.php/63969/cybertracker1_01-d64.zip" cdecc06385ebb5cd1d941889cae8c1ecf33e72965116db652a4e43c50d4040fe cybertracker1_01-d64.zip) || exit $?
song=$("$m3/fetch_fixture.sh" "https://modland.com/pub/modules/Cybertracker%20C64/CyberBrain/arpeggio.ct" d3ae040598e13d825d3f851e6ad18b7efe811f8e83daa0f5cbee0fda780ce40d arpeggio.ct) || exit $?
exec "$cmake" "-DSELECTA=$selecta" "-DWORK=$work" "-DFIXDIR=$fixdir" "-DPLUGINS=$plugins" "-DZIP=$zip" "-DSONG=$song" -P "$netdir/sysfile_cybertracker.cmake"
