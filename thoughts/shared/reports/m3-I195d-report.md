# I195d: stb_vorbis streams (SEL_RP_STREAMING)

Plan: 2026-10-25-m3-library.md D-M3-8, Phase 11.

- plugins/replayers/stb_vorbis/adapter.c: declares SEL_RP_STREAMING. Live source: headers via stb_vorbis_open_pushdata
  (read_wait until complete), then a 66000-byte window (max Ogg page 65307) refilled with the non-blocking read, one
  stb_vorbis_decode_frame_pushdata frame at a time, float to int16, mono duplicated. Underrun renders silence; no seek,
  no duration/size in a live stream. Same shape as the minimp3 live path; whole-file path unchanged.
- plugins/replayers/stb_vorbis/CMakeLists.txt: dropped STB_VORBIS_NO_PUSHDATA_API (third_party source untouched).
  I213a Amiga flags and fpu_math.h unchanged.
- Test stream-live-ogg: tests/net/stream_live.cmake now takes REPEAT/EXT; new fixture tests/m3/stream/long.ogg (8 s,
  218781 bytes; one long file because repeated Ogg files are chained streams). Server sends 60%.
- Fail-before (adapter unchanged, test added): stream-live-ogg Failed, "I195b: script failed rc=Process terminated due
  to timeout after 25 s" (whole-file drain waits for the stalled body).
- After: "first audio after 131268 of 218781 bytes, done=0", passed. stream-live, stream-url, stream-record,
  stream-decode, stream-tags, stb-vorbis-fpu-math, stb-vorbis-fpu-symbols, no-float-engine all pass (host and amiga built).
- Not done: Ogg radio with chained streams (track change in a chain) is untested; Opus/FLAC in Ogg stay unsupported.
