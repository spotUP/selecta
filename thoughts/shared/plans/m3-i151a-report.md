# M3 I151a report: query view rows show title and artist; VIEW_SOURCE playlist and home

Basis: architecture design 9b (view model, F48 title format), M3 plan rows I151, T66, I191.

## Changed

- The library search job (`sel_lib_job`) now returns row metadata next to the keys: `meta` holds, per key, title, artist, format and duration ("m:ss"), filled by `collect_meta` in src/library/db/search.c (one lookup per key, same for FTS and query paths). Note: LIBRARY_RESULT itself carries only counts and never had row tags, so the data comes from the job, not the event.
- Engine: `run_search` passes the buffers; `VIEW_SOURCE query` snapshots them (`qmeta`, `qmeta_off`) together with the qview keys. `sel_view_src` gets `meta`, `meta_off`, `meta_rows`.
- View model: `queue_row_text` in src/engine/view.c fills `%title%`, `%author%`, `%format%`, `%duration%` from the metadata, then the existing `sel_format_expand` with the `view.title_format` pref runs. No second formatter; the text UI is unchanged. Default format `[%author% - ]%title%` gives "lizardking - test tune"; `%duration%` and `%format%` work through the pref.
- `VIEW_SOURCE playlist`: alias of the queue source (playlist folders are later work). `VIEW_SOURCE home`: defined source that runs the query `plays:>0` (songs with a play in history) and shows it as a query snapshot; no library gives an empty list. I191's sections (recently played order, jump back in, new this week) are NOT done. Home overwrites the shared last-search result.

## Golden diffs

- T66 tests/m3/view_query.screen: `   selecta-test-mk.mod` -> `   lizardking - mk tune`; `   selecta-test.mod` -> `   lizardking - test tune`.
- T98 tests/m3/library_screen.screen: `       selecta-test-mk.mod` -> `       lizardking - mk tune`; `       selecta-test.mod` -> `       lizardking - test tune`.

## Tests

view_query.cmake (T66) extended: VIEW_SOURCE playlist and home run, no error, home lands on query with 0 results, playlist shows the queue. Ran `view|text-ui|library|queue|golden|format`: 22 of 22 passed. `selecta-cli commands --unimplemented | wc -l` = 17.

## Open

- Home has no ordering by last played and no history-backed test (needs a catalogue with history rows).
- Query view ordering is still by key; sorting by title is not done.
- Amiga build not tested; two extra buffers of queue text size are allocated (res_meta, qmeta).
