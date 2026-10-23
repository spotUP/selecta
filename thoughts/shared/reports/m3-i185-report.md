# M3 I185 report: metadata panel

Design doc: `thoughts/shared/plans/2026-10-22-architecture-design.md` lines 63 and 83. Plan: Phase 8, row I185, test T87.

## Done
- `src/library/db/user.c`, `db.c`, `db.h`: worker op `SEL_USER_PANEL` and caller `sel_library_song_panel`. One query returns label/value pairs: STIL note (sticker `stil.comment`), Composer (`song_composers.display`, grouped), Release (`songs.album`), Year, Genre, Rating, Play count (history rows with `skipped=0`). Empty, NULL or 0 values add no row.
- `src/engine/engine.c`, `include/selecta/engine.h`: the pump thread reloads the panel when the song changes (not the audio thread, which runs `hist_begin`). `sel_state` gains `panel` and `panel_rows`; stats gain `panel_loads`.
- `plugins/ui/text/text_ui.c`, `tags.def`: the Information view draws the panel rows after the replayer rows; stat `STAT_META_PANEL_ROWS` (0x501C) counts them.
- `apps/selecta/main.c`: prints `meta_panel panel_loads=N meta_panel_rows=N`.

## Test
`metadata-panel` (label m3, `tests/m3/meta_screen.cmake`): imports the catalogue, seeds selecta-test.mod through sqlite3 with HVSC-like data, opens it in the real selecta binary and presses `i`. Expects 7 rows plus a screen golden; selecta-test-mk.mod (no catalogue data) shows 0 rows and replayer metadata only. Passes. The sentinel line did not exist before the change, so the regex fails on the old binary.

## Checks
- Host build clean, Amiga build clean (603 steps, 0 errors).
- `selecta-cli commands --unimplemented`: 10 before and after.

## Caveats
- Fixture is a local playable mod seeded by sqlite3; the HVSC fixture sids are listing-only.
- STIL row uses the file-level sticker only; per-subtune `stil.<n>.*` notes are not shown.
- Release comes from `songs.album`; rating is the raw integer; play count excludes skipped plays.
- Long values are cut at the panel width.
