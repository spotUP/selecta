---
date: 2026-10-26
topic: How the Selecta desktop UI looks and fits on the Amiga (rig 2, uaegfx, Workbench 800x600)
status: complete (one gap: INDEX8 path not exercised, see Colour)
ledger: I358
---

# Amiga desktop UI: look and fit (I358)

Build: main a17cc7c (I286 merged), Amiga 68020 binary from a detached worktree, put to SYS:selecta-test/Selecta on rig 2.
Run: `--ui desktop --backend rtg --stats --db RAM:i358.db --exec "LIBRARY_SCAN SYS:selecta-test/mods" --view home --log RAM:i358.log`
(no --canvas: the screen opened 800x600 by itself, so I340's oversized default is gone). Driven with ami.py click_px; Ctrl-C by `break`.
Screenshots (800x600 PNG): /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/d21f2148-ebf4-461d-8876-9851ed290fe3/scratchpad/i358/shots/ ; stats text: /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/d21f2148-ebf4-461d-8876-9851ed290fe3/scratchpad/i358/stats.txt.

## Verdict

Every page fits 800x600 with no clipping, overflow or cut-off text (missing_glyphs=0). Nothing is unreadable. The defects are content and
polish problems, plus one measurement gap (the 16-bit screen hides the colour budget).

## Home (home.png)
Fits. Search bar, "Welcome back", chips All/Songs/Composers/Playlists, shelf "New this week" with 4 tiles of 144 px (2 more would fit in the
width), transport bar at the bottom. Rail 4..52 px, content 60..796, transport y 530..600. Rows of the tile shelf use 144 px of 600 px height.
- D1: all 4 tiles show the same placeholder art glyph (identical shape, only the gradient differs); titles read "seleambient", "seledown", "seletechno", "seleambient" (the shelf title is not the file's song title; file names are genre_ambient.mod etc., and the Queue shows "genre_ambient"). Home.png. Proposed row: "Amiga Home shelf: tile titles show a mangled name (seleambient) instead of the song title or file name genre_ambient; check the title source and the tile art for modules without cover (all four identical)."
- D2: the shelf occupies only the top third; the lower 55 percent of Home is empty with 4 songs (expected with a tiny library, unverified with a large one). No ledger row.

## Library (library.png, rail icon 3 at y=160)
Fits. Title, "Add Folder" button, tabs Composers/Tree/Columns/Files/History, group row "Unknown composer, 4 songs", Statistics block (Songs in library 4, Total plays 0, Total play time 0:00:00). Text sharp, row height about 24 px.
- D3: the active tab is only blue text; the tab labels sit on a row 24 px high with 120 px spacing, no underline or pill, so the active state is faint at this size. library.png. Proposed row: "Library tab strip: add a visible active indicator (underline or filled chip) as on Home chips."
- D4: the two list-style rail icons (Library at y=160 and Queue at y=240) are near identical, and the rail has no labels or tooltips visible; a new user cannot tell them apart. library.png, queue.png. Proposed row: "Navigation rail: the Library and Queue icons are almost the same; use distinct glyphs (DUI icon set) and show a label on hover."

## Playlists (rail icon 4, playlists.png) and Queue (queue.png, queue_playing.png) and Profile page (nav320.png)
All fit. Playlists: buttons New Folder / New Playlist / Rename / Delete / Play, a Name field, otherwise empty. Queue: Undo/Redo/Reshuffle/Scope: Library, empty while stopped, one row "genre_ambient" with Play Next and close after a song was started. Page at rail icon 7 (y=320): Mark Wrong / Skip Song / Save Note and a Note field.
- D5: Playlists, Queue (empty) and the y=320 page show no empty-state text, only a toolbar and blank space. playlists.png, queue.png, nav320.png. Proposed row: "Empty pages (Playlists, Queue, the y=320 note page) need a one-line empty state, as Search has ('Type to search...')."
- D6: the y=320 page (Mark Wrong / Skip Song / Save Note) has no title and is reachable only from the bottom rail icon, which looks like a profile icon. nav320.png. Proposed row: "Rail icon 7 (person glyph) opens a note and flag page: give it a title and an icon that matches (or move it into the song menu)."
- D7: the toolbar buttons start 68 px from the top with no page title on Playlists, Queue and the note page, while Library has a 28 px title; titles are inconsistent. Proposed row: "Page titles: Playlists, Queue and the note page lack the heading that Library and Home have."

## Search (search.png)
Fits. Focused search field, chips All/Songs/Albums/Artists, hint line. Placeholder reads "Search songs, composers, playlists" but the chips offer Songs/Albums/Artists.
- D8: placeholder text and chip set disagree (composers/playlists versus albums/artists). search.png. Proposed row: "Search placeholder: name what the chips search (songs, albums, artists)."

## Now Playing (nowplaying_idle.png, nowplaying_playing.png, nowplaying_playing_t2.png)
Fits. Cover 344x344 at left (84..428, 68..412), text column from x=452, transport at the bottom with time, seek bar, Playing/Stopped, shuffle, previous, play or pause, next, repeat, volume.
While playing (nowplaying_playing.png): time 00:30, pause glyph, art changes to a second pattern, line "Protracker4" (the format), heading "Next in Queue".
- D9: no song title, no artist and no album line on Now Playing: only "Protracker4" and "Next in Queue" (empty list). nowplaying_playing.png. Proposed row: "Amiga Now Playing: show the song title (and composer, subsong) next to the cover; today only the format name is drawn."
- D10: the seek bar stays empty (00:30 shown, no fill) and the 6 KB module stopped after about 30 s (nowplaying_playing_t2.png shows Stopped, 00:30). Unverified whether the bar fills during play: I took one shot at 3 s after the click and one later. Proposed row: "Check on rig 2 that the seek bar fills during playback (screenshots at 0..5 s of a looping module)."
- D11: the cover is 344 px of 600 px height and leaves 450 px of the 800 px width to a nearly empty text column; the large art is fine, the right column is where title, time and queue should go. Same row as D9.

## Colour budget and frame time
- rtg: screen 800x600 depth 16 (stats line 1 and ui_backend depth=16). The rig 2 screen is a 16-bit truecolour mode, so the INDEX8 path from I286 did NOT run: palette_colours_ui/art/tile/panel/flow=0 and quant_calls=0. Banding or a wrong palette could not be judged. The 16-bit pictures show smooth gradients and no banding. Unverified: INDEX8 on a 256-colour screen (rig 2 has no such mode selected; the backend picked 16 bit). Proposed row: "Amiga INDEX8: force an 8-bit mode (backend option or 256-colour screenmode) on rig 2 and screenshot Home to measure palette_colours_* and banding (I286 acceptance)."
- Frames: ui_frames=95 over about 60 s (event driven), widget_draws=141, draw_prims=5834, dirty_pixels (last frame)=422400 of 480000, dirty_total=15258464, upload_pixels=15258464, glyphs=2886, missing_glyphs=0, tweens=28. ui_waits=45030, keys=0, nav_clicks=9.
- D12: no frame time is reported anywhere in --stats (only counts), and a click-to-screen delay was not measured; the 422400 dirty pixels per page change is 88 percent of the screen. Proposed row: "Amiga --stats: add ms per frame (min/avg/max) and per-page dirty pixel count, then measure the Home to Library switch on the 68020 rig."

## Defect count
12 findings (D1, D3-D12 are ledger row proposals; D2 needs none): 11 proposals.
