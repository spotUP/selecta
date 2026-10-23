# I194: MP3 and Ogg Vorbis replayer plugins (D-M3-8)

## What was built
- `plugins/replayers/minimp3/` (plugin id `minimp3`, ext mp3,mp2) and `plugins/replayers/stb_vorbis/` (plugin id
  `stb_vorbis`, ext ogg): RENDER-kind replayers copied from the openmpt adapter pattern (`sel_add_plugin`, hooks set at
  run time). Metadata: FORMAT ("MP3" / "Ogg Vorbis"), CHANNELS, DURATION_MS, SIZE; Ogg also TITLE and AUTHOR from Vorbis
  comments. Seek supported by both.
- `plugins/replayers/stream/stream_pcm.h`: shared header-only linear resampler. The RENDER hook must deliver frames at the
  rate given to START, so the plugin resamples; equal rates pass through untouched.
- Vendored single headers: `third_party/minimp3/` (minimp3.h, minimp3_impl.c, LICENSE CC0, VERSION) and
  `third_party/stb_vorbis/` (stb_vorbis.c v1.22, VERSION). Copies of the files in the libopenmpt tree, not submodule edits.
- Test `stream-decode` (`tests/m3/stream_decode.sh`, fixtures `tests/m3/stream/sine.mp3` 2716 bytes and `sine.ogg` 4559 bytes).

## Test
One reachability test through `selecta-cli render`. Sentinel: the `replayer=<plugin>` line. Asserts: plugin name,
`rendered_frames` (mp3 44100 Hz exactly 14976 = 13 x 1152; mp3 at 22050 7488 +-2; ogg at native 22050 6615 +-64; ogg at
44100 13230 +-128), non-silent output.
- After: `ctest -R stream-decode` passes.
- Fail-before: the same script against the main checkout CLI (no plugins) fails: `FAIL: ...sine.mp3 at 44100 failed`.

Guards: `link-warnings`, `sdk-isolation` green; `commands --unimplemented | wc -l` stays 8; `plugins --lint` clean.

## Amiga
`build/amiga` (68020 toolchain) compiles and links both plugins, no errors. Not gated. They carry `SEL_RP_NEEDS_FLOAT 1`
(minimp3 and stb_vorbis synthesise in float), so the M2 machine filter hides them on a machine without an FPU. Not run on
the rig: unverified for speed on a real 68020 or 68060; a decode-speed check on the rig is open.

## Notes
- stb_vorbis allocates with malloc internally (not the host allocator); it needs a later pass if the Amiga alloc freeze rule
  must cover it.
- Ogg decode yields about one block more than the granule length (6656 versus 6615 source frames); the test tolerance covers it.
- No ID3 tag text for MP3 yet (the ID3v2 tag is only skipped).
- Byte sources (files only here; URL/stream input) belong to I195, not this row.
