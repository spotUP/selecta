# M3 I171: FLAG wrong/skip, sounds-wrong export, skip honoured by SHUFFLE

## Done
- `SEL_LIB_REQ_USER` extended (not a parallel path): `SEL_USER_FLAG`, `SEL_USER_FLAG_SKIPPED`, `SEL_USER_FLAG_EXPORT`
  in `src/library/db/user.c`; wrappers `sel_library_flag`, `sel_library_flag_skipped`, `sel_library_flags_export`.
- New command `FLAG <wrong|skip> [note]` (commands.def, `cmd_FLAG` in engine.c) acts on the playing song and writes
  the existing `flags` table. Query keys `wrong:yes` / `skip:yes` already existed and now find flagged songs.
- `selecta-cli wrong [--db p] [--export file] [song [note]]` writes the flag and `sounds-wrong.txt` (`key<TAB>note`, sorted).
- `order_ensure()` (the single place the random candidate set is built) drops skip-flagged songs; counter
  `skip_excluded` (engine stats, `@library_stats`) is the sentinel that the check ran.
- Test: `user-data` (label m3) now also runs `tests/m3/userflags.sel` (golden `userflags.expected`) and the
  `wrong` CLI export (golden `sounds-wrong.golden`).

## Verification
- m3 label: 29 of 29 pass. queue-*, ratchet (unimplemented count unchanged, FLAG is implemented from the start),
  stage-commands, codec and playlist tests pass. Host and Amiga builds clean.
- Regression: with the exclusion in `order_ensure` disabled, `user-data` fails; restored, it passes.

## Not done
- RADIO does not exist yet (I186); its skip exclusion belongs there, it should reuse `sel_library_flag_skipped`.
- DEViLBOX push of the flags is I172.
