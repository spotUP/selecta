# SNDH test fixture provenance

File: `oculus_ice.sndh` (1002 bytes, ICE! packed, magic `ICE!`).

Origin: "We accidentally your Oculus" by Zerkman (2018), from the SNDH archive at https://sndh.atari.org
(`sndh_lf/Zerkman/We_accidentally_your_Oculus.sndh`, the 2026 low-footprint archive, 4250 bytes unpacked; header tags
TITL, COMM Zerkman, CONV "Zerkman's simple YM tracker", YEAR 2018, TC50).

Packing: the archive's current files are all stored depacked, so the ICE! packed form was made from that file with
upstream unice68 (`unice68 -p`, packer 2.35, source `unice68/unice68_pack.c` of the sc68 commit named in
`third_party/sc68/VERSION.txt`, built outside the repository). `unice68 -d` of the fixture gives back the original
4250 bytes exactly (cmp equal). The fixture therefore exercises the module's ICE! detection and the vendored
depacker, which is what packed SNDH files in the wild (older archive releases) need.

Terms: UNVERIFIED. sndh.atari.org states no licence for the files; the tune is the work of its composer. It is
kept here only as a 1 KB decoding test input. Before a public release, either obtain the composer's permission in
writing or replace it with a tune whose author licenses it for redistribution.
