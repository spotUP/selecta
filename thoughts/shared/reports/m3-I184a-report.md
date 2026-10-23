# I184a: local md5 filled without `library duplicates`

Plan: thoughts/shared/plans/2026-10-25-m3-library.md, I184 / D-M3-14.

## Problem
OPEN redirects to the best source among songs sharing an md5 (op_best in src/library/db/duplicates.c). The scan only stats files, so local md5 stayed empty until `library duplicates` ran; OPEN of a remote copy could not find its local twin.

## Design
1. OPEN path (op_best): hash_for_open runs before ranking. Local song without md5: hashes itself. Remote song with md5: hashes the unhashed local songs that could be its twin. Reuses md5_of_file and the same UPDATE statements as SEL_DUP_HASH (extracted into store_hash / hash_next; op_hash is now a call to hash_next).
2. Background pass: sel_library_hash_kick / sel_library_hash_poll (db.h). Threaded builds: a job_hash flag on the existing library job thread (job.c), one file at a time, ends at close (job_quit), resumable because every value is stored per file. Cooperative builds (Amiga): kick sets a pending flag, the engine tick calls poll, one file per call, only while no song plays (e->cur == SEL_SLOT_NONE). Kicked when the engine takes its library (open) and after LIBRARY_SCAN succeeds. The scan itself is unchanged.

## Narrowing key
songs.size, stored by the scan for local rows and for remote rows when the catalogue knows it. Remote row with size > 0: hash only local songs with the same size. Otherwise: same file name (basename suffix of path). Remote row with neither: nothing extra, the background pass covers it.

## Tests
- tests/net/similar.cmake (similar-duplicates): new case on a fresh database, local md5 cleared to a scan's state, no `library duplicates`, OPEN of the modland copy must play the local file (DUP redirects=1 played=<path>) and store the local md5.
- Revert check: before the src change the new case failed (ERROR open_failed modland:Planted/amb.mod, DUP redirects=0); after, it passes. Note: with the engine kick, the idle pass can win the race against OPEN, so this case proves the pair (pass or OPEN) rather than the OPEN hash in isolation.
- Passed on build/host: similar-duplicates, library-*, cache-fetch*, prefetch*, library-close-cancel (15 tests selected by regex).
- Builds: host 0 warnings; amiga only pre-existing third_party nuade missing-field-initializers warnings (66), none in touched files. `selecta-cli commands --unimplemented | wc -l` = 8.

## Amiga cost
Scan unchanged. MD5 runs only on idle ticks, one file per tick, so a collection is hashed over time while nothing plays; a single large file on a 68020 can still block one tick for seconds (md5 is tens of KB/s). OPEN hashes at most the files matching the size key, synchronously.
