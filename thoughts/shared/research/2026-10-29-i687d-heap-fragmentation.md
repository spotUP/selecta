---
date: 2026-10-29
row: I687d
status: cause measured, fix not landed (agent budget reached)
---

# I687d: 2 MB tier `library scan` SQLITE_NOMEM grows with the scan root length

Measured on main 7b120c6d (544 KiB tier, I687c), host build `build/host`, 13 songs of
`build/tests/songs` copied under a root of N characters, `--machine a1200`. Temporary probes (not
committed): memsys5 failure branch dumped request size, free lists and a backtrace; a log of every
memsys5 request >= 4000 bytes with caller; `sqlite3_db_status` at the end of `sel_library_scan_batch`.

## Results

| root | db | result | db_heap_peak |
|---|---|---|---|
| 68-200 | :memory: | pass | 317568-332736 |
| 250 | :memory: | NOMEM | 334784 at failure |
| 300 | :memory: | NOMEM | 331712 at failure |
| 120-300 | file | pass | ~280900 |

- Only the `:memory:` database fails. A file database passes at every root length tested (peak flat ~281 KB),
  because its page cache recycles the page slots (cache_size cap); a `:memory:` database cannot evict pages.
- Failing allocations: 1024 bytes in `memjrnlWrite` <- `subjournalPageIfRequired` (statement journal, in memory
  because SQLITE_TEMP_STORE=3) at 250; 4096 bytes in `pcache1Alloc` <- `allocateTempSpace` at 300. Both inside
  `sel_library_scan_batch`.
- At failure the memsys5 part is not fragmented in the large sense: nBlock 10659 x 32 = 341088 usable bytes
  (the 348160-byte part loses 1/33 to memsys5 control bytes), 334784 used, the 6.3 KB free is 5x32, 16x128,
  14x256, 1x512 (+1x1024, 1x2048 at 300). The heap is full. "Peak 310784 of 348160" in I687c compared the peak
  with the raw part size, not the usable 341088, and the peak counter stops at the last successful allocation.

## Where the heap goes (memsys5 part, end of batch, root 60, :memory:)

- schema 115872 bytes (parsed schema: tables, ~15 triggers, FTS5) - a third of the heap, fixed.
- statements 73136 bytes (the three scan statements with their trigger subprograms).
- lookaside: one 48000-byte request (1200 x 40 default) rounded by memsys5 to a 65536 block
  (`openDatabase`); SQLite then uses all 64 KiB as lookaside.
- the `:memory:` database pages: 45-47 of the 47 page slots used; past the slots every page, and every
  pager temp space (`sqlite3PagerSetPagesize`, 4104 bytes, ephemeral tables of the triggers), costs an 8192
  block (4096 + 272 header rounded to a power of two): pcovf 0 at root 60, 8192 at 200, 28672 at 250.
- transient inside the scan transaction: ~100 KB (used 234560 after rollback vs 334784 at failure):
  statement journal chunks, ephemeral tables, FTS5 flush buffers (`fts5WriteInit` 4096).

## Cause

The root path is stored per song three times (files.path, songs.path, songs.key = source:path) plus FTS5
`path` tokens. Longer roots make the `:memory:` database larger; once its pages pass the fixed 47 page slots
each extra page (and each statement-journal copy of a page) comes out of memsys5, page-size objects at 2x
(8192 for 4368). The scan transaction then needs ~100 KB of transient space on top of ~235 KB of fixed
schema + statements + lookaside, in a 341 KB usable part. It is exhaustion driven by data size, with
power-of-two rounding of page-size objects as the multiplier, not free-list fragmentation.

## Fix candidates (cause level, none landed)

1. Lookaside sized to a power of two on the small tier (SQLITE_CONFIG_LOOKASIDE before init, e.g.
   1200 x 27 = 32400 -> 32 KiB block): frees 32 KiB of memsys5. Measure lookaside miss cost (misses go to
   memsys5 rounded up, 1025-1200 -> 2048).
2. Page-size objects never in memsys5: size the page slot share from need, not a fixed 3/8, so the `:memory:`
   case and pager temp spaces stay in exact-size slots; or 2048-byte pages on the small tier
   (2048 + 272 -> 4096, still 1.8x).
3. Schema 116 KB: the largest fixed cost; review trigger count / FTS5 on `path` for the small tier.
4. Decide whether `:memory:` on the 2 MB tier is a product case at all: the whole database lives in the
   heap, so any fixed heap fails at some library size. The Amiga uses a file database, which passes at
   every root length measured. If `:memory:` is a test-only case, the test should not gate root length on it.

Open: which of these Johan accepts (4 is a scope decision). Test extension (120-char root in
tests/m3/scan_small_heap.sh) not written: with this songs set the 120 root passes on host (peak 332544 of
341088 usable, 8.5 KB margin), so it would not fail before a fix; a reliable RED needs root ~250 for :memory:.
