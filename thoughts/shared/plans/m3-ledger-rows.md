## Implementation M3 (plan: thoughts/shared/plans/2026-10-25-m3-library.md, status: draft, not started)

Tests T62 to T97 are defined in the plan's finish-line table. T8 ratchet target after M3: 8.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I139 | [ ] | 0 | Read DEViLBOX server routes (auth, ratings, hip vote/votes, genre, songdb, loudness, lyrics, comments, mirror, playlists, public URL layout); record shape or absence with file:line in todos "Facts gathered" (D-M3-13) | Facts section lists every endpoint the plan uses |
| I140 | [ ] | 0 | Record one live response per external layout by hand (Modland allmods_md5.zip header and changelog, HVSC Songlengths.md5 and STIL, hippoplayer.se list and one `.prg`) into `tests/net/fixtures/` subsets with source URL and date | fixture files committed with provenance header |
| I141 | [ ] | 0 | `tests/net/fixture_server.py` (HTTP, chunked, Range, redirect, delay, TLS test CA, passive FTP), request log, ctest fixture and lock, guard and map env contract (D-M3-6) | fixture server self-test passes; T67 red |
| I142 | [ ] | 0 | T62 to T95 red on desktop with fixtures, label `m3` | ctest -L m3 lists them, all fail with missing-feature reasons |
| I143 | [ ] | 0 | T96 red under vamos, T97 red on the rig (label `rig`, visible skip) | vamos test fails; rig test fails or skips visibly |
| I144 | [ ] | 1 | Vendor SQLite amalgamation, `selecta_sqlite` target, options per D-M3-1, desktop and 68k | both trees build; `library stats` prints `sqlite_version=` |
| I145 | [ ] | 1 | Library worker (thread / subtask), request and result SPSC queues, memsys5 heap at start (D-M3-2) | T62 heap and no-alloc parts green |
| I146 | [ ] | 1 | Schema v1, `user_version` migrations, FTS5 table, account handle on every entry point | T62 green |
| I147 | [ ] | 2 | `src/library/query.c` parser for the full section 9.2 syntax, unknown key and view cycle errors | T63 parse rows green |
| I148 | [ ] | 2 | SQL compiler with bound parameters, durations, sizes, relative time, `in:`, `view:` | T63 green |
| I149 | [ ] | 2 | LIBRARY_SEARCH through the worker, LIBRARY_RESULT row tags in `tags.def` | T64 green; T8 count moves |
| I150 | [ ] | 2 | PLAY_QUERY, QUEUE_ADD_QUERY, VIEW_SAVE (smart playlists) | T65 green; T8 count moves |
| I151 | [ ] | 2 | View model source = library query (VIEW_SOURCE), style switch keeps the key | T66 green |
| I152 | [ ] | 3 | `src/core/{posix,amiga}/net.c` socket table, guard and host map | T67 green |
| I153 | [ ] | 3 | `src/library/net/http.c` HTTP/1.1: GET/PUT/POST/DELETE, chunked, Range, redirects, 3 s / 30 s timeouts | T68 plain parts green |
| I154 | [ ] | 3 | Vendor mbedTLS, TLS stream wrapper, OS CA store, `SELECTA_CA_FILE` (D-M3-4) | T68 green |
| I155 | [ ] | 3 | Passive FTP client (D-M3-5) | T70 green |
| I156 | [ ] | 3 | Mirrors: first-byte ranking, retry next, home mirror first, Amiga route to `/api/mirror/<source>/<path>` | T69 green |
| I157 | [ ] | 4 | `src/library/source.h` (`sel_source`), source registry, song key helpers, Modland format table, `SEL_SRC_*` tags | key round trip in T72; tag lint green |
| I158 | [ ] | 4 | `local` source over `sel_browse`: scan into files/songs with probe metadata, LIBRARY_SCAN | T71 scan part green |
| I159 | [ ] | 4 | Archive adapters (D-M3-7), archives browse as folders; xad/xfd on Amiga | T71 archive part green |
| I160 | [ ] | 4 | `sel_source_rescan(paths)`, folder watcher in `src/core/<os>/watch.c`, Amiga timed rescan | T71 green |
| I161 | [ ] | 4 | `modland` source: allmods_md5 listing, path parse, `url`/`path_in`, changelog, LIBRARY_SYNC | T72 green |
| I162 | [ ] | 4 | `hvsc` source: listing, Songlengths.md5, STIL, hosted-by Modland | T73 green |
| I163 | [ ] | 4 | `hippo` source: JSON list, legacy TSV list, `.prg` fetch through the hippo codec | T74 list and fetch parts green |
| I164 | [ ] | 4 | PLAYLIST_PUBLISH: hippo export, 512 KB check, POST, reply parse | T74 green; T8 count moves |
| I165 | [ ] | 5 | File cache paths, md5 verify with next-mirror retry, LIBRARY_FETCH | T75 fetch part green; T8 count moves |
| I166 | [ ] | 5 | `SEL_RP_COMPANIONS` tag, per-format rules on the replayers that need them, fetched with the song | T75 green |
| I167 | [ ] | 5 | LRU trim to a `sel_machine`-scaled budget, kept and liked exempt | T76 green |
| I168 | [ ] | 5 | Prefetch of the next queued remote song while one plays | T77 prefetch part green |
| I169 | [ ] | 5 | Smart downloads (F34): history and smart-playlist policy, low priority, metered and off prefs | T77 green |
| I170 | [ ] | 6 | History, play and skip counts on SONG_OPENED/SONG_ENDED; LIKE, KEEP; `rp.config` per-song override moves into the DB | T78 history part green; T61 still green |
| I171 | [ ] | 6 | FLAG wrong/skip, `selecta-cli wrong`, `sounds-wrong.txt` export, skip honoured by SHUFFLE and RADIO | T78 flags part green |
| I172 | [ ] | 6 | NOTE and tag edits in the DB, optional DEViLBOX comment push | T78 green |
| I173 | [ ] | 6 | RATE, DEViLBOX login and token store (D-M3-12), PUT/DELETE/batch, pending push queue | T79 green; T8 count moves |
| I174 | [ ] | 6 | VOTE with parameters from I139 | T80 green; T8 count moves |
| I175 | [ ] | 6 | CUE_SET, CUE_GOTO over the `cues` table | T81 green; T8 count moves |
| I176 | [ ] | 6 | STICKER_SET/GET/DEL/LIST (D-M3-17) | T82 green; T8 count moves |
| I177 | [ ] | 6 | Engine emits SCROBBLE_DUE; `plugins/integrations/lastfm` scrobbler, offline queue (D-M3-15) | T95 green |
| I178 | [ ] | 7 | Playlists and folder tree in the DB, FOLDER_ADD/MOVE/DEL, PLAYLIST_MOVE, folder export | T83 DB part green |
| I179 | [ ] | 7 | Text UI tree renders the folder tree | T83 green |
| I180 | [ ] | 7 | SHARE verb, link parse in `selecta` and `selecta-cli open`, DEViLBOX URL form (D-M3-11) | T84 green |
| I181 | [ ] | 8 | `src/library/xxh32.c`, UADE key, genre lookup with local cache, `genre:` key | T85 genre part green |
| I182 | [ ] | 8 | Songdb lookup, composer alias merge, composer page query | T85 green |
| I183 | [ ] | 8 | Similar and also-by queries | T86 similar part green |
| I184 | [ ] | 8 | Duplicates by md5 across sources, best source (D-M3-14) used by OPEN | T86 green |
| I185 | [ ] | 8 | Metadata panel rows (STIL, composer, release, year, genre, rating, plays) in module info | T87 green |
| I186 | [ ] | 8 | RADIO from a seed with genre/composer/rating steering, skip excluded | T88 green |
| I187 | [ ] | 9 | Desktop analysis job: faster-than-real-time render into R128 analyser, RMS+peak fallback, ANALYSE, `selecta-cli analyse` | T89 green |
| I188 | [ ] | 9 | Gain store per song key; DEViLBOX GET/PUT loudness by hash | T90 fetch part green |
| I189 | [ ] | 9 | Mixer 16.16 per-song gain and -1 dBFS limiter, `mix.loudness` pref (D-M3-9) | T90 gain and limiter parts green |
| I190 | [ ] | 9 | Integer adaptive leveler, provisional gain write-back | T90 green |
| I191 | [ ] | 10 | Home view source: recently played, jump back in, new this week | T91 green |
| I192 | [ ] | 10 | Stats SQL views and text UI stats screen | T92 green |
| I193 | [ ] | 10 | Lyrics: LRC, song message, DEViLBOX lyrics, current line in the snapshot | T93 green |
| I194 | [ ] | 11 | minimp3 and stb_vorbis RENDER-kind plugins over a byte stream (D-M3-8) | T94 decode part green |
| I195 | [ ] | 11 | URL and pipe paths in OPEN, M3U URL-only import, `PIPE:` on Amiga | T94 URL part green |
| I196 | [ ] | 11 | Stream-to-disk recording (F36), `selecta-cli record` | T94 green |
| I197 | [ ] | 12 | Library and SQLite in the 68k build, `library export-index`, read-only ATTACH (D-M3-16) | T96 green |
| I198 | [ ] | 12 | Measure the full Modland index (size, `db_heap_peak`, query time under vamos) from one real allmods_md5 build; record in Facts; open the fallback row only if it fails | numbers recorded in todos Facts |
| I199 | [ ] | 12 | Amiga bsdsocket net, DEViLBOX mirror fetch on the rig (D-M3-19) | T97 green |
| I200 | [ ] | 13 | T8 ratchet to 8 in `tests/ratchet/unimplemented.txt` | T8 green, count 8 |
| I201 | [ ] | 13 | RULES.md `## Commands` rows for every new `selecta-cli` and `selecta` command | rows present, each command run once |
| I202 | [ ] | 13 | Full finish-line run (reason: landing M3) | all T62 to T97 and T8 green, none skipped |
| I203 | [ ] | 13 | Manual checks 1 to 6 confirmed by Johan | Johan's confirmation recorded in ledger |
