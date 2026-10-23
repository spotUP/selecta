# I196: stream-to-disk recording (F36), `selecta-cli record`

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md` Phase 11; T94 row `recorded_bytes=` ("recorded file bytes equal served bytes").

## Changed
- `sel_live_open_record` (src/library/stream_open.{h,c}): the record sink lives in the existing download thread's
  `ring_sink`, so the raw bytes are written before they enter the ring from the one HTTP request (no second
  connection). The file opens before the thread starts; `limit` truncates exactly. `sel_live_close` (every cancel,
  STOP, exit path) joins the thread and closes the file, so it is always a complete prefix of the stream.
  New accessors: recorded, finished, failed, discard; counters `sel_stream_record_taps`, `sel_stream_recorded_total`.
- `selecta-cli record <url> <file> [--bytes N] [--seconds S]` (apps/selecta-cli/record.c): drains the ring, ends on
  stream end, byte limit, time limit, SIGINT/SIGTERM. Prints `RECORD stream_bytes= recorded_bytes= record_taps= end=`.
- Test `stream-record` (tests/net/stream_record.cmake) with slow_server.py (60% of 40 x sine.mp3, then stalls):
  byte limit (50000 bytes, cmp with the served prefix), time limit and SIGTERM (both: sha256 equals the 60% served
  bytes, SIGTERM exits in under 8 s), `record_taps` sentinel must be > 0.
- RULES.md Commands row added. Amiga: stubs (sel_live is desktop only until I199); the CLI verb prints an error.

## Evidence
- Fail-before: with the `record` dispatch disabled, `stream-record` fails (required `stream_record_ok` not found).
- Green: stream-record, stream-live, stream-url, net-cancel-race, net-fixture-*. Host lint clean; host and amiga build clean.
- Unimplemented count: still 8. `record` is a CLI subcommand, not an engine verb (no RECORD command exists in
  protocol.h), so `commands --unimplemented` does not list it.

## Found
- Not done: split-per-song on ICY metadata, tag writing and naming shared with F6 (plan text F36 mentions them;
  M3 row of T94 only requires the raw-bytes tap). ICY headers/metadata are not stripped, so a shoutcast server
  that interleaves metadata would put it in the file (needs `Icy-MetaData` request support in net/http.c).
- Recording while playing through OPEN (engine tap) is not wired; the API (`sel_live_open_record`) supports it.
- `nice -n 19` is refused by the sandbox hook in this worktree; builds ran un-niced with -j4.
