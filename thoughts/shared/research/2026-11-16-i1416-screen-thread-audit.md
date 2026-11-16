---
status: draft
rows: I1416
date: 2026-11-16
---

# I1416: calls that can block the screen thread (UI loop + engine pump)

Scope: followed call paths from `pump_turn` (apps/selecta/main.c:209) into `sel_engine_pump` (src/engine/engine.c:11966), `desk_on_event`/art in the desktop UI, text UI, MUI. Pump and UI still share one thread (engine thread = I1082f, not done; main.c:199, :811). Library reads were already moved to posted reads by I829/I906; this audit lists what is left. Existing guard: `sel_library_watch(1)` + `library_ui_wait` log in debug builds (worker.c:111, design 2026-11-01-ui-thread-library-waits.md); its allow list is empty, so the guard log of a real session is the cheapest way to confirm the library rows below. Stall figures are estimates from code shape, not measurements.

## Ranked

| # | Call site | Blocks on | Fires | Worst stall (platform) | Proposed move | Effort |
|---|---|---|---|---|---|---|
| 1 | engine.c:2237 `sel_stream_read` (whole URL/pipe in memory); :2124 `sel_source_stream_url`; :2166 `sel_stream_resolve` (.pls/.m3u fetch); :2182-2213 `sel_live_open*` + `sel_live_head`/`sel_live_skip` (wait for first stream bytes), all in `read_song` from `prepare_slot` | network | every OPEN/NEXT of a URL, radio, podcast, stream | seconds to the net timeout (cancelled only when a newer STOP/OPEN is pumped, and that is on this same thread, so the cancel cannot fire while blocked); all platforms | Same as I1415: do `read_song` (fetch + head) on the rd_play read thread, pump only takes the finished bytes/stream handle and starts the replayer | L |
| 2 | engine.c:2153 `sel_library_fetch` in `read_song` fallback (remote key not covered by `play_meta_run`, `open_fetch_ms_max` measures it); :9277 `cmd_LIBRARY_FETCH` (control command, whole mirror download) | network + disk | OPEN of a remote library key without a covering song-start read; LIBRARY_FETCH command | seconds to minutes (large file, slow mirror); all | Fold into the rd_play read (`play_meta_run` at :3102 already fetches); make LIBRARY_FETCH a library job (task) with a done event | M |
| 3 | engine.c:2064 `read_whole_file` and `sel_file_stream_open/drain` (:2248, :2914) in `prepare_slot`, for current and next track (`prepare_next` :4048, :4080) | disk (NAS/SMB/DLNA mounts, floppy/CF on Amiga) | every song start and the gapless pre-open of the next song | 50-500 ms on NAS Mac; 1-10 s on Amiga for a multi-MB file from CF/IDE; blocks audio-feeding turns on Amiga | Read on rd_play with the archive unpack (I1415); pump hands the buffer to the replayer. Keep `open`/RP_OPEN on the pump | M (with I1415) |
| 4 | engine.c:2107 `sel_library_best_source`, :2432 `sel_library_song_config_get`, :2827 `sel_library_sysfile_request`, :2536 `sel_library_loudness_get_ex` (hist_end/loudness_slot): sync library round trips in song start/end paths when `play_meta_find` has no entry | worker lock (`call_enter` waits behind a whole library job, worker.c:278-292; on Amiga no thread so the SQL runs inline) | song start/end; only when the play_meta read did not cover the path | unbounded while a scan/sync job holds the call lock (tens of s); Amiga: SQL query time inline (10-300 ms) | Cover every path with the rd_play `play_meta_run` read before OPEN (I906g leftover); where no entry exists, defer the OPEN until the read lands instead of falling back to a sync call | M |
| 5 | mui_ui.c:3045 `sel_engine_song_group` per visible row when the list signature changes (list rebuild) -> `sel_library_song_group` | worker round trip per row (inline SQL on Amiga) | every list rebuild: scroll, page, search result change | N rows x query; 20-40 rows x 10-50 ms = 0.2-2 s on 68k; this is the MUI scroll sluggishness candidate | Post as a batch like desktop `sel_engine_library_items_post`/`song_briefs_post` (engine.c:10335 exists), take answer next turn, redraw cells when it lands | M |
| 6 | desktop/art.c:374, :518 `art_thread_start` returns NULL on Amiga -> `job_merge`/`big_merge` run the whole batch inline (`fetch` of blob + JPEG/PNG decode + `fit_square`, `ART_BATCH` covers per call) | CPU + DB blob read | when a view shows rows without cached art; Now Playing big cover | Mac/Linux: async already (worker). Amiga: one batch decode = seconds on 68020 | Amiga: process one cover per turn (time-sliced) instead of a whole batch, or skip art on 68k tiers; no thread exists | M |
| 7 | text_ui.c:1072 `sel_engine_library_items` (sync playlist menu list) | worker round trip | opening the playlist menu | bounded by a running library job (tens of s worst); text UI is Mac/Linux/Amiga | Use `sel_engine_library_items_post/take` as desktop does (desktop_ui.c:12400) | S |
| 8 | engine.c:4048 `prepare_next` -> `prepare_slot` for the next track while the current one plays (replayer open, depack, `unpack_archived_module`) | CPU (+ items 1, 3) | once per song, near its end | Mac ms; Amiga 100 ms to seconds of dropped UI frames, risk of audio underrun on the same thread | unpack on read thread (I1415); replayer open stays | M (part of I1415) |
| 9 | engine.c:6020 `cmd_CUE_GOTO` `sel_library_cue_get`, :5841/:5608 vote/like push, :10560 `songs_unstage` `sel_library_playlist_keys`, :12354 `stats_get_live` `sel_library_pin_rows` | worker round trip | on click (cue jump, like, unstage), stats panel refresh | bounded by running library job; small otherwise | Route through the write queue (I906f `wq_*`) or a read slot; verify with the guard log first, some may already be covered | S each |
| 10 | engine.c:11739 `lyrics_open` `fopen` + `fread` of .lrc next to the song | disk | song start | few ms; NAS 50-200 ms; Amiga slower | Add to the rd_play read | S |
| 11 | engine.c:7351 playlist import `fopen` probe per entry (missing check) | disk | PLAYLIST_IMPORT | N entries x stat; 1000 entries on NAS = seconds; Amiga worse | Run on a library job or limit to probe-on-play | S |
| 12 | engine.c:4275/4375 playlist file parse/save, :4572 sample export `fopen` | disk | on command (load/save/export) | tens of ms; Amiga up to a second | Low priority; leave | S |
| 13 | main.c:11960 `sel_engine_pump_reads` with `sel_pal_sleep_ms(1)` | sleep loop up to 60 s | clockless/test runs only | not a user path | none | - |
| 14 | mui_ui.c:4596 `Delay(1)` loop in `mui_close` | wait for song_briefs answer | close only | one query | none | - |

Already known and not re-researched: archive unpack and replayer start in `prepare_slot` on the pump (I1415 moves unpack to the read thread; replayer start stays).

## Safe as is (checked)

- Library list/home/search/stats/collections/radio/panel/art-ids: posted reads (`lib_reads_flush`/`lib_reads_collect`, engine.c:9899-10060); pump only applies results.
- Scan, sync, search, smart_sync, demozoo, prefetch: library job threads (`sel_library_task_start`, engine.c:5423, :8240, :8455); `sel_library_prefetch` called from `prefetch_sync` only posts.
- Desktop sidebar playlist items and song menu: `sel_engine_library_items_post/take` (desktop_ui.c:12378-12435).
- Desktop cover load: art worker thread (art.c:237 `worker`, :446 `big_worker`); `art_pump` only joins a finished thread and copies tiles (cheap).
- `catalogue length` read in `hw_length_apply` uses `sel_library_nowait` + play_meta (engine.c:1950).
- Desktop `desk_drop_path` stat (one stat per dropped file), `desk_load_theme`/`desk_load_input`/`desk_write_frame`/`desk_control` file I/O: test/CLI/rare paths.
- `refresh_snapshot`/`publish_aux`/`view_sync` (engine.c:11794, :9779): memory only as far as read (not profiled).

## Not checked

- Desktop draw functions (`draw_*`, 18k lines): only grepped for fopen/stat/dir walks/library calls (none found); CPU cost of text layout, atlas building (atlas_build.c), icon rebuild, large-loop sorts in view code not profiled. I574 notes the Amiga AGA c2p/palette cube costs were already fixed.
- `sel_gui_art_cache`, `desk_png`/`stb_impl` decode sites outside art.c; theme/font load at startup (desk_open, 6-12 s on AGA per I574 notes, startup only).
- Sorts and whole-library loops inside engine `view_*`, `queue_*`, `composers_build_list` (engine.c:6940), `search_finish`, `home_apply`: not read; candidates for CPU stalls on large libraries.
- `sel_engine_pump` branches `sf_poll`, `timer_pump`, `reclaim_slots`, `dispatch` handlers one by one (only those that call sel_library_*/fopen were listed from a grep with approximate enclosing-function attribution; line numbers verified individually only for items 4, 5, 9).
- MUI draw paths (render/canvas), text UI `tui_draw`: only grepped for I/O.
- Output plugins' `so.pump` (audio device callbacks, AirPlay) and the SDL/AppKit event pump (I906k).
- Real measurement: run a debug build with the guard in log mode and `--stats` (`pump_ms_max`, `turn_ms_max`, `watched_waits`) on the real library to confirm ranks 4, 5, 7, 9; none was run (read-only audit).
