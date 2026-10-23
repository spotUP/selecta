# I207 report: explicit Bxx to order 0 is a designed loop

Fix (adapter level, plugins/replayers/nuade/adapter.c): when rp_tick would turn a held LOOPED into ENDED
(landing order 0), it first reads the pattern cells of the row that was played on the LOOPED tick (rp_row_jumps,
through the existing rp_tracker_info / rp_tracker_cells hooks, all channels) and keeps LOOPED if any cell carries
effect 0xB. The pt4 jump byte is not used. No engine change, no third_party change.

Finding: nuade pt4 position_jump reports looped only for a jump to an EARLIER order, or to the current order when it
is the last by the IFF-header length byte. A B00 on order 0 of a one-order song is therefore never reported at all
(pre-existing nuade quirk, belongs inside nuade; not changed). The fixture uses 2 orders, B00 on order 1 row 63.

Test: gen_testmod `--jump0` (selecta-jump0.mod, 2 orders, B00 on the last row of order 1); tests/m13/first_jump0.sel;
first_check.cmake extended (loop-first-end): jump0 ends `loop`, selecta-short/selecta-test still `end`.
Revert check: with the `order == 0u` condition restored to its I94a form the test fails
("first_jump0: expected one 'SONG_ENDED loop', got 'SONG_ENDED end'"); with the fix it passes.
ctest -R 'queue|loop|crossfade|golden|nuade|position': 18 of 18 pass.
