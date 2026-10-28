# DUI golden bisect (I642, I644), 2026-10-28

Host build build/sdl (Ninja, Release, SELECTA_UI_DESKTOP=ON), main 0b9eccd6. Goldens are files under tests/golden/desktop, not hashes in tests/dui, so the "good" commit came from `git log` on the golden file.

| Test | Golden last set | First bad commit | Verdict |
|---|---|---|---|
| dui-queue | 6462e8ed (I608) | fd9e94ce (I627) | stale golden |
| dui-songinfo | 6462e8ed | fd9e94ce | stale golden |
| dui-frame | 6462e8ed | fd9e94ce | stale golden |
| dui-search | 6462e8ed | fd9e94ce | stale golden |
| dui-tall-pixels | 813c71ed (I601 merge) | 6462e8ed (I608) | stale golden (see caveat) |

## Evidence

- dui-queue: `git bisect run` between 6462e8ed (good) and main, 84 commits, result fd9e94ce. Reproduced on main: got f28a42168da4be0f, expected 3e7c94ada6435ac2.
- dui-songinfo, dui-frame, dui-search: no separate bisect (same golden-set commit). Run at fd9e94ce^ (e8802ff8): all pass. Run at fd9e94ce: all four fail. So fd9e94ce is the first bad commit for all four.
- dui-tall-pixels: bisect between 813c71ed (good) and main, 119 commits, result 6462e8ed. At e8802ff8 it fails with 7f64ce47947541ac vs golden 476f539184a3371c.

## Cause and judgement

- fd9e94ce "Sidebar Your Library empty state: Create your first playlist card with a Create playlist button (I627)". Changes plugins/ui/desktop/desktop_ui.c `side_lib_draw`: with 0 saved playlists the sidebar draws a card ("Create your first playlist", button "Create playlist"). The sidebar is on every desktop page, and the fixture has no playlists, so every 1280x800 page golden moves. The commit refreshed only home and playlists goldens. Intended visual change; dui-queue, dui-songinfo, dui-frame, dui-search were forgotten. (The Sources golden was refreshed later in 27338b10 for the same reason.) Not a regression.
- 6462e8ed "I608: Your Library list in the desktop sidebar". Draws the playlist list in the sidebar and leaves room in the header band. It refreshed 11 goldens but not library-rgba32-640x256-ydiv2@1.hash (the dui-tall-pixels golden, AGA 640x256 tall-pixel layout). Intended; golden forgotten.
- Caveat for dui-tall-pixels: bisect only gives the first bad commit. fd9e94ce (empty-state card, which uses the bitmap-text line "No playlists yet." on the bitmap tier) may also move that picture. Reblessing from main covers both, but the individual contribution of fd9e94ce to the tall-pixel hash was not separated.
- Not checked: the rendered pictures were not viewed (no images, no window). Verdict "intended" rests on the commit messages and diffs, which state the sidebar change, and on the fact that the same commits refreshed sibling goldens.

## Fix needed (not done here)

Rebless with SELECTA_GOLDEN_UPDATE=1 on main: queue, songinfo, frame, search (1280x800) and library-rgba32-640x256-ydiv2@1 (tall pixels), after Johan looks at the sidebar card on each page.
