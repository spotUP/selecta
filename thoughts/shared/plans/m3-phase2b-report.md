# M3 Phase 2b report (I149, I150, I151): PARTIAL, red tests only

Plan: thoughts/shared/plans/2026-10-25-m3-library.md, Phase 2.

## State: 0 of 3 rows implemented, 3 of 3 red tests committed

| Row | Test | State |
|-----|------|-------|
| I149 LIBRARY_SEARCH, LIBRARY_RESULT tags, REGEXP | T64 library-search | red committed, code open |
| I150 PLAY_QUERY, QUEUE_ADD_QUERY, VIEW_SAVE | T65 play-query | red committed, code open |
| I151 VIEW_SOURCE query, style switch keeps key | T66 view-library-source | red committed, code open |

Commit: "Add T64 T65 T66 red tests ..." (tests/CMakeLists.txt end block, tests/m3/*).
All three fail on build/host (CLI has no `library search`, no `--catalogue`, no `--exec`).
`play_query.expected` and `view_query.screen` are empty placeholders: generate them after green and review by hand.
tests/ratchet/unimplemented.txt stays 21; expected 18 after the work (LIBRARY_SEARCH, PLAY_QUERY, QUEUE_ADD_QUERY done; VIEW_SAVE, VIEW_SOURCE born implemented).

## Remaining design (not started)
1. src/library/regex.c: small case-insensitive backtracking matcher (no groups or alternation), registered as SQL regexp() in sel_library_open.
2. include/selecta/libhook.h: sel_engine_library hook (search, save_view) in sel_engine_config.
3. src/library/db/search.c: job request through sel_library_call; bare words use FTS5 prefix MATCH (fts_queries sentinel), other queries use sel_query_compile plus ORDER BY songs.key; TSV catalogue import; view resolution from the views table.
4. Engine: second queue for results, LIBRARY_SEARCH/PLAY_QUERY/QUEUE_ADD_QUERY/VIEW_SAVE/VIEW_SOURCE handlers, SEL_VIEW_QUERY source, add rows VIEW_SAVE "ss" and VIEW_SOURCE "s" to commands.def, SRC_FTS tag.
5. CLI: `library search`, `script --catalogue`, directives @results and @library_stats. App: `--catalogue`, `--exec`, exit line `view_source= view_style= cursor_key=`.

## Deviations planned
Regex subset; FTS token-prefix instead of LIKE substring for bare words; results ordered by key; VIEW_SOURCE playlist/home unsupported; no per-row events (accessors instead).

## Harness note
The sandbox refused compound Bash commands (literal git path text, git chained with cd, large heredoc batches); use Write or small single commands.
