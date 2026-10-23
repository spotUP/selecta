# M3 Phase 2c report (I149, I150)

## Rows
- I149 done: LIBRARY_SEARCH through the library worker, LIBRARY_RESULT row tags, REGEXP SQL function.
- I150 done: PLAY_QUERY, QUEUE_ADD_QUERY, VIEW_SAVE.
- I151 (VIEW_SOURCE, T66 view-library-source) not done by design; left to a later agent. T66 stays red.

## Tests
- T64 library-search: green.
- T65 play-query: green.
- T66 view-library-source: red (I151).
- protocol, abi, ratchet, tag, codec, library-db-open, query-compile: green.
- Host and amiga builds: clean.
- Unimplemented command count: 21 to 18 (tests/ratchet/unimplemented.txt).

## Deviations
- Regex subset: literals, `.`, classes, escapes, anchors, `* + ?` after one atom. No groups, no alternation.
- Bare words use FTS5 token-prefix MATCH, not LIKE substring. Other queries use sel_query_compile with ORDER BY songs.key.
- No per-row events. Rows are read through the accessor `sel_engine_result_key()`. `sel_engine_config.library` is a pointer to `struct sel_library`, not a libhook.h hook.
- Relative-time `now` comes from `time(NULL)`.
- SRC_KEY tag not added; SRC FTS (0x7002) added.
- selecta-cli: `library search --catalogue <tsv> -- <q>`, `script --catalogue`, directives `@results`, `@library_stats`.
- App `--catalogue` and `--exec` options and VIEW_SOURCE are left to I151.

## Notes
- The ERROR event keeps the pointer to its code string, so the engine copies library error codes into a ring (`lib_err`).
- Harness sandbox refused compound shell commands; single commands used.
