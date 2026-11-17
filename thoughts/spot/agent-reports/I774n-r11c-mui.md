# I774n R11c: MUI song info gaps (A3, A9, A10)

Branch worktree-agent-a14a531a3952c2f32, code commit 9bb399c1.

## Done
- A3: five section listviews (Credits, Other Sources, Found In, Same Tune, Demozoo) in plugins/ui/mui/mui_ui.c. Section routing uses the new shared table sel_uim_si_sections / sel_uim_si_section_index (include/selecta/uimodel/siinfo.h, src/uimodel/siinfo.c). Pick ids CTL_SI_PICK_BASE 0x2AF0 (6 ids), per-list index map si_idx back to the buffer row.
- A10 steps 2-3: Format, Channels, Publisher chips (CTL_FCHIP_BASE 0x2AF8, 3 ids). Channels cycles its caption. Logic shared via sel_uim_search_chan_label/term/apply and sel_uim_search_term_start (src/uimodel/layout.c); the Mac chip click in plugins/ui/desktop/desktop_ui.c now uses the same helpers.
- A9: Found In kind "BBS" now "Bulletin Boards" in the shared builder (src/uimodel/siinfo.c) and in src/library/siinfo_lines.c (literal duplicated, library cannot link uimodel).
- Tests: mui_contract_test.c extended (section table, chan apply, kind label, source greps); mui_ctl_ranges.cmake got SPAN entries. Host: mui-songinfo, mui-songinfo-panel, mui-open-url, mui-ctl-ranges, mui-col-tag, mui-doors all pass.

## Deviation
Chips instead of real Cycle/Popup gadgets: test_col_tag forbids MUIC_Cycle and MUIC_Popup in mui_ui.c. Design A5/A12 asked for popups. Decide whether to relax that test or accept chips.

## Goldens
None re-blessed. Grep for the word BBS in tests, src, include, plugins found no golden containing the label.

## Unverified
- m68k MUI target not compiled (host compile of mui_ui.c clean only). May need cmake/amiga_seg_imports.txt entries for the new uimodel symbols.
- desktop_ui.c not compiled (SELECTA_UI_DESKTOP is OFF on host).
- text-ui songinfo tests not run (golden-unchanged claim rests on grep).
- No rig run, no visual check of layout on Amiga.
- mui-open-nomui, mui-song-arg, mui-exec-queue not run (need unbuilt executables).
- build/ dir (424 MB) deleted from worktree.
