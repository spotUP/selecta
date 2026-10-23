# I195b: OPEN of an http(s) URL plays while it downloads

Plan: `thoughts/shared/plans/2026-10-25-m3-library.md` Phase 11. Needed by internet radio (I196).

## Changed

- `sel_live` (src/library/stream_open.{h,c}): a download thread feeds a lock-free 256 KB byte ring (SPSC, atomic
  counters). The ring is the memory bound; a full ring stops reading the socket. The thread runs with its own
  `sel_net_set_cancel` flag and `total_ms` of 0x7FFFFFFF (no whole-request timeout for an endless stream).
- Replayer SDK: optional `sel_byte_stream` (`read`, `read_wait`, `ended`) on `sel_song_source.stream`, enabled by the
  new tag `SEL_RP_STREAMING` (0x101C). `data`/`size` still hold a copy of the first 16 KB so the probe works.
- Engine (`read_song`, `prepare_slot`, `close_slot`): OPEN of a URL reads only the head, probes it, and hands the
  stream to a replayer that declares SEL_RP_STREAMING. Any other replayer gets the whole file through
  `sel_live_drain`, the same single request as before (stream-url counters unchanged). The slot closes the stream
  after `rp_close`. I195c cancel generation ends the head wait and the download (`net_cancels` still counts it).
- minimp3 adapter: stream mode decodes from a 32 KB window refilled without blocking. On underrun it renders
  silence (the audio thread never waits) and resumes when bytes arrive. No duration, size or seek for a stream.
- CLI: `@stream_live` prints `STREAMLIVE opens= audio_bytes= audio_done= received=`.
- Test `stream-live` (tests/net/stream_live.cmake, slow_server.py): the server advertises 108640 bytes, sends 60% and
  holds the connection. Through `selecta-cli script` the first audio is rendered after 65184 of 108640 bytes with
  `audio_done=0`, the render is 88200 non-silent bytes, and STOP plus exit finish in under 1 s.

## Evidence

- Fail-before: with the minimp3 STREAMING tag set to 0 (the engine then takes the whole-file path, which is the old
  behaviour) `stream-live` fails: "script failed rc=Process terminated due to timeout after 25 s".
- Green: `stream-live`, `stream-url`, `net-cancel-race`, `net-fixture-start/stop/selftest`, `library-close-cancel`,
  `stream-decode`, `stream-tags`. Lint clean. Host and amiga builds clean.

## Found

- Streaming is desktop only. The Amiga build has stubs: `sel_live_open` returns NULL and OPEN reads the whole file
  as before (sockets and cancel arrive with I199). A comment in stream_open.h says so.
- Only minimp3 streams. stb_vorbis and tracker replayers get the whole file via drain.
- A non-2xx body can enter the ring before the status is known; the probe then fails with `no_replayer`.
- ICY/shoutcast status lines and metadata are I196's concern.
- A SEEK or `skip_ms` on a live stream renders and discards (rp_seek returns 0); not meaningful for radio.
