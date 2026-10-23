# M1.2 row I72 report: playlist codecs

- Files: src/library/playlist/{playlist.h, playlist_internal.h, playlist.c, formats/{hippo,m3u,pls,xspf}.c}, tests/playlist/playlist_test.c, CMake wiring in src/library and tests (ctest `playlist-codecs`, T29).
- Gate 2 red: commit 64c6056 with stub codecs; `33 - playlist-codecs (Failed)` (M3U, PLS, XSPF asserts failed).
- Green: `1/1 Test #33: playlist-codecs ... Passed`.
- API: caller-supplied entry array and text buffer (offset 0 = empty string), `sel_playlist_read/write`, sink and warn callbacks, `sel_playlist_format_from_name`, `sel_playlist_sniff`.
- Hippo: stealth is written as 0xA2 + key (not re-emitted as 0xF7 as the original does); subsong > 9 is dropped with warning `lossy subsong`; 131072 entries answer `range` before any byte is written.
- Extra: XSPF carries stealth as `<stealth>1</stealth>` in the selecta extension; M3U/PLS/XSPF drop stealth flag to a plain song (M3U, PLS) with no warning.

## Deviations and notes
- Read API takes no arena parameter: `sel_playlist_init` binds the caller's buffers (same effect).
- Relative path resolution against the playlist directory is not in the codecs (they have no directory); it belongs to PLAYLIST_LOAD (I73).
- PLS reader requires File<n> before Title<n>/Length<n>.
- Worktree needed the submodule dirs third_party/nuade and libopenmpt temporarily symlinked to the main checkout to configure; restored, not committed.
