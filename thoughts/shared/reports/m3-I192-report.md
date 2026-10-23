# I192 Stats SQL views and text UI stats screen (F24, T92)

Design refs: architecture-design F24 row and "Stats screen (F24)" bullet; plan Phase 10.

## Changed
- Schema v4 (`src/library/db/schema.c`): views stats_plays, stats_totals, stats_periods, stats_format_counts,
  stats_artist_counts, stats_top_formats, stats_top_artists. Period = UTC year of played_at; skipped plays excluded;
  missing artist/format/duration count as Unknown/0; ties ranked by name.
- Library op SEL_USER_STATS / `sel_library_stats` builds "label\0value\0" rows from the views only.
- Engine commands STATS_SHOW / STATS_HIDE, `sel_state.stats`, counter `stats_queries` (printed by `selecta` as
  `stats_screen stats_queries=N`).
- Text UI: key `b` toggles the "Statistics" screen (Escape and Backspace close it); it renders rows only.
  Screen shows totals plus the newest 3 years with plays, play time and top 2 formats and artists (fits 80x25).
- Test `stats-screen` (tests/m3/stats_screen.cmake, label m3): seeds a DB with skipped plays, an orphan history key,
  NULL artist and duration, ties; runs the real `selecta` with `tests/m3/stats.keys`; requires `stats_queries=1` and
  every expected row, computed by direct SQL over the base tables, on the screen dump.
- `library-db-open` now expects schema_version 4. RULES.md Commands row added.

## Results
- Host: stats-screen and library-db-open pass; text-ui, metadata-panel, keybind, command, ratchet, folder tests pass.
- Amiga build (build/amiga) compiles.
- Fail-before: with a keys file lacking `b`, `selecta` prints `stats_queries=0` and the test fails its
  `stats_queries=1` requirement. Early bug found by the test: the op had no key and hit the MISUSE branch.

## Found
- `selecta-m13-reach` fails independent of this change: golden tests/m13/reach.screen has the browse footer, the
  code prints the player footer when viz is on (commit d0c5c16).
- Key `b` may collide with the I191 home view key choice.
