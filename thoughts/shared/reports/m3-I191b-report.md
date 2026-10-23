# I191b: album names with quotes open from the home view

Fix level: the query grammar. `parse_term` in src/library/query.c is the single parser for every quoted value
(`key:"..."` and bare `"phrase"`). Inside quotes, `\"` is a quote and `\\` a backslash; any other backslash stays literal, so
existing queries are unchanged. The engine (`cmd_VIEW_ACTIVATE`, src/engine/engine.c) escapes the album name it takes from
the row with the same two rules before building `album:"<name>"`.

Why not the album id: the library has no album table or id; `songs.album` is a plain text column and the home row
(`S @<album>`) is a name by design. An id would need a schema change. The grammar escape fixes every quoted search
(search box, saved views, play-query), not only home rows.

Test:
- tests/m3/home_view.cmake: renames the only Jump back in album to `The "Quoted" Album \ Two`, presses Enter on the row
  (home_album.keys), asserts view_source=query, songs_opened=0 and that only "mk tune" is listed. home_view.screen unchanged.
- tests/m3/query.golden: row `album:"say \"hi\" \\ x"` with the unescaped bound parameter.

Fail-before (source reverted, tests kept): query-compile failed (the old parser split the value at the first inner quote
and compiled extra bare-word terms); home-view failed with "album name with quotes did not list its songs".
After: home-view, query-compile, library-search, play-query, view-library-source pass. Host and Amiga builds clean;
`commands --unimplemented` count is 8.

Limit: a quoted value longer than 95 bytes after unescape fails with too_complex (bound parameter limit
SEL_QUERY_TEXT_MAX 96, unchanged); home keys can be up to 127 bytes, so very long album names still do not open.
