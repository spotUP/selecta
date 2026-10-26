---
date: 2026-10-27
topic: Asynchronous cover art loader for the Amiga desktop UI (I380)
status: design, not implemented; waits for Johan (Q-I380 in thoughts/spot/questions-2026-10-27.md)
ledger: thoughts/spot/todos.md row I380
reference: thoughts/shared/plans m4-amiga-ui.md section 4.5; design doc 2.7 (DUI-P2-4)
---

# Asynchronous cover art loader (I380)

## Problem

Rig 2 `--stats` measured `art_ms_max` 714 ms: one `art_load` call blocks the UI thread for most of a second while it
decodes a cover. Budget (I380 done-when): `art_ms_max` under 40 ms on rig 2 while 3 covers load.

## What the code does today

- `load_tile_art` (plugins/ui/desktop/desktop_ui.c ~1018) runs before the tiles are drawn. It builds the queue path
  pointers and calls `art_load` synchronously, timing it into `d->art_ms_max`.
- `art_load` (plugins/ui/desktop/art.c ~153) starts the worker and joins it at once. The worker, per queue path:
  `src->find` (album id, library db), then `load_album`: cache hit returns; else `fetch` the `thumb144` blob (db) and
  unpack it; else `fetch` the `cover` blob (db), `desk_image_decode` (stb_image), centre crop, `sel_gui_image_scale`,
  `sel_gui_thumb_pack`, and `src->put` writes the thumb back (db).
- posix/art_thread.c runs the worker on a pthread but joins at once: the frame stays deterministic, no gain.
- amiga/art_thread.c has no thread: the job runs inline (I369).
- The worker calls the library (`sel_ui_art` from src/library/db/art.c, one sqlite connection shared with the engine)
  and writes the art cache. Today that is safe only because the caller is blocked for the whole job.

## Invariant

The UI thread never waits on a decode. Every library call and every art cache write happens on the UI thread; the
worker touches only memory handed to it for the job.

## Design

### Split the job into three phases

1. **Gather (UI thread, per title, cheap):** `find` the album; cache hit: done. Else `fetch` the `thumb144` blob and
   unpack it into the cache (no decode; thumb rows are small). Else `fetch` the `cover` blob and append a pending job
   `{album, blob, len}` to the job list. Gather stops after the first pending cover per turn when a job is already in
   flight (one decode at a time; covers are big).
2. **Decode (worker):** decode, crop, scale into a job-owned 144x144 buffer, pack the thumb into a job-owned buffer.
   No library call, no cache call. When done, set the job's `done` flag and wake the UI thread.
3. **Commit (UI thread, in desk_wait):** for a done job, `sel_gui_art_cache_put` + copy pixels, `src->put` the thumb,
   free the job buffers, mark the tiles holding that album dirty so the next frame redraws them only.

Why this split, not a lock: a mutex around the sqlite connection and the cache would make the UI thread wait on the
worker whenever both touch them, and the engine shares the connection; the split needs no lock at all, only a done flag
with one writer.

### API change (art.h)

- `art_load(a, titles, n)` keeps its name and becomes non-blocking: phases 1 and 3 only, starts a decode when one is
  pending and none runs. Returns the number of albums still pending.
- New `int art_poll(desk_art* a)`: phase 3; returns 1 when a tile changed (the caller redraws). desk_wait calls it every
  turn.
- New `uint32_t art_wake_signal(desk_art* a)`: Amiga signal mask for the backend's `Wait()` (0 on posix; the SDL loop
  polls on its tick).
- Deterministic mode for tests and goldens: `--clock fixed` (or a new `art_sync` flag on open) keeps today's behaviour:
  start and join at once, so every existing golden stays byte equal.

### Thread backends

- posix/art_thread.c: unchanged start; add `art_thread_done(handle)` (atomic flag) so `art_poll` does not block.
- amiga/art_thread.c: `CreateNewProcTags(NP_Entry, NP_StackSize 131072, NP_Name "Selecta art", NP_Priority -1)`. The child
  `Signal()`s the UI task with a bit from `AllocSignal(-1)` when the job is done, then exits. stb_image needs the big
  stack; priority -1 keeps the UI and audio ahead. Close waits for a running child (sets a cancel flag the decode loop
  checks between rows of the scale, then waits for the done signal).
- backend_rtg.c `Wait()` (line ~455) adds `art_wake_signal` to its mask so a finished decode wakes the loop at once.

### Tile redraw

Tiles without art draw the placeholder (today's NULL path). Commit marks only the rects of tiles whose album changed, so
the dirty pixel count of a commit frame is the tile area, not the page (dui-dirty stays green).

## Tests

- Reachability (one): `tests/dui/art_async.cmake`, host build, live loop with timed clock and a fixture db with 3 covers
  and no thumb rows. Asserts: `--stats art_ms_max` under 40 with a decode that takes longer (a test hook that sleeps in
  the worker, e.g. env `SELECTA_ART_TEST_DELAY_MS=200`), `art_decodes=3`, the tiles drawn after the commits differ from
  the placeholder frame, and a sentinel `art commits=3`.
- Where it lives: art.c unit test for the split (gather queues a cover, poll commits it, a second load is a cache hit,
  no `src` call happens from the worker: the fake `sel_ui_art` records the thread id of each call).
- Rig 2: `--stats art_ms_max` under 40 while 3 covers load (I380 done-when), `art_decodes` equal to the cover count.
- Must fail on today's main: art_ms_max equals the delay.

## Open questions (in the questions file)

- Q-I380: build it this way (recommended), or keep the Amiga inline and only cache harder?
- Placeholder while loading: today's empty tile, or a shimmer? Default: today's tile (no motion on Amiga).
