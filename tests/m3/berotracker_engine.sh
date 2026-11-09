#!/bin/sh
# I710ba (4): runs tests/m3/berotracker_engine.c over three songs open at once (two different BeRo songs and the first one again).
# Fetched by checksum (tests/m3/fixtures/bbmp_berotracker.SOURCE); exit 77 = skipped (no network).
# usage: berotracker_engine.sh <berotracker_engine binary>
set -u
exe=$1
here=$(dirname "$0")
M=https://modland.com/pub/modules/BeRoTracker/BeRo
s1=$("$here/fetch_fixture.sh" "$M/aa2s.brt" ae3917b011bafbac1d486bcaa47e76a107a01ed8871a62ef573b640289980da7 aa2s.brt) || exit $?
s2=$("$here/fetch_fixture.sh" "$M/b2.brt" c5097a3e2964c2b81f115a23b6a8b6fec9a6bd5107b2df85a650a2bcfc75a4be b2.brt) || exit $?
exec "$exe" "$s1" "$s2" "$s1"
