# M3 I205 / I206 report: text UI library screen and persistent --db

Basis: architecture design sections 9b and 9c, M3 plan rows I151 and T66.

## Changed

- Tab cycles Browse, Queue, Library. `L` jumps to Library. `/` opens a search prompt (reuses the command line) and runs LIBRARY_SEARCH. Enter plays from the results. `a` adds to the queue.
- The screen opens on all songs, tree style, grouped by format. `v` still cycles styles.
- Empty library shows: "Library is empty: scan a folder with :LIBRARY_SCAN <folder>". A non-empty query without rows shows "No songs match the search".
- `apps/selecta/main.c` opens a persistent DB: `--db <path>`; default `~/.config/selecta/library.db` (directory created) on desktop, `PROGDIR:library.db` on Amiga. `--catalogue` wins when given.
- New stats `STAT_LIBRARY_OPENS` (0x5019) and `STAT_LIBRARY_SEARCHES` (0x501A); exit line `library_screen opens=N searches=N`.

## Files

include/selecta/tags.def, plugins/ui/text/text_ui.c, plugins/ui/text/keybind.c, apps/selecta/main.c, tests/CMakeLists.txt, tests/m3/library_screen.{cmake,keys,screen}, tests/m3/library_db.keys.

## Keys

- `L`: library action. `/`: search action. Tab: third stop.
- `L` used to toggle the filter LED. That binding moved to `M-l` (not verified by hand that M-l parses in keybind.c).

## Tests

T98 `text-ui-library-screen` (label m3): part 1 golden screen plus sentinel (`opens=1 searches=2`, `view_source=query`, `songs_opened=1`); part 2 `--db` with LIBRARY_SCAN, asserts rows and the db file. Affected ctest subset (view, text-ui, library, keybind, palette, cmdline, selecta-, reach, browse, bindings, query): 24 of 24 passed, 1 skipped (rig-selecta-upterm). `selecta-cli commands --unimplemented | wc -l` is 17.

## Not done / caveats

- Palette and help overlay list engine commands only, so the `library` and `search` local actions are not listed there. Labels appear in the library screen hint line only. The long hint line on other screens ("Tab Queue") is unchanged because 14 goldens contain it.
- The default DB is skipped for scripted (`--keys`) or null-output runs, so tests never touch the user's DB. Deliberate deviation.
- Search entries go into the shared command-line history.
- Amiga build not tested.
- No dedicated tests for the empty-library hint, the `a` add path, or default-path creation in a real home directory.
