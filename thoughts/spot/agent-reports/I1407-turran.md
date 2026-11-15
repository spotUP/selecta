# I1407 step 2: Turran FTP search source

Branch: worktree-agent-a1c0143f7db99e5ad (based on main after step 1, af53f50a).

## What changed
- `plugins/sources/turran/` (turran.c, turran_row.c/.h, CMakeLists.txt): SOURCE_SEARCH plugin `turran`, name "Turran FTP". `search` calls `sel_uhc_search` with searchtype `turransearch` and the query prefixed `collection/music/ ` (same as HippoPlayer.js line 432 / 9104).
- Song key is `<path>/<file>` (path starts with `Collection/`, case-sensitive: lowercase gave 404). The file is fetched by core through the mirror table: new BUILTIN row `turran` -> `https://uhc.megaburken.net/turran/` in `src/library/net/mirrors.c`. No url/play_url hook needed.
- Row filter keeps only playable extensions. Rejects: paths outside `Collection/`, `..`, control chars and `| " \ ? #` (the index renders non-ASCII as `?`, so such names are dropped).
- New `sel_platform_ext_known(ext)` in `include/selecta/platform.h` / `src/platform/platform.c`: the existing `sel_platform_of_ext` returns the Music family for unknown extensions, so it cannot say "known". Searched: include/selecta/*.h, src/library/sources/local.c, replayer SEL_RP_EXTENSIONS hooks; the single extension table is `k_chips[]` in platform.c, so the new function reads that table.

## Terms (Turran)
No ToS, README, rules or about page found (README.txt, readme.txt, rules.txt, about.html all 404; Contact.txt only gives EAB forum/discord links). The only terms are on the HTTP index page: "only meant to ease browsing for single files", speeds severely limited, limit 100 GiB / 500 files per 7 days, exceeding = temporary ban, FTP recommended for bulk. Verdict: on-demand single-file fetches are within the stated use; no bulk download, no mirroring, nothing committed or bundled. Not a licence statement; scene-files-free-to-use applies (never commit/bundle).

## Download check
`GET https://uhc.megaburken.net/turran/Collection/Music/Mod/Ripped%20By%20Chip/Protracker/Animotion/revolution.mod` -> HTTP 200, bytes start with module title "---)revolution(---". No audio played. Range header appeared ignored.

## Tests (build/host Debug, via buildq, nice 19)
- `turran-row` (new, tests/library/turran_row_test.c, fixture `tests/net/fixtures/uhc.megaburken.net/turransearch_mod.json` recorded from the live index, 8 rows -> 3 kept: 2 .mod + 1 mp3; 7z, zip, `.pp`, no-extension and `?` names skipped): pass.
- `uhc-remix-parse`, `sources`, `sources-off` (expected files updated with the turran row between sndh and zxart): pass. `mirrors-ranking`, `net-fixture-start`: pass.
- `selecta-cli plugins --lint`: clean (107 plugins, 0 unknown/duplicate tags).
- Amiga 68020 Release: `sel_plugin_turran` and `sel_sdkcheck_turran` compile (no rig).

## Unverified
- Live search and playback in the app (no app run, no audio).
- Amiga link/run on a rig.
- Archives (zip, lha, 7z) are not offered: not in the extension table. Packed `.pp` and Amiga prefix-style names (`mod.name`) are excluded; the Turran music tree is mostly mp3/zip, so coverage is partial.
- Platform-wide tests beyond the four listed were not run.
