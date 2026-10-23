# M3 rows I175 (CUE_SET, CUE_GOTO) and I176 (STICKER_SET/GET/DEL/LIST)

## Done
- Engine commands: CUE_SET, CUE_GOTO, STICKER_SET, STICKER_GET, STICKER_DEL, STICKER_LIST (`src/engine/engine.c`).
- Library: SEL_LIB_REQ_USER reused (no new request id). New ops SEL_USER_STICKER_DEL, SEL_USER_STICKER_LIST,
  SEL_USER_CUE_SET, SEL_USER_CUE_GET in `src/library/db/user.c`; wrappers `sel_library_sticker_*`, `sel_library_cue_*`.
- CUE_SET stores `song_frames*1000/rate` of the song being played (key = `hist_key`) in table `cues`.
  CUE_GOTO calls `reopen_current(SEL_BEGIN_SEEK, ...)`, the path SEEK uses, so SEEKED reports the cue position.
- STICKER_* take an explicit song key (MPD style): `STICKER_SET key name value`. Answers are LIBRARY_RESULT events with
  SRC_KEY, SRC_NAME, SRC_PATH (value), SRC_COUNT; LIST emits one per sticker in name order (first 8). No new tags/events.
- Errors: no_library, no_song, library_failed, no_such_cue, no_such_sticker (GET and DEL of a missing name).
- Sentinels: engine stats `cue_sets`, `cue_gotos`, `sticker_ops`; script directive `@user_stats`; `script --db <path>`
  with `--catalogue` for two-process tests.

## Tests
- `cues` (T81): SEEK 5000, 441 frames, CUE_SET 1, SEEK 0, CUE_GOTO 1 -> SEEKED 220941; CUE_GOTO 9 -> no_such_cue;
  second process on the same db gets the same SEEKED; counters asserted.
- `stickers` (T82): set/overwrite/get/list (name order)/del/get-after-del, restart keeps value, deleted stays gone.
- m3 label: 32 of 32 passed; stage-commands and ratchet pass. Ratchet 16 -> 10 (`selecta-cli commands --unimplemented`).
- Host and Amiga (68020) builds compile.

## Notes
- STICKER_LIST caps answers at 8 per command (engine ring); SRC_COUNT carries the real total.
- Values are capped at 255 bytes (SEL_STICKER_VALUE_MAX).
