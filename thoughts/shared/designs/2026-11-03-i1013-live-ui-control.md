---
date: 2026-11-03
row: I1013
status: design (no implementation)
platform: Mac (desktop UI, SDL backend). Amiga: ARexx verbs are a follow-up row, exempt until then.
---

# I1013: live UI control over the control socket

Goal: a session drives a running Mac Selecta window (real-db copy) without touching the screen: feed input, read UI state
as text, dump a frame. Three socket verbs (`ui.input`, `ui.state`, `ui.frame`) and a thin stdio MCP server over the socket.

## D0. What exists and is reused (no second mechanism anywhere)

| Need | Existing piece | File / function |
|---|---|---|
| Socket listen/serve, 0600, one line in, one reply out | `sel_ctl_listen`, `sel_ctl_serve`, `sel_ctl_send` | `apps/common/ctlsock.c`, `ctlsock.h` |
| Verb dispatch + reply framing (`OK` / `OK\n<lines>\n.` / `ERROR ...`) | `ctl_handle` -> `sel_ctl_exec_fwd` | `apps/selecta/main.c` (~line 445), `apps/common/ctlexec.c` |
| Shell -> UI direct call precedent | `SEL_UI_THEME` tag, `shell_ui.theme`, `arexx_theme` | `include/selecta/tags.def:157`, `apps/selecta/main.c:65,292,421` |
| Input path | `desk_replay_input` -> `sel_gui_input_parse` -> `desk_route_input` / `desk_key` (SEL_GUI_IN_*) | `plugins/ui/desktop/desktop_ui.c:13956`, `src/gui/input/gui_input.c:107` |
| Replay script grammar | `click X Y [COUNT] [ctrl|shift]`, `dclick`, `rclick`, `move X Y [X2 Y2]`, `drag X Y X2 Y2`, `wheel ...`, `key NAME`, `wait MS`, `pinch`, `swipe`, `drop`, plus desktop extras `door N`, `tile N`, `topbar N`, `browse N`, `showall N`, `exec LINE`, `resize W H` | same two files |
| Live clock before routing | `desk_live_tick` | `desktop_ui.c:~13921` |
| Text state | `dump_put`, `library_dump_build` (24 KB `library_dump`, `nav page=... hist= pos=` line, `row I x y w h` lines) | `desktop_ui.c:6973, 6989, 7496` |
| Route history | `sel_route_hist d->rh` (`e[]`, `n`, `pos`), `sel_uim_page_at(page)->name` | `include/selecta/uimodel/route.h:57-72` |
| Frame writer | `desk_write_frame` (binary PPM P6 from `d->canvas` via `sel_gui_pixel`) | `desktop_ui.c:~14138` |
| PNG encoder | none in the repo (only `third_party/stb/stb_image.h`, a reader) | see D5 |
| Socket client for scripts | `selecta-cli send <line>` | `apps/selecta-cli` |
| Live loop in tests | `SDL_VIDEODRIVER=dummy` + `SELECTA_SDL_TEST_LIVE=1` | `tests/dui/idle_turns.cmake` |
| Socket ctest harness | `tests/m12/control_socket.sh` (relative `ctl.sock`, wait for `-S`, `selecta-cli send`) | `tests/CMakeLists.txt:1061` |
| MCP launcher pattern | `scripts/replay-mcp.sh` (sh wrapper, `exec python3 <server>`) | `scripts/replay-mcp.sh` |

## D1. Threading: there is no socket thread

The control socket is polled on the shell's main loop, which is the UI thread: `apps/selecta/main.c` loop calls
`g_uis[u].wait(...)` (the desktop's `desk_wait` -> `desk_wait_turn`, which polls SDL), then `pump_turn`, then
`sel_ctl_serve(ctl_fd, ..., ctl_handle, &cc)`. `ctl_handle` already runs on the UI thread (comment I906d at
`main.c:~452`: "a control command runs on the UI thread too"). So:

- No queue or mailbox is needed and none is added. `ui.*` verbs call the UI directly from `ctl_handle`, exactly like
  ARexx `THEME` calls `u->theme(u->state, path)`. The desk state is touched only by its own thread.
- Replies are synchronous for free: the handler fills `reply` before `sel_ctl_serve` writes it.
- Input that waits for an engine answer (`pointer_sent` in `desk_replay_input`) resumes on the next UI turn through the
  existing `input_at` / `input_done` resume, the same as `--input`. The reply says `pending=1`; the client polls
  `ui.state` until `input_pending=0` (D3).
- Constraint: `sel_ctl_serve` waits up to one turn for a connection, so each socket verb costs at most one shell turn of
  latency. Acceptable; do not add a thread.

## D2. The UI hook: `SEL_UI_CONTROL`

- `include/selecta/tags.def`: add `SEL_TAG(UI, CONTROL, 0x50E3, PTR)` (0x50E3 is free, checked 2026-11-03) with a comment:
  `int fn(void* state, const char* line, char* reply, size_t cap)`; returns 0 = OK, non-zero = ERROR; `line` is the text
  after `ui.`.
- Stats tags: `SEL_UI_STAT_CTL_LINES 0x50E4 INT` (input lines fed through the control path, the sentinel),
  `SEL_UI_STAT_CTL_STATES 0x50E5 INT`, `SEL_UI_STAT_CTL_FRAMES 0x50E6 INT`. Re-grep before use.
- `include/selecta/ui.h`: `typedef int (*sel_ui_control_fn)(void* state, const char* line, char* reply, size_t cap); /* UI_CONTROL */`
- `apps/selecta/main.c`: `shell_ui` gets `sel_ui_control_fn control;` filled next to `u->theme` (line ~292) with
  `sel_tag_find(h, SEL_UI_CONTROL, 0)`.
- `apps/selecta/main.c` `ctl_handle`: before `sel_ctl_exec_fwd`, if `strncmp(line, "ui.", 3) == 0`:
  primary UI `g_uis[0]` with `control != NULL` -> call it with `out`; frame the reply with the existing three-way
  `ERROR` / `OK\n...\n.` / `OK` code (no new framing). No control hook -> `ERROR ui no UI control`. Count it like other
  accepted lines (`return rc == 0`) so `socket_cmds` includes it. Also time it into `g_pump_ms_max` like the rest.
- `plugins/ui/desktop/desktop_ui.c` `SEL_PLUGIN_ENTRY`: `set(&g_hooks[7], SEL_UI_CONTROL, SEL_TAG_FN(desk_control));`,
  END moves to `g_hooks[8]` (check the array size of `g_hooks`).
- MUI and text UIs: no hook now (reply `ERROR ui no UI control`).

## D3. Verb grammar and replies

All verbs are one socket line (the protocol reads to `\n`). Multiple input lines are separated by `;`.

### ui.input <line>[; <line>...]

Each part is one replay-script line, unchanged grammar (D0 row "Replay script grammar"). Examples:

```
ui.input click 120 340
ui.input move 400 300; wait 200; click 400 300
ui.input drag 600 200 600 500
ui.input wheel 0 -3
ui.input key Down; key Return
ui.input door 2
```

Replies:
- `OK\nfed=<n> pending=<0|1>\n.`: `fed` lines ran this call; `pending=1` means a line waits for an engine answer and the
  rest runs on later turns.
- `ERROR ui busy`: an earlier `ui.input` is still pending.
- `ERROR ui empty` / `ERROR ui too long` (over 4096 bytes, the `--input` buffer size).

Implementation (`desktop_ui.c`):
- Split `desk_replay_input(d)` into `desk_replay_lines(desk* d, char* text)` (the loop, unchanged, including the
  `input_at` resume) and the `--input` wrapper that keeps the `input_consumed` / once-only bits. The control path calls
  `desk_replay_lines`; it never sets `*d->input_consumed` (that flag belongs to `--input`, I246).
- Buffer: reuse `d->input` (allocate the 4097-byte buffer on first `ui.input` when `--input` was not given). Copy the
  text, turn `;` (and surrounding blanks) into `\n`, set `d->ctl_input = 1`, `d->input_done = 0`, `d->input_at = NULL`.
- Before feeding on a live window call `desk_live_tick(d)` (same as `desk_sdl_route`), so tweens start on the current clock.
- Resume gate in `desk_wait_turn` (`desktop_ui.c:~15274`): `d->ticks == 0u` becomes `(d->ticks == 0u || d->ctl_input)`;
  clear `ctl_input` when the text is used up (`input_done && input_at == NULL`).
- `d->ctl_lines++` per line handled inside the loop when `ctl_input` is set (the sentinel, D7).
- `wait MS` keeps its meaning (`desk_advance`, the UI's own timeline step). In a live window real time also passes
  between socket calls; the MCP tool offers a client-side sleep for real waits (D6).

### ui.state [dump <path>]

Plain `ui.state` reply (`OK\n<lines>\n.`), one fact per line, `key=value` or fixed positional fields, all built with
`dump_put`-style appends into a reply buffer capped at `cap` (16 KB minus framing); a cut reply ends with `truncated=1`:

```
page=library
canvas=1280x800 density=2
route n=3 pos=2
route 0 page=home key= name= scroll=0
route 1 page=library key= name= scroll=0
route 2 page=composer key=Jogeir name=Jogeir Liljedahl scroll=120 *
focus ring 240 180 820 32 kb_list=1
hover tile=0 shelf=0 row=4
scroll lib=120 queue=0 songs=0
rows first=0 count=18
row 0 240 148 820 32 Jogeir Liljedahl
row 1 240 180 820 32 Lizardking
...
input_pending=0
ctl lines=3 states=2 frames=0
```

- `page`: `sel_uim_page_at(d->page)->name` (as the `nav page=` dump line).
- `route`: `d->rh.e[0..n-1]` with `sel_uim_page_at(e.page)->name`, `key`, `name`, `scroll`; `*` marks `pos`.
- `focus`: `d->ring` and `kb_list`; `hover`: `hover_tile`, `hover_shelf1`, and the list row under the pointer (builder:
  find the index the `row_hover_fill` paint uses; if only the fill is kept, add the index next to it in the same paint).
- `scroll`: `lib_q16`, `queue_q16`, `scroll_q16` shifted to whole logical pixels.
- `rows`: the visible rows of the current list page with their click rectangle, from the same rect functions the
  dump uses (`lib_row_rect(d, panel_rect(d, "main"), i)` for Library; Home: `tile_rect` / `shelf_tile_rect`). Label is the
  display text the row draws. One helper `desk_visible_rows(d, emit)` so `library_dump_build` and `ui.state` share it if
  the dump grows the same need (no second copy of the row geometry).
- `ui.state dump <path>`: runs `library_dump_build(d)` (the existing full text dump, 24 KB, too big for one reply) and
  writes it to `<path>`; reply `OK\nwrote=<bytes>\n.`. This is the escape hatch for anything the short form lacks;
  no fields are duplicated into a new dumper.
- `d->ctl_states++`.

### ui.frame <path>

```
ui.frame /Users/spot/tmp/shot.png
ui.frame /Users/spot/tmp/shot.ppm
```

Writes the current `d->canvas` (device pixels) after a present. `.ppm`: `desk_write_frame` as is. `.png`: D5.
Reply `OK\nwrote=<path> w=<w> h=<h>\n.`, `ERROR ui frame <errno text>` on open failure, `ERROR ui frame path` when the
path is not absolute. `d->ctl_frames++`.

## D4. Desktop dispatcher

`static int desk_control(void* state, const char* line, char* reply, size_t cap)` in `desktop_ui.c`, next to
`desk_replay_input`: `input ` -> D3 input, `state` / `state dump ` -> D3 state, `frame ` -> D3 frame, else
`ERROR ui unknown verb`. Under `#ifndef __amigaos__` only for `frame` PNG if stb_image_write does not build there; the rest is
portable (the ARexx follow-up reuses this same hook).

## D5. PNG writing

No PNG encoder exists in the repo. Decision (default): vendor `third_party/stb/stb_image_write.h` next to the already
vendored `stb_image.h` (same project, public domain) and give `desk_write_frame` a PNG branch by extension:
build an RGB row buffer from `sel_gui_pixel` and call `stbi_write_png`. Do not hand-write a deflate/CRC encoder.
One `#define STB_IMAGE_WRITE_IMPLEMENTATION` in one translation unit (a small `plugins/ui/desktop/desk_png.c` keeps it
out of the huge file). Exit-time `--frame-dump` gains PNG for free through the same function.

## D6. MCP server

- `scripts/selecta-ui-mcp.py`: stdio JSON-RPC MCP server (initialize, tools/list, tools/call), Python stdlib only, run with
  `python3 -I`. Socket client: `AF_UNIX` connect to the path from `$SELECTA_SOCKET`, else the same fallback order as
  `sel_ctl_path` (`$XDG_RUNTIME_DIR/selecta.sock`, `$TMPDIR/selecta-<uid>.sock`), send one line, read to EOF, one
  connection per line (the `sel_ctl_send` contract).
- `scripts/selecta-ui-mcp.sh`: launcher in the `replay-mcp.sh` style (`exec python3 -I "$(dirname "$0")/selecta-ui-mcp.py"`).
- Tools:
  - `ui_state()` -> `ui.state` text.
  - `ui_input(lines: string[], settle_ms?: int)` -> joins with `; `, sends `ui.input`; when `pending=1`, polls `ui.state`
    every 50 ms until `input_pending=0` or `settle_ms` (default 3000) passes; returns the last reply.
  - `ui_frame(path: string)` -> `ui.frame`. The caller gives the image to a subagent, never the main session (global rule).
  - `ui_dump(path: string)` -> `ui.state dump <path>`.
  - `ui_sleep(ms: int)` -> real-time wait, client side.
  - `selecta_send(line: string)` -> any protocol line (`PLAY`, `QUIT`, `COMMANDS`), raw reply.
- Registration for a session: `claude mcp add selecta-ui -- sh scripts/selecta-ui-mcp.sh` (document in the script header,
  not in a new doc).

## D7. Sentinel and stats

- Desk counters `ctl_lines`, `ctl_states`, `ctl_frames` exported through `desk_stats` with the D2 tags and printed by the
  shell `--stats` block as `ui_ctl lines=<n> states=<n> frames=<n>` (one `printf` next to `socket_cmds=`).
- `ctl lines=` also appears in every `ui.state` reply, so the live test reads it without quitting.
- Proof the path ran: `ui_ctl lines>=1` and the page change. `lines` counts only lines that entered
  `desk_replay_lines` with `ctl_input` set, so a stub `ui.input` that answers OK without feeding leaves it 0.

## D8. ctests (Mac; the ctest names are new rows in `tests/CMakeLists.txt` next to `control-socket`)

1. `ui-control-socket` (`tests/m12/ui_control.sh <selecta> <selecta-cli> <songs> <work>`), pattern
   `tests/m12/control_socket.sh`:
   - `cd $WORK`, `SELECTA_SOCKET=ctl.sock` (relative, sun_path), `SDL_VIDEODRIVER=dummy`, `SELECTA_SDL_TEST_LIVE=1`.
   - Start `selecta --ui desktop --backend sdl --view library --start-dir <songs fixture> --canvas 1280x800 --stats --output null` in the background; wait for `-S ctl.sock`.
   - `selecta-cli send "ui.state"`: assert `page=library`, `route n=1`, read `row 0 X Y W H`.
   - `selecta-cli send "ui.input click <cx> <cy>"` (row 0 centre; if the Library opens a row on double click, use
     `dclick`, whichever `desk_pointer` opens on: builder checks and writes the reason in the script comment).
   - Poll `ui.state` (max 50 x 0.1 s) until `input_pending=0`; assert the page or route changed (`route n=2`, new `page=`
     or the drill entry), and `ctl lines=1`.
   - `selecta-cli send "ui.frame $WORK/f.png"`: file exists, starts with `\x89PNG`.
   - `selecta-cli send QUIT`; `wait`; assert `ui_ctl lines=1 states>=2 frames=1` in stdout and the socket file is gone.
   - Unknown verb: `ui.nosuch` -> rc 1, reply starts `ERROR`.
2. `ui-control-mcp` (`tests/m12/ui_mcp_smoke.sh`): same launch; pipe `initialize`, `tools/list`, `tools/call ui_state`
   JSON-RPC lines into `python3 -I scripts/selecta-ui-mcp.py`; assert the tool list holds exactly the D6 names and the
   `ui_state` result contains `page=library`; then `QUIT`.
- Both labelled to run in the normal Mac ctest set; caps per global 6a-1 (single process, no parallel suite).

## D9. Revert proof (write the results into the agent report)

1. Make `desk_control` `input` reply `OK\nfed=0 pending=0\n.` without calling `desk_replay_lines`: `ui-control-socket`
   must fail on the page assertion and on `ctl lines=1`.
2. Remove the `ui.` branch from `ctl_handle`: the test must fail (`ERROR` from the engine parser).
3. Restore; both tests pass.

## D10. Security

- The socket already binds under `umask(0177)` (mode 0600, owner only) and is `AF_UNIX` only: no TCP listen, nothing
  reachable off the machine. No change, and the MCP server must not open any network port either (stdio only).
- Default path is per user (`$XDG_RUNTIME_DIR` or `$TMPDIR/selecta-<uid>.sock`). The same user can already send `exec`
  engine lines and `QUIT`; `ui.*` adds no privilege beyond what the owner has.
- `ui.frame` / `ui.state dump` write only to absolute paths the caller names, opened with `O_CREAT|O_TRUNC` and the
  user's umask; refuse relative paths and paths longer than `PATH_MAX`. No reads of arbitrary files.
- Input lines are length-capped (4096) and parsed by the existing parser; no shell, no format string from the client
  (all `dump_put` calls use fixed formats with `%s` for client text).

## D11. Amiga

Exempt for this row (RULES 4a exemption: socket is POSIX-only, the ARexx follow-up row adds `UI.INPUT` / `UI.STATE` /
`UI.FRAME` through `arexx_exec` calling the same `SEL_UI_CONTROL` hook, which the desktop UI on AGA/RTG already exposes).
The desktop plugin must still build for Amiga (no stb_image_write problem: guard D5 if it does not compile with vbcc/gcc-m68k).

## Build checklist (sonnet, in order; tick in the ledger row's report)

- [ ] B1 `tags.def`: `UI_CONTROL 0x50E3 PTR`, `UI_STAT_CTL_LINES/STATES/FRAMES 0x50E4..E6 INT` (re-grep the ids first). `ui.h`: `sel_ui_control_fn`.
- [ ] B2 `main.c`: `shell_ui.control`, filled at line ~292; `ctl_handle` `ui.` branch with existing reply framing; `ui_ctl lines= states= frames=` stats line.
- [ ] B3 `desktop_ui.c`: split `desk_replay_input` into `desk_replay_lines` + `--input` wrapper; no behaviour change for `--input` (run the dui tests that use `--input` touching the replay: affected subset only).
- [ ] B4 `desktop_ui.c`: `ctl_input` flag, buffer reuse, `;` split, `desk_live_tick` before feeding, resume gate change in `desk_wait_turn`, `ctl_lines++`.
- [ ] B5 `desktop_ui.c`: `ui.state` short form (D3 fields), `desk_visible_rows` helper shared with `library_dump_build`, hovered row index, `ui.state dump <path>`.
- [ ] B6 vendor `third_party/stb/stb_image_write.h`; `plugins/ui/desktop/desk_png.c`; PNG branch in `desk_write_frame`; CMake source list; Amiga build check.
- [ ] B7 `desk_control` dispatcher + `SEL_UI_CONTROL` in `SEL_PLUGIN_ENTRY` hooks; counters into `desk_stats`.
- [ ] B8 `tests/m12/ui_control.sh` + ctest row `ui-control-socket` (D8.1).
- [ ] B9 `scripts/selecta-ui-mcp.py` + `scripts/selecta-ui-mcp.sh` (D6).
- [ ] B10 `tests/m12/ui_mcp_smoke.sh` + ctest row `ui-control-mcp` (D8.2).
- [ ] B11 Revert proof D9 (both steps), results in the report.
- [ ] B12 Type-check/build Mac + Amiga desktop plugin build; run only the two new ctests plus the `--input` dui tests B3 touched.
- [ ] B13 Live check on a real-db copy: start the Mac window with `SELECTA_SOCKET`, MCP `ui_state`, `ui_input` door click, `ui_frame` to a PNG handed to a subagent for a look; record in the report.

## Open questions for Johan

1. D5: vendor `stb_image_write.h` (default) vs PPM only with the MCP converting via macOS `sips`. Default keeps PNG in C and testable on any POSIX host.
2. Gate: `ui.*` verbs on whenever the socket is open (default; socket is owner-only) vs only with an extra env (`SELECTA_UI_CONTROL=1`).
