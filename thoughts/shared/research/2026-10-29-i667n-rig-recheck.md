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
