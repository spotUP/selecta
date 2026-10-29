---
date: 2026-10-29
author: spot
status: design, awaiting Johan's answers in section 6
row: I691
---

# I691: platform covers for songs and albums without art

Johan 2026-10-29: most formats Selecta plays have no album art and never will. A song or album without a cover
gets a generic cover chosen by its platform family, drawn from design tokens, never a grey placeholder. Real
cover always wins.

Violated invariant today: the fallback (`sel_gui_genart_draw`, src/gui/image/gui_genart.c:75) is a random-looking
gradient plus a 5x5 glyph keyed on a hash. It says nothing about what the song is, and every album site passes
the literal `"album"` instead of a format, so a shelf of Amiga albums and a shelf of SNES albums look the same.

## 1. Platform families

### The existing tables (searched: `genart`, `sel_format_`, `SEL_RP_EXTENSIONS`, `"sndh"`, `Amiga|Atari|Nintendo`)

| Table | Where | Maps | Linked by the UIs? |
|---|---|---|---|
| `k_chips` | src/gui/image/gui_genart.c:20 | extension -> chip ("paula", "sid", "ay", "2a03", "software"), 9 rows | yes (selecta_gui, desktop and MUI) |
| `g_formats` | src/library/formats_table.c:9 | Modland name -> extension, 50 rows (design doc 9.1) | no (selecta_library only; `sel_format_by_ext` has zero callers outside the file) |
| `SEL_RP_EXTENSIONS` | each replayer adapter (nuade, openmpt built at runtime) | replayer -> extensions it claims | no (registry, engine side) |

No platform or family table exists anywhere. The chip table `k_chips` is already "format -> the hardware it plays
on", which is a platform family in all but name, and it is the only one both UIs link.

**Decision: grow `k_chips` (gui_genart.c:20) into the family table** (`{ext, family}` rows, family an enum), and
derive `sel_gui_genart_chip` from the family (Amiga -> paula, Commodore 64 -> sid, ...) so the chip strings stop being a
second mapping. No new parallel table. `g_formats` stays the Modland name table; a unit test (I691a) walks
`sel_format_get(0..count-1)` and requires every Modland extension to resolve to a family other than Music, which
keeps the two tables consistent without copying one into the other.

### How a song maps to a family

`sel_gui_platform_of_name(const char* name)`: the last extension (`x.mod`) and, when that misses, the Amiga prefix
(`mod.x`, the part before the first dot), case-insensitively. This is the same rule the registry uses for claims
(src/engine/claims.h:11). It replaces the three hand-written extension extractors in the desktop
(`queue_format` desktop_ui.c:1468, the inline loop desktop_ui.c:3245-3256) with one call; `lower_ext` in
src/engine/view.c is engine side and stays.

### The family list (recommended; Johan decides, section 6 Q1)

| Family (label on the cover) | Extensions (from k_chips, g_formats, gme/ym/openmpt adapter lists) | Mark |
|---|---|---|
| Amiga | mod stk nst med mmd0-3 okt digi dbm symmod fc13 fc14 hip hipc hip7 dw fred bp bp3 sid1 sid2 dm1 dm2 sfx tfx mc rh jpn ahx hvl | four vertical bars (Paula's four voices) |
| Commodore 64 | sid | three horizontal bars of falling length (three SID voices) |
| Atari | ym sndh sap | three vertical bars on a baseline |
| ZX Spectrum | ay | four diagonal stripes |
| PC Tracker | xm s3m it mtm 669 far ult gtk rad | a 4x6 grid of pattern cells, some filled |
| Nintendo | nsf nsfe spc gbs | a plus-shaped pad |
| Sega | gym vgm | a ring |
| Console | hes kss | two filled circles side by side |
| Music | mp3 mp2 ogg flac, anything unknown | one filled circle (a disc) |

Long tail: nuade claims a few hundred UADE prefixes built at runtime (plugins/replayers/nuade/adapter.c:594). The UI
cannot ask the registry which replayer owns a file. Rows list the prefixes Selecta's own fixtures and Modland
mirror use; an unlisted Amiga prefix falls back to Music. If that gap matters, section 6 Q4.

## 2. Look of each cover

Flat, Spotify-like (the reference design for the desktop UI):

- **Field:** one solid fill of the family colour. No gradient sweep, no noise. The title hash picks one of three
  tones of that colour (base, base mixed 12% toward black, base mixed 12% toward white) so a shelf of ten Amiga
  albums is not ten identical squares (keeps the I360 property: two titles differ) while staying one family.
- **Mark:** the family mark from the table above, top right, in `ACCENT_TEXT` at 85% over the field, sized edge/4,
  built only from `sel_gui_fill_rect` and `sel_gui_round_rect` (radius = half the side makes a circle). Integer only,
  vector (drawn per size), no bitmaps, no logos or trademarks.
- **Word:** the family label in full English words (`Amiga`, `Commodore 64`, `PC Tracker`, never `C64` or `PC`)
  bottom left, semibold, sized edge/7, in `ACCENT_TEXT`. Drawn only when the cover is at least 64 px on the edge
  (Home tiles, grid, cover flow, album page header, Now Playing panel and page); the player bar and sidebar thumbs
  (about 40-56 px) show field and mark only.
- **Excluded:** no cream or off-white field, no italic words, no monospace label, no pill shapes, no emojis, no
  numbered labels. Corners follow the caller's radius as today.

### Tokens

Token file: include/selecta/uimodel/tokens.h (`SEL_GUI_TOKENS` X-macro, line 14). **New tokens are needed:** eight
role tokens, one per family except Music, which uses the existing `ACCENT`:

`COVER_AMIGA "gui.cover.amiga"`, `COVER_C64 "gui.cover.commodore_64"`, `COVER_ATARI`, `COVER_SPECTRUM`,
`COVER_PC_TRACKER`, `COVER_NINTENDO`, `COVER_SEGA`, `COVER_CONSOLE`, each `0u..0xFFFFFFu`. Proposed defaults (all
dark enough that `ACCENT_TEXT` 0xE4E4EA reads at 4.5:1 or better; the sonnet row measures it): Amiga 0xC2410C,
Commodore 64 0x4338CA, Atari 0x15803D, ZX Spectrum 0xB91C1C, PC Tracker 0x0E7490, Nintendo 0xBE185D, Sega 0x1D4ED8,
Console 0x7E22CE. A theme overrides them like any other role token.

### API

`sel_gui_genart_draw(c, r, format, title_hash)` has no token or font access. It becomes
`sel_gui_cover_draw(sel_gui_canvas* c, sel_gui_rect r, uint32_t family, uint32_t title_hash, const uint32_t* tokens, const sel_gui_atlas* label_font)`;
`label_font == NULL` draws no word. The profiling slot (`SEL_GUI_PROF_GENART`) and `SEL_GUI_RGN_ART` region stay.

### INDEX8 / AGA and MUI

`sel_gui_genart_palette` fills palette entries 32..63 with 16 hues x 2 (src/gui/draw/gui_draw.c:162). It becomes
`sel_gui_cover_palette(out[32], tokens)`: 9 families x 3 tones = 27 entries plus `ACCENT_TEXT`, so the AGA tier draws
the exact cover colours with no dither and no extra palette slots. MUI uses the same `sel_gui_cover_draw` through
plugins/ui/mui/map/mui_canvas.c, so the idea (field, mark, word) is identical; MUI passes NULL for the font until a
live MUI site exists (section 3), so MUI shows field and mark.

## 3. Where it applies

**Replace, not layer.** genart is exactly the anonymous placeholder Johan rejects; keeping it under or beside the
platform cover would leave two fallback systems and two palettes in the 32-slot AGA budget. The platform cover takes
over every genart call; gui_genart.c keeps `sel_gui_str_hash` and becomes the cover module.

| Site | file:line | Today | Switches |
|---|---|---|---|
| Quick picks card (Home) | desktop_ui.c:3186 | genart "album" when no art_tile | yes, family of the row (I691c data) |
| Home shelf tile | desktop_ui.c:3275 | genart format or "album" when no art_tile | yes |
| Collection shelves card | desktop_ui.c:4631 | genart "album" always, never real art | yes; real art via `c->id` is a gap (Found) |
| Artist / composer round avatar | desktop_ui.c:4888 | genart "album" cached in col_av | yes, family of the person's songs |
| Album / playlist page header | desktop_ui.c:5016 | genart "album" | yes (album: family; playlist: Q3) |
| Library grid tile | desktop_ui.c:7018 | genart "album" always, art_id ignored | yes; real art first (Found) |
| Cover flow | desktop_ui.c:7114 | genart "album" always, art_id ignored | yes; real art first (Found) |
| Home queue cards | desktop_ui.c:8164 | genart of ext when no art_tile | yes, `sel_gui_platform_of_name(path)` |
| Sidebar library row thumb | desktop_ui.c:9553 | genart "album" after cover_draw (I692) | yes, mark only |
| Player bar art | desktop_ui.c:9801 | genart f->format after cover_draw (I692) | yes, mark only |
| Now Playing panel | desktop_ui.c:9890 | genart f->format, counts np_genart | yes; np_genart counter renamed np_platform |
| Now Playing page | desktop_ui.c:10075 | genart f->format after cover_draw (I692) | yes |
| MUI grid | plugins/ui/mui/map/mui_canvas.c:77 | genart tile format | yes; but `mui_canvas_set_grid` (mui_canvas_class.c:165) has no caller, unreachable today |
| MUI art | plugins/ui/mui/map/mui_canvas.c:102 | genart on the non-RTG path | yes; `mui_canvas_draw_art` is called only from tests/ui/mui_canvas_test.c |
| Song Information | none | no artwork drawn there today | no change (no site exists) |

## 4. Precedence

1. Real art: `cover_draw` (desktop_ui.c:1510, art_known then load_tile_art then art_tile), the Home `art_tile(r->art_ref)`
   blits, MUI `px != NULL` / datatype path. Unchanged.
2. Platform cover of the family.
3. Nothing else. No grey square, no genart, no empty slot.

**Song:** family of its file name (extension, then Amiga prefix).

**Album, artist, composer (mixed formats):** the family holding the most songs of that album or person; tie goes
to the family earlier in the table order of section 1 (Amiga first). Songs whose family is Music count only when no
other family has a song, so one stray mp3 intro does not turn an Amiga album into Music. The engine supplies the
data (it owns the library); the UI only maps. Concretely the engine returns the majority family's representative
extension (`format`) per album row; the UI resolves it with the same `sel_gui_platform_of_name`.

**Playlist:** see Q3 (recommended: majority rule like an album).

## 5. Build rows (sonnet, one at a time; each names this doc section)

**I691a: family table and cover draw (section 1, 2).** Files: src/gui/image/gui_genart.c, include/selecta/gui/gui_genart.h,
include/selecta/uimodel/tokens.h, src/gui/draw/gui_draw.c:162. Grow `k_chips` into `{ext, family}`, add
`sel_gui_platform_of_name`, `sel_gui_platform_label`, `sel_gui_cover_draw`, `sel_gui_cover_palette`; eight tokens.
Keep `sel_gui_genart_draw` as a thin wrapper until I691b/d land, then delete it in I691e. Test: unit test next to
tests/dui/draw_unit.c (new `cover_unit.c`, pattern of draw_unit.c) linked with selecta_library: every
`sel_format_get(i)->ext` maps to a non-Music family (50 of 50 rows), `mod.foo` and `foo.mod` both give Amiga, `x.mp3`
gives Music; the field pixel equals one of the three tones of the family token; two titles give different tones in
at least 2 of 3 pairs; a 48 px cover draws no word (pixel count of `ACCENT_TEXT` below a bound), a 128 px one does.
Acceptance: 9 families, 27 palette entries, contrast of `ACCENT_TEXT` on each base tone >= 4.5.

**I691b: desktop song sites (section 3, rows 8164, 9801, 9890, 10075).** Files: plugins/ui/desktop/desktop_ui.c only.
Replace `queue_format` and the inline extractor at 3245 with `sel_gui_platform_of_name`. Dump line
`platform_covers np=<family> bar=<family> page=<family> tile0=<family> draws=<n>` (the family name in lower case,
`none` when real art won), counter bumped inside the fallback branch as the sentinel. Test: new
tests/dui/platform_covers.cmake copying genart.cmake (named_tile_hash, `QUEUE_ADD fixture/Tune.<ext>`) and
cover_tiles.cmake (`--input` play script, dump regex): mod -> amiga, sid -> commodore_64, xm -> pc_tracker,
spc -> nintendo; `mod.Tune` -> amiga; the art-music fixture song -> `none` with cover_tiles still bar>=1 np>=1.
Update genart goldens (`SELECTA_GOLDEN_UPDATE=1`) and retitle dui-genart to the platform cover in the same row.

**I691c: engine supplies album and person formats (section 4).** Files: include/selecta/engine.h (append `const char* format`
to `sel_view_row`; `char format[16]` to `sel_engine_collection_row` and `sel_engine_library_item`, fields only
appended), src/engine/view.c, src/library/db/songdb.c (majority query over songs grouped by album / artist /
composer, extension from path). Test: host engine test with the art-music fixture plus a mixed fixture album
(3 mod, 1 mp3) asserting `format` = mod for that album, and a 2 mod / 2 xm album tie -> mod (Amiga before PC Tracker).

**I691d: desktop album and person sites (section 3, rows 3186, 3275, 4631, 4888, 5016, 7018, 7114, 9553).** Files:
desktop_ui.c. Each site passes the row's `format`; grid (7018) and cover flow (7114) gain the real-art-first branch
through `cover_draw` with the row's art key, so precedence holds there too. Dump: extend `platform_covers` with
`shelf=<family> grid=<family> flow=<family> side=<family> header=<family>`. Test: extend platform_covers.cmake with the
mixed fixture library: Home shelf album tile -> amiga, grid view -> amiga, cover flow -> amiga, album page header ->
amiga; a covered album -> none in grid and flow (proves real art wins where it never did).

**I691e: MUI, AGA palette, delete genart (section 2 INDEX8/MUI, section 3 MUI rows).** Files:
plugins/ui/mui/map/mui_canvas.c (`mui_tile.format` resolved through `sel_gui_platform_of_name`), src/gui/draw/gui_draw.c,
remove `sel_gui_genart_draw`/`sel_gui_genart_palette`/`sel_gui_genart_chip` once no caller is left (count callers from
the link map, not grep). Test: tests/ui/mui_canvas_test.c cases at lines 74-106 assert the platform tone for `"mod"`;
tests/dui/aga_budget.cmake still passes with palette entries 32..58 equal to the cover tones. Reachability exemption,
written: no live MUI site calls the canvas grid or art path today; the MUI rig check happens with the MUI row that
wires a grid.

Rig check at the end of the batch (Mac first: Home, Library grid, cover flow, album page, Now Playing with a .mod
from the Modland mirror; then the AGA tier on the rig with virgill-that_meme_again.mod).

## 6. Open questions for Johan

Q1. Family list. Recommended: the nine in section 1 (Amiga, Commodore 64, Atari, ZX Spectrum, PC Tracker, Nintendo,
Sega, Console, Music). Alternative: split consoles by machine (Game Boy, Super Nintendo, PC Engine, Mega Drive);
more precise, but 13 families do not fit the 32-entry AGA cover palette at 3 tones each.

Q2. Variation inside a family. Recommended: three tones of one colour picked by the title, same mark and word.
Alternative: one fixed cover per family; a shelf of one family then looks like one repeated tile.

Q3. Playlist covers. Recommended: majority family of the playlist's songs, like an album. Alternative: a 2x2 mosaic
of the first four songs' covers (Spotify style); richer, but a new draw path and four lookups per header.

Q4. Unlisted UADE prefixes. Recommended: list the prefixes Modland and the fixtures use, rest falls to Music.
Alternative: the engine exposes the owning replayer of a song so every nuade-claimed file is Amiga; exact, but a new
engine field and a registry query per row.

## Found

- The library grid (desktop_ui.c:7018) and cover flow (desktop_ui.c:7114) never draw real art: they call genart
  unconditionally and ignore `row->art_id`. Collection shelves (4631) do the same with `c->id`. "Real art always
  wins" is false there today; I691d fixes it.
- The MUI canvas grid and art path have no live caller (mui_canvas_set_grid, mui_canvas_draw_art: tests only).
- `sel_format_by_ext` / `sel_format_by_name` (formats_table.c) have no caller outside their file.

## 7. Johan's decisions (2026-10-29)

- Q1 families: split consoles. Nintendo and Sega are not single families; each console that has its own formats in the extension table gets its own family (for example NES, Game Boy, SNES, Nintendo 64, Master System, Mega Drive, Game Gear, plus the other consoles the table covers). "Console" stays only for consoles with too few formats to stand alone. I691a lists the final set with the extensions per console.
- Q2: 3 tones per family, picked by a title hash.
- Q3: playlist cover uses the majority family.
- Q4: explicit UADE prefix list on top of the claims.h prefix rule.
- Consequence for AGA: more families times 3 tones can exceed the 32 palette slots at 32-63. Default (implementation choice, stated to Johan): on an AGA tier that runs out of slots, families keep their base tone and drop the two extra tones, base tones first by table order. I691a measures the slot count and adds a test that asserts the AGA palette fits.
