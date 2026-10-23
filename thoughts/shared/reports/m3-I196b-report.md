# I196b: record while playing

Plan: thoughts/shared/plans/2026-10-25-m3-library.md Phase 11, F36.

## Wiring
- New engine option `record_to` (include/selecta/engine.h SEL_OPTIONS), set with the existing `OPTION record_to <file>` verb. No new verb, so `commands --unimplemented` stays at 8.
- src/engine/engine.c (URL branch of the OPEN reader): when `record_to` is non-empty, `sel_live_open_record(..., rec, 0)` replaces `sel_live_open`. The ring/record sink in stream_open.c is reused unchanged: one HTTP request feeds playback and the file.
- Limit: option values are 63 characters (SEL_OPTION_VALUE_MAX 64), so long paths need a relative name. Empty value turns recording off.
- Documented in thoughts/shared/plans/2026-10-22-architecture-design.md next to `OPTION hook_command`.
- `@stream_live` now also prints `record_taps=N` (sentinel).
- tests/net/slow_server.py logs one line per request to `<portfile>.requests`.

## Test
`stream-record-play` (tests/net/stream_record_play.cmake), run via `selecta-cli script`: asserts opens=1, audio_bytes>0, audio_done=0, record_taps>0, exactly 1 server request, recorded file equals the served prefix byte for byte, render not silent.
- Fail-before (engine.c hunk reverted): "I196b: audio did not play, or the record sink did not run (record_taps)".
- After: passes. stream-*, record, http-client related tests: 15 of 15 pass. Host and Amiga builds clean. Unimplemented count 8.

Not done: the CLI `play` path has no flag; the option covers script/engine OPEN. Amiga stays a stub until I199.
