---
date: 2026-11-09
row: I1231
topic: Main search latency (Mac desktop UI)
status: done
---

# I1231: search latency, root cause and fix

Johan 2026-11-09: "when i search in the main search i expect the current list to be cleared instantly and some search
spinner showing, it takes a long time before i see any action when searching now" / "why are the searches so damn slow?
dont we cache our sources?" / "make it super fast".

All measurements on a COPY of Johan's library.db (814 MB, about 650k catalogue rows), never the original, through
`selecta-cli script` with `SOURCE_SEARCH * <q>`, network mapped to a closed port (no live site reachable).

## Root causes

1. **Per-source LIKE full scan, serialized.** Each synced source (modland, hvsc, asma, sndh, keygenmusic, aminet,
   zxart, hippo, local) ran its own `LIKE '%word%'` over `songs` (no index can serve a leading wildcard), one after the
   other behind the library call lock. Modland alone took 3749 ms cold for "jammer"; sndh 126 ms. Every source waited for
   the ones before it, so the first section arrived seconds late.
2. **No instant feedback.** The desktop search view kept drawing the previous result list until the first new rows
   arrived; no spinner existed.
3. **aminet and zxart searched over the network** even when their catalogue was synced locally.

## Fix

- One grouped FTS request (`sel_library_catalogue_search_grouped`, src/library/db/search.c) answers every synced source
  flagged `SEL_SOURCE_CATALOGUE_SEARCH` at once: a word-prefix match over `song_fts` (`"jam"*`), bucketed by source with
  a per-source cap. Query plan: `SCAN song_fts VIRTUAL TABLE INDEX 0:M4` + `SEARCH songs USING INTEGER PRIMARY KEY`.
  Semantics change: word-prefix instead of substring (as in every other modern player search).
- Sources with no synced rows (`SEL_CATALOGUE_NOT_SYNCED`) and unflagged sources still run their live hook.
- aminet and zxart carry the flag, so a synced catalogue is searched locally; unsynced falls back to the live search.
- Desktop UI: on submit the result list clears and a spinner draws on the next frame (draw_search spinner branch,
  `search_spinner` dump line), until the first hits land.

## Numbers (db copy, "jammer" / "love")

| Source / stage | Before | After |
|---|---|---|
| modland, "jammer" | 3749 ms cold (LIKE scan) | in grouped query, 8 ms |
| sndh, "jammer" | 126 ms | in grouped query, 8 ms |
| hvsc, asma, keygenmusic, hippo, local | serialized LIKE scans after modland (per-source times not recorded) | in grouped query |
| grouped FTS query, all synced sources | n/a | "jam" 8 ms, "lov" 9 ms |
| aminet, zxart | live network search | local catalogue (zxart "jammer" 6 hits, aminet "love" 40) |
| whole `SOURCE_SEARCH *`, wall incl. process start | not recorded | jammer 441 ms warm (7895 ms first run, cold page cache of the 814 MB file), love 361-554 ms |
| list clear + spinner | none until first rows | next frame (dui-search-spinner: first_frame_after_fire 0..1) |

"love" before figures per source were not measured separately; the LIKE scan cost is per source and query-independent
in shape (full table scan), so it is of the same order as "jammer".

## Tests

- `source-search-grouped` (tests/engine/source_search_grouped_test.c): engine entry point; grouped==1, LIKE searches==0,
  synced hooks not called, unsynced and unflagged hooks called. Revert (search_start_group disabled): 6 FAIL lines.
- `dui-search-spinner:search_spinner` (tests/dui/search_spinner.cmake): spinner drawn within one frame, stale list
  cleared, hits land after. Revert (spinner branch removed): drawn=0, fails.
- `aminet-search`, `zxart-search`: synced db answers with the site on a closed port, `SOURCESEARCH runs=0`.

## Unverified

- Amiga cross build not run.
- Live Mac check by Johan.
