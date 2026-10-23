# I195e: non-2xx body must not enter the ring or the record sink

Plan: 2026-10-25-m3-library.md Phase 11, D-M3-8.

Fix (src/library/stream_open.c): the download thread hands its sel_http_res to the sink (s->res). sel_http_request sets
res->status before the first body byte, so ring_sink returns 0 for a non-2xx status before the record tap and the ring
write. The request ends as sink_aborted, download() sets failed, and the engine reports the existing open_failed.

Test: ctest stream-status (tests/net/stream_status.cmake). New fixture routes /stream_404 and /stream_503 in
tests/net/fixtures/fixture.test/routes.json serve an HTML error page. Asserts ERROR open_failed (never no_replayer),
STREAMLIVE received=0 (ring), and `record` recorded_bytes=0 record_taps=0 (record sink).

Fail-before: "I195e: 404 error body reached the ring (want received=0)" (received=79 before the fix, 78 for 503).
Pass-after: stream-status, stream-url, stream-live, stream-live-ogg, stream-record, stream-decode, stream-tags,
http-client all pass (-j1); amiga build links.

Finding: with a short HTML body the pre-fix engine already ended in open_failed (the drain path sees failed=1), so
no_replayer in the ledger text was not reproducible with the fixture; the observable defect was the body reaching the
ring and the record file (and a 404 body that sniffs as a streaming format would have played). The "HTTP status error"
is the engine's existing open_failed code; no new error code carries the numeric status.
