# I89a: one scale for sel_rp_cell.note

Defect: MOD period 428 was note 61 (openmpt) but 25 (nuade).

Cause: nuade names notes the ProTracker way (period 856 = C-1, 428 = C-2, value 24; adapter added 1 -> 25). libopenmpt shows the same period three octaves higher (C-5 = 61).

Fix (plugins/replayers/nuade/adapter.c, note_of): add 12 * 3 via NUADE_TO_OPENMPT_OCTAVES. Scale documented on sel_rp_cell.note in include/selecta/replayer.h.

Other nuade tracker formats: every one derives its value from Paula periods in the same ProTracker numbering (nuade_period_to_note / _shifted) and declares any different base in NuadeColumn octave_shift, which note_of already applies. So one constant covers all of them. Not verified by playing every format; only protracker4 has a cell test (T41). Other players with their own base (tronic -2, sonicarranger -3, oktalyzer/soundmon/facethemusic C-1 = 1) rely on their declared octave_shift being correct; unchecked against libopenmpt since libopenmpt does not play them.

Test: T41 pattern-cells, nuade row expected note=25 -> 61 (both modules hold the same patterns, so equality across adapters is asserted by the shared expected value).
- Red (a74692c): `56 - pattern-cells (Failed)` / `6/8 Test #56: pattern-cells ....***Failed`
- Green after fix; 8 of 8 of pattern-cells|nuade|openmpt pass. Host and amiga builds green.
- Revert check: the red commit is the identical test on the unfixed source, which failed.
