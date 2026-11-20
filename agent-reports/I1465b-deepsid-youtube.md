# I1465b DeepSID YouTube link + list all captures (design R5, D5a+b)

Files:
- src/library/db/deepsid_info.c: `sel_deepsid_youtube(json, subtune, url, cap)` (pure; isDefault capture, else first, filtered by subtune when > 0) and "YouTube capture" rows (D5b, max 12, "Subtune N, channel: url") appended in `sel_deepsid_rows` after the compo placings.
- src/library/db/db.h: declaration.
- src/library/db/art_scrape.c: `ds_video` stores the default capture as 'dz_video' of sel_art_dz_id(key) unless a dz_video row exists (Demozoo wins). Called after a fresh ds_file fetch and on the cached-ds_file-without-ds_csdb path only (see gaps).
- tests/library_deepsid_rows_test.c (ctest library-deepsid-rows): default wins, first-for-subtune fallback, subtune miss, empty/absent array, row listing.

Server shape (hippoplayer.se DeepsidRoutes.ts): youtube[] = {channel, videoId, subtune, isDefault}.

Gaps / unverified (nothing built or run):
- Not compiled, not run.
- Cached ds_file rows that already have a ds_csdb row return early and never get dz_video; needs a later pass or the R1 meta-key work.
- Key is the hvsc: DeepSID key, not the R1 meta key (not in this branch). Subtune 0 used at scrape time (playing subtune unknown); the panel rows show all subtunes.
- How a later Demozoo scrape interacts with an existing dz_video row not checked.

Twins: text UI (I1465t) and MUI (I1465m) need no new data path: they read dz_video via SEL_ART_KINDS / the Watch demo link as today, and show the new "YouTube capture" Song Information rows if their list rendering takes arbitrary label/value pairs (rows up to 256 bytes, URL at line end; may need wrapping or an open-URL action).
