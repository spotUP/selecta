# M3 Phase 4a report: I157 source registry, I158 local source scan

Branch `worktree-agent-ad9b993878204076c`, base 917ce91. Spec: plan 2026-10-25-m3-library.md Phase 4; design doc sections 9.1 and 9.2.

## Done

- I157: `src/library/source.h`, `source.c` (sel_source per design 9.1, registry, `sel_song_key`, `sel_song_key_split`), `formats_table.c` (about 49 Modland formats with extensions, case-insensitive lookup by name and extension), `sources/modland.c` (url, path_in, parse of `pub/modules/<format>/<artist>/<file>`), `SEL_SRC_*` tags in `tags.def`.
- I158: `sources/local.c` scans through `sel_browse` (iterative walk, 64 dir stack, batches of 32 rows, 8 MB file cap, probe first YES else first UNSURE, open/metadata/close for title, author, format, duration). `db/scan.c` writes `files` and `songs` in one transaction per batch (idempotent upsert). New library request `SEL_LIB_REQ_SCAN_BATCH`, engine command `LIBRARY_SCAN`, `selecta-cli library scan` and `library key`.

## Tests

- T71 `source-local`: passes. Sentinel is `probed > 0`; asserts scanned_files equals manifest count, db_songs equal, skipped_files >= 1, dirs = 2, rescan idempotent.
- T72 `source-modland-key`: passes (modland key and URL round trip, local key with ':' path, unknown source rejected).
- Tag lint green (`unknown_tags=0 duplicate_tags=0`), ratchet passes (count unchanged).
- Host build -Werror clean; Amiga 68020 build compiles and links (no warnings from new files). No vamos test added for scan.
- Failing in this worktree, not mine: library-search, play-query, view-library-source (I149-I151, other agents).

## Open questions

1. LIBRARY_SCAN goes through an engine config hook (`library_scan`, `library_ctx`) because the engine has no library handle yet. Default kept. Alternative: move the scan into the engine once I149 lands.
2. T71 checks `db_songs` and scan counters, not `library search source:local`, because `library search` (I149) is not in this branch. Default kept. Alternative: extend T71 after I149 merges.
3. `sel_source.parse` takes a scratch buffer (differs from the design doc signature) so returned strings stay valid. Default kept. Alternative: static buffer (not reentrant).

## Remaining

Archive entries, rescan, watcher (I159, I160); Modland listing and transports (I162); REGEXP and search; vamos scan test. Format table is static; owner replayer is looked up by callers via `sel_registry_format_owner`.
