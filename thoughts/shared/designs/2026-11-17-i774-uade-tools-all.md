---
date: 2026-11-17
row: I774i
covers: I774, I774b, I774c, I774d, I774e, I774f, I774g, I774h
status: decided
---

# I774: every audacious-uade-tools data file in Selecta

Johan 2026-11-17: "all 25". Today Selecta uses one file: `tsv/pretty/xxh32/songlengths.tsv`, baked by
`tools/songlen_bake.c` (entry point `sel_songlen_bake` in `src/library/songlen_bake.c`) into `songlengths.bin` (I605,
7.8 MB, format in `src/library/songlen.h`). Authors come over the network from `/api/songdb/lookup`
(`src/library/db/songdb.c`, cached in `songdb_cache(key, authors, fetched)`), which drops publisher, album and year.

Checkout read: `~/Code/HippoPlayer-68k/code/audacious-uade-tools` (commit 013a8a9). Only head, grep and awk counts
were run on it; nothing from it was executed.

## 0. Corrections to the I774a gap report

- **There is no strings data set upstream.** The README lists only `songdb/sources/*/*.tsv`,
  `songdb/sources/metadata/demozoo_*.tsv` and `songdb/sources/audio/*.tsv`; the only GitHub release is `audio`
  (16 files). No script reads `.strings`. I774h therefore cannot import anything; it becomes a Selecta-side feature
  (section 5.4, decision D3).
- **The source rows carry both hashes.** Every first-subsong row in `songdb/sources/{site,collection,group,bbs,cd}`
  has the full 32-hex md5, the file size, the XXH32 of the first 256 KB and the CRC32. Upstream's own
  `md5ToXxh32` (`songdb/scripts/xxh32.sc:9`) builds the md5 to xxh32 map from exactly these columns. This settles the
  key question (section 2).
- **Fingerprints are 1.8 GB compressed** (16 x ~112 MB `.tsv.zst`, GitHub release `audio`), not a "large file".
- **Demozoo is already on hippoplayer.se.** `/api/demozoo/lookup` (hippoplayer.se
  `thoughts/shared/api/demozoo-lookup-v1.md`) bakes the full nightly Demozoo dump. The four `demozoo_*.tsv` extracts
  come from the same dump; their only new value is the file-hash join (section 1, row 22).

## 1. Per file: contents and landing place

Row counts from `wc -l` / awk on the checkout. "Entry" means the position of a key in the sorted key table of
`songlengths.bin` (476,328 xxh32 keys; modinfos has the same 476,328 keys).

| # | File | Holds (key, columns, rows, disk) | Lands in |
|---|---|---|---|
| 1 | tsv/pretty/xxh32/songlengths.tsv | xxh32 key, min subsong, per subsong `ms,songend[,!]`; 476,328 rows, 521,602 lengths; 11.9 MB | bake `songlengths.bin` (exists). v2 keeps the songend type (p/s/l/v) and the `!` duplicate-subsong flag, dropped today |
| 2 | tsv/pretty/xxh32/modinfos.tsv | key, format name (1,217 distinct), channels (193,814 rows set); 476,328 rows; 15.0 MB | bake `uadeinfo.bin`, then `songs.format` / `songs.channels` cache columns |
| 3 | tsv/pretty/xxh32/metadata.tsv | key, authors `~`-joined (26,780 distinct strings), publishers (10,028), album (27,663), year (43 values); 386,700 rows; 14.7 MB. Upstream distils all sources; conflicts picked by `metasToXxh32` (year > album > publisher > author score) | bake `uadeinfo.bin`; feeds `song_composers` and the songs cache columns; the network lookup becomes fallback |
| 4 | tsv/pretty/md5/songlengths.tsv | md5 twin of 1; 476,919 rows; 11.9 MB | bake-time cross-check only (oracle: re-keyed md5 rows must equal the xxh32 rows except upstream's 29 whitelisted xxh32 collisions) |
| 5 | tsv/pretty/md5/modinfos.tsv | md5 twin of 2; 15.0 MB | bake-time cross-check only |
| 6 | tsv/pretty/md5/metadata.tsv | md5 twin of 3; 387,253 rows; 14.8 MB | bake-time cross-check only (the 553 extra md5 rows collapse onto shared xxh32 keys) |
| 7 | md5/amp.tsv | md5, author (AMP); 176,454 rows; 4.4 MB | bake `uadecredits.bin`, source "AMP" |
| 8 | md5/modland.tsv | md5, author from Modland path; 177,185 rows; 4.4 MB | `uadecredits.bin`, source "Modland"; also an agreement check against Selecta's own Modland index (no second author row shown when equal) |
| 9 | md5/demozoo.tsv | md5, author, publisher, prod, year; 77,681 rows; 3.4 MB | `uadecredits.bin`, source "Demozoo" |
| 10 | md5/kestra.tsv | same columns, Kestra/Bitworld; 41,327 rows; 2.1 MB | `uadecredits.bin` |
| 11 | md5/unexotica.tsv | same, game house + game title; 8,154 rows; 0.5 MB | `uadecredits.bin` |
| 12 | md5/oldexotica.tsv | same; 3,269 rows; 0.1 MB | `uadecredits.bin` |
| 13 | md5/modsanthology.tsv | same; 12,153 rows; 0.4 MB | `uadecredits.bin` |
| 14 | md5/fujiology.tsv | same; 10,911 rows; 0.4 MB | `uadecredits.bin` |
| 15 | md5/wantedteam.tsv | same; 5,883 rows; 0.2 MB | `uadecredits.bin` |
| 16 | tsv/encoded/xxh32/{songlengths,modinfos,metadata,xxh32idx}.tsv | near-binary forms of 1-3 plus a base64 b24 index; 9.8 MB together; built for whole-file in-memory init in audacious-uade | not shipped (section 3); used as a size reference only |
| 17 | songdb/sources/site/*.tsv (51) | per subsong: md5(32), subsong, ms, songend, player, format, channels, size, xxh32(256 KB), crc32, path in the archive; 1,435,205 lines; 198 MB | client: source ids per entry in `uadeprov.bin`, and the md5 bridge `uademd5.bin`. Paths: server only |
| 18 | songdb/sources/collection/*.tsv (36) | same; 245,482 lines; 31 MB | same as 17 |
| 19 | songdb/sources/group/*.tsv (6) | same; 6,762 lines; 1.1 MB | same as 17 |
| 20 | songdb/sources/bbs/*.tsv (9) | same; 17,638 lines; 2.3 MB | same as 17 |
| 21 | songdb/sources/cd/*.tsv (293) | same, one file per CD-ROM; 286,751 lines; 35 MB | same as 17 |
| 22 | songdb/sources/metadata/demozoo_{authors,music,prods,soundtracks}.tsv | SQL extracts of the Demozoo dump: 115,091 authors (nicks, country), 229,163 music rows (prod, release date, platform, source type ModlandFile / KestraBitworldRelease / YoutubeVideo, path, authors, groups, tags), 204,822 prod rows, 57,572 soundtracks; 89 MB | server only. hippoplayer.se already has the full dump nightly; add the hash join (file md5 or xxh32 -> Modland path via `site/modland.tsv` -> Demozoo music id) so `/api/demozoo/lookup?key=` answers local files with no Modland path. The extracts serve as a cross-check of the server's own join |
| 23 | songdb/sources/audio/*.tsv (release `audio`) | per md5 + player + subsong: audio bytes, audio md5, chromaprint (base64), from which upstream builds simhash tags and union-find groups (`songdb/scripts/sources/audio.sc`); 1.8 GB zst | server only for raw data; server bakes duplicate/remaster groups into a small client file `uadegroups.bin` (size unmeasured: the release was not downloaded) |
| 24 | "strings" | does not exist upstream (section 0) | Selecta-side extraction, decision D3 |
| 25 | misc/{modland,uade,scrape,precalc}, songdb/*.sc, songdb/scripts/** | shell and Scala build scripts, no data: Modland symlink farms (`modland_amiga_dirs.txt` = 215 Amiga format dirs), `precalc.sh` (runs the audacious-uade precalc binary to make source rows), `extractor.sh` (unpacks scraped archives), `contentdb.sh` (uade content db), `songdb.sc` (the whole pipeline), `audio_match.sc` (Shazam-like matcher) | nothing ships. Reference logic ported into the bake in C: `md5ToXxh32`, `metasToXxh32` conflict pick, `xxh32DupWhiteList`, audio union-find (server). `modland_amiga_dirs.txt` is a useful list for the Amiga format filter |

## 2. Key choice: map md5 to xxh32 once at bake, plus a bridge file

Options measured against what Selecta already has:

- **Compute md5 at scan.** The local scan only stats files (`src/library/db/duplicates.c:5`). md5 already runs in the
  duplicates idle pass, one file per tick; on a 68020 it is "tens of KB/s" and one large file blocks a tick for
  seconds (`thoughts/shared/reports/m3-I184a-report.md:22`). Estimate from the algorithm: ~4,500 cycles per 64-byte
  block on a 14 MHz 68020, about 200 KB/s before I/O; with FFS reads, tens of KB/s matches the report. A 100 KB module
  costs 1-5 s, a 10,000-file library 3-14 hours. Too slow as a scan step. Rejected as the primary key.
- **Map md5 to xxh32 at bake.** The source rows hold md5, size and xxh32 together (section 0), so the bake re-keys
  every md5-keyed file to the xxh32 entry exactly as upstream does for metadata. Zero runtime cost. Selecta's lookup key
  stays `sel_uade_key` (`src/library/xxh32.c`), which reads at most 256 KB.
- **Both (recommended).** Re-key at bake for everything, and also ship `uademd5.bin` (md5 prefix -> entry). It costs
  4.3 MB and pays off where Selecta knows an md5 but has no file bytes: Modland catalogue rows (from `allmods_md5`,
  `files.md5` / `songs.md5`) and local files already hashed by the duplicates pass. Those get format, channels,
  credits and provenance before any download, which is what makes the library filters (section 5.2) work on remote
  rows.

Local files that have neither an md5 nor a played key yet: compute the UADE key in the existing duplicates idle pass
(same bytes, first 256 KB, xxh32 is far cheaper than md5) and store it in a new `songs.uade_key` column (decision D6).

## 3. One bake tool and the file formats

Extend the existing tool: `tools/songlen_bake.c` becomes `uadedb_bake <checkout dir> <out dir>` (old two-argument
form kept for the I605 CMake target and tests). Library-side code goes in `src/library/uadedb_bake.c` next to
`songlen_bake.c`, sharing `sel_songlen_key_bytes`. One tool because every output depends on the same sorted key
table (entry indices) and the same md5 bridge.

Shipping format: Selecta's own seekable big-endian files, not `tsv/encoded`. Reasons: encoded is designed to be read
whole into RAM (9.8 MB, over the 16 MB Amiga baseline once the app is loaded), its decoder exists only as Scala
(`songdb/scripts/convert.sc`), and the author reserves the right to change it (README:100). Our files are read by
seeks, a few hundred bytes of RAM per lookup, like `songlengths.bin` today.

Files, each with a 16-byte header (magic, version, count, a u32 bake id shared by all files of one bake so a mixed
set is rejected):

| File | Layout | Size (estimate from the counts above) |
|---|---|---|
| `songlengths.bin` v2 (SLDB) | unchanged entries (6 key, first, count, u32 length index); lengths u32 with the top 4 bits = songend type (3) + duplicate flag (1); ms fits in 28 bits (74 h). v1 files still read (version check, no flag bits) | 7.8 MB (same) |
| `uadeinfo.bin` (UINF) | indexed by entry, 12 bytes: u16 format id, u8 channels, u24 authors, u24 publishers, u24 album, u8 year-1900; then a string pool (offset table + UTF-8 strings) | 476,328 x 12 = 5.7 MB + pool ~1.1 MB = **6.8 MB** |
| `uadecredits.bin` (UCRD) | per entry u32 start offset, then rows sorted by entry: u8 source, u24 authors, u24 publishers, u24 album, u8 year; own pool. Rows equal to the distilled metadata row are dropped at bake | ~512k rows x 11 B + 1.9 MB offsets + pool ~1 MB = **8.5 MB** upper bound |
| `uadeprov.bin` (UPRV) | per entry u32 start offset, then u16 source ids; source table (id, kind site/collection/group/bbs/cd, file name, display name from `sources.md`) | 1,679,123 pairs x 2 B + 1.9 MB = **5.3 MB** |
| `uademd5.bin` (UMD5) | sorted 6-byte md5 prefix + u24 entry | 476,919 x 9 = **4.3 MB** |
| `uadegroups.bin` (UGRP), server-baked | per entry u24 group id (0 = none) for entries in a group, sorted by entry; group = upstream audio union-find | **unmeasured**, ~1 MB guess |

Lookup cost: one binary search in `songlengths.bin` (~19 seeks for 476k keys), then one or two direct seeks per
other file. A song info open is ~25 seeks.

## 4. Delivery and sizes per platform

All client files together: 7.8 + 6.8 + 8.5 + 5.3 + 4.3 = **~33 MB on disk** (+ groups, unmeasured). Compressed
download estimate 12-15 MB (not measured).

| Platform | What it gets | Disk | RAM |
|---|---|---|---|
| Mac (desktop UI) | all six files in `<config dir>`, bundled in the app or downloaded on first run (D1) | ~33 MB | < 4 KB per lookup, files opened per use |
| Text UI (Mac, and Amiga in UP-Term) | same files as its platform; the text UI is a renderer, not a separate data set | as its platform | as its platform |
| Amiga 68020+ | the same files, optional, each enabled when present and its header checks pass; installed in `PROGDIR:Selecta` or `ENVARC:Selecta` like `songlengths.bin` | up to ~33 MB; any subset works (songlengths + uadeinfo alone = 14.6 MB) | < 4 KB; no file is ever loaded whole, so the 16 MB baseline holds. A file that fails (missing, bad bake id, disk full at install) disables only its feature |
| hippoplayer.se (server) | raw checkout, file paths (88 MB raw, 1.06M distinct basenames), Demozoo join, 1.8 GB fingerprints; bakes and serves the client files | ~2.3 GB | n/a |

Recommended (D1, D5): hippoplayer.se bakes when upstream changes and serves the files as static downloads with a
manifest (version, bake id, size, md5 per file) next to `/sysfiles/`. Selecta downloads through `src/library/cache.c`
(it already checks md5 and retries mirrors). The Mac app also bundles the current bake so a fresh install works
offline. Amiga gets the pack from the release archive or downloads it through the plain-http proxy (I1228), with the
size shown before it starts. `/api/songdb/lookup` stays as the fallback for keys newer than the bake.

## 5. UI per track

The data lands in shared code (`src/library` readers, `src/uimodel` lines and query terms); the three tracks only
render it (RULES 0b, single source of truth).

### 5.1 Song info: credits and provenance

Shared model: new song info sections built in `src/uimodel` (beside the existing song info lines in
`pages.c`/`panels.c`):
- **Credits**: Authors, Group / publisher, Release (album or game), Year, Format, Channels. Origin of each value shown
  on demand ("from UnExoticA").
- **Other sources**: one line per `uadecredits` row that differs ("AMP: Major Tom", "UnExoticA: Titus the Fox, Titus,
  1992").
- **Found in**: provenance by kind with counts ("Sites 4, Collections 2, CD-ROMs 7"), expandable to names. File
  names inside each archive come from the server on an explicit "Show file names" action (row R14).
- **Same tune**: group members from `uadegroups.bin` (other files, other formats, remasters), each playable.
- **Demozoo**: production, platform, release date, tags from the existing `/api/demozoo/lookup`, now also by hash.

| Track | Rendering |
|---|---|
| Mac desktop | sections in the song info panel, Spotify-style (memory: Spotify reference design), names as links into library filters |
| Text UI | the same sections as text pages in the song info view; expandable lists as sub-pages |
| Amiga MUI | song info window with one group per section, MUI 3.8 listviews for Other sources / Found in / Same tune (Opus writes the MUI class addendum, sonnet builds, per memory) |

### 5.2 Format and channel filters

Query terms in the library query compiler (`selecta-cli library query --explain`): `format:"Protracker"`,
`channels:>4`, `publisher:`, `release:`, `found:aminet` / `found:cd`, `sametune:`. Values come from cache columns on
`songs` (`format`, `year`, `album` exist; add `channels`, `publisher`, `uade_key`), filled from the bake files when
empty (tag precedence: D2). Mac: format and channel facets in the library filter bar. Text: a filter menu that writes
the same query terms. MUI: a format popup list and a channels cycle gadget beside the search string gadget.

### 5.3 Duplicate and remaster detection

`src/library/db/duplicates.c` groups by md5 today. Add a second signal: same `uadegroups` group id = "same tune,
different file". Shown as the Same tune section and as a "Hide other versions" library option; the existing best-
source pick (D-M3-14) applies within a group. Subsong `!` flags from `songlengths.bin` v2 hide duplicate subsongs in
the subsong list on all three tracks.

### 5.4 Full-text strings search

No upstream data (section 0). Selecta-side: when a local or cached file is opened or hashed, the replayer's sample
and instrument names and song message go into an FTS5 table `song_strings(key, text)`; query term `text:"cracked
by"`. Size estimate ~700 B per module (31 x 22-char sample names + message), 7 MB per 10,000 modules in library.db.
Unverified: whether the replayer plugin API exposes sample names (`git grep` in `src` found none), and whether the
Amiga SQLite build has FTS5 (`SEL_LIB_STAT_FTS5` reports it; not checked on a rig). Decision D3.

## 6. Build rows

Each row is one sonnet agent unless marked. Every row that builds goes through `buildq` and needs Johan's build go
(memory: ask before every build). Amiga rig check once at the end (R17), per "rig test at end".

| Row | Work | Acceptance test |
|---|---|---|
| R1 | `uadedb_bake` skeleton: extend `tools/songlen_bake.c` to take the checkout dir; songlengths.bin v2 with songend type + dup flag; reader masks flags, still reads v1 | existing `tests/engine/songlen_engine_test.c` passes; new host test on a 20-row fixture reads ms, type and dup flag; v1 file still answers |
| R2 | `uadeinfo.bin` from modinfos + metadata (xxh32), string pool, reader `sel_uadeinfo_get(entry)`; md5 twins (files 4-6) re-keyed as a cross-check | fixture test returns format/channels/authors/publisher/album/year; bake prints `info_entries= pool_bytes= md5_mismatch=` with mismatch 0 outside the 29-key whitelist on the real checkout |
| R3 | md5 bridge `uademd5.bin` from source rows (md5ToXxh32 port), reader md5 -> entry | fixture: an `aminet.tsv` md5 maps to its xxh32 entry; bake prints `md5_mapped= md5_unmapped=` |
| R4 | `uadecredits.bin` from the nine md5 files via the bridge, rows equal to metadata dropped | fixture with amp + unexotica rows; reader lists per-source rows; bake prints per-source kept/dropped/unmapped |
| R5 | `uadeprov.bin`: source ids per entry, source table with kind and display names from `sources.md` | fixture: one md5 in two site files and one cd file gives 3 ids with right kinds |
| R6 | library wiring: `songs.uade_key`, `songs.channels`, `songs.publisher` (schema migration, template bake), key filled in the duplicates idle pass; fill cache columns and `song_composers` from the bake; `songdb.c` keeps publisher/album/year from the endpoint and calls it only for keys not in the bake | library test with fixture bins: a local song gets format/channels/authors with no network; a key missing from the bake still reaches the endpoint (mock) |
| R7 | query terms `format: channels: publisher: release: found: sametune: text:` in the query compiler | `selecta-cli library query --explain` prints the expected `sql=` for each; library test returns the fixture songs |
| R8 | uimodel song info sections (Credits, Other sources, Found in, Same tune, Demozoo) | uimodel unit test on the produced lines for a fixture song |
| R9 | Mac desktop rendering of R8 + filter bar facets | scripted live clicks on a real-db copy (memory: I run Mac UI checks); dui-ratchet + goldens before push |
| R10 | text UI rendering of R8 + filter menu | headless `--size` run prints the sections; text page stats count the new actions |
| R11a (Opus) | MUI class addendum for the song info window groups and filter gadgets | design section reviewed by Johan |
| R11b | MUI build from R11a | `build/amiga` builds; rig check in R17 |
| R12 | hippoplayer.se: bake job + static files + manifest; Selecta downloader via `cache.c` with md5 check; Mac bundles the bake; Amiga opt-in with size shown | manifest served; a host test downloads from a local stub, rejects a wrong md5, keeps the old set |
| R13 | hippoplayer.se: fingerprint groups bake (port the audio.sc union-find, server side) -> `uadegroups.bin`; duplicates.c reads it | server tool prints `groups= members=`; Selecta test: two fixture keys in one group show as Same tune |
| R14 | hippoplayer.se: `/api/songdb/files?hash=` from the source rows (paths per source), client "Show file names" action | endpoint returns the 3 paths for the R5 fixture md5; client test against a stub |
| R15 | hippoplayer.se: Demozoo hash join (xxh32/md5 -> Modland path from `site/modland.tsv` -> music id) inside `/api/demozoo/lookup`; extracts used to cross-check | a local file with no Modland path gets its Demozoo prod; cross-check count printed |
| R16 | strings (only if D3 = build): replayer API for sample names/message, `song_strings` FTS5, `text:` term wired from R7 | fixture mod's sample name found by `text:`; Amiga falls back to LIKE when FTS5 is absent |
| R17 | Amiga batch: install the pack in `PROGDIR:Selecta`, measure seeks and ms per song info open, RAM delta, MUI R11b, each file removed in turn disables only its feature | rig report with numbers; Johan's eyes on the MUI window |

Order: R1-R5 (bake, can run as two agents: R1-R3 then R4-R5), R6, R7, R8, then R9/R10/R11 in lockstep (three
tracks), R12-R15 server rows beside them, R16 after D3, R17 last.

## 7. Decisions for Johan

| # | Decision | Recommended | Trade-off |
|---|---|---|---|
| D1 | Delivery | Mac bundles the bake and updates by download; Amiga gets an optional pack (release archive or download) | bundling adds ~33 MB to the Mac app; download-only makes a fresh offline install show no credits |
| D2 | Precedence when file tags or catalogue fields disagree with the bake | tags and catalogue win, the bake fills blanks; differences visible under Other sources | bake-wins gives cleaner names for badly tagged mods but overwrites what the user's files say |
| D3 | Strings search (I774h): upstream has no data | build Selecta-side extraction for local and cached files (R16) | real work in the replayer API across many plugins; dropping it leaves I774h closed as "no data upstream" |
| D4 | License: the data is GPL-2.0-or-later (COPYING) | ship the bake as a separate data pack, like the player pack (I1076), not inside the MIT-only release | one more download for MIT-only users; bundling inside the MIT app mixes licences |
| D5 | Where the bake runs | hippoplayer.se bakes on upstream change and serves versioned files | needs a server job; the I605 pattern (bake on each build machine) keeps every dev machine on its own checkout and Amiga users can't bake at all |
| D6 | Local files never played: how they get a key | add the UADE key to the existing duplicates idle pass (same file read) | filters cover the whole local library after the pass; on a 68020 the pass already takes hours for big libraries, key-on-play only covers played songs |
| D7 | Fingerprints | server bakes groups, clients get `uadegroups.bin` | small file, works offline, but groups only change with a new bake; an endpoint-only design needs network for Same tune |

Unverified in this design: fingerprint and groups sizes (release not downloaded), compressed pack size, FTS5 on the
Amiga SQLite build, replayer API access to sample names, the 68020 md5 rate beyond the I184a report's figure.

## 8. Johan's answers (2026-11-17)

| # | Answer |
|---|---|
| D1 | Bundle everywhere: every platform's install carries all data files, Amiga included (not an opt-in pack). R12 and R17 change: Amiga install copies the whole set into `PROGDIR:Selecta`; the downloader only updates. Cost accepted: ~33 MB on the Amiga install. |
| D2 | Tags and catalogue win; the bake fills blanks. |
| D3 | Build Selecta-side strings extraction (R16 is in scope). |
| D4 | Separate GPL data pack, like the player pack. Read together with D1: the data pack is always shipped beside the MIT app on every platform, the same way the player pack is (I1075/I1076), so licences stay split while every install has the data. |
| D5 | hippoplayer.se bakes and serves versioned files. |
| D6 | UADE key filled in the existing duplicates idle pass. |
| D7 | Server bakes `uadegroups.bin`; clients use it offline. |
