# M3 Phase 3c report: Mirrors (I156, T69)

Implements plan `2026-10-25-m3-library.md` Phase 3 row I156 and design doc section 9 "Transports".

## Done

- `src/library/net/mirrors.[ch]`: `sel_mirror_set` per source, `sel_mirrors_init/rank/fetch`. Rank: home mirror
  (`SELECTA_MIRROR`) first, then measured mirrors by time to first byte, then unmeasured in list order, then
  failed ones. Each mirror is tried at most once per fetch; any failure (connect, 3 s first-byte timeout, non-2xx,
  FTP error) moves to the next; a `restart` callback lets the sink drop a partial body. HTTP and FTP mirrors both
  work (existing clients).
- Amiga route: `amiga_route` replaces the list by `<devilbox>/api/mirror/<source>/<path>` (plain HTTP,
  `SELECTA_DEVILBOX_URL` or `http://devilbox.uprough.net`); the home mirror still leads.
- `selecta-cli library fetch <source>:<path> [--machine n] [--rounds n] [--out f]`
  (`apps/selecta-cli/mirror_fetch.c`, hooked in `library.c`); prints `mirror_tries= mirror_retries= mirror_served=
  mirror_order=` and per-round and per-mirror counters.
- Test seam: `SELECTA_MIRRORS_<SOURCE>` replaces the built-in list (modland: amigascne HTTP then
  ftp.modland.com; hvsc: hvsc.c64.org).
- T69 `mirrors-ranking` (`tests/net/mirrors_ranking.cmake`, block at the end of `tests/CMakeLists.txt`) over
  fixture hosts `dead|slow|fast|home.mirror.test` and `devilbox.uprough.net/api/mirror/modland/`.

## Tests

`ctest -R "net-guard|http-client|ftp-transport|mirrors-ranking|net-fixture"`: 7 of 7 pass (mirrors-ranking 3.3 s).
T69 asserts: round 1 tries=3 retries=2 served=fast; round 2 tries=1 retries=0, order 2,0,1; dead mirror attempted
exactly once; body sha256; server log counts (fast twice, dead never); home mirror served with 1 try; dead home
costs 1 retry; Amiga fetch hits `/api/mirror/modland/<path>` once and ignores the list. Amiga build of
`selecta_library` (68020) compiles.

## Open questions

1. The spec's "dead mirror costs exactly 1 retry" and the 4 s mirror: with the 3 s first-byte limit the 4 s mirror
   also fails, so round 1 has 2 retries. Default: each failing mirror costs one retry, and the test asserts the dead
   mirror is attempted once in total. Alternative: a probe round that does not count as retries.
2. Ranking lives in the process only (`sel_mirror_set`). Default: the Phase 4 sources keep a set per source.
   Alternative: persist ttfb in the library DB (needs a schema change).
3. Built-in modland list is a guess from the research doc; confirm against `mirrors.txt` on ftp.modland.com.
4. HVSC and other sources' bases, md5 retry on mismatch (T75) are for Phase 4/5.

## Remaining

Nothing in I156. T97 (rig, I199) uses the Amiga route on real bsdsocket.
