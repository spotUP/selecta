# I191c: long album names open from the home row

Plan: 2026-10-25-m3-library.md section 9.2, T63 query-compile; design doc sections 9 and 9b.

## Cause
SEL_QUERY_TEXT_MAX was 96, so a quoted value over 95 bytes failed `too_complex`. Home row keys and album
names reach 127 bytes (SEL_VIEW_KEY_MAX 128 incl. NUL). A second bound existed: bind_text LIKE-escapes
(`\`, `%`, `_` double) and wraps the value in `%...%`, so the stored parameter needs 2*127 + 3 bytes.

## Fix
- query.h: SEL_QUERY_TEXT_MAX 128 (source of the bound); new SEL_QUERY_PARAM_MAX = (TEXT_MAX-1)*2+3 = 257
  for sel_query_param.s; bind_text guard made exact.
- view.h: SEL_VIEW_KEY_MAX now derives from SEL_QUERY_TEXT_MAX (no second magic number).
- engine.c VIEW_ACTIVATE: album query buffer sized for worst case (every byte escaped): (MAX-1)*2+9 = 263;
  overflow reports `too_complex` instead of truncating.
- Stack on Amiga (no VLA/alloca): pq.stack 8 x 128 = 1 KB, parse_term buf 128, view_cond name 128 (the latter
  recurses to depth 8, about 1 KB). sel_query (32 x 257 params) lives in the library struct / static in the
  CLI, not on the stack. fetch.c text[128] and search.c match[256] already fit.

## Tests
- tests/m3/query.golden: 127-byte quoted value with an escaped quote compiles; 128 bytes gives too_complex.
- tests/m3/home_view.cmake: 127-byte album name containing a quote opens from the Jump back in row.
- Fail before (fix reverted): query-compile `[ERROR] 'album:"AAAA...\"BBBB..."': expected sql ... got 'ERROR too_complex'`;
  home-view `127-byte album name did not list its songs` (view_source=home).
- After: query-compile, home-view, play-query, view-library-source, library-search, smart-downloads,
  view-unit, view-tree pass; `commands --unimplemented | wc -l` = 8. build/amiga builds clean in src.
