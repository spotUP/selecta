# M3 I163 report: hippo source

## What works

`selecta-cli library sync hippo <dir>` (engine command LIBRARY_SYNC) reads, from one directory:

- `hippo_list.json` (API v1 list): one catalogue row per playlist. path = server filename, name = title, artist = author, format "Hippo - Playlists". Stickers `genre` and `hippo_id`.
- `uhcplaylists.tsv` (legacy `/uhcplaylists.php`, Latin-1, `Filename Path Title Author Genre`): same rows, converted to UTF-8. `#` lines and the header line are skipped. No id column exists in this feed, so no `hippo_id` sticker.
- Every `*.prg` in the directory: read through the existing Hippo codec (`sel_playlist_read(SEL_PL_HIPPO)`, in `hippo_prg.c`). Entry, divider, stealth and `#song=` counts become stickers `prg_entries`, `prg_dividers`, `prg_stealth`, `prg_subsongs` on the playlist row of the same filename, and the sums go into the sync line.

URL of a row: `https://hippoplayer.se/playlist/<filename>` through the source descriptor (`hippo.c`). Sentinel: `hippo_lists=N` in the CLI sync line.

Test `source-hippo` (T74, `tests/m3/source_hippo.sh`) syncs the JSON and the TSV into separate databases, requires equal rows (columns path, format, name, artist, genre, url), checks the non-ASCII title from both encodings, the hippo_id sticker and the codec counts against counts taken from the fixture files. Host build, host ctest filter (15 tests) and Amiga 68020 build pass. `selecta-cli commands --unimplemented` still gives 17 lines.

## Gaps and assumptions

- The recorded fixtures under `tests/net/fixtures/hippoplayer.se` hold no divider, stealth entry or `#song=`, and no TSV was recorded. The fixtures in `tests/m3/hippo` are therefore synthetic (derived from the recorded index; the TSV header says so). They do not prove the real `/uhcplaylists.php` byte layout.
- Fetch is offline-from-directory, like the modland and hvsc syncs: the files must already be in `<dir>`. No network call is made by the sync.
- `LIBRARY_FETCH hippo:<file>` through the mirror/cache layer was not added (`net/mirrors.c` builtin table and `sel_cache_is_remote`) and is untested. Open as a follow-up.
- `sync_util.h` holds a shared `chomp`; `modland_sync.c` and `hvsc_sync.c` still carry their own copies. Not refactored here.
- PLAYLIST_PUBLISH (POST) is I164 and is not touched.
- The list is read with a small tolerant scanner for flat objects, not a full JSON parser. Nested values in a list record would not be skipped correctly.
