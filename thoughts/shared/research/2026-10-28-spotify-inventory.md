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
