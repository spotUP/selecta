# I218i: folder browser hides files with no recognised format

Cause: sel_browse_list (src/library/local.c) listed every non-hidden file; the engine browse and `library browse` showed them all.

Fix (library layer, single source): `sel_browse.songs_only` flag (local.h), set by sel_engine_create (engine.c), so the text UI, GUI
and `selecta-cli library browse` (all via the engine) share one list. Filter = sel_format_known_file (formats_table.c, declared in
local.h): extension or Amiga prefix ("mod.x", "mdat.x"), case-insensitive, through the Modland table plus an extra-names list for
replayer formats the table lacks (mp3, ogg, wav, flac, ahx, p61, ...). Playlists (sel_playlist_format_from_name) and archives
(zip/lha/lzh/lzx, also their members) stay visible. The library scan's own browse keeps songs_only 0 (it probes content).
No prefix lookup existed; added inside the existing table module, not a second recogniser.

Not changed: `library browse --names` (I218d helper, apps/selecta-cli/library.c) lists unfiltered on purpose.

Test: browse-hides-unrecognised-files (tests/library/browse_filter_test.c). With the filter disabled it fails (readme.txt, cover.jpg,
noextension shown); restored, it passes. Also green: view-unit and the browse/library/scan/folder ctest set (14). unimplemented count 8.
Host and Amiga builds: 0 warnings in project code.

Unverified: the extra-names list is a judgement of formats the replayers play; a format missing there is hidden until added.
