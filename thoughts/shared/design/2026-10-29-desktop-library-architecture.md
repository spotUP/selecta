---
date: 2026-10-29
author: Opus agent for musicplayer-55
status: decided
ledger: I678
---

# Desktop library architecture: Your Library sidebar, main pages, click-to-page map

Johan 2026-10-29 (Mac desktop): "i think we have got the whole main area and sidebar a little wrong"; "spotify has chips for
playlists, podcasts, albums, artists, downloaded in the sidebar. clicking them does not change the middle column it lists them in
the sidebar"; "when i click something in the sidebar list the middle column updates".

The rule this design enforces, in one line: **chips filter the sidebar list only; a click on a sidebar row opens that item's page
in the main area.** Nothing else in the sidebar changes the main area except the doors (Home, Search) and the header title.

Reference: `thoughts/shared/research/2026-10-28-spotify-inventory.md` sections 2, 3, "Spotify sidebar menu", 8 (album), 9
(podcast), 10 (playlist), 11 (Downloaded). Memory: spotify-reference-design (Spotify is the bar), design-not-capped-by-amiga.

Every section below has: current state (file:line), target, data source (which store owns it; the desktop UI only draws and
routes), and one reachability test. All tests use the existing harness: `dui_run(... --ui desktop --backend mem --canvas
1280x800 --clock fixed --stats --input <script>)` (pattern: `tests/dui/sidebar_library_door.cmake:16-35`), seeded with the
fixture library `tests/dui/fixture_library.sql` (pattern: `tests/dui/browse_tiles.cmake:11-20`). Counts in assertions come from
the fixture through `sqlite3`, never typed by hand.

---

## 1. What exists today (the starting point)

- Pages: one enum `desk_page` at `plugins/ui/desktop/desktop_ui.c:80`: Home, Search, Library, Playlists, Queue, Now Playing,
  Songs, Song Information, Visualizer, Tracker, Shelf, Browse. There is **no album, artist, composer or source page**. A Home album
  tile "opens an album search" through VIEW_ACTIVATE (`desktop_ui.c:2155-2156`, `:7842`); an opened Home tile draws the bare
  `draw_detail` view (`:1392`, `:7281`). The composer "page" is a drilldown inside the Library page (`st->composer_page`,
  `include/selecta/engine.h:186`; drawn at `desktop_ui.c:6307`).
- Doors: `src/uimodel/doors.c:7-9`: Home, Search, Your Library, Playlists, Queue, Now Playing, Song Information
  (`SEL_UIM_DOOR_COUNT 7`). I673 hid the duplicate "Your Library" door row; the header title opens the Library page.
- Sidebar list: `side_items[SIDE_LIB_MAX]` (`desktop_ui.c:362`) of `sel_engine_library_item` (`engine.h:467-472`: id, name,
  added, creator). Filled only by `sel_engine_library_playlist_items` (`engine.h:474`, `src/engine/engine.c:5513`): **playlists
  only**. Reload, filter and sort: `side_lib_reload` (`desktop_ui.c:8329-8365`).
- Chips: `side_chip_name[] = {"Playlists", "Albums", "Artists", "Composers"}` (`desktop_ui.c:8098`).
  `side_chip_keeps_playlists` (`:8201`) empties the list for every chip except Playlists (the "nothing here yet" Johan saw, I677).
  `side_chip_click` (`:8210-8229`) jumps to the Library page for Albums, Artists, Composers through `side_chip_lib_mode`
  (`:8207`): the I673 stand-in that breaks the rule above ("artist opens columns").
- Row click: `desktop_ui.c:10470-10477`: always `desk_page_goto(PG_PLAYLISTS)` + `pls_open` (`:4172`), the playlist page
  (`PLS_COVER 160`, `:4213`).
- Sort and filter: `sel_uim_lsort_cmp`, `sel_uim_lfilter_match`, `SEL_UIM_LSORTS`, `SEL_UIM_LVIEWS`
  (`include/selecta/uimodel/browse.h:35-45`). "Recents" keeps engine order (oldest first), it is not a recency sort.
- Library page: `draw_library` (`desktop_ui.c:6292`), six tabs `k_mode_name` (`src/uimodel/browse.c:8`): Composers, Tree,
  Columns, Files, History, Sources; their engine lines in `sel_uim_libmode_lines` (`browse.c:20-40`).
- Layout regions: topbar, sidebar, main, detail (right panel), player (`src/uimodel/layout.c:106-162`).
- Library data: `albums(id, name, artist, year, source)` (`src/library/db/schema.c:17`), `composers(canon, display, songs)`
  (`:140`), `source_songs(source, n)` (`:185`), `history(key, played_at)` (`:19`), `liked(key, at)` (`:22`), `playlists`
  (`:35`). Home text: `sel_library_home` (`src/library/db/db.h:414-417`): Recently played, Jump back in, New this week.
- Bitmap (Amiga AGA) tier inside the desktop UI: `desk_bitmap_text` (`desktop_ui.c:713`); chips collapse to one cycle button
  (`side_chip_single`, `:8150`).

---

## 2. The library item model (one mixed list, owned by the library store)

**Current.** Playlists only (section 1). No album, artist, composer or source listing call reaches the UI; composers arrive only
as the Library page's `st->composers` blob.

**Target.** One list of typed items, the Spotify "Your Library" (inventory section 2 rows: Liked Songs, Local Files, album,
Your Episodes, artist). Selecta kinds:

| Kind | Rows from | Thumbnail | Subtitle (Spotify pattern, sections 8.1, 10.1) |
|---|---|---|---|
| Pinned: Liked Songs | `liked` | gradient tile with heart icon | pin icon, "Playlist • N songs" |
| Pinned: Recently Played | `history` | gradient tile with clock icon | pin icon, "Playlist • N songs" |
| Pinned: Local Files | `songs` with a local source | gradient tile with folder icon | "Playlist • N songs" |
| Playlist | `playlists` | cover of first song, else 2x2 mosaic, else generated art | owner name ("You") |
| Album | `albums` | album artwork (`artwork` by `albums.id`) | "Album • Artist" |
| Artist | distinct `albums.artist` and song artists | round avatar (generated art, round mask) | "Artist" |
| Composer | `composers` | round avatar (generated art) | "Composer • N songs" |
| Source | `source_songs` + source registry | source icon tile | "Source • N songs" |

Selecta's kinds replace Spotify's Podcasts and Downloaded: Composers and Sources are the kinds Spotify has no word for; Local
Files covers Downloaded (Johan 2026-10-29: "clicking Downloaded in the sidebar opens the Local Files playlist").

**Data source.** A new library call `sel_library_items(lib, kind_mask, out, max)` in `src/library/db/` (one SQL union over the
tables above, with per item: kind, id or key, name, subtitle fields, song count, added time, last played time from `history`,
art id). Engine wrapper `sel_engine_library_items` replaces `sel_engine_library_playlist_items` (`engine.h:474`); the item
struct grows `kind`, `key[96]`, `songs`, `last_played`, `art_id`, `pinned`. Subtitle text is built in `src/uimodel` (one
formatter for every UI, including MUI), never in `desktop_ui.c`. Caps: the store pages the list (window of SIDE_LIB_MAX rows plus
a total count) so a 30 000-album modland library does not land in one array.

**Reachability test (dui-library-items).** Fixture with P playlists, A albums, R artists, C composers, S sources (each counted by
`sqlite3` from the fixture). From Home, no chip: the `sidelib` stats line reports `n = 3 + P + A + R + C + S` and the new sentinel
`side_items_calls` is at least 1 (the new engine call ran; the old playlist-only call count is 0). Revert proof: on old code
`n = P`.

---

## 3. Your Library sidebar: one mixed list, pinned rows first

**Current.** `side_lib_draw` (`desktop_ui.c:8557`) draws playlist rows with generated "album" art (`:8643`, `:8653`); open-row
highlight only for playlists (`side_lib_open_item`, `:8519`).

**Target (inventory section 2, 10.1).**
- Header row: "Your Library" title (opens nothing new: see decision D1), Create Playlist plus, Expand Library control (I609, kept).
- Chip row, then the tools row (magnifier and filter field left, sort label and view icon right; I611 kept).
- Pinned rows first in fixed order: Liked Songs, Recently Played, Local Files (pin icon in the accent colour before the
  subtitle). I612 (user pins) extends the same `pinned` field later; this design does not need the pin menu.
- Then every other item in the chosen sort. Row: 48 px thumbnail (square; round for artists and composers), title in text
  colour, subtitle in muted colour, the open item's row in the selected fill (`#2a2a2a`-style surface token, sections 8.1, 9.1:
  flat lighter rounded rectangle, no accent bar).
- The open-row highlight follows the main page: whichever item page is open marks its row, for every kind.

**Data source.** Items from section 2; open item identity `(kind, id/key)` from the page model (section 9). The UI only matches
the two.

**Reachability test (dui-sidebar-mixed).** From Home: the first three `sidelib_row` names are "Liked Songs", "Recently Played",
"Local Files", each with `pinned=1`; row 4 onward has `pinned=0`. Click the row of album 1 (rect from the `sidelib_row` line):
`sidelib_open kind=album row=<its index>` and exactly one row reports `open=1`. Sentinel: `side_opens` 1.

---

## 4. Chips filter the sidebar only

**Current.** `side_chip_click` jumps to the Library page (`desktop_ui.c:8224-8227`); `side_chip_keeps_playlists` empties the
list (`:8201-8203`).

**Target (inventory sections 8.1, 9.1, 10.1, 11.1).**
- Chips: Playlists, Albums, Artists, Composers, Sources, Local Files (decision D2 on the last label). Overflow scrolls with the
  chevrons (I669 kept).
- A selected chip turns white-filled with dark text and a round clear (X) button appears left of the chips (sections 8.1, 10.1);
  the other chips hide except the second-level chips of that kind (Spotify "By you" / "By Spotify" under Playlists, section
  10.1). Selecta second level: Playlists: "By You", "Imported" (only when imported playlists exist); Albums and Artists: one chip
  per source with items ("Local", "Modland", ...). Second level is row I678d, optional for the first build.
- A chip **never** calls `desk_page_goto` and never touches the main area: main page, its scroll offset and the page history are
  unchanged by any chip click.
- Local Files chip: the list shows the single Local Files row (section 11.1); the main area still does not change; a click on
  that row opens the Local Files page.
- Chip, text filter and sort compose: chip (kind mask) AND filter text, then sort.

**Data source.** The kind mask goes to the library call (section 2), so filtering happens in the store, not in a UI loop.
`side_chip_keeps_playlists` and `side_chip_lib_mode` are deleted.

**Reachability test (dui-sidebar-chips).** From Home, for each chip k in order: click it; assert `nav page=home` (unchanged),
`nav_clicks` unchanged, `home_y` unchanged, and `sidelib n` equals the fixture count of that kind (A albums, R artists, C
composers, S sources, 1 for Local Files, P playlists). Click the X: `sidelib n` back to the section 2 total. Sentinel: `side_filters`
grows by one per click. Revert proof: on old code the Albums chip gives `page=library` and `n=0`.

---

## 5. Sort: Recents, Recently Added, Alphabetical, Creator

**Current.** `sel_uim_lsort_cmp` (`browse.h:42`): Recents keeps engine order (playlist id order, oldest first), Creator is always
"You" (I611 found).

**Target (inventory "Spotify sidebar menu").** Menu: "Sort by" Recents (checked, accent), Recently Added, Alphabetical, Creator;
"View as" strip (section 6). Meanings per kind:
- Recents: newest of (last played song of the item, last time the item page was opened). Pinned rows stay on top.
- Recently Added: playlists by creation, albums and sources by first scan time of their first song, artists and composers by the
  first song added.
- Alphabetical: name, case-insensitive, leading "The " ignored for artists (decision D4).
- Creator: playlists by owner; albums by artist; artists and composers by name; sources by source name.

**Data source.** `last_played` and `added` come from the store (section 2); "page opened" times are written by the engine
through a new `LIBRARY_TOUCH kind id` command into a small `opened(kind, id, at)` table, so the Recents order is the same in
every UI and survives a restart. `sel_uim_lsort_cmp` takes the item struct (kind-aware). Sort and view stay persisted as
`ui.desktop.library_sort` / `ui.desktop.library_view` (I611).

**Reachability test (dui-sidebar-recents).** Fixture with three albums; script plays a song of album 3, then opens album 1's page,
then returns Home. With Recents, the non-pinned `sidelib_row` order starts album 1, album 3. Switch to Alphabetical through the
menu: order equals `sqlite3 ... ORDER BY name COLLATE NOCASE`. Sentinel: `lib_touch_calls` 1 (the open wrote the touch).

---

## 6. Density and the expanded sidebar

**Current.** `SEL_UIM_LVIEWS` Compact list, List, Small grid, Large grid (`browse.h:39-40`), `side_lib_cols` (`desktop_ui.c:1021`);
width toggle `side_wide_toggle` (`:498`, I668).

**Target.**
- Compact list: 32 px rows, no thumbnail, title and "Type • Subtitle" on one line.
- List: 48 px thumbnail rows (default, sections 8.1, 10.1).
- Small grid / Large grid: square tiles (round for artists and composers), title under the tile, subtitle under the title.
- Expanded sidebar (Spotify's diagonal-arrows control): the sidebar widens to about 40 percent of the window and List view
  becomes a table with columns Title, Type, Date Added, Played (relative "2 days ago"); grid views fill the wider area. Collapsed
  rail (icon-only) shows thumbnails only.

**Data source.** View choice in prefs (I611); column values from the item (section 2); relative time formatting in
`src/uimodel` (shared formatter).

**Reachability test (dui-sidebar-density).** For each of the four views, pick it in the menu and read `sidelib rows=<drawn>
row_h=<px> cols=<n>`: compact row_h 32, list 48, small grid cols >= 2, large grid cols < small grid cols. Expand: `sidelib cols`
line reports 4 table columns and the sidebar region width is at least 0.4 of the canvas (from the `region sidebar` line).

---

## 7. The collection page (one page type for every list of songs)

**Current.** The playlist page (`pls_open`, `desktop_ui.c:4172`; cover 160 px at `:4213`), the composer drilldown (`:6307`), the
bare `draw_detail` (`:1392`) and the album search are four unrelated renderings.

**Target (inventory sections 8.2, 10.2, 11.2).** One page, `PG_COLLECTION`, with a kind:
- Header band with a gradient from the cover's dominant colour fading into the panel colour by the end of the header (sections
  8.2, 10.2). Square cover about 200 px (round for artist and composer); or, for pinned items without art, the large wordmark
  title (Local Files, section 11.2).
- Type label ("Album", "Single", "Playlist", "Artist", "Composer", "Source"), very large bold title, meta line: avatar, owner or
  artist in bold, then muted "• year • N songs, total length".
- Action row: round accent Play button (about 56 px), Shuffle, kind actions (Add to Liked Songs check for albums; Add to Queue;
  More menu), right side: search field and sort label (Custom Order, Title, Artist, Album, Date Added, Duration) with the list
  icon.
- Track list: column header "#", "Title", "Album" (or "Plays" on album pages, from history counts), duration clock; rows with
  number (play icon on hover), 40 px thumbnail (not on album pages), title with artist below, album, duration. The playing song
  shows its title in the accent colour.
- Under the list, per kind: album: release year, "More by <artist>" shelf; playlist: "Find more" and "Recommended" (similar
  songs, I-series later); artist and composer: see section 8.

**Data source.** A new engine command `COLLECTION_OPEN <kind> <id>` publishes `st->collection` (header fields, total songs and
length, rows windowed like `VIEW_WINDOW`). The engine builds it from the store: playlist entries, album songs, artist songs,
composer songs (`sel_library_composer_page`, `db.h:523`), source songs, liked, history, local files. The gradient colour comes
from the artwork module (`plugins/ui/desktop/art.c` already decodes covers; the dominant colour is computed once per art id and
cached there, not per frame). Sort and search of the track list are engine view commands (as I611's `COMPOSERS_VIEW`), not UI
loops.

**Reachability test (dui-collection-page).** Open album 1 from the sidebar: `page=collection kind=album`, `collection songs=<N
from sqlite3 for that album>`, `collection rows_drawn` equals min(N, rows that fit), title line equals the album name, type
"Album". Click Play: `queue_count` = N and the first queued key is the album's first song by track order. Sentinel:
`collection_opens` 1, `collection_play` 1. Repeat for a playlist (type "Playlist", songs = entries) to prove one page type serves
both.

---

## 8. The page set and each kind's page

**Target, kind by kind (all built on section 7 unless noted).**
- Home: section 11.
- Search / Browse: unchanged pages (`PG_SEARCH`, `PG_BROWSE`); Browse tiles for formats, composers, sources open the matching
  collection page instead of a search.
- Album page: section 8.2 of the inventory: cover, "Album", title, artist avatar and name (click opens the artist page), year,
  song count and length, Plays column, "More by <artist>" shelf of that artist's other albums.
- Artist page: round avatar header, "Artist", name, "N albums, M songs"; Play and Shuffle; "Popular" (top 5 by play count, then
  "Show more" to 10); "Albums" shelf (cover cards with year); "Appears On" (compilations naming the artist) when present.
- Composer page: as artist, labelled "Composer"; the song list is the composer's songs (replaces the Library drilldown and its
  "Back to all composers" line, `desktop_ui.c:3651`); "Spellings" note when the composer has aliases (`composer_spellings`).
- Playlist page: section 10.2 of the inventory: cover or mosaic, "Playlist", owner, Add, rename ("Name and Details"), Custom
  Order; today's `pls_*` code becomes the playlist kind of the collection page.
- Source page: source icon header, "Source", name, "N songs", source status (sync state, toggle; today on the Library Sources tab,
  `desktop_ui.c:6494`), then the source's folder tree (section 10).
- Liked Songs, Recently Played, Local Files pages: wordmark header (section 11.2), Play, Shuffle, search and sort; Recently
  Played sorts newest play first and shows a "Played" column.
- The right panel (detail region) keeps showing the playing context, not the open page (inventory 8.3, 9.3, 10.3: Spotify does
  the same).

**Data source.** Per kind, the engine `COLLECTION_OPEN` builder (section 7); artist and composer shelves come from new store
queries (`albums WHERE artist = ?`, top songs by `history` count).

**Reachability test (dui-kind-pages).** For each kind in the fixture (album, artist, composer, playlist, source, Liked Songs,
Recently Played, Local Files): click its sidebar row, assert `page=collection kind=<kind>` and `collection songs` equals the
`sqlite3` count for that item; for the artist page also `collection shelf albums=<count of that artist's albums>`. Sentinel:
`collection_opens` = 8 after the script.

---

## 9. Click-to-page map and history

| Click | Opens in the main area | Sidebar effect |
|---|---|---|
| Door Home / Search (top bar) | Home / Search | none |
| Header title "Your Library" | decision D1 | none |
| Chip, chip X, chevrons | nothing | filters the list |
| Sidebar row of any kind | that item's collection page | row marked open |
| Home quick-access tile or shelf card | the item's page (album, playlist, artist, composer) | open row marked if listed |
| Home song card | plays the song (no page) | none |
| Shelf "Show all" | Shelf page (I617) | none |
| Artist or album name on a track row | artist / album page | open row marked if listed |
| Artist name on an album page meta line | artist page | |
| Album card in "More by" or artist "Albums" | album page | |
| Composer name in Song Information | composer page | |
| Browse tile (format, composer, source) | the matching collection page | |
| Track row (double-click / Enter) | plays from that row in the page's order (queue = the page) | none |

Back and Forward (top bar chevrons) walk one history of pages, each entry `(page, kind, id, scroll)`; today's per-page `hist`
(`desktop_ui.c:337`, cleared on every page change at `:7601`) is replaced by one app-wide history so Back from an album page
reached from an artist page returns to the artist page.

**Data source.** The page model (`desk_page` + collection kind and id) moves its routing table to `src/uimodel` (a
`sel_uim_route` table: click target kind to page), so the MUI UI uses the same map. The desktop UI calls the route; it holds no
per-kind branching.

**Reachability test (dui-click-map).** Script: Home, click a Home album tile (album page), click the artist name (artist page),
click Back (album page, same scroll), click Back (Home), click Forward (album page). Assert the `nav` line sequence
`home, collection:album, collection:artist, collection:album, home, collection:album`, and `route_calls` = 3 (tile, artist, the
Forward replays without routing: count from the route table's own counter).

---

## 10. Where each Library page tab goes; whether the Library page stays

| Tab today (`browse.c:8`) | Goes to | Why |
|---|---|---|
| Composers | Composers chip in the sidebar + composer pages (section 8) | Spotify lists kinds in the sidebar |
| Tree | "Folders" on each source page (section 8) and a "Browse Folders" page from Search / Browse | the tree is per source in practice |
| Columns | "Browse Folders" page, column view toggle | same data as Tree, other view |
| Files | Local Files page (local) and source pages (remote) | file listing per source |
| History | Recently Played pinned item | Spotify has it as a list item |
| Sources | Sources chip + source pages; source settings also in Settings | per-source page holds the toggle and Sync |

The Library page itself (`PG_LIBRARY`) is retired once every tab has its new home (row I678i, decision D3). The header title
"Your Library" then no longer opens it (D1). The I677 (b) Albums tab is not built: albums live in the sidebar (Albums chip) and
in the Home "Your Albums" shelf "Show all" grid.

**Data source.** Tree, Columns and Files keep their engine view lines (`sel_uim_libmode_lines`, `browse.c:20-40`), now sent by
the Browse Folders and source pages.

**Reachability test (dui-library-retired).** `--view library` maps to Home (or Browse Folders per D3); no frame draws the six-tab
row (`lib_tabs drawn=0` in every page of a scripted walk through all doors). Click the Sources chip, then a source row: source page
with `collection kind=source` and its folder rows drawn (`view rows` from the engine's browse source > 0). Sentinel:
`browse_folders_opens` 1 from the Search / Browse tile.

---

## 11. The Home feed shelves

**Current.** Quick picks grid (`QUICK_*`, `desktop_ui.c:1881`), shelves from `sel_library_home` (Recently played, Jump back in,
New this week; `db.h:414-417`), Your composers shelf (`engine.h:99`), Show all (I617).

**Target (inventory section 3).** In order:
1. Chips "All", "Music", "Modules" (tracker formats), "Chiptunes" (SID, AY, SAP and other chip formats): filter the Home shelves
   only (decision D5 on the chip set).
2. Quick access grid: 8 tiles (2 columns by 4 rows on narrow, 4 by 2 on wide), the 8 newest items by section 5 Recents across all
   kinds, pinned Liked Songs first. Same items as the sidebar's Recents top; one source.
3. "Recently Played" (items, not songs: the albums, playlists, composers played lately).
4. "Jump Back In" (albums with a skipped song and no finished play: today's query).
5. "Your Albums", "Your Composers", "Your Playlists" (most played first).
6. "New This Week" (modland changelog count).
7. "More Like <recent artist or composer>" from the similar-songs store (`src/library/db/similar.c`), when it has rows.
Every card opens the item's collection page (section 9); a song card plays.

**Data source.** `sel_library_home` grows item shelves from the same `sel_library_items` query (section 2) with a shelf id per
line; the UI draws shelves in the order the engine sends.

**Reachability test (dui-home-shelves).** Fixture with history: the `home shelf` lines name the shelves in the order above
(only those with rows), quick access tile count is 8 (or the item total when fewer), and tile 1 is "Liked Songs". Click a "Your
Albums" card: `page=collection kind=album` with that album's id. Sentinel: `home_item_shelves` > 0 (the new item shelf builder
ran).

---

## 12. Amiga bitmap tier mapping (desktop UI, `desk_bitmap_text`)

The bitmap tier (AGA, 320x256 and 640x256 screens; `desktop_ui.c:713`) runs the same model, routes and pages; only drawing
reduces. MUI parity is a separate track (I670) that consumes the same `src/uimodel` route table and item formatter.

| Feature | Mac (rich) | Amiga bitmap tier |
|---|---|---|
| Sidebar list | mixed list, thumbnails | mixed list, text rows with a one-letter kind mark (P, A, R, C, S) in muted colour; pinned rows marked "*" |
| Chips | pill row, X, chevrons | one cycle button naming the chip in force (`side_chip_single`, kept); key K cycles chips |
| Sort | menu | key O cycles sort (I611), label shows the sort |
| View as | four views | Compact list only; grid views skipped in the cycle |
| Expanded sidebar | table columns | not offered (screen too narrow) |
| Collection header | gradient, 200 px cover, big title | flat panel colour, 64 px cover at 640 wide, none at 320; title in the large bitmap font |
| Play button | round accent | text button "Play"; Enter on the header plays |
| Track list | thumbnail, two-line cell | one line: number, title, duration |
| Home | chips, quick grid, shelves | quick grid as 4 text rows; shelves as one line per card; Enter on a shelf header opens Show all (I617) |
| Back / Forward | top bar chevrons | Backspace / Shift+Backspace |

**Data source.** Same store calls and route table; per-tier choices are in the layout (`src/uimodel/layout.c`) and the UI's tier
checks only pick a draw routine.

**Reachability test (dui-aga-library).** Same script as section 4 and section 8 at `--canvas 640x256` with the AGA bitmap tier:
`sidelib n` per chip equals the Mac counts (the cycle button stepped with key K), a row Enter opens `page=collection` with the same
`collection songs`, and `collection header_art_px` = 64 (0 at 320x256). Sentinel: `side_chip_cycles` grows per K press. Rig check
at the end of the batch on a real mod (memory real-song-for-rig-checks).

---

## 13. Decisions (Johan, AskUserQuestion 2026-10-29)

Johan chose the recommended option (a) for every decision: D1 the title does nothing; D2 a "Local Files" chip; D3 the Library page retires; D4 Alphabetical ignores a leading "The"; D5 Home chips All, Music, Modules, Chiptunes; D6 generated round art from the name hash. The options as asked:


- D1 Header title "Your Library" click: (a) nothing, as Spotify (Recommended: the title is a label in Spotify; the chips and
  rows do the work); (b) clears the chip and filter; (c) keeps opening the Library page until I678i lands.
- D2 The Downloaded equivalent chip: (a) "Local Files" chip listing the Local Files row (Recommended: matches Johan's own reading
  of section 11); (b) "Downloaded" listing Local Files plus cached remote sources; (c) no chip, Local Files only pinned.
- D3 Library page: (a) retire it, tabs move per section 10 (Recommended: one way to every list, Spotify has none); (b) keep it as
  "Browse Folders" for Tree, Columns, Files only; (c) keep all six tabs beside the new sidebar.
- D4 Alphabetical for artists ignores a leading "The": (a) yes (Recommended: what music players do); (b) no.
- D5 Home chips: (a) All, Music, Modules, Chiptunes (Recommended: Selecta's real split by format family); (b) All, Music only;
  (c) none.
- D6 Artist avatar without a photo: (a) round generated art from the name hash (Recommended: no network needed); (b) round mosaic
  of their album covers.

---

## 14. Row split for sonnet builds

Each row names its section; the build agent reads that section only. Order is dependency order. Each row: build, host tests for
its section, revert proof; one rig run at the end of the batch (I678k).

| ID | Section | Scope | Files | Depends on | Replaces |
|---|---|---|---|---|---|
| I678a | 2 | `sel_library_items` (union query, kind mask, paging, counts), engine wrapper, item struct, subtitle formatter in uimodel; db and engine unit tests | `src/library/db/` (new `items.c`), `db.h`, `src/engine/engine.c`, `include/selecta/engine.h`, `src/uimodel/browse.c` | none | I677 (a) data part |
| I678b | 3, 4 | sidebar draws the mixed list with pinned rows; chips Playlists, Albums, Artists, Composers, Sources, Local Files filter through the kind mask; X clear; delete `side_chip_keeps_playlists` and `side_chip_lib_mode`; open-row mark per kind | `plugins/ui/desktop/desktop_ui.c` (side_* block ~8043-8660), tests `dui-sidebar-mixed`, `dui-sidebar-chips` | I678a | I677 (a), I673 stand-ins (chip jump, empty kind list) |
| I678c | 5 | Recents by last played or opened, `LIBRARY_TOUCH` + `opened` table, kind-aware `sel_uim_lsort_cmp`, real Creator | `src/library/db/schema.c`, `items.c`, `commands.def`, `browse.c`, test `dui-sidebar-recents` | I678a | I611 open part (tools row now acts on the one mixed list; the Library page rows it could not filter go away with I678i) |
| I678d | 6, 4 second level | four densities for all kinds, expanded sidebar table, second-level chips | `desktop_ui.c`, `browse.c`, test `dui-sidebar-density` | I678b | none |
| I678e | 7 | `PG_COLLECTION` page, `COLLECTION_OPEN`, header with gradient (dominant colour in art.c), action row, track list, sort and search; playlist page ported onto it | `desktop_ui.c`, `art.c`, `src/engine/`, `commands.def`, test `dui-collection-page` | I678a | none (pls page becomes a kind) |
| I678f | 8 | album, artist, composer kinds (shelves "More by", "Popular", "Albums"); composer drilldown removed | `src/engine/`, `src/library/db/`, `desktop_ui.c`, test `dui-kind-pages` (album, artist, composer part) | I678e | I677 (a) row click part |
| I678g | 8 | source, Liked Songs, Recently Played, Local Files kinds (wordmark header, source status block) | same, test `dui-kind-pages` (rest) | I678e | none |
| I678h | 9 | route table in uimodel, every click in the map wired, app-wide Back / Forward history | `src/uimodel/` (new `route.c`), `desktop_ui.c`, test `dui-click-map` | I678f, I678g | I673 header door (per D1) |
| I678i | 10 | Library page retired (per D3): Browse Folders page, tabs moved, `--view library` remapped, goldens reblessed | `desktop_ui.c`, `browse.c`, `doors.c`, test `dui-library-retired`; old tests dui-library, dui-library-sources, dui-library-tools, dui-sidebar-library-door merged or deleted | I678h | I677 (b) Albums tab (not built), I611 Library page tools row |
| I678j | 11 | Home chips, quick access from Recents, item shelves | `src/library/db/` (home), `desktop_ui.c`, test `dui-home-shelves` | I678c, I678f | none |
| I678k | 12 | bitmap tier: kind marks, K chip cycle, compact-only view, reduced header, Backspace history; one rig run for the batch | `desktop_ui.c`, `layout.c`, test `dui-aga-library` | I678b, I678e, I678h | none (I670 MUI stays separate) |


After the split: I677 closes as replaced by I678a, b, f, i; I673 stand-ins replaced by I678b and I678h; I611 open part replaced by
I678c and I678i.
