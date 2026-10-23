# I191a: home view rows act on Enter

Design doc: 2026-10-22-architecture-design.md, home view.

## Change
- `sel_view_row` (include/selecta/engine.h) gained `key` and `album`; the home source stores keys in a new `fkey` buffer (src/engine/view.c).
- Library home text: `S <key>\t<entry>` (song) and `S @<album>\t<entry>` (album) (src/library/db/user.c `home_list`).
- `cmd_VIEW_ACTIVATE` (src/engine/engine.c), source home: song row -> `queue_insert` + `queue_start` (same as a query row); album row -> `run_search("album:\"<name>\"")` then the existing `cmd_VIEW_SOURCE query` path. Headings and the count line do nothing.

## Test
`home-view` extended: tests/m3/home_song.keys, tests/m3/home_album.keys, assertions in tests/m3/home_view.cmake. Song: songs_opened=1, playlist_entries=1, status "Playing  selecta test". Album: view_source=query, songs_opened=0, screen lists "mk tune" and not "test tune". home_view.screen golden unchanged and green.

## Fail-before (new assertions, old code)
```
home_view.cmake:80: Enter on a Recently played row did not open the song:
songs_opened=0 ... playlist_entries=0 ... view_cursor=1 view_source=home
```
## After
home-view, text-ui-keys-wait, folders-tree, text-ui-folders-tree, metadata-panel, stats-screen, view-library-source, text-ui-library-screen all pass. host and amiga builds clean. `commands --unimplemented | wc -l` = 8.

## Notes
- Album key is the album name; a name containing a double quote cannot be quoted in the query grammar (open edge).
- Worktree had empty submodules; copied nuade and libopenmpt from the main checkout (untracked, not committed).
- Machine-busy hook (fs-uae rig) delayed the build.
