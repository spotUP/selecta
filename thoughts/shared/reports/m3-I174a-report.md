# I174a report: vote count in the playlist row

Design doc: thoughts/shared/plans/2026-10-22-architecture-design.md, "Configurable columns and title formats (F48)" (line 326) and section 9b (view model resolves rows; UIs get ready rows). Plan goal: T80 (2026-10-25-m3-library.md line 62).

## Changes
- Title-format field `votes` (SEL_FMT_VOTES) in src/engine/format.[ch]. Default `view.title_format` is now `[%author% - ]%title%[  Votes %votes%]` (prefs.def, SEL_VIEW_TITLE_DEFAULT). An empty field drops the optional section, so rows without a count are unchanged.
- View (src/engine/view.[ch]): `votes` callback in `sel_view_src`; `row_votes()` memoises the text per row slot and re-reads only when the slot shows another queue entry or `votes_generation` moves. Used by the regular, tree and drilldown row paths.
- Store: the count is the sticker `votes` under the song key (same per-song store as sticker `vote`; no parallel cache). `vote_store()` in src/engine/engine.c writes it in the pump when a finished request (`sel_vote_result.seq` moved, status OK, mkey matches) arrives. Failed or rate-limited requests keep the stored value.
- selecta-cli: new script directive `@playlist` prints `PLAYLIST rows=N` and `ROW <text>` lines of the view state.
- Tests: tests/m3/vote.sel and tests/net/vote_devilbox.cmake drive the T80 script and assert ROW lines: `ambient.mod`, then `ambient.mod  Votes 1` x3, then `ambient.mod  Votes 0`. tests/view/format_test.c covers `%votes%` present and empty.

## Tests (host, nice 19)
15 of 15 pass: ratchet, view-unit, view-tree, title-format, text-ui-keys-wait, playlist-codecs, playlist-verbs, stage-commands, net-fixture-start, rate-devilbox, net-fixture-stop, text-ui-folders-tree, view-library-source, text-ui-library-screen, vote-devilbox. title-format re-run after the format_test edit: pass. Amiga build (build/amiga): success.

## Open items
- Regression-by-revert check not run.
- Server endpoints unverified (design doc line 395).
- "Hippo playlist entry" = a song with a stored `votes` sticker; the default format now shows it for any such song.
- Ledger rows I174 and I174a not ticked (caller does it).
