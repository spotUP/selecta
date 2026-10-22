---
date: 2026-10-23
topic: Gap between the M0b codebase and a first M1 slice (shell, text UI, view model, queue, transport, subsong, seek, info panel)
tags: [research, m1, engine, ui]
status: complete
---

# M1 codebase gap (R9)

Method: grep for names, then counted from the structures that produce them (CMake source lists, the include/selecta header set, the dispatch table, the built CLI). Design refs: sections 3, 7, 9b, 9c, 10, 14 of `thoughts/shared/plans/2026-10-22-architecture-design.md`.

## 1. Engine commands

`include/selecta/commands.def` has 68 rows (`grep -c '^SEL_CMD'`). The dispatch is `SEL_CMD_IMPLEMENTED(X)` at `src/engine/engine.c:21-34`, used by `dispatch()` (`engine.c:531-545`) and `sel_engine_cmd_implemented()` (`engine.c:547-557`); anything else goes to `cmd_not_implemented` (`engine.c:528`, replies ERROR not_implemented).

- Implemented (14, counted from the X-list): PLAY, PAUSE (toggle), PAUSE_PLAYBACK, STOP, VOLUME, OUTPUT, OPTION, COMMANDS, EVENTS, TAGTYPES, BEGIN, END, OPEN, SUBSONG.
- Unimplemented: 68 - 14 = 54. `build/host/apps/selecta-cli/selecta-cli commands --unimplemented | wc -l` = 54 (binary exists). `tests/ratchet/unimplemented.txt` also holds 54. Both numbers agree.
- M1-relevant verbs that are NOT implemented: NEXT, PREV, SEEK, QUEUE_ADD, QUEUE_REMOVE, QUEUE_CLEAR, QUEUE_PLAY_NEXT, QUEUE_MOVE, QUEUE_SWAP, QUEUE_SHUFFLE, REPEAT, MUTE, SOLO, SLEEP_TIMER, PLAYLIST_LOAD/SAVE, LIBRARY_BROWSE, VIEW_STYLE/GROUP/EXPAND/COLLAPSE/DRILL, UI_SWITCH, IDLE, all the rest.
- Note: OPTION stores `consume/single/random/repeat/...` strings in `sel_state.options` (`engine.c:304`), but nothing acts on them (no queue to consume or repeat).
- SUBSONG (`engine.c:511-526`): range-checks against `rp_info.subsong_count`, restarts via `start_subsong`, emits POSITION only (no SONG_OPENED, no STATE).

## 2. Events (`events.def`, 19 rows)

Emitted today (all from `src/engine/engine.c`):
- STATE (`emit_state`, :142), VOLUME (:149), OUTPUT_CHANGED (:156), OPTIONS_CHANGED (:163): from `set_play`/VOLUME/OUTPUT/OPTION through `changed()`/`flush_pending` (coalesced inside BEGIN..END, :40).
- ERROR (:225, via `sel_engine_report_error`), LOG (:234, reflection lines for COMMANDS/EVENTS/TAGTYPES).
- SONG_OPENED (:505, tags = replayer metadata, then POSITION), SONG_ENDED (:614, in pump when `e->ended`, then state goes STOPPED), POSITION (`emit_position` :356; fields subsong, ticks, seconds, frames; emitted at OPEN, SUBSONG, and in pump (:618) whenever whole seconds change).
- Emitted: 9 of 19 (STATE, SONG_OPENED, SONG_ENDED, POSITION, VOLUME, OUTPUT_CHANGED, OPTIONS_CHANGED, ERROR, LOG; matches the 9 `evt_init(&` calls in engine.c). Never emitted (10): VOICE, PREFS_CHANGED, QUEUE_CHANGED, QUEUE_HISTORY, LIBRARY_RESULT, VIEW_CHANGED, VIEW_ROWS, SEEKED, SCROBBLE_DUE, ALBUMART.

## 3. Engine state, threading, waiting

- `sel_engine_state()` (`engine.c:630`, `include/selecta/engine.h:70`) returns `const sel_state*` (a plain struct, not a tag list): `play` (STOPPED/PLAYING/PAUSED), `volume` 0..100, `output[32]`, `options[9][64]` (`engine.h:37-42`). There is no song, metadata, position, subsong, queue or view data in it. Position and metadata exist only as events; the engine keeps `subsong`, `song_frames`, `ticks`, `rp_info` privately.
- Threading: `sel_engine_send` pushes to the command SPSC ring, `sel_engine_pump` (`engine.c:603-628`) drains commands, dispatches, then drains the event ring into a sink callback on the caller's thread. `sel_engine_render` runs on the audio thread and only reads state, mixes, sets `e->ended`; events are produced on the pump side only (comment at `engine.c:~651`). Ring: `src/engine/spsc.c` (47 lines) uses `volatile` head/tail, no atomics, no wake-up. `src/engine/thread.h` is a debug-only assertion that render runs on the marked audio thread and send/pump never do (pthread, Apple/Linux, non-NDEBUG).
- Wait/notify: ABSENT. No condvar, mutex, semaphore, poll, select or sleep in `include/ src/ apps/ plugins/` (grep for pthread_cond|pthread_mutex|nanosleep|usleep|poll\(|select\( gave only thread.h's pthread_self/equal). The design's `SEL_UI_WAIT` (blocks on OS input + engine signal, section 7) has nothing to build on: the engine cannot signal, and `sel_engine_pump` is poll-only. The CLI `play` loop pumps the output instead (`apps/selecta-cli/play.c`). A wake primitive per OS (`src/core/<os>/`) is needed, or the shell polls with a timeout.
- Pause: `cmd_PAUSE` toggles (:257); `render` outputs silence unless PLAYING (:~668).

## 4. Plugin ABI and registry

- `include/selecta/plugin.h:15-24`: enum `sel_plugin_kind` already has REPLAYER=1, OUTPUT=2, DSP=3, VIZ=4, UI=5, CONVERTER=6, SOURCE=7, INTEGRATION=8, SCRIPTING=9. So the UI kind value exists.
- `include/selecta/tags.def`: namespaces reserved in the header comment (UI 0x5000, SRC 0x7000, VIZ 0x4000, DSP 0x3000 "M1") but NO rows exist for them. Defined rows: PLUGIN (4), RP (14), OUT (15), META (15), MACHINE (11), HOSTQ (1), MIX (1). Zero `SEL_UI_*`, `SEL_SRC_*`, `SEL_VIZ_*`, `SEL_DSP_*` tags (`grep -rn 'SEL_UI\|UI_' include src` returned nothing).
- Tag rows are the single source: tags.def feeds the enum (`tags.h`), the name/type table (`src/core/tag.c`), TAGTYPES reflection and the lint. Adding a UI tag = add `SEL_TAG(UI, NAME, 0x5000, STR)` etc. rows (types INT/STR/PTR/LIST), update the test counts derived in `tests/CMakeLists.txt` (`SEL_COUNT_META` is computed from the file; not verified how the other counts are derived).
- Registry: `src/engine/registry.c`: `g_desc[SEL_REGISTRY_MAX]` filled by `sel_registry_init` calling every `sel_registry_entries[i](SEL_API_VERSION)` (:20-21); `sel_registry_find(kind,id)` (:37); lint at :56+. The entries array is generated by `cmake/selecta_registry.cmake` from the `SEL_PLUGINS` global property, filled by `sel_add_plugin(name kind sources...)` (`cmake/selecta_plugin.cmake`; also makes a `sel_sdkcheck_<name>` target compiled with `include/` only). `plugins/CMakeLists.txt` registers null, wav, coreaudio (Apple), nuade; kind passed as a number.
- Adding a new kind: no registry change is needed (it is kind-agnostic); add `sel_add_plugin(textui 5 plugins/ui/text/text.c)` plus its SEL_UI_* tag rows and hook typedefs in a new `include/selecta/ui.h`. The engine's `find_replayer` filters by kind itself (`engine.c:~432`). The `sel_engine*` and `sel_library*` needed by `SEL_UI_OPEN` (section 7): `sel_engine` is opaque in `engine.h` (OK); `sel_library` does not exist.
- The plugin SDK rule means a UI plugin using terminal I/O needs OS code under `plugins/ui/text/<os>/` or `src/core/<os>/` (RULES.md 3); nothing exists there.

## 5. Replayer metadata, subsongs, position

- `SEL_META_*` (tags.def): TITLE, AUTHOR, FORMAT, CHANNELS, PATTERNS, INSTRUMENTS, SAMPLES, SUBSONGS, DURATION_MS, MESSAGE, SIZE, YEAR, RELEASE, SID_MODEL, STIL (15).
- nuade adapter `plugins/replayers/nuade/adapter.c:rp_metadata` (~:121-140) reports exactly 3: TITLE, AUTHOR, FORMAT, from NUADE_META_TITLE/ARTIST/FORMAT in `set_tag` (:104-112). It also has an `add_instrument` sink, but instruments are not emitted. Not reported: CHANNELS, PATTERNS, INSTRUMENTS, SAMPLES, SUBSONGS, DURATION_MS, MESSAGE, SIZE. A module info panel therefore needs the adapter extended (instrument/sample names would need a new list tag form, none exists; unverified what nuade's sink exposes beyond what is wired).
- Subsong: supported in the replayer API: `sel_rp_info.subsong_count/default_subsong` (`replayer.h:29-34`), adapter copies them (:70-71), `rp_start(state, subsong, ...)` (:85). The engine uses them (OPEN starts the default, SUBSONG n restarts), but neither count nor current subsong is in `sel_state` or SONG_OPENED tags (META_SUBSONGS not reported). The only place the current subsong is visible is POSITION arg 0.
- Position: engine counts frames/ticks/seconds (`emit_position` :356); no order/row/pattern (design says POSITION "order/row/tick"); events.def signature is `uuuu` and the engine fills subsong, ticks, seconds, frames. Duration: absent. SEEK: unimplemented; the replayer ABI has no seek hook (`replayer.h` has probe/open/close/start/tick/metadata only), so seek needs either a new RP_ tag (`SEEK`) or restart-and-fast-forward in the engine.

## 6. Absent features (queue, playlist, library/source, browsing, terminal)

Established by (a) the CMake source lists: `src/CMakeLists.txt` builds only core/version, tag, codec, machine, host/host,mem,log, engine/spsc,engine,voices,scheduler, mixer, platform clock+machine (no `src/library/`, no `src/ui`, no `src/source`); `apps/CMakeLists.txt` has one subdirectory, `selecta-cli` (no `apps/selecta`); `plugins/CMakeLists.txt` has outputs null/wav/coreaudio and replayer nuade only (no ui/source/viz/dsp plugin). (b) The `include/selecta/` header set, 14 files: api, engine, host, machine, output, plugin, protocol, registry, replayer, tags, voices (+ commands.def, events.def, tags.def). No queue, playlist, library, source, view, ui or terminal header. (c) Grep over include/src/apps/plugins for queue|playlist|termios|tcsetattr|isatty|opendir|readdir|dirent|poll|select: only hits are the SPSC command/event queues, the mixer's per-voice "queued sample" and the commands.def/engine.h option names. All of these are ABSENT:
- queue (commands exist as unimplemented rows; no data structure, no entry ids, no QUEUE_VERSION),
- playlist and 9c abstraction, library view, `sel_library`, `sel_source`,
- file browsing/directory listing (the only file IO is `read_song`, `engine.c:~385`, `fopen` of one path; `host->read` exists but is replayer file reading),
- view model (VIEW_* verbs unimplemented, no row/window type),
- terminal I/O: raw mode, cell grid, key decoding, SGR output, alternate screen; none,
- shell `apps/selecta`, config store/prefs, hotkey map.
- Related existing building blocks: `sel_codec_*` (text line codec for commands/events, `src/core/codec.c`) which a `:` command line could reuse; `COMMANDS` reflection for a palette; `sel_host_default()` (`host.c`).

## 7. selecta-cli

`apps/selecta-cli/` (CMake: `main.c abi.c plugins.c play_null.c machine.c render.c session.c play.c`, linked to selecta_registry + selecta_core). Subcommands (`main.c:299-352`, usage :16): version, help, commands [--unimplemented], events, tagtypes, roundtrip, script <file>, abi [voices], plugins [--lint], machine, play-null, play <song> --output --seconds [--rate] [--resample], render <song> --output --seconds --rate --resample.
- Argument parser: hand-rolled, a `strcmp` chain on `argv[1]` in `main()` and per-command `--flag value` loops (e.g. `play.c:43-50`). No shared option parser. Reusable pieces for a shell: `session.c` (`sel_cli_find_output`, `sel_cli_start_song`, `sel_cli_engine_pull`; creates an engine with registry hooks, sets the mix, OPEN + PLAY), `cmd_script` (`main.c:253-298`: decode a line, `sel_engine_send`, `sel_engine_pump` with `print_event`, a working reference of the loop a shell needs), and `play.c` (engine + real output pump). `cli.h` declares them. Shell should copy the session helpers into a shared place rather than reimplement; they live in the app dir today, not a library (decision needed, unverified plan).
- `script` has no timing: it pumps once after each line, so it cannot drive a song to the end unless the output pulls frames.

## 8. Test infrastructure

- `tests/CMakeLists.txt`: helper `sel_cli_test(name "args" -DEXPECT|EXPECT_FILE|LINES|EXPECT_REGEX ...)` running `tests/expect.cmake` against selecta-cli stdout (:7-11). Tests: protocol-roundtrip/reflection (counts derived from the def files), registry-tags(+negative), abi-host/voices-layout, m0-script (`tests/m0/basic.sel` + `.expected`: a script-driven protocol golden), m0-null-output, ratchet, testmod-sha, render goldens (`tests/golden/nuade/selecta-test.<resampler>.sha256` committed), coreaudio-play, sdk-isolation, machine-probe, amiga-* (vamos), no-float-engine, mixer-unit, thread-assert-* (Debug), nuade_ab.
- Ratchet (`tests/ratchet.cmake`, `tests/ratchet/unimplemented.txt`): live count of `commands --unimplemented` must be <= the file's number (54 now); each plan that implements verbs lowers the file in the same commit.
- Reusable for M1: (1) `.sel` scripts + `.expected` through `script` for queue/NEXT/PREV/SUBSONG/SEEK logic in the engine (needs `script` to pump long enough, or a frame-driving flag); (2) `sel_cli_test` for any new CLI verb; (3) the ratchet for the verb count; (4) `tests/songs/selecta-test.mod` (generated by `tools/gen_testmod.c`) as the song; (5) the `thread_assert_test.c` pattern for a C test executable with CMake-driven expectation. Missing: a pty/terminal-driving test harness for the text UI (nothing exists; a UI "reachability" test needs a fake terminal backend or a pty script); no unit-test framework beyond hand-written mains. The test-harness resource caps (RULES / global 6a-1) apply to ctest runs.

## Gap summary for M1 (rows to create)

1. Engine: queue + entry ids + QUEUE_* verbs/events; NEXT/PREV (+ SONG_ENDED auto-advance); SEEK (+ replayer SEEK hook or restart-skip) and SEEKED; state snapshot gaining song info, subsong count/current, position, queue, view; a wake primitive for SEL_UI_WAIT.
2. Replayer: nuade adapter reports SUBSONGS, CHANNELS, PATTERNS, SAMPLES/INSTRUMENTS, DURATION_MS (and names) for the info panel.
3. ABI: `SEL_UI_*` tag rows (NAME, OPEN, ON_EVENT, WAIT, CLOSE, VIEW_STYLES, ...) and `include/selecta/ui.h`; `sel_library` type decision (section 7 passes it to OPEN; none exists).
4. Source/browse: a local-files lister (src/library or a SOURCE plugin; directory listing needs an OS shim under `src/core/<os>/`).
5. View model (regular list first) in `src/library/` or engine, driven by VIEW_* verbs and VIEW_ROWS/VIEW_CHANGED events.
6. Terminal backend (raw mode, cell grid with diff flush, key decode) under an OS dir, and the text UI plugin; `apps/selecta` main loop + CMake wiring + RULES.md Commands row (already lists the run command as "not yet implemented").
7. Tests: a fake-terminal harness; ratchet lowered per implemented verb.
