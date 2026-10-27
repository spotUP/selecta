# I534: pattern-cells and home-view failing on main (726547d)

Both failures were stale expectations after intended behaviour changes. The product code is correct; no code fix.

## pattern-cells
- Breaking commit: 61f2ae8 (I282, 2026-10-25) added RP_POSITION to the nuade adapter (plugins/replayers/nuade/adapter.c rp_position).
  The engine (src/engine/engine.c emit_position) appends order, row, pattern to POSITION when the replayer reports a position,
  so the nuade POSITION event went from `POSITION 0 0 0 0` to `POSITION 0 0 0 0 0 0 0`, the same shape openmpt always had.
- Evidence: tests/m13/pattern.expected line 2 was the only diff; the openmpt block already had 7 fields.
- Fix: update tests/m13/pattern.expected line 2.

## home-view
- Breaking commit: e2e3346 (I478, 2026-10-26) put a Quick picks grid (distinct played albums, max 8) above the shelves
  (src/library/db/user.c home_text). One tile (Hardcore Heroes) makes home_rows 14, the test sum counted 13.
  Later commits in the same area (I485: entry text is the name alone, artist drawn separately) changed the screen too.
- Test fixes: home_view.cmake counts the Quick picks tiles with its own SQL query (quick=1) and adds them to the expected sum;
  home_view.screen golden updated (Quick picks heading + tile, no artist suffix); home_song.keys needs 3 Down (was 1) and
  home_album.keys 6 Down (was 4) because two rows were added above Recently played.

## Revert check
Before the test edits both tests fail on the unchanged tree (measured: POSITION diff; home_rows=14, want 13). After: both pass
(ctest -R 'pattern-cells|home-view'). The edits are to the tests themselves, so reverting them is the failing state above.
