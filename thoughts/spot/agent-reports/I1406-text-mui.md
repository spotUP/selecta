# I1406 text UI and MUI search fold

Commits: b75c1c2f (WIP code), f1fbbe0b (text UI fold test). Branch worktree-agent-a515d3fdb1bf50667.

Host: mui-list-search-fold, text-ui-keys-wait, new text-ui-search-fold pass. text-ui-library-screen fails on golden casing only
("mk tune" vs "Mk Tune", "selecta test" vs "Selecta Test"); not touched by this diff (pre-existing, golden needs a rebless).

Text UI test (tests/m3/search_fold.cmake): folded 3 rows + "Show all (5)", Enter on Songs heading expands to 5 rows + "Show less".
The left sidebar column still repeats the unfolded list (not part of this change).

Amiga: build/amiga selecta only links after a local fix to plugins/ui/desktop/desktop_ui.c:6975
(`#ifdef SELECTA_DESKTOP_SDL` calls desk_sdl_set_clipboard in the AGA build -> -Werror implicit declaration; from I1243).
Changed to `#if defined(SELECTA_DESKTOP_SDL) && !defined(SELECTA_DESKTOP_AGA)`, NOT committed. Main's Amiga build is broken until that lands.

Rig 2 (Amiberry, 256 MB, no audio), selecta --ui mui --catalogue catalogue_fold.tsv, search "foldy":
- list shows Songs (5), 3 song rows, "Show all (5)", Albums (0), Artists (1), Foldy: PASS
- click Show all: 5 rows + "Show less": PASS; click Show less: back to 3 rows: PASS
- click on row Fold Tune 5 after expand selects that row; click Artists after collapse selects row 6: PASS (pending-highlight suspect not reproduced)
- fold applies only in the plain list style (code: mui_style_kind == MUI_STYLE_LIST); Grid, Cover Flow, Compact not live-tested.
Rig stopped, vtcon/build/rig2 deleted. No screenshots taken.
