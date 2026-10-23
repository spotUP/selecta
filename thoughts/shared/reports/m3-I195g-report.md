# I195g: HTTP status number in the stream open error and the text UI

Plan: 2026-10-25-m3-library.md Phase 11; design doc section 1a.

Fix: the failed request's status is now carried to the engine's existing open_failed error.
- src/library/stream_open.c/.h: `sel_live_status(live)` (stored by the download thread before eof/failed; Amiga stub returns 0)
  and `sel_stream_status()` (status of the last whole-body read, for the non-live path).
- src/engine/engine.c: `open_status` is set in read_song (head failure and sel_stream_read) and in the drain path;
  `report_open_failed` prefixes the detail: `ERROR open_failed "http_status=404 <url>"`. The status leads so the 64-byte
  detail cap cannot cut it. Non-HTTP failures keep the old detail (the path).
- plugins/ui/text/text_ui.c: tui_on_event copies any engine error to the status line as `Error: <code> <detail>` (the UI
  ignored errors before).

Test: stream-status (tests/net/stream_status.cmake, extended; now gets -DSELECTA) asserts for 404 and 503, through
selecta-cli script, `ERROR open_failed "http_status=<n> `; and through `selecta --exec "OPEN ..."` that the dumped screen holds
`Error: open_failed http_status=<n> `. Fail-before verified: with src/plugins reverted the test fails with "I195g: 404
error does not carry the HTTP status number". Passing with the fix: stream-status, stream-url, stream-live*, stream-record,
text-ui-keys-wait and basic (9 cases); build/amiga compiles.

Open: the Amiga has no live stream (stub status 0), so only the whole-body path can carry a status there; untested on the rig.
