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
| L4 | [ ] | Write the M1.2 and M1.3 plans after I62 | thoughts/shared/plans/ |

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

## Implementation M1.1 (plan: thoughts/shared/plans/2026-10-23-m1-1-text-ui-queue.md, status: approved 2026-10-23)

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
| I56 | [ ] | 3 | RP_SEEK in engine; T21 openmpt half; T26 extended | T21, T26 green |
| I57 | [x] | 4 | T24 red; selecta_library, dir.c, LIBRARY_BROWSE, LIBRARY_RESULT | ratchet 46 |
| I58 | [ ] | 4 | view.c; VIEW_SHOW/CURSOR/ACTIVATE/WINDOW born implemented; VIEW_STYLE regular; VIEW_CHANGED, VIEW_ROWS | T24 green, ratchet 45 |
| I59 | [ ] | 5 | T25 red; grid.c, keys.c, theme.c, posix/term.c | T25 green |
| I60 | [ ] | 5 | text_ui.c; apps/selecta shell; T19 red then green | T19 green |
| I61 | [ ] | 5 | real-terminal check on macOS (plan Verification, manual 1); only the user ticks it | user confirms |
| I62 | [ ] | 6 | RULES.md Commands, README, design doc sections 3/5/14/16, validation doc, plan status done | counts reconciled |

## Open decisions

Round 5 picks decided 2026-10-22 (AskUserQuestion, two calls): in = F19, F20, F21, F23, F24, F25, F29, F30, F31, F33 to F41, F43 to F46, F48, F49; restored into scope = F50 multi-zone sync, F51 server transcoding, F52 multi-user/multi-library; not picked = F32 CD audio, F42 rippers, F47 device sync; still out = Atmos, mpd client messaging, Tidal/Qobuz. Recorded in design doc sections 1a and 16. Open: none.

- [ ] D-M2-1: nuade's voice API is float/double, so a nuade player ticking under a CIA interrupt on a 68020 without FPU runs soft-float in the interrupt (rule 4). Options for M2: tick in a high-priority task, upstream an integer voice API to emoon/nuade, or require an FPU for Paula-direct nuade. Source: thoughts/shared/research/2026-10-23_nuade-adapter.md. Not blocking M0b.
- [ ] D-M2-2: plain M.K. .mod on the Amiga. No nuade player probes M.K. (R8) and libopenmpt stays desktop-only in M1.1. Options for M2: a nuade M.K. player upstream (author is a friend of the user) or the UADE-native plugin. Decide at M2 planning.

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
