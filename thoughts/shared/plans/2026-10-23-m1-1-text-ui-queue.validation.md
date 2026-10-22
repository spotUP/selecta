---
date: 2026-10-23
topic: M1.1 validation (T19 to T27 and T8)
tags: [validation, m1, text-ui]
status: in-progress
plan: thoughts/shared/plans/2026-10-23-m1-1-text-ui-queue.md
---

# M1.1 validation

## Full run (2026-10-23, Debug, macOS arm64, landing M1.1)

`cmake --build build/host -j 2` green; `nice -n 19 ctest --test-dir build/host -j2`: `100% tests passed, 0 tests failed out of 48`; `cmake --build build/amiga -j 2` green. No test SKIPPED in this run. Reason for the full run: landing the milestone (plan Verification).

Open: I61 (real-terminal check on macOS, only the user can tick it). Open: the listening check behind the openmpt golden hash (I55): the hash `tests/golden/openmpt/selecta-test-mk.sha256` is committed (`fda8a12`) and `render-golden-openmpt` passes against it, but this repo records no user confirmation that it was listened to. Open from M0b and unchanged: the nuade pinned hashes (I34).

## Reconciled counts (each from its source)

- ctest total: 48 (`ctest --test-dir build/host -N`, "Total Tests: 48").
- Ratchet unimplemented count: 45 (`tests/ratchet/unimplemented.txt` lines, equals `selecta-cli commands --unimplemented`; limit in the T8 row is 45).
- no_float objects: 12 (`cmake/no_float.cmake` asserts `nobjs EQUAL 12`: 6 engine, 2 mixer, wav, null, 2 library).
- nuade players: 121 (`ls third_party/nuade/players/*/nuade_player.cmake | wc -l`; `nuade-players` test compares it with the registry walk).
- Open rows: `grep -c '\[ \]' thoughts/spot/todos.md` is 5 after this row (I61 is the M1.1 one); plan statuses: M1.1 in-progress (I61), design doc draft.

## T19 selecta-browse-play

Observed: passed. Last screen equals `tests/m1/browse_play.screen` (title `selecta test`, `00:02 / 00:30`, info panel); sentinels `songs_opened=1 queue_version=1`, K equals the keys-file count. Text differs from the original plan row (plan Deviations I60). Pass.

## T20 queue-engine

Observed: passed (`selecta-cli script tests/m1/queue.sel` against `queue.expected`: ids, PLAY_NEXT, NEXT past end, PREV 3000 ms rule, auto-advance, version +1 per edit). Pass.

## T21 seek-render

Observed: `seek-render` (nuade, byte-exact tail, `seek_path=skip`, `SEEKED 220500`) passed. `seek-render-openmpt` and `seek-render-openmpt-break` passed: openmpt uses `seek_path=native`, not byte-exact by user decision (I56); the check is the landing position (row 41 equals the straight render). Pass with that accepted deviation.

## T22 render-golden-openmpt

Observed: passed; `replayer=openmpt`, `rendered_frames=441000`, hash equals the committed file. Pass. Pending: listening confirmation (I55), see above.

## T23 nuade-players

Observed: passed; `nuade_players=121` equals the glob. Pass.

## T24 view-unit

Observed: passed. Pass.

## T25 grid-unit

Observed: passed (flush, full, keys, theme and term groups). Pass.

## T26 no-alloc-render

Observed: `no-alloc`, `no-alloc-render`, `no-alloc-render-openmpt`, `no-alloc-render-queue` passed. Pass.

## T27 nuade-link-graph and link-warnings

Configure-time `FATAL_ERROR` check passed (the host configured); `link-warnings` passed. Pass.

## T8 ratchet

Observed: passed, 45 unimplemented (limit 45). Pass.

## Manual

I61: PENDING (user). Steps: plan "Verification" manual 1, LOCAL, `build/host/apps/selecta/selecta --start-dir ~/Music/mods`.
