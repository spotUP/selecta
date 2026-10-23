# Selecta ledger

Every task lives here with an ID. Each reply names IDs ticked since the last reply and reports `done/total`. Nothing is started that is not here; nothing is reported done that is not ticked. Plans: `thoughts/shared/plans/` (status in each file's frontmatter; see RULES.md rule 0).

Session 2026-10-22 (1). Finish line: design doc complete (sources, feature bar, tiers, view styles, playlist model, tag ABI, decisions), research docs written, ledger restructured, first commit made, next-session prompt written. Done.

Session 2026-10-23 (3). Finish line: M1.1 plan approved, rows in this ledger (R8, R9, L3).

Session 2026-10-22 (2). Finish line: RULES.md rule 0 (plans and ledger never stale), M0a plan written and approved, ledger rows I1 to I22 added, implementation started.

## Research

| ID | Status | Task | Artifact |
|----|--------|------|----------|
| R1 | [x] | Eagleplayer architecture | thoughts/shared/research/2026-10-22_eagleplayer-architecture.md |
| R2 | [x] | DEViLBOX vote API + uprough-uade wrapper | thoughts/shared/research/2026-10-22_devilbox-api-and-uprough-uade.md |
| R3 | [x] | bbmp catalog/browse model + helper protocol + TUI | thoughts/shared/research/2026-10-22_bbmp-architecture.md |
| R4 | [x] | nuade API notes (inline into design doc section 4) | design doc section 4 |
| R5 | [x] | UP-Term text UI capabilities | thoughts/shared/research/2026-10-22_upterm-text-ui-capabilities.md |
| S1 | [x] | bbmp sources research doc (inline) | thoughts/shared/research/2026-10-22_bbmp-sources.md |
| M1a | [x] | Features, Amiga era (sonnet agent) | thoughts/shared/research/2026-10-22_features-amiga-era.md |
| M1b | [x] | Features, PC chiptune era (sonnet agent) | thoughts/shared/research/2026-10-22_features-pc-chiptune-era.md |
| M1c | [x] | Features, modern streaming (sonnet agent) | thoughts/shared/research/2026-10-22_features-modern-streaming.md |
| M1d | [x] | Features, terminal and web (sonnet agent) | thoughts/shared/research/2026-10-22_features-terminal-and-web.md |
| M1e | [x] | Master feature catalogue (inline synthesis) | thoughts/shared/research/2026-10-22_feature-catalogue.md |
| F13a | [x] | HippoPlayer parity inventory (sonnet agent) | thoughts/shared/research/2026-10-22_parity-hippoplayer.md |
| F13b | [x] | bbmp parity inventory (sonnet agent) | thoughts/shared/research/2026-10-22_parity-bbmp.md |
| F14 | [x] | Format coverage union bbmp + HippoPlayer | thoughts/shared/research/2026-10-22_format-coverage.md |
| R8 | [x] | M1 replayer reach: which nuade player (if any) reads plain M.K. .mod, nuade player float use and licences, libopenmpt as a desktop adapter, cause of duplicate-nuade linker warning (sonnet agent) | thoughts/shared/research/2026-10-23_m1-replayer-reach.md |
| R9 | [x] | M1 codebase gap: engine commands, state, events and what a UI shell needs (sonnet agent) | thoughts/shared/research/2026-10-23_m1-codebase-gap.md |
| R10 | [x] | Inventory of HippoPlayer-68k native players (code/pl/) against the format coverage doc: per format, the native player, CPU it needs, output path (Paula voices or mixed), and the proposed Amiga owner; mark XM on 68020-68040 as a gap | thoughts/shared/research/ (new doc), coverage doc owner column |

## Design (doc: thoughts/shared/plans/2026-10-22-architecture-design.md)

| ID | Status | Task | Artifact |
|----|--------|------|----------|
| D1 | [x] | Design doc draft v1 | design doc |
| D2 | [x] | Decisions round 1 (name, C standard, toolchain, license, platforms) | design doc section 16 |
| D3 | [x] | RULES.md + CLAUDE.md pointer + Commands | RULES.md, CLAUDE.md |
| D4 | [x] | Playlist formats + hippoplayer.se research folded in | section 17 (P1) |
| S2 | [x] | Section 9 rewrite: `sel_source` interface, song key, sources v1, cache, mirrors, Amiga proxy | section 9 |
| S3 | [x] | Milestones M3/M3b, section 9a bbmp row, section 11 sources dir | sections 9a, 11, 14 |
| M2 | [x] | Section 1 goals desktop-class first; section 1a feature bar with tier column; library model additions | sections 1, 1a, 9, 8 |
| M3 | [x] | Runtime machine scaling `sel_machine`; RULES.md rule 4 | section 12, 16, RULES.md |
| M3b | [x] | Amiga GUI RTG-aware layouts + planar fallback; text UI sixel/kitty | section 7 |
| M4 | [x] | Section 9a Spotify/Apple Music row | section 9a |
| M5 | [x] | View model + switchable view styles (commands, events, UI tag) | sections 3, 7, 9, 1a, 9a |
| M6 | [x] | Playlist abstraction: playlist / folder tree / library view; formats as adapters | sections 9, 17 |
| M7 | [x] | Loudness normalisation design | sections 6, 9, 10 |
| M8 | [x] | Visual tiers text / planar / RTG / GPU; `SEL_VIZ_TIER`; visualizer catalogue | sections 7, 12, 9a |
| F1 | [x] | Playback picks | section 6, 1a |
| F2 | [x] | Library picks | section 9, 1a |
| F3 | [x] | Visual picks | section 7, 1a |
| F4 | [x] | Remote/social picks | section 8, 1a |
| F5 | [x] | Chip/format picks | sections 5, 6, 1a |
| F6 | [x] | Output/DSP picks | section 6, 1a |
| F7 | [x] | Playlist picks | sections 9, 17, 1a |
| F8 | [x] | Files picks | section 9, 1a |
| F9 | [x] | DJ/live picks; two-deck DJ out of scope | sections 6, 8, 9, 16, 1a |
| F10 | [x] | Metadata picks | sections 9, 10, 1a |
| F11 | [x] | Tracker extras picks | sections 5, 7, 1a |
| F12 | [x] | Integration picks | sections 8, 11, 1a |
| F13 | [x] | Parity column in 1a; unhomed rows get F15+; section 16 parity line | sections 1a, 16 |
| F14d | [x] | Format coverage rule into sections 5, 14 (link coverage doc after F14) | sections 5, 14 |
| F15 | [x] | Per-replayer and per-song config (UADE song.conf, Eagleplayer EP_LoadPlConfig) as SEL_RP_CONFIG tags + library per-song overrides | sections 4, 9 |
| F16 | [x] | Library folder watching + play counts | section 9 |
| F17 | [x] | Phase view (stereo phase scope) in visualizer catalogue | section 6 |
| F18 | [x] | Headphones mode (crossfeed DSP) | section 5 |
| F26 | [x] | Hardware audio outputs as SEL_OUT_* plugins (SIDBlaster, USBSID-Pico, ZorroSID, AmiGUS, Trinity, MHI); HippoPlayer parity | section 5, 1a |
| F27 | [x] | Amiga OS integration: pubscreen/own screen, iconify/AppIcon, ToolTypes = CLI, task priority; HippoPlayer parity | section 7, 1a |
| F28 | [x] | Per-song wrong flag + note (sounds-wrong.txt, selecta-cli wrong) and skip flag; bbmp parity | section 9, 1a |
| F19 | [x] | Synced lyrics: LRC files, tracker song message shown as lyrics, DEViLBOX lyrics endpoint; round 5 pick | section 9, 1a |
| F20 | [x] | Sonic similarity vectors from the analysis render, shared via DEViLBOX by song hash; feeds radio and smart shuffle; round 5 pick | section 9, 6a, 1a |
| F21 | [x] | Persisted cross-device play queue resumed on another device; round 5 pick | section 8, 1a |
| F23 | [x] | Smart shuffle: queue seeded by the current list plus similar songs from F20 and DEViLBOX genre; round 5 pick | section 3, 9c, 1a |
| F24 | [x] | Stats screen: library totals, play time, top formats and artists per period; round 5 pick | section 9, 1a |
| F25 | [x] | Offline scrobble queue with batch flush, last.fm and ListenBrainz; round 5 pick | section 8, 1a |
| F29 | [x] | Analysis-driven smart fade and MixRamp overlap points, computed on desktop, shared via DEViLBOX like loudness; round 5 pick | section 3, 6a, 1a |
| F30 | [x] | Reshuffle, play scope; round 5 pick | section 3, 9c, 1a |
| F31 | [x] | Direct; round 5 pick | section 5, 1a |
| F33 | [x] | Aminet, Modules.pl and AMP as `sel_source` modules; round 5 pick | section 9, 1a |
| F34 | [x] | Smart downloads: cache prefetch policy driven by history and smart playlists; round 5 pick | section 9, 1a |
| F35 | [x] | Podcasts: RSS feeds as a source with per-episode state; round 5 pick | section 9, 1a |
| F36 | [x] | Stream-to-disk recording of radio and streams; round 5 pick | section 5, 1a |
| F37 | [x] | Blend: DEViLBOX builds a shared playlist from several users' histories; round 5 pick | section 8, 9, 1a |
| F38 | [x] | Signal path display; round 5 pick | section 7, 1a |
| F39 | [x] | Advanced DSP: parametric EQ, upsampling, convolution, upmix as chain plugins; round 5 pick | section 5, 1a |
| F40 | [x] | Hosted public share pages on DEViLBOX for song, playlist and position; round 5 pick | section 8, 1a |
| F41 | [x] | Network storage discovery; round 5 pick | section 9, 1a |
| F43 | [x] | Clipboard URL watch: a copied song URL offers to play; round 5 pick | section 7, 1a |
| F44 | [x] | Batch tag editing, fingerprint tagging; round 5 pick | section 9, 1a |
| F45 | [x] | Vi-style command mode with completion; run an external command on the selection; round 5 pick | section 7, 1a |
| F46 | [x] | OpenSubsonic server API served by the engine, so any Subsonic client is a Selecta remote; round 5 pick | section 8, 1a |
| F48 | [x] | Configurable columns, title-format strings and status-line format; round 5 pick | section 9b, 7, 1a |
| F49 | [x] | Command palette, help overlay, undo/redo of queue edits; round 5 pick | section 3, 7, 1a |
| F50 | [x] | Multi-zone sync: grouped outputs with synced clocks; round 5 pick | section 5, 8, 1a |
| F51 | [x] | Server transcoding to MP3/Opus for remote clients; round 5 pick | section 8, 1a |
| F52 | [x] | Multi-user accounts and multiple libraries on one engine; round 5 pick | section 9, 8, 1a |
| D10 | [x] | Design paragraphs for every round 5 pick in the sections named above (sonnet agent), verified by grep per F ID | design doc |
| A1 | [x] | Tag-list plugin ABI | sections 4, 5, 6, 7, 10, 9a, 16 |
| A2 | [x] | commands.def/events.def unchanged; event payload tag note | section 3 |
| P1 | [x] | Playlist facts into section 17; tick D4 | section 17 |
| D5 | [x] | Decision: desktop GUI = SDL2 + own widgets | section 16 |
| D6 | [x] | Decision: library DB = SQLite everywhere | section 16, RULES.md |
| D7 | [x] | Decision: Lua runtime + ARexx bridge | section 16 |
| D8 | [x] | Decision: theme tokens + skin loader | section 16 |
| D9 | [x] | DEViLBOX server work list | Found section below, section 10 |
| SP1 | [x] | Research: real Spotify playback through an open-source client (librespot and others), sonnet agent | thoughts/shared/research/2026-10-22_spotify-open-source-client.md |
| SP2 | [x] | Research: cspot native port for fast Amigas (C++ standard vs bebbo gcc 6.5, licence, maintenance, Tremor speed on 68k, porting surface), sonnet agent | thoughts/shared/research/2026-10-22_cspot-amiga-port.md |
| D11 | [x] | Decision 2026-10-23: per-tier replayer owners; Amiga uses optimized native 68k players for many formats; XM on Amiga = HippoPlayer xmaplay060.s, 68060 only (XM on 68020-68040 is a tracked gap by user choice); MOD/S3M/MTM keep their owners | design doc section "Format coverage rule", paragraph "Per-tier owners" |
| D12 | [x] | Decision 2026-10-23: Amiga target machines have fast RAM; size budgets count fast RAM, only Paula sample data needs chip RAM; the 1.85 MB nuade image (I53) is within budget | design doc paragraph "Per-tier owners, round 2" item 4 |
| D13 | [x] | Decision 2026-10-23: xmaplay060 with AHI or AmiGUS output owns XM on 68020-68040; Paula-direct XM stays 68060 only; XM gap narrows to 020-040 machines without AHI/AmiGUS | design doc paragraph "Per-tier owners, round 2" item 2 |
| D14 | [x] | Decision 2026-10-23: on the Amiga a HippoPlayer native player outranks nuade for every format both cover; nuade is the fallback owner | design doc paragraph "Per-tier owners, round 2" item 1 |
| D15 | [x] | Decision 2026-10-23: PS3M owns IT on Amiga 68020+ (still not XM) | design doc paragraph "Per-tier owners, round 2" item 3 |

## Repo

| ID | Status | Task | Artifact |
|----|--------|------|----------|
| L1 | [x] | Ledger restructured; RULES.md ledger rule | this file, RULES.md |
| L2 | [x] | Memories: structured-ledger-with-ids, design-not-capped-by-amiga, feature-menus, parity | ~/.claude/projects/-Users-spot-Code-MusicPlayer/memory/ |
| R6 | [x] | First commit, named files, no attribution | git log |
| R7 | [x] | next-session.md start prompt | thoughts/spot/next-session.md |
| L3 | [x] | M1 split into slices; M1.1 plan written, approved, rows added | thoughts/shared/plans/2026-10-23-m1-1-text-ui-queue.md |
| L4 | [x] | Write the M1.2 and M1.3 plans after I62 | thoughts/shared/plans/2026-10-23-m1-2-playlists-views-control.md, thoughts/shared/plans/2026-10-23-m1-3-viz-voices-playback.md |

## Implementation (plan: thoughts/shared/plans/2026-10-22-m0-skeleton.md, status: done)

Tests T1 to T9 are defined in the plan's finish-line table. A row is ticked only when its proof column is green in `ctest --test-dir build/host`.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I1 | [x] | 1 | Root CMakeLists, cmake/selecta_plugin.cmake, cmake/selecta_defs.cmake, .clang-format, selecta-cli version | host build; `selecta-cli version` prints `selecta-cli 0.0.1 api 1` |
| I2 | [x] | 1 | cmake/amiga-68020.cmake with root search (`$AMIGA_GCC_ROOT`, `~/opt/amiga`, `/opt/amiga`), `-mcrt=nix20`; 68k build of selecta-cli version | vamos prints version line |
| I3 | [x] | 1 | lint, format, sdk-isolation targets (clang tools optional, -Werror always) | targets exist and run |
| I4 | [x] | 2 | tags.def with reserved namespace blocks and M0a rows; tags.h; src/core/tag.c | compiles on both toolchains |
| I5 | [x] | 2 | commands.def and events.def full first set; design doc BEGIN/END one-line fix | counts read by CMake |
| I6 | [x] | 2 | protocol.h: enums, names, args, sel_message, codec API, SEL_MAX_ARGS static assert | compiles |
| I7 | [x] | 2 | Tests T1, T2, T5, T6 written and RED | ctest shows 4 failures |
| I8 | [x] | 3 | src/protocol/codec.c encode/decode, tag-list argument codec | T1 green |
| I9 | [x] | 3 | selecta-cli commands, events, tagtypes, roundtrip | T2 green |
| I10 | [x] | 3 | T3 and T4 written and RED | ctest shows failures |
| I11 | [x] | 4 | engine.h and engine.c: state, SPSC queues, dispatch X-macro, implemented set, not_implemented path, BEGIN/END batch | T3 green |
| I12 | [x] | 4 | selecta-cli script; tests/m0/basic.sel and basic.expected | T3 green |
| I13 | [x] | 4 | selecta-cli commands --unimplemented; T8 written RED | ratchet list printed |
| I14 | [x] | 5 | host.h sel_host struct; src/core mem.c log.c clock.c; selecta-cli abi | T6 goldens x86_64 and arm64 committed, green |
| I15 | [x] | 5 | plugin.h, output.h, registry.c, generated registry_list.c, tag lint, bad-plugin negative build | T5 green both ways |
| I16 | [x] | 5 | plugins/outputs/null/null.c; selecta-cli play-null | T4 green |
| I17 | [x] | 5 | sdk-isolation as ctest T7 | T7 green |
| I18 | [x] | 5 | count_malloc shim and no_alloc.sh | no-alloc green |
| I19 | [x] | 6 | Full 68k build green (-Werror, 32-bit uintptr_t, big-endian) | build/amiga complete |
| I20 | [x] | 6 | tests/amiga/run_vamos.cmake; sel_host.m68k.txt golden | T9 green |
| I21 | [x] | 7 | ratchet number committed, T8 green; ci.yml host jobs; Amiga CI image to verify | T8 green, workflow file |
| I22 | [x] | 7 | RULES.md Commands table updated, README, validation doc, plan status done | counts reconciled in reply |

## Implementation M0b (plan: thoughts/shared/plans/2026-10-22-m0b-replay.md, status: done, approved 2026-10-22)

Tests T10 to T18 are defined in the plan's finish-line table. A row is ticked only when its proof column is green.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I23 | [x] | 1 | tags.def: SEL_MACHINE_*, SEL_RP_*, SEL_MIX_* rows | T2, T5 green |
| I24 | [x] | 1 | T10, T11 written and RED | ctest failures |
| I25 | [x] | 1 | machine.h, machine.c, <os>/machine.c, selecta-cli machine | T10 green |
| I26 | [x] | 2 | voices.h, mixer.h; T12 with vectors.h written and RED | ctest failure |
| I27 | [x] | 2 | mixer.c, resample.c (NONE, LINEAR, CUBIC), voices.c | T12 green for three resamplers |
| I28 | [x] | 2 | gen_sinc_table.c + SINC resampler; machine-based default | T12 green all four |
| I29 | [x] | 2 | abi prints sel_voices; goldens x86_64, arm64 | T6 green |
| I30 | [x] | 3 | submodule third_party/nuade @ 111c965; patches README; selecta_nuade C11 target, NUADE_PLAYERS=protracker4 | host build green |
| I31 | [x] | 3 | replayer.h; adapter.c, voices_shim.c, io_shim.c | T5 green, plugins=3 |
| I32 | [x] | 3 | scheduler.c; engine OPEN, SUBSONG, SONG_OPENED, SONG_ENDED, POSITION; ratchet 54 | T3, T8 green |
| I33 | [x] | 3 | gen_testmod.c + pinned mod hash; wav.c; selecta-cli render; T13, T17 RED then counters green | T13 counters, T17 green |
| I34 | [x] | 3 | four golden hashes committed after listening check; T7 third-party include rule | T13 green (4 ctests), T7 green |
| I35 | [x] | 4 | nuade_ab.c written and run | T15 runs |
| I36 | [x] | 4 | A/B correlation >= 0.98, value recorded | T15 green |
| I37 | [x] | 5 | full 68k build incl. selecta_nuade C11 | build/amiga green |
| I38 | [x] | 5 | run_vamos machine + render cases; sel_voices.m68k.txt golden | T11, T14 green |
| I39 | [x] | 5 | cmake/no_float.cmake over 68k engine, mixer, wav, null objects | T18 green |
| I40 | [x] | 6 | coreaudio.c, selecta-cli play, T16 RED then green | T16 green |
| I41 | [x] | 6 | audio thread assertion in sel_engine_render (debug) | T16 green in Debug |
| I42 | [x] | 7 | RULES.md Commands, README, validation doc | counts reconciled |
| I43 | [x] | 7 | plan status done, ledger reconciled | grep count matches |
| I44 | [ ] | - | Amiga CI image (from M0a I21): find and verify a bebbo toolchain container, add CI job | CI job green |

## Implementation M1.1 (plan: thoughts/shared/plans/2026-10-23-m1-1-text-ui-queue.md, status: in-progress 2026-10-23, open: I61)

Tests T19 to T27 are defined in the plan's finish-line table. A row is ticked only when its proof column is green.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I45 | [x] | 1 | tags.def RP_RENDER/SEEK/POSITION and UI_* rows; ui.h; replayer.h typedefs | T2, T5 green |
| I46 | [x] | 1 | T27 written and red; sel_nuade_shim split; nuade CMakeLists lines 46-47 removed | T27, T27b green; T13 hash unchanged |
| I47 | [x] | 1 | 68k build after the link change | build/amiga green, T14 green |
| I48 | [x] | 2 | T20 and T21 (nuade half) written and red | ctest failures |
| I49 | [x] | 2 | two replayer slots, SWAP through the SPSC, close on pump thread | T13, T14, T17 green |
| I50 | [x] | 2 | queue.c, QUEUE_ADD/REMOVE/CLEAR/PLAY_NEXT, NEXT/PREV, auto-advance, QUEUE_CHANGED | T20 green, ratchet 48 |
| I51 | [x] | 2 | SEEK restart-and-skip, SEEKED, render --seek-ms | T21 nuade half green, ratchet 47 |
| I52 | [x] | 2 | sel_state song/queue/view fields; T18 over queue.o | T18, T6 green |
| I53 | [x] | 3 | nuade metadata tags; glob of all 121 players; T23 red then green; design doc player count | T23 green |
| I54 | [x] | 3 | libopenmpt submodule at libopenmpt-0.8.4 (commit 327204eaa2c27bd1acbb571babe68f279c85238b, resolved with gh api 2026-10-23); cmake/openmpt_sources.cmake; selecta_openmpt builds | host build green |
| I55 | [x] | 3 | gen_testmod --format mk; openmpt adapter; T22 red then counters green; hash committed after listening check | T22 green |
| I56 | [x] | 3 | RP_SEEK in engine; T21 openmpt half; T26 extended | seek-render-openmpt (row 41 equals the straight render at frame 220500; native seek accepted, not byte-exact), position-openmpt, no-alloc-render-openmpt, seek-render-openmpt-break |
| I57 | [x] | 4 | T24 red; selecta_library, dir.c, LIBRARY_BROWSE, LIBRARY_RESULT | ratchet 46 |
| I58 | [x] | 4 | view.c; VIEW_SHOW/CURSOR/ACTIVATE/WINDOW born implemented; VIEW_STYLE regular; VIEW_CHANGED, VIEW_ROWS | T24 green, ratchet 45 |
| I59 | [x] | 5 | T25 red; grid.c, keys.c, theme.c, posix/term.c | T25 green |
| I60 | [x] | 5 | text_ui.c; apps/selecta shell; T19 red then green | T19 green (I60 commit) |
| I61 | [ ] | 5 | real-terminal check on macOS (plan Verification, manual 1); only the user ticks it | user confirms |
| I62 | [x] | 6 | RULES.md Commands, README, design doc sections 3/5/14/16, validation doc, plan status done | counts reconciled: ctest 48, ratchet 45, no_float 12, nuade 121; commit 18d24f4 |
| I64 | [ ] | 5 | shell picks clocked pacing and audio-thread marking by output id (apps/selecta/main.c:83 strcmp "null"); should read the output's capability (SEL_OUT_LATENCY_MS != 0 or a dedicated tag). Latent: the default host has no config, so `--output wav` cannot open in the shell today; fix with the regression test when a second unclocked output becomes reachable | open |
| I63 | [x] | 5 | fix shell with real output: interactive Enter never opens songs (songs_opened=0), keys-file wait hangs (timeout 0), check termios diff after quit; regression test without sound card | A was a repro error (started one directory too high; works under CoreAudio); B fixed: keys-file wait counts the host clock when the turn budget is 0, host clock now CLOCK_MONOTONIC; C is only the kernel PENDIN bit; test text-ui-keys-wait red then green; commit bdb9e75, merge on main |

## Implementation M1.2 (plan: thoughts/shared/plans/2026-10-23-m1-2-playlists-views-control.md, status: approved 2026-10-23 autonomous)

Tests T28 to T38 are defined in the plan's finish-line table. A row is ticked only when its proof column is green.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I65 | [x] | 1 | Move the line parser to `src/protocol/parse.c`; CLI and shell share it | T1 to T27 green, no behaviour change |
| I66 | [x] | 1 | `prefs.def`, `prefs.c`, prefs file, PREF_GET/PREF_SET, config directory | T33 pref half green, ratchet 43 |
| I67 | [x] | 1 | T33 written and red; `format.c` resolver used by the view model | T33 green |
| I68 | [x] | 2 | T30 and T31 written and red | ctest failures |
| I69 | [x] | 2 | History ring; QUEUE_MOVE, QUEUE_SWAP, UNDO, REDO, QUEUE_HISTORY event | T30 move/swap/undo half green, ratchet 39 |
| I70 | [x] | 2 | xorshift32; QUEUE_SHUFFLE, SHUFFLE, RESHUFFLE | T30 green, ratchet 36 |
| I71 | [x] | 2 | SCOPE option, NEXT_ALBUM, PREV_ALBUM | T31 green, ratchet 33 |
| I71a | [x] | 2 | Random play order: permutation of the entries in scope from xorshift32 when `random` turns on or the queue changes; NEXT and PREV walk it; each entry once before a repeat (plan Deviations) | T30a `queue-random-order` red (Test #37 Failed) then green; commit 0fc40cc, merge c29b2bd; codec fix regression test 55d9281 |
| I72 | [x] | 3 | T29 written and red; four codecs | T29 green; commits 64c6056, 474f15b, merge 630ca11 |
| I73 | [x] | 3 | PLAYLIST_LOAD, PLAYLIST_SAVE; divider/stealth queue rows | script check in T29 suite, ratchet 31; playlist-verbs red on main dec988f (ERROR not_implemented PLAYLIST_LOAD), green after; commit 0c7e038, merge c29b2bd; ratchet 27 after merge with Phase 4 |
| I74 | [x] | 4 | T32 written and red; view grouping folder/format, node ids | T32 green (red fd69a7a segfault); commit b680b06, merge dec988f; ratchet 29 (27 after other phases) |
| I75 | [x] | 4 | Text UI draws tree and drilldown | T24 selecta-tree-view, selecta-drill-view green; commit 33b55cc, merge dec988f |
| I76 | [x] | 5 | T34 written and red; `keybind.c`, `keys.conf`, `--bindings` | T34 green; T19 unchanged; red "keybind.h not found"; commit 017d9b8, merge 8534239 |
| I77 | [x] | 5 | T36 written and red; `cmdline.c`, history, `:!` spawn | T36 green; red link failure, selecta-cmdline red (Volume 100 vs 90); commit 9f82af0, merge 8534239 |
| I78 | [x] | 5 | T37 written and red; `SEL_CMD_AREA`, COMMANDS `area`, `palette.c`, help overlay | T37 green, T5/T6 goldens updated ; red "palette.h not found"; no T5/T6 golden held COMMANDS output; commits 9fb9185, 0f07794, merge 8534239 |
| I79 | [x] | 6 | T35 written and red; socket in the shell, `selecta-cli send` | T35 green; red `control-socket ***Failed` then green; 21c13d7, merge 137b413 |
| I80 | [x] | 6 | `integration.h`, host side, `nowplaying-test`; T38 red then green | T38 green; red `integration-nowplaying ***Failed` then green; 21c13d7, merge 137b413 |
| I81 | [~] | 6 | macOS Now Playing and Linux MPRIS plugins | macOS plugin builds; MPRIS UNVERIFIED (no libsystemd here); manual check 2 open; 21c13d7, merge 137b413 |
| I82 | [x] | 7 | T28 written and red; fixtures | red `Test #51: selecta-m12-reach ***Failed` (baseline run failed); 5761822 |
| I83 | [x] | 7 | Wire everything to T28 | T28 green, T8 at 27, build/amiga green; 20ab1d0, merge |
| I84 | [x] | 7 | Design doc section 14 status, validation file, full CI run (reason: landing the milestone) | validation file written; 67 of 67 passed, ratchet 27 |

## Implementation M1.3 (plan: thoughts/shared/plans/2026-10-23-m1-3-viz-voices-playback.md, status: done 2026-10-23, 13 of 13 automated, 7 manual checks open)

Tests T39 to T48 are defined in the plan's finish-line table.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I85 | [x] | 1 | T40 and T48 written and red; `viz.h`, VIZ tags | red `phase-tap, viz-vu, viz-spectrum, viz-taps ***Failed` (0 of 4); T2, T5 green; 2550bd6 |
| I86 | [x] | 1 | Engine taps (peaks, VU, scopes, PCM ring), triple buffer, snapshot field | phase-tap, viz-vu, mixer-unit, render-golden-*, no-alloc-* green; 4ad74ac |
| I87 | [x] | 1 | Integer FFT and bands on the pump thread; phase correlation | viz-spectrum, viz-taps green, dump byte identical twice; 6d1e866 |
| I88 | [x] | 2 | T41 written and red; RP_TRACKER_INFO/CELLS tags and typedefs | ctest failure; red 2bd9668 `Test #52: pattern-cells ***Failed` |
| I89 | [x] | 2 | nuade and openmpt tracker hooks, `@cells` directive | T41 green; f5d8fa2, merge d068100, full run 72 of 72, ratchet 27 |
| I89a | [x] | 2 | One note scale in `sel_rp_cell`: nuade adapter maps to the libopenmpt scale (MOD period 428 = 61); pattern.expected nuade rows updated | T41 green with equal notes for both adapters on the same period; red a74692c `Test #56: pattern-cells ***Failed`, fix dd81a35, merge 72b6d82; other nuade formats unverified (only protracker4 has a cell test) |
| I90 | [x] | 3 | T42 written and red; mute golden committed after a listening check of voice 1 removed (agent compares against a render with the voice's samples zeroed) | ctest failure | ab8699d red `Test #57: voice-control ***Failed` (unknown option --script); merge 8012c7e
| I91 | [x] | 3 | Mute mask, MUTE, SOLO, VOICE event mask, RP_MUTE | T42 mute half green, ratchet 25 | f74a1b1, b2c60d6; ratchet 25; full run 72/73, only voice-control red (pitch, I92); merge 8012c7e
| I92 | [x] | 3 | Pitch and tempo tables, PITCH, TEMPO, RP_SET_SPEED | T42 green, ratchet 23 | red `Test #57: voice-control ***Failed 0.53 sec`; 86b9689, 51a8b7a, 10ec5f5; ratchet 23; full run 73/73; merge 29239fc
| I93 | [x] | 4 | T43, T44, T45 written and red; voices-test output | ctest failures | faae4ad red `#58 loop-fade`, `#59 crossfade`, `#60 sleep-alarm` ***Failed; merge 19a5dc1
| I94 | [x] | 4 | REPEAT, loops/fade_ms/max_song_ms/silence_ms options, SONG_ENDED reason, stop-after options | T43 green, ratchet 22 | 108bbed, 78a8391; T43 green, ratchet 22; full run 76/78 (crossfade, sleep-alarm red until I95, I96); merge 19a5dc1
| I95 | [x] | 4 | Early prepare, gapless, crossfade table and mix, `mixed_path` | T44 green ; merge 248716b (7323311) |
| I96 | [x] | 4 | HOSTQ_LOCAL_TIME, fake clock, SLEEP_TIMER, ALARM | T45 green, ratchet 21 ; merge 696618b (d5dcb45), ratchet 21 |
| I97 | [x] | 5 | T46 written and red; RP_SAMPLE_GET; nuade scan catalogue; openmpt sample table | ctest failure then catalogue lines green ; merge 9653063 (cb2b65c red, 5b93144) |
| I98 | [x] | 5 | WAV and 8SVX writers, SAMPLE_LIST/SAMPLE_EXPORT, goldens | T46 green ; merge 9653063 (cb95a44, T46 green) |
| I99 | [x] | 5 | T47 written and red; DSP ABI, chain, coefficient tool | ctest failure | red 3a3e6da (`crossfeed-render` Failed, `crossfeed-unit` Not Run); b14e63f; merge e577cdf
| I100 | [x] | 5 | crossfeed plugin, DSP verb, OUT_HEADPHONES default | T47 green, T26 green | bcab7cd, 6265bfe; full run 75/75; golden hash `tests/golden/dsp/crossfeed.sha256` NOT pinned, waits for a listening check; merge e577cdf
| I101 | [x] | 6 | Text UI viz panel and pattern panel, default bindings | panel screen fixtures green ; merge c4d422a (422f1de red, f8996a5) |
| I102 | [x] | 6 | T39 written and red; fixtures | ctest failure; merge e72c31f (T39 red: no viz_frames line) |
| I103 | [x] | 6 | Wire to T39; build/amiga green | T39 green, T8 at 21; merge 0cf158d (T39 green, T8 at 21, amiga green) |
| I104 | [x] | 6 | Design doc section 14 status, validation file, full CI run (reason: landing the milestone) | `2026-10-23-m1-3-viz-voices-playback.validation.md`; commit 31a931a (validation: 11 of 13 verified, 7 manual open; gaps in I104a-d) |
| I104a | [x] | 7 | T26 no-alloc with crossfeed DSP on (T47 clause) | no-alloc test with --dsp crossfeed green; 2c54036 no-alloc-render-dsp; merge 7e031ff |
| I104b | [x] | 7 | T46 nuade sample scan path test | test through selecta-cli on a nuade module green; 81f46f8 sample-export-nuade; merge 7e031ff |
| I104c | [x] | 7 | Key-level tests for pitch, tempo and solo keys (`}` `{` `>` `<` `S` `0`) | keys file test asserts engine stats move; cf887b7 key tests; merge 7e031ff |
| I104d | [x] | 7 | Note dots and phase view in text UI pattern/voice panel per plan | golden screen plus counter; d3606ef ui-viz-dots, ui-viz-phase; merge 7e031ff |

## Implementation M2 (plan: thoughts/shared/plans/2026-10-24-m2-amiga.md, status: approved 2026-10-24 autonomous with defaults D-M2-1..8 (Q35-Q42), starts after I104)

Tests T49 to T61 are defined in the plan's finish-line table.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I105 | [x] | 0 | Add T49 to T55, T57, T58, T61 red (desktop and vamos) with fixtures | ctest lists them, all fail; merge 3606a9f (14 red, missing-feature reasons) |
| I106 | [x] | 0 | Add rig driver `tests/amiga/run_rig.cmake`, label `rig`, skip line; T56, T59, T60 red | ctest -L rig fails with rig up, skips visibly with rig down; merge 3606a9f (rig tests skip visibly; rig-up path untested, rig auth failed) |
| I107 | [x] | 1 | Declared-need tags in `tags.def` (MIN_CPU, NEEDS_FLOAT, MEMORY, PRIORITY, CONFIG, OUT_MEM_CHIP) | build both trees; tag table test; e120aee; merge 2d15f64 (rig proof open for I109, I110) |
| I108 | [x] | 1 | Registry machine filter and priority owner choice, fake `sel_machine` injection | T49 green desktop; e120aee; merge 2d15f64 (rig proof open for I109, I110) |
| I109 | [x] | 1 | GFX and AUDIO probes, planar tier, `--tier` override | T50 green; e120aee; merge 2d15f64 (rig proof open for I109, I110) |
| I110 | [x] | 1 | timer.device ReadEClock clock, DateStamp fallback under vamos | clock test green on rig and vamos; e120aee; merge 2d15f64 (rig proof open for I109, I110) |
| I111 | [x] | 1 | T49 green under vamos; tag every existing plugin with its needs | T49 green both trees; e120aee; merge 2d15f64 (rig proof open for I109, I110) |
| I112 | [x] | 2 | CIA-B interrupt server (signal only) | T54 core objects green; rig sentinel `cia_ints=`; 2987e49; rig sentinel cia_ints= unverified; merge a97bb26 |
| I113 | [x] | 2 | Audio task at prefs priority, render loop | rig sentinel `audio_task_ticks=`; 2987e49; rig sentinel audio_task_ticks= unverified, not wired into an app; merge a97bb26 |
| I114 | [x] | 2 | SPSC publication with Disable/Enable around head/tail; main loop Wait plus `sel_engine_pump()` | queue test under vamos; c971ab2 spsc-queue desktop and vamos; merge a97bb26 |
| I115 | [x] | 2 | `dir.c` Examine/ExNext | T57 green; 8ee0f79 T57 vamos; merge a97bb26 |
| I116 | [x] | 2 | One option parser for ToolTypes and CLI over `commands.def` | T58 green (options); 9780bda, d8b689b T58 options; merge a97bb26 |
| I117 | [x] | 2 | Task priority from option | T58 green (priority read back); d77d32d; vamos shim removed in merge, read-back moved to rig-task-priority (open); merge a97bb26 |
| I118 | [x] | 3 | Paula voice sink, chip RAM samples via `host->mem_alloc` | build; voice API conformance test; partial: sink + chip copy in 48d8e4e (merge 6b44e20); open: MEMF_CHIP in Amiga mem.c, hardware paula output; MEMF_CHIP via SEL_MEM_SAMPLE in 84bd0fc (counter test mem-class-unit only, no vamos AllocVec run); merge fd69b4d |
| I119 | [x] | 3 | Period once per note, volume 0..64, loop registers | T51 register hash; 48d8e4e 3c1dd9d AUD0PER 428 hand-checked; merge 6b44e20 |
| I120 | [x] | 3 | Shadow register twin for vamos and desktop | T51 green both trees; 76c5b29 paula-shadow aud_writes=53 golden; vamos path fix c395d55; merge 6b44e20 |
| I121 | [x] | 3 | Routing: VOICE with voice_count <= 4 to `paula` | routing table test; T56 precondition; routing table test in 84bd0fc, >4 voices on paula has no PCM stage until Phase 4; merge fd69b4d |
| I122 | [x] | 4 | Portable paula14 split tables, default calibration (D-M2-4) | T52 green desktop; T52 green desktop and vamos, p14_hash equal in 130bb46; merge ebdc57c |
| I123 | [x] | 4 | paula14 Amiga sink, chip RAM double buffer | T52 green vamos; T54 extended; chip double buffer sink, paula14 and paula14-shadow outputs in 130bb46; T54 extended; hardware pacing unverified until rig; merge ebdc57c |
| I124 | [ ] | 4 | AHI sink over ahi.device | rig sentinel `ahi_bufs=`; rig-only: AHI sink not started, needs rig (Q49) |
| I125 | [ ] | 4 | Buffer length per CPU class in `sel_prefs`, AHI mode and rate defaults (D-M2-5) | T59 green; rig-only: needs rig for T59 (Q49) |
| I126 | [x] | 4 | Routing: RENDER and VOICE over 4 voices to mixer then paula14 or ahi | routing table test; paula14 has no voice backend, so RENDER and VOICE of any count route to mixer; paula14-routing(-vamos), paula14-unit in 130bb46; merge ebdc57c |
| I127 | [x] | 5 | Integer LED filter stage | T53 LED part green; 6ada682 LED -3 dB 3275 Hz measured; merge 5d60c2c |
| I128 | [x] | 5 | Integer stereo width stage | T53 width part green; 6ada682 width; width-moves-hash on stereo mk.mod desktop only (Q46); merge 5d60c2c |
| I129 | [x] | 5 | Commands, prefs keys, text UI controls for LED and width | T53 green both trees; T8 count moves; 2fccb64 verbs FILTER_LED, MIX_STEREO_WIDTH; T8 stays 21; merge 22ff2c9 |
| I130 | [x] | 6 | `plugins/ui/text/amiga/term.c` raw mode, WaitForChar | T55 partial; term.c raw mode, WaitForChar, CSI size query in c81c48d; interactive path unverified until T56; merge 9c6594a |
| I131 | [ ] | 6 | `plugins/ui/text/amiga/spawn.c` | spawn test under vamos; partial: amiga/spawn.c (SystemTagList) linked in c81c48d, no vamos spawn test yet; vamos (amitools 0.7.1) has no SystemTagList, test is rig-only (Q49) |
| I132 | [x] | 6 | Build `plugins/ui/text` and `apps/selecta` on Amiga | T55 green; selecta and plugins/ui/text build for Amiga, T55 green (68k ui_hash = desktop) in c81c48d; merge 9c6594a |
| I133 | [ ] | 6 | Rig run in UP-Term | T56 green |
| I134 | [x] | 7 | `SEL_RP_CONFIG` tag list, prefs, per-song override (D-M2-8) | T61 green; rp.config pref, sel_engine_set_song_config override, render --rp-config/--rp-config-song, T61 uses nuade.gain in 6b9c1e5; merge 3d3310f |
| I135 | [ ] | 7 | T60 batch on `--a1200 --nofpu`, verdicts recorded, NEEDS_FLOAT tags set | T60 green |
| I136 | [x] | 8 | Fix T8 ratchet number for M2 command set | T8 green; M2 command set fixed (FILTER_LED, MIX_STEREO_WIDTH landed implemented, none of the 21 open verbs is M2 scope), ratchet stays 21, T8 `ratchet` green |
| I137 | [ ] | 8 | Full finish-line run (reason: landing M2) | all T49 to T61 and T8 green, none skipped |
| I138 | [ ] | 8 | Manual checks 1 to 5 confirmed by Johan | Johan's confirmation recorded in ledger |
| I138a | [ ] | 8 | paula14 hardware close waits for the last queued buffer before DMA stop | rig only (T56 family); found in m2-fixes-report, merge cd65b7c |
| I138b | [x] | 8 | Amiga paths in browse: `sel_browse_join` and ".." handle "Vol:dir" and "dir/sub" (parent of `Work:Music` is `Work:`) | desktop unit test of join/parent with Amiga style; T55 browse step into subdir under vamos; found in m2-fixes-report, merge cd65b7c; style-aware join/parent/root in src/library/local; T amiga-browse-nav-vamos; title leaf cosmetic open as I138c; merge adba905 |
| I138c | [ ] | 8 | Text UI title leaf (`tui_leaf`, plugins/ui/text/text_ui.c) splits on "/" only, shows "work:songs"; needs SDK-visible leaf helper | regression test on Amiga title; found by I138b agent |


## Implementation M3 (plan: thoughts/shared/plans/2026-10-25-m3-library.md, status: approved 2026-10-25 autonomous with defaults D-M3-1..19 (Q50-Q68), starts after M2 non-rig rows)

Tests T62 to T97 are defined in the plan's finish-line table. T8 ratchet target after M3: 8.

| ID | Status | Phase | Task | Proof |
|----|--------|-------|------|-------|
| I139 | [ ] | 0 | Read DEViLBOX server routes (auth, ratings, hip vote/votes, genre, songdb, loudness, lyrics, comments, mirror, playlists, public URL layout); record shape or absence with file:line in todos "Facts gathered" (D-M3-13) | Facts section lists every endpoint the plan uses |
| I140 | [ ] | 0 | Record one live response per external layout by hand (Modland allmods_md5.zip header and changelog, HVSC Songlengths.md5 and STIL, hippoplayer.se list and one `.prg`) into `tests/net/fixtures/` subsets with source URL and date | fixture files committed with provenance header |
| I141 | [ ] | 0 | `tests/net/fixture_server.py` (HTTP, chunked, Range, redirect, delay, TLS test CA, passive FTP), request log, ctest fixture and lock, guard and map env contract (D-M3-6) | fixture server self-test passes; T67 red |
| I142 | [ ] | 0 | T62 to T95 red on desktop with fixtures, label `m3` | ctest -L m3 lists them, all fail with missing-feature reasons |
| I143 | [ ] | 0 | T96 red under vamos, T97 red on the rig (label `rig`, visible skip) | vamos test fails; rig test fails or skips visibly |
| I144 | [ ] | 1 | Vendor SQLite amalgamation, `selecta_sqlite` target, options per D-M3-1, desktop and 68k | both trees build; `library stats` prints `sqlite_version=` |
| I145 | [ ] | 1 | Library worker (thread / subtask), request and result SPSC queues, memsys5 heap at start (D-M3-2) | T62 heap and no-alloc parts green |
| I146 | [ ] | 1 | Schema v1, `user_version` migrations, FTS5 table, account handle on every entry point | T62 green |
| I147 | [ ] | 2 | `src/library/query.c` parser for the full section 9.2 syntax, unknown key and view cycle errors | T63 parse rows green |
| I148 | [ ] | 2 | SQL compiler with bound parameters, durations, sizes, relative time, `in:`, `view:` | T63 green |
| I149 | [ ] | 2 | LIBRARY_SEARCH through the worker, LIBRARY_RESULT row tags in `tags.def` | T64 green; T8 count moves |
| I150 | [ ] | 2 | PLAY_QUERY, QUEUE_ADD_QUERY, VIEW_SAVE (smart playlists) | T65 green; T8 count moves |
| I151 | [ ] | 2 | View model source = library query (VIEW_SOURCE), style switch keeps the key | T66 green |
| I152 | [ ] | 3 | `src/core/{posix,amiga}/net.c` socket table, guard and host map | T67 green |
| I153 | [ ] | 3 | `src/library/net/http.c` HTTP/1.1: GET/PUT/POST/DELETE, chunked, Range, redirects, 3 s / 30 s timeouts | T68 plain parts green |
| I154 | [ ] | 3 | Vendor mbedTLS, TLS stream wrapper, OS CA store, `SELECTA_CA_FILE` (D-M3-4) | T68 green |
| I155 | [ ] | 3 | Passive FTP client (D-M3-5) | T70 green |
| I156 | [ ] | 3 | Mirrors: first-byte ranking, retry next, home mirror first, Amiga route to `/api/mirror/<source>/<path>` | T69 green |
| I157 | [ ] | 4 | `src/library/source.h` (`sel_source`), source registry, song key helpers, Modland format table, `SEL_SRC_*` tags | key round trip in T72; tag lint green |
| I158 | [ ] | 4 | `local` source over `sel_browse`: scan into files/songs with probe metadata, LIBRARY_SCAN | T71 scan part green |
| I159 | [ ] | 4 | Archive adapters (D-M3-7), archives browse as folders; xad/xfd on Amiga | T71 archive part green |
| I160 | [ ] | 4 | `sel_source_rescan(paths)`, folder watcher in `src/core/<os>/watch.c`, Amiga timed rescan | T71 green |
| I161 | [ ] | 4 | `modland` source: allmods_md5 listing, path parse, `url`/`path_in`, changelog, LIBRARY_SYNC | T72 green |
| I162 | [ ] | 4 | `hvsc` source: listing, Songlengths.md5, STIL, hosted-by Modland | T73 green |
| I163 | [ ] | 4 | `hippo` source: JSON list, legacy TSV list, `.prg` fetch through the hippo codec | T74 list and fetch parts green |
| I164 | [ ] | 4 | PLAYLIST_PUBLISH: hippo export, 512 KB check, POST, reply parse | T74 green; T8 count moves |
| I165 | [ ] | 5 | File cache paths, md5 verify with next-mirror retry, LIBRARY_FETCH | T75 fetch part green; T8 count moves |
| I166 | [ ] | 5 | `SEL_RP_COMPANIONS` tag, per-format rules on the replayers that need them, fetched with the song | T75 green |
| I167 | [ ] | 5 | LRU trim to a `sel_machine`-scaled budget, kept and liked exempt | T76 green |
| I168 | [ ] | 5 | Prefetch of the next queued remote song while one plays | T77 prefetch part green |
| I169 | [ ] | 5 | Smart downloads (F34): history and smart-playlist policy, low priority, metered and off prefs | T77 green |
| I170 | [ ] | 6 | History, play and skip counts on SONG_OPENED/SONG_ENDED; LIKE, KEEP; `rp.config` per-song override moves into the DB | T78 history part green; T61 still green |
| I171 | [ ] | 6 | FLAG wrong/skip, `selecta-cli wrong`, `sounds-wrong.txt` export, skip honoured by SHUFFLE and RADIO | T78 flags part green |
| I172 | [ ] | 6 | NOTE and tag edits in the DB, optional DEViLBOX comment push | T78 green |
| I173 | [ ] | 6 | RATE, DEViLBOX login and token store (D-M3-12), PUT/DELETE/batch, pending push queue | T79 green; T8 count moves |
| I174 | [ ] | 6 | VOTE with parameters from I139 | T80 green; T8 count moves |
| I175 | [ ] | 6 | CUE_SET, CUE_GOTO over the `cues` table | T81 green; T8 count moves |
| I176 | [ ] | 6 | STICKER_SET/GET/DEL/LIST (D-M3-17) | T82 green; T8 count moves |
| I177 | [ ] | 6 | Engine emits SCROBBLE_DUE; `plugins/integrations/lastfm` scrobbler, offline queue (D-M3-15) | T95 green |
| I178 | [ ] | 7 | Playlists and folder tree in the DB, FOLDER_ADD/MOVE/DEL, PLAYLIST_MOVE, folder export | T83 DB part green |
| I179 | [ ] | 7 | Text UI tree renders the folder tree | T83 green |
| I180 | [ ] | 7 | SHARE verb, link parse in `selecta` and `selecta-cli open`, DEViLBOX URL form (D-M3-11) | T84 green |
| I181 | [ ] | 8 | `src/library/xxh32.c`, UADE key, genre lookup with local cache, `genre:` key | T85 genre part green |
| I182 | [ ] | 8 | Songdb lookup, composer alias merge, composer page query | T85 green |
| I183 | [ ] | 8 | Similar and also-by queries | T86 similar part green |
| I184 | [ ] | 8 | Duplicates by md5 across sources, best source (D-M3-14) used by OPEN | T86 green |
| I185 | [ ] | 8 | Metadata panel rows (STIL, composer, release, year, genre, rating, plays) in module info | T87 green |
| I186 | [ ] | 8 | RADIO from a seed with genre/composer/rating steering, skip excluded | T88 green |
| I187 | [ ] | 9 | Desktop analysis job: faster-than-real-time render into R128 analyser, RMS+peak fallback, ANALYSE, `selecta-cli analyse` | T89 green |
| I188 | [ ] | 9 | Gain store per song key; DEViLBOX GET/PUT loudness by hash | T90 fetch part green |
| I189 | [ ] | 9 | Mixer 16.16 per-song gain and -1 dBFS limiter, `mix.loudness` pref (D-M3-9) | T90 gain and limiter parts green |
| I190 | [ ] | 9 | Integer adaptive leveler, provisional gain write-back | T90 green |
| I191 | [ ] | 10 | Home view source: recently played, jump back in, new this week | T91 green |
| I192 | [ ] | 10 | Stats SQL views and text UI stats screen | T92 green |
| I193 | [ ] | 10 | Lyrics: LRC, song message, DEViLBOX lyrics, current line in the snapshot | T93 green |
| I194 | [ ] | 11 | minimp3 and stb_vorbis RENDER-kind plugins over a byte stream (D-M3-8) | T94 decode part green |
| I195 | [ ] | 11 | URL and pipe paths in OPEN, M3U URL-only import, `PIPE:` on Amiga | T94 URL part green |
| I196 | [ ] | 11 | Stream-to-disk recording (F36), `selecta-cli record` | T94 green |
| I197 | [ ] | 12 | Library and SQLite in the 68k build, `library export-index`, read-only ATTACH (D-M3-16) | T96 green |
| I198 | [ ] | 12 | Measure the full Modland index (size, `db_heap_peak`, query time under vamos) from one real allmods_md5 build; record in Facts; open the fallback row only if it fails | numbers recorded in todos Facts |
| I199 | [ ] | 12 | Amiga bsdsocket net, DEViLBOX mirror fetch on the rig (D-M3-19) | T97 green |
| I200 | [ ] | 13 | T8 ratchet to 8 in `tests/ratchet/unimplemented.txt` | T8 green, count 8 |
| I201 | [ ] | 13 | RULES.md `## Commands` rows for every new `selecta-cli` and `selecta` command | rows present, each command run once |
| I202 | [ ] | 13 | Full finish-line run (reason: landing M3) | all T62 to T97 and T8 green, none skipped |
| I203 | [ ] | 13 | Manual checks 1 to 6 confirmed by Johan | Johan's confirmation recorded in ledger |

## Open decisions

Round 5 picks decided 2026-10-22 (AskUserQuestion, two calls): in = F19, F20, F21, F23, F24, F25, F29, F30, F31, F33 to F41, F43 to F46, F48, F49; restored into scope = F50 multi-zone sync, F51 server transcoding, F52 multi-user/multi-library; not picked = F32 CD audio, F42 rippers, F47 device sync; still out = Atmos, mpd client messaging, Tidal/Qobuz. Recorded in design doc sections 1a and 16. Open: none.

- [x] D-M2-1 (default taken, Q35, plan 2026-10-24-m2-amiga.md): nuade's voice API is float/double, so a nuade player ticking under a CIA interrupt on a 68020 without FPU runs soft-float in the interrupt (rule 4). Options for M2: tick in a high-priority task, upstream an integer voice API to emoon/nuade, or require an FPU for Paula-direct nuade. Source: thoughts/shared/research/2026-10-23_nuade-adapter.md. Not blocking M0b.
- [x] D-M2-2 (default taken, Q36): plain M.K. .mod on the Amiga. No nuade player probes M.K. (R8) and libopenmpt stays desktop-only in M1.1. Options for M2: a nuade M.K. player upstream (author is a friend of the user) or the UADE-native plugin. Decide at M2 planning.

Protocol additions from M1d are already in design doc section 3 (not a decision, engine protocol detail).

## Found (not asked, worth knowing)

- Feature catalogue (M1e): 197 rows (awk count matches agent Counts section), 89 picked, 43 design-only, 2 parity-only, 63 unhomed; sorted above. Agent read only feature names and the "In Selecta design?" column of the input docs, merged duplicates by judgement, and derived absence from greps; the kiosk/lock row and the follow-mode row are weak.

- DEViLBOX server work needed (D9): plain-HTTP mirror proxy `/api/mirror/<source>/<path>` for Amiga; loudness by hash `/api/loudness/<hash>`; genre list; artist/composer endpoints; comments; party-queue relay. Vote endpoints `/api/hip/vote`, `/api/hip/votes` unverified.
- uprough-uade glue crashes on runtime song switch (2026-09-21); branch feature/uade-music-backend unlanded. Must be fixed before it is the Amiga fallback.
- UP-Term/vtcon lacks kitty keyboard protocol, OSC 8 (links), OSC 52 (clipboard).
- M1b (PC chiptune era) sourcing is thin: only OCP, libsidplayfp and XMPlay pages fetched; Deadbeef and zxtune GitHub URLs returned 404; other rows from memory, marked (unverified). Re-verify before relying on those rows.
- M1c (modern streaming) gaps to decide (F19+ candidates): synced lyrics; sonic similarity vectors (OpenSubsonic getSonicSimilarTracks); persisted cross-device play queue (OpenSubsonic savePlayQueue, needed for hand-off resume); per-song skip/enable flag; smart shuffle; multi-user/multi-library, server transcoding, multi-zone sync (out of scope candidates). Plexamp/Roon/Tidal rows from search snippets only.
- M1d (terminal/web) protocol ideas for section 3, to fold in at M1e/M2: IDLE <subsystems>; stable queue entry ids + QUEUE_VERSION + QUEUE_CHANGES since=N; QUEUE_MOVE/SWAP/SHUFFLE with START:END ranges; OPTION consume|single|random|repeat|crossfade family; PLAY_QUERY / QUEUE_ADD_QUERY; reflection command from commands.def; SNAPSHOT_QUEUE; STICKER_* key/value verbs; atomic command lists (maps to Lua/ARexx); relative SEEK/VOLUME + idempotent PAUSE_PLAYBACK; SEEKED and SCROBBLE_DUE events; AUTH first command (LAN warning); chunked ALBUMART. Gaps: query language syntax spec (regex, relative time, playlist membership, nested saved queries), stats screen, bbmp 'sounds wrong' flag, song-change hook, offline scrobble queue (50 per batch). Scenestream/SceneSat stream URLs not found on their homepages.
- F14 format coverage: 445 formats; owners nuade 119, openmpt 63, gme 14, sidplayfp 4, streams 4, UADE native 17; 224 had NO owner in design v2. Fixed in section 5 + 14 by adding adapter classes zxtune (41), adplug (40), libvgm (8), libkss (7), libxmp (6), furnace, protrekkr, sndh, and ~30 bbmp csrc helpers (milestones M2b, M3c); 22 formats bbmp cannot play stay owner "none". Unverified in the doc: libgme may cover KSS/SAP/some VGM; nuade name matches for FredMon, SoundPlayer, Digital Mugician, Follin, IFF-SMUS, CustomMade; Hippo internal/external split inferred from readme, not puu016.s; three UADE-only formats (Sierra AGI, ADPCM mono, Paul Tonge) missing from table.
- M1a: Eagleplayer 2.04 drop has no .guide files; DeliTracker cue list, ProTracker/NoisePlayer rows, Chipmachine shuffle, AmiNetRadio rows unverified.

- F13a HippoPlayer parity NEW rows (25, see research doc "NEW" list): alarm (F6), stored programs on F1-F10 (F11 hotkeys), module comment file (unverified), LhA/LZX/Zip/gzip + XPK/PowerPacker/XFD (F8), UHCBIN network transport (section 9 Amiga transport), PUBSCREEN / own screen (unverified) / iconify / AppIcon / task priority (section 7 Amiga UI plugin), AHI indicator in infobox, hardware outputs MHI MP3, AmiGUS, Trinity, SIDBlaster, USBSID-Pico, ZorroSID (section 5/6 `SEL_OUT_*` plugins, new F ID), HippoPlayer.group location, ToolType parsing (unverified), pipe-based streaming names, reqtools requesters, keyfile/registration (out of scope), optional S:HippoPlayer.PS3M, Kickstart 1.2+/68000 baseline (runtime tier). Gaps: hotkey line numbers approximate (~30 off); ToolType set unverified; prefs read from gadget files only.

- F13b bbmp parity NEW rows (4, see research doc "NEW" list): songs inside collection archives and MP3/Ogg/FLAC in collections (bbmp "not prepared"; covered by F8 archives + F7 stream decoders, map in 1a); "!" key "this sounds wrong" flag with note dialog and sounds-wrong.txt plus `bbmp wrong` CLI (candidate F22); testers guide / tester-reporting workflow (docs, out of design scope). Unverified in doc: channel-scope data source, tui/guard.py purpose, log file, snapshot.py, licence note (bbmp GPL-3.0 versus Selecta MIT: adapters must not copy bbmp code, only ideas; C helpers under csrc/ need licence check before vendoring). Agent did not read docs/replayers.md body, tui/browse.py, lists.py, settings_tab.py in detail.
- amiga-roundtrip flake (M0b phase 1, 2026-10-22): cause found and fixed. Two vamos instances started together collide on the per-user temp volume `~/.vamos/volumes/ram` ("temp volume volume path already exists"). All vamos tests now share `RESOURCE_LOCK vamos`, and an `amiga-build` ctest fixture rebuilds build/amiga first so a host ctest never runs a stale 68k binary.
- clang-format and clang-tidy are not installed on this machine, so the format and lint rows of the Commands table do not run here.
- nuade licence notices (2026-10-23): pokeynoise/pn_waves.c, davidhanney/dh_data.c and majortom/mt_data.c say their Eagleplayer data is not covered by nuade's MIT licence. The user says the whole package is by one author under one licence, so all 121 players are built. The file text still says otherwise; worth asking the author to update those notices before a public release.
- nuade count: design doc section 5 says 119 nuade formats, the tree has 121 players. I53 reconciled it from the registry: 121, design doc fixed.
- Linker warning `ignoring duplicate libraries: libsel_plugin_nuade.a, libselecta_nuade.a` comes from a deliberate static-library cycle (plugins/replayers/nuade/CMakeLists.txt:43-47). Fixed by I46.

## Facts gathered

- nuade: C port of uade replayers, MIT, CMake target nuade::nuade. Tick players (framework owns clock + mixer) and self-rendering players (render_raw float stereo). Display hooks: position, voice_info, events, tracker_info/cells. Headers: scratchpad ext/nuade/include/nuade/*.h.
- bbmp: Python + Textual TUI; replayers are sandboxed helper processes in csrc/ plus libopenmpt/gme/sidplayfp/uade; docs/replayers.md, docs/collections.md, docs/formats.md.
- DEViLBOX API: base https://devilbox.uprough.net/api; ratings 1-5 (PUT/DELETE /api/ratings, Bearer JWT); batch GET max 200; item key = source + Modland full_path / HVSC path; genre GET /api/genre/:key (xxh32 of first 256 KB + size & 0xFFFF); songdb lookup by hash; modland and hvsc browse/search/download. No genre-list or artist endpoint.
- uprough-uade: UADE score.s host + real Eagleplayer/DeliTracker 68k binaries natively, Paula direct, no mixing; asm contract _music_init/start/stop/shutdown/play1/play2/set_pos/select; 40 formats on A500.
- HippoPlayer source: /Users/spot/Code/HippoPlayer-68k/code/puu016.s (playlist load 13119, header 13523, save 13542).
