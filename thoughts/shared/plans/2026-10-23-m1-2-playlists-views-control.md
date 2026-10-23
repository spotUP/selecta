---
date: 2026-10-23
topic: M1.2 implementation plan: playlist codecs, tree and drilldown views, title formats, command mode, palette, key bindings, control socket, Now Playing and MPRIS, queue history, shuffle, scope and album navigation
tags: [plan, m1, playlists, view-model, text-ui, queue, integration]
status: approved
rows: I65 to I84 in thoughts/spot/todos.md
---

> Approval 2026-10-23: the user asked for autonomous overnight work and for questions to be collected for the next morning. Every open point got the default named under "Decisions taken in this plan"; points the user may want to overturn are listed in `thoughts/spot/questions-for-tomorrow.md`. The plan counts as approved with those defaults.

# M1.2: playlists, views, command mode, bindings, control socket, queue history

Implements design doc `thoughts/shared/plans/2026-10-22-architecture-design.md` section 3 (F30 queue scope and album navigation, F49 queue undo and redo), section 7 (text UI: F45 vi command mode, F48 columns and title formats, F49 palette and help overlay), section 9b (tree and drilldown styles), section 9c (playlist model), section 14 (M1.2 slice) and section 17 (playlist formats). RULES.md rules 1 (engine is the single source of truth), 3 (plugins see `include/selecta/` only), 4 (no float in engine, no allocation after start on the audio thread), 5 (adapt, never fork) are the contract. Research input with the user decisions: `thoughts/shared/research/2026-10-23-m1-2-m1-3-requirements.md`. M1.1 (`2026-10-23-m1-1-text-ui-queue.md`) is the base.

## Finish line

`cmake --build build/host && nice -n 19 ctest --test-dir build/host -j2` green on macOS and `cmake --build build/amiga` green, with T1 to T27 still green and these new tests:

| ID | Test (ctest name) | Entry point | Sentinel (proves new code ran) | Spec number asserted |
|----|-------------------|-------------|--------------------------------|----------------------|
| T28 | `selecta-m12-reach` (the M1.2 reachability test) | `selecta --output null --keys tests/m12/reach.keys --bindings tests/m12/reach.bindings --screen-dump <tmp>.txt` | on exit the shell prints `cmdline_runs=C palette_opens=P bindings=B playlist_entries=E undo_depth=U redo_depth=R view_style=S` | keys: `:PLAYLIST_LOAD tests/m12/three.m3u` Enter (E=3), `:VIEW_STYLE tree` Enter, user-bound key `x` (bound to `QUEUE_SWAP 1 2` in the bindings file), `:UNDO` Enter, Ctrl-P palette open and Escape, quit. C=3, P=1, B equals the binding count of the default table plus 1, U=1 (load is 1 edit; swap then undo leaves 1 undo and 1 redo), R=1, S=tree; last dumped screen equals `tests/m12/reach.screen` (tree rows grouped by folder, title format column `%title% - %format%`) |
| T29 | `playlist-codecs` | `tests/playlist/playlist_test` (unit) | n/a | Hippo `.prg` fixture (header `HiPPrg\n\n`, 4 entries, one 0xF7 divider, one 0xA2 stealth, one `#name=`, one `#song=2`) reads to 4 entries with kinds song, divider, stealth, song and writes back byte-identical; M3U write of the same list emits `#EXTM3U` and `#EXTINF:<sec>,<title>` and reads back the same keys; PLS `NumberOfEntries=4` and `Version=2`; XSPF puts the key in `<location>` and the subsong in `<extension application="selecta"><subsong>2</subsong></extension>`; writing a divider to M3U, PLS or XSPF returns warning `lossy divider`; a 131072-entry Hippo write returns `range` |
| T30 | `queue-history` | `selecta-cli script tests/m12/history.sel` | `.expected` lines carry `QUEUE_HISTORY u r` | MOVE, SWAP, QUEUE_SHUFFLE 7, RESHUFFLE, REMOVE, CLEAR each raise undo by 1 and set redo to 0; UNDO restores the exact id order and version+1; REDO re-applies; 65 edits leave undo depth 64 (oldest dropped); QUEUE_SHUFFLE with seed 7 on ids 1..5 gives the order listed in the expected file (xorshift32, section "Decisions"); playback position unchanged by UNDO |
| T31 | `queue-scope-album` | `selecta-cli script tests/m12/album.sel` | `.expected` lines carry `SONG_OPENED` keys | queue of 5 songs in dirs a, a, b, b, c: NEXT_ALBUM from entry 1 plays entry 3, again plays 5, again emits `STATE stopped`; PREV_ALBUM from entry 4 plays entry 1 (first song of the previous album); PREV_ALBUM from entry 3 plays entry 1; `SCOPE album` makes NEXT at the last song of album b stop instead of entering c; `SCOPE library` restores; `SCOPE playlist` equals queue scope in M1 (answer `OK`); `SCOPE bad` answers `ERROR range SCOPE` |
| T32 | `view-tree` | `tests/view/view_test` (extended, unit) | n/a | `VIEW_GROUP folder` over a 3-folder list gives 3 collapsed node rows; `VIEW_EXPAND 2` shows that folder's songs indented; `VIEW_COLLAPSE 2` hides them; cursor keeps the same song id across `VIEW_STYLE regular` -> `tree` -> `drilldown`; `VIEW_GROUP format` groups by `META_FORMAT`; drilldown `VIEW_DRILL 0 <folder>` filters column 1 to that folder; `VIEW_GROUP artist` answers `ERROR unsupported VIEW_GROUP` (no metadata store before M3) |
| T33 | `title-format` | `tests/view/format_test` (unit) | n/a | `%title%` `%author%` `%format%` `%subsong%` `%duration%` `%path%` `%file%` `%channels%` expand; `[%author% - ]%title%` drops the bracket when author is empty; `%%` gives `%`; unknown field `%foo%` or an unclosed `[` falls back to the style default and returns warning `bad_format`; `PREF_SET view.title_format <fmt>` changes rows and emits `PREFS_CHANGED view.title_format`; `PREF_GET` of an unknown key answers `ERROR unknown_pref` |
| T34 | `keybind-unit` | `tests/ui/keybind_test` (unit) | n/a | default table has one row per key the M1.1 switch handled (k j Up Down s n p Left Right a d i q Enter) plus `:` and Ctrl-P; a file line `x QUEUE_SWAP 1 2` overrides or adds; `unbind n` removes; a line naming a verb not in COMMANDS is rejected with line number; `F1`..`F12`, `C-a`, `M-x`, `Space`, `Tab` key names parse |
| T35 | `control-socket` | `selecta --output null --keys tests/m12/wait.keys` in the background, then `selecta-cli send NEXT` and `selecta-cli send PAUSE` | shell prints `socket_cmds=N` on exit | N=2; `selecta-cli send` exits 0 and prints the reply line `OK`; `send` with no shell running exits 2 with `no selecta running at <path>`; socket file removed on exit |
| T36 | `cmdline-unit` | `tests/ui/cmdline_test` (unit) | n/a | `:QUEUE_SH` Tab completes to `QUEUE_SHUFFLE`; second Tab cycles to `QUEUE_SHUFFLE` only (single match), `:QU` Tab lists matches in the status line; Up recalls the previous line; history keeps 100 lines; `:!tests/m12/argdump.sh` with two selected rows runs the program with the two paths as arguments (the script writes them to a file the test reads) |
| T37 | `palette-unit` | `tests/ui/palette_test` (unit) | n/a | query `qsh` ranks `QUEUE_SHUFFLE` first (subsequence match, earlier and contiguous matches score higher); every COMMANDS verb appears exactly once; area grouping follows the `commands.def` comment sections (transport, queue, playlists and library, view model and shell, protocol) read from `COMMANDS` output field `area`; current binding shown (`n` for `NEXT`) |
| T38 | `integration-nowplaying` | `selecta-cli script tests/m12/nowplaying.sel --integration nowplaying-test` | the integration plugin's fake backend prints `np_publish=N np_command=C` | after OPEN of `selecta-test.mod` the published record has title `selecta test`, duration 30000 ms, state playing; a fake remote `next` command reaches the engine as `NEXT` (C=1); PAUSE publishes state paused (N=3: open, play, pause) |
| T8 | `ratchet` (lowered) | `selecta-cli commands --unimplemented` | as M0a | count <= 27 (45 minus NEXT_ALBUM, PREV_ALBUM, QUEUE_MOVE, QUEUE_SWAP, QUEUE_SHUFFLE, RESHUFFLE, SHUFFLE, SCOPE, UNDO, REDO, PLAYLIST_LOAD, PLAYLIST_SAVE, PREF_GET, PREF_SET, VIEW_GROUP, VIEW_EXPAND, VIEW_COLLAPSE, VIEW_DRILL) |

Gate 2 (red first): every T row is written and seen failing before the code it checks exists; each ledger row names the T rows that went red then green.

## Decisions taken in this plan (not open)

User decisions (2026-10-23, research file): album = parent directory in M1; no OS-wide global hotkeys, instead user key bindings plus a control socket plus the Now Playing / MPRIS integration plugin (moved up from M5). The rest are defaults chosen here:

- **Playlist codec API.** `src/library/playlist/playlist.h`: `sel_playlist` (fixed arrays from a caller arena: entries with `kind` song/divider/stealth, `key` offset, `subsong`, `name` offset, `duration_ms`), `sel_playlist_read(fmt, bytes, len, arena, out, warn_cb)` and `sel_playlist_write(fmt, pl, sink, warn_cb)`. Format chosen by extension (`.prg` Hippo, `.m3u`/`.m3u8`, `.pls`, `.xspf`), then by sniffing the first bytes (`HiPPrg`, `#EXTM3U`, `[playlist]`, `<?xml`) when the extension is unknown. Song keys: in M1 a key is a local path; relative paths resolve against the playlist's directory; Amiga paths with a volume (`Work:Mods/x.mod`) are kept verbatim and play only when the core can open them; `http(s)://` keys load as rows but OPEN answers `ERROR unsupported_source` until M3.
- **Dividers and stealth in the queue.** `PLAYLIST_LOAD` adds songs to the queue and turns a divider into a queue row of kind divider (shown, never played, skipped by NEXT/PREV/auto-advance). Stealth entries are queued as songs with flag hidden (played, not shown in the regular style). `PLAYLIST_SAVE` writes the queue including divider rows. Load appends (does not clear); it is one undo record.
- **Queue history.** Ring of 64 records allocated in `sel_engine_create` from the queue arena. A record stores the full id order before the edit (ids are u32; the queue cap is 256 entries in default config, so a record is at most 1 KB and the ring 64 KB on desktop; on machines where the queue cap is smaller the record shrinks with it). This replaces per-edit inverse records: one mechanism, exact for every edit kind, including shuffle. Removed entries keep their key text until the record that references them leaves the ring.
- **Shuffle and seed policy.** xorshift32 (`x ^= x << 13; x ^= x >> 17; x ^= x << 5`), seeded from the `seed` argument or, when absent, from the host clock at the command. `QUEUE_SHUFFLE [seed]` permutes all not-yet-played rows after the playing one (Fisher-Yates) once and is undoable. `SHUFFLE on|off [seed]` sets the `random` option and the seed; with random on, NEXT picks the next entry from a permutation computed at SHUFFLE time and kept as a play order (rows do not move). `RESHUFFLE` recomputes that play order (random on) or re-permutes the remaining rows (random off) with seed = next xorshift output, so it is reproducible from the first seed.
- **SCOPE.** `library` (default, whole queue), `album` (NEXT, PREV and shuffle stay inside the current album), `playlist` (same as library in M1: the queue is the only playlist; real playlists arrive with M3 storage). Persisted as option `scope`.
- **Album navigation.** `NEXT_ALBUM`: first queue entry after the playing one whose parent directory differs from the playing one; none = stop. `PREV_ALBUM`: if not at the album's first song, go to the album's first song; else the first song of the previous album. Divider rows also break albums.
- **Tree and drilldown.** View model grouping keys in M1: `folder` and `format` only (the keys with data before the M3 catalogue). Node ids are stable u32 per group key value. Lazy children: a node lists only when expanded. Drilldown columns: `folder > song` and `format > folder > song`. `VIEW_GROUP artist|album|genre|composer|source` answer `ERROR unsupported VIEW_GROUP`.
- **Prefs.** `PREF_GET key` / `PREF_SET key value` over a fixed table in `src/engine/prefs.c` (one X-macro, `include/selecta/prefs.def`, born in the SDK with the tag lint rule that no pref is born elsewhere). M1.2 keys: `view.title_format` (default `[%author% - ]%title%`), `view.columns` (default `title,format,duration`), `view.status_format` (default `%state% %elapsed% / %duration%  %title%`), `ui.history_size` (100). Values persist to the prefs file (next bullet). Unknown key: `ERROR unknown_pref`.
- **Title format syntax.** foobar2000 subset: `%field%`, `[ ... ]` optional section dropped when every field in it is empty, `%%` literal. Fields: title, author, format, subsong, subsongs, duration, elapsed, path, file, channels, state. No functions (`$if` etc.) in M1. Resolved in `src/engine/format.c`, called by the view model; UIs get finished strings.
- **Files and locations.** Config directory `$XDG_CONFIG_HOME/selecta` or `~/.config/selecta` on Linux and macOS (one rule for both; `~/Library/Application Support` not used so dotfile setups work), `ENVARC:Selecta` on the Amiga. Files: `prefs.conf` (`key = value` lines), `keys.conf` (bindings), `history` (command line). `--bindings <file>` and `--config-dir <dir>` override for tests.
- **Key bindings.** Bindings live in the text UI (the engine has no keys), as a table `key -> command line`. Defaults equal the M1.1 switch, plus `:` (command mode), `/` (palette filtered), Ctrl-P (palette), `?` (help overlay), `u` (UNDO), Ctrl-R (REDO), `t` (VIEW_STYLE cycle), `]`/`[` (NEXT_ALBUM/PREV_ALBUM). File syntax per line: `<key> <COMMAND args>` or `unbind <key>`; `#` comments. A bound command line goes through the same path as `:` command mode, so a binding cannot do anything a typed command cannot (no UI-private actions; UI-local actions such as opening the palette are pseudo-commands `ui.palette`, `ui.help`, `ui.cmdline`, `ui.quit` listed by the palette too).
- **Command mode.** `:` opens a one-line editor at the bottom row. Tab completes the first word from COMMANDS (plus the `ui.*` pseudo-commands). Enter sends the line through the shell's protocol parser (the one `selecta-cli script` uses, moved to `src/protocol/parse.c` so shell and CLI share it). Reply `ERROR ...` shows in the status line. `:!prog args` runs `prog args <path>...` with the selected rows' paths (cursor row when nothing is selected) via `posix_spawn`, detached, stdout and stderr to `/dev/null`; the Amiga path uses `SystemTags` with `SYS_Asynch` in M2. History: last 100 lines in `history`.
- **Palette and help.** Ctrl-P opens a fuzzy filter over COMMANDS rows plus `ui.*`; Enter on a row with no arguments sends it, with arguments it opens command mode prefilled. `?` shows the help overlay grouped by area. Areas come from a new `area` field in the `COMMANDS` reply, taken from the `commands.def` section comments (made machine-readable: `SEL_CMD_AREA(name)` rows replace the comments). Score: subsequence match; +16 per contiguous run char, +8 at word start, -1 per skipped char.
- **Control socket.** Unix domain socket `$XDG_RUNTIME_DIR/selecta.sock`, else `$TMPDIR/selecta-$UID.sock`, mode 0600, created by the shell at start, removed at exit; a second `selecta` refuses with `already running` unless the socket is stale (connect fails), then it replaces it. Protocol: the same line protocol as `selecta-cli script` (one command per line, one reply line). `selecta-cli send <verb> [args]` connects, writes one line, prints the reply, exits 0 on `OK`, 1 on `ERROR`, 2 when no shell runs. The shell polls the socket in its existing wait loop (UI_WAIT gets the fd through the same mechanism as the terminal fd). Amiga: ARexx port in M4 is the equivalent; no socket on the Amiga.
- **Now Playing and MPRIS.** One integration plugin per OS, kind `SEL_PLUGIN_KIND_INTEGRATION`: `plugins/integrations/nowplaying/macos/` (MPNowPlayingInfoCenter and MPRemoteCommandCenter, Objective-C in the plugin only; needs a run loop, so the shell's main thread runs `CFRunLoopRunInMode` for 0 s each wait turn on macOS) and `plugins/integrations/mpris/linux/` (sd-bus when `libsystemd` is found, else not built). New integration ABI in `include/selecta/integration.h`: `INT_OPEN(host)`, `INT_ON_EVENT(state, msg)`, `INT_POLL(state)`, `INT_CLOSE`; host gives `send_command(line)` so remote commands (play, pause, next, previous, seek) become protocol commands. Test plugin `nowplaying-test` (fake backend, prints counters) carries T38; the OS calls are checked manually.
- **New verbs.** None. All 18 verbs this plan implements already exist in `commands.def`.

## File layout created or changed by this plan

- `include/selecta/commands.def` (`SEL_CMD_AREA` rows), `include/selecta/prefs.def` (new), `include/selecta/integration.h` (new), `include/selecta/engine.h` (options `scope`, history accessors in the snapshot)
- `src/engine/{engine.c, queue.c, queue.h, history.c (new), shuffle.c (new), prefs.c (new), format.c (new), view.c, view.h}`
- `src/library/playlist/{playlist.h, playlist.c, formats/hippo.c, formats/m3u.c, formats/pls.c, formats/xspf.c}` (new)
- `src/protocol/parse.c` (new; moved out of `apps/selecta-cli`), `src/core/{macos,linux}/socket.c` (new), `src/core/config_dir.c` (new)
- `apps/selecta/main.c` (socket, bindings and config flags, counters), `apps/selecta-cli/{main.c, session.c}` (`send`, shared parser, `--integration`)
- `plugins/ui/text/{text_ui.c, keybind.c (new), cmdline.c (new), palette.c (new), tree.c (new)}`
- `plugins/integrations/{nowplaying/macos, mpris/linux, test}` (new)
- `tests/{playlist (new), m12 (new), view, ui}` and `tests/CMakeLists.txt`

## Phase 1: protocol parser, prefs, title formats (rows I65 to I67)
Shared parser out of the CLI, prefs table and file, title-format resolver. T33 red then green.

## Phase 2: engine queue history, shuffle, scope, album (rows I68 to I71)
History ring, MOVE/SWAP/SHUFFLE/RESHUFFLE/SHUFFLE/UNDO/REDO, SCOPE and album navigation. T30, T31 red then green.

## Phase 3: playlist codecs and PLAYLIST_LOAD/SAVE (rows I72 to I73)
Four codecs, divider and stealth queue rows. T29 red then green.

## Phase 4: view model tree and drilldown (rows I74 to I75)
VIEW_GROUP/EXPAND/COLLAPSE/DRILL, text UI rendering of both styles. T32 red then green.

## Phase 5: text UI bindings, command mode, palette and help (rows I76 to I78)
T34, T36, T37 red then green.

## Phase 6: control socket and integration plugins (rows I79 to I81)
T35, T38 red then green; macOS Now Playing and Linux MPRIS.

## Phase 7: reachability, docs, validation (rows I82 to I84)
T28 red then green, ratchet 27, design doc section 14 status line, validation file.

## Ledger rows (thoughts/spot/todos.md, section "Implementation M1.2")

| ID | Phase | Task | Proof |
|----|-------|------|-------|
| I65 | 1 | Move the line parser to `src/protocol/parse.c`; CLI and shell share it | T1 to T27 green, no behaviour change |
| I66 | 1 | `prefs.def`, `prefs.c`, prefs file, PREF_GET/PREF_SET, config directory | T33 pref half green, ratchet 43 |
| I67 | 1 | T33 written and red; `format.c` resolver used by the view model | T33 green |
| I68 | 2 | T30 and T31 written and red | ctest failures |
| I69 | 2 | History ring; QUEUE_MOVE, QUEUE_SWAP, UNDO, REDO, QUEUE_HISTORY event | T30 move/swap/undo half green, ratchet 39 |
| I70 | 2 | xorshift32; QUEUE_SHUFFLE, SHUFFLE, RESHUFFLE | T30 green, ratchet 36 |
| I71 | 2 | SCOPE option, NEXT_ALBUM, PREV_ALBUM | T31 green, ratchet 33 |
| I72 | 3 | T29 written and red; four codecs | T29 green |
| I73 | 3 | PLAYLIST_LOAD, PLAYLIST_SAVE; divider/stealth queue rows | script check in T29 suite, ratchet 31 |
| I74 | 4 | T32 written and red; view grouping folder/format, node ids | T32 green, ratchet 27 |
| I75 | 4 | Text UI draws tree and drilldown | T24, T32 green; screen fixture |
| I76 | 5 | T34 written and red; `keybind.c`, `keys.conf`, `--bindings` | T34 green; T19 unchanged |
| I77 | 5 | T36 written and red; `cmdline.c`, history, `:!` spawn | T36 green |
| I78 | 5 | T37 written and red; `SEL_CMD_AREA`, COMMANDS `area`, `palette.c`, help overlay | T37 green, T5/T6 goldens updated |
| I79 | 6 | T35 written and red; socket in the shell, `selecta-cli send` | T35 green |
| I80 | 6 | `integration.h`, host side, `nowplaying-test`; T38 red then green | T38 green |
| I81 | 6 | macOS Now Playing and Linux MPRIS plugins | build green; manual check 2 |
| I82 | 7 | T28 written and red; fixtures | ctest failure |
| I83 | 7 | Wire everything to T28 | T28 green, T8 at 27, build/amiga green |
| I84 | 7 | Design doc section 14 status, validation file, full CI run (reason: landing the milestone) | `2026-10-23-m1-2-playlists-views-control.validation.md` |

## Verification

Automated: the finish-line table, run per phase on the affected tests only (`nice -n 19 ctest --test-dir build/host -R '<names>'`), the full CI run once at I84 (reason: landing the milestone).

Manual (only the user can confirm, never ticked by the agent):
1. LOCAL, macOS Terminal: `build/host/apps/selecta/selecta --start-dir ~/Music/mods`. PASS: `:PLAYLIST_LOAD <some .m3u or .prg>` fills the queue; `t` cycles regular, tree, drilldown and the cursor stays on the same song; Ctrl-P lists commands with bindings; `?` shows help grouped by area; `u` undoes a queue edit; a line `x NEXT` in `~/.config/selecta/keys.conf` makes `x` skip after a restart.
2. LOCAL, macOS: while `selecta` plays, the media keys (F7 F8 F9) and the Control Center Now Playing tile show the title and control play, pause and next. In a second terminal `build/host/apps/selecta-cli/selecta-cli send NEXT` skips. PASS: title visible, all three controls act.
3. LOCAL, Linux desktop (if available): `playerctl metadata` shows the title; `playerctl next` skips.

## Out of scope for M1.2 (goes to M1.3 or later)

Everything listed under M1.3; artist/album/genre grouping and smart playlists (M3, need the catalogue); PLAYLIST_PUBLISH (M3); CUE (later); title-format functions such as `$if` (later, on request); Windows SMTC (M5); ARexx (M4); OS-wide global hotkeys (user decision: not built).

## Deviations

- 2026-10-23, I71: PREV_ALBUM always goes to the first song of the previous album, as T31 and design doc section 3 (F30) say. The "Album navigation" decision above said it first returns to the start of the current album; T31 and the design doc win, the decision text is superseded.
- 2026-10-23, I70: `SHUFFLE on|off` only sets the `random` option; NEXT does not yet follow a random play order. Added row I71a: random play order (a permutation of the entries in scope, built with the xorshift32 shuffle when `random` turns on or the queue changes, NEXT and PREV walk it, every entry plays once before a repeat), with its own test T30a `queue-random-order`.
- 2026-10-23, I66: PREF_SET with an invalid title format answers `ERROR bad_format`; only a value loaded from prefs.conf falls back to the default. PREF_GET answers with a `PREFS_CHANGED key value` event.
- 2026-10-23, I67: queue rows format from the path only (title and file = basename, format = extension) until the M3 metadata store; browse rows are not formatted.
- 2026-10-23, I72: `sel_playlist_init` binds a caller array instead of an arena parameter; relative path resolution belongs to PLAYLIST_LOAD (I73); Hippo stealth written as 0xA2 plus key (question Q9); Hippo subsongs above 9 are dropped with `lossy subsong`.
- Phase 4 (I74, I75; report m12-phase4-report.md): tree and drilldown views apply to the queue only; browse stays flat until the library index exists (question Q13). Node ids are sequential from 1 in first-appearance order, not hashed; the group table holds 512 groups and overflow folds into a "(more)" group. VIEW_GROUP on a regular view switches it to tree. VIEW_DRILL with value `..` clears that level and deeper. VIEW_CHANGED carries only the style name; group key and drill values are in the snapshot. The help line is unchanged to keep the T19 golden (question Q14). `view-unit` and `view-tree` share a song directory, so they hold a RESOURCE_LOCK in tests/CMakeLists.txt. The `v` key now cycles view style, which clashes with the proposed viz key (question Q11). Ratchet measured 29 after the merge.
- I71a and I73 (report m12-i71a-i73-report.md): `SHUFFLE on [seed]` takes an optional seed (signature `ss?`). Queue entries gained a `flags` field (divider, hidden). PLAYLIST_LOAD `mode` is `append` (default) or `replace` (clears first, same undo record); other values give `ERROR range` (question Q15). Relative keys resolve against the playlist's directory; URLs, Unix absolute and Amiga volume paths are kept verbatim. Subsong n in a playlist plays replayer subsong n-1. PLAYLIST_SAVE to M3U or XSPF logs `playlist lossy divider`. The codec counted a trailing `?` wrong (the argument before it stayed required); fixed in src/core/codec.c with regression test `codec-optional-arg` (question Q16 asks for acceptance since it changes every `?` verb). The playlist-verbs script was not run red in the agent's tree; red was recorded on main dec988f instead. Ratchet measured 27 after both merges, the plan's target.
- Phase 5 (I76 to I78; report m12-phase5-report.md): extra default bindings `?` help, `u` UNDO, `C-r` REDO, `m` mark; keys.conf gained a `Ctrl-P` token and F1..F12 decoding. A scripted `--keys` run ignores keys.conf unless `--bindings` is given. `:!` uses the `m` marks (browse files only), else the file under the cursor. Command line history is in memory only because the host API has no file write (question Q18). Palette Enter sends the bare verb; verbs that need arguments are typed on the command line. The palette reads areas from `sel_cmd_area` (the table behind COMMANDS `area=`) instead of parsing COMMANDS output. No T5/T6 golden held COMMANDS output, so none changed. `selecta-cli` no longer links `selecta_core` directly (it came in twice through `selecta_textui`). The `#area` stringify kept quotes; fixed in src/core/codec.c.
