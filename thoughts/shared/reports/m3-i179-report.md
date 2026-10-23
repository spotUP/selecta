# I179 report: text UI playlist folder tree

## Blocked on me
- Decision: key `f` opens the tree (no Shift, free letter). Veto if you want another key or a place in the Tab cycle.
- Decision: folders are always shown open (no collapse), Enter on a playlist loads it into the queue and starts its first song, Enter on a folder does nothing. Tree is capped at 256 rows and 4096 bytes of tree text.
- Manual check: `selecta --db <file>` on a database with folders, press `f`; the tree should read Rock / Metal / Mix_one ... nested two spaces per level.

## Changed
- Engine: new view source `folders` (`SEL_VIEW_FOLDERS`, rows `SEL_ROW_FOLDER` / `SEL_ROW_PLAYLIST`), built from `sel_library_folder_tree` by `folders_reload`; reloaded after FOLDER_ADD / FOLDER_MOVE / FOLDER_DEL / PLAYLIST_MOVE. The UI never touches SQLite: it sends `VIEW_SOURCE folders` and draws view rows.
- Text UI: action `folders` (key `f`), title "Playlist folders  N rows", folder rows `[-] name`, playlist rows ` -  name`, own hint line, sentinel `SEL_UI_STAT_FOLDER_OPENS` (0x501B). Exit line `folders_screen opens=<n> rows_built=<n>`.
- Test: ctest `text-ui-folders-tree` (label m3) builds the tree with `folders.sel`, drives the real `selecta` with `tests/m3/folders_screen.keys`, asserts `folders_screen opens=1`, `view_source=folders`, `songs_opened=1` and the golden `tests/m3/folders_screen.screen`.
- Green (host): text-ui-folders-tree, folders-tree, text-ui-library-screen, view-*, keybind, palette, cmdline, ratchet, protocol-reflection-*, selecta-*, ui-*, render-golden-*, play-query, stage-commands. Amiga build of `selecta` compiles.

## Found
- The text screen dump has no cursor marker, so the golden does not show which row is selected.
- `rows_built` counts every reload, so it rises with each folder command as well as with opening the screen.
