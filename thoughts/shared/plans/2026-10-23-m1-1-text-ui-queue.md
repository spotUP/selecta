---
date: 2026-10-23
topic: M1.1 implementation plan: text UI shell, queue, browse, seek, module info, libopenmpt adapter, all nuade players
tags: [plan, m1, text-ui, queue, openmpt, nuade]
status: approved
rows: I45 to I62 in thoughts/spot/todos.md
---

# M1.1: text UI shell, queue, local browse, seek, module info, libopenmpt, nuade players

Implements design doc `thoughts/shared/plans/2026-10-22-architecture-design.md` section 3 (queue identity, NEXT/PREV, SEEK, SEEKED, state snapshot), section 4 (RENDER-kind replayer, metadata tags), section 7 (UI plugin tag list, shell main loop, text UI over a cell grid with diff flush), section 9b (view model, regular style only) and section 14 (first slice of M1). RULES.md rules 1 (engine is the single source of truth), 3 (plugins see `include/selecta/` only), 4 (no float in engine or interrupt path, no allocation after `start`), 5 (adapt, never fork) and 6 (golden WAVs) are the contract. Research input: `thoughts/shared/research/2026-10-23_m1-codebase-gap.md` (R9) and `thoughts/shared/research/2026-10-23_m1-replayer-reach.md` (R8). M0b (`2026-10-22-m0b-replay.md`, done) is the base.

## M1 split (all of design section 14 M1 stays in scope)

- **M1.1 (this plan):** shell `apps/selecta`, UI plugin ABI, text UI plugin (regular view), engine queue with stable ids, NEXT/PREV and auto-advance, SEEK, state snapshot with song and position, local file browse, module info panel, RENDER-kind replayers with a libopenmpt adapter (plain M.K. .mod, XM, S3M, IT on desktop), all 121 nuade players, nuade link-cycle fix.
- **M1.2:** playlist codecs (Hippo, M3U, PLS, XSPF), tree and drilldown views, configurable columns and title formats (F48), vi command mode (F45), palette and help overlay (F49), user key bindings (F11 part), queue undo/redo, reshuffle, scope and album navigation (F30).
- **M1.3:** scopes, VU, pattern scroller, phase view (F17), voice mute/solo, loop detect, auto-stop and fade, crossfade and gapless, sleep timer and alarm, sample export, crossfeed DSP (F18).

M1.2 and M1.3 get their own plan files when M1.1 is done; ledger row L4 tracks writing them.

## Finish line

`cmake --build build/host && nice -n 19 ctest --test-dir build/host -j2` green on macOS and `cmake --build build/amiga` green, with T1 to T18 still green and these new tests:

| ID | Test (ctest name) | Entry point | Sentinel (proves new code ran) | Spec number asserted |
|----|-------------------|-------------|--------------------------------|----------------------|
| T19 | `selecta-browse-play` (the M1.1 reachability test) | `selecta --output null --keys tests/m1/browse_play.keys --screen-dump <tmp>.txt` | on exit the shell prints `ui_waits=W flushes=F keys=K` (shell and grid counters) and `songs_opened=S queue_version=Q` (engine counters) | keys browse into `build/host/tests/songs/`, move to `selecta-test-mk.mod`, press Enter, wait 2000 ms, press `i`, quit. `S=1`; `K` equals the key count of the keys file (CMake counts it); last dumped screen equals `tests/m1/browse_play.screen` (header shows `Playing`, title `selecta-test`, `00:02 / 00:10`; info panel shows `Format`, `Channels 4`, `Patterns`, `Samples`); `Q=1` |
| T20 | `queue-engine` | `selecta-cli script tests/m1/queue.sel` | `.expected` lines carry the `QUEUE_CHANGED` versions | ids are 1, 2, 3 in add order and never reused after `QUEUE_REMOVE 2`; `QUEUE_PLAY_NEXT` inserts after the playing entry; `NEXT` past the end emits `STATE stopped`; `PREV` within the first 3000 ms of a song goes to the previous entry, after it restarts the song; SONG_ENDED auto-advances and emits `SONG_OPENED` for entry 2; every edit raises the version by exactly 1 |
| T21 | `seek-render` | `selecta-cli render <song> --seek-ms 5000 --seconds 5` for the nuade song and the openmpt song | prints `seek_path=native` (openmpt) or `seek_path=skip skipped_frames=220500` (nuade) | the 5 s WAV after the seek equals bytes 882000 onward of the 10 s golden render (same resampler), byte for byte, for both replayers; `SEEKED 220500` in the event log |
| T22 | `render-golden-openmpt` | `selecta-cli render build/host/tests/songs/selecta-test-mk.mod --output <tmp>.wav --seconds 10 --rate 44100` | prints `replayer=openmpt render_calls=R rendered_frames=441000` | `rendered_frames=441000`; WAV data chunk 1764000 bytes; SHA-256 equals `tests/golden/openmpt/selecta-test-mk.sha256` (committed only after the listening check, row I55) |
| T23 | `nuade-players` | `selecta-cli plugins --formats nuade` | prints `nuade_players=N` from the adapter's registry walk | `N` equals the count of `third_party/nuade/players/*/nuade_player.cmake` (CMake globs it at test time; 121 on today's tree); `pokeynoise`, `davidhanney`, `majortom` listed |
| T24 | `view-unit` | `tests/view/view_test` (unit, behaviour where it lives) | n/a | directory rows sort directories first then case-insensitive name; cursor keeps the same entry id across a queue edit; `VIEW_ACTIVATE` on a directory row lists it, on a file row sends `QUEUE_ADD` + `PLAY` of the new id; `..` row present except at the filesystem root |
| T25 | `grid-unit` | `tests/grid/grid_test` (unit) | n/a | a one-cell change on an 80x24 grid flushes one cursor move + one cell (byte vector in `tests/grid/vectors.h`); an unchanged frame flushes 0 bytes; a full redraw is wrapped in synchronized output `CSI ? 2026 h` / `CSI ? 2026 l` |
| T26 | `no-alloc-render` (extended) | M0a shim over `selecta-cli render` for the openmpt song and over a queue auto-advance script | shim counts `malloc` between the engine `START` marker and end of render, on the audio thread only | zero allocations on the audio thread; song opens happen on the pump thread (the shim logs the thread of each call) |
| T27 | `nuade-link-graph` (configure time) | `plugins/replayers/nuade/CMakeLists.txt` | `message(FATAL_ERROR)` when `selecta_nuade`'s `LINK_LIBRARIES` contains `sel_plugin_nuade` | fails on today's tree (cycle at lines 43 and 47), passes after I46; the host link prints no `ignoring duplicate libraries` line (checked by T27b `link-warnings`, which greps the log of a forced relink of `selecta-cli`) |
| T8 | `ratchet` (lowered) | `selecta-cli commands --unimplemented` | as M0a | count <= 45 (54 minus NEXT, PREV, SEEK, QUEUE_ADD, QUEUE_REMOVE, QUEUE_CLEAR, QUEUE_PLAY_NEXT, LIBRARY_BROWSE, VIEW_STYLE); the four new VIEW verbs are implemented when born |

Gate 2 (red first): every T row is written and seen failing before the code it checks exists; each ledger row names the T rows that went red then green.

## Decisions taken in this plan (not open)

- **Plain M.K. .mod is played by a libopenmpt adapter** (user decision 2026-10-23, AskUserQuestion). R8 established that no nuade player probes `M.K.`: the only 0x438 readers are `protracker4` (IFF MODL only, `protracker4.c:256-259`) and `audiosculpture` (`FLT4`/`EXO4` with a companion bank); other hits are period tables. The openmpt adapter moves from M5 to M1.1, desktop only. The 68k build of libopenmpt (C++17) is not attempted; the plugin is not added on `SEL_AMIGA`. On the Amiga, plain .mod reaches Paula in M2 through a nuade or UADE-native player (ledger row D-M2-2).
- **libopenmpt source.** Git submodule `third_party/libopenmpt` from `https://github.com/OpenMPT/openmpt`, pinned at tag `libopenmpt-0.8.4` (the version Homebrew has on this machine; pin by commit SHA resolved with `gh api` at I50). Built by our CMake as static target `selecta_openmpt` (C++17, upstream flags) from the `LIBOPENMPT_CXX_SOURCES` and `COMMON_CXX_SOURCES` lists of upstream's root `Makefile` (the list is read at configure time by `cmake/openmpt_sources.cmake`, not copied), with zlib, mpg123, ogg and vorbis off (`MPT_WITH_*` undefined), so MO3 with compressed samples is reported unsupported. The system Homebrew copy is not used: golden hashes must not depend on what a machine has installed.
- **RENDER-kind replayer ABI.** New tags in `tags.def`: `RP_RENDER (0x100E, PTR)`, `RP_SEEK (0x100F, PTR)`, `RP_POSITION (0x1010, PTR)`. In `replayer.h`:

  ```c
  /* RP_RENDER: interleaved stereo int16 at the rate given to RP_START;
   * returns frames written, < frames only at song end. RENDER kind only. */
  typedef uint32_t (*sel_rp_render_fn)(void* state, int16_t* out, uint32_t frames);
  /* RP_SEEK: optional. Returns 1 when the replayer seeked natively. */
  typedef int (*sel_rp_seek_fn)(void* state, uint32_t ms);
  /* RP_POSITION: optional. Fills order/row/pattern; 0 = not reported. */
  typedef struct sel_rp_position { uint32_t order, row, pattern; } sel_rp_position;
  typedef int (*sel_rp_position_fn)(void* state, sel_rp_position* out);
  ```

  `RP_START` gets `voices = NULL` for RENDER kind. The engine render path calls `RP_RENDER` straight into the output buffer and skips the scheduler and mixer. Float inside libopenmpt runs on the desktop audio thread, not under an interrupt and not in engine code, so rule 4 holds; the adapter converts nothing (libopenmpt has an int16 read).
- **Seek.** `SEEK` takes milliseconds (`"s"` argument parsed as `ms`, `+ms`, `-ms`; pattern:order:row form is M1.3). Engine order: `RP_SEEK` when present and it returns 1 (`seek_path=native`); otherwise restart the song (`RP_START` same subsong) and run the scheduler without mixing until the target frame (`seek_path=skip`), which is exact because the VOICE path is deterministic. The skip runs on the pump thread on a fresh replayer instance and is swapped in like a song change (next bullet). `SEEKED frame` is emitted after the swap.
- **Song changes never touch the audio thread's replayer.** The engine keeps two replayer slots. OPEN, auto-advance and seek prepare the idle slot on the pump thread (file read, `RP_OPEN`, `RP_START`, all allocation), then publish it through the existing command SPSC (`SWAP slot`); the audio thread switches at the next buffer boundary and returns the old slot through the event SPSC; the pump thread calls `RP_CLOSE`. This is the base the M1.3 crossfade reuses. SONG_ENDED from the audio thread makes the pump thread advance the queue.
- **Queue.** `src/engine/queue.c`, owned by the engine, fixed capacity allocated at `sel_engine_start`: `queue_capacity` entries and a `queue_text_bytes` path arena, both in `sel_engine_config` with defaults scaled from `sel_machine` fast RAM (>= 64 MB: 8192 entries, 2 MB; >= 8 MB: 2048, 256 KB; else 256, 32 KB). Entry = `{ uint32_t id; uint32_t subsong_plus1; uint32_t path_offset; }`. Ids start at 1, increase, never reused within a session. A full queue answers `ERROR queue_full`. Remove compacts the arena in place. `QUEUE_ADD path`, `QUEUE_PLAY_NEXT path`, `QUEUE_REMOVE id`, `QUEUE_CLEAR`, `NEXT`, `PREV` (restart when position >= 3000 ms, previous entry otherwise). `QUEUE_CHANGED version` on every edit.
- **Snapshot.** `sel_state` grows (appended fields only):

  ```c
  typedef struct sel_song_state {
      uint32_t queue_id;            /* 0 = opened outside the queue */
      uint32_t subsong, subsong_count;
      uint32_t duration_ms;         /* 0 = unknown */
      uint32_t position_ms;
      sel_rp_position pattern_pos;  /* zeros when not reported */
      const sel_tag* meta;          /* SEL_META_* list, valid until next pump */
      const char* path;
  } sel_song_state;
  /* appended to sel_state: */
  sel_song_state song;
  const sel_queue_entry* queue; uint32_t queue_count, queue_playing, queue_version;
  const sel_view_state* view;
  ```
- **Metadata.** The nuade adapter adds `META_CHANNELS`, `META_SUBSONGS`, `META_PATTERNS`, `META_INSTRUMENTS` and `META_SAMPLES` where nuade's `tracker_info` reports them, and `META_SIZE` always; `META_DURATION_MS` only when nuade reports a length. The openmpt adapter reports TITLE, AUTHOR (`artist`), FORMAT (`type_long`), MESSAGE, CHANNELS, PATTERNS, INSTRUMENTS, SAMPLES, SUBSONGS, DURATION_MS (`get_duration_seconds` converted once at open, the only float conversion), SIZE. The info panel lists every tag the song has, in `tags.def` order; it has no fixed field list.
- **nuade players.** `NUADE_PLAYERS` is set to the glob of `players/*/nuade_player.cmake`: all 121 players, no exclusion list. R8 flagged `pokeynoise`, `davidhanney` and `majortom` because their data files carry a note about UADE Eagleplayer data; the user confirmed on 2026-10-23 that the whole nuade package, these players included, is by the same author under one licence, so the flag is withdrawn. The 32 players with float are fine on desktop; their Amiga interrupt question stays D-M2-1. The design doc says 119 nuade formats while the tree has 121 players; I53 reconciles the number in the design doc from the registry count.
- **nuade link cycle.** Root cause: the players call `nuade_voice_*` defined in the plugin's `voices_shim.c`, and the plugin calls the players, so the two archives depend on each other (`plugins/replayers/nuade/CMakeLists.txt:43,47`, multiplicity 3 at :46). Fix: `voices_shim.c` and `io_shim.c` move into a third static target `sel_nuade_shim` (still our code, still compiled against `include/selecta/` plus nuade's public headers, T7 unchanged). Link order is plugin -> `selecta_nuade` -> `sel_nuade_shim`, a DAG; lines 46 and 47 go. The shim calls `sel_voices` through the pointer table it receives, never back into the plugin, so no cycle remains.
- **UI plugin ABI.** New `include/selecta/ui.h` and tags `UI_NAME (0x5000, STR)`, `UI_OPEN (0x5001, PTR)`, `UI_ON_EVENT (0x5002, PTR)`, `UI_WAIT (0x5003, PTR)`, `UI_CLOSE (0x5004, PTR)`, `UI_VIEW_STYLES (0x5005, INT)`, `UI_NEEDS_RTG (0x5006, INT)`, `UI_HOTKEYS (0x5007, PTR)`, `UI_STATS (0x5008, PTR)`. `UI_OPEN (const sel_ui_open_args*)` where the args struct carries `sel_engine*`, `const sel_host*`, `argc/argv` of UI options, and a reserved `void* library` (NULL until M3 creates `sel_library`; section 7's signature is kept). `UI_WAIT(state, uint32_t timeout_ms) -> int` returns 0 on timeout, 1 on input handled, -1 on quit. `UI_SURFACES` is M1.3 with viz.
- **Waking the UI.** No syscall on the audio thread. The shell calls `UI_WAIT(state, 20)`; the text UI's POSIX backend uses `poll()` on the input fd with that timeout. 20 ms equals one 50 Hz tick, so position updates redraw at most 50 times a second and the engine needs no wake primitive in M1.1. On the Amiga (M2) the same contract maps to `Wait()` on the console signal plus a `timer.device` signal.
- **Local browse.** `LIBRARY_BROWSE path` is implemented in `src/library/local.c` (new `selecta_library` target, portable) over a directory iterator in `src/core/<os>/dir.c` (POSIX `opendir` on macOS and Linux; Amiga `Examine/ExNext` in M2). Listing goes into a fixed-size browse buffer allocated at start (`browse_entries`, `browse_text_bytes`, scaled like the queue); an overflowing directory is truncated with `LOG browse_truncated n`. Event `LIBRARY_RESULT` carries `{path, count}` as tags. This is not the M3 `sel_source` interface; M3 wraps `local.c` as the `local` source without changing its listing code.
- **View model (regular style).** `src/engine/view.c`: one view over either the queue or the browse listing. Design section 9b puts cursor and visible window in the engine view model, and section 3's VIEW verbs (STYLE, GROUP, EXPAND, COLLAPSE, DRILL) have no verb for source, cursor, activation or window height. Four verbs are added to `commands.def` in phase 4, born implemented, and design section 3 is extended in I62: `VIEW_SHOW "s"` (`queue` or `browse`), `VIEW_CURSOR "s"` (`n`, `+n`, `-n`, `top`, `bottom`), `VIEW_ACTIVATE ""` (directory: browse into it; `..`: parent; file: `QUEUE_ADD` + `PLAY` that id; queue row: play it), `VIEW_WINDOW "u"` (visible row count; the engine keeps the window start so the cursor stays visible). Existing `VIEW_STYLE regular` is implemented in the same row (other styles answer `ERROR unsupported_style` until M1.2). Each row = `{ kind (dir, file, queue), id, depth 0, text }` where `text` is the file name (browse) or META_TITLE else file name (queue); title formats are M1.2 (F48). `VIEW_CHANGED` carries style and row count; `VIEW_ROWS` is emitted with the visible window after every change. Ratchet: VIEW_STYLE removed from the unimplemented list (T8 count <= 45 at I58).
- **Text UI.** Plugin `plugins/ui/text/`: `text_ui.c` (layout, key map, drawing from the snapshot), `grid.c` (cell grid, diff flush, synchronized output), `keys.c` (byte stream to key codes: arrows, Enter, Backspace, Tab, printable), portable; `plugins/ui/text/posix/term.c` (termios raw mode, alternate screen, `poll`, SIGWINCH size). Layout at 80x24 and up: header line 1 now playing (state, title, author), line 2 format, subsong `i / n`, time `mm:ss / mm:ss`, volume; main pane the view rows with the cursor row in reverse video; optional right info panel (toggled with `i`, full META list); bottom status line with key hints in full words (`Enter Play`, `Tab Queue`, `Space Pause`, `i Info`, `q Quit`). Default keys: Up/Down or j/k cursor, PgUp/PgDn, Enter activate, Backspace parent, Tab switch browse/queue, Space pause, s stop, n next, p previous, Left/Right seek -5000/+5000, `[`/`]` subsong, `+`/`-` volume, a add to queue, d remove from queue, i info, q quit. On start and on every SIGWINCH the UI sends `VIEW_WINDOW <main pane rows>`. Colours come from a token table in `plugins/ui/text/theme.c` (D8); no colour literal elsewhere in the plugin. Glyphs: ASCII and box drawing only (RULES.md rule 8).
- **Shell.** `apps/selecta/main.c`: parses `--output name` (default `coreaudio` on macOS, `null` elsewhere), `--ui name` (default `text`), `--keys file`, `--screen-dump file`, `--start-dir path` (default current directory), registry and engine start, opens the UI, loop: `UI_WAIT(20)`; `sel_engine_pump`; for each event `UI_ON_EVENT`; until `UI_WAIT` returns -1. With the `null` output the shell pumps the null sink by exactly 20 ms of frames per loop turn, so tests are deterministic. `--keys` feeds the file's bytes (with `wait <ms>` lines expanded to loop turns) into the same `keys.c` decoder instead of the terminal; `--screen-dump` writes the grid as UTF-8 text after each flush (last frame kept). Both flags are test entry points into the real UI code, not a second UI.
- **Amiga.** `apps/selecta`, the text UI plugin and the openmpt plugin are not built under `SEL_AMIGA` in M1.1 (text UI on Amiga is M2 via UP-Term). The engine, queue, view and library code is built for 68k and covered by T18 (no-float), extended to `queue.o`, `view.o`, `local.o`.

## File layout created or changed by this plan

```
include/selecta/
  ui.h                      new: UI hook typedefs, sel_ui_open_args
  replayer.h                RP_RENDER, RP_SEEK, RP_POSITION typedefs
  engine.h                  sel_song_state, queue/view fields appended, config fields
  tags.def                  RP_RENDER..RP_POSITION, UI_* rows
  commands.def              VIEW_SHOW, VIEW_CURSOR, VIEW_ACTIVATE, VIEW_WINDOW
src/engine/
  engine.c                  slots + SWAP, queue/next/prev/seek verbs, snapshot
  queue.c, queue.h          new
  view.c, view.h            new
src/library/
  CMakeLists.txt, local.c, local.h   new (selecta_library)
src/core/macos/dir.c, src/core/linux/dir.c   new (POSIX, one file shared via CMake)
plugins/replayers/nuade/
  CMakeLists.txt            sel_nuade_shim target, player glob, T27 check
  adapter.c                 metadata tags, registry count for T23
plugins/replayers/openmpt/
  CMakeLists.txt, adapter.c new
plugins/ui/text/
  CMakeLists.txt, text_ui.c, grid.c, grid.h, keys.c, keys.h, theme.c, posix/term.c   new
apps/selecta/
  CMakeLists.txt, main.c    new
apps/selecta-cli/           render --seek-ms; plugins --formats
cmake/openmpt_sources.cmake new
third_party/libopenmpt      new submodule
tests/m1/                   browse_play.keys, browse_play.screen, queue.sel, queue.expected, seek_render.cmake
tests/view/, tests/grid/    new unit tests
tests/golden/openmpt/       selecta-test-mk.sha256
tools/gen_testmod.c         --format mk (plain M.K. variant of the same song; pinned hash)
```

## Phase 1: ABI, tags, link fix (rows I45 to I47)

Tag rows, `ui.h`, `replayer.h` additions; T27 red, then `sel_nuade_shim` split, T27 green; T2, T5, T7 stay green. No `commands.def` rows in this phase: a verb born without dispatch would raise the ratchet, so the new VIEW verbs land in phase 4 together with their dispatch.

## Phase 2: engine queue, slots, NEXT/PREV, seek, snapshot (rows I48 to I52)

T20, T21 (nuade half) red; replayer slots and SWAP; queue.c; NEXT/PREV/auto-advance; restart-and-skip seek and SEEKED; snapshot fields; ratchet to 47 (SEEK, NEXT, PREV, four QUEUE verbs); T20, T21 nuade half green; T13, T14, T17 stay green.

## Phase 3: replayers (rows I53 to I56)

nuade metadata tags and all-player glob (T23 red then green, design doc count reconciled); libopenmpt submodule and `selecta_openmpt`; `gen_testmod --format mk`; openmpt adapter (probe via `openmpt_probe_file_header`, RENDER, SEEK via `set_position_seconds` with the ms value converted once, POSITION via `get_current_order/row/pattern`, metadata); T22 counters green, hash committed after the listening check; T21 openmpt half and T26 green.

## Phase 4: library browse and view model (rows I57 to I58)

T24 red; `selecta_library` + `dir.c`; LIBRARY_BROWSE (ratchet to 46); `view.c` with VIEW_SHOW, VIEW_CURSOR, VIEW_ACTIVATE, VIEW_WINDOW born implemented and VIEW_STYLE regular (ratchet to 45); T24 green.

## Phase 5: text UI and shell (rows I59 to I61)

T25 red; `grid.c`, `keys.c`, `term.c`, `theme.c`, `text_ui.c`; T25 green; `apps/selecta` shell; T19 red then green; manual check in a real terminal (Blocked on me).

## Phase 6: docs, ledger, validation (row I62)

RULES.md Commands rows for `selecta` (the "not yet implemented" row becomes real), openmpt render, the new tests; README; design doc section 3 (four VIEW verbs, RP_RENDER/SEEK/POSITION), section 5 (nuade count from the registry), section 14 (M1 split, openmpt in M1.1) and section 16 (decisions of 2026-10-23); validation doc `2026-10-23-m1-1-text-ui-queue.validation.md`; plan status done; open-row count reconciled.

## Ledger rows (thoughts/spot/todos.md, section "Implementation M1.1")

| ID | Phase | Task | Proof |
|----|-------|------|-------|
| I45 | 1 | tags.def RP_RENDER/SEEK/POSITION and UI_* rows; ui.h; replayer.h typedefs | T2, T5 green |
| I46 | 1 | T27 written and red; sel_nuade_shim split; lines 46-47 removed | T27, T27b green; T13 hash unchanged |
| I47 | 1 | 68k build after the link change | build/amiga green, T14 green |
| I48 | 2 | T20 and T21 (nuade) written and red | ctest failures |
| I49 | 2 | two replayer slots, SWAP through the SPSC, close on pump thread | T13, T14, T17 green |
| I50 | 2 | queue.c, QUEUE_ADD/REMOVE/CLEAR/PLAY_NEXT, NEXT/PREV, auto-advance, QUEUE_CHANGED | T20 green, ratchet 48 |
| I51 | 2 | SEEK restart-and-skip, SEEKED, render --seek-ms | T21 nuade half green, ratchet 47 |
| I52 | 2 | sel_state song/queue fields; T18 over queue.o | T18 green, abi goldens updated (T6) |
| I53 | 3 | nuade metadata tags; glob of all players; T23 red then green; design doc player count | T23 green |
| I54 | 3 | libopenmpt submodule at libopenmpt-0.8.4 SHA; cmake/openmpt_sources.cmake; selecta_openmpt builds | host build green |
| I55 | 3 | gen_testmod --format mk; openmpt adapter; T22 red then counters green; hash committed after listening check | T22 green |
| I56 | 3 | RP_SEEK in engine; T21 openmpt half; T26 extended | T21, T26 green, seek-render-openmpt-break green |
| I57 | 4 | T24 red; selecta_library, dir.c, LIBRARY_BROWSE, LIBRARY_RESULT | ratchet 46 |
| I58 | 4 | view.c; VIEW_SHOW, VIEW_CURSOR, VIEW_ACTIVATE, VIEW_WINDOW born implemented; VIEW_STYLE regular; VIEW_CHANGED, VIEW_ROWS | T24 green, ratchet 45 |
| I59 | 5 | T25 red; grid.c, keys.c, theme.c, posix/term.c | T25 green |
| I60 | 5 | text_ui.c; apps/selecta shell; T19 red then green | T19 green |
| I61 | 5 | real-terminal check on macOS (Verification, manual 1); the agent never ticks it | user confirms |
| I62 | 6 | RULES.md Commands, README, design doc 14 and 16, validation doc, plan status done | counts reconciled |

Also: L4 (write M1.2 and M1.3 plans after I62), D-M2-2 (plain .mod on Amiga: nuade M.K. player upstream or UADE native; decide at M2 planning).

## Verification

Automated: the finish-line table, run per phase on the affected tests only (`nice -n 19 ctest --test-dir build/host -R '<names>'`), the full CI run once at I62 (reason: landing the milestone).

Manual (only the user can confirm, never ticked by the agent):
1. LOCAL, macOS Terminal: `build/host/apps/selecta/selecta --start-dir ~/Music/mods` (any folder of .mod/.xm files). PASS: the browser lists the folder, Enter on a plain .mod plays sound through the default output, the header time advances, Left/Right seek audibly, `n` plays the next queued song, `i` shows the module info, `q` restores the terminal (no stray characters, cursor visible).
2. Listening check of `selecta-test-mk.mod` through openmpt before I55 commits the hash (the same song as the nuade golden; it should sound the same, minus the Amiga filter).

## Out of scope for M1.1 (goes to M1.2, M1.3 or later)

Everything listed under M1.2 and M1.3 above; `sel_source` and SQLite (M3); openmpt on 68k; UI switching at runtime (`UI_SWITCH` stays unimplemented until a second UI exists); text UI on the Amiga (M2); sixel/kitty graphics.

## Deviations

- I46: T27b `link-warnings` runs `tests/link_warnings.cmake` (new, next to `ratchet.cmake`; the file layout list did not name it) and is `RUN_SERIAL` because it deletes and relinks `selecta-cli`. The shim split also adds `sel_sdkcheck_nuade_shim` under `sdk-isolation`, so T7 still covers `voices_shim.c` and `io_shim.c` now that they left `sel_add_plugin`.
- I45: `ui.h` hook typedefs take `void* state` and `const sel_message*` (UI_ON_EVENT); the plan fixed only `sel_ui_open_args` and the UI_WAIT return codes.
- I48: T20 needs script-only directives that do not exist yet and that the plan did not name: `@queue` (prints `QUEUE version=V playing=ID ids=...` from the snapshot), `@render N` and `@render_until_end` (render on the audio side, then pump; the latter hides POSITION). The test song `selecta-test.mod` never reports SONG_ENDED (it loops past 100 s), so auto-advance in `queue.sel` needs a song that ends; I50 must supply one (an ending variant from `tools/gen_testmod.c`) and may adjust `queue.expected` (SONG_OPENED tag list, restart event `SEEKED 0`) when it goes green. `queue-engine` runs with WORKING_DIRECTORY `build/host/tests/songs`. `seek_render.cmake` sits in `tests/m1/` and is registered in `tests/CMakeLists.txt`, not a new `tests/m1/CMakeLists.txt`.
- I49: SWAP and the slot return do not use the existing command and event SPSCs: those have the pump thread as consumer and as producer, while a swap needs the audio thread on one end. The engine owns two more `sel_spsc` rings of the same type (`swap_q` pump to audio, `ret_q` audio to pump, depth 4) carrying internal message ids `SEL_RING_SWAP`/`SEL_RING_RETURN` (engine.c constants, not commands.def entries). The audio thread switches at the top of `sel_engine_render` and pushes the old slot index to `ret_q`; the pump thread calls RP_CLOSE when it drains `ret_q`. SUBSONG now re-prepares the same file into the idle slot (file re-read, RP_OPEN, RP_START) instead of restarting live audio-owned state, so it is a swap too. When both slots are busy (old slot not yet returned, e.g. no audio thread running) OPEN/SUBSONG is deferred and retried on each pump; the latest request wins. `sel_engine_stats_get` reads the slot the audio thread mixes.
- I50: (1) The engine treats a tick flagged `SEL_TICK_LOOPED` like `SEL_TICK_ENDED` (repeat count 1, as the nuade core does): protracker4 never reports ENDED, only LOOPED, so no module ever produced SONG_ENDED and auto-advance could not fire. REPEAT and the loop options will make this configurable. (2) `sel_engine_config` gained `queue_capacity`, `queue_text_bytes` and `machine` (the `sel_machine_probe` tag list; the front end probes, the engine does not; NULL gives 256 entries and 32 KB). Only `selecta-cli script` passes it; `session.c` stays on the smallest defaults to avoid the 100 ms probe per render/play. The queue is allocated in `sel_engine_create` because there is no separate engine start. (3) Four read-only accessors (`sel_engine_queue_version/count/playing_id/id_at`) back `@queue`; I52 folds them into the snapshot. (4) T20 script and expected changed: the directives `@queue`, `@render`, `@render_until_end` exist in `cmd_script`; a one-frame `@render 1` sits between song changes because OPEN-style changes defer (latest wins) while the audio side has not taken the published slot, and the script has no audio thread; the final section plays the new `selecta-short.mod` (`gen_testmod --short`, title "selecta short", one pass) because selecta-test.mod is 30 s; PREV restart emits `SEEKED 0` then `POSITION 0 0 0 0` since the song restarts in the idle slot. (5) PLAY starts the queue (entry at the hint position) only when no song is open or the open song ended or the queue ran out; otherwise it resumes. Removing the playing entry keeps the song playing, clears the playing id and makes the entry now at that position the next one. `seek-render` stays red until I51.
- I54: the top-level `add_compile_options` now wraps the Selecta warning set in `$<$<COMPILE_LANGUAGE:C>:...>` (same flags for C, none for C++), so the one C++ target `selecta_openmpt` does not inherit `-Werror -pedantic -Wstrict-prototypes`; it uses `-Wall` only (vendored code). `selecta_openmpt` lives in `plugins/replayers/openmpt/CMakeLists.txt` (created here, not only at I55), added only when `NOT SEL_AMIGA`, so build/amiga is unchanged. `cmake/openmpt_sources.cmake` reads the three `*_CXX_SOURCES` blocks (COMMON, SOUNDLIB, LIBOPENMPT) of upstream's Makefile by regex (149 sources, no test-only `soundfile_write`, no `unarchiver`); a Makefile layout change fails configure.
- I51: SEEK is a fourth song-change kind (`SEL_BEGIN_SEEK`) through `begin_song`/`prepare_slot`, so it defers like OPEN when both slots are busy (`deferred_seek`). The skip needs the voices to end where a full render leaves them, so the mixer got `sel_mixer_advance` (same position arithmetic as `sel_mixer_mix`, no resampling, no `mixed_frames` count); the plan said only "run the scheduler without mixing". The CLI reads `seek_path` and `skipped_frames` from two new `sel_engine_stats` fields (`seek_path`, `seek_skipped_frames`, set on the pump thread) and prints `SEEKED <frame>` from the event sink on stdout. SEEK before any song answers `ERROR no_song SEEK`, so `tests/m0/basic.expected` line 7 changed from `not_implemented`. Argument errors answer `ERROR range SEEK`; targets are capped at 4,000,000 ms. A seek past the song end ends the skip at the last frame and the next pump reports SONG_ENDED.
- I52: `sel_state` gained `song`, `queue`, `queue_count`, `queue_playing`, `queue_version`, `view`, plus an extra `queue_text` pointer so the UI can resolve `path_offset` (not in the row text). `view` is an opaque pointer, NULL until phase 4. `song.pattern_pos` stays zero until RP_POSITION is wired (I56). The snapshot is refreshed by `sel_engine_pump` before event delivery. The four `sel_engine_queue_*` accessors were removed and `@queue` in selecta-cli now reads the snapshot. `sel_queue_entry` moved into the public header. No `abi state` golden was added (none exists for `sel_state`); T6 stays green on the existing host and voices goldens. `cmake/no_float.cmake` expects 9 objects (queue.o added), so T18 covers queue.o.
- I53: `selecta-cli plugins --formats <id>` prints `<id>_players=N` plus one `player <id>` line per entry; T23 (`nuade-players`) compares N with the glob of `third_party/nuade/players/*/nuade_player.cmake` (121, counted from the registry walk). The design doc section 5 figure 119 is corrected to 121.
- I53: the nuade plugin RP_NAME changed from `nuade.protracker4` to `nuade`; SEL_RP_FORMATS is now the comma list of player ids (was one display name); SEL_RP_EXTENSIONS is the deduplicated union.
- I53: players without `tick` (self-rendering `render_raw`) still count in the 121 but are skipped by probe and open, since the VOICE-kind adapter cannot drive them. A song with `voice_count` above 32 is refused at open.
- I53: metadata reports only tags the player gives (TITLE, AUTHOR, FORMAT, MESSAGE, CHANNELS, SUBSONGS, INSTRUMENTS, SIZE). PATTERNS, SAMPLES and DURATION_MS are absent: nuade has no player-level value for them.
- I53: the 68k image with all 121 players is 1.85 MB, above vamos's default RAM (1 MiB) and close to the 2 MB 68020 baseline. Every vamos call in the tests now passes `-m 8192`; T11 `amiga-machine` expected FAST_RAM changed from 1044480 to 8384512 (re-measured). The image size against the 2 MB baseline is an open sizing question for D-M2-1.
- I55: the engine had no RENDER-kind support (`prepare_slot` returned `unsupported_kind`), so it gained it here: slot fields `rp_render`/`rp_name`, stats fields `replayer`/`render_kind`/`replayer_render_calls`/`rendered_frames`, a RENDER loop in `sel_engine_render` (short read ends the song), and a skip-by-discard placeholder in `skip_frames`. I56 still owns calling RP_SEEK and RP_POSITION natively; the adapter implements both hooks but the engine does not call them yet. `selecta-cli render` prints `replayer=... render_calls=... rendered_frames=...` for RENDER replayers, the old `ticks=` line otherwise. `selecta_openmpt` links `CMAKE_CXX_IMPLICIT_LINK_LIBRARIES` because the apps link as C. `registry-tags` expects one more plugin on host builds. T22 counters, WAV size and header are green; the SHA-256 check is guarded and skipped ("golden hash absent") until `tests/golden/openmpt/selecta-test-mk.sha256` is committed after the listening check, so row I55's hash part is PENDING.
- I56: (1) T21 openmpt half is RED for byte equality and stays unticked: libopenmpt's `set_position_seconds` re-enters the song with fresh channel state (the seeked WAV starts at sample 516 ramping up; the continuous render has -8689 at the same frame, and no offset of the 10 s render matches), so a native seek can never equal bytes 882000 onward. `seek-render-openmpt` therefore asserts `seek_path=native`, `SEEKED 220500`, the 882044-byte size and a non-silent body (`EXACT=0`); the byte-exact assertion exists for nuade only. Decision for the user: keep native seek (fast, not sample-exact) or give openmpt restart-and-render-discard (exact, costs one render pass of the skipped span). (2) The seek target now travels in ms through `begin_song`/`prepare_slot`/`deferred_seek` and is converted to a frame once in `prepare_slot`; native sets `song_frames` to that frame. (3) RP_POSITION is sampled into slot fields on the thread that owns the instance (audio thread after each render call, pump thread while the slot is unpublished) because the pump must not call into libopenmpt while the audio thread renders; POSITION events carry `order row pattern` as three extra uints and the snapshot fills `song.pattern_pos` only for replayers with RP_POSITION (nuade unchanged). Test `position-openmpt` plus script directive `@position`. (4) T26: libopenmpt allocates nothing during render on this module (shim window 0, before-count 975 proves interposing); added `no-alloc-render-openmpt` and `no-alloc-render-queue` (script directive `@alloc_guard on` brackets each engine render call; 3-song auto-advance). The seek call itself runs on the pump thread and is outside the window. (5) `tests/golden/openmpt/selecta-test-mk.sha256` appeared untracked in the working tree during I56 and does not match the render (also fails at HEAD engine); not committed here, `render-golden-openmpt` fails until the hash is regenerated after the listening check.
- I56 (user decision 2026-10-23): native openmpt seek (`seek_path=native`) is accepted as not byte-exact. T21 for openmpt therefore checks position, not bytes: `seek-render-openmpt` asserts the order/row/pattern printed by `render --seek-ms 5000` right after the seek (`seek_position order=0 row=42 pattern=0`). A straight-through render reports row 41 at frame 220500 (speed 6, tempo 125, 5292 frames per row); libopenmpt `set_position_seconds` lands on the first row boundary at or after the target (row 42, frame 222264), one row (about 42 ms) late. Byte equality is asserted for nuade only. Fixed afterwards in the adapter: `rp_seek` lands on the row containing the target (steps back one row when libopenmpt snapped up) and render-discards the lead-in into a buffer held in the adapter state, so the seek now reports row 41, equal to the straight render (`position.expected` updated to match). The row-1 step-back was wrong across pattern breaks, jumps and delays, so `rp_seek` now retries `set_position_seconds` with a doubling back-off until the reported landing time is at or before the target and render-discards the rest; test `seek-render-openmpt-break` (module `gen_testmod --break --format mk`, Dxx at row 15, expected order=0 row=15 from `tests/m1/position-break.sel`) fails with the row-1 code and passes now.
- I57: `selecta_library` (`src/library`) holds `local.c` (one fixed browse buffer, sized like the queue unless `browse_entries`/`browse_text_bytes` are set in `sel_engine_config`) and one `sel_dir_each` implementation: `src/core/macos/dir.c` (POSIX; shared by macOS and Linux, chosen by CMake) or `src/core/amiga/dir.c`, a stub that always fails until M2, so LIBRARY_BROWSE answers `ERROR browse_failed` on the 68k build. Listing: `..` first (not at `/`), directories, then files, case-insensitive ASCII; dotfiles skipped; overflow truncated and logged as `LOG browse_truncated n`; only absolute paths are supported (`sel_browse_join` is lexical). `opendir` allocates inside libc on the pump thread (never the audio thread). LIBRARY_RESULT carries two new tags in the `SRC` namespace, `SRC_PATH` (0x7000) and `SRC_COUNT` (0x7001); the engine keeps a 4-slot ring of tag lists so pointers survive until the pump delivers. The public `sel_view_state` types are defined in `engine.h` already here so the red T24 compiles; the engine does not fill `view` until I58. T24 is registered with WILL_FAIL until I58 (a plain red test would break the gate); I58 removes it. `no_float.cmake` now also scans the 2 library objects (11 total).
- I58: VIEW_ROWS carries tags only (UI_ROW_COUNT, UI_CURSOR, UI_WINDOW_START, UI_WINDOW_ROWS); the rows are read from the snapshot `view`, not serialised in the event. The tags reuse the UI namespace (0x5009-0x500C) instead of a new VIEW namespace. VIEW_CHANGED sends the style name plus the same tags, followed by a VIEW_ROWS event. The verbs VIEW_SHOW, VIEW_CURSOR, VIEW_ACTIVATE and VIEW_WINDOW are new and born implemented; VIEW_STYLE accepts only `regular`. View events stay silent until the first VIEW_ command ran, so scripts that never use the view see no new output. A queue row shows the file basename; the META_TITLE of the playing entry is not used yet. Activate: directory enters it (".." lands on the directory just left), file is appended to the queue and started, queue row starts that entry. The WILL_FAIL on view-unit is removed. The no_float object count is now 12 (6 engine objects). The T24 activate-file group clears the queue first, since earlier groups leave entries.
- I59: the text UI files live in `plugins/ui/text/` and build as the plain static library `selecta_textui` (desktop only, `if(NOT SEL_AMIGA)`), not through `sel_add_plugin`: that needs a plugin entry symbol, which arrives with `text_ui.c` in I60. `posix/term.c` is the only file with OS headers (macOS and Linux). Two headers beyond the plan list were added, `keys.h` and `term.h`, and `keys.c` also parses SGR mouse reports (`TXT_KEY_MOUSE`). T25 is registered as `grid-unit` and holds the groups flush, full, keys, theme and term in one executable; the term group drives a pty and checks raw mode, size, the enter and leave sequences, termios restore and rejection of a pipe. The termios restore check compares only ICANON, ECHO, ISIG and IEXTEN: macOS sets an extra kernel-owned c_lflag bit after any mode change, so full-word equality never holds. Red and green happen inside the one I59 commit (the first run failed on the term check; there is no separate red commit). The Amiga build and the `no_float` object count (12) are unchanged; the Amiga text UI runs in UP-Term in M2.
