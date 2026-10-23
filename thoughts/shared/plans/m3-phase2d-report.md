# M3 Phase 2d report (I151)

## Done
- VIEW_SOURCE "s" (commands.def, engine.c): `query` snapshots the last LIBRARY_SEARCH result (sel_engine_result_key) into a second queue `qview` and switches the view to the new source SEL_VIEW_QUERY; `queue` and `browse` switch back; `playlist`/`home` answer ERROR unsupported.
- view.c treats every non-browse source as a list, so tree, drilldown and the cursor-keeps-its-song behaviour are reused unchanged over `qview` (view_src picks the queue by source). Enter on a query row adds the key to the queue and starts it.
- Text UI: `v` and the style keys work in the query view; heading "Query N results"; playing marker only for the queue source (ids collide otherwise).
- App `selecta`: `--catalogue <tsv>`, repeatable `--exec "<command>"`, exit line `view_source= view_style= cursor_key=`. sel_cli_open_catalogue moved from selecta-cli/library.c to apps/common/catalogue.[ch] (shared).
- Accessors: sel_view_source_label, sel_engine_view_cursor_key.
- Fixtures: tests/m3/view_query.screen filled from the real output; play_query.expected now has VIEW_CHANGED/VIEW_ROWS (0 rows) instead of unknown_command VIEW_SOURCE.

## Tests
T66 view-library-source green; play-query, library-search, ratchet, protocol, abi, text UI, queue and view suites green (26 of 26 in the affected set). Host and Amiga builds compile. VIEW_SOURCE was never in unimplemented.txt (the verb did not exist), so the ratchet count stays 18.

## Open questions
1. qview allocates a second queue of the same size as the main queue (Amiga fast RAM). Default: keep. Alternative: cap it at a smaller row count.
2. The query view is a snapshot; a new LIBRARY_SEARCH needs another VIEW_SOURCE query. Default: snapshot. Alternative: live follow.

## Remaining
VIEW_SOURCE playlist/home (needs M4 playlists), per-row library columns in the query view (title, artist from the catalogue instead of the key path).
