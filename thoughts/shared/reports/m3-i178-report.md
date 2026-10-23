# I178 report: playlists and folder tree in the DB

## Blocked on me
- Decision: non-empty FOLDER_DEL answers `ERROR folder_not_empty` and deletes nothing. The design states no rule; this is my choice. Veto if you want cascade.
- Decision: the "unimplemented drops by N" criterion cannot hold (see Found). Confirm the ratchet stays at 10.

## Changed
- DB: `src/library/db/folders.c/.h`, ops dispatched from `user.c`, wrappers in `db.c/db.h`. No schema migration (tables already in SCHEMA_V1, user_version stays 1).
- Engine: FOLDER_ADD, FOLDER_MOVE, FOLDER_DEL, PLAYLIST_MOVE in `commands.def` and `engine.c`; stats field `folder_ops`.
- CLI: script directives `@folder_stats`, `@folder_tree`, `@playlist_add`; `library export-folder` (one file per playlist).
- Test: ctest `folders-tree` (label m3) passes: nested folders, PLAYLIST_MOVE, FOLDER_MOVE, FOLDER_DEL empty and non-empty, `ERROR folder_cycle` x2, restart in second process, `folders= playlists_in_folders=` asserted, export compared to golden. Also green: ratchet, stage-commands, play-query, stickers, protocol-reflection-commands. Host and Amiga builds compile.

## Found
- `selecta-cli commands --unimplemented` stays 10. The four verbs are born implemented; none of the 10 listed (QUEUE_CHANGES, SNAPSHOT_QUEUE, RATE, VOTE, UI_SWITCH, IDLE, AUTH, DEVICE_LIST, DEVICE_TRANSFER, DEVICE_FOLLOW) belongs to I178. `tests/ratchet/unimplemented.txt` unchanged.
- No engine verb creates a DB playlist; only test directive `@playlist_add` and `sel_library_playlist_add`. Follow-up: save queue as DB playlist.
- FOLDER_MOVE / PLAYLIST_MOVE with target 0 = root; moving a folder into itself or its subtree = `folder_cycle`.
