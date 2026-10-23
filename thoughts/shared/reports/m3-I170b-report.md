# I170b: KEEP on|off and the kept query field

Design: D-M3-10 (KEEP on|off) in thoughts/shared/plans/2026-10-25-m3-library.md.

- commands.def: KEEP is now "s?" (no argument = on; anything but on/off reports `range`).
- engine.c cmd_KEEP picks sel_library_keep or the new sel_library_unkeep through the existing user_mark path (same errors).
- New SEL_USER_UNKEEP (last in the enum: the folder range check in user.c uses ordering) runs `DELETE FROM kept WHERE key = ?1`.
- query.c: `{"kept", K_BOOL, "kept", 0}`, compiled like liked (EXISTS / NOT EXISTS on kept.key = songs.key). Two golden rows (kept:yes, kept:no) in tests/m3/query.golden.
- tests/net/keep.cmake + keep.sel: run A (KEEP) lists play1 for kept:yes and the trim keeps it; run B (none) does not list it and the trim evicts it; new run C (KEEP, KEEP off) does not list it and the trim evicts it.
- Revert check: making the off branch set on=1 turns keep-command RED with "T78 keep C: kept:yes still lists play1 after KEEP off"; restored, green.
- Results: ctest -R 'keep|query|protocol-reflection' 8 of 8 pass; unimplemented count 8; host and amiga build with 0 warnings outside third_party (the build log's other warnings are third_party).
