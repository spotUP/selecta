---
status: draft
row: I1082
parent: I1080
date: 2026-11-05
author: Opus design agent
---

# I1082 Engine off the UI thread: engine thread, published snapshot, copied commands

Ledger row: `thoughts/spot/todos.md` I1082 (evidence rows I1078, I1080). Line numbers are against main `e8c51f89`
(2026-11-05). Prior research: `thoughts/shared/research/2026-11-01-ui-thread-library-waits.md` (I906, read only here).

Johan 2026-11-05: "move engine off the UI thread". The beach balls behind it:

- I1078: sample of pid 11612, main thread in `sel_engine_pump > begin_song > prepare_slot > find_replayer > zxtune
  rp_probe > Raw::Scaner` (whole multi-MB podcast scanned on the UI thread).
- I1080: sample of pid 90665, main thread in `sel_engine_pump > begin_song > prepare_slot > sel_live_drain >
  view_read_wait` while the download thread sits in `sel_http_request`.

Both were fixed at their own site (bounded probe, ID3 skip). The class is not: any blocking step inside the pump
freezes every UI, because the pump runs on the UI thread.

## 1. Current state

### 1.1 Where the pump runs

One shell loop in `apps/selecta/main.c` runs every UI and the engine on the process main thread. There is no engine
thread. This is the written design: architecture doc `2026-10-22-architecture-design.md:202` ("the UI thread calls
`sel_engine_pump()`").

- Shell loop `main.c:893-945`: for each UI `g_uis[u].wait(state, wait_ms)` (main.c:904), then `pump_turn`
  (main.c:909), then `so.pump(so.out, ...)` for outputs the shell paces (main.c:925-928), `pump_turn` again (933),
  `sel_int_session_poll` (934), control socket `sel_ctl_serve` (937), ARexx `arexx_poll` (942, Amiga).
- `pump_turn` (main.c:149-157): clockless runs call `sel_engine_pump_reads(e, sink, ctx, 60000)` (waits up to 60 s for
  posted reads, determinism for scripts and dui goldens), paced runs `sel_engine_pump`. `g_pump_ms_max` (main.c:148,
  I906a) measures it.
- Other pump callers on the same thread: startup and `--exec` steps (main.c:828, 853, 859, 868, 885), `main.c:425`,
  `apps/common/ctlexec.c:64` (control line), `apps/common/integration_host.c:32`.
- Desktop (`plugins/ui/desktop`): `desk_wait` (desktop_ui.c:16264) -> `desk_wait_turn` (16279): follows
  (`desk_library_follow` 14573, art, playlist job, sync, side), SDL poll, draw, `d->frames++` (16311). `turn_ms_max`
  covers desk_wait only, not the pump.
- MUI (`plugins/ui/mui/mui_ui.c`): `mui_wait` (3387) -> `mui_wait_step` with `mui_io_wait` (3364): Exec `Wait()` on the
  MUI port mask, a timer.device request, Ctrl-C and `wake_mask` (3393; set to 0 at 3300, so no engine wake is wired).
- Text UI: `tui_wait` (`plugins/ui/text/text_ui.c:1291`). Web: no web UI plugin exists (`plugins/ui` holds desktop,
  mui, text only).

### 1.2 How UIs talk to the engine today

- Commands in: `sel_engine_send` (engine.c:10589) pushes onto `cmd_q`, a `sel_spsc` (engine.c:477, init 10412). It
  bumps `e->cancel_gen` for STOP and OPEN before the push (I195a). `sel_engine_send_line` (`src/protocol/parse.c:7`)
  decodes the line with string arguments pointing INTO A CALLER POOL; the engine reads them on its next pump. Desktop
  keeps `static char ring[16][512]` (desktop_ui.c:12932-12939, comment: "the pool outlives the call"), text UI a
  rotating pool (text_ui.c:94-99), MUI a ring (mui_ui.c:452).
- Events out: the pump drains `evt_q` at its end (engine.c:10917-10922) into the shell `sink` (main.c:118-131), which
  calls every UI's `on_event` (main.c:127-128) and the integration session, synchronously on the pump thread.
- State: `sel_engine_state()` returns `&e->state` (engine.c:10930-10932), the live struct the pump mutates. Its pointer
  fields (`queue`, `queue_text`, `ctx_name`, `view`, `viz`, `pattern`, `panel`, `stats`, `composers`, `formats`,
  `composer_page`, `collection`, lyric strings; `include/selecta/engine.h:228-270`) are engine buffers "valid until
  the next pump" (5 such notes in engine.h). Serials (`panel_serial`, `search_serial`, `library_serial`,
  `sources_serial`, `library_pins`; engine.h:251-255) are plain `uint32_t` reads.
- Direct accessor calls by UI modules (call counts from grep of each module, distinct functions from engine.h):
  - desktop: `sel_engine_state` 136, `stats_get` 14, `plugin_host` 7, `view_row_path` 3, `song_text` 3,
    `scan_progress` 2, `scan_info` 2, `sample_get` 2, `library_items_post/busy/take` 2 each, `lib_site` 2, `lib_nowait`
    2, `sync_progress` 1, `sync_cancel` 1, `sources_pending` 1, `song_text_generation` 1,
    `song_briefs_post/busy/take` 1 each, `send_line` 1, `queue_groups` 1, `pattern_request` 1, `lib_deferred` 1.
  - MUI: `state` 13, `view_row_path` 2, `plugin_host` 2, `song_group` 1, `send_line` 1, `pattern_request` 1.
  - text: `state` 12, `view_cursor_key` 2, `send_line` 1, `pattern_request` 1.
  - `src/uimodel` (UI side, shared): `state`, `stats_get`, `scan_info`, `sync_info`, `library_items` (synchronous).
  - shell `apps/selecta`: `state` 15, pump 5, lifecycle and output setters.
- UI to library directly: `sel_ui_art` callbacks `find/get/put` (`include/selecta/ui.h:13-17`) reach the library from
  the UI thread (`art_load`, `plugins/ui/desktop/art.c:194`, start-then-join, 1816 of 4170 guard hits in the I906 log).

### 1.3 Threads that exist

| Thread | Created | Owns |
|---|---|---|
| main (UI + engine pump + paced output pump + control socket) | process | every UI, `sel_engine_pump`, `so.pump`, ctl, ARexx |
| audio (output callback) | output plugin (`plugins/outputs/coreaudio/macos/coreaudio.c`) | `sel_engine_render` (engine.c:11641, `SEL_THREAD_AUDIO`), slot swaps via `swap_q` (11652) |
| library worker | `src/library/db/worker.c:242` | the sqlite connection (owner check `owner_check`, worker.c:112-129); callers block in `sel_library_request` (173-208) |
| library job | `src/library/db/job.c:100` | scans, syncs, pin ring |
| download | `src/library/stream_open.c:709`, `:771` | one HTTP stream per live open |
| posted reads | engine.c:4360 (`lib_read_main`), 7874 (search), 9188 (items), 9268 (briefs), 9329 (playlist keys) | one read each, `__atomic` state 0/1/2 (engine.c:4343-4383) |
| art decode | `plugins/ui/desktop/posix/art_thread.c:28` | joined at once by `art_load` |

All posted-read threads exist only under `SEL_LI_THREAD` (engine.c:50-54: not Amiga). The audio thread already reads
`e->state.play` without a barrier (engine.c:11672 `sel_timer_poll(..., e->state.play == SEL_PLAYING, ...)`).

On the Amiga there is one task: MUI wait, engine pump and the AHI fill (`so.pump` -> `out_pump`,
`plugins/outputs/ahi/common/ahi_out.c:91`, paced by `WaitIO` on the double buffer, `ahi/amiga/ahi.c:103`) all run in
it. So on the Amiga a blocking open also starves the playing song's audio, not only the window.

## 2. Violated invariant and target architecture

**Invariant:** a UI thread never waits on anything slower than one frame. Engine work (opens, probes, drains, unpack,
library calls, sync collection) may take any time; it must never take it on the thread that draws.

**Root-cause level:** the shell's thread model (`apps/selecta/main.c` and design doc :202), not any one open. Fixing
sites (I1078 probe, I1080 ID3 skip, I906 posted reads) closes known holes one by one; the next blocking step (slow SMB
volume, archive unpack, a sync collect) freezes the window again.

### 2.1 Target (desktop, paced runs)

```
 main thread (UI)                          engine thread                         audio thread
 ---------------                           -------------                         ------------
 desk_wait / tui_wait                      loop: wait(cmd signal | turn timer)   output callback
   draw from published snapshot  <-- pub --  pump: dispatch cmd_q, collect,       sel_engine_render
   on_event from UI event ring   <-- evt --       begin_song (may block),         (unchanged, swap_q)
   send: copy line into cmd_q    -- cmd -->       publish snapshot, push events
   ctl socket, ARexx (producers)              so.pump (paced outputs), hw_step,
                                              sel_int_session_poll
 library: never                             library: posted reads + waits OK    library: never
```

- **What crosses, commands in:** `cmd_q` stays the one SPSC ring; every producer (UI sends, control socket, ARexx,
  `--exec`) stays on the main thread, so it keeps one producer. Each command carries its string arguments COPIED into
  the ring slot (an arena per slot), so the caller pools in desktop/MUI/text go away. Today a stalled engine with 17
  queued desktop lines overwrites `ring[16]` before the engine reads it.
- **What crosses, state out:** at the end of each pump the engine publishes a snapshot: `sel_state` plus a deep copy of
  every pointer field into one of three engine-owned buffers (triple buffer, one atomic index). The UI acquires the
  newest at the start of its turn and reads only that until its next turn. `sel_engine_state()` keeps its signature;
  called from the UI thread it returns the acquired snapshot. Single-thread shells (clockless, Amiga) publish nothing
  and return the live state, as today.
- **What crosses, events out:** the engine thread's `evt_q` is drained into a second SPSC ring (engine -> UI) instead of
  calling the sink; the UI thread drains it in its turn and calls `on_event` there. Event string payloads are copied
  into the ring slot (same rule as commands).
- **Accessors:** every engine.h function a UI calls is put in one of three classes (section 4): snapshot field,
  command plus snapshot/event answer, or "any thread" with a stated lock-free protocol (seqlock like
  `sel_engine_scan_progress`, engine.c:4747-4756).
- **db ownership:** unchanged at the bottom (the worker thread owns sqlite). The engine thread is the only caller that
  may wait on the library. The UI thread never calls the library: the I906 watch (`sel_library_watch(1)`, main.c:684)
  stays on the main thread, which becomes the UI-only thread; `sel_ui_art` callbacks move behind an engine request
  (or an art thread with no join, I906c).
- **Wakeups:** `sel_engine_send` signals the engine thread (condvar or semaphore), so a command never waits a whole
  turn; the engine's publish wakes the UI (SDL user event on desktop, `wake_mask` signal on MUI, a pipe byte for the
  text UI), so a new snapshot draws without polling.
- **Cancel on STOP/OPEN:** `cancel_gen` and `cancel_served` (engine.c:10591-10593, 10841-10848) become `__atomic`
  (written on the main thread, read on the engine thread inside net waits). Every blocking open step checks it:
  net waits already do (`sel_net_set_cancel`, engine.c:10839), drains are cancel-aware since I1080; archive unpack
  (`unpack_archived_module`, engine.c:2582) checks per member; local file reads go chunked with a check per chunk;
  probes are bounded (I1078 contract in `include/selecta/replayer.h`). A cancelled open returns at once and the next
  OPEN runs.
- **Shutdown order:** UI sees quit -> sends QUIT and sets an atomic stop flag + cancel bump -> engine thread finishes
  its pump (blocking open returns on cancel), closes the shell-paced output, exits -> main joins (bounded wait, a
  stuck join is reported with `engine_thread_join_ms=` and aborts in debug) -> UIs close -> `sel_engine_destroy` ->
  `sel_library_close`. Today's order (main.c:1153-1166) stays after the join.
- **Clockless runs keep the inline pump.** dui goldens, `selecta-cli script` and `--exec` steps rely on "one UI turn per
  pump" (main.c:146-147). They keep today's single-thread loop; only paced runs start the engine thread
  (decision D1).

## 3. Amiga tier (68020, MUI)

Recommendation: the Amiga keeps the single task and the pump in the MUI loop; its blocking opens become stepped
(bounded work per pump). Reasons, all Amiga specific:

- libnix `malloc` and stdio are not reentrant across Exec tasks; engine and UI code both allocate. A second task needs
  every allocation behind a semaphore or on `AllocVec`, a change across shared code with no measured gain yet.
- `bsdsocket.library` bases and socket descriptors belong to the opening task; the net code would have to open its own
  base in the engine task and never hand a socket to the UI task.
- The AHI fill runs in the shell task (`so.pump`); an engine task would have to take it along, then MUI gadgets and
  the fill would share one CPU with Exec scheduling only. On a 68020 the snapshot copy per pump is cheap (a few KB),
  but the task split itself is the risk.
- Posted library reads already run inline there (`SEL_LI_THREAD` off, engine.c:50-54); the library waits I906 counts
  are the Amiga's main cost and stay what they are.

Stepped open: `prepare_slot` (engine.c:2605) becomes a resumable step function (read head -> probe -> drain one chunk ->
unpack one member -> open replayer), state kept in the slot, each pump advancing it under a budget (e.g. 20 ms by the
`sel_machine` speed class), so the MUI loop and the AHI fill keep their turns. The same code runs on desktop on the
engine thread with an unlimited budget, so there is one open path, not two. If the rig shows MUI still stalls (library
waits, unpack of a big LhA), an Exec engine task is a later row with its own design (D2).

Cost limits: no new allocation per pump on 68020; stepped state lives in the slot; budget from `sel_machine`, never a
build-time constant (RULES 4).

## 4. Risks: every UI-reachable engine call that becomes a race

Counted from `include/selecta/engine.h`: 72 `sel_engine_*` declarations (two header greps: column-0 declarations, and
every `sel_engine_*(` line minus 3 comment lines, both 72). Classed by who may call them after the split:

| Class | Functions (engine.h line) | Race after the split | Fix |
|---|---|---|---|
| A. Shell lifecycle, before the engine thread starts or after it joins | create 373, destroy 374, quit_queue_dropped 376, set_mix 419, set_output 425, output_opened 428, set_voice_sink 431, route 437, voice_route 439, set_handover 445, set_flush 450, set_output_switch 457, handed_over 459, set_hw_stepped 463, hw_step 464, routed_direct 466, set_stage 472, set_paula_output 473, set_ui_view_styles 735, take_ui_switch 736, take_quit 738, in_batch 740, report_error 384, plugin_host 409 | output switch, hw_step and take_* are called mid-loop today (main.c:910-920, 930) | move into the engine thread's loop; take_ui_switch / take_quit become events; report_error goes through cmd_q |
| B. Command in | send 380, send_line 390 | caller pool overwritten while the engine is stalled (desktop_ui.c:12932-12939, text_ui.c:94-99, mui_ui.c:452); `cancel_gen` non-atomic | copy strings into the ring slot; atomics |
| C. Pump, engine thread only | pump 394, reads_pending 397, sources_pending 400 (desktop calls it), pump_reads 403, scan_wait 371 | `sources_pending` read from desktop | snapshot field |
| D. State reads | state 405 (136 desktop + 13 MUI + 12 text + 15 shell + uimodel), stats_get 608 (14 desktop), scan_info 311, scan_progress 325 (seqlock, safe), sync_progress 335 (no lock, reads `sync_ctx` the job updates), sync_info 352, scan_busy 370, result_key 610, view_cursor_key 619, view_row_path 621 (walks `e->view`, `e->browse`, `e->queue`), tracker_info 710, tracker_cells 711 (calls the replayer on the caller's thread, engine.h:151), sample_get 723, render_calls 702 | torn reads, pointers freed by the next pump | published snapshot; tracker cells and samples answered into the snapshot by request |
| E. Posted reads and caches mutated from the UI thread | library_items 642 (sync, uimodel), library_items_post/busy/take 649-651, playlist_keys_post/busy/take 657-659, song_briefs 678 + post/busy/take 682-684, queue_groups 667, song_text 668 (callocs and fills `e->txt` from the caller, engine.c:8522-8535), song_text_generation 669, song_title 670, song_group 672 (MUI), song_meta 674, pattern_request 725 (writes `pat_*` plain, engine.c:11738-11747) | two threads write the same engine fields | each becomes a command whose answer lands in the snapshot (generation counter), or a slot with a single writer per field |
| F. Thread-local library flags | lib_nowait 687, lib_site 689, lib_deferred 690 | `__thread` flags set on the UI thread stop meaning anything once the UI never calls the library | drop from UI code; engine thread sets its own |
| G. Audio | mark_audio_thread 697, unmark_audio_thread 700, render 701, fade 704 | `audio_pull` (main.c:161-171) toggles marks by `g_pumping`, set around `so.pump`, which moves to the engine thread | `g_pumping` set by the engine thread; audio reads of `e->state.play` (engine.c:11672) become an atomic copy |
| H. Sync control | sync_cancel 336 (calls `sel_library_task_cancel_request` from the UI) | safe only if that call is lock-free (unverified) | command `SYNC_CANCEL` or verify |
| I. Pure | cmd_implemented 729 | none | none |

Plus the non-header paths: the shell `sink` calling `on_event` on the pump thread (main.c:127-128; moves to the UI
ring), the `sel_ui_art` callbacks (ui.h:13-17), and every `d->...` field the desktop follows set from `on_event`.

Other risks: dui goldens and frame hashes assume one pump per UI turn (kept by D1); `sel_spsc` was designed for
UI->engine single-core use (design doc :203, `volatile` on Amiga); its memory ordering on arm64 is unverified (I1082b
must read it and use acquire/release); the I906 guard counts waits on the watched thread, which stays the main thread.

## 5. Rows

Each row is one commit, RED test first (fails on main, passes with the row, fails again when reverted). Build and test
commands: RULES `## Commands` (dui tests on `build/sdl`).

| Row | Scope | RED test (sentinel) |
|---|---|---|
| I1082a | Reachability test first, red on main: `dui-engine-thread-stall` runs the real `Selecta` binary (`build/sdl`, desktop UI, SDL backend, a paced silent output) against `tests/net/slow_server.py` serving a module that needs the whole file (sends 10 percent, then stalls 10 s), sends `OPEN <url>`. Adds the dump lines `ui_gap_ms_max=` (longest wall time between two `desk_wait` returns, pump included) and `engine_thread=0/1`, `engine_pumps=` | during the stall: `frames=` (desktop `ctl ... frames=`, desktop_ui.c:15224) grows by at least 20, `ui_gap_ms_max < 100`, `engine_thread=1` and `engine_pumps` moved (proves the new thread ran). Red on main: frames stop, gap about 10 s. Needs a paced silent output: if `null` has no paced mode, this row adds one (`SEL_OUT_UNPACED` absent, no audio device) |
| I1082b | Command strings copied into the `cmd_q` slot arena; `cancel_gen`/`cancel_served` atomic; drop the caller pools in desktop, MUI, text | engine test: 64 `send_line` calls with distinct string args through one 16-entry pool, no pump between, then one pump: every arg intact (red: overwritten) |
| I1082c | Engine -> UI event ring; shell sink pushes, UI turn drains and calls `on_event` on the UI thread; event strings copied | engine test with a threaded shell harness: `on_event` thread id == UI thread id for every event, none lost across 10 000 events |
| I1082d | Published snapshot: triple buffer, deep copy of every `sel_state` pointer field at pump end, `sel_engine_state()` returns the acquired copy on the UI thread; copy bytes and time counted (`pub_bytes_max=`, `pub_us_max=`) | TSan test: UI-side reader walks queue/view/collection while the engine thread edits the queue and rebuilds views 10 000 times; clean (red: TSan reports on main). Desktop stays green on the dui suite (affected set) |
| I1082e | Class E and D accessors (section 4): song_text, song_group, song_meta, song_title, queue_groups, items/briefs/playlist_keys slots, pattern_request, view_row_path, view_cursor_key, tracker_cells, sample_get, stats_get, sync_progress, sources_pending become commands with snapshot answers or single-writer slots | TSan test per group, same harness as I1082d; each existing dui test of the moved entry (direct_calls_async etc.) stays green |
| I1082f | Shell split in `apps/selecta/main.c`: paced runs start the engine thread (pump_turn, `so.pump`, hw_step, int_session_poll, output switch, take_quit/take_ui_switch as events); send wakes the engine; publish wakes the UI (SDL user event, text pipe); clockless runs unchanged | I1082a goes green; `engine_thread=1` only for paced runs, `0` for clockless (a clockless dui test prints it) |
| I1082g | Cancel through every blocking open step: archive unpack per member, chunked local read, drain (I1080), probe bound (I1078); OPEN B during a stalled open A | test over slow_server: OPEN A (stalls), OPEN B (local file) 100 ms later: B plays within 300 ms, `net_cancels` moved by 1, A's download thread joined |
| I1082h | UI never calls the library: `sel_ui_art` find/get/put behind an engine request answered by event, no join on the UI thread (shares the I906c design) | dui test with the I906 guard in abort mode on the main thread: open Home and a cover page while the worker is busy, `sel_library_watched_waits()==0` |
| I1082i | Shutdown order (section 2.1): quit during a stalled download | dui test: quit 1 s into the stall, process exits within 1 s, `engine_thread_join_ms < 500`, no leak report |
| I1082j | Amiga stepped open: `prepare_slot` as a resumable step function with a `sel_machine` budget per pump; desktop runs it with no budget on the engine thread (one path) | host test: stepped open of a 2 MB archive with a 5 ms budget takes several pumps, each under budget, same audio hash as the one-shot open; rig check (Amiberry, rig 2 `--ram 256`, MUI): during a stalled HTTP open the MUI window keeps redrawing and the playing song keeps sounding by its `ahi_bufs` counter (no audio out, RULES no-audio memory). Exemption if the rig has no route to the host fixture: named in the row |
| I1082k | Docs: architecture design doc threading model (:201-203) rewritten to section 2.1; engine.h thread-class note on every function of section 4; RULES Commands line for the new test | grep of engine.h: every `sel_engine_*` declaration carries a class tag (72 of 72) |

Order: a (red), b, c, d, e, f (a turns green here), g, h, i, j, k. Done for I1082 = I1082a green through the real
`Selecta` entry, TSan tests clean, Johan Mac check (Mac: play a podcast and a Modland song over a slow link, the
window keeps moving, STOP answers at once), Amiga rig check of I1082j.

## 6. Open decisions for Johan

- **D1 Engine thread only for paced runs.** Recommended: yes; clockless runs (dui goldens, scripts, `--exec`) keep
  the inline pump. Trade-off: two shell modes to keep alive, against rewriting 158 dui tests and the golden flow that
  depend on one pump per UI turn; the reachability test needs a paced silent output.
- **D2 Amiga: stepped open in the single task, not an Exec engine task.** Recommended: stepped open now, engine task
  only if the rig shows MUI still stalls. Trade-off: the stepped open fixes opens only (library waits still cost MUI
  turns on the Amiga), against an engine task that needs task-safe libnix allocation, a per-task bsdsocket base and the
  AHI fill moved to the engine task.
- **D3 Snapshot by copy per pump, not a lock.** Recommended: triple-buffered deep copy. Trade-off: copy cost per pump
  (measured by `pub_bytes_max`/`pub_us_max`; the collection page and queue text are the large parts), against a mutex
  the UI takes while it draws, which makes the UI wait on a busy engine again (the beach ball moved, not removed).
- **D4 I906 rows after this.** Recommended: finish the I906 rows in flight; new I906 rows only where command latency
  is visible (a page waits seconds for its data). Trade-off: with the engine thread a library wait no longer freezes
  the window, it only delays the answer; continuing I906 everywhere spends agent time on latency the user may not see.

## Gaps (not verified in this design)

- Whether `null` has a paced mode for I1082a (checked only `SEL_OUT_UNPACED` use in main.c:262, 712).
- `sel_spsc` memory ordering on arm64 (src/core spsc not read).
- Whether `sel_library_task_cancel_request` is lock-free (class H).
- Snapshot size: not measured; I1082d measures before choosing the copy granularity.
