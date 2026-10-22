---
date: 2026-10-23
topic: nuade library as Selecta replayer plugin source and engine oracle
tags: [nuade, replayer, sdk, oracle, m0b]
status: complete
---

Checkout: /Users/spot/Code/DEViLBOX/third-party/nuade (paths below relative to it). Research budget hit; items marked UNCONFIRMED were not verified.

## 1. Upstream
- Remote: https://github.com/emoon/nuade (Daniel Collin / emoon) - UPSTREAM.md:3.
- Pinned commit 111c9657965f00e9a94fec3545ce238d43af57f4, pin date 2026-10-14 - UPSTREAM.md:4-5.
- License: MIT, "Copyright (c) 2026 Daniel Collin" (LICENSE:1-3; SPDX MIT). Caveat (README License section, THIRD_PARTY_NOTICES.md): most players are translations of Amiga replay routines / eagleplayers that "state no license"; NostalgicPlayer ports keep its MIT. The library licence is MIT but the player code has mixed provenance. Redistribution inside Selecta needs a per-player review.
- Vendoring: plain COPY of a tarball (not a submodule, no .git inside; `git ls-files` lists files as ordinary DEViLBOX blobs). `update.sh <sha>` re-fetches; tarball sha256 in UPSTREAM.md:6. DEViLBOX changes: none; its glue is in /Users/spot/Code/DEViLBOX/nuade-wasm/ (shim src/nuade_wasm.c, CMake, build.sh).
- Other copies ~/Code/devilbox-{dancehall,genre,r2f}/third-party/nuade: UPSTREAM.md pins the same commit 111c965 in all three (git HEAD of each differs only because it is the DEViLBOX worktree HEAD: DEViLBOX 2136df7, dancehall 95bb891, genre 7729b7b, r2f d868bb6). File-level diff not run (UNCONFIRMED identical content).
- Direct clone of upstream HEAD not checked; pinned commit may be behind (UNCONFIRMED).

## 2. Language and build
- Pure C, `project(nuade LANGUAGES C)` CMakeLists.txt:2; `c_std_11` PUBLIC, `C_EXTENSIONS OFF` (strict C11) :35-37. Not C99: m68k-amigaos-gcc needs -std=c11 (gcc 6+ ok, UNCONFIRMED for the Amiga toolchain version). Internals define `nullptr` as `((void*)0)` for C (src/nuade_base.h:22-23), use stdbool/stdint.
- Build: CMake >= 3.16. One STATIC lib `nuade` (alias `nuade::nuade`), src/*.c (12 files, 2473 lines) + one directory per player. `NUADE_PLAYERS` cache var = "all" or id list (e.g. "sonix;soundmon"), `NUADE_EXTRA_PLAYER_DIRS` for extra players (CMakeLists.txt:3-9, cmake/nuade_players.cmake). Links `m` on UNIX. No Makefile.
- External deps: libc + libm only. OS header: `<sys/stat.h>` in src/nuade_io_default.c:9 (default file IO, replaceable via NuadeIo in include/nuade/io.h). Host can use nuade_open_memory.
- Floats: yes. The mixer is float/double (src/nuade_mixer.c: f32 samples, `double position/pitch_hz/step` :255, read_sample :346). 34 of 121 player dirs use float/double (actionamics, activisionpro, bendaglish, bendaglishsid, davidwhittaker, deltamusic1/2, digmug, dss, facethemusic, fred_np, futurecomposer, gmc, instereo1/2, jamcracker, medley, musicassembler, oktalyzer, quadracomposer, ronklaren, sidmon2, silmarils, sonicarranger, sonix, soundcontrol, soundfactory, soundfx, soundmon, synthesis, thomashermann, voodoo). So on a 68020 without FPU this is soft-float or must be replaced (m68k-amigaos-gcc supports soft float; speed is the risk). The pure tick path of the other ~87 players is integer (grep-level, UNCONFIRMED per file).
- Endianness: no `__BYTE_ORDER`/bswap/packed hits in src, include, players (grep). Players read module bytes with explicit big-endian helpers (e.g. players/actionamics/actionamics.c:41, spl_module.c:45), so they are host-endian-independent by construction. Mixer `(const i16*)s->data` (nuade_mixer.c:347) assumes samples are host-endian int16: native on m68k, but BE-correct data only if players store converted samples natively. UNCONFIRMED per player.
- 64-bit: u64 used for frame counters and the scheduler remainder (src/nuade_scheduler.c:8) - fine with gcc long long on m68k; `size_t` casts throughout. No pointer-to-int assumptions found by grep (not exhaustive).
- Heap: 553 malloc/calloc call sites in players; no mmap/threads seen.

## 3. API
Public headers: include/nuade/{nuade.h (193 lines), player.h (177), voice.h (72), io.h, paula.h, viz.h}. `NUADE_PLAYER_API_VERSION 2` (player.h).

Host API (nuade.h):
- Registry: `uint32_t nuade_player_count(void)`; `const NuadePlayer* nuade_player_get(uint32_t index)`; `nuade_player_find(const char* id)` (src/nuade_registry.c:9-17; table is generated at build from the players list, `nuade_registry_players[]`).
- Open: `NuadeSongResult nuade_open(const char* path)`; `nuade_open_memory(const void* data, size_t size, const char* path)`; `nuade_open_ex(const NuadeOpenParams*)`; `void nuade_close(NuadeSong*)`. Path is needed for external files (companion sample files); probing picks the player (NuadeProbe_Supported / Unsupported / Unsure).
- Info: `const NuadeSongInfo* nuade_info(song)` {voice_count, subsong_count, default_subsong, timebase_hz, display_voices, player_mutes}; `nuade_player(song)`; `nuade_player_data(song)`.
- Play: `bool nuade_start(song, subsong, sample_rate)`; `NuadeRenderResult nuade_render(song, float* stereo, float* const* voices /*per-voice or NULL*/, uint32_t frames)` returns {frames, flags Looped|Ended}; `uint64_t nuade_position(song)`; `bool nuade_seek(song, frame)` (restarts and re-renders if seeking back, nuade_song.c:644); `nuade_duration(song, subsong, rate, max_frames, NuadeDuration*)`; `nuade_scan` / `nuade_scan_free`; `nuade_snapshot`; `nuade_set_repeat_count`; `nuade_set_stereo_separation`; `nuade_mute_voice`, `nuade_mute_instrument`, `nuade_mute_clear`.
- Display/metadata: `nuade_events`, `nuade_tracker_info`, `nuade_tracker_cells`, `nuade_vu`, `nuade_scope`, `nuade_meta(song, key)` (keys title/artist/format/tool/message), `nuade_instrument_count`, `nuade_instrument_name`.

Player vtable `NuadePlayer` (player.h): `api_version, id, name, version, extensions (csv, no dots)`; `probe(data,size,filename)`; `open(const NuadeSongSource*)` -> {user_data, info, error}; `close`; `start(user_data, subsong, rate, NuadeVoices*)`; `tick(user_data, NuadeVoices*)` -> {length in timebase units, flags Looped|Ended}; `render_raw(user_data, float* stereo, float* const* voices, frames)`; `metadata(user_data, NuadeMetadataSink*)`; `position`, `voice_info`, `events`, `tracker_info`, `tracker_cells`, `grid_ticks`, `dump_state`.
- Exactly one of tick / render_raw must be set (nuade_song.c:266). By grep NO player in players/ sets render_raw: all ship as tick (voice) players. render_raw is an escape hatch only (nuade_song.c:457).
- Voice backend contract (voice.h): `nuade_voice_play(voices, voice, const NuadeSample*, start_frame)`, `nuade_voice_queue`, `nuade_voice_stop`, `nuade_voice_set_pitch(hz = source frames/s, 0 = hold)`, `nuade_voice_set_volume(0..1)`, `nuade_voice_set_pan(-1..1)`, `nuade_voice_is_playing`, `nuade_voice_get_state(NuadeVoiceState{playing,sample,position(double),pitch_hz,volume,pan,wraps,event_frame,frame})`, `nuade_voice_muted`, `nuade_instrument_muted`. `NuadeSample{data, format S8|S16, length, loop_start, loop_length(0=one shot)}`. Backend is an opaque `NuadeVoices*`: Selecta could supply its own (Paula-style) implementation of these 11 functions; they are plain extern functions defined in src/nuade_mixer.c, so replacing the mixer = link a different file (needs the internal nuade_mixer.h hooks used by nuade_song.c - nuade_mixer_mix/advance/reset/tag_starts - UNCONFIRMED scope).
- Scale: 121 player directories = 121 nuade players covering 132 UADE formats (docs/players.md:8: "132 ported (by 121 nuade players), 13 not ported, 31 skipped on purpose"). Runtime count = nuade_player_count() (not run).

## 4. Reference renderer / CLI (A/B oracle)
- No CLI, no WAV writer, no tests/ directory in the library. The only executable-shaped item is the README example (README.md "Example"), which CI compiles (`ci.yml` awk-extracts it): `nuade_open` -> `nuade_set_repeat_count(song,1)` -> `nuade_start(song, default_subsong, 48000)` -> loop `nuade_render(song, float buf[1024*2], NULL, 1024)` -> fwrite until `NuadeRenderFlags_Ended`. Output: raw interleaved stereo float32, any rate you pass (README uses 48000), no header. Build it: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build; cc -std=c11 -Iinclude example.c build/libnuade.a -lm -o nuade-render`.
- Mixer (src/nuade_mixer.c:1): "Raw voice mixer: nearest sample, no filter, no ramp". Position is a double; step = pitch_hz / sample_rate (:255); sample read = `data[(u32)position]/32768 or /128`; pan gain = `NUADE_HEADROOM(0.5) * 0.5 * (1 -/+ pan*separation)` (:370-371). So: NO interpolation, NO Amiga low-pass/LED filter, NO volume ramps; per-voice float sum, stereo summed with +=. A reference render therefore equals "ideal Paula at arbitrary rate", not a hardware-exact render.
- Tick rate: each tick returns `length` in `timebase_hz` units (709379 = PAL CIA clock ticks, or 50 = VBlank; player.h NuadeSongInfo comment). Scheduler converts exactly with integer remainder carry: `total = remainder + length*sample_rate; frames = total / timebase_hz; remainder = total % timebase_hz` (src/nuade_scheduler.c:8-11). Deterministic, drift-free; this is the engine tick contract Selecta should mirror.
- The practical oracle in the DEViLBOX tree is /Users/spot/Code/DEViLBOX/tools/nuade/nuadeCompare.ts (renders public/nuade/Nuade.wasm headless and correlates against UADE: 20 ms loudness envelope of mono sum, Pearson correlation, best lag within +-200 ms; thresholds 0.95/0.90 mentioned in its header). Useful as method, not as sample-exact A/B. Nuade itself is A/B-tested against UADE (README Status), not bit-exact.

## 5. Memory
- Framework allocs: mixer created at open (calloc voices, track_muted; realloc instrument_muted grows on mute calls, nuade_mixer.c:60-128); song struct/tags/instruments via calloc/realloc at open and during metadata (nuade_song.c:104,125,217-245); Paula-memory channel calloc (nuade_paula_channel.c:80). The render/tick loop in nuade_song.c (:405-440) shows no allocation in the loop itself; `nuade_viz_record` per tick (viz.c) and `nuade_trace` not checked (UNCONFIRMED). Players with tick-time allocation not audited; one found doing calloc in a pre-walk (players/maniacsofnoise/mon_walk.c:225-227, scan at open).
- Sample ownership: `NuadeSample.data` points into PLAYER memory and "must stay valid while a voice uses it. The mixer reads it live" (voice.h), so a synth player may mutate waveform data under a playing voice; the song data buffer passed to open stays valid until player close (player.h NuadeSongSource comment). nuade_open_memory copies the input (nuade_song.c:217 malloc).

## 6. Test modules
- None in the repo: the only non-source files are LICENSE, .gitignore, .clang-format, players/pumatracker/pumatracker_waves.inc. No tests dir, no sample songs (tree is 8.9 MB of code). Golden inputs must come from elsewhere: DEViLBOX has a corpus fetcher (/Users/spot/Code/DEViLBOX/tools/nuade/fetchCorpus.ts, findCorpus.ts, nuade-compare.json) - not inspected for licences.

## 7. Known issues
- README Status: "very early stage of development. Expect bugs." Most work (porting and A/B testing) "done by AI agents". "Nuade may diverge from UADE where that makes sense."
- Player API is versioned (NUADE_PLAYER_API_VERSION 2) with no stability promise stated; version 0.1.0 in CMake.
- docs/players.md: 13 UADE players not ported, 31 skipped on purpose.
- Licensing provenance of ported players is unclear (see section 1).
- No TODO/FIXME hits in src/ or include/ (grep returned nothing).
- DEViLBOX notes a mixer-dependent quirk: SoundMon numbers voices by Paula channel (3 - n) - UPSTREAM.md bump note; hosts that show columns must map.
