
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
