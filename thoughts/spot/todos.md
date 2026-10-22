# Selecta ledger

Every task lives here with an ID. Each reply names IDs ticked since the last reply and reports `done/total`. Nothing is started that is not here; nothing is reported done that is not ticked. Plans: `thoughts/shared/plans/` (status in each file's frontmatter; see RULES.md rule 0).

Session 2026-10-22 (1). Finish line: design doc complete (sources, feature bar, tiers, view styles, playlist model, tag ABI, decisions), research docs written, ledger restructured, first commit made, next-session prompt written. Done.

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

## Repo

| ID | Status | Task | Artifact |
|----|--------|------|----------|
| L1 | [x] | Ledger restructured; RULES.md ledger rule | this file, RULES.md |
| L2 | [x] | Memories: structured-ledger-with-ids, design-not-capped-by-amiga, feature-menus, parity | ~/.claude/projects/-Users-spot-Code-MusicPlayer/memory/ |
| R6 | [x] | First commit, named files, no attribution | git log |
| R7 | [x] | next-session.md start prompt | thoughts/spot/next-session.md |

## Implementation (plan: thoughts/shared/plans/2026-10-22-m0-skeleton.md, status: in-progress)

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

## Open decisions

Round 5 picks decided 2026-10-22 (AskUserQuestion, two calls): in = F19, F20, F21, F23, F24, F25, F29, F30, F31, F33 to F41, F43 to F46, F48, F49; restored into scope = F50 multi-zone sync, F51 server transcoding, F52 multi-user/multi-library; not picked = F32 CD audio, F42 rippers, F47 device sync; still out = Atmos, mpd client messaging, Tidal/Qobuz. Recorded in design doc sections 1a and 16. Open: none.

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

## Facts gathered

- nuade: C port of uade replayers, MIT, CMake target nuade::nuade. Tick players (framework owns clock + mixer) and self-rendering players (render_raw float stereo). Display hooks: position, voice_info, events, tracker_info/cells. Headers: scratchpad ext/nuade/include/nuade/*.h.
- bbmp: Python + Textual TUI; replayers are sandboxed helper processes in csrc/ plus libopenmpt/gme/sidplayfp/uade; docs/replayers.md, docs/collections.md, docs/formats.md.
- DEViLBOX API: base https://devilbox.uprough.net/api; ratings 1-5 (PUT/DELETE /api/ratings, Bearer JWT); batch GET max 200; item key = source + Modland full_path / HVSC path; genre GET /api/genre/:key (xxh32 of first 256 KB + size & 0xFFFF); songdb lookup by hash; modland and hvsc browse/search/download. No genre-list or artist endpoint.
- uprough-uade: UADE score.s host + real Eagleplayer/DeliTracker 68k binaries natively, Paula direct, no mixing; asm contract _music_init/start/stop/shutdown/play1/play2/set_pos/select; 40 formats on A500.
- HippoPlayer source: /Users/spot/Code/HippoPlayer-68k/code/puu016.s (playlist load 13119, header 13523, save 13542).
