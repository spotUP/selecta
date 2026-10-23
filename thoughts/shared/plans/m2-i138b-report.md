# I138b: AmigaDOS browse join and parent

Invariant: browse path join/parent/root/basename must follow the path style (Amiga: Volume:dir/sub), owned by src/library/local.[ch].

Fix (root level: the library helper, not the UI):
- `sel_browse_join_style`, `sel_browse_basename_style`, `sel_browse_is_root_style` (+ platform wrappers `sel_browse_join`, `sel_browse_basename`, `sel_browse_is_root`, selected by `SEL_PATH_STYLE_AMIGA`, same mechanism as `sel_path_is_absolute_style`).
- `fill` (the ".." row) and `selecta-cli library` entry count use `sel_browse_is_root` (a bare "Volume:" is a root, no ".." row).
- Desktop "/" branch of the join is untouched.

Tests:
- `library-path-unit` (tests/library_path_test.c): both styles, join/parent/root/basename. Fails before (new API absent).
- `amiga-browse-nav-vamos` (tests/m2/browse_nav.cmake): Amiga Selecta under vamos browses work: -> songs -> sub -> back to songs -> work:, asserts screen-dump title and rows. Verified red with the Amiga style forced off, green with the fix.
- Green: selecta-browse-play, amiga-text-ui-vamos, amiga-library-browse-vamos. `commands --unimplemented` = 21.

Found: the text UI title leaf (`tui_leaf` in plugins/ui/text/text_ui.c) splits on "/" only, so it shows "work:songs" at volume level. Cosmetic; a plugin cannot include src/library, so it needs an SDK-visible leaf helper (new row).
