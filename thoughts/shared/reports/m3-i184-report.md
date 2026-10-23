# I184 report: duplicates by md5, best source plays on OPEN

Row T86 (ctest similar-duplicates), decision D-M3-14.

## Done
- src/library/db/duplicates.c: worker ops SEL_DUP_HASH, SEL_DUP_NEXT, SEL_BEST_SOURCE plus caller halves
  sel_library_hash_local, sel_library_duplicates, sel_library_best_source.
- `selecta-cli library duplicates --db <db>` prints `group=<md5>|<members>|<best key>` per group and `dupes=N`.
- Engine read_song asks sel_library_best_source first; when a better twin exists it reads that one
  (local key: the file path; remote key: the existing fetch/cache path). Stats: dup_redirects, dup_played.
- `selecta-cli script` accepts `--db` alone and has the directive `@dup_stats` (sentinel: `DUP redirects=N played=<key>`).
- tests/net/similar.cmake extended (same test, no parallel test): 3 planted groups, md5 computed by cmake file(MD5),
  dupes=3, best key per group, local md5 stored, OPEN of the modland copy plays the local file
  (redirects=1, played=local key), songs_opened=2.

## Caveats
- Rank: local, modland, hvsc, hippo, other. Home mirror and cache are transports of a modland/hvsc key, not source rows, so they are ranked implicitly.
- The D-M3-14 "md5-verified first" tie-break is replaced by library order (lowest id).
- Local md5 is hashed lazily by the duplicates step. OPEN redirects only after the md5s exist.
- engine.c checks the "local:" prefix itself: including library/source.h clashes with playlist.h (sel_playlist typedef).
- The test needs the sqlite3 shell (as I183); it passes -cmd "PRAGMA trusted_schema=1" because of the FTS triggers.

## Not run
Full ctest, lint target. The Amiga build (build/amiga) compiles and links.
