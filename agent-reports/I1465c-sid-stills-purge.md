# I1465c SID stills purge (design R6, D7a, section 6)

Files: src/library/db/schema.c (SCHEMA_V27, appended to g_migrations), src/library/db/db.h (SEL_LIB_SCHEMA_VERSION 26 -> 27),
tests/library/schema_tx_test.c (new v26 -> v27 section, test library-schema-tx), tests/library/dup_rebuild_empty_test.c (statement bound +4).

Migration v27, data only (SCHEMA_FINAL unchanged, like v21): deletes artwork kinds cover, dz1-dz7, dz_key, dz_more, dz_more2, dz_miss
for albums that also hold a ds_file, ds_csdb, dz_more or dz_more2 row. Keeps ds_* answers, dz_video, CSDb thumbnail covers (csdb:<id> albums have none of the markers).
No backup (alpha db).

Rescrape: art_scrape.c dz_step: sel_art_find -> no cover row -> composer_art + csdb_cover (ds_step is gated by existing ds_file/ds_csdb rows, so no
network refetch of the DeepSID JSON; only the images via sel_deepsid_csdb_own) -> dz_more2 written again. dz_miss gone, so no retry window blocks it.
Triggered on next play of the song; no DeepSID fetch/parse code edited.

Unverified (nothing built or run): compile, library-schema-tx, library-dup-rebuild-empty, library-schema-final, library-db-open.
Risk: songs whose stills sit under a meta key with no ds_*/dz_more marker (twin of a SID reached via another source) are not purged.
