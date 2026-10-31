---
status: draft
rows: I731
depends: I667a (mui_aa_draw), I730 (dialog attrs, own screen vs Workbench split)
design-doc: thoughts/shared/design/2026-10-28-mui-custom-classes.md (add section 9 "Tooltips" from this plan when built)
---

# I731: flat MUI tooltips (MUI 3.8)

Johan 2026-10-30 (rig 2): "the help hints that pops up when hovering a button looks super bad they need to be modernized and
flat as well". Goal: the MUI UI shows the rich desktop UI tooltip look (flat rounded SURFACE_PRESSED card, TEXT colour, no 3D
frame), drawn with the shared mui_aa_draw (I667a), on MUI 3.8 only.

Checklist (tick in the ledger row I731, report n of 8):

- [ ] T1 map/mui_aatip.c pure plan, place and state step + sentinel (section 2.1)
- [ ] T2 mui_aatip.c / .h Amiga module: tip window, timer, draw (section 2.2)
- [ ] T3 AABtn routes MUIA_ShortHelp and hover through aatip (section 2.3)
- [ ] T4 AASlider routes MUIA_ShortHelp and hover through aatip (section 2.3)
- [ ] T5 AAField routes MUIA_ShortHelp and hover through aatip (section 2.3)
- [ ] T6 mui_ui.c: door tips prefer the right side, retheme and close hide the tip (section 2.4)
- [ ] T7 host test mui-tooltip-flat, red when reverted (section 6)
- [ ] T8 Amiga build clean, rig 2 look by Johan AGA + RTG (section 7)

## 1. Chosen path and why

**Path: Selecta's own borderless popup (a plain Intuition window drawn by mui_aa_draw), timed by a MUI timer input handler;
MUI never receives the help text, so MUI's own bubble never appears.** Not a MUI bubble hook.

Evidence:

- The MUI 3.8 header in the toolchain (`~/opt/amiga/m68k-amigaos/ndk-include/libraries/mui.h`, `MUIMASTER_VLATEST 19` at :137)
  exposes only these bubble entry points, all on Area: `MUIM_CreateBubble` (V18, :1533), `MUIM_CreateShortHelp` (V11, :1534),
  `MUIM_DeleteBubble` (V18, :1535), `MUIM_DeleteShortHelp` (V11, :1536); messages at :1553-1556. None is marked
  `/* Custom Class */` (compare `MUIM_Draw` :1542, `MUIM_HandleEvent` :1544), so the header does not promise that MUI's
  internal bubble timer dispatches through them on a subclass.
- `MUIM_CreateBubble` takes only `x, y, txt, flags` and returns an opaque `APTR bubble`: the bubble window is MUI-private and
  drawn with MUI's own frame and background. Even a working override would have to return our own window, i.e. the own-popup
  path anyway, but with timing that depends on undocumented dispatch. 3.8 has no bubble look attribute (no `MUIA_*Bubble*` in
  the header). No autodocs are in the tree or the toolchain to settle the dispatch question, and the rig is the only place it
  could be probed.
- The own-popup pattern is already proven on rig 2: the AACycle popup (`plugins/ui/mui/mui_aacycle_class.c:157-195`) opens a
  borderless window on `_screen(obj)` (`MUIA_Window_Screen` fix at :178-179). A tooltip must not take focus, needs no layout
  and no root object, so it is a plain `OpenWindowTags` window (`WA_Activate FALSE`), lighter than a MUI Window object.
- `aa_text`, `aa_round_fill`, `aa_round_corners` (`mui_aa_draw.h:51-63`) draw into any RastPort; `aa_text` only reads the
  owner object for `_font`/`_dri` (`mui_aa_draw.c:279-330`). The hovered control's own `aa_ctx` (pens held on the same
  screen, `aa_setup` :149-177) therefore draws the tip into the tip window's RastPort with no new pen or font handling.
- MUI 3.8 has a timer input handler: `struct MUI_InputHandlerNode` with `ihn_Millis` (UWORD) and `MUIIHNF_TIMER`
  (`mui.h:508-533`), added by `MUIM_Application_AddInputHandler` (:1309). One node gives the 600 ms delay of the desktop
  (`plugins/ui/desktop/desktop_ui.c:9420`, `TIP_DELAY_MS = 600`) without INTUITICKS on every control.
- Every control that gets a help text today is a Selecta AA class: `mui_set_tip` (`mui_ui.c:821-828`) is called on AABtn
  (doors :888, transport :959/:1972, like :971, palette :1105, Now Playing :1109/:2695), AASlider (seek, volume :972-973) and
  AAField (search, command :1084/:1128/:1131); `mui_aabtn_class.c:269` and :305 set it on every AABtn; the extras row
  (`mui_ui.c:936`) sets it on AABtn too. So capturing `MUIA_ShortHelp` in those three classes removes MUI's bubble everywhere
  with no caller changes.

Suppressing MUI's bubble: each of the three classes takes the `MUIA_ShortHelp` tag out of `OM_NEW`/`OM_SET` before the super
call (tag set to `TAG_IGNORE` around `DoSuperMethodA`, restored after) and keeps the pointer itself; `OM_GET MUIA_ShortHelp`
answers from the class. MUI never holds a help text, so it never opens a bubble. This depends on nothing undocumented
(rejected alternative: override `MUIM_CreateShortHelp` to return NULL; works only if MUI dispatches it on the subclass).

Cost of the choice: MUI's prefs bubble delay and "no bubbles" switch no longer apply to Selecta's tips; the delay is the
desktop's 600 ms. Accepted (desktop parity is the bar, memory "MUI matches rich UI").

## 2. Class and function changes per file

### 2.1 `plugins/ui/mui/map/mui_aatip.c` + declarations in `map/mui_aa.h` (new, pure C, host built) [T1]

Add to the `mui_map` source list in `plugins/ui/mui/CMakeLists.txt:21`.

```c
enum { MUI_AA_TIP_ABOVE, MUI_AA_TIP_BELOW, MUI_AA_TIP_RIGHT, MUI_AA_TIP_LEFT };
typedef struct mui_aa_tip_t {
    int w, h, pad_x, pad_y, radius, gap;
    uint32_t face, text, corner; /* 0xRRGGBB */
} mui_aa_tip_t;
/* Box size and colours: face = SURFACE_PRESSED, text = TEXT, corner = SURFACE (the panel colour the wedges outside the
 * rounded box are painted with), radius = RADIUS_SMALL clamped to h / 2, pad_x = SPACING, pad_y = SPACING / 2 (tier S:
 * font_px <= 0 -> pad_x 6, pad_y 3, radius 4), gap 4 (tier S 2). w = text_w + 2 * pad_x, h = text_h + 2 * pad_y. */
void mui_aa_tip_plan(const uint32_t* token, int font_px, int text_w, int text_h, mui_aa_tip_t* out);
/* Screen position of a w x h box next to the anchor rect (screen coordinates), side pref first; ABOVE that does not fit flips
 * BELOW and back, RIGHT flips LEFT and back; the box is then clamped inside 0..scr_w-w, 0..scr_h-h. Returns the side used. */
int mui_aa_tip_place(int ax, int ay, int aw, int ah, int w, int h, int scr_w, int scr_h, int gap, int pref, int* x, int* y);
/* The hover state machine (section 4). */
enum { MUI_AA_TIP_IDLE, MUI_AA_TIP_PENDING, MUI_AA_TIP_SHOWN, MUI_AA_TIP_MUTED };
enum { MUI_AA_TIP_EV_ENTER, MUI_AA_TIP_EV_LEAVE, MUI_AA_TIP_EV_TIMER, MUI_AA_TIP_EV_PRESS, MUI_AA_TIP_EV_KEY,
       MUI_AA_TIP_EV_INACTIVE, MUI_AA_TIP_EV_GONE };
enum { MUI_AA_TIP_DO_NONE = 0, MUI_AA_TIP_DO_ARM = 1, MUI_AA_TIP_DO_DISARM = 2, MUI_AA_TIP_DO_SHOW = 4, MUI_AA_TIP_DO_HIDE = 8 };
int mui_aa_tip_step(int state, int ev, int* actions); /* returns the next state, ORs actions */
#define MUI_AA_TIP_DELAY_MS 600 /* desktop TIP_DELAY_MS */
uint32_t mui_aa_tip_calls(void); /* sentinel: mui_aa_tip_plan calls */
```

Put the colour choice in `mui_aa_tip_plan` only; the Amiga module never names a token itself.

### 2.2 `plugins/ui/mui/mui_aatip.c` + `mui_aatip.h` (new, Amiga only) [T2]

Add to the Amiga `target_sources` in `plugins/ui/mui/CMakeLists.txt:33`. One process-wide tip (only one is ever visible):

```c
typedef struct aatip_slot { const char* text; int side; } aatip_slot;  /* lives in each class's instance data */
#define AATIP_M_FIRE  (TAG_USER | 0x5E1A0731) /* private method the timer node sends to the owner */
#define AATIP_A_Side  (TAG_USER | 0x5E1A0732) /* I/S: preferred side, MUI_AA_TIP_* */
/* OM_NEW / OM_SET: take MUIA_ShortHelp and AATIP_A_Side out of the tag list into t, call the super, restore the tags. */
ULONG aatip_super_set(struct IClass* cl, Object* obj, struct opSet* msg, aatip_slot* t);
/* OM_GET MUIA_ShortHelp -> t->text. Returns 1 when answered. */
int aatip_get(struct opGet* msg, const aatip_slot* t);
/* Call first in the owner's MUIM_HandleEvent; never eats. in = pointer inside the owner. */
void aatip_event(Object* obj, aa_ctx* aa, const aatip_slot* t, const struct IntuiMessage* im, int in);
/* AATIP_M_FIRE: the timer ran out on obj. */
ULONG aatip_fire(Object* obj, aa_ctx* aa, const aatip_slot* t);
/* MUIM_Hide / MUIM_Cleanup of the owner, retheme, window close: drop the tip if obj owns it (obj NULL = any owner). */
void aatip_drop(Object* obj);
uint32_t aatip_shows(void); /* tips opened, for the rig log line */
```

Internals (one static `g_tip`: owner Object*, state, `struct MUI_InputHandlerNode ihn`, `struct Window* win`, app Object*):

- `aatip_event` maps the IntuiMessage to an event (MOUSEMOVE with `in` changed -> ENTER/LEAVE; MOUSEBUTTONS down -> PRESS;
  RAWKEY -> KEY; INACTIVEWINDOW -> INACTIVE), runs `mui_aa_tip_step`, executes the actions. ENTER on another owner first
  drops the current tip. An owner with `t->text == NULL` or empty never arms.
- ARM: `ihn.ihn_Object = obj; ihn.ihn_Flags = MUIIHNF_TIMER; ihn.ihn_Millis = MUI_AA_TIP_DELAY_MS; ihn.ihn_Method =
  AATIP_M_FIRE;` then `DoMethod(app, MUIM_Application_AddInputHandler, &ihn)` (app from `MUIA_ApplicationObject` of `_win`).
  DISARM: `MUIM_Application_RemInputHandler`, guarded by an `armed` flag (MUI timers repeat; fire disarms first).
- SHOW (`aatip_fire`, only when `aa_live(obj)` and `_window(obj)->Flags & WFLG_WINDOWACTIVE`):
  1. `tw = aa_text_width(aa, obj, text)`, `th = aa_text_height(aa, obj)`; `mui_aa_tip_plan(aa->tok, aa->font_px, tw, th, &p)`.
  2. Anchor in screen coordinates: `_window(obj)->LeftEdge + _left(obj)`, `TopEdge + _top(obj)`, `_width`, `_height`;
     `mui_aa_tip_place(..., _screen(obj)->Width, _screen(obj)->Height, p.gap, t->side, &x, &y)`.
  3. `OpenWindowTags(NULL, WA_CustomScreen, _screen(obj), WA_Left, x, WA_Top, y, WA_Width, p.w, WA_Height, p.h,
     WA_Borderless, TRUE, WA_Activate, FALSE, WA_SmartRefresh, TRUE, WA_NoCareRefresh, TRUE, WA_RMBTrap, TRUE,
     WA_IDCMP, 0, TAG_DONE)`. No title (I730 lesson, `mui_map.h:413-416`: a titled borderless window gets a title bar).
  4. Draw into `win->RPort`: `SetAPen(pen(p.corner)); RectFill` the whole box, `aa_round_fill(rp, 0, 0, p.w, p.h, p.radius,
     aa_pen(aa, p.face))`, then `aa_text(aa, obj, rp, p.pad_x, p.pad_y, text, p.text, p.face)` (the contrast gate in
     `aa_text` keeps the PLAIN tier readable). No `aa_round_frame`, no MUI frame.
- HIDE: `CloseWindow(win)`, `win = NULL`. `aatip_drop` = DISARM + HIDE, state IDLE.
- `aa_retheme_all` (`mui_aa_draw.c:370`) calls `aatip_drop(NULL)` first, so a THEME change never leaves a stale-colour tip.

### 2.3 The three tipped classes [T3, T4, T5]

Each gets `aatip_slot tip;` in its instance data and these dispatcher cases (same shape in all three; no per-class copy of
the logic, only the routing):

- `OM_NEW`: `aatip_super_set` replaces the plain `DoSuperMethodA` (on NULL result nothing is kept).
- `OM_SET`: `aatip_super_set` first, then the class's own attribute handling as today.
- `OM_GET`: `if (aatip_get(msg, &d->tip)) return TRUE;` before the super.
- `AATIP_M_FIRE`: `return aatip_fire(obj, &d->aa, &d->tip);`
- `MUIM_Hide` and `MUIM_Cleanup`: `aatip_drop(obj)` before the super.
- `MUIM_HandleEvent`: `aatip_event(obj, &d->aa, &d->tip, msg->imsg, in)` first; `ehn_Events` gains `IDCMP_MOUSEBUTTONS |
  IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW` where missing. The handlers return 0 for events they did not eat before, so the
  added bits change nothing else.

Per class:

- `mui_aabtn_class.c` (T3): ehn at :71 is `IDCMP_MOUSEMOVE` only, add the three bits; the existing hover `in` test feeds
  `aatip_event`. Lines :269 and :305 stay (they now land in `d->tip`).
- `mui_aaslider_class.c` (T4): ehn at :75 has MOUSEMOVE and MOUSEBUTTONS; add RAWKEY and INACTIVEWINDOW. The slider is a
  Numeric subclass (:215); a drag PRESS mutes the tip until LEAVE. Only the slider class, not aaprogress (:340, no tip).
- `mui_aafield_class.c` (T5): Group subclass (:204) with no event handler today. Add `MUIM_Setup`/`MUIM_Cleanup` that add and
  remove an ehn (`MUIA_Window_AddEventHandler`, events MOUSEMOVE | MOUSEBUTTONS | RAWKEY | INACTIVEWINDOW, never eats) and a
  `MUIM_HandleEvent` that computes `in` from `_left.._bottom` of the group and calls `aatip_event`. `mui_set_tip` on
  `s->str_search`/`s->str_cmd` (`mui_ui.c:1084/1128/1131`) targets the AAField object returned by `mui_make_string`; the
  builder must check that, and if it is the inner String, retarget `mui_set_tip` to the AAField (one place, `mui_make_string`).
  The AAField instance also needs an `aa_ctx`; if it has none, it does `aa_setup` in its new `MUIM_Setup` like AABtn (:64).

### 2.4 `plugins/ui/mui/mui_ui.c` [T6]

- Doors (`mui_ui.c:888`): after `mui_set_tip`, `SetAttrs(b, AATIP_A_Side, MUI_AA_TIP_RIGHT, TAG_DONE)` (desktop puts door
  tips right of the rail, `desktop_ui.c:10922-10924`). Every other control keeps the default ABOVE; top bar controls flip
  BELOW by the placement rule, no attribute needed.
- Window close / screen close path: `aatip_drop(NULL)` before the main window or any dialog closes and before `CloseScreen`.
- No change to `mui_set_tip` or the tip texts (`mui_tip` stays the one text source).

## 3. Theme tokens used

All read through the owner's `aa_ctx.tok` (copied from `mui_theme_tokens()`, so live THEME and the AGA pen plan apply):

| Use | Token | Desktop source |
|---|---|---|
| Card face | `SEL_GUI_TOKEN_SURFACE_PRESSED` | `desktop_ui.c:10928-10929`, `gui_widget.c:410-412` (`x->s->pressed`) |
| Text | `SEL_GUI_TOKEN_TEXT` | `desktop_ui.c:10931-10932` |
| Corner radius | `SEL_GUI_TOKEN_RADIUS_SMALL` (clamped to h/2; tier S 4) | `desktop_ui.c:10928` |
| Horizontal padding | `SEL_GUI_TOKEN_SPACING` (tier S 6) | `desktop_ui.c:10908` |
| Corner wedges outside the rounded card | `SEL_GUI_TOKEN_SURFACE` | none (desktop composites; Intuition windows are rectangular) |

No border, no shadow (the desktop shadow is a soft blur the 68k path does not have; flat card only). Font: the owner's base
`aa_ctx` font (`font_px` from `mui_aa_font_px`), the PLAIN tier the screen font.

## 4. Show / hide timing and placement rules

State machine (`mui_aa_tip_step`):

| State | ENTER | LEAVE | TIMER | PRESS / KEY | INACTIVE / GONE |
|---|---|---|---|---|---|
| IDLE | PENDING, ARM | IDLE | IDLE, DISARM | IDLE | IDLE |
| PENDING | PENDING | IDLE, DISARM | SHOWN, DISARM + SHOW | MUTED, DISARM | IDLE, DISARM |
| SHOWN | SHOWN | IDLE, HIDE | SHOWN, DISARM | MUTED, HIDE | IDLE, HIDE |
| MUTED | MUTED | IDLE | MUTED, DISARM | MUTED | IDLE |

- Delay 600 ms from the last ENTER (desktop parity). No auto-hide timeout (desktop has none).
- After a click or key on the control the tip stays away until the pointer leaves and re-enters (MUTED).
- Moving from one tipped control to another: the old owner gets LEAVE (or is dropped by the new ENTER), the new one waits its
  own 600 ms.
- Placement: box centred on the anchor along the cross axis, `gap` px off the anchor edge on the preferred side; ABOVE is the
  default (desktop transport and control tips, `desktop_ui.c:10918/10927`); doors RIGHT, vertically centred (`:10924`). Does
  not fit -> opposite side; then clamped inside the screen. The box never covers the anchor, so the pointer stays on the
  control and MOUSEMOVE keeps going to Selecta's window (the tip window has no IDCMP and is never active).

## 5. Workbench vs own screen

- Own SELECTA screen (full screen default): the tip opens on `_screen(obj)` above the borderless backdrop main window or a
  dialog (I730); clamped to the screen.
- Workbench (compact mode, `g_scr == NULL`): same flat borderless tip, on the Workbench screen, clamped to the screen, so
  it may extend outside Selecta's window (as MUI's bubbles do). Tooltips stay borderless here, unlike I730 dialogs: a tooltip
  with a window frame is never wanted, and `mui_dialog_attrs` is about movable windows. The colours come from the same
  `aa_ctx` pens obtained on the Workbench ColorMap; the PAL/PLAIN fallbacks and the `aa_text` contrast gate handle the
  crowded 4/16-colour Workbench exactly as the controls do.
- No `mui_window_attrs` / `mui_dialog_attrs` call for the tip (it is not a MUI window).

## 6. Host test [T7]

`mui-tooltip-flat`: `add_test(NAME mui-tooltip-flat COMMAND mui_contract_test tooltip-flat)` in `tests/CMakeLists.txt` next
to `mui-dialog-windows` (:495), and added to the `LABELS mui` list in `set_tests_properties` (:601). New case
`tooltip-flat` in `tests/ui/mui_contract_test.c` (built from `tests/CMakeLists.txt:475`, links `mui_map`).

What it drives and asserts:

1. Sentinel: `mui_aa_tip_calls()` before and after; the case calls `mui_aa_tip_plan` with `mui_theme_tokens()` and asserts
   the delta is exactly the number of calls made (proves the case ran the real function, not a stub).
2. Spec numbers from the desktop: `face == token[SEL_GUI_TOKEN_SURFACE_PRESSED]`, `text == token[SEL_GUI_TOKEN_TEXT]`,
   `radius == min(token[RADIUS_SMALL], h/2)`, `pad_x == token[SPACING]`, `w == text_w + 2*pad_x`; tier S values (6, 3, 4).
3. Placement: anchor in the middle -> ABOVE, box centred, bottom edge `gap` above the anchor; anchor at y 0 -> BELOW;
   door pref RIGHT at the left edge -> RIGHT; anchor at the right screen edge -> clamped `x == scr_w - w`.
4. State machine: ENTER -> PENDING + ARM; TIMER -> SHOWN + SHOW; PRESS while SHOWN -> MUTED + HIDE; ENTER in MUTED stays
   MUTED; LEAVE -> IDLE; `MUI_AA_TIP_DELAY_MS == 600`.
5. Wiring source checks (the Amiga module cannot run on the host; same style as the checks at `mui_contract_test.c:212-220`,
   reading files under `MUI_DIR`):
   - `mui_aatip.c` contains `mui_aa_tip_plan(`, `mui_aa_tip_place(`, `mui_aa_tip_step(`, `aa_round_fill(`, `aa_text(`,
     `WA_Borderless`, `WA_Activate, FALSE`, `MUIIHNF_TIMER`, and no `MUIM_CreateBubble`, `MUIA_Frame`, `aa_round_frame(`.
   - each of `mui_aabtn_class.c`, `mui_aaslider_class.c`, `mui_aafield_class.c` contains `aatip_super_set(`,
     `aatip_event(`, `AATIP_M_FIRE`, `aatip_drop(`.
   - `mui_aa_draw.c` `aa_retheme_all` body contains `aatip_drop(NULL)`.
   - no file under `MUI_DIR` passes `MUIA_ShortHelp` to `MUI_NewObject` of a stock class (scan `MUIA_ShortHelp` lines: allowed
     only in `mui_ui.c` `SetAttrs` calls on AA objects, `mui_aabtn_class.c` and `mui_aatip.c`).

Red when reverted: reverting T1 breaks the link/numbers (items 1-4); reverting T2-T5 fails item 5 (the builder runs the
case with each piece reverted once and records the red line in the row). Run: `nice -n 19 ctest --test-dir build/host -R
mui-tooltip-flat --output-on-failure`. Honest limit: the host proves the plan, placement, timing and wiring; the drawn pixels
are proven only on the rig (section 7). Also re-run `mui-dialog-windows` and `mui-aa-state` (touch the same map library).

Rig reach (one rig run at the end, memory "rig test at end"): `aatip_fire` prints once per run to stderr
`selecta mui: tip shown side=<n> w=<w> h=<h> backend=<TC|PAL|PLAIN>` on the first show, so the rig log shows the new path ran.

## 7. Rig 2 look steps for Johan (Amiga, rig 2, AGA and RTG)

Rig 2 starts with `--ram 256`, silent, real song; the agent starts Selecta, Johan only looks.

AGA (rig 2, native AGA screen, Selecta full screen):
1. Rest the pointer on the Play button in the player bar for about a second.
   PASS: a flat rounded dark-grey card with light text ("Play (Space)" or the current tip) appears just above the button,
   no bevel, no MUI bubble frame, no black corners. FAIL as reported: MUI's yellowish 3D bubble.
2. Rest the pointer on a sidebar door: the card appears to the right of the rail, level with the door.
3. Rest on the search field in the top bar: the card appears below the field.
4. Click the button while the tip shows: the tip disappears and does not come back until you move off and on again.
5. Move quickly across several buttons: no tip flashes up until you stop on one.

RTG (rig 2 UAE with uaegfx, same steps 1-5): same look; colours identical to the desktop (truecolour path).

Workbench (compact mode, either tier): step 1 again; the tip is flat and borderless on the Workbench screen.

## 8. Risks

- R1 Corner wedges: an Intuition window is rectangular, so the four corner wedges are painted with SURFACE; over a panel of
  another colour they show as small square corners (radius 4-6 px). Mitigation if Johan sees it: radius 0 for that owner via
  the plan (one place). Shaped windows need OS 3.5+/OS4, out of scope.
- R2 Timer input handler: `ihn_Millis` is UWORD and MUI timers repeat; the module must remove the node on the first fire and
  on every drop, or a stale node calls a disposed object (crash). `aatip_drop` in `MUIM_Cleanup` of each owner is mandatory.
- R3 AAField hover: the String child may eat MOUSEMOVE before the group's handler sees it; if hover never reaches the group,
  give the ehn a higher `ehn_Priority` (never eat). Check on the rig (step 3).
- R4 MUI prefs bubble delay and "no bubbles" no longer apply to Selecta. Accepted for desktop parity; flag if Johan wants a
  prefs.conf switch.
- R5 AGA pen budget: SURFACE_PRESSED and TEXT are already held by AABtn (`mui_aabtn_class.c:64-66`); SURFACE too. No new pens
  expected; the rig log line reports the backend.
- R6 Workbench: a tip outside Selecta's window over another app's window is normal bubble behaviour, but on a 4-colour RTG
  Workbench the PLAIN tier shows a flat card in the nearest pens only; same limit as the controls today.
- R7 Opening and closing an Intuition window per tip costs a layer operation on 68020; one at a time, after a 600 ms rest,
  so no visible cost expected. Measure on the AGA rig if Johan sees lag.
