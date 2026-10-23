# I178a: engine verb that creates a DB playlist

Verb: `PLAYLIST_CREATE <name> [folder]` ("su?", group playlists). Saves the current queue, in order, as a library DB
playlist (folder 0 or absent = root; dividers skipped). Reuses `sel_library_playlist_add` (same path as `@playlist_add`),
errors through `folder_report` / `sel_engine_report_error` (no_library, no_such_folder, queue_full, library_failed).
Answer: sticker-style answer kind `playlist` carrying the new id. `PLAYLIST_SAVE` (file export) is unchanged.
FOLDER_DEL behaviour unchanged.

Files: include/selecta/commands.def, src/engine/engine.c (X-list + handler), tests/CMakeLists.txt,
tests/m3/playlist_create.sel, tests/m3/playlist_create.sh.

Test `playlist-create`: FOLDER_ADD, QUEUE_ADD two songs, PLAYLIST_CREATE into folder 1, then the folder tree shows the
playlist and `library export-folder` yields the two songs in queue order from the DB.

Results: playlist-create, protocol-reflection-commands, folders-tree, text-ui-folders-tree pass. `commands --unimplemented`
prints 8. Host and Amiga builds: 0 warnings outside third_party. Revert check: handler body disabled -> playlist-create
FAILED; restored -> passes.
Not run: amiga-roundtrip under vamos.
