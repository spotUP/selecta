# I1440n3 tag twins (text UI + MUI)

Design: thoughts/shared/designs/2026-11-16-i1440-deepsid.md sections 3.3, 3.4.

- Shared helpers in include/selecta/uimodel/pages.h + src/uimodel/pages.c: sel_uim_tag_name, sel_uim_tag_count, sel_uim_tagchip_label ("Tag" / "Tag: x"; not sel_uim_tag_label, that is the song-tag helper in display.c), sel_uim_tag_next (All Tags, tag 0.. , All Tags), sel_uim_tag_line (COLLECTION_TAG "<tag>").
- Mac chip now uses them (no inline list builder, no literal COLLECTION_TAG).
- Text UI: collection page key t cycles, x clears; header line shows "[Tag: x]"; status "Tag filter: <name>/All Tags".
- MUI: Tag button (id 0x2814) in the collection head, hidden unless the page has tags or an active tag; cycles (MUI 3.8 has no popup). Caption follows the active tag.
- Tests: uim-tag-chip (new), mui-col-tag (new). Passed with mui-ctl-ranges, mui-mac-hook-parity, mui-col-about, text-ui-tiles-back, text-ui-settings, text-ui-menu, text-ui-keys-wait.
- Not run: other text-ui-* m3 tests fail in 0.01 s because sel_sqlite3_shell and other test targets were not built in this worktree (environment, not code).
- Amiga compile of mui_ui.c: not done (worktree has no build/amiga; main's was busy).
- Unverified live: Tag key on text UI and Tag button on MUI never seen on a real composer page with cached DeepSID tags.
