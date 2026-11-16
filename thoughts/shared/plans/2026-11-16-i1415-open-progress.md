---
status: approved
rows: I1415
date: 2026-11-16
---

# I1415: progress feedback from click to sound, with cancel

Johan 2026-11-16: "can we add progress bars when connecting/downloading/unpacking/etc so i get feedback until the song actually plays?"
Johan chose: progress in the now-playing bar AND a thin bar on the clicked row (list, grid, cover flow); a cancel button
that stops the download while the song playing before keeps playing.

## Current state (code read 2026-11-16, not run)

- OPEN (`commands.def:22`) -> `cmd_OPEN` -> `begin_song` -> `play_meta_ready` posts `play_meta_run` on the read thread
  `rd_play` (engine.c:3086). That thread runs `sel_library_fetch` -> `sel_cache_fetch_song` -> `sel_cache_fetch`
  (cache.c:221): mirror download to `.part`, md5, rename, `sel_source_unzip` for zipped sources, then companions.
- The pump (UI main-loop thread) retries the open from `e->deferred` each pump; when the answer is published,
  `prepare_slot` -> `read_song` -> `unpack_archived_module` (LHA/LZX/zip modules, engine.c:2766) -> replayer open.
  Unpack and replayer start block the pump thread.
- Byte counting exists: cache `sink.offset` (cache.c:125). Total size is parsed (`content_length`, http.c:349) but only
  reported after the body ends. No progress reaches the UI.
- Cancel: `c->cancel` stops a cache download at its next chunk, but only the prefetch cache sets it (fetch.c:158).
  `rd_play` registers no `sel_net_set_cancel`. STOP/OPEN bump `cancel_gen`, which only cancels pump-side URL reads.
- A second click while a download runs: the new path waits in `e->deferred` until the first download finishes
  (no abort), then starts. Latest click wins, but slowly.
- UIs already poll snapshot structs each frame for long jobs (`sel_engine_sync_progress`, engine.h:345). No progress event.

## Design

### 1. Engine snapshot: `sel_engine_open_progress`

Same pattern as `sel_sync_progress` (polled, no new event kind):

```c
/* I1415: the song being opened, for the now-playing bar and the clicked row. active 0 = no open in progress. */
typedef enum { SEL_OPEN_IDLE, SEL_OPEN_CONNECTING, SEL_OPEN_DOWNLOADING, SEL_OPEN_UNPACKING, SEL_OPEN_STARTING } sel_open_phase;
typedef struct sel_open_progress {
    uint32_t active, phase;
    uint32_t done, total;          /* bytes of the current file; total 0 = unknown (indeterminate bar) */
    uint32_t file, files;          /* 1-based file of `files` (song plus companions); files 0 = unknown */
    uint32_t elapsed_ms;           /* since OPEN; UIs show nothing below the show threshold */
    char path[SEL_DEFERRED_PATH_MAX]; /* matches the row the user clicked */
} sel_open_progress;
void sel_engine_open_progress(const sel_engine* e, sel_open_progress* out);
```

- Writers: `rd_play` (Connecting, Downloading, Unpacking for zipped sources) and the pump (Unpacking for archived
  modules, Starting). Fields are `volatile uint32_t` written with the existing `SEL_ATOMIC_*` helpers; `path` is
  written by the pump only (at OPEN), before `active` is set. A torn done/total pair shows one wrong frame; accepted.
- Ends: `active = 0` on SONG_OPENED, on ERROR for that path, and on cancel.

### 2. Byte progress from the download

- `sel_http_req` gains `volatile long* length_out`: http.c writes `content_length` there as soon as the head is
  parsed, before the first body byte. `sel_mirror_opts` passes it through. One field, no new callback.
- The cache `sink` gains `volatile uint32_t* progress_done` (and the cache a `progress` pointer set like `cancel`):
  `on_body` stores `s->offset`; `on_restart` stores 0. Range resumes (I1413) report from the resumed offset.
- Phase: Connecting from OPEN until the first body byte, then Downloading. Companions: `file`/`files` from the
  companion loop (cache.c:671).
- Local files and cache hits never leave Connecting for more than a few ms; the show threshold hides them.

### 3. Cancel: new command CANCEL_OPEN

- `commands.def`: `SEL_CMD(CANCEL_OPEN, "", "transport", "", "Cancel Loading")`.
- Engine: a new counter `e->open_cancel` (not `cancel_gen`: STOP semantics must not apply).
  - `play_meta_run` points the foreground fetch at a per-run cancel flag and registers it with
    `sel_net_set_cancel` on `rd_play`. Per call, not by mutating the shared `lib->cache->cancel` field
    (the builder confirms who else uses `lib->cache`; if only `rd_play`, setting the field per run is fine).
  - `cmd_CANCEL_OPEN`: sets the flag, clears `e->deferred` for that path, sets `active = 0`. The playing slot is
    not touched, so the previous song keeps playing.
  - The cancelled run publishes nothing for the path (or a `cancelled` mark `prepare_slot` drops), so no late start.
  - Unpack on the pump checks the flag between archive members (`on_archive_member` returns stop).
  - The `.part` file stays: the existing slice adopt (cache.c:250) resumes it on the next click.
- Second click on another song: `cmd_OPEN` sets the same cancel flag when `rd_play` is busy with a different path,
  so the new song starts at once instead of after the old download.

### 4. Unpack moves to the read thread (Johan's choice)

- `play_meta_run` already holds the fetched file on `rd_play`. After the fetch it reads the file, runs
  `sel_archive_kind` and, for a folder archive, the same `sel_archive_each` + `on_archive_member` walk
  `unpack_archived_module` does today (engine.c:2766), keeping the chosen member buffer in the `sel_play_meta`
  entry (owned there, freed with the entry). Phase = Unpacking during it; the cancel flag stops the member walk.
- `read_song` takes the unpacked buffer from the entry; `unpack_archived_module` stays as the fallback for paths
  no song-start read covered (local files opened inline), so one function serves both threads (no copy).
- The builder checks `on_archive_member` and the host allocator for pump-only state first; any found is the
  first thing to fix, not to work around.
- Starting (replayer open) stays on the pump: the pump sets the phase, returns, and opens on the next pump, so the
  UI draws one "Starting" frame first. The bar stands still during the replayer open only.

### 5. UI twins (one shared formatter, three drawers)

- Shared: `sel_open_progress_text(const sel_open_progress*, char* out, size_t n)` in the uimodel layer, giving
  "Connecting", "Downloading 42%" (or "Downloading 1.2 MB" when total unknown), "Downloading file 2 of 3 - 42%",
  "Unpacking", "Starting". Fraction helper returns q16 or -1 for indeterminate. All three UIs use it.
- Mac desktop (`plugins/ui/desktop/desktop_ui.c`):
  - Now-playing bar `draw_transport`: while active, the title area shows the clicked song's title with the phase text;
    the seek bar becomes the load bar (`draw_bar`), indeterminate = moving segment; a cancel icon button (lucide `x`)
    sits where the play button is. Esc cancels too.
  - Rows: `draw_lib_styles` (list), `draw_grid_rows` (grid), `draw_flow_rows` (cover flow), plus `draw_browse` and
    `draw_playlists`: the row whose path equals `path` draws a 2 px bar along its bottom edge (accent token).
- Text UI (`plugins/ui/text/text_ui.c`): `tui_player_bar` shows `[Downloading 42%] ######----` and the clicked
  row in `tui_styled_rows` shows a `*` marker with the percent at the row end. Esc cancels; the help bar names it.
- MUI (`plugins/ui/mui/mui_ui.c`): `mui_np_text` shows the phase text; a Gauge object in the now-playing group shows
  the bar (hidden when idle); a "Cancel" button beside it. The list row in `mui_listview_class.c` gets the percent
  in its status column. Poll on the existing `mui_refresh` tick.

### 6. Tests (host)

- Fixture HTTP route that serves a large body slowly: assert the snapshot goes Connecting -> Downloading with
  done rising and total equal to the body size -> idle after SONG_OPENED.
- CANCEL_OPEN mid-download: previous song still in the playing slot, no SONG_OPENED for the cancelled path,
  `.part` kept, a second OPEN of it resumes with Range.
- Second OPEN mid-download of another path: the second song opens without waiting for the first body.
- Formatter unit test for every phase and the unknown-total case.

### 7. Build order (sonnet per track, each needs Johan's go before building)

1. Engine + http/cache plumbing + CANCEL_OPEN + host tests.
2. Mac desktop drawers. 3. Text UI. 4. MUI (Amiga compile + one rig check at the end, RULES 4a).

## Decisions for Johan (asked 2026-11-16)

- Second click while downloading: abort the first download, start the new song at once (Recommended).
- Bar shows after 300 ms of loading, never for fast local or cached opens (Recommended).
- Esc cancels on Mac and text UI while loading, only when nothing else takes Esc (Recommended).
- Unpack moves off the screen thread to the read thread (Johan chose this over the one-frame trick).
