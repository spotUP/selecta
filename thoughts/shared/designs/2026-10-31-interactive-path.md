---
date: 2026-10-31
topic: Interactive path - library threads, query shape and budgets (I847)
tags: [library, sqlite, performance, search, threads, amiga, modland]
status: accepted
supersedes: I846 (library list query speed; folds into I847g)
related: I829 (UI posts library requests and returns), I845 (dirty rects + cover cache), I844 (online search pending state), I838 (stored item_formats, schema 15)
---

# Interactive path design (I847)

Goal: no user interaction ever waits on a bulk write, a full-table aggregate or a
sort over every hit. Selecta must feel at least as fast as HippoPlayer and bbmp on
a 600 000-song library, on Mac and on an Amiga 68060.

Inputs treated as given: I829 (UI thread posts library requests and returns; the
reply arrives as an event) and I845 (dirty rects, cover cache).

## 0. Why Selecta is slow today (verified in code, 2026-10-31)

| ID | Flaw | Evidence |
|----|------|----------|
| F1 | One library thread, one connection, rollback journal. A sync holds `call_lock` for the whole job and between its requests, so any UI read waits behind sync. SQLite is built `SQLITE_THREADSAFE=0` and `SQLITE_OMIT_WAL` on every platform: no second connection and no WAL are possible. | `src/library/db/worker.c:124,135`; `third_party/sqlite/CMakeLists.txt` |
| F2 | Sync commits every 32 rows (`SYNC_BATCH 32`, one `SEL_LIB_REQ_SCAN_BATCH` each, each a BEGIN/COMMIT). A full Modland list (516k rows) is about 16 000 commits, each with journal fsyncs and FTS trigger work. | `modland_sync.c`, `scan.c` |
| F3 | Search re-runs the whole FTS query and sorts every hit for each page (`SELECT key FROM (arm) WHERE key > :after ORDER BY key LIMIT :rows`), merges two arms, counts all remaining hits, and re-tokenizes every row for 4+ letter words (3-letter prefix trick, I198b). Cost is O(hits x pages); results are alphabetical, not ranked. | `src/library/db/search.c` (collect, count_rest) |
| F4 | Lists compute aggregates per reply: Library All runs `GROUP BY album, artist` and `GROUP BY artist` over all songs (CTE_SA, CTE_SR) with `COLLATE NOCASE` sorts; Home runs correlated history x songs subqueries per item; the Home chip filter uses `lower(COALESCE(format,'')) IN (...)`, which no index can serve. | `src/library/db/items.c` |
| F5 | Page cache about 12 MiB (memsys5, 3/8 of a 32 MiB heap), `SQLITE_MAX_MMAP_SIZE=0`, on a 726 MB `library.db`: nearly every cold query reads pages from disk. | `db.c:175`, `sel_library_heap_bytes`; Johan's db size |
| F6 | Amiga has no library thread at all: `SEL_LIBRARY_NO_THREAD` runs every request inline on the UI task. | `src/library/CMakeLists.txt:30` |

Measured symptoms: Library All 0.3-3.4 s; Sources was 2628 ms before I838 (0.8 ms after the stored aggregate, which proves the F4 fix pattern).

## 1. Architecture

### 1.1 Desktop (Mac, Linux, Windows)

Threads:

- **UI thread.** Never opens the database. Never waits on a reply (I829). Posts a request with a generation number and paints the reply when it arrives. Keeps showing the old content (or the pending state) until then.
- **Writer thread.** Owns the only read-write connection. Runs every write: sync, scan, likes, history, pins, migrations. Bulk writes run in time-bounded chunks (target 50 ms or 2000 rows, whichever comes first), commit, then yield. A **user-write lane** (like, history, pin, playlist edit) is checked between chunks and always runs before the next bulk chunk. After a chunk, a PASSIVE WAL checkpoint runs if the WAL is over 4 MB.
- **Interactive reader thread.** Own read-only connection. Serves only what the user waits for: search, list page, source page, Home, play lookup. Latest-wins per channel: a new search replaces a queued one, and a running stale query is stopped with `sqlite3_progress_handler` checking the channel generation (or `sqlite3_interrupt`).
- **Background reader thread.** Own read-only connection. Serves prefetch, counts, stats, cover lookups, anything the UI does not wait on. The existing job thread (`job.c`) becomes this reader's client, not a second writer.

Storage settings:

- `journal_mode=WAL`, `synchronous=NORMAL` (see D5). Readers see a consistent snapshot and never block the writer; the writer never blocks readers.
- SQLite built with `SQLITE_THREADSAFE=2` (multi-thread: each connection used by one thread at a time), WAL enabled, `SQLITE_MAX_MMAP_SIZE` raised; readers set `mmap_size` to 1 GiB so the OS page cache serves the 726 MB file.
- Allocator: on the big heap tier (more than 128 MB RAM) use the system malloc with `sqlite3_soft_heap_limit64`; memsys5 stays for the small tiers (D8). Each reader gets its own `cache_size` (interactive 32 MiB, background 16 MiB, writer 16 MiB).
- Caches stamped with `sqlite3_total_changes64` move to `PRAGMA data_version` plus stored generation counters per table group (songs, history, likes), because `total_changes` is per connection and readers never see the writer's count.

What the UI may wait on: nothing. What the UI may show while a reply is pending: the previous content, a pending state after 150 ms (I844 pattern), a spinner never earlier than 300 ms.

What the interactive reader may run: queries whose cost is O(page) or O(log n) per reply: index seeks, FTS first page, stored aggregates. No full scan, no `GROUP BY` over songs, no `count(*)` over all hits on the first reply.

### 1.2 Query shape (both platforms)

- **Search:** one FTS query per keystroke, first page only (200 rows), in FTS order (rank or rowid, D2), `LIMIT 201` to know "more". Further pages continue from the last rank/rowid, never re-sort all hits. The total count runs on the background reader afterwards and is capped ("2000+"), as bbmp does. Desktop matches substrings through an FTS5 trigram index (D1); a query under 3 characters falls back to the word-prefix index. Amiga keeps word prefix; the 3-letter re-tokenize trick stays only on the 512 KiB heap tier.
- **Lists (Library All, Artists, Albums):** stored aggregate tables (`artists`, `albums` with song counts and sort keys), maintained by the writer per chunk, same pattern as I838 `item_formats`. Pages read by `WHERE sort_key > :after ORDER BY sort_key LIMIT :rows` on an index.
- **Chips:** stored `format_family` column on songs with index `(source, format_family, sort_key)`. The chip filter becomes an index range, not `lower(...) IN (...)`.
- **Home:** a `home_shelves` snapshot table rebuilt by the background side after history or likes change (debounced 2 s). First paint reads the snapshot only.
- **Play:** song row by key (primary key seek) plus file resolution; no list query on the play path.

### 1.3 Amiga (68020 to 68060)

- The library runs as its own Exec process (`CreateNewProc`), priority one below the UI task, with the same request queues and reply events as desktop (D4). One connection only (no threads in SQLite: `THREADSAFE=0` stays), rollback journal or WAL with `locking_mode=EXCLUSIVE` later (WAL needs no shared memory then).
- The same chunked writes apply: a sync chunk is at most 30 ms of CPU (about 200 rows on a 060), then the process checks the user-write lane and the interactive queue before the next chunk. Interactive requests always run before the next bulk chunk.
- Search stays local on the 3-letter prefix index (512 KiB to 8 MiB heap tiers). Modland search can also go remote through the UHC search endpoint HippoPlayer uses (D3), which costs the Amiga no index work at all.

### 1.4 Budgets

Times are from the input event to the reply painted. p95 over 50 runs on the generated 600 000-song db (section 4). Amiga 060 numbers are measured on the rig (WinUAE 060 profile), Mac on Johan's class of machine (Apple Silicon).

| Interaction | Mac p95 | Amiga 060 p95 | Guarding perf test (bench key) |
|-------------|---------|---------------|--------------------------------|
| Keystroke echo in search field | 16 ms | 40 ms | `ui_echo_ms` (UI only, no library) |
| Search as you type, first page painted | 100 ms from last keystroke | 1000 ms local; 1500 ms remote Modland | `search_first_page_ms` for words a, ch, chip, purple motion, 4mat |
| List switch (All/Artists/Albums) or chip switch | 100 ms | 500 ms | `list_switch_ms`, `chip_switch_ms` |
| Open a source page (Modland, Local, Pins) | 50 ms | 300 ms | `source_page_ms` |
| Home first paint (cold app start, warm OS cache) | 50 ms after window shown | 300 ms | `home_first_paint_ms` |
| Play song, library part | 5 ms | 30 ms | `play_lookup_ms` |
| Play song, press to sound (cached file) | 150 ms | 500 ms | existing play path test plus `play_lookup_ms` |
| Any of the above while a full Modland sync runs | budget + 20 % | budget + 20 % | `*_during_sync_ms` |
| Like/history write visible while sync runs | 100 ms | 300 ms | `user_write_during_sync_ms` |
| UI frame time while sync runs | 16 ms | 40 ms | `frame_during_sync_ms` |
| Full Modland list sync (516k rows) | 60 s (measure first, then fix) | 15 min (measure first) | `sync_full_s` |

Background sync never shows a modal and never takes the interactive reader.

## 2. How HippoPlayer searches Modland

Verified from `HippoPlayer-68k/code/puu016.s` (around lines 58368-58470 and 59140-59175) and a live probe from the Mac on 2026-10-31:

- **Verified:** Search is a command, not as-you-type (CTRL+M). HippoPlayer writes a script to T: that adds the UHC tools path and runs `<type>search SEARCHLIMIT=10000 <terms>`, `Execute()`s it, and shows a modal "Searching..." box with the main window locked.
- **Verified:** The UHC tool (`uhcmirrorsearch`, then `UHCSearch`) asks a mirror (uhc.megaburken.net, uhc.driar.se, uhc.no0b.de, uhc.hex9d.com): `http://<mirror>/uhcsearch/?searchtype=modlandsearch&searchword=<words>&searchlimit=N`. The server searches; the Amiga does no indexing at all.
- **Verified:** The reply is a fixed-width text table (#, Filename, Path, Author, Fmt, Coll, Size). HippoPlayer imports it as a module list, filters it with a local Modland whitelist (`.modlandFilter`, prefixes and postfixes) and plays from `http://ftp.modland.com/pub/modules/`.
- **Verified (measured from Mac, mirror driar):** "chip" 0.39 s to first byte, 0.79 s total, 1.1 MB, 3986 rows; "purple" 0.46 s, 319 rows; "4mat" 0.39 s, 10 rows; "jogeir liljedahl" 0.39 s, 222 rows. No limit: 166 MB, 358 506 rows in 15 s.
- **Unverified:** server implementation and match semantics. "4mat" giving only 10 rows suggests a filename substring match, not an author match. Amiga-side times (TCP stack, 68k text parsing of 1 MB) not measured.

What Selecta must match: a first visible result for a Modland search in at most about 1 s on a 060 (HippoPlayer gives about 1 s plus a modal). Selecta beats it by being as-you-type and non-modal, and by searching artist and title words, not only file names.

## 3. HippoPlayer, bbmp and Selecta compared

bbmp source read from a shallow clone of `github.com/creep-ltx/bbmp` (main, 2026-10-31; clone deleted). Line numbers refer to that tree.

| Aspect | HippoPlayer | bbmp | Selecta today |
|--------|-------------|------|---------------|
| Data structure | Modland list lives on the server; local list is an in-memory module list (verified) | One read-only SQLite catalog per collection (`files` table), attached and unioned by TEMP views; user data (favourites, history) in a separate `library.db` (verified, `index.py:97-137, 625-640`, `library.py:24-33`) | One `library.db` (726 MB) with songs, FTS, history, likes, items in one file and one connection (verified) |
| Writes during use | None locally | Catalogs are never written while open: an update builds a temp DB and swaps the file; open Index keeps its snapshot (verified, `index.py:287-460`, `_take` stamp check `index.py:754-775`). User db: WAL + `synchronous=NORMAL` (verified, `library.py:32-33`) | Sync writes the same db in 32-row transactions, rollback journal, and holds `call_lock` (verified) |
| Index | Server side (unverified how) | FTS5 `fts(name, artist, album, format)`, unicode61, remove diacritics, no prefix index (verified, `index.py:135-139`); `files_browse(format, artist, hidden)`; stored `formats` counts table; other counts kept in a JSON file stamped by catalog name/time/size (verified, `index.py:700-712`) | FTS5 `prefix='2 3'` plus re-tokenize for 4+ letters; Library All and Home computed per reply (verified) |
| Threading | Search runs as an external command with a modal box; UI locked (verified) | Textual UI thread; each query in a `@work(thread=True, exclusive=True)` worker; pool of up to 3 read-only connections (`CONNECTIONS = 3`, `index.py:617`); stale replies dropped by comparing the current query (verified, `browse.py:898-912`) | One library worker; UI posts and waits (until I829); Amiga inline (verified) |
| Per-keystroke work | None (no as-you-type) | Debounce 0.3 s, only from 2 characters (verified, `browse.py:876-879`). One FTS query per catalog, `ORDER BY rank LIMIT 2000`, sorted by rank in Python; `count(*)` only when the list is full (verified, `index.py:1148-1175`, `browse.py:33,901-904`) | No debounce in the library; full FTS query + sort per page, count of all hits, two arms merged (verified) |
| Why it feels fast | Server does the work; one request | Reads never wait on writes (read-only files, snapshot swap), parallel read connections, first page only with a hard cap, rank order straight from FTS, stored counts (verified mechanisms; no timing measured here) | Opposite on each point |

Lessons taken into section 1: separate read connections that never wait on a writer (WAL replaces bbmp's file swap), latest-wins with stale-drop, first page with cap and lazy count, stored aggregates, debounce. Not taken: bbmp's per-collection files (one db with WAL gives the same read isolation and keeps cross-source queries simple), bbmp's 11-file ATTACH limit.

## 4. Perf fixture and tests

- `selecta-cli library gen --songs 600000 --out <path>` makes a deterministic db: 516 000 Modland-like rows (real format mix, artist/title word distribution sampled from a checked-in 5 000-line Modland path sample), 60 000 local rows, 24 000 other-source rows, 50 000 history rows, 2 000 likes, 200 pins, full FTS.
- `selecta-cli library bench --db <path> [--during-sync]` drives the engine commands (the same entry points the UI uses, not raw SQL), prints `<key>_p95=<ms>` lines and a call-count sentinel per path (proves the new query ran). `--during-sync` runs a full Modland list import on the writer at the same time.
- ctest label `perf` runs bench against the budgets in section 1.4 (Mac numbers; CI machines get a 2x factor). Amiga: same bench on the rig with a 516k Modland-only fixture or the full fixture (D7).
- `EXPLAIN QUERY PLAN` tests (from I846) assert no `SCAN songs` and no `USE TEMP B-TREE FOR ORDER BY` on every interactive query.
- I847a baseline (2026-11-01, Mac, Debug build of main at the merge, 600k fixture from `selecta-cli library gen --songs 600000 --out <db>`: gen 331 s, db 321 MB; `selecta-cli library bench --db <db> --runs 3`, p95 ms, warm): search_first_page_a 5.1; search_first_page_ch 486.7 (7373 rows, no 200-row cap on the FTS path); search_first_page_chip 306.8 (7209 rows); search_first_page_purple_motion 36.6; search_first_page_4mat 237.6; chip_switch 912.5; list_switch_artists 641.8; list_switch_albums 552.7; source_page 116.5. home_first_paint did not finish within 10 minutes at 600k (6.9 s at 20k songs); play_lookup not measured at 600k (it runs after home_first_paint). Not implemented yet: `--during-sync` and the sync, quit and writer-wait keys (later rows). Manual 600k run: the two commands above. ctest `library-gen-bench` (label m3) runs gen at 5 000 songs plus bench once and requires every measure line.

## 5. Build rows

Each row is one sonnet agent, builds and runs its own tests (affected subset only), and ends with its bench line recorded in the ledger. Order matters.

| ID | Row | Test that proves it |
|----|-----|---------------------|
| I847a | Fixture generator and bench command (section 4); record today's baseline numbers for every key | bench prints all keys on the 600k db; baseline written to ledger |
| I847b | SQLite build per platform: desktop `THREADSAFE=2`, WAL on, mmap allowed; Amiga flags unchanged. Allocator by tier (system malloc + soft heap limit on big tier) | unit test opens two connections on two threads, WAL mode reported; Amiga build size unchanged within 2 % |
| I847c | Writer/reader split: one writer, interactive reader, background reader; every `SEL_LIB_REQ_*` (21) tagged read or write and routed; caches keyed on `data_version` + generations instead of `total_changes64` | reachability: engine search command runs on the interactive reader (sentinel), like runs on the writer; cache invalidates after a writer change |
| I847d | Chunked bulk writes (50 ms / 2000 rows), `call_lock` no longer held across a sync, user-write lane, PASSIVE checkpoints, `synchronous=NORMAL` | `user_write_during_sync_ms` and `frame_during_sync_ms` within budget; `sync_full_s` recorded |
| I847e | Latest-wins channels with generation, progress-handler cancel of stale searches, 150 ms pending state hook (I844 uses it) | typing 5 keystrokes 30 ms apart runs at most 2 full queries (call count); stale reply never painted |
| I847f | Search query shape: first page 200 in FTS order, cursor paging, lazy capped count on background reader, full prefix on desktop | `search_first_page_ms` within budget for all five words; EXPLAIN tests |
| I847g | Stored list aggregates (artists, albums) maintained by writer per chunk; Library All/Artists/Albums read pages by index (I846 folds in) | `list_switch_ms` within budget; EXPLAIN shows no GROUP BY over songs |
| I847h | `format_family` column + index for chips; `home_shelves` snapshot table | `chip_switch_ms`, `home_first_paint_ms` within budget |
| I847i | Cache and mmap sizing per reader, warm-up of FTS and aggregate pages at start on the background reader | cold-start `home_first_paint_ms` and first search within budget |
| I847j | Contention reachability test: drive search, list switch, source page, play and like through engine commands while a full sync runs | all `*_during_sync_ms` within budget + 20 % |
| I847k | Amiga library as its own Exec process with the same queues; chunked writes 30 ms | rig: UI stays responsive (frame log) during a Modland sync; bench on rig |
| I847l | Amiga Modland remote search through UHC endpoint with local fallback (D3 accepted) | rig: "purple" shows rows within 1500 ms; offline falls back to local |

I844 (Modules.pl pending state) stays its own row and uses the I847e pending hook.

## 6. Decisions (answered by Johan 2026-10-31)

Answers: D1 substring on Mac (FTS5 trigram; under 3 characters falls back to word prefix), word prefix on Amiga. D2 to D8 as recommended below.

| ID | Question | Recommended | Trade-off |
|----|----------|-------------|-----------|
| D1 | Search matching | Word prefix (FTS5, as today and as bbmp) | Substring/trigram finds "mat" inside "4mat" like UHC, but makes the FTS index about 3x larger and slower on Amiga |
| D2 | Order of the first page | Relevance (FTS rank), alphabetical by click | Alphabetical first page forces a sort of every hit: the F3 cost returns for common words |
| D3 | Modland search on Amiga | Remote UHC search with local fallback | Fast and no index work, but depends on third-party mirrors; local-only is slower on 020/030; own endpoint is more control and more upkeep |
| D4 | Amiga library execution | Own Exec process | Inline is simpler but every sync and slow query freezes the UI |
| D5 | Durability | `synchronous=NORMAL` with WAL | Power loss can drop the last few commits (sync redoes them; a like may be lost); FULL costs an fsync per chunk |
| D6 | When perf tests run | Per library row and before push | Every CI run catches more but costs minutes per run and flakes on shared runners |
| D7 | Amiga perf fixture | Modland-only 516k | Full 600k mirrors Mac but takes long to build on the rig |
| D8 | Desktop allocator | System malloc + soft heap limit | memsys5 with a mutex keeps one code path but caps cache at heap size and adds lock contention |
