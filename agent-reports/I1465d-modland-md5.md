# I1465d Modland md5 at sync (R7 / D6a)

Root cause: songs.md5 / files.md5 columns and the allmods_md5 parser already exist (modland_sync.c). The network sync fetched allmods.zip (member allmods.txt, "<size><TAB><path>", no md5), so every row got md5 ''. Fix: src/library/sources/modland_net.c now fetches allmods_md5.zip (member allmods_md5.txt) first and falls back to allmods.zip on any non-200.
Schema: no change, no migration (columns exist since v1). Existing rows get md5 on next sync only if the upsert overwrites md5 (unverified; check sel_scan upsert).
Test: extended source-modland-net (tests/net/modland_net.cmake) with host md5.modland.test (fixture tests/net/fixtures/md5.modland.test/allmods_md5.zip); asserts every listing md5 appears in the --dump rows.
Unverified (unbuilt): compile, test run, that --dump works with a URL, that ftp.modland.com serves allmods_md5.zip (assumed from memory; fallback covers absence), DEViLBOX proxy route for it (404 falls back), changelog-added rows still have no md5, and the md5 zip is ~2x bigger.
