---
date: 2026-11-06
author: spotUP
status: draft
implements: I685a
related:
  - thoughts/shared/design/2026-10-28-mui-custom-classes.md (sections 2 and 3, extended here)
  - RULES.md rule 3 (plugin include roots, plugins/shared/<topic>/)
  - include/selecta/uimodel/tokens.h, include/selecta/uimodel/theme.h
  - plugins/ui/mui/ (current class sources)
---

# MUI custom class package: .mcc libraries, .mcp prefs, theme, docs, demo

Johan, 2026-10-29: "convert all mui components and release as a theme+mcc package so people can make modern mui apps".
Scope he chose: full `.mcc` + theme package. This document extends sections 2 (class list) and 3 (drawing and theme)
of `2026-10-28-mui-custom-classes.md`. It is a design only. Nothing here is built yet.

Target: MUI 3.8 (muimaster.library v19, `MUIMASTER_VLATEST 19` at `ndk-include/libraries/mui.h:136`), no MUI 4/5 API,
68020 baseline with fast RAM.

Notation: "verified" means read from the source or the file system in this session. "unverified" means from memory or
from another document. "OPEN: not examined" means I did not look.

---

## 0. The invariant this package must keep

**One class source, two consumers.** After the package exists, Selecta's MUI UI and the shipped `.mcc` libraries are
built from the same `.c` files. There is no fork, no copy, no "public edition".

Why: `mui_contract_test` and `mui_aa_test` read the class sources by path and test the pure maths in `map/`. If the
package were a copy, every fix would land twice and one copy would drift. The test suite would also stop covering what
users run.

How: section 3 (source tree) and section 8 (build). The class code moves once into `plugins/shared/mcc/`. One CMake
target set builds it two ways from the same object list:

1. a static archive `sel_mcc_static` that `sel_plugin_mui_ui` links (Selecta today: plugins are static on Amiga,
   `cmake/selecta_plugin.cmake:37-42`, verified);
2. eleven `.mcc` shared libraries plus one support library, for the release archive.

Selecta keeps working with the classes linked in. It does not need the installed `.mcc` files. The final gate (section
10) proves both paths run the same code.

---

## 1. Class list

Decision: **ship 11 classes from 10 source files, keep 3 private.**

| Class (current name) | File (plugins/ui/mui/) | Lines | Superclass | Ship? | Reason |
|---|---|---|---|---|---|
| AAButton (push, toggle, check, icon-only roles) | mui_aabtn_class.c | 377 | Area | ship | general button |
| AAText | mui_aatext_class.c | 205 | Area | ship | general label |
| AAPanel | mui_aapanel_class.c | 115 | Group | ship | rounded surface group |
| AATabs | mui_aatabs_class.c | 207 | Group of AAButton | ship | general tab bar |
| AABalance | mui_aabalance_class.c | 141 | MUIC_Balance | ship | themed splitter |
| AASlider | mui_aaslider_class.c:243 | 400 (both) | MUIC_Numeric | ship | themed slider |
| AAProgress | mui_aaslider_class.c:368 | (same file) | Area | ship | progress bar |
| AACycle | mui_aacycle_class.c | 335 | Area, opens AAList popup | ship | themed cycle |
| AAField | mui_aafield_class.c | 279 | Group wrapping stock String | ship | themed text field |
| AAScroller | mui_aascroller_class.c | 223 | Area | ship | themed scroll bar |
| AAList | mui_aalist_class.c | 480 | Area, uses AAScroller | ship | virtual multi-column list |
| Canvas | mui_canvas_class.c | 184 | Area | private | tile grid, needs `amiga/p96_write.h`, `sel_gui_rect`, `mui_canvas_fit` |
| VizArea | mui_vizarea_class.c | 207 | Area | private | needs `sel_viz_frame`, `sel_uim_budget`, mui_vizset |
| Listview | mui_listview_class.c | 84 | Group | private | Selecta Song menu keys to ReturnID |

Line counts verified (3237 lines in the `.c` files; the brief's 3470 probably counts headers, unverified).

The 3 private classes stay in `plugins/ui/mui/` and are built only into Selecta. A later row can generalise Canvas into
a "tile grid" class. That is not in this scope.

---

## 2. Library layout and size

Decision: **one `.mcc` per public class, plus one support library `selecta_aa.library` (name OPEN, section 12 D1)
holding the shared drawing, tooltip, theme and font code.**

Reason: every class uses the same base: `aa_ctx` drawing (`mui_aa_draw.c`, 384 lines), the tooltip half
(`mui_aatip.c`, 213 lines), `map/mui_aa.c`, `map/mui_theme.c`, `map/mui_aatip.c` and the theme parser
(`src/uimodel/theme.c`, `tokens.c`). If each `.mcc` linked that base statically:

- about 20 KB would be duplicated in each of 11 libraries (about 220 KB on disk and in RAM when all are open);
- the font cache (one `TT_OpenFont` per size) would exist once per library;
- the live retheme list (`aa_retheme_all`, the `next`/`linked` chain in `aa_ctx`) would split per library, so a theme
  change would reach only objects of the class that received it.

The third point breaks behaviour, not just size. One support library keeps one font cache and one retheme list.

Measured Amiga object sizes (text+data+bss, bytes, from `build/amiga`, verified):

| Part | Bytes |
|---|---|
| aabtn 4556, aalist 5580, aacycle 3144, aaslider 3368, aafield 2680 | |
| aascroller 1872, aatext 1672, aatabs 1636, aabalance 992, aapanel 992 | |
| aa_draw 3868, aatip 1748, map/mui_aa 6688, map/mui_theme 3576, map/mui_aatip 652, theme.c 3648 | 20176 base |
| map/mui_icons 1508 + mui_icons_baked 20000 | 21508 icons |

Estimates (unverified until row I685b builds one):

- each class `.mcc`: object size plus about 1-2 KB library header, function table and `MCC_Query` glue, so 2-7 KB;
- support library without icons: 22-25 KB;
- the icon set, if shipped (section 6): +21.5 KB in the support library.
- whole package in RAM with every class open: about 65-75 KB, about 87-97 KB with icons. Fine on the fast RAM baseline.

Inter-class opens (verified from the sources): AATabs creates AAButton children, AACycle creates an AAList popup,
AAList creates AAScroller. In the `.mcc` build each of these opens the other class by name through `MUI_NewObject`
(`"AAButton.mcc"` and so on), never by linking its `MUI_CustomClass` pointer. Inside Selecta's static build the same
call resolves to the linked class (section 3, the `AA_CLASS()` switch).

---

## 3. Source tree and the `.mcc` skeleton

### 3.1 Where the source lives

Decision: **`plugins/shared/mcc/`**, laid out as:

```
plugins/shared/mcc/
  include/mcc/aa.h            public header: tag base, every class name, tag, method, value (section 8)
  include/mcc/aa_internal.h   shared to class sources only (aa_ctx, aatip)
  src/aa_draw.c  aa_tip.c     moved from plugins/ui/mui/mui_aa_draw.c, mui_aatip.c
  src/map/                    moved from plugins/ui/mui/map/mui_aa.c, mui_theme.c, mui_aatip.c, mui_icons.c (pure, host tested)
  src/class/aabtn.c ...       the 10 shipped class files, moved from plugins/ui/mui/
  lib/libheader.c             .mcc skeleton (one file, built once per class with -DAA_CLASS_ID=...)
  lib/support_lib.c           support library function table
  prefs/aa_prefs.c            the .mcp (section 5)
  demo/aademo.c               demo program (section 9)
  dist/                       Installer script, README, AutoDocs source, LICENSE copies
  CMakeLists.txt
```

State after row I685c (commit d8061fd4, 2026-11-06), where the tree differs from the layout above:

- The move kept every file name (`src/mui_aa_draw.c`, `src/mui_aatip.c`, `src/class/mui_aabtn_class.c`, ...). The
  short names above (`src/aa_draw.c`, `src/class/aabtn.c`) and `include/mcc/` do not exist yet; the header arrives
  with I685d as `include/mcc/up.h` (decision D1).
- `src/map/` holds 7 files, not 4: `mui_aa.c/.h`, `mui_theme.c/.h`, `mui_icons.c/.h`, `mui_aatip.c`.
- `sel_mcc_static` builds on every target (the map sources on the host, the class sources on the Amiga only), so the
  host tests link the same archive. The spike targets stay Amiga only.
- The package still includes two headers from `plugins/ui/mui`: `mui_gfxbase.h` (12 files) and `map/mui_map.h`
  (`mui_aabtn_class.c`). The test `mcc-include-check` (`cmake/mcc_check_includes.cmake`) allows exactly the 13 lines
  in `plugins/shared/mcc/include_allowlist.txt` and fails on a new one or on a stale one. Row I685e empties the list.
- `sel_mcc_static` and `mui_map` link each other (`mui_icons.c` reads the baked table in `mui_map`, the classes call
  `mui_map.h` functions). Row I685e removes the cycle.
- The include check resolves quoted includes against the package tree and `plugins/ui/mui` only. An include that
  resolves through `plugins/shared` (`amiga/p96_write.h`) is not reported. Row I685e covers it.

State after rows I685e3 and I685d2 (commits d9e9ebdf, 08faeaee, 2026-11-06), which replaces the last three points above:

- The package includes nothing outside itself: `mui_gfxbase.h` is `plugins/shared/mcc/src/mui_gfxbase.h`,
  `mui_aabtn_minmax` and `mui_btn_box_t` live in `src/map/mui_aa.h` / `mui_aa.c`, and `plugins/ui/mui/map/mui_map.h`
  includes the package header. `include_allowlist.txt` holds 0 lines.
- `mcc-include-check` takes its include roots from the real target (`MCC_CHECK_ROOTS`: the `INCLUDE_DIRECTORIES` of
  `sel_mcc_static` plus `plugins/shared`) and reports every quoted include that resolves outside the package.
- Correction: no package file includes `amiga/p96_write.h` (grep, 2026-11-06). The "known case" named above did not
  exist; the Canvas class that needs it is private to Selecta (section 2 table).
- The link cycle `sel_mcc_static` <-> `mui_map` is still there: `mui_icons.c` reads the baked icon table from
  `mui_map`. Row I685e4 removes it.

Reason: RULES.md rule 3 (line 9, verified) says plugins see `include/selecta/` only and shared plugin code lives in
`plugins/shared/<topic>/`. `plugins/shared/` already holds amiga, archive, cast, mdns, paula, raop, source, stream,
upnp and viz (verified). The package is shared plugin code with a second consumer (the release), so it belongs there.
`cmake/sel_check_includes.cmake` must accept `plugins/shared/mcc/include/` for `plugins/ui/mui/`. The exact rule the
check applies to `plugins/shared/` is OPEN: not examined.

The package itself may include `<selecta/uimodel/tokens.h>` and `<selecta/uimodel/theme.h>` (both are under
`include/selecta/`, allowed by rule 3). It must not include anything from `src/` or `plugins/ui/mui/`. The dependency
on `sel_uim_cols_split` (`src/uimodel/cols.c:8`, called from `map/mui_aa.c:780-792`) is solved by linking
`selecta_uimodel` (already a static library, `src/uimodel/CMakeLists.txt:4`), the same way `mui_map` does today
(`plugins/ui/mui/CMakeLists.txt:21`). The release support library links the same archive. No copy.

### 3.2 The static/shared switch

Each class file today ends with `MUI_CreateCustomClass(NULL, ...)` in a `_class()` getter (for example
`mui_aabtn_class.c:298`). Decision: one macro in `aa_internal.h`:

- static build (`AA_STATIC`): `AA_CLASS(x)` returns the linked `MUI_CustomClass *`; constructors call
  `NewObject(mcc->mcc_Class, NULL, ...)`, as now;
- `.mcc` build: the class is created once in the library init with the library base as first argument, and other code
  uses `MUI_NewObject(AAC_Button, ...)` by name.

Only these two places differ. The dispatcher, draw, layout and input code is identical in both builds.

### 3.3 The `.mcc` skeleton

Facts:

- No Amiga shared library is built anywhere in the repo today: no `-nostartfiles`, no `Resident`, no `libinit` use in
  CMake or sources (verified by grep).
- The MUI developer kit (`mccheader.c`, `mccinit.c`) is not on this machine and not in the toolchain (verified: only
  `ndk-include/libraries/mui.h` exists).
- libnix in bebbo amiga-gcc ships library startup objects in `~/opt/amiga/m68k-amigaos/libnix/lib/`: `libinit.o`,
  `libinitr.o`, `libinitb.o`, `init_shared.o`, `devinit.o` (verified present). `sys-include/stabs.h` holds the
  `ADDTABL_*` / `ADDTABL_END()` / `__FuncTable__` / `ADD2INIT` macros for the function table (verified, :26-43).
  Whether `libinit.o` produces a working library with `-mcrt=nix20` is unverified.
- MUI 3.8 finds an external class by opening `<name>.mcc` as a library and calling `MCC_Query(which)` at LVO -30:
  0 returns the `MUI_CustomClass *`, 1 the prefs class (in the `.mcp`), 2 the prefs image. **Unverified (memory).**
- Install directory: `MUI:Libs/mui/`, next to the stock `.mui` classes, **not** `MUI:Libs/mcc/`. Verified from the
  MUI 3.8 tree in `~/Downloads/testbbs/MUI/Libs/mui/`, which holds `Busy.mcc`, `Busy.mcp`, `Listtree.mcc`,
  `Listtree.mcp`, `Crawling.mcc`, `MagicWB.mcc`.

Decision: **write our own skeleton on libnix `libinit.o` and `stabs.h`**, one file `lib/libheader.c`, compiled once per
class with `-DAA_CLASS_ID`. Contents:

1. Library init: open `muimaster.library` v19 (minimum 11 per `MUIMASTER_VMIN`, mui.h:135, but we require 19), the
   support library, `utility`, `graphics`, `intuition`; call `MUI_CreateCustomClass(LibBase, super, NULL, size,
   dispatcher)`; fill the `mcc_*Base` fields that `struct MUI_CustomClass` carries (mui.h:3161, verified).
2. Expunge: `MUI_DeleteCustomClass`, close libraries. Refuse expunge while open count is above 0.
3. Function table: Open, Close, Expunge, Reserved, then `MCC_Query` as the first user LVO (-30).
4. `MCC_Query(0)` returns the class; 1 and 2 return NULL in class libraries and the prefs class and image in the
   `.mcp`.

Dispatcher stubs stay as they are: the asm stub that pushes a1/a2/a0 and calls the C dispatcher
(`mui_aabtn_class.c:254-257`, verified). No A4 small-data model is used today, so the library code needs no
`__saveds`/A4 setup. Global data in a shared library is shared between every opener; the classes keep per-object
state in instance data and the font cache and retheme list belong to the support library, so this is correct.

The `GfxBase` rename (`plugins/ui/mui/mui_gfxbase.h`, `sel_mui_GfxBase`, verified, I719) stays: in the library build
the library init sets it from its own `OpenLibrary`, the same contract the plugin keeps in `mui_open`.

Why own skeleton: the MUI kit is not available here and its licence for redistributing compiled headers is
OPEN: not examined. libnix is already the toolchain C runtime. Risk: section 13 R1. Row I685b is a spike that proves
one empty class loads from `MUI:Libs/mui/` on rig 2 before any class moves.

Spike result (I685b, commit c4f2fcd3, report `thoughts/spot/agent-reports/I685b.md`): the libnix skeleton builds
without a hand-written Resident. Facts the build needed, binding for every later class library:
- `libinit.o` (`${AMIGA_GCC_ROOT}/m68k-amigaos/libnix/lib/libinit.o`) is an EXTERNAL_OBJECT source of the target and
  the FIRST object on the link line; link with `-nostartfiles`.
- `stabs.h` is in `m68k-amigaos/sys-include`; `libinit.h` (libnix include directory) gives the only accepted
  prototypes: `LONG __stdargs __UserLibInit(struct Library*, REG(a4, APTR))` and
  `VOID __stdargs __UserLibCleanup(REG(a4, APTR))`. `__UserLibInit` returns 0 on success.
- The file that uses `ADDTABL_*` needs `-fasm` (`-std=c99` hides `asm`); the macros already end in `;`.
- A function pointer passed as `APTR` is cast `(APTR)(uintptr_t)fn` under `-pedantic`.
- Guard: `if(SEL_AMIGA) add_subdirectory(mcc)` in `plugins/shared/CMakeLists.txt`.
- The local Amiga configure needs `-DSEL_DESKTOP_AGA=OFF` when the host bake tools are not built.
- `Busy.mcc` (MUI 3.8) differs in two points that do not matter to MUI: it is not RTF_AUTOINIT (its init calls
  `MakeLibrary`) and its Reserved vector is NULL. Our skeleton is RTF_AUTOINIT with a real Reserved function.
Open after the spike: MUI 3.8 opening the hyphenated `UP-Spike.mcc` at run time and expunge after close (rig run). Rig result 2026-11-06: MUI 3.8 opens `UP-Spike.mcc` by name from `MUI:Libs/mui/` (program prints "spike: ok class=UP-Spike.mcc", rc 0). Expunge after close is not checked yet.

---

## 4. Decoupling table

Every Selecta coupling in the shipped classes, with the fix. Line numbers verified from the current files.

| File:line | Coupling | Fix |
|---|---|---|
| mui_aabalance_class.c:19, :21 | includes `<selecta/uimodel/theme.h>`, `map/mui_theme.h` | keep: both move into the package (theme.h is public Selecta header, allowed) |
| mui_aabalance_class.c:42, :77 | `mui_aa_balance_rgb/line(d->aa.tok, ...)` | none, moves with map/mui_aa.c |
| mui_aabalance_class.c:110-111 | `mui_aa_balance_rgb(mui_theme_tokens(), ...)`, `mui_theme_bg_spec` | none; `mui_theme_tokens()` becomes a support library call |
| mui_aabtn_class.c:22 | `map/mui_icons.h` | icon becomes `AAA_Button_Icon` tag (section 6) |
| mui_aabtn_class.c:23 | `map/mui_map.h` for `mui_aabtn_minmax` (mui_map.h:127) | move `mui_aabtn_minmax` into package `map/mui_aa.c`; mui_map.h keeps a forward include |
| mui_aabtn_class.c:63 | `mui_aa_metrics_aspect(..., mui_aa_screen_aspect(), ...)`; global `g_screen_aspect` set by Selecta (map/mui_aa.c:192-200) | aspect read per screen in `aa_setup` from the screen's `DrawInfo` (`_dri(obj)->dri_Resolution`), kept in the context; Selecta's setter and the global removed. Rule: section 4.1 |
| mui_aabtn_class.c:68-70, :229 | `SEL_GUI_TOKEN_FOCUS_RING / TEXT_MUTED / SURFACE` | keep token enum (public, tokens.h); public header re-exports names as `AATOK_*` |
| mui_aabtn_class.c:130-142 | `mui_icon_body_wh / body / sizes / heights` (baked Lucide) | section 6 |
| mui_aabtn_class.c, spec `.id` | ReturnID inside class | remove; caller uses `MUIM_Notify` on `MUIA_Pressed`/`MUIA_Selected`; Selecta wrapper adds the notify |
| mui_aacycle_class.c:72, :145 | FOCUS_RING token | keep (as above) |
| mui_aacycle_class.c spec `.id` | ReturnID | notify on `AAA_Cycle_Active` |
| mui_aaslider_class.c:73, :146 | FOCUS_RING token | keep |
| mui_aaslider_class.c spec `.id` | ReturnID | notify on `MUIA_Numeric_Value` |
| mui_aatext_class.c:58, :94 | `SEL_GUI_TOKEN_PANEL` | keep |
| mui_aafield_class.c:125 | `fprintf` "selecta mui: field string pens" | route through one `aa_log()` in the support library, quiet unless `ENV:AA/Debug` set (name per D5) |
| mui_aafield_class.c:246 | `mui_theme_bg_spec(sp->tokens[MUI_AA_FIELD_FACE], ...)`; tokens from spec | tokens default from support library; `AAA_Field_Tokens` tag optional |
| mui_aafield_class.c spec `.id` | ReturnID | notify on `MUIA_String_Acknowledge` |
| mui_aapanel_class.c:52, :93 | `mui_aa_panel_radius(d->aa.tok, ...)`, `mui_aa_panel_rgb(sp->tokens, sp->role)` | none (moves); `role` becomes `AAA_Panel_Role` tag |
| mui_aalist_class.c spec `row`, `cell`, `ctx` | C function pointers + context | `AAA_List_RowHook`, `AAA_List_CellHook` (`struct Hook *`); Selecta wrapper builds the hooks from its functions |
| mui_aalist_class.c, `cols` | uses `sel_uim_cols_split` (src/uimodel/cols.c:8 via map/mui_aa.c:780-792) | link `selecta_uimodel` (section 3.1) |
| mui_aa_draw.c:18-19 | includes theme.h, map/mui_theme.h | moves |
| mui_aa_draw.c:21-27, :34-56 | hard-coded TTE tag values and LP inline calls into TTEngineBase | stays, inside support library only (section 7) |
| mui_aa_draw.c:65 | font search `RAM:Inter-Regular.ttf`, `Selecta:Fonts/Inter-Regular.ttf`, `FONTS:Inter-Regular.ttf` | search list from theme key `font.file` (new), default `FONTS:Inter-Regular.ttf`; Selecta sets its own path through the same API |
| mui_aa_draw.c:80, :96 | `OpenLibrary("ttengine.library", 6)`, log "selecta: mui: ..." | keep open; log through `aa_log()` without "selecta" |
| mui_aa_draw.c:112, :160, :164, :303, :377 | `mui_theme_pen_precisions`, `memcpy(c->tok, mui_theme_tokens(), ...)`, `mui_aa_font_px(scr->Height, scr->Width)`, `mui_theme_text_uses_token` | none, all move into support library |
| mui_aatip.h | `AATIP_M_FIRE (TAG_USER\|0x5E1A0731)`, `AATIP_A_Side (...0732)` | rebase on the package tag base (section 8) |
| map/mui_aa.h, mui_aa.c | include `<selecta/uimodel/tokens.h>`, `<selecta/uimodel/cols.h>` | allowed (public headers) |
| all classes | `_made()` sentinels, `_class_free()` | keep in static build (tests use them); `.mcc` build counts in library open count |

Couplings in the 3 private classes (not decoupled, they stay in Selecta): canvas :82, :133 `sel_gui_rect`; vizarea
:163 `mui_vizarea_step(... sel_viz_frame*, sel_uim_budget* ...)`.

Selecta-side glue that stays in `plugins/ui/mui/`: theme file path `MUI_THEME_ENV_PATH "ENV:Selecta/theme"`
(mui_ui.c:277), `mui_theme_tokens_set` (mui_ui.c:713), `aa_retheme_all` call (mui_ui.c:754). These become calls into
the package API; the logic stays one copy in the package.

### 4.1 Screen aspect rule (row I685e1, examined 2026-11-06)

Facts read from the sources:

- Today one global holds the aspect: `g_screen_aspect` (map/mui_aa.c:192). Selecta sets it once in `mui_screen_open`
  (plugins/ui/mui/mui_ui.c:646-651): `GetDisplayInfoData(DTAG_DISP, plan.mode)`, then
  `mui_pixel_aspect_q8(di.Resolution.x, di.Resolution.y)`, then `mui_aa_set_screen_aspect`.
- Five package map functions read the global without an argument: `mui_aa_slider_metrics` (:339),
  `mui_aa_cycle_metrics` (:464), `mui_aa_field_metrics` (:547), `mui_aa_scroller_metrics` (:601),
  `mui_aa_list_metrics` (:661). `model_metrics` in map/mui_icons.c:84 reads it through `mui_aa_screen_aspect()`, as do
  `mui_aabtn_class.c:63` and one line in `mui_aapanel_class.c`.
- `aa_setup` (mui_aa_draw.c:150) already takes everything else per screen from `_screen(obj)`: colour map, depth,
  font size from the screen height and width.
- `struct DrawInfo` carries `dri_Resolution` with the NDK comment "from DisplayInfo database for initial display mode"
  (intuition/screens.h:76-79). MUI gives it per object as `_dri(obj)`, valid from `MUIM_Setup` to `MUIM_Cleanup`,
  the same span as the `aa_ctx`.

Why the global must go: a `.mcc` and the support library are shared by every program that opens them. Two programs
on screens of different pixel shape (a 640x256 Workbench and an RTG screen) would overwrite each other's value.

Rule:

1. `aa_setup` sets `c->aspect_q8 = mui_pixel_aspect_q8(_dri(obj)->dri_Resolution.X, _dri(obj)->dri_Resolution.Y)`.
   `mui_pixel_aspect_q8` is unchanged: 0 or negative input gives 256, the result is clamped to 256..512. A NULL
   `_dri(obj)` gives 256.
2. No display database call in `aa_setup`. `aa_setup` runs once per object in `MUIM_Setup` (several hundred objects
   in Selecta); a field read costs nothing, `GetDisplayInfoData` per object would add to the start-up time row I625a
   measures. No cache keyed on a screen pointer either (a closed screen's address can be reused).
3. The five metrics functions and `model_metrics` take the aspect as an argument (`int aspect_q8` after `font_px`).
   Classes pass `c->aspect_q8` (or `d->aa.aspect_q8`). `g_screen_aspect`, `mui_aa_set_screen_aspect` and
   `mui_aa_screen_aspect` are deleted, not kept as a fallback.
4. Selecta keeps its own `g_pixel_aspect_q8` (mui_ui.c) for its own layout decisions (screen plan, viz, band) and
   passes it where its layout code calls a package metrics function. It no longer sets anything in the package.
5. Same number as today for Selecta by construction on its own screen: the screen is opened with `plan.mode`, and
   Intuition fills `dri_Resolution` from the display database entry of that mode, the entry `mui_screen_open` reads
   today. Unverified on hardware: that Picasso96 (uaegfx) fills `dri_Resolution` with the same values as
   `DisplayInfo.Resolution`. The rig check below decides it.

Checks for the row:

- Host: `mui_pixel_aspect_q8` cases stay; a new case proves each of the five metrics functions returns the tall-pixel
  values for 512 and the square values for 256 in ONE process (two calls, two answers: the proof the global is gone).
  A source check fails when `g_screen_aspect` or `mui_aa_set_screen_aspect` is written anywhere under the package.
- Rig (PAL 640x256, PAL laced 640x512, RTG): the debug line prints both numbers once per screen open,
  `dri_Resolution` and `DisplayInfo.Resolution` of `GetVPModeID(&scr->ViewPort)`; PASS when they give the same
  `aspect_q8` on all three screens and the layout matches the screenshots from before the change. A mismatch on any
  screen stops the row and is reported with both numbers; it is not patched with a fallback.

---

## 5. Theme

Decision: **the theme is the existing token set and file format, unchanged, owned by the support library.**

- Format: `gui.<name> = 0xRRGGBB` lines plus `@gui.base.<name>` references, parsed by `txt_theme_parse`
  (`include/selecta/uimodel/theme.h`, the one parser, verified). The package does not get a second parser.
- Tokens: the X-macro in `tokens.h` (148 lines, `X(id, "gui.key", default, lo, hi)`, enum ends at
  `SEL_GUI_TOKEN_COUNT`, :125, verified). Public header exports the general tokens (ACCENT 3B82F6, SURFACE 1E1E24, TEXT
  C8C8D0, BACKGROUND 121214, SURFACE_RAISED/HOVER/PRESSED, TEXT_MUTED/DISABLED, ACCENT_TEXT, DIVIDER, FOCUS_RING,
  ERROR, WARNING, SUCCESS, SCRIM, RADIUS/SMALL/LARGE, SPACING, FRAME, PANEL, HEADER_TINT, TRACK, HOVER_LIGHT,
  TILE_PAD). The `COVER_*` tokens stay in the enum (one enum) but are documented as Selecta-only and ignored by the
  shipped classes.
- Storage: per application first, global second: `ENV:AA/<application base>.theme`, then `ENV:AA/theme`, then
  built-in defaults. `<application base>` from `MUIA_Application_Base` (mui.h:1331, verified). Selecta keeps
  `ENV:Selecta/theme` by passing that path to the same load call (`mui_theme_load(txt_theme*, env_path, arg_path)`
  already takes a path). Directory name OPEN for Johan (D5).
- Live change: the support library keeps the retheme list. `AA_ThemeReload()` re-reads the file and calls
  `aa_retheme_all`. The `.mcp` (below) writes the file and calls it, so open windows update without restart.
- Theme files shipped: `Dark.theme` (current defaults) and one light theme. The light theme values are OPEN: not
  examined (no light token set exists in the repo that I checked).

The `.mcp` prefs class: decision **one `AA.mcp` for the whole package**, not one per class, because the settings are
the shared tokens, not per-class options. It subclasses `MUIC_Mccprefs` (`"Mccprefs.mui"`, mui.h:2352, verified) and
implements `MUIM_Settingsgroup_ConfigToGadgets` / `GadgetsToConfig` (mui.h:2404-2405, verified, V11). Gadgets: theme
file chooser, accent colour, surface colour, text colour, corner radius, font file, font size scale, "Reset to
defaults". Whether MUI 3.8 shows one `.mcp` for several `.mcc` in the MUI prefs class list, or only a `.mcp` named
after one `.mcc`, is unverified; fallback is `AAButton.mcp` carrying the shared page. The prefs write the theme file
rather than the MUI dataspace (`MUIM_Dataspace_*`, mui.h:2813-2817), because Selecta and the theme file format already
use the file and one storage is the single source of truth.

---

## 6. Icons and the Lucide licence

Facts: AAButton draws baked Lucide icons (`mui_icon_body_wh` etc., mui_aabtn_class.c:130-142) from
`mui_icons_baked.c` (20000 bytes object, 59.6 KB source), produced at build time by `tools/gui_iconbake.c` and
`src/gui/widgets/gui_icon_pack.c` (plugins/ui/mui/CMakeLists.txt:4-18, verified). Lucide licence:
`third_party/lucide/LICENSE` (ISC, plus `icons/` and `VERSION`, verified present; ISC per the file name and earlier
doc, text not re-read this session).

Decision (recommended, D4): **ship the baked icon set in the support library, addressed by name.**

- Tag `AAA_Button_Icon` takes a `STRPTR` icon name (`"play"`, `"pause"`, ...) resolved in the support library; tag
  `AAA_Button_IconData` takes caller-supplied alpha mask + size for icons not in the set.
- Selecta passes names too, through the same tag. The `sel_gui_icon` enum maps to names in the Selecta wrapper only.
- The release archive carries `third_party/lucide/LICENSE` as `Docs/LICENSE.Lucide` and the README names it. ISC
  requires the copyright notice and permission notice in copies; the baked data is a copy.

Reason: modern-looking apps need icons; making every user bake masks defeats the package's purpose. 21.5 KB is
acceptable on the baseline (section 2).

---

## 7. ttengine

Facts: ttengine is used only in `mui_aa_draw.c` (open at :80 with v6, inline LVOs at :34-56, verified). Comments name
it in mui_ui.c:532, mui_aabtn_class.h:37, map/mui_aa.h:34, 70, 107 (verified). There is a PLAIN backend fallback
(`aa_ctx.backend`, log at :96 "ttengine or Inter missing, plain text", verified). The 2026-10-28 doc line 71 says
ttengine is freeware without a redistribution grant: unverified by me.

Decision: **ttengine is an optional, user-installed dependency. The package does not ship it. Inter is shipped.**

- The support library opens ttengine at first `aa_setup`; missing means the PLAIN backend (system font, no
  anti-aliasing), as today.
- Inter (SIL Open Font License 1.1, unverified: the font licence file in the repo is OPEN: not examined) ships as
  `Fonts/Inter-Regular.ttf` with its licence file; the Installer copies it to `FONTS:`.
- README and Installer say where to get ttengine (Aminet path OPEN: not examined).

Reason: shipping a binary without a grant is a licence risk; the fallback already works.

---

## 8. Public API

### 8.1 Header

One public header `include/mcc/aa.h` (installs as `Include/mcc/aa.h` in the archive, plus per-class headers that
include it, for example `mcc/AAButton_mcc.h`, to match the MUI convention of one header per class). Contents: class
name strings, tag base, every attribute, method and value, the token IDs, the support library prototypes.

### 8.2 Tag base, interim and swappable in one place

```c
/* include/mcc/aa.h: the only place the base is written. Interim value until a registered MUI serial exists. */
#define AA_TAGBASE   (TAG_USER | 0x53410000UL)
#define AA_TAG(cls, n) (AA_TAGBASE | ((ULONG)(cls) << 8) | (ULONG)(n))
```

Every tag is `AA_TAG(class, offset)`. Today's values already follow this pattern (`AAT_Contents` 0x53410001,
`AAP_Value` 0x53410101, `AATab_Active` 0x53410301, `AAS_*` 0x534104xx, `AAL_*` 0x534105xx, `AAC_*` 0x534102xx,
verified), so converting them keeps every current number. The two outliers, `AATIP_M_FIRE` 0x5E1A0731 and
`AATIP_A_Side` 0x5E1A0732 (mui_aatip.h, verified), move to `AA_TAG(0x0F, 0x31/0x32)`. A registered serial later is a
one-line change to `AA_TAGBASE` plus a major version bump.

Class index byte: 00 Text, 01 Progress/Slider value, 02 Cycle, 03 Tabs, 04 Scroller, 05 List, 06 Button, 07 Field,
08 Panel, 09 Balance, 0F tooltip and shared. 06-09 are new (AAButton has no tags today, verified).

State after row I685d (commits 6a4e64cd, 2bfffdb5, 2026-11-06):

- The header is `plugins/shared/mcc/include/mcc/up.h` with `UP_TAGBASE`, `UP_TAG(cls, n)` and `UP_CLS_*` (read every
  `aa.h` / `AA_TAGBASE` / `AA_TAG` in this section as those names, decision D1).
- 19 tags and methods exist today and all are `UP_TAG(class, offset)`. Three numbers changed: the two tooltip tags
  (class 0F, offsets 0x31 and 0x32) and `AAF_Redraw`, which had the same number as `AATab_Active` (0x53410301) and is
  now class 07 offset 0x01.
- On the host the header defines `ULONG` and `TAG_USER` itself (guard `__amigaos__` / `AMIGA`). MorphOS, AROS and
  AmigaOS 4 compilers are not covered by that guard; row I685l (distribution) decides whether they are targets.
- `mcc-include-check` is both a build target and a ctest entry (tests/CMakeLists.txt); it is not counted by
  `ctest -R '^mui-'`.
- Token names: `up.h` defines `UPTOK_<id> = SEL_GUI_TOKEN_<id>` for every entry of `SEL_GUI_TOKENS` and `UPTOK_END_`
  (row I685d2, commit 08faeaee; before that commit the header had no token names). Tag names stay `AA*` until row I685f.

### 8.3 From spec structs to tags

Today every class is built from a C spec struct (`mui_aabtn_spec`, `mui_aalist_spec`, ...). Decision: tags become the
only class interface; the spec structs stay as Selecta's wrapper input. Each Selecta constructor
(`mui_aabtn_new(spec)` and so on) becomes about 10 lines that turn the spec into a tag list and add the ReturnID
notify. Mapping:

| Spec field | Tag |
|---|---|
| aabtn caption, widest, help, icon, alt_icon, icon_only, role, selected, weight, fixed_w/h, align | `AAA_Button_Caption`, `_Widest`, `MUIA_ShortHelp`, `_Icon`, `_AltIcon`, `_IconOnly`, `_Role`, `MUIA_Selected`, `MUIA_Weight`, `_FixedWidth`, `_FixedHeight`, `_Align` |
| aacycle entries, active | `AAA_Cycle_Entries`, `AAA_Cycle_Active` (0x53410201-02, kept) |
| aafield tokens, maxlen, max_w, placeholder | `AAA_Field_Tokens`, `MUIA_String_MaxLen`, `_MaxWidth`, `_Placeholder` |
| aalist count, active, row, ctx, max_rows, cell, cols, title, double_pick, no_select | `AAA_List_Count`, `_Active`, `_RowHook`, (hook h_Data), `_MaxRows`, `_CellHook`, `_Columns`, `_Title`, `_DoublePick`, `_NoSelect`; `AAM_List_GetRow` (0x53410510, kept) |
| aapanel tokens, role, fixed_px, fixed_h | `AAA_Panel_Tokens`, `_Role`, `_FixedWidth`, `_FixedHeight` |
| aaslider lo, hi, value, fix_w | `MUIA_Numeric_Min/Max/Value`, `_FixedWidth` |
| aatabs captions, active | `AAA_Tabs_Captions`, `AAA_Tabs_Active` (0x53410301 kept) |
| aatext contents, widest, tone, align, ellipsis, frame, background, fixed, hidden | `AAA_Text_Contents` (0x53410001 kept), `_Widest`, `_Tone`, `_Align`, `_Ellipsis`, `_Frame`, `_Background`, `_Fixed`, `_Hidden` |
| every `id` | removed from class; Selecta wrapper notify |

Tag names in this table use the prefix `AAA_` as a placeholder. The final prefix depends on D1.

### 8.4 Support library API

`AA_ThemeLoad(path)`, `AA_ThemeReload()`, `AA_ThemeSetTokens(const ULONG *rgb, ULONG count)` (what Selecta's
`mui_theme_tokens_set` becomes), `AA_ThemeTokens()`, `AA_IconFind(name)`, `AA_SetFontFile(path)`. Function table via
`stabs.h` ADDTABL. SFD file `dist/sfd/aa_lib.sfd` generates the pragmas/inline/proto headers with `sfdc` (whether
`sfdc` is installed in `~/opt/amiga` is OPEN: not examined; fallback: hand-written LP inlines like
mui_aa_draw.c:34-56).

### 8.5 AutoDocs and versions

- AutoDocs: one `Docs/AA.doc` per class in classic AutoDoc layout, generated from `/****** AAButton.mcc/...` blocks
  in the class sources. Tool for extraction OPEN: not examined (a 40-line host script is enough).
- Versioning: each `.mcc` carries `$VER: AAButton.mcc 1.0 (dd.mm.yyyy)`. Library version 1, revision per release.
  `MUIA_Version`/`MUIA_Revision` on each class answer queries. Callers use `MUI_NewObject` and check
  `MUIA_Version >= n` for newer tags. Minor = new tags only. Major = tag base swap or removed tag.

---

## 9. Distribution

- Archive: `AA-MCC.lha` (name per D1), layout:
  `Libs/mui/*.mcc`, `Libs/mui/AA.mcp`, `Libs/aa_support.library`, `Fonts/Inter-Regular.ttf`, `Themes/*.theme`,
  `Include/mcc/*.h`, `Docs/*.doc`, `Docs/LICENSE`, `Docs/LICENSE.Lucide`, `Docs/LICENSE.Inter`, `Demo/AADemo`,
  `Demo/AADemo.c`, `Install-AA` (Installer script), `README`, `AA-MCC.readme` (Aminet header).
- Installer: copies `.mcc`/`.mcp` to `MUI:Libs/mui/` (section 3.3, verified path), support library to `LIBS:` (or
  `MUI:Libs/`; OPEN: not examined whether MUI adds `MUI:Libs` to the library search path), font to `FONTS:`, themes
  to `ENVARC:AA/`. Warns when ttengine is missing.
- Demo: `AADemo` opens one window with every shipped class: a button row (push, toggle, check, icon-only), tabs, a
  panel, text in all tones, slider, progress, cycle, field, a 1000-row list with columns and scroller, a balance.
  A "Theme" cycle switches between the shipped themes live. Its source is the example in the docs.
- Licence: MIT for the package code (D3), plus the third-party licence files.
- Where published (D2): built by CI from this repo as a release asset on `https://github.com/spotUP/selecta` (remote
  verified), then uploaded to Aminet (`dev/mui`). Aminet upload procedure is OPEN: not examined. Deferred (section 12.1 D2): no Aminet upload until the Selecta UI is final, row I685m.

---

## 10. Verification

Per row (section 11 lists which): host build, `cmake --build build/host --target lint`, the touched `mui-*` tests via
`nice -n 19 ctest --test-dir build/host -R mui-`, and the Amiga build (`build/amiga`, toolchain
`cmake/amiga-68020.cmake`). Rig checks run once per batch at the end (memory: rig test at end), on rig 2 with
`--ram 256`, fs-uae preferred, with a real song where playback matters.

The test-path problem: `tests/ui/mui_contract_test.c` and `tests/ui/mui_aa_test.c` read class sources by path
(references measured: aabtn 8, aatext 6, aalist 6, aapanel 5, aafield 5, mui_aa_draw.c 5, aaslider 4, aacycle 4,
aascroller 3, aabalance 3, aatabs 2: 51 in total). Decision: the move row (I685c) adds one `MCC_DIR` define next to
`MUI_DIR` (tests/CMakeLists.txt:511-515) and changes every path in the same commit. The contract test must pass before
and after with zero assertions removed: count `mui-*` tests registered (145 on 2026-11-06, measured by I685c; the 102 first written here was wrong) from `ctest -N -R mui-`, not grep.

**Final gate (I685k):**

1. `AADemo` built against the release headers only (no Selecta include path), run on rig 2 with the package installed
   by `Install-AA`. It opens every one of the 11 shipped classes by name from `MUI:Libs/mui/`. Proof it is not linked
   in: the demo binary's symbol table has no `aa_*` / class dispatcher symbols (`m68k-amigaos-nm`), and a sentinel
   attribute `AAA_Shared_Origin` returns the `.mcc` library base, non-NULL, for each class. Demo prints
   `[OK] <class> from MUI:Libs/mui` per class and exits 0; any missing class prints `[ERROR]` and exits 20.
2. Theme reload from `AA.mcp` changes the demo's accent colour live (screenshot checked by a sonnet subagent with a crop).
3. ttengine removed: demo still opens all classes in PLAIN backend.
4. `mui_contract_test` passes unchanged, and the Selecta static build and the `.mcc` build compile the same object list:
   a CMake check compares `sel_mcc_static` sources with the union of `.mcc` sources and fails on any difference.
5. Selecta on rig 2 runs its normal MUI rig suite (`rig-mui-*`) with the classes statically linked.

---

## 11. Build rows

One sonnet agent of about 30 tool calls per row. "P" means it can run in parallel with the row named.

| Row | Work | Files | Done test | Depends | Parallel |
|---|---|---|---|---|---|
| I685b | Spike: empty `.mcc` skeleton on libnix `libinit.o` + `stabs.h`, one dummy Area subclass, `MCC_Query` at -30 | `plugins/shared/mcc/lib/libheader.c`, `demo/spike.c`, CMake | rig 2: spike program opens `Spike.mcc` from `MUI:Libs/mui/` by name, window shows, exit 0; `nm` shows no class symbols in the program | none | P with I685c |
| I685c | Move: shipped class files, aa_draw, aatip, map/ into `plugins/shared/mcc/`; `sel_mcc_static`; `MCC_DIR` in tests; include check rule | moved files, both CMakeLists, tests/CMakeLists.txt, mui tests, `cmake/sel_check_includes.cmake` | all 102 `mui-*` tests pass, same count; Amiga build links; no file content change except paths | none | P with I685b |
| I685d | Public header + tag base macro; rebase all tags incl. aatip; token names | `include/mcc/aa.h`, class files | contract test asserts every tag equals `AA_TAG(...)`; values unchanged except aatip | I685c | P with I685e |
| I685e | Decouple: screen aspect from display info, `aa_log`, font search list, `mui_aabtn_minmax` move, `mui_theme_tokens` behind support API | aa_draw.c, map/mui_aa.c, mui_map.h, mui_ui.c | host tests; Selecta rig run later shows same layout | I685c | P with I685d |
| I685f | Spec structs to tags; Selecta wrappers; ReturnID to notify; list hooks | 10 class files, Selecta wrappers in plugins/ui/mui | contract test plus new test: each wrapper builds tag list equal to spec | I685d, I685e | none |
| I685g | Icons by name + caller data tag | aabtn.c, map/mui_icons.c, wrapper | host test: every `sel_gui_icon` maps to a name that resolves | I685f | P with I685h |
| I685h | Support library build (`stabs.h` table, SFD or LP inlines), `AA_Theme*` API, retheme list in library | `lib/support_lib.c`, `dist/sfd/` | Amiga build makes `aa_support.library`; host test of API on the static build | I685b, I685e | P with I685g |
| I685i | Eleven `.mcc` targets from same objects + same-source CMake check | `plugins/shared/mcc/CMakeLists.txt` | Amiga build makes 11 `.mcc`; CMake check fails when a source differs | I685f, I685h | none |
| I685j | `AA.mcp` prefs + theme files + `AA_ThemeReload` | `prefs/aa_prefs.c`, `dist/themes/` | rig 2: MUI prefs shows page, Save changes theme file, open window updates | I685i | P with I685l |
| I685k | Demo + final gate (section 10) | `demo/aademo.c` | section 10 gate items 1-5 | I685i, I685j | none |
| I685l | Distribution: Installer, README, AutoDocs extraction, licence files, lha target, CI release asset | `dist/`, CI workflow | `lha l` lists layout of section 9; Installer run on rig 2 installs to right directories | I685i | P with I685j |

11 rows. First: I685b (skeleton spike) and I685c (move) in parallel; I685b first if only one slot, because it is the
only row that can prove the plan wrong.

Amiga exemption: none; every row builds for Amiga (RULES 4a).

---

## 12. Open decisions for Johan

| ID | Decision | Recommended | Other | Trade-off |
|---|---|---|---|---|
| D1 | Class name prefix | unique brand prefix (for example `Selecta` names: `SelectaButton.mcc`) | keep `AA` (`AAButton.mcc`) | `AA` names can collide with other authors' classes in the flat `MUI:Libs/mui/` namespace; brand prefix is longer |
| D2 | Where published | in-tree `plugins/shared/mcc` + CI release asset + Aminet | separate repo | separate repo breaks the single-source rule unless Selecta vendors it back |
| D3 | Licence | MIT | LGPL | MIT lets closed-source MUI apps use it; LGPL forces changes back |
| D4 | Icons | ship baked Lucide set, by name | caller-supplied data only | +21.5 KB vs every user baking icons |
| D5 | Theme location | `ENV:AA/` (per prefix in D1) | `ENV:MUI/` | own directory avoids touching MUI's settings; one more ENV directory |
| D6 | ttengine | optional user install, PLAIN fallback | ship it | shipping without a grant is a licence risk (grant unverified) |

### 12.1 Decided (Johan, 2026-11-06, through the question tool)

| ID | Answer | Effect on the rows |
|---|---|---|
| D1 | Prefix `UP-`, file names spelled with the hyphen: `UP-Button.mcc`, `UP-List.mcc`, ... (his typed answer "UP-", then "UP-Button.mcc" over `UPButton.mcc`) | Wherever this document writes `AA<Name>.mcc` read `UP-<Name>.mcc`. C names cannot hold a hyphen: the class macro is `MUIC_UPButton "UP-Button.mcc"`, tags and methods use `UPA_` / `UPM_` style names, the public header is `include/mcc/up.h`, the tag base macro is `UP_TAGBASE`. The support library is `up-mcc.library`, the prefs class `UP-MCC.mcp`. I685b proves on the rig that MUI 3.8 opens a class whose name holds a hyphen (rename the spike to `UP-Spike.mcc` for the rig run). |
| D2 | In the Selecta repository (`plugins/shared/mcc`) plus a CI release asset. **No Aminet release yet** (Johan: "no aminet release yet though we do that when the selecta ui is final") | I685l builds the LhA and the release asset only. Aminet upload is its own row, I685m, blocked until Johan says the Selecta UI is final. |
| D3 | MIT | Licence file in the archive; Lucide and Inter licence files beside it. |
| D4 | Ship the baked Lucide set, icons by name | I685g as written. |
| D5 | Own directory `ENV:UP-MCC/` and `ENVARC:UP-MCC/` | Wherever this document writes `ENV:AA/` read `ENV:UP-MCC/`. |
| D6 | ttengine optional, plain bitmap text fallback; the archive does not hold ttengine | README points to the ttengine Aminet archive. |

---

## 13. Risks

| ID | Risk | Evidence | Mitigation |
|---|---|---|---|
| R1 | No Amiga shared library or MUI class library has ever been built in this repo or toolchain | grep: no `-nostartfiles`, `Resident`, `libinit` use; no `mccheader.c` locally (verified) | I685b spike first; libnix `libinit.o` present (verified) |
| R2 | `MCC_Query` LVO and selector values | CONFIRMED 2026-11-06 on `Busy.mcc` (I685b): fifth vector (LVO -30), selector in d0, 0 returns the class pointer, other values 0 | run-time load of our own hyphenated `UP-Spike.mcc` from `MUI:Libs/mui/` proven on rig 2 2026-11-06 (rc 0, report I1134); expunge check still open (I685b rig run) |
| R3 | 51 test source-path references break on move | measured counts, section 10 | one `MCC_DIR` define, same commit, test count compared before and after |
| R4 | Interim tag base `0x5341xxxx` is not a registered serial; collides if another author uses it | no serial (out of scope) | one-line swap in `AA_TAGBASE`, major version bump |
| R5 | ttengine redistribution | 2026-10-28 doc line 71, unverified | not shipped; PLAIN fallback |
| R6 | One `.mcp` for 11 `.mcc` may not show in MUI 3.8 prefs | unverified | fallback `AAButton.mcp` |
| R7 | Shared library globals are shared across openers; any static per-application state breaks with two apps open | `g_screen_aspect` global (map/mui_aa.c:192-200), font cache (verified as globals) | I685e makes aspect per screen; font cache keyed by size, read-only after open, protected by a semaphore in the support library |
| R8 | Selecta's `GfxBase` rename (I719) and libnix auto-open in a library | mui_gfxbase.h (verified) | library init opens graphics itself; spike checks it |

---

## OPEN: not examined

- `cmake/sel_check_includes.cmake` rule details for `plugins/shared/`.
- Screen aspect source details (display info fields).
- Light theme token values.
- Inter font licence file in the repo; ttengine Aminet path.
- Whether `sfdc` exists in `~/opt/amiga`.
- AutoDoc extraction tool.
- Whether MUI adds `MUI:Libs` to the library search path.
- MUI developer kit redistribution terms.
- Aminet upload procedure.
