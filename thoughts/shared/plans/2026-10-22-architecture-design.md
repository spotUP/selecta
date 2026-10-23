---
date: 2026-10-22
topic: Selecta, multi-format music player, architecture design (draft v2)
tags: [design, architecture, amiga, portable-c, plugins, nuade, eagleplayer, sources, playlists, tags]
status: draft
---

# Selecta: architecture design

Name: **Selecta**. Code prefix `sel_`, header dir `include/selecta/`, binary `selecta`, CLI `selecta-cli`. Decided 2026-10-22 together with: Amiga baseline build 68020 + 2 MB (stock A1200, AGA/ECS), toolchain bebbo amiga-gcc + CMake (vasm for the native UADE glue), license MIT. The baseline is a build target, not a design ceiling (section 12).

Inputs: research docs in `thoughts/shared/research/2026-10-22_*.md` (Eagleplayer 2.04, DEViLBOX API + UpRough UADE, bbmp architecture, bbmp sources, UP-Term capabilities, playlist formats + hippoplayer.se, feature research per era, parity inventories, format coverage), nuade headers (`include/nuade/{nuade,player,voice,paula,viz}.h`).

## 1. Goals and non-goals

Goals
- A desktop-class player first: the feature bar is set by Spotify, Apple Music, foobar2000 and Open Cubic Player, not by what a stock A1200 can draw. The Amiga is one target tier; every feature has a desktop implementation and scales down or hides itself at run time (section 12, `sel_machine`).
- One engine, portable C, builds for AmigaOS 3.x (68k), Linux, macOS, Windows.
- Engine is headless and is the single source of truth. UIs render engine state and send commands; they hold no playback logic.
- UIs are plugins, switchable at run time without stopping playback: text mode, GadTools, MUI, an SDL2 desktop GUI, remote clients.
- Replayers, audio outputs, visualizers, DSP, converters and UIs are plugins with a published C SDK and a tag-list ABI that extends without breaking (section 10).
- Amiga: every replayer reaches Paula and AHI. Voice-driven replayers reach Paula with zero mixing. Native 68k UADE/Eagleplayer players run as hardware-exclusive plugins through the UpRough UADE glue.
- Library: every music source bbmp supports plus local files and streaming radio; browse by source, format, artist, album, genre; rate and vote through the DEViLBOX API.
- Parity: everything HippoPlayer and bbmp do is in scope for v1 (section 1a, parity column).
- Formats: the union of what HippoPlayer and bbmp play has a Selecta plugin owner (section 5).
- No duplicated logic. One definition of commands/events, one registry, one voice abstraction, one metadata model, one view model.

Non-goals (v1)
- Editing songs. Sample export is kept (nuade has it) but no editor.
- Multi-process GUI on Amiga. In-process plugin UIs give on-the-fly switching; a remote protocol covers out-of-process clients.
- Re-implementing replayers that exist as C libraries (libopenmpt, libgme, libsidplayfp, nuade). Adapt, never fork.
- Two-deck DJ mode (crossfader, beat-synced decks). Cue points, BPM and party queue are in; a second deck is not (decision 2026-10-22).

## 1a. Feature bar

Every row is a feature Johan picked on 2026-10-22 (AskUserQuestion rounds 1 to 4) or that parity with HippoPlayer / bbmp requires. Columns: component that owns it, lowest tier that can run it (text / planar / RTG / GPU, section 12), milestone (section 14), parity (H = HippoPlayer has it, B = bbmp has it). Parity rows F26 to F28 come from the parity inventories (`2026-10-22_parity-hippoplayer.md`, `2026-10-22_parity-bbmp.md`); every other parity row is mapped to an existing feature in those docs. Rows from the feature catalogue (`2026-10-22_feature-catalogue.md`) are added from its "Unhomed" list; a row with no home gets a new ledger ID, never silently dropped.

| Area | Feature | Component | Lowest tier | Milestone | Parity |
|------|---------|-----------|-------------|-----------|--------|
| Playback | Crossfade + gapless (two replayer instances, RAM-scaled) | engine | text | M1 | B |
| Playback | Loudness normalisation (section 6a) | engine mixer + library + DEViLBOX | text | M3 | |
| Playback | Loop detect, auto-stop, fade (Songlengths + Hippo-style detection) | engine | text | M1 | H B |
| Playback | Channel mute / solo; pitch and tempo where the replayer allows | engine | text | M1 | H |
| Playback | Subsongs, random, repeat modes, song length | engine | text | M0 | H B |
| Library | Typed search filters (`format:`, `artist:`, `year:`, `source:`, `rating:`), one query language | library | text | M3 | B |
| Library | Smart playlists = saved queries | library | text | M3 | |
| Library | Home view: recently played, jump back in, Modland changelog "new this week" | library + view model | text | M3 | |
| Library | Ratings synced to DEViLBOX, liked list, history, scrobbling (desktop) | library | text | M3 | B |
| Library | All bbmp sources: modland, hvsc, asma, sndh, zxart, keygenmusic; local files; hippoplayer.se playlists | library sources | text | M3 / M3b | H B |
| Library | Favourites, kept songs, cache budget, prefetch next song, companions | library cache | text | M3 | B |
| Visual | Pattern scroller + sample/instrument list with click-to-play | viz + replayer hooks | text | M1 | H |
| Visual | Per-voice scopes, spectrum (FFT only with fast float, integer bands otherwise), VU | viz | text | M1 | H B |
| Visual | Chip register panels (SID, AY, YM, Paula) via replayer tags | viz | text | M4 | |
| Visual | GPU visualizers (projectM class) + generated artwork per format/chip | viz | GPU | M5 | |
| Visual | Switchable view styles: regular, tree, drilldown, cover flow, grid, compact (section 9b) | view model + UI | text (cover flow/grid: RTG) | M1 | |
| Remote | Device hand-off (Connect style) | protocol | text | M5 | |
| Remote | Web remote served by the engine | protocol | text | M5 | |
| Remote | Collaborative lists: hippoplayer.se publish, DEViLBOX votes and comments inline | library | text | M3 | H |
| Remote | Share links `selecta://` + devilbox URLs (song, playlist, position) | library | text | M3 | |
| Chip | SID 6581/8580 + filter per song | replayer tags | text | M5 | B |
| Chip | Paula LED filter emulation + stereo width | mixer | text | M2 | H |
| Chip | Resampler choice none/linear/cubic/sinc by tier | mixer | text | M0 | |
| Chip | HVSC / STIL / composer / release metadata panel | library + UI | text | M3 | B |
| Output | DSP chain plugins (EQ, reverb, stereo expander, limiter) | DSP plugin kind | text | M5 | |
| Output | Record to WAV/FLAC faster than realtime (same path as loudness analysis and golden tests) | engine + wav output | text | M0 | |
| Output | Network outputs: Chromecast, AirPlay, UPnP (desktop) | output plugins | text | M5 | |
| Output | Sleep timer + alarm, engine side | engine | text | M1 | H |
| Playlist | Hippo dividers + stealth entries round-trip; M3U, PLS, XSPF | playlist codecs | text | M1 | H |
| Playlist | Radio across collections (chipmachine-style shuffle, DEViLBOX genre/ratings steering, "start radio from this song") | library | text | M3 | B |
| Playlist | Playlist folders tree | library | text | M3 | |
| Playlist | Streaming radio stations (Nectarine, SceneSat) as a source; mp3/ogg stream decoders as replayer plugins | sources + replayers | text | M5 | H |
| Files | Archive decrunch (lha, lzx, zip, xpk, gzip; xfd/xad on Amiga, built in on desktop); archives browse as folders | library archives | text | M3 | H B |
| Files | Companion fetch (sample sets, multi-file formats) | library cache | text | M3 | B |
| Files | Module converters (converter plugin kind, Eagleplayer heritage) | converter plugins | text | M5 | |
| Files | OS file integration: drag-drop, AppWindow/AppIcon, open-with | UI plugins per OS | planar | M4 | H |
| DJ/live | Cue points + bookmarks per song (pattern/row or seconds, shareable) | engine + library | text | M3 | |
| DJ/live | Party mode voting queue via web remote | protocol + library | text | M5 | |
| DJ/live | BPM detect + tempo match (tracker tempo where known, analysis on desktop) | engine analysis | text | M5 | |
| Metadata | Composer pages across sources (author normalisation via DEViLBOX songdb) | library | text | M3 | B |
| Metadata | Similar and also-by (DEViLBOX genre + ratings; radio seeds) | library | text | M3 | |
| Metadata | Duplicate detection by md5 across sources, best-source choice | library | text | M3 | B |
| Metadata | Local notes + tag edits, optional push to DEViLBOX | library | text | M3 | B |
| Tracker | Module info panel (format, channels, patterns, instruments, size, length, message) | replayer tags + UI | text | M1 | H B |
| Tracker | Seek by pattern / order position | engine | text | M1 | H B |
| Tracker | Sample export to WAV / 8SVX | engine | text | M1 | |
| Tracker | Global hotkeys + user key bindings in every UI | shell | text | M1 | H |
| Integration | MPRIS / macOS Now Playing / Windows SMTC plugin per OS | integration plugins | text | M5 | |
| Integration | `selecta-cli` protocol client (scripting and tests) | CLI | text | M0 | B |
| Integration | HTTP/JSON API + MCP server over the same protocol | protocol | text | M5 | |
| Integration | Discord rich presence plugin | integration plugins | text | M5 | |
| Integration | Lua runtime + ARexx port mapping 1:1 to commands (section 16, D7) | scripting plugin | text | M4 | H |
| Theming | Design tokens per UI + bitmap skin loader in the SDL UI (D8) | UI plugins | text | M5 | |
| Chip / format | Per-replayer and per-song config (UADE song.conf, Eagleplayer EP_LoadPlConfig heritage): `SEL_RP_CONFIG` tag list + library per-song overrides (F15) | replayer SDK + library | text | M2 | H |
| Library | Folder watching (desktop notify, Amiga rescan) + play counts (F16) | library | text | M3 | B |
| Visual | Phase view (stereo phase scope) (F17) | visualizer | text | M1 | |
| Output / DSP | Headphones mode: crossfeed DSP for hard-panned Paula output (F18) | DSP plugin | text | M1 | H |
| Output | Hardware audio outputs: SIDBlaster, USBSID-Pico, ZorroSID (SID passthrough), AmiGUS, Trinity, MHI MP3 decoder (F26) | output plugins | planar (Amiga) | M4 | H |
| Integration | Amiga OS integration: public screen / own screen, iconify to AppIcon, AppWindow, ToolTypes = CLI = commands, task priority (F27) | Amiga UI plugins + core/amiga | planar | M2 | H |
| Library | Per-song `wrong` flag with note (sounds-wrong.txt, `selecta-cli wrong`) and `skip` flag, both query keys (F28) | library + cli | text | M3 | B |
| Playback | Streaming MP3/Ogg from URLs and pipes (`PIPE:` on Amiga); M3U URL-only import quirk honoured (section 17) | engine + stream decoder plugins + local source | text | M3 | H |
| Library | Companion/archive songs inside collection archives (bbmp "not prepared"): archives browse as folders in every source, not only `local` | library sources + F8 decrunch | text | M3b | B |
| Library | Synced lyrics: LRC files, tracker song message shown as lyrics, DEViLBOX lyrics endpoint (F19) | library + UI plugins | text | M3 | - |
| Library | Sonic similarity vectors from the analysis render, shared via DEViLBOX by song hash; feeds radio and smart shuffle (F20) | analysis path (F6) + library + DEViLBOX | text (fetch), desktop (compute) | M5 | - |
| Remote | Persisted cross-device play queue resumed on another device (F21, completes F4 hand-off) | engine + library + DEViLBOX | text | M5 | - |
| Playback | Smart shuffle: queue seeded by the current list plus similar songs from F20 and DEViLBOX genre (F23) | engine + library | text | M5 | - |
| Library | Stats screen: library totals, play time, top formats and artists per period (F24) | library SQL views + UI plugins | text | M3 | - |
| Integration | Offline scrobble queue with batch flush, last.fm and ListenBrainz (F25) | scrobble plugin | text | M5 | - |
| Playback | Analysis-driven smart fade and MixRamp overlap points, computed on desktop, shared via DEViLBOX like loudness (F29, extends F1) | engine + analysis path + DEViLBOX | text (fetch) | M5 | - |
| Queue | Reshuffle, play scope (library / playlist / album only), next and previous album (F30) | engine, section 9c | text | M1 | - |
| Output | Direct (bit-perfect) volume flag on output plugins; per-output enable and independent output partitions (F31) | output plugins + engine routing | text | M5 | - |
| Library | Aminet, Modules.pl and AMP as `sel_source` modules (F33) | library sources | text | M3b | H |
| Library | Smart downloads: cache prefetch policy driven by history and smart playlists (F34) | library cache | text | M3 | - |
| Library | Podcasts: RSS feeds as a source with per-episode state (F35) | library source + stream decoder plugins | text | M5 | - |
| Output | Stream-to-disk recording of radio and streams (F36, F6 record path) | record sink | text | M3 | - |
| Social | Blend: DEViLBOX builds a shared playlist from several users' histories (F37, server work D9) | DEViLBOX + library | text | M5 | - |
| Visuals | Signal path display (replayer > DSP chain > output) and fullscreen visual mode (F38) | engine state + UI plugins | text (path), planar (fullscreen) | M4 | - |
| Output | Advanced DSP: parametric EQ, upsampling, convolution, upmix as chain plugins (F39, extends F6) | DSP plugins | RTG / desktop (float; hidden by `sel_machine`) | M5 | - |
| Social | Hosted public share pages on DEViLBOX for song, playlist and position (F40, extends F4; server work D9) | DEViLBOX | text | M5 | - |
| Library | Network storage discovery (SMB/NFS mounts, neighbours) as local-source roots (F41) | local source + core/<os> | text | M5 | - |
| Files | Clipboard URL watch: a copied song URL offers to play (F43) | desktop UI plugins | desktop | M5 | - |
| Metadata | Batch tag editing, fingerprint tagging (AcoustID/MusicBrainz, mp3/stream only), Demozoo lookup, last.fm/MusicBrainz artist info (F44) | library + metadata lookup plugins | text | M5 | - |
| UI | Vi-style command mode with completion; run an external command on the selection (F45) | text UI | text | M1 | - |
| Integration | OpenSubsonic server API served by the engine, so any Subsonic client is a Selecta remote (F46) | remote server, section 8 | text | M5 | - |
| UI | Configurable columns, title-format strings and status-line format (F48) | view model (section 9b) + UIs | text | M1 | - |
| UI | Command palette, help overlay, undo/redo of queue edits (F49) | engine queue history + UIs | text | M1 | - |
| Output | Multi-zone sync: grouped outputs with synced clocks (F50, restored into scope) | engine routing + network outputs | desktop | M5+ | - |
| Remote | Server transcoding to MP3/Opus for remote clients (F51, restored into scope) | remote server + encoder plugins | desktop | M5+ | - |
| Library | Multi-user accounts and multiple libraries on one engine (F52, restored into scope) | library + remote server | text | M5+ | - |

## 2. Layering

```
+-----------------------------------------------------------------------------+
| apps/selecta (shell: loads plugins, runs active UI, handles UI switch)      |
+-------------------+-------------------+-------------------+-----------------+
| UI plugins        | viz plugins       | remote protocol   | CLI (render,    |
| text/gadtools/    | scope/spectrum/   | (socket/pipe,     |  probe, test)   |
| mui/sdl           | pattern view      |  same cmd/event)  |                 |
+-------------------+-------------------+-------------------+-----------------+
| engine (headless): commands -> state -> events                              |
|   song lifecycle | scheduler | voices (mixer | paula) | routing | queue     |
|   view model (section 9b)                                                   |
+-----------------------------------------------------------------------------+
| library (headless): sources | catalog (SQLite) | cache | archives |         |
|   playlists + folder tree | devilbox client | loudness store                |
+-----------------------------------------------------------------------------+
| replayer plugins          | output / DSP plugins  | host table (SDK runtime)|
| nuade-adapter, openmpt,   | paula, paula14, ahi,  | alloc, io, log, timer,  |
| gme, sid, uade-native(hw) | coreaudio, pulse,     | voices, config, query,  |
| stream decoders           | wasapi, sdl, wav      | machine                 |
+-----------------------------------------------------------------------------+
| core / platform abstraction: mem (chip/fast), file io, dynload, clock, net, |
|   machine probe (section 12)                                                |
+-----------------------------------------------------------------------------+
```

Rules
- Arrows point down only. Engine never includes a UI header. Plugins never include OS headers except in `plugins/*/amiga/` or `plugins/*/<os>/` directories, and only output/hardware plugins get that right.
- Every plugin kind talks to the host only through the `sel_host` function table it receives at load. That table is the "runtime provided to plugins" (Eagleplayer's model, koobo's point).
- Hot paths (tick, render, voice ops) are function pointers; everything else, descriptors, capabilities, metadata, queries, is a tag list (section 10).

## 3. Engine: state, commands, events

The engine is a state machine driven only by commands. Every state change emits an event. UIs never mutate state directly.

```c
typedef enum sel_cmd_id {
#define SEL_CMD(id, args) SEL_CMD_##id,
#include <selecta/commands.def>
#undef SEL_CMD
} sel_cmd_id;
```

`commands.def` and `events.def` are X-macro tables: one line per command/event with its argument list. They generate the C enums, the text codec of the remote protocol, and the reference docs. One source of truth. Decision 2026-10-22: the tag-list ABI (section 10) does not replace these tables; an event payload that carries plugin data (metadata, voice info, chip registers) carries it as a `sel_tag` list, and the `.def` line says so.

Commands (first set): OPEN path|data, PLAY, PAUSE, STOP, NEXT, PREV, SUBSONG n, SEEK frame|pattern:order:row, VOLUME v, MUTE voice on|off, SOLO voice, PITCH, TEMPO, REPEAT mode, OUTPUT name, QUEUE_ADD/REMOVE/CLEAR/PLAY_NEXT, PLAYLIST_LOAD/SAVE/PUBLISH, LIBRARY_* (browse/search/fetch), RATE stars, VOTE, CUE_SET/CUE_GOTO, SLEEP_TIMER, VIEW_STYLE style, VIEW_GROUP key, VIEW_EXPAND node, VIEW_COLLAPSE node, VIEW_DRILL level value, UI_SWITCH name (handled by the shell, forwarded for logging).

Events (first set): STATE (stopped/playing/paused), SONG_OPENED (metadata tags, voices, subsongs, player id), SONG_ENDED, POSITION (order/row/tick, frame), VOICE (per-voice note/instrument/volume snapshot), OUTPUT_CHANGED, QUEUE_CHANGED, LIBRARY_RESULT, VIEW_CHANGED (style, group keys, row count), VIEW_ROWS (window of rows with depth, node id, artwork id), ERROR, LOG.

Protocol additions from the terminal/web survey (research doc `2026-10-22_features-terminal-and-web.md`, mpd/musikcube/cmus heritage), 2026-10-22. These go into `commands.def` / `events.def` at M0 so every client (text UI, web remote, Lua, ARexx, `selecta-cli`) gets them for free:
- **Queue identity.** Every queue row has a stable entry id (not its index). `QUEUE_CHANGED` carries a monotonically increasing `QUEUE_VERSION`; `QUEUE_CHANGES since=N` returns only rows changed after version N, so a remote never re-fetches a long queue. `QUEUE_MOVE from to`, `QUEUE_SWAP a b`, `QUEUE_SHUFFLE [start:end]` take ids or `START:END` ranges. `SNAPSHOT_QUEUE`, `DEVICE_LIST` / `DEVICE_TRANSFER <id>` / `DEVICE_FOLLOW` (hand-off, section 8) returns a paged window for clients that page.
- **Option family.** `OPTION name value` for consume, single, random, repeat, crossfade, stop-after-current, stop-after-queue; `OPTIONS_CHANGED` event. One verb instead of one command per mode.
- **Query playback.** `PLAY_QUERY <query>` and `QUEUE_ADD_QUERY <query>` take a library query (section 9.2) directly, so smart playlists and radio seeds are one command.
- **Command lists.** `BEGIN` ... `END` wraps several commands into one all-or-nothing batch (mpd command_list heritage); the engine applies them under one state lock and emits one `QUEUE_CHANGED` / `VIEW_CHANGED` at the end.
- **Reflection.** `COMMANDS` and `EVENTS` list every verb with its argument signature, generated from `commands.def` / `events.def` at build time, so a client (Lua, ARexx, web remote, `selecta-cli`) can discover the protocol it talks to; `TAGTYPES` lists the `SEL_META_*` names.
- **Follow mode.** `OPTION follow on|off`: the view model cursor tracks the playing song (ncmpcpp/cmus follow); off keeps the user cursor where it is.
- **Idle and subscription.** `IDLE <subsystems>` blocks (or, over the socket, defers the reply) until one of the named subsystems changes: player, queue, options, library, output, view, sticker. Replaces polling for every remote.
- **Sticker store.** `STICKER_SET/GET/DEL/LIST key value` per song key: generic key/value storage that scripts, remotes and UIs use for their own per-song data without a schema change (mpd sticker model). Ratings, liked, cue points and local notes use the typed library tables, not stickers.
- **Reflection.** `COMMANDS` lists every command with its argument signature, generated from `commands.def`; `EVENTS` likewise. A client can discover a newer engine.
- **Atomic lists.** `BEGIN ... END` (the same verbs as the batch bullet in the protocol additions; `COMMAND_LIST_BEGIN` was an earlier name for the same feature) runs several commands as one transaction with one reply; this is what the Lua runtime and the ARexx port (D7) send.
- **Relative and idempotent verbs.** `SEEK +10`/`SEEK -10`, `VOLUME +5`; `PAUSE` toggles, `PAUSE_PLAYBACK on|off` is idempotent for scripts.
- **Events.** `SEEKED` (position jumped, as distinct from `POSITION` ticks), `SCROBBLE_DUE` (the song passed the last.fm half-or-four-minutes rule; the scrobble plugin listens), `ALBUMART` chunked by offset so a remote can fetch artwork over the same socket.
- **Auth.** `AUTH <token>` as the first command on a network socket; the engine logs a warning when the remote listener is not bound to a LAN-only address. Local UIs never authenticate.
- **Song-change hook.** `OPTION hook_command <path>` runs a user program on `SONG_OPENED` (cmus/mpd heritage), desktop and Amiga (ARexx script).
- **Record while playing (F36, I196a/I196b/I196c).** `OPTION record_to <directory>` (up to 255 characters, the limit of every option value; empty turns it off) names a DIRECTORY. Every OPEN of an http(s) stream writes a new file there while it plays, from the same single HTTP request, and the request sends `Icy-MetaData: 1`: the in-band ICY metadata blocks (`icy-metaint`) are removed from the audio the decoder and the recorded bytes see, and each `StreamTitle` becomes the song title; a changed title starts a new file (one file per song). File names come from the shared helper `src/library/record_name.c` (title as name, " (2)" suffix instead of overwriting; there was no earlier F6 naming code); an MP3 file of an ICY stream starts with an ID3v2.3 block holding the title (TIT2). `selecta-cli play <url> --output <id> --record <dir>` drives the same option; `selecta-cli record <url> <file>` is the headless recorder on the same sink (single file, metadata removed, no split).

M1.1 as built (2026-10-23, plan `2026-10-23-m1-1-text-ui-queue.md`, Deviations I45 to I60). Queue: stable ids, `QUEUE_ADD/REMOVE/CLEAR/PLAY_NEXT`, `NEXT`, `PREV` (within 3000 ms goes back, after that restarts), auto-advance on song end (a `LOOPED` tick counts as the end for now), `QUEUE_CHANGED` carries the version. `SEEK` takes milliseconds, answers `SEEKED`, and runs as a song change into the idle replayer slot: restart-and-skip for VOICE-kind replayers (byte-exact), `RP_SEEK` for RENDER-kind ones (libopenmpt native seek, accepted as not byte-exact; the position after the seek is asserted instead). `RP_RENDER`, `RP_SEEK` and `RP_POSITION` are in `tags.def`; `POSITION` carries order, row and pattern. Four VIEW verbs are born implemented: `VIEW_SHOW`, `VIEW_CURSOR`, `VIEW_ACTIVATE`, `VIEW_WINDOW`; `VIEW_STYLE` accepts only `regular`; `VIEW_ROWS` carries the tags `UI_ROW_COUNT`, `UI_CURSOR`, `UI_WINDOW_START`, `UI_WINDOW_ROWS` and the rows are read from the snapshot `view`. UI tags are `0x5009` to `0x500E` (the last two are the stats `UI_STAT_FLUSHES`, `UI_STAT_KEYS`); `LIBRARY_RESULT` carries `SRC_PATH` (`0x7000`) and `SRC_COUNT` (`0x7001`). `sel_state` gained `song`, `queue`, `queue_count`, `queue_playing`, `queue_version`, `queue_text` and `view`.

State snapshot: `const sel_state* sel_engine_state(sel_engine*)` is read-only and valid between two `sel_engine_pump()` calls. A UI redraws from it on every event; it does not cache or derive playback state of its own. The view model (section 9b) is part of the snapshot.

Threading model (one design, two realities)
- Desktop: an audio thread (output callback) pulls frames from the engine's render path; the UI thread calls `sel_engine_pump()`. Commands go UI -> engine through an SPSC queue, events engine -> UI through another. No locks in the render path, no allocation after `start`.
- Amiga: the audio callback is an interrupt (CIA timer tick for voice players, AHI's player hook for mixed output). Single core, so the same SPSC queues work with `volatile` and interrupt disable only around queue head/tail publication. `sel_engine_pump()` runs from the main loop (Wait on signals).

- **Queue scope and album navigation (F30).** `RESHUFFLE` re-randomises the remaining queue order with the current seed policy, `SCOPE library|playlist|album` limits what next, previous and shuffle may pick from, and `NEXT_ALBUM` and `PREV_ALBUM` jump to the first song of the adjacent album in the current scope. All four are plain commands in `commands.def` and change only engine queue state, which UIs render from the snapshot. Section 9c holds the queue semantics they act on.
- **Queue undo and redo (F49).** Every queue edit (add, remove, move, clear, reshuffle) pushes an inverse record onto a fixed-size history ring allocated at start. `UNDO` and `REDO` pop and replay it, and the event `QUEUE_HISTORY` carries the undo and redo depths so a UI can grey out its entries. Playback position is never part of the history, only the list.
- **Smart shuffle (F23).** `SHUFFLE smart` seeds the queue from the current list and then interleaves songs ranked by the F20 similarity vectors and the DEViLBOX genre list. The ranking itself is integer distance over fixed-size vectors fetched by the library, so the engine side only consumes an ordered candidate list and never computes similarity. On machines without vectors or network it falls back to ordinary shuffle and reports so in the reply.
- **Smart fade and MixRamp (F29).** The engine applies overlap points (fade-out start and fade-in end per song) that the library fetched from the analysis cache or DEViLBOX, and starts the next song early so the ramps meet. The fade curve is a 16.16 fixed-point gain table with no float on the audio path. The analysis that finds the points runs on the desktop tier only; other machines just consume the stored values and fall back to the plain crossfade (F1) when none exist.

## 4. Replayer plugin SDK

Modelled on nuade's `NuadePlayer` (file `include/nuade/player.h`), which is the cleanest replayer contract available: probe, open, start, then either `tick` (voice-driven) or `render_raw` (self-rendering), plus read-only display hooks. We define our own ABI with that shape and bridge nuade through one adapter plugin. Reason: nuade is "very early" and may change its ABI; our SDK must stay stable for third-party authors; the same ABI must also host libopenmpt, gme, sid, bbmp's helpers, stream decoders and native 68k players.

Three replayer kinds

| Kind | Who owns the clock | Who produces audio | Reaches Paula via |
|---|---|---|---|
| `SEL_RP_KIND_VOICE` (tick) | engine scheduler | engine voices backend | Paula-direct voices (no mixing) when voice_count <= 4, else mixer -> paula14/AHI |
| `SEL_RP_KIND_RENDER` | plugin (sample clock) | plugin, int16 stereo into host buffers | mixer sink -> paula14 or AHI |
| `SEL_RP_KIND_HW` (Amiga only) | plugin (takes CIA) | plugin writes Paula | directly; engine releases hardware first |

A replayer is a tag list (section 10). Every entry is optional except name, kind, probe, open, close and start; the host asks for what it needs with `sel_tag_find` and never reads a struct by offset:

```c
static const sel_tag sonix_tags[] = {
    { SEL_RP_NAME,        (uintptr_t)"nuade.sonix" },
    { SEL_RP_VERSION,     0x00010000 },
    { SEL_RP_KIND,        SEL_RP_KIND_VOICE },
    { SEL_RP_EXTENSIONS,  (uintptr_t)"smus,snx,tiny" },
    { SEL_RP_FORMATS,     (uintptr_t)"Sonix Music Driver" },       /* Modland format names, section 9 */
    { SEL_RP_MAX_VOICES,  4 },
    { SEL_RP_NEEDS_CHIPMEM, 1 },
    { SEL_RP_PROBE,       (uintptr_t)sonix_probe },                 /* (data, size, name) -> SEL_PROBE_YES/NO/UNSURE */
    { SEL_RP_OPEN,        (uintptr_t)sonix_open },                  /* (const sel_song_source*, const sel_host*) -> state */
    { SEL_RP_CLOSE,       (uintptr_t)sonix_close },
    { SEL_RP_START,       (uintptr_t)sonix_start },                 /* (state, subsong, rate, sel_voices*) */
    { SEL_RP_TICK,        (uintptr_t)sonix_tick },                  /* VOICE kind */
    { SEL_RP_METADATA,    (uintptr_t)sonix_metadata },              /* (state) -> const sel_tag* of SEL_META_* */
    { SEL_RP_POSITION,    (uintptr_t)sonix_position },              /* display hooks, read-only, optional */
    { SEL_RP_VOICE_INFO,  (uintptr_t)sonix_voice_info },
    { SEL_RP_TRACKER_CELLS, (uintptr_t)sonix_cells },
    { SEL_RP_SAMPLE_GET,  (uintptr_t)sonix_sample_get },            /* sample list + export */
    { SEL_RP_CHIP_REGS,   (uintptr_t)sonix_chip_regs },             /* chip register panel, SEL_CHIP_PAULA */
    { SEL_RP_FLAGS,       SEL_RP_CAN_MUTE | SEL_RP_CAN_PITCH | SEL_RP_CAN_SEEK },
    { SEL_TAG_END, 0 }
};
```

Hooks named by tag: `SEL_RP_RENDER` (RENDER kind: `(state, int16_t* stereo, int16_t* const* per_voice, uint32_t frames)`), `SEL_RP_EVENTS`, `SEL_RP_TRACKER_INFO`, `SEL_RP_GRID_TICKS`, `SEL_RP_SAMPLE_COUNT`, `SEL_RP_DUSEL_STATE`, `SEL_RP_SUBSONG_COUNT`, `SEL_RP_DURATION`, `SEL_RP_CHIP_MODEL` (SID 6581/8580 + filter curve, selectable per song), `SEL_RP_COMPANIONS` (file name rule for sample sets), `SEL_RP_MIN_CPU`, `SEL_RP_NEEDS_FLOAT`, `SEL_RP_MEMORY` (machine requirements, section 12).

Metadata is a tag list too (`SEL_META_TITLE`, `SEL_META_AUTHOR`, `SEL_META_FORMAT`, `SEL_META_CHANNELS`, `SEL_META_PATTERNS`, `SEL_META_INSTRUMENTS`, `SEL_META_SAMPLES`, `SEL_META_SUBSONGS`, `SEL_META_DURATION_MS`, `SEL_META_MESSAGE`, `SEL_META_SIZE`, `SEL_META_YEAR`, `SEL_META_RELEASE`, `SEL_META_SID_MODEL`, `SEL_META_STIL`, ...). The replayer returns what it knows; the library merges it with catalogue data (section 9) under the same keys. There is no fixed `sel_meta_sink` struct.

Voices (`sel_voices`): the nuade voice API verbatim in shape: play(sample,start), queue(sample), stop, set_pitch, set_volume, set_pan, get_state. One addition for Amiga-native players and for 68000 without FPU: `set_period(paula_period)` beside `set_pitch_hz`; the backend converts in whichever direction it needs with integer math.

Sample memory: a plugin allocates sample data through `host->mem_alloc(size, SEL_MEM_SAMPLE)`. On Amiga that is `MEMF_CHIP`; elsewhere a plain allocation. The nuade adapter copies sample buffers once at open when a player keeps them in its own memory. Documented SDK rule, not a convention.

Mutes, solo, repeat count, stereo separation, seek (by re-running from start, nuade style; by pattern/order position where `SEL_RP_CAN_SEEK` says the plugin seeks itself), cue points, loop detection and fade live in the engine, not in plugins.

Replayer configuration (F15): a replayer publishes its tunable parameters as a `SEL_RP_CONFIG` tag list (name, type, range, default per parameter, the same shape as `SEL_DSP_PARAMS`), so every UI can generate a settings panel without knowing the replayer. The library stores two override layers keyed by replayer name and by song key: per-replayer defaults (Eagleplayer `EP_LoadPlConfig` heritage) and per-song overrides (UADE `song.conf` heritage: subsong, timeout, filter, panning, speed hack). The engine applies defaults, then the per-song layer, through `SEL_RP_SET_PARAM` before `start`. Imports: UADE `song.conf` and `eagleplayer.conf` are read by the UADE adapter as the initial per-song layer.

## 5. Output plugins, DSP and routing

Two sink classes, because Paula is not a PCM device for voice players. An output plugin is a tag list with `SEL_OUT_NAME`, `SEL_OUT_SINK` (`SEL_SINK_PCM` and/or `SEL_SINK_VOICES`), `SEL_OUT_RATES`, `SEL_OUT_CHANNELS`, `SEL_OUT_LATENCY_MS`, `SEL_OUT_OPEN`, `SEL_OUT_CLOSE`, `SEL_OUT_SET_SOURCE` (PCM sink: pull model, `(state, sel_pcm_pull fn, void* ctx)`), `SEL_OUT_VOICES` (voice sink: `(state) -> sel_voices*`, the output IS the voices backend).

Routing policy (engine, one function, user-overridable per song): VOICE player and an open voice sink with enough channels -> Paula direct. Otherwise mixer -> the PCM sink. HW player -> release everything, hand over. The policy is data (a table), not a chain of ifs across the code base.

Mixer: one fixed-point mixer everywhere (16.16 phase, 32-bit accumulate, int16 out), resampler selectable by tag (`SEL_MIX_RESAMPLE_NONE/LINEAR/CUBIC/SINC`, default chosen from `sel_machine`), per-song gain for loudness normalisation (section 6a), Paula LED filter emulation and stereo width as integer stages on the mixed output when the active output is not real Paula.

DSP chain: a DSP plugin kind (`SEL_DSP_NAME`, `SEL_DSP_PROCESS (state, int16_t* stereo, frames)`, `SEL_DSP_NEEDS_FLOAT`, `SEL_DSP_PARAMS` tag list for UI generation) sits between mixer and PCM sink; the chain is engine state, ordered, with an EQ, reverb, stereo expander and limiter shipped first. DSP never runs on the Paula-direct path (there is no PCM to process); a DSP that needs float is hidden on machines without measured fast float (section 12).

Headphones mode (F18): a built-in `crossfeed` DSP plugin (integer Bauer-style crossfeed: delayed, low-passed bleed between channels) for Paula's hard left/right panning; enabled by the `SEL_OUT_HEADPHONES` output tag or by the user, and the `stereo separation` mixer tag covers the cheap case on slow machines.

Amiga outputs: `paula` (voice sink, 4 channels, chip RAM samples), `paula14` (PCM sink, 14-bit two-channel-per-side mixing, Eagleplayer "14 Bit Amplifier" and AHI's paula driver technique), `ahi` (PCM sink via ahi.device). Desktop: coreaudio, pulse/alsa, wasapi, sdl2; network outputs chromecast, airplay, upnp (desktop only, M5); `wav` and `flac` everywhere (also the test oracle and the record-to-file feature, running faster than real time when no live output is open). Mixed Amiga outputs double-buffer sample data (HippoPlayer doublebuffering heritage): two chip-RAM buffers swapped on the audio interrupt, buffer length a `sel_prefs` key defaulted from `sel_machine` CPU class.

Hardware audio outputs (F26, HippoPlayer parity): output plugins for external chips and decoders, each a `SEL_OUT_*` tag list like any sink. SID passthrough sinks `sidblaster`, `usbsid` (USBSID-Pico) and `zorrosid` take the SID register stream from the replayer instead of PCM (`SEL_OUT_SINK_SID`: the sidplayfp-class replayer exposes register writes by tag, the sink forwards them, no mixing). PCM sinks `amigus` (AmiGUS hardware mixer), `trinity` (accelerator audio) and `mhi` (MHI hardware MP3 decoder: a sink that takes the compressed stream, so the mp3 stream decoder plugin is skipped when `mhi` is selected). All Amiga tier, loaded only when `sel_machine` reports the hardware; absent hardware hides the plugin, never fails the engine.

Format coverage rule: every format HippoPlayer or bbmp plays has a Selecta plugin owner in `2026-10-22_format-coverage.md` (F14, 445 formats). Owner classes, each one adapter plugin wrapping one upstream library (rule 5, adapt never fork): **nuade** (121 players, M1; counted from the registry walk, `selecta-cli plugins --formats nuade`), **openmpt** (63 trackers), **gme** (14 console), **sidplayfp** (4 SID), **stream decoders** (mp3/ogg/flac/wav for radio stations and audio files), **native UADE** (Amiga only, 17 formats nuade skips, M4), and the classes the F14 count showed missing from v1 of this doc: **zxtune** (41: ZX Spectrum trackers, DS/PSF/PS2/Saturn/Dreamcast/N64 sound formats, HivelyTracker), **adplug** (40 Ad Lib/OPL formats), **libvgm** (8 VGM/S98 variants), **libkss** (7 MSX), **libxmp** (6 trackers openmpt lacks), **furnace** (4), **protrekkr** (4), **sndh** (Atari ST, 1), and about 30 single-format C helpers that bbmp ships under `csrc/` (soundclub, fxm, synder, buz, ym, pxtone, organya, goattracker, musicline, ... ) wrapped one plugin each from the same source. Milestones: nuade + openmpt + gme + sidplayfp + streams at M1/M2; zxtune, adplug, libvgm, libkss, libxmp at M2b; furnace, protrekkr, sndh and the bbmp helper set at M3c. 22 formats bbmp lists but cannot play (Renoise, Tunefish, ...) stay listed with owner "none" and are a tracked gap, as in bbmp. A format in the catalogue with no owner is a defect, not a gap. Coverage counts are re-derived from the doc's table with the grep commands it records, never retyped.

Per-tier owners (decided 2026-10-23): a format may have more than one owner, and the Amiga build uses optimized native 68k players for many formats instead of the portable C library that owns the format on the desktop. Each replayer descriptor declares the machine it needs (minimum CPU class, FPU, RAM) and a priority as tags; the registry, given `sel_machine`, picks the highest-priority owner whose requirement the running machine meets. One format table, one selection rule, no per-platform branches in the engine. Native 68k players come from HippoPlayer-68k (`~/Code/HippoPlayer-68k/code/pl/`, about 40 sources) and are wrapped as adapter plugins like any other (rule 5). Which formats get a native owner is a research row (inventory of HippoPlayer's players against `2026-10-22_format-coverage.md`). First decision under this rule: XM on the Amiga uses HippoPlayer's `xmaplay060.s` (8bitbubsy's Fasttracker II port), 68060 only; PS3M is not used for XM. On 68020 to 68040 XM therefore has no owner: a tracked gap by the user's choice, listed as such in the coverage doc, not a silent hole. MOD, S3M and MTM keep their current owners.

Per-tier owners, round 2 (decided 2026-10-23, after the R10 inventory `thoughts/shared/research/2026-10-23_hippoplayer-native-players.md`): (1) On the Amiga a HippoPlayer native player outranks nuade for every format both cover (about 20); nuade stays the fallback owner, so a native player's priority tag is higher than nuade's on the Amiga tiers. (2) XM keeps `xmaplay060.s` as its only Amiga owner, but its machine tag is split by output path: Paula-direct output needs a 68060, while AHI or AmiGUS output runs on 68020 to 68040 (the player's code gate is 68020, `puu016.s:53419`). The XM gap on 68020 to 68040 therefore narrows to machines without AHI or AmiGUS hardware. (3) PS3M (`Hippo_PS3M3.s`) owns IT on 68020 and up; it still does not own XM, and MOD, S3M and MTM keep their owners. (4) Amiga target machines have fast RAM: binary and heap size count fast RAM, and only sample data Paula plays has to sit in chip RAM. The 1.85 MB 68k image with all 121 nuade players is within budget, and the vamos tests run with `-m 8192`.

- **Direct volume and output partitions (F31).** An output plugin may set `SEL_OUT_DIRECT_VOLUME` to declare that it passes samples bit-exact, and the engine then bypasses software volume, DSP and normalisation for that output and routes volume to the device when it has a control. Each output also has an enable flag, and a partition table says which outputs play which stream, so two outputs can carry two independent streams. The routing table is plain engine state set by commands and shown in the snapshot.
- **Stream recording (F36).** A record sink attaches to a stream source (radio, podcast, network stream) and writes the incoming bytes or the decoded audio to disk, split per song when the stream carries metadata changes. It is the same record path as F6, so the file naming, tag writing and format options are shared and only the tap point differs. On machines without a writable big disk the sink is hidden.
- **Advanced DSP chain (F39).** Parametric EQ, upsampling, convolution and upmix are chain plugins with `SEL_DSP_PARAMS` lists like any other DSP, so every UI generates their panels without special code. Plugins that need float or long filters are marked in their tag list and `sel_machine` hides them where float is slow, while a fixed-point variant is offered where one exists. The chain order is user-editable and persisted in the profile.
- **Multi-zone sync (F50).** Outputs can be grouped into a zone that shares one clock: the group master renders once and each member output receives the stream with a per-output delay. Drift between device clocks is measured against the master and corrected by integer-ratio resampling nudges, never by dropping blocks. This is desktop tier and scheduled for M5+, with network outputs as the usual members.

## 6. Visualizers

Split data from drawing. The engine produces taps: per-voice scope buffers, VU, spectrum bands (FFT when float is fast, integer band filters otherwise), note/instrument per voice (nuade `viz.h` already defines these), tracker cells, chip registers (`SEL_RP_CHIP_REGS`). A viz plugin consumes taps and draws into an abstract surface the UI hands it:

```c
typedef struct sel_surface { uint32_t w, h, pitch; sel_pixfmt fmt; void* pixels; const uint32_t* palette; } sel_surface;   /* bitmap UIs: planar palette or chunky truecolor */
typedef struct sel_textgrid { uint32_t cols, rows; sel_cell* cells; } sel_textgrid;                                         /* text UI */
typedef struct sel_gpu_ctx { ... } sel_gpu_ctx;                                                                             /* GPU tier: GL/Metal/Vulkan handle from the SDL UI */
```

A viz plugin declares which surface kinds it renders and its tier with tags (`SEL_VIZ_SURFACES`, `SEL_VIZ_TIER` = text / planar / RTG / GPU). The host offers only visualizers whose tier `sel_machine` supports (section 12) and the UI blits. Visualizer code stays portable; a scope written once runs in the text UI, on an AGA screen and in SDL. Catalogue (section 9a): Eagleplayer analyzers, HippoPlayer scopes, nuade taps, Open Cubic Player views (pattern, note dots, channel view), projectM/Milkdrop class as the GPU reference, generated artwork per format or chip for songs with no cover.

Visualizer catalogue (M8, M1 research): per-voice scopes, VU meters, spectrum (integer bands on slow machines, FFT with fast float), pattern scroller, note dots (OCP), chip register panels, phase view (stereo phase scope, F17, needs the stereo PCM tap), artwork and generated art, projectM-class GPU scenes. Every one reads only host taps: voice levels, scope buffers per voice, the post-mix stereo PCM tap, tracker cells, chip register snapshots.

## 6a. Loudness normalisation

Module loudness is only known after rendering, so this is a data problem, not an impossible one. Design: ReplayGain-style per-song gain (integrated loudness, EBU R128 on desktop, RMS + peak as the cheap fallback) stored in the library per song key and shared through DEViLBOX by song hash (`GET/PUT /api/loudness/<hash>`, server work, ledger Found). Desktop computes it offline: a background render job runs the replayer faster than real time into an analyser with no audio output (the same path as record-to-file and golden tests). The Amiga never computes; it fetches. Playback applies the gain in the fixed-point mixer (16.16 gain, one multiply per sample, no float) with a peak limiter in the mixer stage. When no gain is known: an optional adaptive leveler (slow integer AGC) converges within a few seconds and writes its result back as a provisional gain. Album and playlist gain modes come later from the same data. Replayers render into host buffers so the analyser sees the same samples as playback; no replayer code changes.

## 7. UI plugins and on-the-fly switching

A UI plugin is a tag list: `SEL_UI_NAME`, `SEL_UI_OPEN (state, sel_engine*, sel_library*, const sel_host*)`, `SEL_UI_ON_EVENT (state, const sel_event*)` (redraw from `sel_engine_state()`), `SEL_UI_WAIT (state) -> int` (blocks on OS input + engine signal; returns when there is work), `SEL_UI_CLOSE`, `SEL_UI_VIEW_STYLES` (bitmask of section 9b styles it draws), `SEL_UI_NEEDS_RTG`, `SEL_UI_SURFACES` (which viz surfaces it can host), `SEL_UI_HOTKEYS` (default key map, user-rebindable through the shell).

The shell (`apps/selecta`) owns the main loop: `wait` on the active UI, pump engine, dispatch events. `UI_SWITCH` closes the active UI plugin and opens another; the engine is untouched, so playback continues. Several UIs may be attached at once (GadTools window + text UI over serial), since each is only an event consumer. The shell also owns global hotkeys, OS file integration hooks (drag-drop, AppWindow/AppIcon, open-with) delegated to the per-OS UI plugin, and the list of view styles offered: the intersection of what the active UI declares and what `sel_machine` allows.

Planned UIs: text (first), GadTools, MUI, SDL2 desktop (decision D5: SDL2 + own widget set in C).

Machine-scaled layouts: Amiga GUI plugins (MUI, GadTools) carry an RTG-aware rich layout (artwork panel, truecolor visualizer window, scaled fonts, grid and cover flow views) and a planar fallback layout (8 to 256 colours, flat/tree/drilldown views), chosen from `sel_machine` at open. The text UI scales the same way: sixel/kitty artwork and bitmap scopes when the terminal reports graphics, glyph scopes and plain lists otherwise.

Text UI targets one terminal profile: xterm-256color, UTF-8, SGR mouse, alternate screen, synchronized output, with 24-bit colour and sixel/kitty graphics used when the terminal answers for them. On the Amiga that terminal is UP-Term (`~/Code/UP-Term/vtcon`): its XCON: handler gives 256/24-bit colour, Unicode glyphs, sixel and kitty graphics, SGR mouse, so the same text UI runs in a Mac terminal and in an UP-Term window with no Amiga-specific drawing code (research doc `2026-10-22_upterm-text-ui-capabilities.md`). Own small widget layer over a cell grid with a diff-based flush (no curses); no kitty keyboard protocol, no OSC 8/52 (UP-Term lacks them). Scopes in the text UI are viz plugins rendering to a bitmap surface pushed as sixel/kitty, with a braille/block fallback.

Visual tiers, named once and used by UI, viz and `sel_machine`: **text** (glyph scopes, bars), **planar** (ECS/AGA copper/blitter scopes, 8 to 256 colours), **RTG** (chunky truecolor, per-pixel effects, artwork, cover flow), **GPU** (desktop OpenGL/Metal/Vulkan, or Vampire/PiStorm-class with fast chunky: 3D scenes, shaders). "From text to 3D" is the same plugin data at four tiers.

Theming (D8): every UI reads design tokens (colours, spacing, font roles) from a theme file; the SDL UI adds a bitmap skin loader (Winamp/AmigaAMP style) later. No hardcoded colours in a UI plugin.

Amiga OS integration (F27, HippoPlayer parity): the Amiga UI plugins open on a public screen chosen by setting (`PUBSCREEN`, default Workbench) or on their own screen; iconify to a Workbench AppIcon (drop a file on it = `QUEUE_ADD`); AppWindow drag and drop; ToolTypes and CLI arguments map 1:1 to the same commands and options as every other UI (one parser in `src/core/amiga/`); task priority of the player process is a setting; file requesters use asl (reqtools not needed on 2.0+). Stored programs on function keys F1 to F10 are user key bindings (F11), not a separate mechanism.

- **Signal path display and fullscreen visual mode (F38).** The engine snapshot exposes the active chain as data (replayer, each DSP plugin, the output, with rates and bit depths), and a UI renders it as a line or diagram. Fullscreen visual mode hands the whole surface to a visualizer plugin with the UI chrome hidden and any key restoring it. The text path display works on every tier, the fullscreen mode needs a planar or better surface.
- **Clipboard URL watch (F43).** Desktop UI plugins poll the system clipboard on focus and, when it holds a song or stream URL the source registry can resolve, offer to play or queue it. The prompt is a normal UI action that sends `QUEUE_ADD` or `PLAY`, and the feature can be switched off in settings. The clipboard access lives in `src/core/<os>/` so the UI code stays portable.
- **Vi-style command mode (F45).** The text UI opens a command line on `:` with completion drawn from the `COMMANDS` reflection output, so every typed command maps 1:1 to a protocol command and nothing is UI-private. A second form runs an external program on the current selection, passing the song paths as arguments, in the same spirit as the song-change hook. History is kept in the UI profile.
- **Configurable columns and title formats (F48).** Columns, title-format strings and the status-line format are user settings, but the strings are resolved in the view model (section 9b), which turns a song and a format into display text. UIs receive ready rows and column widths and do no formatting of their own. Defaults come from the active view style, and a bad format string falls back to the default with a warning.
- **Command palette and help overlay (F49).** A fuzzy-search palette lists every entry from the `COMMANDS` reflection output with its current key binding, so new commands appear without UI changes. The help overlay renders the same list grouped by area. Queue undo and redo (section 3) are reachable from both.

## 8. Headless and remote

Headless = library + CLI. `apps/selecta-cli` plays, probes, renders to WAV/FLAC, computes loudness, and talks the protocol to a running engine (scripting and tests) with no UI. `src/protocol/` exposes the same commands/events as newline-delimited text over a socket or pipe (bsdsocket.library on Amiga). Any external GUI, a web page, or a phone talks to it; the shell's in-process UIs do not go through it (no IPC cost on 68k). On top of the same protocol: an HTTP/JSON API with a web remote page served by the engine (party mode voting queue lives here), an MCP server (desktop), device hand-off (a second Selecta instance takes over the queue and position, Connect style), and per-OS integration plugins (MPRIS, macOS Now Playing, Windows SMTC, Discord rich presence) that are protocol clients in-process.

Device hand-off (F4, Spotify Connect model): every running engine announces itself on the LAN (mDNS `_selecta._tcp` on desktop, a UDP beacon on the Amiga stack) and lists the others as **devices**. `DEVICE_LIST`, `DEVICE_TRANSFER <id>` move playback: the source engine sends `SNAPSHOT_QUEUE` (queue with ids, cursor, position, options) to the target over the remote protocol, the target resolves song keys through its own sources and cache, starts at the same position, and the source stops. A UI connected to engine A keeps showing A's state unless it follows the transfer (`DEVICE_FOLLOW`). Hand-off needs no server; DEViLBOX is only involved when a song key is not reachable from the target (it fetches through the mirror). Party-queue relay (F9) and the web remote (F4) ride the same protocol and the same device list.

- **Persisted cross-device queue (F21).** The queue is stored by the library and mirrored to DEViLBOX under the user's account, so another device can fetch it and resume at the saved position. This completes the F4 hand-off for the case where the first device is off. Writes are batched on queue change and on pause, and a device that finds a newer remote copy asks before replacing its own.
- **Offline scrobble queue (F25).** The scrobble plugin appends each due scrobble (from the `SCROBBLE_DUE` event) to a persistent queue and flushes it in batches to last.fm and ListenBrainz when the network is up. Failed batches stay queued with their original timestamps and retry with back-off, and a size cap drops the oldest entries with a warning.
- **Blend (F37).** DEViLBOX builds a shared playlist from the histories of several users who opt in, weighted so each contributes a fair share. The engine side is only a client that requests the result and plays it as a normal playlist. The server work is ledger item D9.
- **Hosted share pages (F40).** DEViLBOX hosts a public page for a song, a playlist or a song with a position, and the engine asks it for a link through the remote API. The page plays through the DEViLBOX web player and needs no account to view. The server work is ledger item D9, and the engine shares only keys and hashes, never local paths.
- **OpenSubsonic server API (F46).** The remote server also speaks the OpenSubsonic REST API, so any Subsonic client can browse the library and control playback. Each endpoint maps to the same commands and events as the native protocol, with no state of its own, and authentication reuses the token model. Endpoints that have no Selecta meaning return the documented not-supported reply.
- **Server transcoding (F51).** For remote clients that cannot decode a tracker format, the server renders the song and encodes it to MP3 or Opus through encoder plugins and streams the result. Encoding runs on the desktop tier only, scheduled for M5+, and the encoder is a plugin so licensing stays isolated. Results can be cached by song hash and bitrate.

## 9. Library: sources, catalogue, cache

- **Synced lyrics (F19).** Lyrics come from LRC files next to the song, from the tracker song message shown as plain lyrics, or from the DEViLBOX lyrics endpoint by song hash. The library parses LRC into timed lines and the view model hands the current line to UIs on a position tick. Songs without lyrics simply show nothing.
- **Sonic similarity vectors (F20).** The analysis render (F6) produces a small fixed-size integer vector per song, computed on desktop and shared through DEViLBOX by song hash so other machines only fetch it. The vectors feed radio mode and smart shuffle (F23). Nothing in the engine computes them at play time.
- **Stats screen (F24).** Library totals, play time and top formats and artists per period are SQL views over the history tables, and a UI renders the query results through the view model. No counters are kept outside the database, so the numbers always match the history.
- **Aminet, Modules.pl and AMP sources (F33).** Each is a `sel_source` module with the same interface as the bbmp sources, covering browse, search and fetch into the cache. This is part of HippoPlayer and bbmp parity, and each source is its own plugin so a machine loads only the ones it uses.
- **Smart downloads (F34).** The cache prefetch policy uses play history and smart playlists to decide which remote songs to fetch ahead of time, within the cache budget and respecting kept and liked songs. Prefetch runs at low priority and pauses on metered or slow links, and users can turn it off.
- **Podcasts (F35).** An RSS feed is a source whose items are episodes, each with its own state (new, played, position, downloaded). Stream decoder plugins handle the enclosure formats, and refresh runs on a timer or on request. Episode position is saved on pause so playback resumes where it stopped.
- **Network storage discovery (F41).** SMB and NFS shares and neighbouring hosts are found through code in `src/core/<os>/` and offered as roots for the `local` source. A found share is mounted or accessed through the OS and then scanned like any folder. Credentials are kept in the profile, never in the library DB.
- **Tag tools and metadata lookup (F44).** Batch tag edit, AcoustID fingerprinting (only for mp3 and stream files, where audio tags are meaningful), Demozoo lookup for tracker songs, and last.fm and MusicBrainz artist info are all metadata lookup plugins behind one interface. Results are proposals that the user accepts, and accepted values go into the library DB, with file tag writes only where the format allows.
- **Multi-user accounts and libraries (F52).** One engine can hold several accounts, each with its own library DB, queue, history and settings, selected at login on the remote server. The library layer takes an account handle instead of a global, so no code assumes a single library. Scheduled for M5+.

### 9.1 Sources

Every place music comes from is a `sel_source`, modelled on bbmp's `Collection` (research doc `2026-10-22_bbmp-sources.md`). One C file per source under `src/library/sources/<id>.c`; adding a source never touches the engine, the UI or the replayers.

```c
typedef struct sel_source {
    const char* id;                  /* "modland", "hvsc", "asma", "sndh", "zxart", "keygenmusic", "local", "hippo", "radio" */
    const char* name; const char* home; const char* about;
    const sel_tag* tags;             /* SEL_SRC_HAS_LISTS, SEL_SRC_HOSTED_BY, SEL_SRC_HAS_MD5, SEL_SRC_TRANSPORTS ... */
    bool (*listing)(void* st, sel_listing_sink* out, sel_progress* p);   /* full catalogue: path, size, md5?, format, artist */
    bool (*parse)(const char* path, sel_tag* out);                        /* format / artist / title from a path */
    const sel_transport* (*transport)(void* st);                          /* FTP, HTTP, or Mirrors (ranked list) */
    bool (*url)(const char* path, char* out, size_t n);                   /* canonical public URL */
    bool (*path_in)(const char* url, char* path, size_t n);               /* inverse */
    bool (*published_lists)(void* st, sel_list_sink* out);                /* optional: the source's own playlists */
    bool (*read_list)(void* st, const char* id, sel_playlist* out);
} sel_source;
```

Song key = `<source id>:<path>`: globally unique, stable, human readable, and the key for ratings, history, liked, cue points, loudness and playlists. Format names reuse Modland's (`Protracker`, `Fasttracker 2`, `SID`, ...) so one format table maps a name to a replayer plugin across every source.

Sources v1: `local` (filesystem with archive decrunch, archives browse as folders), `modland` (FTP + HTTPS mirrors, `allmods_md5.zip`, daily changelogs feed the home view's "new this week", publishes its own lists), `hvsc` (hosted by Modland, Songlengths.md5 and STIL from hvsc.c64.org), `asma`, `sndh` (yearly zip, central directory via two Range requests), `zxart` (paged JSON), `keygenmusic`, `hippo` (hippoplayer.se, playlists only, section 17), `radio` (streaming stations: Nectarine, SceneSat; M5). DEViLBOX is not a file source; it is the overlay for genre, ratings, votes, comments, songdb identity, loudness and the Amiga mirror proxy.

Transports: `Mirrors` ranks its transports by measured time to first byte and retries the next on failure (3 s to first byte, 30 s body, bbmp's limits). A home mirror setting (`mirror` in config or `SELECTA_MIRROR`) is put first in every source's list, so a LAN copy of Modland serves everything. Amiga builds reach HTTPS-only sites through the plain-HTTP DEViLBOX endpoint `/api/mirror/<source>/<path>` (decision 2026-10-22; server work, ledger Found D9); desktop builds talk HTTPS directly.

Amiga transport extras (HippoPlayer parity): the `local` source also accepts AmigaDOS device and pipe names (`PIPE:`, `RAM:`, `DF0:`) as paths; HippoPlayer's UHCBIN downloader is not reused, the Amiga build talks to the DEViLBOX mirror with its own bsdsocket HTTP client (section 16). Archive and XPK/PowerPacker decrunch is the `local` source's job through xfd/xad (F8), so a packed module is just another path.

### 9.2 Catalogue and library model

Library DB = SQLite everywhere (decision D6): one table set per source (`files(source, path, format, artist, album, name, hidden, md5, size)` from bbmp) plus Selecta's own tables: `songs` (song key, metadata tags merged from catalogue + replayer, duration, loudness gain, md5), `albums` / `releases` (from path structure, HVSC release data, DEViLBOX songdb), `artwork` (cover or generated), `history`, `liked`, `ratings` (mirrored to DEViLBOX), `cues`, `notes`, `playlists`, `playlist_entries`, `folders` (section 9c), `views` (saved queries). FTS5 prefix search over name, artist, format, path. On the Amiga the same schema runs on a small SQLite build with a read-only prebuilt index for the big sources and a writable local DB for the user's own data; the 68000 reduction later may swap the big index for a flat sorted table behind the same query API.

Per-song flags (F28, bbmp parity): a `wrong` flag with a free-text note ("this sounds wrong": bad replayer choice, broken sample, wrong speed) stored per song key in the library DB and exported to a `sounds-wrong.txt` list; `selecta-cli wrong <song> [note]` sets it; DEViLBOX push is optional and shares the comments endpoint (D9). A per-song `skip` flag (never play in radio or shuffle) lives next to it. Both are query keys (`wrong:yes`, `skip:yes`, section 9.2).

One query language serves search, smart playlists and the view model: typed filters `format:`, `artist:`, `album:`, `year:`, `source:`, `rating:`, `genre:`, `length:`, `liked:`, `played:` plus free text; compiled to SQL in `src/library/query.c`, nowhere else. Smart playlists are saved queries with a name.

Query syntax (Quod Libet and mpd filter heritage, fixed here so every client and the Lua/ARexx bindings agree): `key:value` is a case-insensitive substring match; `key:=value` exact; `key:/regex/` regular expression; `key:>n`, `key:<n`, `key:n..m` numeric and duration ranges (`length:>3m`, `size:<64k`); relative time `played:<7d`, `added:>2w`, `lastplayed:never`; `in:<playlist>` and `in:<folder>` test membership; `view:<saved query>` nests a saved query; `-term` and `NOT`, `OR`, parentheses; bare words match name, artist, path. Unknown keys are an error, not free text, so a typo never silently widens a smart playlist. The parser is one function with one test table; the view model, search box, `PLAY_QUERY` and smart playlists all call it. Genre, similar songs, also-by, composer pages and duplicate detection (md5 across sources, best-source preference) are queries over this DB plus DEViLBOX answers cached locally.

Local library maintenance (F16): the `local` source scans configured folders into the catalogue; on desktop a folder watcher (FSEvents, inotify, ReadDirectoryChangesW in `src/core/<os>/`) queues rescans of changed directories, on Amiga a manual or timed rescan does the same through one `sel_source_rescan(paths)` entry. Play counts, last played and skip counts live in the `history` table per song key and feed the home view, smart playlists (`plays:>5`) and radio weighting.

Browse dimensions: source > format > artist > file from Modland's path structure (bbmp), album/release and year where known, genre from DEViLBOX `GET /api/genre/:key` (key = the 12-hex UADE key: xxh32 of the first 256 KB plus `size & 0xFFFF`), rating 1..5 through `PUT /api/ratings` with a Bearer JWT, batch read via `GET /api/ratings/batch` (max 200). Item identity = song key, no hash, except where DEViLBOX keys by hash (genre, loudness, songdb).

Gaps in the server (work on the DEViLBOX side, not here, ledger Found D9): mirror proxy, loudness by hash, genre list, artist/composer endpoints, comments, party-queue relay; vote endpoints `/api/hip/vote` and `/api/hip/votes` unverified.

### 9.3 Cache, companions, prefetch

File cache `<cache dir>/files/<source id>/<path>`; md5 checked when the catalogue knows it; LRU trim to a size budget scaled by `sel_machine` disk and RAM; "kept" songs and liked songs are exempt. Companions (sample sets, `.smp`/`.sam`, multi-file formats) are fetched with the song by a per-format rule the replayer declares (`SEL_RP_COMPANIONS`). The next song in the queue is prefetched while the current one plays. All copied from bbmp.

## 9a. What we take from the inspirations

Eagleplayer 2.04 (research doc `2026-10-22_eagleplayer-architecture.md`): it is AmigaOS-only 68k asm, so nothing ports; its seams do. Copied: tag-list plugin descriptors everywhere (our section 10; koobo: "eagleplayer tag interfaces are easily expandable, this is also a major thing"), staged detection on a prefix then the whole file (our `probe` gets the first 2200 bytes first, then full data on `SEL_PROBE_UNSURE`), the `AS_` struct plus amplifier jump table (our output PCM/voice sinks), `DTN_NoteStruct` notes to the mixer (our voice events for viz), module-info and pattern-info tags (our `SEL_META_*` and `tracker_cells`), engines activated/deactivated by tag (our UI open/close switch), LoadSeg of a plugin whose first hunk is the descriptor (our Amiga loader), analyzers as separate engines (our viz plugins), converters and XFD decrunch (our converter plugin kind and archive layer). Not copied: task engines messaging a named port (replaced by in-process event delivery plus the remote protocol), Paula-poke amplifier emulation for every replayer (voice sink does it once).

HippoPlayer (`~/Code/HippoPlayer-68k`, parity doc `2026-10-22_parity-hippoplayer.md`): module info, sample list, scopes, pattern view, Modland/HTTP download, stealth entries and dividers in playlists, song length detection, filter and fade, external replayers, ARexx, hotkeys. All in section 1a with parity H.

bbmp (research docs `2026-10-22_bbmp-architecture.md`, `2026-10-22_bbmp-sources.md`): catalogue = one SQLite DB per collection with a `files` table and FTS5 prefix search; format is both browse key and replayer selector; six sources behind one interface; cache, companions, prefetch, mirrors, home mirror, favourites, kept songs, history, queue. Copied: all of it (section 9). Not copied: helper processes with a JSON header and raw PCM (no command channel, so seek restarts the process), 12 Hz status polling in the UI (we are event-driven). bbmp's C helpers (`csrc/`, `open()` + `render()` ABI) are RENDER-kind replayer candidates behind a thin adapter.

nuade: the replayer contract, voice model and viz taps, see sections 4 and 6.

UP-Term: the terminal the text UI assumes on the Amiga, section 7.

Spotify / Apple Music (research doc `2026-10-22_features-modern-streaming.md`): home view with recently played and "jump back in", liked songs, queue with play-next versus add, playlist folders, radio from a seed, device hand-off (Connect), crossfade and normalisation on by default, collaborative playlists, share links, artwork-first grid, iTunes column browser and cover flow heritage, offline cache. All in section 1a; the column browser and cover flow are view styles in section 9b.

foobar2000 / Open Cubic Player / XMPlay (research doc `2026-10-22_features-pc-chiptune-era.md`): facets and album list panels (our drilldown and grid styles), ReplayGain (section 6a), DSP chain (section 5), note dots and channel views (section 6), format plugins with chip panels (section 4 `SEL_RP_CHIP_REGS`).

Terminal and web players (research doc `2026-10-22_features-terminal-and-web.md`): mpd's client/server split and musikcube's server + remote confirm section 8; Quod Libet's query language is the model for section 9.2; cmus/ncmpcpp key maps inform the text UI defaults.

## 9b. View model and view styles

The engine owns one view model: an ordered, filtered, grouped projection of a playlist, the queue or a library query (grouping keys, sort, cursor, selection, expanded nodes, visible window). UIs never build their own lists; they render the view model and send `VIEW_*` commands (section 3). Cursor and selection survive a style switch: the same song stays selected.

Styles: **regular** (flat list, sortable columns; HippoPlayer, Eagleplayer, bbmp), **tree** (hierarchy by source / folder / format / artist / album, collapsible nodes, lazy children), **drilldown** (Miller columns, iTunes column browser, foobar2000 facets: source > format > artist > album > song, or genre > artist > song; each column filters the next), **cover flow** (artwork carousel over albums/releases; needs artwork and RTG or better), **grid** (artwork tiles, Spotify/Apple Music; RTG or better, glyph tiles in text), **compact** (now playing + next few). Grouping keys come from the library model: source, format, artist, album/release, genre, year, rating, folder. Smart playlists and the queue render in every style.

A UI declares the styles it draws (`SEL_UI_VIEW_STYLES`); the shell offers the intersection with what `sel_machine` supports. Text UI: regular, tree, drilldown always; grid and cover flow when the terminal reports sixel/kitty. GadTools: regular, tree, drilldown on planar; MUI adds grid and cover flow on RTG. SDL desktop UI: all styles.

## 9c. Playlist abstraction

Three things kept apart (koobo: modern players use a higher abstraction; Spotify playlists are flat lists stored in a tree):

- **Playlist** = flat ordered list of entries, each `song key + optional subsong + optional display name`, plus divider rows (HippoPlayer dividers and stealth entries round-trip as entry kinds). A playlist has id, title, author, notes, modified time.
- **Playlist folder tree** = user-organised hierarchy of playlists (Spotify folders), stored only in the library DB.
- **Library view** = a saved query (filter + group + sort + style), never a stored list. Smart playlists are library views with a name.
- **Queue** = a playlist with special semantics: play-next inserts at the cursor, add appends, consumed rows drop, radio mode appends from a seed query.

File formats are import/export adapters only (`src/library/playlist/formats/<fmt>.c`: hippo, m3u/m3u8, pls, xspf; cue later): read produces a playlist, write serialises one; lossy fields warn. Publishing to hippoplayer.se = export Hippo + POST. Exporting a folder writes one file per playlist into a directory. Formats are specified in section 17.

## 10. Plugin ABI: tag lists, loading, versioning

Decision 2026-10-22 (koobo via Johan: "eagleplayer tag interfaces are easily expandable, this is also a major thing"): every plugin descriptor, capability list, metadata record and host query is a tag list, not an append-only struct.

```c
typedef struct sel_tag { uint32_t tag; uintptr_t value; } sel_tag;      /* AmigaOS TagItem shape: asm-friendly, 8 bytes on 68k */
#define SEL_TAG_END   0
#define SEL_TAG_MORE  1          /* value = pointer to another list (TAG_MORE) */
#define SEL_TAG_SKIP  2
uintptr_t      sel_tag_find(const sel_tag* list, uint32_t tag, uintptr_t def);
const sel_tag* sel_tag_next(const sel_tag** iter);                     /* walks END/MORE/SKIP */
```

Tag namespaces, each a block of 0x1000 values with a reserved user range (`SEL_TAG_USER` = 0x8000_0000 bit, Eagleplayer's scheme): `SEL_RP_*` replayer (section 4), `SEL_OUT_*` output, `SEL_DSP_*`, `SEL_VIZ_*`, `SEL_UI_*`, `SEL_CONV_*` converter, `SEL_SRC_*` source, `SEL_META_*` metadata values, `SEL_MACHINE_*` (section 12), `SEL_HOSTQ_*` host queries. Rules: a plugin declares only what it implements; unknown tags are skipped; the host asks for a capability by tag, never by struct size or offset; tags are never renumbered; a changed meaning gets a new tag.

The `sel_host` table stays a versioned struct (`size`, `api_version`, then function pointers called at tick rate: mem_alloc, mem_free, read, log, clock, voices, config_get) because hot calls must not walk a list. Host queries that are not hot go through `host->query(const sel_tag* ask, sel_tag* answer)` (machine capabilities, library lookups, current output). Fields in `sel_host` are appended, never reordered; the host checks `size` before touching a newer field.

One exported symbol per plugin: `const sel_tag* sel_plugin_entry(uint32_t host_api)`, returning the descriptor list (`SEL_PLUGIN_KIND`, `SEL_PLUGIN_ID`, `SEL_PLUGIN_VERSION`, `SEL_PLUGIN_TAGS` -> the kind-specific list). Loaders: static registry (all built in; default for 68000 builds and for tests), `dlopen`, `LoadLibrary`, and Amiga `LoadSeg` with the entry at the first hunk (Eagleplayer's scheme, so an Eagleplayer-style external player directory keeps working, and a native plugin can be written in asm). All loaders feed the same registry. The registry hides plugins whose `SEL_*_MIN_CPU` / `NEEDS_FLOAT` / `MEMORY` / `NEEDS_RTG` tags the probed `sel_machine` cannot satisfy.

Scripting runtime (D7): a Lua plugin exposes the command/event protocol to scripts (`selecta.send("PLAY")`, `selecta.on("SONG_OPENED", fn)`); on Amiga an ARexx port maps 1:1 onto the same commands so HippoPlayer-style ARexx scripts port by renaming the port. Both are protocol clients, no second command vocabulary.

## 11. Repository layout

```
include/selecta/        public API and SDK headers (engine.h plugin.h tags.h replayer.h output.h dsp.h viz.h ui.h host.h voices.h machine.h commands.def events.def)
src/core/               platform abstraction: mem, io, dynload, clock, log, config, net, machine probe
src/engine/             song, scheduler, voices_mixer, routing, queue, view model, events, registry, loudness apply
src/library/            catalog (SQLite), query compiler, sources/<id>.c, cache, archives, playlist/ + playlist/formats/, devilbox client, loudness analyse
src/protocol/           command/event codec, remote server, HTTP/JSON + web remote, MCP
plugins/replayers/      nuade/ openmpt/ gme/ sid/ uade-native/ (amiga) stream/ ...
plugins/outputs/        paula/ paula14/ ahi/ coreaudio/ pulse/ wasapi/ sdl/ wav/ flac/ chromecast/ airplay/ upnp/
plugins/dsp/            eq/ reverb/ stereo/ limiter/
plugins/viz/            scope/ spectrum/ vu/ pattern/ chipregs/ projectm/
plugins/ui/             text/ gadtools/ mui/ sdl/
plugins/integration/    mpris/ nowplaying/ smtc/ discord/
plugins/scripting/      lua/ arexx/ (amiga)
plugins/converters/
apps/selecta/           shell (per-platform main under apps/selecta/<os>/)
apps/selecta-cli/       headless CLI, protocol client and test harness
third_party/            nuade (submodule), sqlite, other adapted libraries; patches/
tests/                  unit (engine, protocol codec, routing, query compiler, playlist codecs), golden WAV renders per replayer
tools/                  index builder, plugin lint, release
art/                    icon set and themes
thoughts/
```

## 12. Build, language, machine scaling

- C99, `-Wall -Wextra -Werror`, no VLAs, no `alloca`. Public headers compile as C++ too (`extern "C"`).
- CMake everywhere. Amiga through a toolchain file for bebbo's `m68k-amigaos-gcc` with vasm for the few `.s` files the native UADE glue needs.
- Absolute rules in engine and plugin SDK: no floating point in any path that runs under interrupt on 68k, no dynamic allocation after `start` in engine or plugins (the SDK lint checks for `malloc` in tick/render paths), sample memory via `host->mem_alloc`.
- Each plugin builds alone against `include/selecta/` only. That is the SDK test: a plugin that needs anything else is a defect.
- Settings store (feature catalogue 2026-10-22, "preferences store" row): one `sel_prefs` key/value model in the library DB (section 9, SQLite; a flat file on the read-only 68000 tier), namespaced `global`, `ui.<plugin>`, `source.<id>`, `replayer.<id>`, `output.<id>`. Every UI edits the same keys through `PREF_GET` / `PREF_SET` commands and a `PREFS_CHANGED` event, so a value changed in the text UI shows in MUI at once. Amiga ToolTypes and the CLI are overrides parsed by the one parser (section 7, F27); replayer-specific config files (HippoPlayer PS3M config heritage) map onto `SEL_RP_CONFIG` tags (section 5, F15) and are never read by the engine directly.

Runtime machine scaling (decision 2026-10-22; Johan: "we shall not let amiga/020/aga limit us there are fast modern amigas with rtg graphics etc so rich media is possible as long as we scale the ui for the machine it runs on"). Build time decides only the ISA: `-m68020` baseline, later `-m68000` and `-m68060` builds, x86-64 and arm64 on desktop. Everything else is probed once at start into `sel_machine`, a tag list: `SEL_MACHINE_CPU` (68000/020/030/040/060, Vampire, PiStorm/Emu68, desktop), `SEL_MACHINE_CPU_SPEED` (measured loop), `SEL_MACHINE_FPU` (present, plus measured float throughput), `SEL_MACHINE_FAST_RAM`, `SEL_MACHINE_CHIP_RAM`, `SEL_MACHINE_GFX` (ECS/AGA planar, RTG via Picasso96/CyberGraphX with depth and screen size, desktop GPU), `SEL_MACHINE_AUDIO` (Paula, AHI, Toccata, desktop API), `SEL_MACHINE_NET` (bsdsocket present, proxy reachable), `SEL_MACHINE_DISK`, `SEL_MACHINE_TIER` (derived: text / planar / RTG / GPU). UI plugins, viz plugins and engine features read it and scale: artwork and truecolor visualizers on RTG or desktop, 8-colour layouts on ECS, float DSP (FFT, sinc resampler, EBU R128) only where measured fast enough, crossfade only with RAM for two instances, library index size by RAM. Plugins declare needs by tag (`SEL_RP_NEEDS_FLOAT`, `SEL_RP_MIN_CPU`, `SEL_RP_MEMORY`, `SEL_UI_NEEDS_RTG`, `SEL_VIZ_TIER`, `SEL_DSP_NEEDS_FLOAT`); the registry hides what the machine cannot run. No feature is cut from the design for the A1200 baseline; it is scaled or hidden at run time. `selecta-cli machine` prints the probe for bug reports.

## 13. Verification (reachability)

- Engine oracle: `selecta-cli render <song> --output wav` writes a WAV; a golden hash per replayer/song pair; CI diffs. The same oracle A/B tests the nuade adapter against nuade's own renderer, and is the record-to-file and loudness-analysis path, so those features are covered by the same test.
- Protocol: round-trip test for every command/event generated from the `.def` tables (one test enumerates the table, so a new entry without a codec fails).
- Routing: table-driven unit test (player kind x output capabilities -> chosen path).
- Tags: a lint walks every shipped plugin descriptor and fails on an unknown or duplicate tag; `sel_plugin_entry` of every plugin loads through the static registry in tests.
- Query compiler and playlist codecs: golden files per query and per format (Hippo round-trip keeps dividers, stealth and `#song=`).
- View model: one property test per style: style switch keeps the selected song key.
- Machine scaling: a fake `sel_machine` per tier drives the registry test; every tagged plugin appears or hides as declared.
- Amiga smoke: CI builds the 68k binary, runs `selecta-cli render` under vamos/fs-uae headless against `wav` output and compares hashes with the desktop build (same code, same answer).
- Manual: Paula-direct playback and AHI on real hardware or WinUAE, text UI over serial, RTG layouts on a Picasso96 machine.

## 14. Milestones

- M0 Skeleton: core, machine probe, tag ABI + registry, engine, nuade adapter, fixed-point mixer with resampler choice, `wav` + one desktop output, `selecta-cli render`. Golden tests, 68k cross-compile in CI. Proves the SDK shape.
- M1 Text UI on desktop: view model with regular/tree/drilldown, browse local files, playlists (Hippo, M3U, PLS, XSPF codecs), queue, subsongs, voice mutes/solo, loop detect and fade, crossfade, scope and pattern viz, module info, sample export, hotkeys, sleep timer. Proves engine->UI event model.
  - M1.1 (done except the manual terminal check I61 and the openmpt golden hash I55, 2026-10-23): shell `apps/selecta`, UI plugin ABI, text UI plugin (regular view), queue with stable ids, NEXT/PREV and auto-advance, SEEK, state snapshot, local browse, module info, RENDER-kind replayers with a libopenmpt adapter (desktop), all 121 nuade players, nuade link-cycle fix.
  - M1.2 (done except the Linux MPRIS check and the manual Control Center and terminal checks, 2026-10-23; validation `2026-10-23-m1-2-playlists-views-control.validation.md`): playlist codecs, tree and drilldown views, configurable columns and title formats, vi command mode, palette and help overlay, key bindings, queue undo/redo, reshuffle, scope and album navigation.
  - M1.3 (done 2026-10-24: automated checks green, 85 of 85; manual listening and terminal checks open; validation `2026-10-23-m1-3-viz-voices-playback.validation.md`): scopes, VU, pattern scroller, phase view, voice mute/solo, loop detect, auto-stop and fade, crossfade and gapless, sleep timer and alarm, sample export, crossfeed DSP.
- M2 Amiga: core/pal for 68k, Paula-direct voices, paula14, AHI, LED filter and stereo width, text UI in UP-Term, static plugin registry, planar tier. Proves "all players reach Paula and AHI".
- M3 Library: `sel_source` interface, SQLite catalogue, query language, local + modland + hvsc, DEViLBOX overlay (genre, ratings, votes, loudness fetch), cache/companions/prefetch, home view, liked/history, playlist folder tree, cue points, hippoplayer.se list/fetch/publish, share links, metadata panel, composer/similar/duplicates, loudness analysis on desktop.
- M3b Sources: asma, sndh, zxart, keygenmusic (same interface, one file each, no new mechanism); Modland published lists.
- M2b Replayer classes 2: zxtune, adplug, libvgm, libkss, libxmp adapters (F14 coverage doc: 102 formats with no owner in v1 of this doc). Each is one adapter plugin plus golden WAVs; no engine change.
- M3c Replayer classes 3: furnace, protrekkr, sndh adapters and the bbmp `csrc/` single-format helpers wrapped one plugin each (about 30). After M3c the only formats without an owner are the 22 bbmp itself cannot play.
- M4 Amiga UIs and native players: GadTools, MUI with RTG and planar layouts, `LoadSeg` loader, UADE-native HW replayer plugin (lifted out of UpRough), viz plugins on bitmap surfaces, chip register panels, ARexx port, OS file integration.
- M5 Remote protocol server, HTTP/JSON API + web remote + party queue, device hand-off, MCP, SDL desktop UI with grid and cover flow, GPU visualizers, DSP chain, network outputs, more replayers (openmpt, gme, sid, bbmp helpers, stream decoders), radio stations, converters, integration plugins (MPRIS, Now Playing, SMTC, Discord), Lua, skins, BPM detect.

Round 5 picks (section 1a rows F19 to F52) land in the milestone each row names; M5+ marks desktop-tier features scheduled after M5.

## 15. Risks

- 68000 and floating point: nuade's voice API is `double pitch_hz` and its mixer is float. Paula-direct avoids the mixer; period conversion happens once per note. Software-mixed output uses our fixed-point mixer; nuade's float mixer stays in the adapter for A/B only.
- Native UADE glue is wired into the demo runtime and takes the CIA vectors; lifting it into a plugin that can hand hardware back is real work, and song switching crashed as of 2026-09-21.
- Nuade is early and AI-translated; the adapter isolates us, but bugs will surface in its players and the A/B harness must stay.
- DEViLBOX API gaps (mirror proxy, loudness, genre list, artist endpoints, comments, party relay, vote parameters) are server work on another project; the library must degrade to local-only when they are missing.
- SQLite on a 2 MB Amiga: the prebuilt read-only index must be measured early (M3); the fallback is a flat sorted table behind the same query API, not a second library.
- Tag-list ABI makes it easy to add a tag without documenting it; the tag lint and the SDK header are the only places a tag may be born.

## 16. Decisions

Decided 2026-10-22: name Selecta; Amiga baseline build 68020 + 2 MB AGA/ECS, 68000 reduced build later; toolchain bebbo amiga-gcc (/opt/amiga) + CMake, vasm for `.s`; license MIT.

Mixer (implementation choice, taken): one fixed-point mixer everywhere (16.16 phase, 32-bit accumulate, int16 out), float nowhere in the engine. One code path, same golden WAVs on 68k and desktop. nuade's float mixer stays inside the adapter only for A/B tests.

Also decided 2026-10-22: macOS + text UI first, Amiga at M2 (68k cross-compile in CI from M0 so 68k constraints surface early); repository at /Users/spot/Code/MusicPlayer (git, GitHub user spotUP); v1 playlists read and publish HippoPlayer playlists through hippoplayer.se plus local M3U/PLS/XSPF/Hippo read and write.

Decided 2026-10-22 (Johan, AskUserQuestion):
- Sources: all six bbmp collections plus local, hippoplayer.se and radio; Amiga reaches HTTPS-only sites through a plain-HTTP DEViLBOX mirror endpoint; desktop talks HTTPS directly; bbmp-style home mirror setting on both. Why: no cheap TLS on a classic Amiga, and one setting serves a LAN mirror everywhere.
- Plugin ABI: tag lists for every descriptor, capability, metadata record and host query; the host table stays a versioned struct for hot calls. Why: expandable without ABI breaks, asm-friendly on Amiga, one mechanism for descriptors, metadata and queries (Eagleplayer's proven model).
- No design ceiling at 68020/AGA: features scale or hide at run time from `sel_machine`; build time decides only the ISA. Why: fast Amigas with RTG exist and the design must serve them and the desktop in full.
- Switchable view styles (regular, tree, drilldown, cover flow, grid, compact) over one engine-owned view model. Why: UIs stay dumb, selection survives a switch, every style is available to every UI that can draw it.
- Playlist abstraction: playlist, folder tree and library view kept apart; file formats are adapters. Why: Spotify-style organisation without tying the model to any file format.
- Loudness normalisation is in: per-song gain computed offline on desktop, shared via DEViLBOX, applied in the integer mixer. Why: modern players do it; the Amiga only needs to fetch a number.
- Visual tiers text / planar / RTG / GPU with plugins tagged by tier. Why: "from text to 3D" with one plugin data model.
- D5 Desktop GUI = SDL2 + own widget set in C. Why: one codebase on three OSes, GPU visualizers and skins; native look is not a goal.
- D6 Library DB = SQLite everywhere. Why: bbmp's proven model, smart playlists are SQL; the 68000 tier gets a read-only prebuilt index.
- D7 Scripting = Lua runtime plugin over the command/event protocol, plus an ARexx port on Amiga mapping 1:1 to the same commands. Why: koobo's "plugins get a runtime", and HippoPlayer ARexx scripts port by renaming the port.
- D8 Theming = design tokens in a theme file for every UI, bitmap skin loader in the SDL UI later. Why: no hardcoded colours, Winamp/AmigaAMP skins are a known want.
- Parity with HippoPlayer and bbmp is a v1 goal; nothing of theirs is out of scope. Format coverage = union of both.
- Feature picks rounds 1 to 4 recorded in section 1a. Out of scope: two-deck DJ mode. Also out of scope: HippoPlayer keyfile/registration (Selecta is MIT, there is nothing to unlock).
- Feature picks round 5 (2026-10-22, from the feature catalogue Unhomed list): F19 to F21, F23 to F25, F29 to F31, F33 to F41, F43 to F46, F48 and F49 are in (rows in section 1a). Restored into scope: multi-zone sync (F50), server transcoding (F51), multi-user / multi-library (F52), all at M5+ on the desktop tier. Not picked: CD audio (F32), rippers (F42), device sync (F47). Still out: Atmos, mpd-style client messaging, Tidal/Qobuz paid sources. Why: Johan picked them; the design is not capped by the Amiga baseline, each scales or hides by `sel_machine`.

Decided 2026-10-23 (M1.1): libopenmpt owns plain .mod, XM, S3M and IT on the desktop as a RENDER-kind replayer (`selecta_openmpt`, desktop only; the 68k build keeps nuade and the native players); native openmpt seek is accepted as not byte-exact (Johan); the 68k image with all 121 nuade players is 1.85 MB and counts fast RAM, so it fits the baseline; the engine treats `SEL_TICK_LOOPED` as song end with repeat count 1 until REPEAT and loop options exist; the text UI plugin is registered as a UI plugin and the shell `apps/selecta` loads it by `--ui`.

## 17. Playlist formats and the hippoplayer.se server

Model: section 9c. This section is the format specification (research doc `2026-10-22_playlist-formats-and-hippoplayer-server.md`).

HippoPlayer module program (`.prg`, source `~/Code/HippoPlayer-68k/code/puu016.s`: load at line 13119, header at 13523, save at 13542): header `HiPPrg\n\n`; one entry per line; an entry is an Amiga path or an `http(s)://` URL, optionally followed by `#name=<display name>` and `#song=<digit>` (subsong); divider rows are a line whose first byte is 0xF7 (stealth entry: 0xA2); at most 131071 entries. HippoPlayer also reads M3U by file extension and drops `#` comment lines. Selecta reads and writes `.prg` with dividers, stealth and `#song=` round-tripping through playlist entry kinds; URLs and Modland paths map to song keys by `path_in` of each source. Eagleplayer has no playlist format.

M3U/M3U8 (default export, `#EXTINF` with duration and title), PLS, XSPF: read and write; XSPF carries the song key in `<location>` and subsong in `<extension>`. CUE later.

hippoplayer.se (`~/Code/hippoplayer.se`), no authentication: `GET /api/v1/playlists` returns a JSON list (id, title, author, genre, filename, url); `GET /api/v1/playlists/<file>` returns the raw `.prg`; `POST /api/v1/playlists?title=&author=&genre=&filename=` with the raw `.prg` as body (max 512 KB, must start with `HiPPrg`) replies `OK <id> <filename> <url>`. Legacy `/uhcplaylists.php` returns a tab-separated Latin-1 list and stays readable. Voting on playlists lives on DEViLBOX (`/api/hip/vote`, `/api/hip/votes`); parameters unverified as of 2026-10-22, to be confirmed against the DEViLBOX source before M3.
