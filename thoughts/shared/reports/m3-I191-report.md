# I191 home view source (T91)

Plan: thoughts/shared/plans/2026-10-25-m3-library.md, row I191, test T91 (`home-view`).

## What works
- `VIEW_SOURCE home` and key `H` show a Home view with three sections: Recently played (distinct songs, newest play first, limit 10), Jump back in (albums with a skipped play whose song has no finished play, limit 10), New this week (modland meta count `modland.new_this_week`).
- Data comes from `sel_library_home` (src/library/db/db.c, user.c op `SEL_USER_HOME`); the engine (`home_reload`) feeds `sel_view_set_home` (src/engine/view.c) into the shared text-row storage used by the folders source. The text UI renders the rows and the "Home" title.
- Sentinel `home_view home_rows=N` (main.c) = recent entries + jump entries + new-this-week count.

## Tests
- `home-view`: runs `tests/m3/history.sel` through selecta-cli (one finished play, one skip), seeds an album, a later play and the meta count with sqlite3, counts 2 + 1 + 7 with independent SQL, requires `home_rows=10` from the real `selecta` binary, and compares the screen golden `tests/m3/home_view.screen`.
- Fail before the fix: with the engine reload treating the library reply as failure, the run printed `home_rows=0, want 10`. The old stub mapped home to the query source and had no sentinel.
- `view-library-source` updated: home is now its own source (`view_source=home`).
- Full `ctest -L m3`: 48 of 48 pass. Amiga 68020 build compiles.

## Open
- Enter on a home row does nothing: rows are display text without song keys. Needs key storage per row.
- "Jump back in" definition (skipped, never finished) is my choice; the plan does not define it.
