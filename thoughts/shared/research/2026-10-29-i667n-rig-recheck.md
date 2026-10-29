# I667n rig 2 recheck, build/amiga selecta rebuilt on main 13598ec8 (host fixes 90c3bd19)

Method: rig 2 (purpose "musicplayer-55: I667n MUI flat-look recheck"), SelectaMUI with meme.mod (virgill-that_meme_again.mod) queued, clicks on
volume slider middle, Play, search field; `ami.py shot`, PIL colour counts per uitree box. RTG pass first (rig booted RTG), then `rig.py aga`.
Screenshots (scratchpad): .../7b6e9b73-f7a9-4810-b8de-0f5891b82b93/scratchpad/rtg_full.png (800x600), aga_full.png (640x512), rtg_fields.png.

| Item | AGA | RTG |
|---|---|---|
| 1 Balance box 121214 + 2C2C34 line | PASS: box (169,3,8x376) 121214 2648 px, 2C2C34 360 px | PASS: box (204,3,8x467) 121214 3285 px, 2C2C34 451 px |
| 2 AAField no dark inner rectangle | FAIL: 121214 rectangle remains | FAIL: 121214 rectangle remains |
| 3 Volume 3A3A44 track beside fill | PASS: (545,397,83x16) 3A3A44 168 px, 3B82F6 160, 1E1E24 1000 | PASS: (682,488,106x16) 3A3A44 212 px, 3B82F6 208, 1E1E24 1276 |
| 4 Play glyph E4E4EA (RTG only) | n/a: C8C8D0 68 px by design | FAIL: C8C8D0 68 px; E4E4EA 0 px on whole screen |

Item 2 detail: field face 2A2A32 (RTG face x226..787 y11..44). Dark rectangle RGB 121214 = exactly the String gadget box from uitree
(custom and string gadgets share one box): AGA search 199,20 199x11 (2123 of 2189 px, rest 1E1E24 66 px = cursor), command 420,20 200x11
(2200/2200); RTG search 234,22 262x8 (2032 px, rest EDEDEB cursor 64 px), command 518,22 262x8 (2096/2096). Row 25 AGA: x192-198 2A2A32,
x199-200 1E1E24 (cursor), x201..397 121214, x398.. 2A2A32. So it matches the String gadget's own frame/box (not the AAField face, which is
2A2A32 around it); explicit MUIA_Background from 90c3bd19 did not change the String gadget interior. Not fixed (measure only).
Also: the volume row in item 3 was clicked at mid; Play glyph also 93C5FD 88 px (focus ring) in both modes.


## Root causes (I667n follow-up, host-proven, rig recheck pending)

### 1. AAField dark rectangle (121214 inside the String box)
- Evidence: the dark area matches the inner String gadget box exactly (search 199,20 199x11 AGA; 234,22 262x8 RTG), and 121214 is screen pen 0 (MUI_PEN_BACKGROUND) on the SELECTA screen.
- Cause: MUI's String class is a Gadget subclass. Intuition's string gadget renders its whole box with the StringExtend background pen (Pens[1] / ActivePens[1]), which MUI fills from the MUI prefs and resolves to pen 0. MUIA_Background on the String only paints the area MUI owns around the gadget, so the fix in 90c3bd19 (`set(str, MUIA_Background, ...)`) could not reach those pixels.
- Fix: `aafield_string_pens()` in mui_aafield_class.c gets MUIA_Gadget_Gadget and checks GTYP_STRGADGET and SpecialInfo->Extension. It then writes Pens[1] and ActivePens[1] = aa_pen(mui_aa_field_string_bg(token, focus)) (SURFACE_RAISED 2A2A32) before DoSuperMethodA draws. It logs the old pens once to stderr: `selecta mui: field string pens a/b active c/d -> background x/y`. Read that line on the rig to confirm the old background pen was 0.
- Unverified: whether MUI rewrites the Extension pens on every MUIM_Draw. If the rig still shows 121214, check the stderr line first.

### 2. RTG Play glyph C8C8D0 instead of E4E4EA
- Cause: the Play glyph is drawn by aabtn_icon through aa_pen ramp pens, not through aa_text. aa_pen's obtain() in mui_aa_draw.c called ObtainBestPen with PRECISION_GUI only. On the RTG screen GUI precision accepts the nearest held pen, so E4E4EA mapped to the already held TEXT pen C8C8D0 (0 px of E4E4EA on screen).
- Fix: obtain() walks mui_theme_pen_precisions(): PRECISION_EXACT with OBP_FailIfBad TRUE first, then PRECISION_GUI as the fallback. AGA has a full 16-pen palette in use, so exact fails there and GUI keeps C8C8D0, as designed.

### Tests and revert proof
- mui-i667n-root (tests/ui/mui_aa_test.c test_i667n_root): string_bg = 2A2A32 for both focus states with the call sentinel +2; precision plan is exact then GUI; aafield_draw calls aafield_string_pens(d) before DoSuperMethodA; obtain() uses the plan and has no fixed PRECISION_GUI.
- Revert proof: with the draw call removed, line 629 fails ("field sets the string pens before the super draws the gadget"). With obtain() back to a fixed PRECISION_GUI, line 638 fails ("aa_pen obtains through the precision plan, no fixed GUI precision").


## Recheck after e75ba5f1

Build: build/amiga selecta rebuilt from main e75ba5f1 (ninja rc 0). Host: mui-aabtn-tokens, mui-aacheck, mui-i667n, mui-i667n-root 4/4 passed.
Rig 2 (purpose "musicplayer-55: I667n field and Play glyph recheck"), binary as RAM:SelectaMUI, meme.mod queued, Play clicked, search field clicked; `ami.py shot`, PIL histograms of the uitree boxes.

| Mode | Element | Expected | Measured | Result | Screenshot |
|---|---|---|---|---|---|
| AGA | search String box (199,20 199x11) | 2A2A32 | 121214 2123 px, 1E1E24 66 px (cursor) | FAIL | /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/7b6e9b73-f7a9-4810-b8de-0f5891b82b93/scratchpad/i667n_aga.png |
| AGA | command String box (420,20 200x11) | 2A2A32 | 121214 2200 px | FAIL | same |
| AGA | Play glyph | C8C8D0 (by design) | C8C8D0 68 px, 93C5FD 88 px | PASS | same |
| RTG | search String box (234,22 262x8) | 2A2A32 | 121214 2032 px, EDEDEB 64 px (cursor) | FAIL | /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/7b6e9b73-f7a9-4810-b8de-0f5891b82b93/scratchpad/i667n_rtg.png |
| RTG | command String box (518,22 262x8) | 2A2A32 | 121214 2096 px | FAIL | same |
| RTG | Play glyph | E4E4EA | C8C8D0 68 px, 93C5FD 88 px, E4E4EA 0 px | FAIL | same |

Not measured: Settings window field (not opened, call budget). Stderr pens line NOT captured: Execute/Run drop stderr (`2>` in the script created no file; `ami.py exec` returns no output for the long-running app), so whether aafield_string_pens() ran and what the old pens were is unknown. Next step: make the line reach a file (a Selecta log option, or a stats field) before guessing again; also confirm the new binary was the one deployed (put of build/amiga/apps/selecta/selecta after the e75ba5f1 rebuild, so it was).

## Cause hunt 2

Rig 2 (RTG uaegfx 800x600, and the stored before-fix run), evidence from `Selecta --log RAM:sel.log` plus temporary diagnostics (removed before commit).

### Field dark box (Johan: "The inputs have a darker rectangle in them")

- Log: `field pens entry ... gadget 0x109c2dbc type 0005 ... ext 0x109c2e22`. MUI 3.8 String hands back a GTYP_CUSTOMGADGET (strgclass, type 5), not GTYP_STRGADGET (4).
- `aafield_string_pens()` returned at the `!= GTYP_STRGADGET` check, so the `field string pens ... -> background` line never printed. StringExtend pens stayed 1/0 after the super draw; box pixel was pen 0 (121214). Pen 3 = 2A2A32 was obtained fine.
- Fix: `mui_aa_field_string_gadget()` (map/mui_aa.c) accepts type 4 or 5; the field class uses it.
- After fix the log shows `field string pens 1/3 active 1/3 -> background 3/3`.

### RTG Play glyph

- Log before fix: `screen plan mode 50041303 800x600 depth 4`. The uaegfx (P96) mode does not set DIPF_IS_FOREIGN, so `mui_screen_open` planned a 16-pen planar screen on RTG. All pens were taken: `obtain e4e4ea prec -1 failifbad 1 -> pen -1`, then the icon fell back to pen 1 (C8C8D0).
- Fix: `mui_screen_is_rtg()` (map/mui_screen.c) = DIPF_IS_FOREIGN, or a non-BMF_STANDARD Workbench bitmap, or depth > 8.
- After fix: `screen plan mode 50041303 800x600 depth 8`, `obtain e4e4ea ... -> pen 240 (e4e4ea)`.
- Side effect: `g_screen_rtg` is now true on uaegfx, so the canvas and vizarea RTG paths (I684b) run there too.

### Rig 2 pixels after fix

| Check | Mode | Region | Result |
|---|---|---|---|
| Search box | RTG | 234,22 262x8 | 2A2A32 2096 px (focused 2032 + 64 cursor) PASS |
| Command box | RTG | 518,22 262x8 | 2A2A32 2096 px PASS |
| Play glyph | RTG | 123,481 105x30 | 3B82F6 3032, E4E4EA 46, ABC3EE 28 PASS |
| Search, command box | AGA | 199,20 / 420,20 | not run (budget); next step |

Rig 2 also logs `ttengine or Inter missing, plain text` (AA_PLAIN backend).

## Recheck after 9bbc2327

Rig 2 (purpose "musicplayer-55: I667n AGA boxes and RTG visualizer check"), build/amiga selecta up to date on main 7b120c6d, meme.mod queued, Play clicked. Pixels by PIL on `ami.py shot`.

| Check | Mode | Region | Result |
|---|---|---|---|
| Search box | AGA | 199,20 199x11 | 2A2A32 2189 of 2189 px (was 121214 2123) PASS |
| Command box | AGA | 420,20 200x11 | 2A2A32 2200 of 2200 px PASS |
| Play glyph | AGA | 100,390 83x30 | C8C8D0 68 px, 3B82F6 2302, 93C5FD 88 (focus ring); no E4E4EA, by design PASS |
| Visualizer area | RTG 800x600 | whole screen, 3 shots 1 s apart | NOT PROVEN: only the seek bar row (37,561 to 693,568, 127 to 172 px) changes. The viz area lives in the Detail slot, which the layout hides below 1100 px wide (SEL_UIM_BP_THREE_COLUMN_MIN), so it is not on screen at 800x600 |
| Hang on RTG | RTG | uitree and click response | no hang: Selecta answered a Play click 35+ s after start (5569 px changed), uitree 1.2 s; the seek bar kept moving |

Open: a visible viz on RTG needs a screen of 1100 px or wider (no such uaegfx mode planned) or a layout that shows Detail at 800 px.
