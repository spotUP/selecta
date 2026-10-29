# Spotify desktop UI inventory (Mac, dark theme)

Source: /Users/spot/Pictures/claude-inbox/CleanShot 2026-10-28 at 11.45.53@2x.png
Coordinates below are in the displayed frame (2000 x 1084). Multiply by 1.36 for the original 2728 x 1478.
Overall: a dark (near-black) app window with three floating rounded panels (left, centre, right) on a black frame, a top bar above them, and a player bar below.

## Proportions (approximate, displayed frame)

- Window: 2000 wide x 1084 tall. Top bar y 0-92. Panels y 93-955. Player bar y 958-1084.
- Left sidebar (Your Library): x 10-468, about 458 px (23% of width). Rounded panel, slightly lighter than the frame.
- Centre main: x 481-1515, about 1034 px (52%). Gap of about 13 px to each side panel.
- Right panel (now playing / context): x 1528-1988, about 460 px (23%).
- Gutters between panels: about 12-13 px. Outer margin: about 10 px.

## 1. Top bar (y 10-80, on the black frame)

- Traffic lights (macOS window controls): red, yellow, green dots at x 37, 66, 95, y 45. Native, not app UI.
- Back and forward chevrons: thin grey `<` and `>` at x 154 and 203, y 45. Look dimmed (history at start).
- Home button: circular dark-grey filled button about 70 px in diameter, centre x 645, y 45. White filled house icon. Visually separate from the search pill, with a gap of about 12 px.
- Search pill: x 692-1386, y 10-80, fully rounded (pill), dark grey fill (about #242424). Left magnifier icon at x 727 (white/light grey, outline). Placeholder text "What do you want to play?" in light grey, about 20 px, starts x 763.
- Search pill divider: thin vertical line at x 1310.
- Browse icon inside search pill, right end: tray/box-with-arrow icon at x 1346 (the "Browse" entry point).
- Top-right cluster (on the frame, no pill behind):
  - Bell icon (notifications) at x 1823, y 45, outline, light grey.
  - Two-people icon (Friends / Activity) at x 1882, y 45, outline, light grey.
  - User avatar at x 1952, y 45: circle about 46 px, orange fill, white letter "S". Blue dot badge at top-right of the circle (about x 1970, y 27), indicating unread or update.
  - Avatar sits in a slightly lighter rounded square behind it (about x 1918-1988, y 10-80).

## 2. Left sidebar: Your Library (x 10-468, y 93-955)

- Header "Your Library": bold, white, about 20 px, x 38, y 141.
- Header right controls:
  - Plus icon (create playlist) at x 356, y 141, light grey outline, no circle.
  - Expand/collapse icon (diagonal arrows) at x 420, y 141.
- Filter chips row (y 214): pill chips, dark grey fill, white text about 16 px.
  - "Playlists" (x 33-142)
  - "Podcasts" (x 155-273)
  - "Albums" (x 287-391), text looks dimmer than the first two (grey text, likely lower emphasis). Not clearly a selected state.
  - Right chevron in a round dark button at x 423 (scrolls more chips).
  - No chip shows a selected fill in this frame.
- Promo card "Import your music from other apps" (y 273-445, x 33-445):
  - Gradient background: purple to deep blue, purple dominant at top.
  - Close X at x 416, y 302, white thin icon.
  - Title bold white, about 18 px: "Import your music from other apps" (two lines, wrapped).
  - Subtitle regular white, about 16 px: "Bring your playlists, songs and artists."
  - Button "Import library": white pill, black bold text about 17 px, x 56-234, y 376-422, about 46 px tall.
  - Spotify logo: green circle with black arcs, on a dark rounded tile at right, x 348-428, y 350-430, tilted slightly.
  - Card corners: rounded, about 12 px.
- Search and sort row (y 495):
  - Magnifier icon (search the library) at x 55, grey outline.
  - "Recents" sort label at x 324-398, grey text, about 17 px, with a dropdown-style list icon at x 422 (sort/view toggle; list icon = list view).
- Library list rows (vertical pitch about 92 px, row heights about 70 px thumbnail):
  - Row 1, "Liked Songs" (y 543-612): thumbnail 70 x 70, square, purple to light-blue gradient, white filled heart in centre. Title white, about 20 px. Subtitle: green pin icon (pinned item), then "Playlist • spotup-us", grey, about 16 px. Pinned item.
  - Row 2, "Local Files" (y 635-704): square thumbnail, navy/blue fill, green folder icon. Title white. Subtitle: green download arrow icon (downloaded), "Playlist • 6 843 tracks".
  - Row 3, album (y 727-800): square thumbnail (album art, Swedish album). Title truncated with ellipsis "Sånger om tvivelaktiga förhå..." Subtitle "Album • Skärgårdsoppositionen" grey.
  - Row 4, "Your Episodes" (y 823-893): square thumbnail, teal-green fill, white bookmark icon. Subtitle "Playlist • Saved & downloaded ep..." truncated.
  - Row 5, "Coast Contra" (y 918-955+): circular thumbnail (artist avatar), cut off at the bottom edge, so the list scrolls.
- Row text left edge x 120; thumbnails x 33-103.
- No explicit badge seen. No hover or selected row shown.

## 3. Main page (x 481-1515, y 93-955)

Background: dark grey, slightly lighter than the frame, with a subtle top-to-bottom tone.

- Filter chips (y 122-168): pill chips, about 46 px tall.
  - "All": selected, white fill, black text, bold-ish, x 540-598.
  - "Music": dark grey fill (about #2a2a2a), white text, x 611-699.
  - "Podcasts": same style, x 712-830.
  - "Audiobooks": same style, x 844-990.
  - No overflow chevron visible.
- Quick-access grid: 2 columns, 4 rows, each tile a dark grey rounded rectangle about 70 px tall.
  - Column A tiles at x 540-991, column B at x 1005-1456. Row y: 192-262, 274-344, 356-426, 438-508. Gap about 12 px.
  - Each tile: square thumbnail (70 x 70, flush left, rounded left corners) and bold white title, about 19 px, left at x 622 (column A) and x 1087 (column B).
  - Items, reading order:
    1. "Roseland NYC Live 25 (Remastered 2023)" (two lines, live photo thumbnail)
    2. "Digital Dancehall - Music genre - Melodigging" (two lines)
    3. "Jamaican Dancehall Classics" (one line, colourful cartoon thumbnail)
    4. "Sånger om tvivelaktiga förhållanden bland kobbar och skär" (two lines)
    5. "Zara Larsson" (one line, portrait)
    6. "Roliga låtar" with emoji in the title (two lines; content text, not chrome)
    7. "Coola låtar" with emoji (one line)
    8. "JUMP" (one line, pink art)
  - No hover state shown.
- Section label "Made For" (y 569): small, grey, about 16 px, x 540.
- Section title "spotup-us" (y 606): large, bold, white, about 32 px, x 540. Clearly the largest text on the page.
- "Show all" link: grey, about 17 px, right side at x 1366-1445, y 611.
- Card row (4 cards, horizontal, the last one clipped at the panel edge):
  - Card size 225 x 225, square, rounded about 6 px, top at y 656. Lefts at x 540, 800, 1061, 1321 (clipped).
  - Card 1: "Discover Weekly" art, bold white and black block letters on multicolour gradient, Spotify green dot badge top-left (about 14 px).
  - Card 2: "Daily Mix 01", illustrated art on teal, yellow "Daily Mix" label, large "01" numeral, Spotify dot badge top-left.
  - Card 3: "Daily Mix 02", yellow/red illustration, "02" numeral.
  - Card 4: "Daily Mix 03" (clipped), dark grey photo, red label, "03" partly visible.
  - Under each card: description text in grey, about 16 px, two lines, ellipsised, e.g. "Your shortcut to hidden gems, deep...", "Inna De Yard, Michael Palmer, King Kong a...", "I Wayne, Jah Cure, Burro Banton and...", "danjerr, Tokyo Tears, skyfall beats and...".
  - No card titles visible; the card art itself carries the label. Card text is grey, not white.

## 4. Right panel: now playing context (x 1528-1988, y 93-955)

- Full-bleed artwork fills the whole panel (cartoon-style illustrated portrait, warm orange and brown tones). Bottom fades to black (gradient overlay) to hold the text.
- Header top-left: "Jamaican Dancehall Classics" bold, white, about 20 px, x 1551, y 139. This is the playing context (playlist or album name), not the track name.
- Track block near the bottom:
  - Track title "Who Am I", bold, white, about 32 px, x 1551, y 645. Second largest text in this panel.
  - Artist "Beenie Man", grey/white, about 18 px, y 683.
  - Circular plus-in-circle button (add to library or playlist) at x 1952, y 660, white outline.
- Section "Related music videos" (y 755): bold, white, about 20 px.
- Video thumbnails (horizontal row, clipped at the bottom of the panel):
  - First card x 1551-1836, y 795-955, about 285 x 160, rounded corners about 4 px, colourful photo.
  - Second card starts at x 1858, cropped at the panel edge, dark purple/red photo.
- No visible close button, no tabs on this panel, no like button in this panel.

## 5. Player bar (y 958-1084, full width, black)

Left group:
- Album art thumbnail: square about 78 px, x 25-103, y 985-1060, rounded corners about 4 px. Shows the track's cartoon art.
- Track title "Who Am I", white, about 18 px, x 126, y 1009.
- Artist "Beenie Man", grey, about 15 px, x 126, y 1035.
- Add-to-library (circle plus) icon at x 263, y 1021, outline, light grey. This is the like or save control; it is a plus in a circle, not a heart, in this frame.

Centre group (transport):
- Shuffle icon at x 869, y 1003: grey, no active-state colour visible. Off state.
- Previous track (skip back) at x 928: white filled, same size as next.
- Play button at x 997, y 1003: large white filled circle about 50 px diameter, black filled triangle play icon. Most prominent control in the bar.
- Next track (skip forward) at x 1068: white filled.
- Repeat icon at x 1127: grey, no active indicator visible. Off state.
- Progress row (y 1050):
  - Elapsed time "0:56", light grey, about 14 px, x 638-676.
  - Progress track x 688-1308, thin (about 4 px), grey (dark) background, white fill to x 866 (about 30 percent played). No visible thumb circle in this frame.
  - Total time "3:16", light grey, x 1321-1358.

Right group (left to right):
- Lyrics or related icon at x 1581, y 1021: appears dimmed (disabled or low-emphasis). Could not identify with confidence; likely the lyrics control (microphone-style or quote glyph).
- Queue icon (list with lines) at x 1628, y 1021: light grey outline, lines of different lengths.
- Connect to a device icon at x 1676, y 1021: device/speaker glyph, light grey outline.
- Volume icon (speaker) at x 1722, y 1021, light grey.
- Volume slider: x 1746-1882, y 1021, thin track, white fill nearly full (to about x 1880), no thumb visible.
- Mini-player (picture-in-picture style) icon at x 1917, y 1021, outline.
- Full-screen icon at x 1964, y 1021, outline (four corner brackets).
- All icons are light grey outlines about 20 px; the play button is the only filled white circle in the bar.

## 6. Anything else

- No notification toast, no modal, no tooltip, no context menu visible.
- No hover or focus state visible anywhere (cursor not shown in the capture).
- Window has no visible title bar text; the app chrome is the top bar.
- Two-tone typographic hierarchy: bold white titles, grey secondary text; section headers are bold white at two sizes (about 20 px and 32 px).
- Corners: panels about 12 px radius, chips and search fully pill-shaped, thumbnails about 4 px radius, cards about 6 px.
- Palette: near-black frame (about #000), panel grey (about #121212 to #1a1a1a), tile grey (about #2a2a2a), white text, grey secondary text, Spotify green used sparingly (pin icon, download icon, logo).

## Could not read or uncertain

- Exact glyph identity of the icon at x 1581 in the player bar (dimmed; possibly lyrics).
- Whether the Albums chip is disabled or simply lower-contrast.
- Exact hex colours (estimated from the image, not sampled).
- Tooltips, hover states, and the true state of shuffle and repeat (shown grey, read as off).
- Anything outside this window (menus, other pages, scrolled content below the visible rows).

## Library sort and view menu (screenshot 2026-10-28 11.50.15)


## Spotify sidebar menu (source: CleanShot 2026-10-28 at 11.50.15@2x.png)

Crop covers only the top-right of the library panel. The left edge of the sidebar is not in the image, so the opener is read from the crop only.

### Opener
- Trigger: list icon (lines with dots, about x=545 y=44 in the 2x image) at the right end of the header row, next to the text "Recents". The "Recents" text is the current sort label and is also part of the trigger.
- Menu anchors below the header, left-aligned at about x=144 (2x px), spanning to about x=560. Panel top about y=92, bottom about y=686.

### Menu items, in order
1. Section header "Sort by" (grey, not selectable, no icon)
2. "Recents": green text, checkmark at right. Checked state.
3. "Recently added": white, no check
4. "Alphabetical": white, no check
5. "Creator": white, no check
6. Separator line (thin grey, full inner width, about y=508)
7. Section header "View as" (grey, not selectable)
8. View-as segmented control, 4 icon buttons in a rounded strip:
   a. Compact list icon (three horizontal lines), not selected
   b. List icon (lines with dots), green, selected: sits on a rounded grey pill
   c. Small grid (3x3 dots), not selected
   d. Large grid (2x2 squares), not selected

Not visible: keyboard shortcuts, submenu arrows, disabled items, icons next to the sort items (none shown).

### Visual treatment (estimated by eye, not measured)
- Panel: about 416 x 594 px at 2x (about 208 x 297 pt). Dark charcoal background, roughly #282828. Corner radius small, about 4-6 pt. Soft dark shadow, faintly visible at the edges.
- Section headers: grey (about #B3B3B3), medium weight, about 18 pt at 1x.
- Item labels: white, regular weight, about 20 pt at 1x. Selected label is green (Spotify green).
- Row height about 80 px at 2x (about 40 pt). Left text inset about 32 px at 2x (about 16 pt).
- Hover row: none visible in the screenshot.
- View-as strip: darker rounded container, selected button has a lighter grey rounded background.

### Other visible state (versus a normal library list)
- Library list behind the menu is in list view: first row "Liked Songs" with purple-to-blue heart tile, green pin, then a folder tile, a podcast-style cover, a bookmark tile.
- Bottom now-playing bar shows "Who Am I" by Beenie Man with an add (plus-circle) button.
- Nothing else in the crop is clearly changed from a normal sidebar. The left sidebar is not in the crop, so I cannot confirm anything about it.

### Could not read
- Exact pixel sizes, colors, shadow values and corner radius are estimates from the image, not measured.
- Whether the trigger is in the left sidebar: the crop shows the right side of the library header only.

## 8. Album page (sidebar album clicked), shot 2026-10-29 01.06.45

Source: /Users/spot/Pictures/claude-inbox/CleanShot 2026-10-29 at 01.06.45@2x.png
Coordinates in the displayed frame (2000 x 1649). Multiply by 1.64 for the original 3274 x 2700.
Overall: same window layout as section 1 to 7 (black frame, three rounded panels, top bar, player bar), but the frame is now taller and the centre panel shows an album page. Colours are visual estimates, not sampled values.

### 8.1 Left sidebar (Your Library, x 162-542, y 230-1390)

- Header "Your Library" at (240,270), bold white. Circular "+" button at (448,270) and expand icon at (501,270), both light grey outline.
- Filter row: round dark "X" (clear filter) at (200,331). Active chip "Albums" at (273,331): white filled pill, black text. This is the only active chip. The chip row has no other chips visible.
- Sort row: search icon at (200,381). "Recents" label at (465,382) with a list-style sort icon at (503,382), grey.
- Selected row: "Hi Bich" at y 568-646, x 172-532. Background is a flat lighter grey rounded rectangle (about #2a2a2a, estimate). Title white, subtitle grey.
- Row pitch about 78 px. Rows at approx y 447, 527, 605 (selected), 683, 761, 839, 917, 995, 1073, 1152, 1230, 1308. Thumbnails are 58 px squares with a small corner radius, at x 181-239. No circular thumbnails in this list (all rows are albums or singles).
- Subtitle pattern: optional type word, then a bullet "•", then artist. Examples: "Single • Bhad Bhabie", "Single • Jahneration", "Single • Vana Liya". Albums without the type word show the artist only: "Space", "Staffan Götestam", "Skarra Mucci", "Bombus", "Dirty Dike", "Hieroglyphics". Subtitle is grey, about 20 px, title about 22 px white. Long titles truncate with an ellipsis ("Sånger om tvivelaktiga förhå...").

### 8.2 Middle column: album page (x 553-1448, y 230-1390)

- Background gradient: fills the top of the panel from y 230 to about y 515, then fades to the near-black panel colour (about #1a1a1a at y 520, estimate). Top colour is dark red, about #6a0f17 (estimate). The colour appears to come from the cover art (cover is red-dominant). Unclear whether it is sampled from the image or a fixed per-album value.
- Cover: square, x 575-771, y 296-492 (about 196 px). Has a small explicit ("E") mark at the bottom right corner of the art.
- "Single" type label at (818,301), white, about 20 px, regular weight.
- Title "Hi Bich": very large bold white, x 797-1137, cap height about 90 px displayed (about 55 pt at 1x, estimate). The largest text on the page.
- Artist row at y 478: small circular avatar (about 28 px) at (808,478), then "Bhad Bhabie" bold white, then grey "• 2017 • 1 song, 1 min 44 sec" on the same line.
- Action row at y 571:
  - Play: solid green circle, centre (608,571), about 68 px diameter, white play triangle. Green about #1ed760 (estimate).
  - Cover thumbnail in a bordered box, x 666-712, y 543-600 (about 46 x 58 px). Looks like a "now playing / follow" mini-cover. Its purpose is unclear.
  - Shuffle icon at (755,571), grey outline.
  - Added-to-library check: green filled circle with white check at (816,571), about 36 px.
  - Download icon: circle with down arrow at (877,571), grey outline.
  - More "..." at (939,571), grey.
  - Right side: "List" label at (1373,571) with a list icon at (1407,571). This is the list/compact view toggle.
- Track list header at y 651: "#" at x 607, "Title" at x 651, "Plays" at x 1146, clock icon at x 1356. Grey, about 20 px. Divider line at y 674, x 576-1425.
- Track row 1 (y 690-745):
  - Number "1" at (605,721).
  - Title "Hi Bich" white at (665,710).
  - Artist line below in grey at (645,735): explicit "E" badge followed by "Bhad Bhabie".
  - Plays "152 756 415" right-aligned under the Plays column at (1116,721).
  - Duration "1:44" at (1351,721).
  - No hover or playing state visible on the row. Unclear whether the row is in a playing state.
- Below the list:
  - Release date "September 21, 2017" at (651,808), about 20 px, light grey.
  - Copyright lines "© 2017" at (598,829) and "℗ 2017" at (598,850), smaller grey.
  - "More by Bhad Bhabie" section heading at (716,960), bold white, about 28 px. "See discography" grey link at right (1350,964).
  - Four square album cards, about 190 px, at x 575, 795, 1015, 1236 and y 1002-1192. First card has a light outline (focus or hover, unclear).
  - Card titles at y 1216: "15", "Ms. Whitman", "22 (Remix)", "What U Like". Years at y 1245: 2018, 2025, 2021, 2026. Cards are not explicitly tagged with type.
  - Nothing visible below y 1260 inside the panel until the panel bottom at y 1390.

### 8.3 Right panel (x 1459-1838, y 230-1390)

- Top label "Jamaican Dancehall Classics" at (1604,268), white bold, about 22 px. This matches the now-playing context shown in the player bar, not the album. Unclear whether it is a playlist, a radio or a "now playing" context name.
- Large artwork card from about y 300 to 800, showing a comic-style illustration. Overlay text "Who Am I" bold white at (1543,686), artist "Beenie Man" at (1523,717). Circle plus icon at (1808,699), white outline.
- "Related music videos" heading at (1573,778), bold white.
- Video cards: first at x 1478-1715, y 811-944, subtitle "Girls Dem Sugar" at y 967, artist line "Beenie Man, Mýa" at y 995 (grey). Second card is cut off at the right edge (x 1735-1820), "So Me Sta..." and "Beenie Man".
- "About the artist" card: x 1478-1819, y 1036-1390. Top half shows an artist portrait over the card. Title overlay "About the artist" white at (1570,1068). Below the portrait, name "Beenie Man" bold white with a green verified badge at (1550,1296). Text "5 116 435 monthly listeners" grey at y 1332-1360. Outlined "Follow" pill button at (1752,1346), white text.

### 8.4 Top bar and player bar

- Top bar, same arrangement as section 1, with these differences:
  - Back chevron at (282,190) is bright white. Forward chevron at (322,190) is grey. The back control is enabled (there is history), where section 1 had both dimmed.
  - Home circle at (705,190), search pill x 745-1323, y 161-219. Placeholder "What do you want to play?" unchanged. Browse icon at (1289,190).
  - Bell (1701,190), friends (1750,190), avatar (1808,190) with blue dot. Unchanged.
- Player bar (y 1390-1490):
  - Now-playing: thumbnail about 64 px at (207,1443). Title "Who Am I" at (296,1433), artist "Beenie Man" at (297,1456). Circle plus at (373,1443).
  - Transport centre: shuffle (891,1428), previous (940,1428), play circle white with black icon (999,1428), next (1058,1428), repeat (1107,1428).
  - Progress: "0:56" at (710,1468), bar x 736-1263 at y 1468, filled to about x 887, "3:16" at (1290,1468).
  - Right cluster: a dimmed link-style icon (1499,1443), queue (1539,1443), devices (1579,1443), volume (1617,1443) with white bar x 1637-1749, PiP (1780,1443), fullscreen (1819,1443).
  - Playing state: "Who Am I" is playing, not "Hi Bich". The page shows a different track from the one in the player.

Unclear: whether the track row for "Hi Bich" shows a playing state, what the mini-cover box at (688,571) does, and the source of the header gradient colour.

## 9. Podcast page (sidebar podcast clicked), shot 2026-10-29 01.07.26

Source: /Users/spot/Pictures/claude-inbox/CleanShot 2026-10-29 at 01.07.26@2x.png
Coordinates in the displayed frame (2000 x 1649). Multiply by 1.64 for the original 3274 x 2700.
Overall: same window layout as section 1 to 8 (black frame, three rounded panels, top bar, player bar). The centre panel shows a podcast show page. Colours are visual estimates, not sampled values.

### 9.1 Left sidebar (Your Library, x 162-542, y 230-1390)

- Header "Your Library" at (240,270), bold white. Circular "+" at (448,270) and expand icon at (501,270), light grey outline. Same as section 8.
- Filter row: round dark "X" at (200,331). Active chip "Podcasts" at (279,331): white filled pill, black text. This is the only active chip. Section 8 had "Albums" in the same place.
- Sort row: search icon at (200,381). "Recents" label at (467,382) with list icon at (503,382), grey. Unchanged.
- Two rows only, the rest of the list area (y 580-1390) is empty:
  - "Your Episodes" at y 437 (title at x 316). Thumbnail: green square, x 181-239, y 420-478, white bookmark icon. Subtitle "Playlist • Saved & downloaded e..." (grey, truncated). Not a podcast row; it is a system playlist.
  - "Magiska Godnattsagor" at y 516, subtitle "Magiska Godnattsagor" at y 543 (grey). Thumbnail x 181-239, y 500-557 (about 58 px), robot mascot cover, small corner radius.
- Selected state: the show row (y 490-567, x 172-532) has a flat lighter grey rounded background (about #2a2a2a, estimate). Title and subtitle stay white and grey as on the other row. No accent colour, no left bar.
- Row kind: podcast show rows use a square thumbnail, no type word and no bullet in the subtitle. The subtitle is the publisher name (same as the title here). Album rows in section 8 used "Type • Artist" or artist only.

### 9.2 Middle column: podcast page (x 553-1448, y 230-1390)

- Background gradient: fills the top of the panel from y 230 to about y 515, deep navy blue (about #1a1f8c at top, estimate). It darkens toward y 600 and reaches near-black panel colour (about #121212) by about y 640. Section 8 was dark red on the same layout. Source of the colour unclear (probably cover art, which is blue-dominant).
- Cover: square, x 575-771, y 296-492 (about 196 px), same size as the album cover in section 8. Robot mascot art with "MAGISKA GODNATTSAGOR" lettering. No "E" mark.
- "Podcast" type label at (825,374), white, about 20 px, regular weight. Section 8 used "Single" at (818,301), placed higher.
- Title "Magiska Godnattsagor": very large bold white, x 794-1409, text band y 395-447 (about 60 px displayed, estimate). Wider than the album title (section 8: x 797-1137 for "Hi Bich").
- Publisher line "Magiska Godnattsagor" at (949,475): bold white, about 28 px, x 794-1103. No avatar, no year, no track count. Section 8 had a 28 px circular avatar and grey "• 2017 • 1 song" text on this line. Podcast has no rating in the header; rating is in the chip row below.
- Action row at y 571:
  - "Following" outlined pill, x 575-693, y 552-590, white text, centre (634,571). Section 8 had a filled green Play circle here.
  - "..." (three grey dots) at (736,571).
  - No green Play button, no shuffle, no download, no added-to-library check, no list toggle. Nothing at the right side of this row.
- Promo banner: x 575-1425, y 629-697, dark grey (about #2a2a2a, estimate). "Exclusive benefits for members" bold white at (730,652). "Bonus Episodes" grey at (650,677). Outlined "Try free" pill at x 1310-1411, y 645-682, white text. Not in section 8.
- "About" heading at (617,745), bold white, about 40 px displayed (estimate). Not in section 8.
- Description text, light grey, about 22 px, left-aligned at x 575, lines at y 796, 823, 850, 877, 904. Swedish text, cut after the 5th line with a URL line. "... Show more" bold white link at (634,930). Line 1 is a short title line, line 2 starts "Valkommen till Magiska Godnattsagor".
- Trailer card: x 575-1425, y 973-1080, dark grey (about #2a2a2a). Thumbnail x 589-667, y 987-1065 (about 78 px) with white play triangle overlay. Title "Trailer Magiska Godnattsagor" white bold, about 22 px, at (816,1014). Small outlined tag "Trailer" at (707,1041), then "2 min 21 sec" grey at (790,1041). "..." at (1387,1026).
- Chip row at y 1129: rating pill "4,5 star (1,9K)" x 575-698, y 1110-1148, dark grey. Dot separator at (712,1129). Category pill "Stories" x 727-809, y 1110-1148, dark grey. Unclear whether these pills are clickable.
- Episode list header at y 1217: "All Episodes" dropdown, bold white, at (585,1217) with down arrow at (713,1217). Search icon at (1221,1217). "Newest to Oldest" sort dropdown at (1252,1217) with down arrow at (1406,1217), grey. Divider line at y 1257, x 575-1425. Section 8 had a "#  Title  Plays  clock" column header instead.
- Episode row 1 (y 1278 onward, cut off by the panel bottom at 1390):
  - Thumbnail x 589-804, y 1278-1390+ (about 215 px wide, larger than the 58 px album thumbnails). Rabbit-on-purple artwork.
  - Blue filled dot at (838,1292) = unplayed indicator (unclear, estimate).
  - Title "Kaninen som blev en enhörning" white, about 22 px, at (854,1292).
  - Show name "Magiska Godnattsagor" grey at (834,1322).
  - Description starts at y 1364, grey, text begins with a sparkle symbol, then "Vill ni ha ännu fler magiska godnattsagor?" and a club sign-up line, cut at the panel bottom.
  - Date, duration, progress bar and per-row play button: not visible in this shot (cut off). Unclear.
- Nothing below y 1390 is visible in the panel (the scroll continues).

### 9.3 Right panel (x 1459-1838, y 230-1390)

- Unchanged from section 8.3: "Jamaican Dancehall Classics" label at (1604,268), art card with "Who Am I" / "Beenie Man" at (1543,686) and (1523,717), "Related music videos" at (1573,778), video cards "Girls Dem Sugar" and a cut-off "So Me Sta...", and "About the artist" card with Follow at (1752,1346).
- The right panel does not change to show the podcast. It still shows the music now-playing context.

### 9.4 Top bar and player bar

- Top bar: same as section 8.4. Back chevron at (282,190) bright white (enabled), forward at (322,190) grey. Search pill x 745-1323, y 161-219. Bell, friends and avatar unchanged.
- Player bar (y 1390-1490): unchanged from section 8.4. Still shows "Who Am I" / "Beenie Man", progress 0:56 of 3:16. The podcast episode is not playing; opening the show did not change the player.
- Transport shows shuffle, previous, play, next, repeat. No podcast-specific controls (skip 15 s / speed) visible in this shot. Unclear whether they appear while an episode plays.

Unclear: the exact gradient colour source, whether the rating and category pills are clickable, the unplayed-dot meaning, and the episode row's date, duration and play button (below the crop).

## 10. Playlist page (sidebar playlist clicked), shot 2026-10-29 01.08.15

Source: /Users/spot/Pictures/claude-inbox/CleanShot 2026-10-29 at 01.08.15@2x.png
Coordinates in the displayed frame (2000 x 1649). Multiply by 1.64 for the original 3274 x 2700.
Overall: same window frame, top bar, player bar and right panel as section 8. The centre panel shows a playlist page. Colours are visual estimates, not sampled values.

### 10.1 Left sidebar (Your Library, x 162-542, y 230-1390)

- Header unchanged from section 8: "Your Library" at (240,270), "+" at (448,270), expand icon at (501,270).
- Filter row: clear "X" at (200,331). Active chip "Playlists" at (275,331), white filled pill, black text. Dark inactive chips "By you" at (371,331) and "By Spotify" (clipped) at (460,331), with a chevron at (503,331) to scroll the chips. Section 8 had "Albums" as the active chip.
- Sort row: search icon at (200,382). "Recents" at (452,382) with list icon at (503,382). Same as section 8.
- Selected row: "Midsommar med Janika" / "spotup-us" at y 646-722, x 172-532. Flat lighter grey rounded background, same treatment as section 8. Title white, subtitle grey.
- Row pitch about 78 px. Rows at approx y 450 (Liked Songs), 530 (Local Files), 607 (Your Episodes), 686 (selected), 760, 840, 918, 995, 1077, 1141, 1219, 1298, 1375 (clipped at panel bottom).
- Thumbnails: 58 px squares with a small corner radius, at x 181-239. Some are a single cover (the selected row: yellow with blue band). Others are a 2x2 mosaic of cover art (Midsommar med janika, Valborg 2026, Lova, Midsommar 2025, spotUP's Discogs Collection). Liked Songs is a purple-to-blue gradient square with a white heart. Local Files is a blue square with a green folder icon. Your Episodes is a green square with a bookmark icon.
- Subtitle pattern: no type word and no bullet. Plain owner name: "spotup-us" for own playlists, "Spotify" for This Is Tokyo Tears, "Malin Lindqvist" for Valborg 2026, "celestialyouth" for SONGS THAT ACTUALLY SO... Special subtitles: Liked Songs has a green pin icon before "spotup-us"; Local Files has a green download icon before "6 844 tracks"; Your Episodes shows "Saved & downloaded episodes".
- Unavailable row: "This playlist is not available" at (366,1077), with a grey music-note placeholder thumbnail and a small equaliser-style glyph before the text. Unclear what the glyph means.
- Long titles truncate with an ellipsis ("SONGS THAT ACTUALLY SO...", "Jag sköt palme och satte Chr...").

### 10.2 Middle column: playlist page (x 553-1448, y 230-1390)

- Background gradient: fills the top of the panel from y 230 to about y 515, then fades to the near-black panel colour. Top colour is indigo-purple, about #3b2a7a (estimate). It appears to come from the cover (yellow with a blue band). Unclear whether it is sampled from the image. Section 8 (album) was dark red.
- Cover: square, x 575-771, y 296-492 (about 196 px). A single cover image (yellow, blue band, "FUCKING CONSUMER" text), not a mosaic. It is the same art as the first track's album.
- Type label "Public Playlist" at (846,394), white, about 20 px, regular weight. Section 8 had "Single" in the same spot.
- Title "Midsommar med Janika" at x 794-1228, y 430, bold white, about the same size as the section 8 title (estimate). No description line visible.
- Owner row at y 478: circle with a "+" glyph at (808,478), about 28 px. Then "spotup-us" bold white, then grey "• 3 songs, 16 min 26 sec". No saves count and no followers count visible. The circle is either a placeholder avatar or an add control; unclear.
- Action row at y 571:
  - Play: solid green circle, centre (608,571), about 68 px, white play triangle.
  - Small cover box at x 666-712, y 543-600 with a light outline, showing the cover. Same position as the section 8 mini-cover box. Purpose unclear.
  - Shuffle at (755,571), grey outline.
  - Person-with-plus icon at (816,571), grey outline. Section 8 had a green check here (added to library). Probably "invite collaborators"; unclear.
  - Download circle with down arrow at (877,571), grey outline.
  - More "..." at (939,571).
  - Right side: search icon at (1241,571), then "Custom order" at (1333,571) with a list icon at (1407,571). Section 8 had a "List" view toggle in this spot. Here the label reads as a sort control; unclear whether it also changes the view.
- Playlist-only chips at y 655: "+ Add" at x 575-672, "Mix" with a mix icon at x 682-774, "Name & details" with a pencil icon at x 784-966. Section 8 has none of these.
- Track list header at y 705: "#" at x 607, "Title" at x 634, "Album" at x 1044, clock icon at x 1356. Divider at y 728, x 576-1425. Section 8 had a "Plays" column where this page has "Album". No "Date added" column is visible; unclear whether the layout hides it.
- Track rows (pitch about 68 px). Each row: number at x 605, cover thumb at x 635-684, two-line cell (title white above, artist grey below), album at x 1044, duration at x 1351.
  1. y 776: thumb yellow (same art as the cover). Title "Space Invaders Are Smoking Grass" at (700,764). Artist "I-F" at (709,789). Album "Fucking Consumer". Duration 6:11.
  2. y 844: thumb "INDULGENCE" art (y 820-868). Title "Indulgence" at (700,833). Artist "Maverick Masters" at (700,857). Album "Indulgence". Duration 6:17.
  3. y 912: thumb orange/teal art (y 889-937). Title "Tro Rätt Tro Fel" at (700,901). Artist "Dia Psalma" at (700,926). Album "Gryningstid". Duration 3:58.
- No explicit-"E" badge visible on these rows. No plays column. No row shows a playing or hover state, although the player bar is playing a different track.
- "Find more" at (1366,998), white bold, right-aligned, below the track list.
- "Recommended" heading at (678,1065), bold white, about 28 px. Subtitle "Based on what's in this playlist" in grey at (691,1100).
- Recommended rows: thumb at x 587-636, title above artist at x 650, album name at x 918, outline pill "Add" button at x 1330-1404. Rows at y 1176, 1244, 1313, 1381 (clipped). Rows: "Åka Hiss" / Tripnaha / album "Åka Hiss"; "Saga" / Tripnaha / album "Saga"; "Kickstart My Heart - Instr..." / Parralox / album "Holiday '23"; "The Sadist, Pt. II" / artist clipped / album "Apocalypse Dom...". No duration column in this block.
- The panel scrolls (thin scroll thumb at x about 1442, y 230-1015). Content below the Recommended rows is not visible in this shot.

### 10.3 Right panel (x 1459-1838, y 230-1390)

- Content identical to section 8.3: top label "Jamaican Dancehall Classics" at (1604,268), artwork card "Who Am I" / "Beenie Man", "Related music videos" with "Girls Dem Sugar" and clipped "So Me Sta...", "About the artist" card with verified "Beenie Man", "5 116 435 monthly listeners", and "Follow" button at (1752,1346).
- The right panel shows the playback context, not the open playlist. Unclear why "Jamaican Dancehall Classics" shows while "Midsommar med Janika" is open.

### 10.4 Top bar and player bar

- Top bar, same as section 8.4: back chevron at (282,190) bright white (enabled), forward at (322,190) grey, home at (705,190), search pill x 745-1323, y 161-219 with placeholder "What do you want to play?" and browse icon at (1289,190). Bell (1701,190), friends (1750,190), avatar "S" with blue dot (1808,190).
- Player bar: same as section 8.4. "Who Am I" / "Beenie Man" is playing, 0:56 of 3:16. The playing track is not in the open playlist.

Unclear: purpose of the mini-cover box at (688,571); what the "+" circle at (808,478) does; source of the header gradient colour; whether "Custom order" only sorts or also changes the view; why no "Date added" column is shown (window width or playlist layout; not verified).

## 11. Downloaded page (sidebar Downloaded clicked), shot 2026-10-29 01.09.08

Source: /Users/spot/Pictures/claude-inbox/CleanShot 2026-10-29 at 01.09.08@2x.png
Coordinates in the displayed frame (2000 x 1649). Multiply by 1.64 for the original 3274 x 2700.
Overall: same window frame, top bar, player bar and right panel as sections 8 and 10. The centre panel shows the "Local Files" playlist (the only item in the Downloaded filter). Colours are visual estimates, not sampled values.

### 11.1 Left sidebar (Your Library, x 162-542, y 230-1390)

- Header unchanged from section 8: "Your Library" at (240,270), "+" at (448,270), expand icon at (501,270).
- Filter row: clear "X" at (200,331). Active chip "Downloaded" at (294,331), white filled pill, black text. Inactive dark chip "Playlists" at (415,331). Section 10 had "Playlists" as the active chip and section 8 had "Albums". No chevron visible.
- Sort row: search icon at (200,382). "Recents" at (452,382) with list icon at (503,382). Same as sections 8 and 10.
- Only one row is listed: "Local Files" / "Playlist • 6 844 tracks", at y 412-490, x 172-532. Flat lighter grey rounded background, the same selected-row treatment as section 10. Title white, subtitle grey.
- The subtitle has a green circle with a down-arrow icon before "Playlist", which marks the item as downloaded. Section 10 used a green pin for Liked Songs and a green download icon for Local Files before "6 844 tracks"; here the same icon sits before "Playlist". The track count is 6 844.
- Thumbnail: 58 px square, x 181-239, blue with a green folder icon. Same as the Local Files thumbnail in section 10.
- Below the single row the sidebar is empty down to y 1390. No other rows, no empty-state message.

### 11.2 Middle column: Local Files page (x 553-1448, y 230-1390)

- Background gradient: fills the top of the panel from y 230 to about y 515, then fades to the near-black panel colour. Top colour is a saturated violet-blue, about #3010e0 (estimate). It is brighter than the indigo of section 10. No cover image is involved.
- No cover art. The large white wordmark "Local Files" sits at x 576-1106, y 330-425, bold, roughly 110 px cap height (estimate). It replaces the square cover of section 10.
- Subtitle "Files from your computer" at (685,472), white, regular weight.
- No type label ("Public Playlist" is absent), no title line under a cover, no owner row, no avatar circle, no saves or follower count, no duration total. The page shows no "Playlist" or "Downloaded" label in the header.
- Action row at y 571:
  - Play: solid green circle, centre (608,571), about 68 px, white play triangle.
  - Shuffle at (686,571), grey outline.
  - Search icon at (1251,571), then "Custom order" at (1333,571) with a list icon at (1407,571). Same position and label style as section 10.
  - No Add-collaborator, Download or "..." icons are visible. No mini cover box. The section 10 icons at (755,571), (816,571), (877,571) and (939,571) are absent.
- No "+ Add", "Mix" or "Name & details" chips. Section 10 had them at y 655; here the space is empty.
- Track list header at y 651: "#" at x 607, "Title" at x 634, "Album" at x 1044, clock icon at x 1356. Divider at about y 675. Section 10 had the header at y 705 and an "Album" column in the same place.
- Track rows (pitch about 68 px). Each row: number at x 598-612, grey placeholder thumbnail at x 636-684 (a grey square with a music-note glyph, not cover art), a single-line title at x 700, Album column empty, duration at x 1351.
  1. y 721: "Synchronize", 5:29
  2. y 789: "GTCC-Receive_and_Dispatch_Alar..." (truncated), 1:08
  3. y 858: "Infographics", 1:10
  4. y 926: "Infographics_18", 0:24
  5. y 995: "Infographics_26", 0:04
  6. y 1063: "Infographics_10", 1:00
  7. y 1131: "Infographics_31", 1:08
  8. y 1200: "Infographics_11", 0:01
  9. y 1268: "Infographics_12", 1:08
  10. y 1337: "Infographics_4", 1:10
  11. y 1385: row clipped at the panel bottom, content not readable.
- No artist line under any title, so the rows are single-line and the artist name is not shown. The album column is empty for every visible row. No "Recommended" block, no "Find more" link. The list is cut off at row 11; any content below is not visible.
- No row shows a playing or hover state, although the player bar is playing a different track.

### 11.3 Right panel (x 1459-1838, y 230-1390)

- Content identical to section 10.3: top label "Jamaican Dancehall Classics" at (1604,268), artwork card "Who Am I" / "Beenie Man", "Related music videos" with "Girls Dem Sugar" and clipped "So Me Sta...", "About the artist" card with verified "Beenie Man", "5 116 435 monthly listeners", and "Follow" button at (1752,1346).
- The panel shows the playback context, not the open Local Files page. Same mismatch as section 10.3.

### 11.4 Top bar and player bar

- Top bar, same as section 10.4: back chevron at (282,190) bright white (enabled), forward at (322,190) grey, home at (705,190), search pill x 745-1323, y 161-219 with placeholder "What do you want to play?" and browse icon at (1289,190). Bell (1701,190), friends (1750,190), avatar "S" with blue dot (1808,190).
- Player bar: same as section 10.4. "Who Am I" / "Beenie Man" is playing, 0:56 of 3:16. The playing track is not in the open page.

### 11.5 Differences from section 10 (playlist page)

- Sidebar: Downloaded chip active instead of Playlists; one row (Local Files) instead of a list of about 13; the row carries a green download icon.
- Header: large wordmark title and a "Files from your computer" subtitle; no cover art, no type label, no owner row.
- Action row: only Play, Shuffle, Search and Custom order. No collaborator, download or more icons, no mini cover box.
- No "+ Add / Mix / Name & details" chips, no "Find more", no "Recommended" block.
- Track rows: single-line titles with no artist line, grey placeholder thumbnails, empty Album column. Section 10 had two-line rows with cover art and album names.

### Unclear

- Whether the middle column is the Local Files playlist itself or a filtered view of downloaded items. The sidebar label "Playlist • 6 844 tracks" and the custom-order track list suggest the playlist itself, but the shot does not prove it.
- Whether the "X" chip at (200,331) clears the Downloaded filter (it appears to be the same clear control as section 10).
- Meaning of the green download icon before "Playlist" in the sidebar row: most likely downloaded status, not verified.
- Whether the header gradient is sampled from the page or fixed to the Local Files style; unclear.
- Whether "Custom order" only sorts or also changes the view (not verified; same question as section 10).
- Whether the track list continues past row 10 (6 844 tracks in total), and what the scroll thumb shows; not visible in this shot.
- Why the Album column is empty for every row: local files without album tags, or a layout choice; not verified.
- Why the grey thumbnails carry no artwork, and whether rows can show artwork at all for local files; not verified.
- Why the right panel shows "Jamaican Dancehall Classics" while the open page is Local Files (same unexplained mismatch as section 10).
