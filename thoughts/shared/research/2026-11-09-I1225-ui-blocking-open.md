---
date: 2026-11-09
row: I1225
status: fix landed on worktree branch (desktop); text/MUI noted only
---

# I1225: UI blocks (beach ball) while a remote song or system file opens

## Reports (Johan, Mac)

- "hvsc plays now after a brief beach ball"
- "nert tracker works but i got a beach ball when it downloaded the needed file (not the song)"
- "clicking a new nerdtracker song is slow and throws a beach ball"

## Violated invariant

The UI thread never waits on network, system-file download, or engine open/decode. That work runs off the UI
thread, and the UI shows a loading state.

## Root-cause layer

Engine, not UI. On the desktop the engine pump runs on the UI thread (`apps/selecta/main.c` pump_turn calls
`sel_engine_pump`). A song start (`begin_song`, and the queue-next path) runs `prepare_slot` on that pump, and
`prepare_slot` calls `read_song`. For a remote key (hvsc:, modland:, any `sel_cache_is_remote` key), `read_song`
called `sel_library_fetch` synchronously (`src/engine/engine.c` read_song, the remote block, about line 2067
before the fix). `sel_library_fetch` (`src/library/db/fetch.c` about line 519) downloads the whole file and its
companions and holds `cache_lock` for the whole transfer. The UI froze for the full download.

## Measurements (instrumented, cold cache, desktop SDL loop, fixture mirror)

New engine stats (`open_phases` line in `--stats`): `open_fetch_ms_max` (sync fetch on the pump inside
read_song), `open_rp_ms_max` (replayer open on the pump), `open_prepare_ms_max` (whole prepare_slot on the
pump), `open_fetch_reads` (fetches done on the song-start read slot).

| Case | Before fix | After fix |
|------|-----------|-----------|
| Remote song, mirror answers after 4 s (song.mod, first run) | open_fetch_ms_max=3025, open_prepare_ms_max=3025 (fetch gave up at its first-byte timeout, all on the UI thread) | n/a (fixture changed, see test) |
| Remote song, mirror answers after 2 s (slowplay.mod) | open_fetch_ms_max=2013, open_prepare_ms_max=2014 | open_fetch_ms_max=0, open_prepare_ms_max=2, open_fetch_reads=1, song plays |
| Sysfile probe, sysfile answers after 2 s | download already async (job thread, `sel_sysf_job_run`); the reopen went through read_song again | open_prepare_ms_max=2, installed=1 reopens=1 |

The blocking call per case:

- hvsc / modland song: `read_song` -> `sel_library_fetch` on the pump. ms = the whole transfer (2-3 s measured
  on the fixture; on a real mirror it is the full download time).
- .ned song (bbmp_ned): the song file itself goes through the same `read_song` -> `sel_library_fetch` path, so
  the same fix covers it. The nt2replay.bin/.lbl system file download is already off the UI thread
  (`sel_library_sysfile_request` posts to the job thread; `sf_poll` polls; `sf_landed` reopens). The beach ball
  Johan saw during "the needed file" download is the song fetch plus the reopen's second open on the pump, not
  the sysfile transfer. UNVERIFIED for .ned specifically: this worktree's base lacks bbmp_ned (b74c14a7) and the
  I1218 hvsc mirror fix (4e2ff2d9), so the .ned replayer open CPU time (open_rp_ms_max for nt2replay) was not
  measured. Check it on main with `--stats` (open_phases line).
- Replayer open/decode CPU (`open_rp_ms_max`): runs on the pump. Measured 0 ms for the fixtures here. Heavy
  replayers (song-length pre-scan, .ned) can still cost the UI thread; that is I1082 (engine off the UI thread).

## Fix

Reuse of the I906g song-start read (no new thread mechanism):

- `play_meta_run` (runs on the `rd_play` read slot, off the pump) now also performs the remote download for the
  key the start will open (the best source when there is one), into the `sel_play_meta` entry: `fetch_key`,
  `cached`, `files`, `fetch_rc`, `fetch_status`, `banks_missing`, `fetched`.
- `play_meta_ready` fills `e->pm_rules` (companion rules from the registry) on the pump before posting the read,
  so the read thread does not walk the registry.
- `read_song` takes the cached file from the entry when it covers the path; no network call on the pump. A path
  no song-start read covered still uses the old sync fetch, timed into `open_fetch_ms_max` so any remaining
  pump fetch is visible.
- `begin_song` already defers (`pm_wait`, `deferred_path`) until the read lands, so the UI keeps painting and the
  existing loading state shows. The queue-next path also calls `play_meta_ready` before `prepare_slot`, so
  gapless/next starts are covered too.
- After a sysfile wait, `play_meta_drop` drops the entry, so the reopen re-reads; the second fetch is a cache hit
  on the read thread.

## Regression test

`dui-open-async` (`tests/net/open_async.cmake`, labels m3;dui): desktop SDL loop (dummy video, null paced
output, no audio).

- Leg (a): `OPEN modland:Protracker/test/slowplay.mod` on slow.mirror.test (first byte after 2 s). Asserts
  pump_ms_max < 250, open_prepare_ms_max < 250, open_fetch_reads >= 1 (sentinel: the fetch ran on the read
  slot), songs_opened=1.
- Leg (b): probe.sfp whose system file arrives after 2 s. Asserts the same bounds and
  `fetches=1 installed=1 reopens=1`.
- `--clock fixed` freezes the frame clock, so pump_ms_max alone reads ~0; open_prepare_ms_max uses the host wall
  clock and is the binding bound.
- Fail before: with the read-slot fetch disabled, leg (a) fails with open_prepare_ms_max=2014
  (open_fetch_ms_max=2013). Pass after: 2 ms.

Fixture delays are 2 s because the fetch first-byte timeout is about 3 s (a 3-4 s delay made the fetch fail).

## Known limits / left open

- One `rd_play` read at a time: a second click while a slow fetch runs waits for that fetch before its own read
  posts. The UI stays responsive; the second song starts later. Cancelling the in-flight fetch is a follow-up.
- `sel_live_drain` (live streams with non-streaming replayers) and replayer open/decode CPU still run on the
  pump: I1082.
- Amiga: `lib_read_post` runs inline (no read thread), so the fetch still blocks there; no gain, no loss.
- Text UI and MUI: the fix is in the engine; any UI that pumps the same engine gets it (text UI on host, MUI on
  host builds). Not tested separately per the row; no text/MUI test added.
- .ned replayer open time: unmeasured in this worktree (see above).
