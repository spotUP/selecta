# M3 I162a: HVSC network fetch of Songlengths.md5 and STIL.txt

## What changed
- `LIBRARY_SYNC hvsc <http(s) url>` now downloads `<base>/C64Music/DOCUMENTS/Songlengths.md5` and `STIL.txt` through the HTTP client into `<cache>/sync/hvsc`, derives `hvsc_listing.txt` from the `; /PATH` lines of Songlengths.md5, then runs the existing file based `sel_hvsc_sync`. The fixture-dir path (`hvsc <dir>`) is unchanged.
- New `src/library/sources/hvsc_net.{c,h}` and `sync_net.{c,h}`. `sync_net` holds the HTTP file download moved out of `modland_net.c`, now shared by both.
- `sync_util.h` now holds shared `sel_sync_chomp`, `sel_sync_request` and `sel_sync_flush_scan`; `modland_sync.c`, `hvsc_sync.c` and `hippo_sync.c` use them (private chomp and flush copies removed).
- `sel_sync_info` gained `net_songlengths_bytes`, `net_stil_bytes`, `net_listing_derived` (and reuses `net_requests`); the CLI prints them.

## Host path
Verified with curl on 2026-10-23: `https://hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5` HTTP 200 (5205150 bytes); `.../STIL.txt` HTTP 200 (3750199 bytes). The mirror `https://hvsc.brona.dk/HVSC/C64Music/DOCUMENTS/` also returns 200 for both. The mirror list entry `hvsc` in `net/mirrors.c` already carries `https://hvsc.c64.org/download/`.
NOT verified: `SEL_HVSC_HOST_DIR` (`pub/modules/HVSC/`) in hvsc_sync.h is the Modland-hosted layout for song files; ftp.modland.com was not checked.

## Test
`source-hvsc-net` (tests/net/hvsc_net.cmake, labels m3, net-fixture lock). Runs the CLI with the net fixture (SELECTA_NET_MAP) and asserts counts measured from the fixture files: listing rows, md5 lines, subtunes, STIL entries, db songs, two song rows, plus sentinels net_requests == 2, byte counts equal file sizes, net_listing_derived equal to row count.
Affected tests pass: source-hvsc, source-hvsc-net, source-modland, source-modland-net, source-modland-key, source-hippo, hippo-publish (100%).

## Revert check
With the engine.c URL dispatch disabled (`if (0 && ...)`), `source-hvsc-net` FAILED (required regex hvsc_net_ok not found). Restored, rebuilt, passes.

## Builds
Host and Amiga (cmake/amiga-68020.cmake) both build, 0 warnings outside third_party.

## Open items
- The listing is derived from Songlengths.md5, so only SIDs that have lengths appear.
- The default base URL is not wired into the engine: the caller passes the URL.
- `SEL_LIB_REQ_ANNOTATE = 8` collision risk and the `source.h` line 72 comment from I162 remain.
- The worktree had empty third_party/nuade and libopenmpt submodules; contents were copied from the main checkout only so cmake could configure. Nothing under third_party is committed.
