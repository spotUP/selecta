---
date: 2026-11-01
topic: I889 dui-search-online flake, first investigation
status: fixed
---
# I889: dui-search-online flake, first investigation (Sonnet agent, no fix)

## Reproduction
Fails on the unmodified build under load (load average 44-60, or 6 `yes` loops) and about 1 run in 4 without extra load. Failure modes:
- tests/dui/search_online.cmake:70 `scope=- searches=0`: the pass 2 chip click was lost.
- :93 `plays=0`: the pass 3 row click was lost.
- :116 (only with the `|| d->src_wait` hold): after the restart the modulespl source shows `|0|0`, not 1 pinned song.

## Findings (plugins/ui/desktop/desktop_ui.c line numbers approximate)
- The initial SOURCES request comes from `desk_wait_turn` (`nag_asked`, ~14294) and sets `src_wait=1`; the first SOURCES row clears it (~14169). `--view search` sends no other SOURCES request.
- `desk_replay_input` (~13149) runs `wait 300` and then the click without waiting for that answer. Both holds (~13297, ~14424) test only `on_state == 1u`.
- The chips do not exist at click time: `online_scope_count` needs `src_n >= 2`. Pass 1 only supplies chip coordinates.
- After the answer `src_n` is 15 or 16 (engine caps SOURCES at 16, `sources_row`, src/engine/engine.c ~4516). `sel_engine_reads_pending` is 0 right after the answer.
- With a `|| d->src_wait` hold, passes 1-3 pass but pass 4 fails 3 of 3: the dump is taken after the first SOURCES answer, which carries song count 0. `cmd_SOURCES` (engine.c ~4565) sets `sources_retry` when rows went out without counts, and `lib_reads_collect` resends them; the dump does not wait for that resend. The unmodified build passes pass 4 only by timing.
- Unchecked: a retry SOURCES answer arriving while `src_wait` is 0 appends rows (`d->src_n++`, ~14175) instead of replacing them.
- Pattern to reuse: `desk_side_settle` (~11282, I787) settles the sidebar read at the end of a `wait` by polling the worker. A SOURCES equivalent is missing.

## Open question for the next agent
Why is the pass 4 song count still 0 when `sel_engine_reads_pending` reads 0? Possibly the library has not finished opening when the first answer is built.

## Proposed direction (not tested)
- Engine exposes "SOURCES fully answered, including the counted retry" (for example a settled flag the UI can read).
- Replay holds on it in both replay points, besides `on_state == 1u`, with the existing 20000-nap bound.
- Also hold on drawn rows before a row click (`on_state == 2u` until `on_drawn > 0`) for the :93 case.

## Root cause and fix (second agent, Opus)
- Passes 2, 3 and 5 (:70, :93, :128) are one failure: the replay clicked the scope chip before the first SOURCES answer drew the chips, so the search ran in scope `-`, no online row existed, and the row or radio click hit nothing (red logs show `online scope=- searches=0` at :93 and :128 too). No separate row-drawn race was seen.
- Pass 4 (:116): when the first SOURCES answer met a busy library, its rows went out with count 0 and `cmd_SOURCES` set `sources_retry`; `lib_reads_collect` then resent the full answer. The desktop handler (`desk_on_event`, SOURCES branch) appended every row of an answer that arrived with no request out (`src_wait == 0`): with 16 rows held, `d->src_n < 16` failed and the resend was dropped; with 15 held, only its first row landed at index 15. So `sel_engine_reads_pending` read 0 (the resend had run) while the held count stayed 0.
- Fix: the engine exposes `sel_engine_sources_pending` (the counted resend is still to come); the desktop holds a scripted turn (`desk_wait_turn`, 20000-nap bound) and the replay's next line while a SOURCES request is out or that resend is pending (`desk_src_pending`); a SOURCES row with no request out replaces the held row of its key (`src_slot`) instead of appending.
- Proof: red 4 of 6 before the fix under 6 `yes` loops; green 10 of 10 after under the same load.
