# I195 report: URL and pipe paths in OPEN, M3U URL-only import, PIPE: on Amiga

## Changed
- New src/library/stream_open.{h,c}: sel_stream_is_url (http/https), sel_stream_is_pipe (`pipe:<cmd>` via popen on desktop; always 0 on Amiga so `PIPE:` takes the normal fopen path), sel_stream_read (growing buffer, cap 256 MB desktop / 24 MB Amiga; URLs via the existing sel_http_request, status 2xx required), counters sel_stream_reads/sel_stream_bytes.
- src/engine/engine.c read_song: URL or pipe path goes to sel_stream_read before fopen.
- apps/selecta-cli/main.c: script directive @stream_stats.
- M3U: no code change needed. playlist_resolve already treats scheme:// as absolute and m3u.c keeps lines verbatim; the test now guards it.
- Test stream-url (tests/net/stream_url.cmake, tests/m3/stream_url.sel, stream_m3u.sel, fixture fixture.test/stream.mod): drives selecta-cli script, holds net-fixture lock. Asserts counters, 176400-byte non-silent dumps, URL render == pipe render == local render (sha256), M3U with two URL-only lines imports ids=1,2 and PLAY fetches via the stream reader.

## Results
- Host: stream-url, link-warnings, net-guard, net-fixture*, lyrics, selecta-browse-play, queue-engine pass. `commands --unimplemented` = 8.
- Fail-before: engine.c hunk reverted -> stream-url FAILED with `ERROR open_failed http://fixture.test/stream.mod`.
- Amiga: build/amiga builds clean (stream_open.c compiled for 68k). No vamos/emulator run.

## Found
- Engine read_song does not set the net cancel flag; a stuck download blocks the engine thread until HTTP timeouts.
- Whole stream is downloaded into memory before playing (not true streaming).
- MP3/Ogg URL not tested separately (I194 fixtures belong to another agent); path is replayer-agnostic.
- position_ms stays 0 for this replayer, so raw dump hashes used.
- Ledger row I195 not ticked (todos.md edits forbidden).
