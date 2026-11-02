---
date: 2026-11-02
topic: MagPlaySID parity, port every SID feature MagPlaySID has that Selecta lacks
tags: [plan, sid, hvsc, transcription, viz, playlists, export, magplaysid]
status: approved
---

# MagPlaySID parity plan

Johan 2026-11-02: "this is a premium sid player that does everything we dont. write a plan to port over everything it
does that we dont: https://magnarharestad.github.io/MagPlaySID/"

Rows below are proposed (MPS-n). Nothing is in `thoughts/spot/todos.md` yet: when Johan approves, each MPS row becomes
an I row in the ledger and in its track queue under `## Now` (RULES.md rule 0, 0a) in the same edit.

## 0. What MagPlaySID is (sources read 2026-11-02)

- Page https://magnarharestad.github.io/MagPlaySID/ (v1.26, released 2026-11-02), repo
  https://github.com/MagnarHarestad/MagPlaySID (README, tree). No separate docs; release notes on GitHub releases (not read).
- C# / .NET 8, Avalonia window, PortAudio sound. Windows, macOS, Linux.
- Engine: `MagSid.Sid` is a C# translation of **reSID** (Dag Lem); `MagPlay.C64` is its own C64 for SID tunes (CIA
  timers, interrupts, raster, banked memory); `MagSid.Playback` the 6502 and .sid loader. Not libsidplayfp, not reSIDfp.
- The distinctive part: `MagPlay.Transcribe` turns any .sid into a score (`MagPlay.Score`: notes, instruments, wave /
  pulse / filter programs, patterns, order lists) using grid finding from `MagSid.Core` (shared with the MagSID editor).
  Claimed: 96.5% of register changes explained by notes and instruments on average.
- Licence: GPL-2.0-or-later.

## 1. Gap table

Status: has / partial / missing. Evidence is file:line in this repo. "unverified" = not run, only read.

| # | Feature | MagPlaySID behaviour | Selecta status | Port approach (row) |
|---|---|---|---|---|
| 1 | PSID + RSID playback | Own C64 + reSID | has: host `plugins/replayers/sid/adapter.c:153` (libsidplayfp, reSIDfp), Amiga `plugins/replayers/playsid/adapter.c:53` | none |
| 2 | Full C64 (CIA, IRQ, raster, banking), cycle-exact SID writes | own emulator | has: libsidplayfp `plugins/replayers/sid/CMakeLists.txt:8-11` | none |
| 3 | PAL / NTSC, raster or CIA timer, multi-speed | from header | has from header; partial: no user override of clock or chip model on host (`shim.cpp:44-50` sets only frequency, sampling, emulation) | MPS-1 |
| 4 | NMI digis | played as written | has (reSIDfp); partial: `digiBoost` never set for 8580 (`SidConfig.h:117`, `shim.cpp:44-50`) | MPS-1 |
| 5 | 2SID / 3SID, each chip the model its header names | yes | has on host (libsidplayfp); Amiga partial I434d (`todos.md:1332`) | none (I434d) |
| 6 | Song length by repeat detection on the C64 | clock shows time before repetition | partial: HVSC Songlengths.md5 only (`src/library/db/db.h:252`, I434f, I605); unknown tunes have no length | MPS-7 |
| 7 | Transcription of a .sid into notes, instruments, programs, patterns, orders | `MagPlay.Transcribe` | missing (no transcriber anywhere in `src/`, `plugins/`) | MPS-5 |
| 8 | First 30 s fast, full tune in background, rows never jump | yes | missing | MPS-6 |
| 9 | Scrolling rows per voice (note, instrument, effects) | yes | partial: pattern view exists for nuade/openmpt via `RP_TRACKER_INFO/CELLS` (`include/selecta/tags.def:46-47`, I88/I89, text UI I101); SID replayers answer neither | MPS-8 |
| 10 | Instrument inspector: envelope, hard restart, wave / pulse / filter program, active step lit | click a voice | missing | MPS-9 |
| 11 | Accuracy figure (share of register changes explained) | 96.5% | missing | MPS-5 (test metric) |
| 12 | Spectrum bars | meter 1 | has: `plugins/viz/spectrum` | none |
| 13 | Wireframe landscape meter (bass tallest in the middle) | meter 2 | missing | MPS-11 |
| 14 | Neon waves meter (blocks, light lines, beat glow, sparks) | meter 3 | missing | MPS-11 |
| 15 | Click meter to cycle, choice remembered | yes | partial: pref `ui.desktop.viz` (`include/selecta/prefs.def:13`), Visualizer page with 3 plugins (I269 open); no click-to-cycle (unverified) | MPS-11 |
| 16 | Scope per voice | yes | partial: Amiga playsid reSID feeds per-SID buffers (`RP_VIZ_VOICES`, I434g, inventory `2026-11-01-playsid-inventory.md:58`); host SID has no voice feed (`adapter.c` sets no voice hooks), desktop `plugins/viz/scope/scope.c:1` is post-mix stereo only | MPS-4 |
| 17 | Click a scope to mute that voice | yes | missing for SID: `MUTE`/`SOLO` exist (`include/selecta/commands.def:35-36`), only openmpt implements `RP_MUTE` (`plugins/replayers/openmpt/adapter.c:117`); libsidplayfp has `mute()` (`third_party/libsidplayfp/src/sidplayfp/sidplayfp.h:166`) unused | MPS-2, MPS-4 |
| 18 | Piano, 8 octaves, each voice's key lit in its colour by envelope strength | yes | missing (no piano viz; `plugins/viz/` has levels, scope, sidregs, spectrum) | MPS-10 |
| 19 | SID register view | not in MagPlaySID | Selecta has more: SID PatternScope `plugins/viz/sidregs/sidregs.c:1` (Amiga), host feed I434j6 open | none |
| 20 | Drop folder or tunes on the window to make a playlist | yes | partial: SDL drop handled (`plugins/ui/desktop/backend_sdl.c:31-32`); whether a drop makes a playlist is unverified | MPS-18 |
| 21 | Open folder as playlist | menu | has: `PLAYLIST_ADD_FOLDER` (commands.def) | none |
| 22 | Heart: add to own playlists, favourites | yes | has: `LIKE`, `PLAYLIST_ADD_SONG` | none |
| 23 | Drag tunes to reorder | yes | partial: `QUEUE_MOVE` exists; drag reorder in a playlist page unverified | MPS-14 |
| 24 | Per-playlist heart colour | right-click | missing (cosmetic) | MPS-14 (decision D7) |
| 25 | Show / hide playlist strip | menu | partial: sidebar exists; collapse breakpoints I253 open | none here (I253) |
| 26 | Advance when the length has played | yes | has: songlengths + `RP_HW_POLL` (I434f, I605) | none |
| 27 | Play playlist from the top | round button | has: `PLAYLIST_PLAY` | none |
| 28 | Badges: songs per tune, played length | yes | partial: subsong count reported (`SEL_META_SUBSONGS`, `adapter.c:131`); badge in rows unverified | MPS-14 |
| 29 | Sort by year, title, composer, group, file date, songs, length, recently played, most played, shuffle; restore own order | sort button | partial: library sort has 4 keys only (`SEL_UIM_LSORT_RECENTS/ADDED/ALPHA/CREATOR`, prefs.def:22-23); shuffle has (`SHUFFLE`); no song-list sort by year/group/length/plays, no "restore own order" | MPS-14 |
| 30 | Subsong buttons, playing one ringed | yes | partial: `SUBSONG` command (commands.def:32); button row in desktop/MUI unverified | MPS-13 |
| 31 | Keys 1-9, +, - pick songs | yes | partial, unverified per UI | MPS-13 |
| 32 | "Play first": user sets a tune's start song | right-click | missing | MPS-13 |
| 33 | "Play every song of a tune" before moving on | menu | missing (queue entries carry `subsong_plus1`, `src/engine/engine.c:3341`, but nothing expands a tune into its subsongs) | MPS-13 |
| 34 | Export notes as PDF sheet music (staff per voice, clef, rests, ties, accidentals, 4/4, tempo) | menu | missing | MPS-12 |
| 35 | Export notes as text (bar, beat, time per event) | menu | missing | MPS-12 |
| 36 | Volume knob, click to mute | yes | has: `VOLUME`, `MUTE` | none |
| 37 | Big font (+25%) | menu | partial: HiDPI scaling only; no user text-size pref (prefs.def has none) | MPS-16 |
| 38 | Check for updates at start (asked once) | menu | missing | MPS-17 |
| 39 | Previous / next tune | buttons, arrows | has: `PREV`, `NEXT` | none |
| 40 | Hold fast-forward: 4x, quiet, visuals keep pace | button, F held | partial: `TEMPO` + `rp_set_speed` for RENDER kind (`src/engine/engine.c:238`); host SID adapter has no `SET_SPEED`; playsid forward only in classic mode (I434e); hold gesture unverified | MPS-15 |
| 41 | Space pause, arrows prev/next | keys | has (text UI I208; desktop unverified) | none |
| 42 | File on the command line | yes | has: `OPEN` | none |
| 43 | HVSC as a source | links to hvsc.c64.org | has: `plugins/sources/hvsc/hvsc.c:1`, STIL + Songlengths (`src/library/db/user.c:744-753`) | none |

Totals: 43 features; has 14, partial 15, missing 14 (29 gaps). Counted from the table rows above, by status column.

Found while checking, not a MagPlaySID feature: the host SID shim never calls `setRoms` (`sidplayfp.h:200`;
`shim.cpp` has no call), so BASIC tunes do not run and RSID tunes that call KERNAL routines get libsidplayfp's
fallback. Folded into MPS-1 because it is the same config surface.

## 2. Architecture constraints that shape every row

- Rule 1: transcription, length detection and score data are engine/library behaviour (`src/engine/`, `src/library/`);
  UIs and viz plugins only draw it.
- Rule 2: the score reaches UIs through the existing `RP_TRACKER_INFO` / `RP_TRACKER_CELLS` contract and the existing
  pattern views, not a second "SID rows" concept.
- Rule 3/5: libsidplayfp and libresidfp stay behind `plugins/replayers/sid/`; any change to them is a patch in
  `third_party/patches/`.
- Rule 4: one feature set, scaled by `sel_machine`. Tier notes per row: 68020/AGA, 060/RTG, desktop.
- The transcriber's input is per-player-call register snapshots, not per-audio-block mirrors: `sel_viz_sid_chip` today
  is "at block close" (`include/selecta/viz.h:26`), which merges calls of multi-speed tunes. Host: step libsidplayfp
  with `play(cycles)` one player call at a time (`sidplayfp.h:113`, call period from `getCia1TimerA()` at :217 or the
  raster) and read `getSidStatus()` (:227) after each step; no patch needed. Amiga: playsid exposes a register mirror
  (inventory doc:91, `playsidbase+50`), polled per frame.

## 3. Rows

Checklist (tick here and in the ledger in the same edit):

- [x] MPS-1 Host SID configuration: chip model, clock, filter, digi boost, C64 ROMs
- [x] MPS-2 Host SID voice mute and solo
- [ ] MPS-3 (dependency only) host SID register feed = existing I434j6, not a new row
- [ ] MPS-4 Per-voice SID scopes on host, click a scope to mute
- [ ] MPS-5 SID transcriber core (score from register snapshots)
- [ ] MPS-6 Background transcription and score cache
- [ ] MPS-7 Song length by repeat detection
- [ ] MPS-8 SID score in the pattern views (scrolling rows per voice)
- [ ] MPS-9 Instrument inspector
- [ ] MPS-10 Piano viz
- [ ] MPS-11 Landscape and neon meters, click to cycle
- [ ] MPS-12 Score export: text and PDF sheet music
- [ ] MPS-13 Subsong controls: buttons, keys, Play First, Play Every Song
- [ ] MPS-14 Playlist sort keys, restore own order, badges, drag reorder (playlist colour per D7)
- [ ] MPS-15 Hold fast-forward for SID
- [ ] MPS-16 Text size option
- [ ] MPS-17 Update check at start
- [ ] MPS-18 Drop files or folder makes a playlist

### MPS-1 Host SID configuration
- Scope: prefs `sid.model` (auto, 6581, 8580), `sid.clock` (auto, pal, ntsc), `sid.filter` (on, off), `sid.digiboost`
  (auto: on for 8580); `SidConfig::defaultSidModel/forceSidModel/defaultC64Model/forceC64Model/digiBoost`
  (`SidConfig.h:97-117`) and `filter()`. C64 ROMs: load `kernal`, `basic`, `chargen` from the config dir when present
  and call `setRoms` (ROMs are copyrighted, never bundled; open-source replacement KERNAL as default is a D-question).
  Settings page rows with full English labels ("Chip Model", "Video Standard").
- Files: `include/selecta/prefs.def`, `plugins/replayers/sid/shim.cpp`, `shim.h`, `adapter.c`, `src/core/config_dir.c`
  (read only), desktop + MUI settings pages.
- Tiers: desktop only (host plugin). Amiga equivalent already exists as `playsid.mode` (prefs.def:105-108).
- Depends on: none.
- Reachability: `selecta-cli render` of a fixture .sid with `sid.model=6581` and `=8580` gives two different hashes;
  sentinel stat `sid.cfg_applied` = 1 per start; a BASIC fixture tune renders non-silent audio (RMS > 0) only with
  ROMs present.

### MPS-2 Host SID voice mute and solo
- Scope: sid adapter reports `voice_count = 3 * chips` (+1 digi channel per chip, voice 3 in `mute()`), implements
  `RP_MUTE` with `sidplayfp::mute(sid, voice, enable)`. `SOLO` goes through the engine's existing solo path.
- Files: `plugins/replayers/sid/adapter.c`, `shim.cpp`, `shim.h`.
- Tiers: desktop. Amiga: playsid inventory lists no channel mute API (unverified, inventory doc); Amiga mute stays
  missing unless playsid gains one (D9).
- Depends on: none.
- Reachability: engine test opens a real 3-voice .sid through `sel_engine` top level, sends `MUTE 1 on`; sentinel
  `rp_mute` call count = 1; rendered RMS with voice 1 muted differs from unmuted; unmute restores the golden hash.

### MPS-3 Host SID register feed
- Already ledger row I434j6 (`todos.md:1346`). MPS-4, MPS-5, MPS-10 depend on it. No new row.

### MPS-4 Per-voice SID scopes on host, click to mute
- Scope: per-voice waveforms. libsidplayfp `buffers()` (`sidplayfp.h:100`) is per chip, not per voice. Recommended
  (D3): a patch in `third_party/patches/` exposing each reSIDfp voice's output into a small ring, delivered through
  `RP_VIZ_VOICES` extended to RENDER kind (today HW kind only, `replayer.h:137`). Desktop scope viz grows a per-voice
  layout; clicking a voice scope sends `MUTE` (UI glue only).
- Files: `third_party/patches/libresidfp-voice-out.patch`, `plugins/replayers/sid/*`, `include/selecta/replayer.h`
  (comment: hook allowed for RENDER), `src/engine/engine.c` (call site), `plugins/viz/scope/scope.c`, desktop UI click.
- Tiers: desktop. Amiga reSID mode already has per-chip buffers (I434g); per-voice on Amiga is not possible through
  playsid (one buffer per SID, inventory:58).
- Depends on: MPS-2.
- Reachability: engine test with a 1SID fixture: `sel_viz_frame` holds 3 voice rows, sentinel `rp_viz_voices` count >
  0, voice 2 row is all zero after `MUTE 2 on`; desktop golden of the Visualizer page with three traces.

### MPS-5 SID transcriber core
- Scope: pure C99 integer module `src/library/sidscore/` (clean-room, MIT, D1). Input: per-call register snapshots
  (25 regs x chips) + clock. Output: per voice rows of note (frequency to note via PAL/NTSC tables), instrument
  (clustered by ADSR + wave/pulse/filter program sequence after gate on), effects (slide, vibrato, arpeggio, pulse and
  filter sweeps, hard restart), row grid (tempo from note-on spacing), patterns and orders (repeat detection over rows).
  Exposes the score through `RP_TRACKER_INFO/CELLS` from the SID adapters (host sid, playsid). Reports an "explained"
  ratio (register changes covered by note/instrument/effect) as a stat.
- Files: `src/library/sidscore/{sidscore.c,sidscore.h,CMakeLists.txt}`, `plugins/replayers/sid/*` (step per call,
  snapshot), `plugins/replayers/playsid/adapter.c` (mirror poll), tests under `tests/`.
- Tiers: core runs everywhere (integer, small state). 68020/AGA live only (MPS-6 offline pass is desktop + 060).
- Depends on: MPS-3 (I434j6) for the snapshot path idea; host stepping is new code in the shim.
- Reachability: `selecta-cli` command (e.g. `score <song>`) through the engine on 5 HVSC fixtures (Commando, a
  multi-speed tune, a 2SID tune, a digi tune, an NTSC tune); sentinel stat `sidscore.frames`; asserts explained ratio
  >= 90% averaged (MagPlaySID claims 96.5%), first 16 notes of voice 1 of Commando equal a hand-checked list.

### MPS-6 Background transcription and score cache
- Scope: a second, silent libsidplayfp instance runs the tune faster than real time: first 30 s ready first, then the
  whole tune; rows already shown never change. Result cached per md5 + subsong in the library cache
  (`src/library/cache.h`). Job runs on the library job thread, cancellable on song change.
- Files: `src/library/sidscore/`, `src/library/cache.*`, `plugins/replayers/sid/shim.cpp`.
- Tiers: desktop full; 060 measured (enable if a 3-minute tune transcribes in under its play time); 68020 live fill
  only (D2).
- Depends on: MPS-5.
- Reachability: open a fixture through the engine; sentinel `sidscore.offline_frames` reaches the 30 s mark before
  play time 5 s (desktop), whole tune done; second open reads cache (sentinel `cache_hit` = 1, transcriber frames 0).

### MPS-7 Song length by repeat detection
- Scope: during the offline pass (MPS-6) detect loop by repeated memory state hash or repeated note/order sequence;
  store as duration side data (`db.h:252`) only when HVSC Songlengths has no entry (D8).
- Files: `src/library/sidscore/`, `src/library/db/*`.
- Tiers: desktop + 060 (offline pass); Amiga 68020 uses the database only.
- Depends on: MPS-6.
- Reachability: fixture .sid removed from a fixture Songlengths; after open, the engine ends the song at the measured
  length within 1 s of the HVSC value; sentinel `sidscore.loop_found` = 1.

### MPS-8 SID score in the pattern views
- Scope: per-voice scrolling rows (note, instrument, effects) in the existing pattern views: text UI pattern panel
  (I101), desktop pattern page / Now Playing panel, MUI. Voice colours from the theme tokens, shared with piano and
  scopes.
- Files: `src/uimodel/*` (if a field is missing), desktop UI pattern page, `plugins/ui/mui/*`, text UI panel.
- Tiers: all (text and MUI on 68020; desktop).
- Depends on: MPS-5.
- Reachability: one `dui-*` golden with Commando at a fixed time showing three voice columns; sentinel: pattern view
  rows drawn > 0 for a SID song (today 0); text UI screen fixture.

### MPS-9 Instrument inspector
- Scope: select a voice: envelope (ADSR), hard restart, wave / pulse / filter programs, with the active step lit.
  Data from the score (MPS-5) through a new uimodel field; drawing per UI.
- Files: `include/selecta/uimodel*.h`, `src/uimodel/*`, desktop UI, MUI, text UI.
- Tiers: all; MUI as a list group.
- Depends on: MPS-5, MPS-8.
- Reachability: desktop golden with voice 1 selected on Commando; sentinel counts inspector frames; the lit step index
  equals the score's program position at that time.

### MPS-10 Piano viz
- Scope: new viz plugin `plugins/viz/piano/`: 8 octaves, each voice's key lit in its colour, brightness by envelope
  level (envelope estimated from ADSR + gate in the register frames; voice 3 real value from ENV3). For tracker songs
  the same plugin reads the current row's notes. Integer only.
- Files: `plugins/viz/piano/piano.c`, `plugins/CMakeLists.txt:45` list, MUI viz list (`mui_viz.c` `k_viz_ids`).
- Tiers: AGA (planar, cheap), RTG, desktop.
- Depends on: MPS-3 (host) / I434j1 (Amiga, done).
- Reachability: `viz-piano` test: frame with voice 1 freq = A4 lights key 57 in voice 1 colour; sentinel prims > 0;
  engine run with a real .sid draws > 0 lit keys per second.

### MPS-11 Landscape and neon meters, click to cycle
- Scope: two viz plugins (wireframe landscape from the spectrum history, neon waves with beat glow); clicking the
  meter cycles plugins and stores `ui.desktop.viz`.
- Files: `plugins/viz/landscape/`, `plugins/viz/neon/`, desktop UI click handler.
- Tiers: desktop and RTG; hidden on AGA by `sel_machine` (D6).
- Depends on: none (spectrum and beat data exist).
- Reachability: `dui-viz` golden per plugin; click test: three clicks return to the first plugin and the pref equals
  it after restart.

### MPS-12 Score export (CUT by D4)
- Scope: `SCORE_EXPORT` command (format `text` or `pdf`, path). Text: per voice notes, instruments, effects with bar,
  beat, time. PDF: own minimal PDF writer (MIT), staff per voice, automatic clef, rests, ties, sharps and naturals,
  4/4 bars, tempo.
- Files: `include/selecta/commands.def`, `src/library/sidscore/export_text.c`, `export_pdf.c`, desktop menu, CLI.
- Tiers: text everywhere; PDF everywhere (pure C), menu entry desktop first.
- Depends on: MPS-5 (whole-tune score: MPS-6).
- Reachability: `selecta-cli` `SCORE_EXPORT text` on Commando: file hash; `pdf`: file starts `%PDF-`, page count and
  bar count equal the score's; sentinel `export.bars`.

### MPS-13 Subsong controls
- Scope: subsong buttons (playing one marked) in desktop and MUI player; keys 1-9, +, -; "Play First" sticker per tune
  (start subsong override, read by the engine at OPEN); "Play Every Song of a Tune" pref that expands a tune into its
  subsongs in the queue (`subsong_plus1`, `engine.c:3341`).
- Files: `include/selecta/prefs.def`, `src/engine/engine.c` (queue expansion, start-song sticker), desktop + MUI.
- Tiers: all.
- Depends on: none.
- Reachability: engine test: a 3-subsong fixture with the pref on plays subsongs 1, 2, 3 then the next tune (sentinel
  SONG_STARTED events with subsong numbers); with sticker `start=2`, OPEN starts subsong 2.

### MPS-14 Playlist sort and badges
- Scope: song-list sort keys year, title, composer, group (release), file date, subsong count, length, recently played,
  most played, shuffle, and "Your Own Order" (stored order kept, never overwritten by a sort). Row badges: subsong
  count and played length. Drag reorder in playlist pages (verify; add if missing). Per-playlist colour per D7.
- Files: `src/library/query.c`, `src/library/db/*`, `include/selecta/uimodel*.h`, desktop + MUI.
- Tiers: all.
- Depends on: none.
- Reachability: library test over a fixture playlist: each key gives the expected first row; "own order" after a sort
  equals the stored order; desktop golden with badges.

### MPS-15 Hold fast-forward for SID
- Scope: host sid adapter `SET_SPEED` (tempo factor) by running `play()` for N times the cycles and decimating; engine
  quiets output while held; viz paced by played time. UI hold gesture (button + `F` held). Amiga playsid: forward only
  in classic mode (I434e rules stand).
- Files: `plugins/replayers/sid/*`, desktop UI, MUI.
- Tiers: desktop; Amiga classic mode only.
- Depends on: none.
- Reachability: engine test: `TEMPO` 4x for 2 s wall clock advances song position about 8 s (+-0.5 s); sentinel
  `rp_set_speed` count.

### MPS-16 Text size option
- Scope: desktop pref `ui.desktop.text_scale` (100, 125) applied to rows, details and labels through the theme tokens.
  MUI follows MUI prefs fonts (exempt, written reason).
- Files: `include/selecta/prefs.def`, desktop theme.
- Tiers: desktop.
- Reachability: two `dui-*` goldens at 100 and 125; row height ratio 1.25.

### MPS-17 Update check at start
- Scope: asked once (opt-in), then at start fetch the latest release tag over `include/selecta/net.h`; a toast offers
  the release page when newer. No auto install.
- Files: `include/selecta/prefs.def`, `src/library/` net job, desktop toast, MUI requester.
- Tiers: desktop and Amiga with network.
- Reachability: test against a local fixture server: newer tag gives one toast event, same tag none.

### MPS-18 Drop makes a playlist
- Scope: verify what a drop of several files or a folder does today (`backend_sdl.c:31-32`); if it only queues, create
  a playlist named after the folder (`PLAYLIST_CREATE` + `PLAYLIST_ADD_FOLDER`).
- Files: `plugins/ui/desktop/backend_sdl.c`, `desktop_ui.c`.
- Tiers: desktop (Workbench AppWindow drop on MUI later).
- Reachability: scripted drop input of a fixture folder; playlist count +1 with N songs.

### Order

1. MPS-1, MPS-2, MPS-13 (small, independent, user-visible).
2. I434j6 (exists), MPS-5, MPS-8 (the core value: see how a tune is built).
3. MPS-6, MPS-7, MPS-9, MPS-10, MPS-4.
4. MPS-14, MPS-15, MPS-11, MPS-18, MPS-16, MPS-17.

## 4. Licence check

- Selecta core is MIT (`thoughts/shared/plans/2026-10-22-architecture-design.md`, header). GPL code builds only as
  loadable modules under `SELECTA_GPL_PLUGINS` (`CMakeLists.txt:32-33`; `plugins/replayers/sid/CMakeLists.txt:1-2`).
- MagPlaySID is GPL-2.0-or-later and C#. Nothing can be linked; translating its C# (transcriber, grid finding, score
  player, meters) into C makes a derived work under GPL, which then could only live in a GPL module, never in the core,
  the MUI build or the Amiga playsid path.
- Therefore: reimplement clean-room from the published behaviour (page, README), not from its source (D1). Agents
  implementing MPS-5/6/7/9/12 must not read MagPlaySID's `src/`.
- Reusable as-is: libsidplayfp + libresidfp (GPL-2, already vendored as a GPL module); the MPS-4 patch is to GPL code and
  stays in that module. reSID (Dag Lem, GPL) adds nothing over reSIDfp.
- C64 ROMs (KERNAL, BASIC, chargen) are copyrighted: user supplied only, never committed or packaged.
- The PDF writer and meters are new MIT code.

## 5. Decisions (Johan, 2026-11-02)

- D1 Transcriber: clean-room C in `src/library/sidscore/`, under Selecta's licence. Agents never read MagPlaySID's src/.
- D2 68020/AGA: live transcription only, rows fill as the tune plays; offline pass and cache on desktop and 060.
- D3 Desktop voice scopes: patch libresidfp to expose each voice's output (true waveforms, digis and filter included).
- D4 Sheet music export: skipped. MPS-12 is cut (no text or PDF score export).
- D5 Update check: opt-in, asked once at first start, toast only.
- D6 Landscape and neon meters: every tier, including a reduced AGA version (MPS-11 covers AGA too).
- D7 Per-playlist heart colour: skipped.
- D8 Song length: HVSC Songlengths wins; measured length only when HVSC has no entry.
- D9 Amiga SID voice mute: wait for playsid.library to offer a mute call; ask its maintainer. No register-masking hack.
  REVERSED by Johan 2026-11-02 (after MPS-2): "Add mute", Amiga SID mute now. Row MPS-19: Opus design pass first (how playsid.library is driven, where voices can be silenced, 68020 cost), then build.
- D10 C64 ROMs: bundle an open replacement KERNAL (MPS-1 checks its licence before shipping; BASIC tunes that need the
  real BASIC ROM may still fail, reported as such). Johan 2026-11-02: Settings lets users load their real KERNAL, BASIC and character ROMs (row MPS-1c).
