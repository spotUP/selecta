---
date: 2026-11-13
topic: Amiga Selecta loads UIs (and later sources) as LoadSeg modules on demand, for ledger row I1378
status: research
---

# Amiga UIs and plugins on demand (I1378)

## What is linked today (measured, build/amiga, 2026-11-13 21:38)
Image build/amiga/apps/selecta/selecta: .text 3,347,552, .data 18,648, .bss 446,728.
Why: sel_make_registry (cmake/selecta_registry.cmake:5-69) links every sel_plugin_<name> on SEL_PLUGINS (:67-69); generated/registry_list.c lists 44 entry points. sel_add_plugin (cmake/selecta_plugin.cmake:68-86) turns a plugin into a LoadSeg module only when it passes AMIGA_MANIFEST. Only 12 replayers do this today (pxtone organya hively bbmp_ptk minimp3 nuade klystron kplayer stb_vorbis playsid sunvox dr_flac).

Per-archive text+data (m68k-amigaos-size -t):

| module | bytes | in image as |
|---|---|---|
| desktop_ui (+ gui_icons_baked, font_baked) | 1,301,492 | full code |
| selecta_uimodel / selecta_gui (pulled in by UIs) | 68,156 / 48,076 | full code |
| mui_ui | 289,076 | full code |
| text_ui (+ txt_locale_utf8) | 44,468 | full code |
| sources (asma aminet hvsc zxart dlna jamendo podcast ...) | ~80,000 total | full code |
| outputs paula paula14 ahi + shadows, wav, null | 1-3.5 KB each | full code |
| viz (krakatoa 25 KB, sidregs 8 KB, others <2 KB), arexx 35 KB | | full code |
| nuade / playsid / kplayer manifest units | 15 KB / <1 KB / <1 KB | manifest only, players in Players/ |

Corrections to the I1374 report: nuade, playsid and kplayer are already on demand. The MUI UI is 289 KB, not 1.4 MB. The "714 KB icons" and "298 KB locale" numbers come from nm -S on a.out, which measures the gap to the next global symbol. The icons are inside desktop_ui's 1.3 MB, and the locale data is inside text_ui's 44 KB.

## Why UIs are not modules yet
The seg ABI (src/engine/seg_crt.c:8-28, header 'SELP', version 1, init/fini/entry; loader src/engine/loader_seg.c:95-136 load_module, :139-176 resolve) gives a module only sel_host. A module "links no selecta_core" (plan 2026-11-10-amiga-loadseg-plugins.md, Host table section and the gap list at :70). The UIs call core directly. Undefined project symbols per archive:
- desktop_ui: 262 (sel_uim 135, sel_gui 63, sel_engine 26, sel_route 8, sel_platform 7, txt_* 17, sel_tag/format/view/plugin)
- text_ui: 105
- mui_ui: 48

UI selection is already prepared for modules. apps/selecta/main.c:380-397 ui_resolve calls sel_plugin_resolve, which on the Amiga loads the module (src/core/plugin_resolve.c:20). UI_SWITCH goes through the same function.

## Design
1. Import table (one mechanism for every module kind). A checked-in list cmake/amiga_seg_imports.txt holds one core symbol per line, append only (the same rule as sel_host).
   - Host side: a generated C file defines `const void* const sel_seg_imports[] = { &sel_engine_state, ... }`. This also forces those symbols into the image.
   - Module side: a generated .s with one m68k thunk per symbol, `_sym: move.l _sel_seg_imp,a0; move.l N*4(a0),a0; jmp (a0)`. The thunk works for any C signature because args stay on the stack.
   - Header: SELP version 2 adds `+24 imports slot`, a writable .long. load_module (loader_seg.c:108) writes the table address there before calling init and accepts version 1 or 2. Table length = list count. A module whose count is larger than the host's is rejected with sel_registry_note_rejected.
   - A module privately links only stateless helpers the host does not define (txt_palette, txt_bindings, txt_cmdline, gui blit code, icon/font data). Anything with state that the host also has (engine, uimodel, if main.c uses it) goes through imports so no state is duplicated. Rule: imports = (nm -u module) intersected with (defined in host). A build check fails if a seg's link leaves anything unresolved.
2. UIs become manifest plus module. Each UI gets a manifest.c (SEL_PLUGIN_MANIFEST: id, kind 5, SEL_PLUGIN_MODULE "sel_<id>_ui", plus every static tag that sel_registry_default_ui (src/engine/registry.c:493) and the UI picker read). sel_add_plugin(... AMIGA_MANIFEST manifest.c). The seg target links the UI's own libraries (selecta_gui and private txt helpers; MUI keeps mui_map and sel_mcc_static).
3. Choice at start: no change. --ui, the machine default and UI_SWITCH all go through ui_resolve. A missing file means the resolver returns NULL and STARTUP_FAIL "ui '<id>' is not registered". Change that message to name Players/sel_<id>_ui.
4. Icons and locale move inside their UI module (desktop: gui_icons_baked.c and font_baked.c; text: locale.c). No separate data files. They load only when that UI loads, and the bake tools (I1315, I463) stay as they are.
5. Outputs (paula, paula14, ahi, about 10 KB) stay linked in as the boot set for now. They are hardware glue that is needed before any song opens. Decision for Johan.
6. Sources (about 80 KB): optional later row. resolve is already called in src/library/source.c:96.

Expected image: 3.35 MB - 1.30 (desktop) - 0.29 (mui) - 0.04 (text) - up to 0.12 (gui/uimodel, whatever only UIs use) = about 1.6-1.9 MB text. Measure it in I1378e.

## Tests
- amiga-image-budget (ctest, Amiga build): m68k-amigaos-size of selecta, text+data <= budget (set from the I1378e measurement plus 5%). Also an nm check that `_sel_plugin_entry_desktop_ui`, `_sel_gui_icons_baked`, `_txt_locale_utf8` and every `_sel_plugin_entry_mui_ui` symbol are absent from the image.
- text-ui-on-demand-vamos: Players/ holds sel_text_ui only. `selecta --ui text` starts and quits under -m 8192. The stats show module_loads == 1 and no desktop or MUI file was opened.
- ui-module-missing-vamos: --ui desktop with no Players/sel_desktop_ui fails with the named file in the message.
- seg-imports-unit: hively built as a version 2 module through thunks renders the same golden hash as version 1. A count mismatch is rejected.
- Rig (rule 4a): desktop AGA and RTG start and MUI start on rig 2 (--ram 256). Timing before and after goes into I427e6.

## Rows (one agent each, in order)
- I1378a (Opus): import table and SELP v2. Files: seg_crt.c, loader_seg.c, selecta_plugin.cmake, list file and generator, seg-imports-unit test. Prove it on one replayer.
- I1378b (Sonnet, after a): text_ui as a seg module, plus text-ui-on-demand-vamos and ui-module-missing-vamos.
- I1378c (Sonnet, after a): desktop_ui as a seg module (icons and font inside), rig check.
- I1378d (Sonnet, after a): mui_ui as a seg module, rig check.
- I1378e (Sonnet, after b-d): amiga-image-budget test, measure and set the budget, update the 6.2 MB figure in RULES.md and the vamos -m note.
- I1378f (optional, after a): source plugins as seg modules.

b, c and d can run in parallel after a. They all touch the shared list file, which is append only, so expect merge conflicts there.

## Open decisions (Johan)
(a) Outputs linked in as the boot set, or modules too? (b) Sources now or later? (c) selecta-cli without text_ui and mui_ui (its registry_list_cli.c lists both)? (d) Import thunks generated from a list, or a hand-written versioned sel_ui_host table (far more code, about 260 entries for desktop)?

## Not checked
Which symbols sel_registry_default_ui reads, whether main.c uses uimodel state (decides import vs private copy), the sqlite share of the remaining image. Risk: thunks assume cdecl stack args; any -mregparm or varargs import needs checking in I1378a.

## Decisions (Johan, 2026-11-13 night)
- (d) Import mechanism: generated thunks from the checked-in list, SELP v2.
- (a) Outputs paula, paula14, ahi stay linked in as the boot set.
- (b) Source plugins: later, optional row I1378f after the UIs land.
- (c) selecta-cli drops text_ui and mui_ui too and loads them through the same modules.
