---
date: 2026-10-25
author: spot
status: in-progress
topic: Selecta finish-line run
---

# Selecta finish-line run: handoff

Ledger: `thoughts/spot/todos.md`, 347/415. Origin main = f1d64eb.
Finish line: I202 (finish-line run) green, every ledger row closed except the
Johan-only rows I138, I203 and I248.

## Landed this session (all pushed)

- I290 (c74c189): control code points never paint; text wrap breaks on LF/CR/CRLF; Now Playing message wraps to 6 lines.
- I293 (5686bee): `find_replayer` picks highest SEL_RP_PRIORITY among YES, then UNSURE answers.
- I294 (6687397): queue.expected and playlist.expected were stale after bbdc6dd (POSITION gained order/row/pattern). Not a code bug.
- I272 part (a312f1b): `draw_button` primitive (desktop_ui.c ~432, radius token 6 px) and `desk_hover_move` on real SDL motion. Covers chips, page buttons, tabs, viz switch, Add Folder, sidebar doors, tiles. Test: dui-nav section 10.
- I124 findings in its row (8d66f0b): stutter is nuade on non-M.K. modules; M.K. goes to kplayer and runs realtime.

## Open, in order

Mac track (one desktop_ui.c agent at a time):
1. I272 remainder: transport buttons, seek and volume sliders, menu items, search field, Now Playing controls, pointer cursor; fold in I244 tooltips. Reuse `draw_button` and `hover_fill_of`.
2. I291 Mac UI lag: measure input-to-present latency, frame interval, pump/draw cost, vsync, event-wait timeout in backend_sdl.c before any fix.
3. I292 spectrum flicker (both UIs): suspect sel_viz_acquire/analyse in refresh_snapshot (engine.c). Test: snapshots never drop to zero.
4. P4-14, P4-15, I265(a)+I273+I281, I278 (P4-12 remainder). Fold in I253-I255, I256-I261, I264, I267, I269.

Amiga track:
1. I124: retest with an M.K. module Johan names. Possible design question: lighter path than nuade for non-M.K. on 060.
2. I286 / DUI-P6-4: research doc `thoughts/shared/research/2026-10-25_i286-amiga-readiness.md`; find `___divdi3`/`___udivdi3` callers.
3. I289 then I287 (views to src/gui/views; extend dui-m68k-nofloat).
4. I277 (SEL_VAMOS_RAM_KB 64 MB; update memory amiga-baseline-has-fast-ram), FS-UAE I251/I219i.

Then: I285 YouTube design (needs Johan approval), I230 slices 2-3, I202.

## Gotchas

- Worktree agents symlink third_party/nuade and third_party/libopenmpt; remove the symlinks and recreate empty dirs before `git worktree remove`.
- `wasabi run` of selecta-cli starves wasabid ~25 s; use `run --detach` and read T:wasabi-bg-N.
- Do not remove `.claude/worktrees/agent-aa732a611176357ff` (unknown origin).
- Live Mac window pid 69107 is this session's (scratchpad live.db); Johan's windows are never killed.
- Perl ledger edits: use `[^\n]*`, not `\s*$` under /m.

## Pending Johan checks

Mac: hover on chips, buttons, tabs, doors, tiles (I272); Now Playing sample names as lines (I290); tracker roll and centred bar; Add Folder layout and toast; drag and drop; channel header mute; Add to Queue toast and Queue page; viz switch buttons; P4-6 to P4-9 looks.
Amiga: name an M.K. module that stutters (I124).
Others: I219h, I218d, I218e, I242, I248, R10 retest, PR #81 in ~/Code/devilbox-selecta, mpris error, detail-view title shift, I138, I203, Monday BMS pass.

## Update 2026-10-25 evening

- Johan's "hover does nothing" report narrowed to the Search chips only: they had no click handler (I256, never built). Hover and clicks work elsewhere. I256 landed in c22a6a4: engine view source `search` (SEL_VIEW_SEARCH) gives Songs, Albums and Artists groups with counts; chips All/Songs/Albums/Artists filter; Home chips navigate. Johan has not yet confirmed it live (Mac).
- Known gaps from I256: artist rows have no key (activating does nothing); album and artist distinct counts saturate at 256; each group lists at most 16 entries. Fixed an existing overflow: `cursor_of`/`id_of` in src/engine/view.h were [4] with home at index 4.
- I291 measurement: SDL present takes median 0 ms, p95 1 ms, max 6 ms over 4440 frames, so the lag is between input and redraw (loop wait, tween frames), not in present.
- da744c9: three more m12 goldens (history, album, random) were stale for POSITION order/row/pattern, same class as I294.
