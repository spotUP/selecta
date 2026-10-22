---
date: 2026-10-23
topic: M1.2 and M1.3 requirements extracted from the architecture design (ledger L4)
status: research
---

# M1.2 and M1.3 requirements

DOC = thoughts/shared/plans/2026-10-22-architecture-design.md. Split source: thoughts/shared/plans/2026-10-23-m1-1-text-ui-queue.md:13-20. F-numbers are defined only in DOC section 1a (DOC:34-128) and section 16 (DOC:549); no separate features file defines them (grep F45/F48 in thoughts/ hits only DOC).

Implemented-verb state: SEL_CMD_IMPLEMENTED in src/engine/engine.c:33-60 lists 26 verbs (PLAY PAUSE PAUSE_PLAYBACK STOP VOLUME OUTPUT OPTION COMMANDS EVENTS TAGTYPES BEGIN END OPEN SUBSONG NEXT PREV SEEK QUEUE_ADD QUEUE_REMOVE QUEUE_CLEAR QUEUE_PLAY_NEXT LIBRARY_BROWSE VIEW_SHOW VIEW_CURSOR VIEW_ACTIVATE VIEW_WINDOW VIEW_STYLE). tests/ratchet/unimplemented.txt currently allows 45 (T8, tests/ratchet.cmake:1-4; adding a row to the table lowers it). Every verb named below that is not in those 26 is still unimplemented and each plan must lower the ratchet. Events in include/selecta/events.def already include VOICE, POSITION, SEEKED, QUEUE_CHANGED, QUEUE_HISTORY ("uu"), OPTIONS_CHANGED, VIEW_CHANGED, VIEW_ROWS.

## M1.2 (10 listed, 10 found)

### 1. Playlist codecs Hippo / M3U / PLS / XSPF
- DOC:1a row "Playlist | Hippo dividers + stealth entries round-trip; M3U, PLS, XSPF" (DOC:~68); DOC:425-435 (9c); DOC:551-557 (17).
- Location/shape: `src/library/playlist/formats/<fmt>.c` (hippo, m3u/m3u8, pls, xspf; cue later); read -> playlist, write -> serialise; lossy fields warn (DOC:433). Publishing = export Hippo + POST (M3, not here).
- Playlist model (DOC:427): entries = song key + optional subsong + optional display name; divider rows; id, title, author, notes, modified time.
- Hippo `.prg`: header `HiPPrg\n\n`; one entry per line; Amiga path or http(s) URL; optional `#name=<display name>` and `#song=<digit>`; divider = line whose first byte is 0xF7; stealth = 0xA2; max 131071 entries; source puu016.s load 13119, header 13523, save 13542 (DOC:553). HippoPlayer also reads M3U by extension and drops `#` lines. Round-trip dividers, stealth, `#song=`; URLs and Modland paths map to song keys via each source's `path_in`.
- M3U/M3U8: default export, `#EXTINF` duration and title. PLS read/write. XSPF: song key in `<location>`, subsong in `<extension>` (DOC:555). Streaming M3U URL-only import quirk honoured (row F-stream, M3, DOC section 1a).
- Commands: PLAYLIST_LOAD "s", PLAYLIST_SAVE "s" (both unimplemented). PLAYLIST_PUBLISH is M3.
- Open: no format selector in PLAYLIST_SAVE args (extension-driven vs explicit); how a `.prg` divider/stealth entry maps to queue rows (queue is a playlist, DOC:430); `path_in`/song-key resolution before M3 sources exist (only local in M1).

### 2. Tree and drilldown views
- DOC:417-423 (9b); row "Switchable view styles" (M1); DOC:310 text UI draws regular, tree, drilldown always.
- Tree: hierarchy by source / folder / format / artist / album, collapsible nodes, lazy children. Drilldown: Miller columns, e.g. source > format > artist > album > song, or genre > artist > song; each column filters the next. Grouping keys: source, format, artist, album/release, genre, year, rating, folder. Cursor and selection survive a style switch.
- Commands: VIEW_STYLE (impl), VIEW_GROUP "s", VIEW_EXPAND "u", VIEW_COLLAPSE "u", VIEW_DRILL "us" (all unimplemented; DOC:176 says VIEW_GROUP key, VIEW_DRILL level value). Events VIEW_CHANGED (style, group keys, row count) and VIEW_ROWS (window with depth, node id, artwork id) (DOC:178).
- Open: only local files exist in M1, so which grouping keys have data (folder, format; artist/album/genre need tags or library, M3); lazy child loading contract; level numbering for VIEW_DRILL.

### 3. Configurable columns and title formats (F48)
- DOC:324, row at DOC:124. Columns, title-format strings, status-line format are user settings resolved in the view model (9b); UIs receive ready rows and column widths and format nothing. Defaults come from the active view style; a bad format string falls back to the default with a warning.
- Commands: none dedicated; settings go through PREF_GET/PREF_SET "ss" (unimplemented; PREFS_CHANGED event exists).
- Open: format string syntax (placeholders) is not specified anywhere; pref key names; where column widths are computed for a given terminal width.

### 4. Vi command mode (F45)
- DOC:323, row DOC:122. `:` opens command line; completion from COMMANDS reflection output; each typed command = one protocol command, nothing UI-private. Second form: run an external program on the current selection with song paths as arguments (like the song-change hook, `OPTION hook_command <path>`, DOC:190). History kept in the UI profile.
- Commands: COMMANDS (impl). Verb typing reuses the wire codec of commands.def (header comment, arg letters s i u t ? *).
- Open: external-command syntax (prefix such as `!`), quoting, selection to path mapping for tree/drilldown nodes; where the UI profile lives; ARexx/Amiga spawn path (text UI also runs in UP-Term).

### 5. Command palette and help overlay (F49)
- DOC:325, row DOC:127. Fuzzy-search palette lists every COMMANDS entry with its current key binding; help overlay renders the same list grouped by area; undo/redo reachable from both.
- Commands: COMMANDS (impl); UNDO, REDO (unimplemented).
- Open: "area" grouping has no metadata in commands.def (the file has comment sections transport/queue/playlists/view/protocol only); fuzzy algorithm unspecified; key-binding display depends on item 6.

### 6. User key bindings (F11 part)
- Row DOC:86 "Global hotkeys + user key bindings in every UI" (shell, M1, H); DOC:305 `SEL_UI_HOTKEYS` (default key map, user-rebindable through the shell); DOC:307 shell owns global hotkeys; DOC:314 stored programs on F1-F10 are key bindings, not a separate mechanism.
- Commands: none exist for binding; bindings map a key to a protocol command (or command list via BEGIN/END, implemented).
- Open: no BIND/UNBIND verb or pref key is defined; file format and location of the bindings; "part" in the M1.2 split implies the rest (global hotkeys) is elsewhere, the design does not say which half is which; key-parser exists from I59 (text UI grid, key parser) so key names are already fixed there.

### 7. Queue undo and redo
- DOC:203 (F49), DOC:127. Every queue edit (add, remove, move, clear, reshuffle) pushes an inverse record on a fixed-size history ring allocated at start. UNDO/REDO pop and replay. Event QUEUE_HISTORY carries undo and redo depths (events.def "uu"). Playback position is never part of history.
- Commands: UNDO "", REDO "" (unimplemented); edits covered: QUEUE_ADD, QUEUE_REMOVE, QUEUE_CLEAR, QUEUE_PLAY_NEXT (impl), QUEUE_MOVE, QUEUE_SWAP, QUEUE_SHUFFLE, RESHUFFLE (unimplemented).
- Open: ring size ("fixed-size") is not given; whether consumed rows dropping on advance is an edit (design says only list edits); whether SHUFFLE and BEGIN..END batches are one history step.

### 8. Reshuffle
- DOC:200 (F30), row DOC:104. `RESHUFFLE` re-randomises the remaining queue order with the current seed policy. Pushes history (item 7).
- Commands: RESHUFFLE "" (unimplemented), related QUEUE_SHUFFLE "s?" (START:END range, DOC:180), SHUFFLE "s?" (smart is M5, DOC:204), OPTION random (impl).
- Open: "current seed policy" is undefined; no event beyond QUEUE_CHANGED.

### 9. Scope and album navigation (F30)
- DOC:200. `SCOPE library|playlist|album` limits what next, previous, shuffle may pick from; `NEXT_ALBUM`/`PREV_ALBUM` jump to the first song of the adjacent album in the current scope. Engine queue state only, rendered from the snapshot. Semantics in 9c (DOC:425-435).
- Commands: SCOPE "s", NEXT_ALBUM "", PREV_ALBUM "" (all unimplemented).
- Open: "album" has no data source until library tags (M3); in M1 an album must be defined (folder? SEL_META_RELEASE?) - undecided; scope's persistence (option vs state) and snapshot field name.

### 10. Cross-item note
Item 10 of the listed M1.2 set is the grouping above: the split line lists 8 comma items; with "tree and drilldown views" and "scope and album navigation" counted as one each, the listed count is 10 only if codecs are four (Hippo, M3U, PLS, XSPF are one bullet here). Counting rows: codecs, tree, drilldown, columns, command mode, palette, help overlay, key bindings, undo/redo, reshuffle, scope/album = 11 sub-items; all are covered in sections 1-9 above (tree and drilldown share one section; palette and help overlay share one).

## M1.3 (11 listed, 11 found)

### 1. Scopes (per-voice) and VU
- DOC:285-297 (6), row "Per-voice scopes, spectrum, VU" (M1, H B). Engine produces taps: per-voice scope buffers, VU, spectrum bands (FFT when float fast, integer bands otherwise), note/instrument per voice. Viz plugin: `SEL_VIZ_SURFACES`, `SEL_VIZ_TIER` text/planar/RTG/GPU; surfaces sel_surface / sel_textgrid / sel_gpu_ctx (DOC:289-293). Text UI scopes: bitmap surface as sixel/kitty with braille/block fallback (DOC:~317). RENDER kind hook `SEL_RP_RENDER(state, int16 stereo, int16* const* per_voice, frames)` carries per-voice PCM (DOC:~242). Voice info: `SEL_RP_VOICE_INFO`; event VOICE "ut" (per-voice note/instrument/volume).
- tags.def: VIZ category 0x4000 reserved "(M1)" (tags.def:10); no SEL_VIZ_* tags defined yet.
- Commands: none; taps are snapshot/viz-host data. Open: tap buffer sizes and sample rate of scope buffers, spectrum band count, VU ballistics (no numbers anywhere in DOC); whether a viz plugin kind is registered in M1 or the text UI draws taps directly; spectrum is listed in section 1a with scopes/VU but not in the M1.3 split (undecided if in scope).

### 2. Pattern scroller
- Row DOC:~46 "Pattern scroller + sample/instrument list with click-to-play" (viz + replayer hooks, M1, H); DOC:297 reads tracker cells. Hooks: `SEL_RP_TRACKER_INFO`, `SEL_RP_GRID_TICKS`, `SEL_RP_SAMPLE_COUNT` (DOC:~246). POSITION event "uuuu" (order/row/tick, frame) drives scroll.
- Open: tracker cell data format/ABI not defined in DOC; click-to-play needs sample play command (none exists) and a mouse path in the text UI (SGR mouse is in the terminal profile, DOC:315).

### 3. Phase view (F17)
- Row DOC:~124 "Phase view (stereo phase scope) (F17)", visualizer, text, M1; DOC:295 needs the stereo PCM tap (post-mix). No numbers. Open: tap exposure for Paula-direct path (no PCM, DOC:262) so phase view is mixer-path only.

### 4. Voice mute and solo
- Row "Channel mute / solo; pitch and tempo where the replayer allows" (M1, H); DOC:254 live in the engine, not plugins; `SEL_RP_CAN_MUTE` flag (DOC:~241). DOC:176 "MUTE voice on|off, SOLO voice".
- Commands: MUTE "us" (voice index, on|off), SOLO "u" (unimplemented), PITCH "i", TEMPO "i" (unimplemented; pitch/tempo wording "where the replayer allows" is a flag `SEL_RP_CAN_PITCH`).
- Open: SOLO toggle vs set semantics (arg is only the voice); max voices for mute mask; state exposure in snapshot; MUTE for RENDER kind needs per_voice buffers or openmpt mute API.

### 5. Loop detect, auto-stop and fade
- Row DOC:42 "Loop detect, auto-stop, fade (Songlengths + Hippo-style detection)" (engine, M1, H B); DOC:254 loop detection and fade in the engine. Songlengths from HVSC is M3 (DOC:373); SEL_META_DURATION_MS and `SEL_RP_DURATION` give length. Options: `stop-after-current`, `stop-after-queue`, `repeat` (DOC:182). Existing: engine.c:1362 treats a looping song as ended after first pass (repeat count 1).
- Commands: OPTION "ss" (impl) with names above; REPEAT "s" (unimplemented).
- Open: NO numbers: detection method (Hippo-style), fade length, fade curve (F29 says 16.16 fixed gain table, no float, for smart fade only), silence timeout, max play time default. All undecided. Hippo source puu016.s is the reference to read.

### 6. Crossfade and gapless
- Row DOC:40 "Crossfade + gapless (two replayer instances, RAM-scaled)" (engine, M1, B); OPTION crossfade (DOC:182); F29 smart fade/MixRamp is M5 and "falls back to plain crossfade (F1)" (DOC:205). Existing: engine already has two replayer slots swapped via spsc rings (engine.c:6-13).
- Commands: OPTION crossfade (impl verb, option unimplemented?). Open: crossfade duration default/unit and whether OPTION crossfade value = ms; gapless for VOICE-kind players (Paula-direct has no PCM mix; DOC:262 only mixer path can crossfade); RAM scaling rule is unquantified; fade curve (16.16 gain table is the only stated format).

### 7. Sleep timer and alarm
- Row DOC:67 "Sleep timer + alarm, engine side" (M1, H). Command SLEEP_TIMER "u" (unimplemented). No alarm verb exists in commands.def or DOC.
- Open: alarm verb/args/action (play what, when, wall clock vs relative), sleep timer units (minutes? seconds?) and end behaviour (fade? stop?), event on expiry, persistence across restart; ARexx/Amiga clock source.

### 8. Sample export
- Row DOC:85 "Sample export to WAV / 8SVX" (engine, M1); DOC:29 sample export kept, no editor; `SEL_RP_SAMPLE_GET` "sample list + export" (DOC:~239), `SEL_RP_SAMPLE_COUNT`; META_SAMPLES tag (tags.def:87).
- Commands: none in commands.def (no SAMPLE_EXPORT verb). Open: verb and args missing (sample index, path, format); 8SVX header details not given; whether libopenmpt adapter can supply samples (openmpt path) - only nuade documents SAMPLE_GET.

### 9. Crossfeed DSP (F18)
- Row DOC:~79; DOC:268 built-in `crossfeed` DSP plugin (integer Bauer-style: delayed, low-passed bleed between channels); enabled by `SEL_OUT_HEADPHONES` output tag (tags.def:59, 0x200D) or by user; `stereo separation` mixer tag covers the cheap case. DSP chain: `SEL_DSP_NAME`, `SEL_DSP_PROCESS(state, int16 stereo, frames)`, `SEL_DSP_NEEDS_FLOAT`, `SEL_DSP_PARAMS` (DOC:266); sits between mixer and PCM sink; never on Paula-direct. tags.def DSP category 0x3000 "(M1)" reserved, no tags yet.
- Commands: none for DSP chain (no DSP_* verbs); user enable would go via PREF_SET/OPTION. Open: filter cutoff, delay (samples) and bleed gain are unspecified; DSP chain ordering/enable command undefined (F39 says order is user-editable and in profile, M5).

### 10. Loop detect vs auto-stop vs fade (listed as three)
Covered in item 5: the split lists loop detect, auto-stop and fade as separate words but the design has one row; plans should split acceptance tests.

### 11. Counting
Listed in M1.3: scopes, VU, pattern scroller, phase view, voice mute/solo, loop detect, auto-stop and fade, crossfade and gapless, sleep timer and alarm, sample export, crossfeed = 11 comma items; all have design rows (sections 1-9 above; VU shares item 1, loop/auto-stop/fade share item 5, crossfade/gapless share item 6, sleep/alarm share item 7).

## Cross-cutting open questions
1. No numbers anywhere in DOC for: fade length, crossfade length, silence/loop thresholds, crossfeed coefficients, VU ballistics, scope buffer sizes, undo ring size.
2. Missing verbs: alarm, sample export, key binding, DSP enable, preference key names (columns/formats).
3. Album definition in M1 (no library until M3).
4. Format-string syntax for F48.
5. Vi external-command syntax.
6. Whether spectrum and note-dots (in DOC section 6 catalogue, not in the M1.3 split) are in scope.

## User decisions (2026-10-23)

- Album in M1 (F30): a directory is an album. NEXT_ALBUM/PREV_ALBUM jump to the next queue entry whose parent directory differs. M3 metadata replaces the source later.
- F11 split (revised the same day): no OS-wide global hotkeys. M1.2 gets user key bindings in the text UI, a local control socket driven by `selecta-cli send <verb>` (users bind keys in skhd, Hammerspoon, sxhkd or desktop shortcuts), and the macOS Now Playing / Linux MPRIS integration plugin moved up from M5 for media keys. Reason: no Accessibility or Input Monitoring prompts, works on Wayland.
- Viz scope: spectrum and note-dots join M1.3 next to scopes, VU and the pattern scroller (same PCM tap).
- Paula-direct path: phase view and crossfade work only on the mixed path (AHI and desktop). On Paula-direct they report unsupported through a capability tag.
- All other open questions (numbers, syntax, ring sizes, verb shapes) get a default in the M1.2/M1.3 plans, recorded under "Decisions taken in this plan".
