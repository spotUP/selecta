# I265 Sources page report (2026-10-26)

Commits (local, not pushed): 446bc70 engine/library, 550d8e8 desktop UI + tests.

## What works
- Library page, sixth tab "Sources": one card per SOURCES row (name, Local/Remote, "N songs", on/off switch, Sync button for enabled remote sources). No source ids in the UI code.
- Pref `library.sources_off` (CSV of disabled ids, PREF_SET). SOURCES reports SRC_ENABLED from it. Owning place: `sel_library_key_source_off` in src/library/db/search.c (collect() skips off sources); radio `admit()` rejects them.
- SRC_COUNT counts main songs plus idx.songs rows of the source not already in main (src/library/db/songdb.c, op_source_songs).
- Sync sends `LIBRARY_SYNC <key> "<path>"`, then SOURCES; toast at start and on the next SOURCES reply.

## Tests
- `sources-off` (host, label m3): PREF_SET off -> SOURCES SRC_ENABLED=0, search 5 -> 2 results, back to 5. Revert of the search.c skip makes it fail (verified).
- `dui-library-sources` (needs SDL): 8 rows with kind/name/count, golden hash, toggle row 1 -> pref changed and SOURCES reports 0. Revert of the PREF_SET write makes it fail (verified).
- Side effects of the sixth tab: library.cmake expects 6 tabs; library_style.cmake canvas 1280 -> 1440 (the labelled style chooser no longer fits beside six tabs at 1280, it falls back to icons; narrow case at 900 unchanged); grid-view and coverflow goldens regenerated (tab row only).
- `ctest -L dui -LE dui-red` in build/sdl: 56 tests, only the three above failed, all fixed and re-run green.

## Findings / open
- build/host has SELECTA_UI_DESKTOP=OFF, so SDL dui tests pass trivially (0.03 s) there. Real run needs `build/sdl` (cmake -DSELECTA_UI_DESKTOP=ON, Debug, Ninja; git-ignored).
- At 1280 px wide the labelled style chooser now collapses to icons: product trade-off of the sixth tab; Johan to judge.
- Radio is not a registered source (I410): not on the page.
- LIBRARY_SYNC is synchronous: UI stalls during Sync, no progress events. cmd_LIBRARY_SYNC hardcodes the source id list until I517.
- Search overflow count (count_rest) is not source-filtered.
- idx.songs count assumes columns `source` and `key`; no test covers it.
- Sources rows are not scrollable (max 16 rows).
- Two whitespace-only residue hunks in desktop_ui.c (np_like, np_rating) left unstaged on purpose.
