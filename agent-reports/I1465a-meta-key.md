# I1465a: meta key (design 2026-11-20-i1465-i1468, section 6 R1 + R2)

## Changed
- Merged branch i1465 (30b70dbf, TWIN route of sel_deepsid_key) first.
- R1: `sel_meta_key` (src/library/db/user.c): SID family via sel_deepsid_key, then md5 group (hvsc, amp, lowest key), then unique title + composer twin on the same platform family (lower of the two keys), else own key. Cached in new table `song_meta_key` (schema v26, SCHEMA_FINAL mirrored). Declared in db_internal.h with SEL_META_* routes.
- R2: new library op `SEL_ART_META_KEY`; `art_find_dz` reads stills under the meta key, falling back to the own key (pre-existing data, no migration). `dz_step` resolves the meta key once and uses it for stills, dz_key, dz_miss, DeepSID (composer_art, csdb_cover) and the Demozoo query (sel_dz_query). `folder_art` reads the own-key directory but stores dz_key = meta key; `url_art` reads cover_url from the own key, stores under the meta key. csdb_cover and composer_art unchanged (they receive the meta key).
- Tests: new `library-meta-key` (tests/library/meta_key_test.c); dup_rebuild_empty_test bound +4 statements for v26.

## Tests (build/host, affected subset)
library-meta-key, library-deepsid-key, library-schema-final, library-dup-rebuild-empty, art-scrape, demozoo-art, deepsid-cover: all pass.

## Open
- library-dup-rank is "Not Run" (not built in my subset; unrelated to the change, unverified).
- Cached meta keys are not refreshed when a twin is added later (`DELETE FROM song_meta_key` re-resolves). No uade_key route.
- R3-R7 not done. No Amiga build, no rig.
