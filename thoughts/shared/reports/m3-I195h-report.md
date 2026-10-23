# I195h: stb_vorbis decoder arena (no allocation after start)

Design doc section 1a (streaming); RULES.md rule 4. Follows m3-I195f-report.md.

## Problem
`rechain()` (I195f), called from `pull_live` on the render path at a chained Ogg BOS page, opened a new decoder with
`stb_vorbis_open_pushdata(..., NULL)`, so stb_vorbis called malloc (hundreds of times) mid-playback. `rp_open` also
opened with a NULL allocator, outside `host->mem_alloc`. The vendored stb_vorbis.c is the OpenMPT variant: with no
arena its temporaries use malloc, not alloca (the task text assumed alloca; the count test covers both).

## Fix (plugins/replayers/stb_vorbis/adapter.c)
- `rp_open` allocates one arena with `host->mem_alloc(VORBIS_ARENA_BYTES, SEL_MEM_FAST)`; it lives in
  `vorbis_state.arena` (`stb_vorbis_alloc`), is passed to `open_memory` and every `open_pushdata` (open_live retries
  and rechain), and is freed in `rp_close` and on every open-failure path.
- `rechain()` closes the old decoder first (its stream ended at the BOS page), sets `s->v = NULL`, then reopens into
  the same arena. If the headers are incomplete it returns 0 and the caller renders silence and retries as before;
  `pull_live` decodes nothing while `s->v == NULL`; `rp_close` and `rechain` guard NULL.
- VORBIS_outofmem: `rp_open` returns NULL and frees state and arena (tested with a 4 KB arena). In the live path a
  chained header that does not fit ends the stream like any undecodable header.

## Arena size: 768 KiB (`VORBIS_ARENA_BYTES`, overridable by -D for the test)
Measured with `stb_vorbis_get_info` on all five fixtures (22.05/44.1 kHz stereo, blocks 256/2048): setup 162.5 KB,
temporaries 4 KB (7.2 KB during setup), decoder struct about 2 KB: about 170 KB. Real files with bigger codebooks or
8192-sample blocks need up to roughly 3x; 768 KB is 4.5x the measured need. It does not scale with sel_machine:
plugins have no machine query, the arena exists only while an Ogg file is open, and a smaller arena on a small
machine would reject real files. Open item: if Amiga fast RAM proves tight with a Vorbis song loaded, either lower
the constant or add a machine-scaled size once the host exposes sel_machine to plugins.

## Tests
- New `stb-vorbis-arena` (tests/m3/stb_vorbis_arena_test.c + stb_vorbis_count.h): adapter and stb_vorbis.c are
  compiled into the test with counting malloc/alloca; drives the plugin hooks (entry, open, start, render across the
  chain, close) in live-stream and whole-file mode. Asserts 0 stb mallocs/allocas, host mem_alloc count fixed after
  open (2: state, arena) and balanced by close, title "Chain One" then "Chain Two" (sentinel that the BOS re-open ran
  inside render), sound after the chain. Verified to FAIL on the old adapter (668 mallocs live, 343 in memory mode).
- New `stb-vorbis-arena_tiny`: 4 KB arena, rp_open fails cleanly, no leak, no stb malloc.
- `ctest -R 'stb|ogg|stream-'`: 14 of 14 pass, including stream-live-ogg and stream-live-ogg-chained.
- No Ogg golden WAV exists in tests/golden, so there is no hash to move (grep for ogg/vorbis: none).
- Host and Amiga (build/amiga) builds green with -Wall -Wextra -Werror.

## Not covered
- The arena test runs on the desktop only; the Amiga FPU symbol test (stb-vorbis-fpu-symbols) was not run (needs the
  amiga_cli fixture / vamos).
- `sel_stream_pcm` and engine-side allocation after start are out of scope.
