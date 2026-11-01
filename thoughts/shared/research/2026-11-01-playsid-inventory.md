---
date: 2026-11-01
topic: HippoPlayer-68k playsid.library integration inventory and Selecta port plan (ledger I434a)
tags: [research, amiga, sid, playsid, hippoplayer, i434a, hvsc, per-tier-owners]
status: complete (code reading only; no rig, no build, no code edits)
---

# playsid.library integration inventory (I434a, step 1)

Sources read: `~/Code/HippoPlayer-68k/code/puu016.s` (P = this file; line numbers verified by grep against the ledger's numbers), `code/playsid.library/{playsid.asm,README.txt,playsid_lib.i}`, `code/dist/hippoplayerupdate.doc` section 11 (lines 170-192), `code/LICENSE.md`. Selecta: `plugins/replayers/sid/adapter.c`, `plugins/replayers/kplayer/adapter.c`, `plugins/sources/hvsc/*`, `include/selecta/{replayer.h,prefs.def,viz.h}`, `src/engine/{registry.c,engine.c}`.

Corrections to the ledger text: line 39498 is `SetRESIDMode` (oversampling 0-3), the operating mode itself is set by `SetOperatingMode` at about P:39470 (`SetRESIDMode` is a different LVO, playsid.asm:841). `SetOperatingMode` is playsid.asm:830, `GetOperatingMode` :849. Hippo never calls `RewindSong`; the library has it (playsid.asm:1310) but Hippo's only seek is `ForwardSong`, and only in non-reSID mode (P:39795, guarded by the check at about P:39790: "Do not ffwd with special modes").

## 1. Shape of the Hippo integration

One player block `p_sid` (P:39221, type `pt_sid`) with entry points `.init` (P:39250), `.song` (subsong change), `.end` (P:39767), `.stop` (pause, P:39774), `.cont` (P:39779), `.eteen` (forward, P:39795), `.volume` (P:39802), `.free`. State bytes: `sidmode`, `residmode`, `residfilter`, `residboost`, `sidflag`, `maxsongs`, `songnumber`, `kokonaisaika`, `slIndexPtr`, `sidSongLengthData`, `residBufPtr1..3` (P:586, 907, 1020, 1583-1600).

## 2. Function-by-function table

Selecta shorthand: SID = `plugins/replayers/sid/adapter.c` (libsidplayfp, RENDER kind, host builds, 169 lines); KP = `plugins/replayers/kplayer/adapter.c` (the only HW-kind Amiga native wrapper, template for a playsid adapter: hooks at :318-343 incl. `SEL_RP_MIN_CPU` :330, `SEL_RP_PRIORITY` :331, `SEL_RP_HW_PAUSE/RELEASE/POLL` :326-328); REG = `src/engine/registry.c` (owner pick by priority + min CPU, :62-:88); PREFS = `include/selecta/prefs.def` (append-only `SEL_PREF` rows, last AMIGA_* rows :102-104); HVSC = `plugins/sources/hvsc/`.

### Group 0: open, close, availability (precondition for all groups)

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| `get_sid` (P:33585-33610) | `OldOpenLibrary("playsid.library")`, reads `LIB_VERSION/REVISION/IDSTRING` | Lazy open of LIBS:playsid.library; no lib means init returns `ier_nosid` | New plugin `plugins/replayers/playsid/adapter.c`, copy KP skeleton; absent lib: `rp_probe` answers NO so the registry falls back to nuade/no_replayer (the I434a requirement) |
| `isPlaysidReSID` (P:40274) | LIB_VERSION 1, REVISION >= 9 | Gate: is this the reSID/SIDBlaster-capable library | New, same file (`ps_has_resid()`) |
| close in shutdown (P:3820-3836) | `rem_sidpatch`, `CloseLibrary` | Frees STIL/SL data, patch, library | `rp_close` + plugin teardown |
| `playSidInRESIDMode` (P:40309) | `GetOperatingMode` (d0, d1) | True when mode is 6581/8580/auto; scopes and ffwd depend on it | New, same file |

### Group 1: PlaySID mode preference (Normal, reSID 6581/8580/Auto, SIDBlaster, ZorroSID, USBSID-Pico, Trinity)

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| Mode switch `.m1..m7` (P:39287-39370) | `SetOperatingMode(d0)` with `OM_NORMAL/OM_RESID_6581/OM_RESID_8580/OM_RESID_AUTO/OM_SIDBLASTER_USB/OM_ZORROSID/OM_USBSID_PICO/OM_TRINITY` (about P:39470) | Maps pref byte `sidmode` 0-7 to the library operating mode before `AllocEmulResource` | New pref `playsid.mode` in PREFS (values normal,6581,8580,auto,sidblaster,zorrosid,usbsid,trinity); plugin reads it via host pref getter in `rp_start` |
| `sid_getSidVersion` (P:39891) | none (reads PSID header flags) | Auto mode picks 6581 vs 8580 label from the file's SID model bits | New, same file (shared with the info text) |
| Mode text into title (P:39445-39495, `.mode/.o1`) | none | Writes "reSID 8580 14-bit" / "AHI" / "SIDBlaster" into the info line; `ps3m_sampleDataModulo` for scopes | Selecta format label: `SEL_RP_METADATA` tag (SID `rp_metadata` :114) |
| AHI mode (P:39500-39520) | `SetAHIMode(d0)` or NULL | reSID output through AHI instead of Paula 14-bit | New pref `playsid.ahi_mode` (8-digit hex id) only if the I871 per-output owner work wants it; otherwise follow the global output sink (open question Q2) |
| `AllocEmulResource` / `FreeEmulResource` (P:39523, 39734) | same names | Takes and gives back SID hardware or emulation buffers; error path maps `SID_NOSIDBLASTER` etc. to messages (P:39709-39735) | `rp_start` / `rp_hw_release` in the new plugin; error text via host log |
| Prefs UI + load/save (P:15860, 16012, 18617-18736; P:13964, 14266, 14768, 15252) | none | Prefs > Play-2 requester cycling gadgets, stored in the prefs struct | Selecta MUI/Mac settings page + PREFS rows; the settings page is wired from the PREFS table (see `mui-settings` test) |

### Group 2: reSID filter and boost, oversampling

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| Filter (P:39604-39641) | `SetRESIDFilter(d0 = internal on, d1 = external on)`; pref 0 = internal, 1 = int+ext, 2 = all off | Applied after `AllocEmulResource`, only in reSID modes | New pref `playsid.filter` (internal, internal_ext, off) |
| Boost (P:39631-39641) | `SetRESIDBoost(d0)`; pref 0-3 (off,2x,3x,4x) gets +1 because 0 and 1 are both no-ops | Output gain | New pref `playsid.boost` (off,2x,3x,4x) |
| Oversample (P:39495-39498) | `SetRESIDMode(d0)` 0=normal, 1-3 = 2x-4x | reSID quality | New pref `playsid.oversample` (normal,2x,3x,4x) |
| Prefs UI (P:18682-18736) | none | Filter names "Internal on / Internal+Ext / All off", boost "Off 2x 3x 4x" | Same settings page as group 1, full English labels |

### Group 3: mono, 2SID, 3SID

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| `.sidIsStereo` (P:39863) | none | PSID version >= 3 and header byte $7a (SID2 base) non-zero | New helper `ps_sid_count(const uint8_t* hdr)`; libsidplayfp side already counts SIDs, SID: `rp_open` :37 |
| `.sidIsThree` (P:39875) | none | version >= 4 and byte $7b non-zero | same helper |
| Title/label per SID count (P:39530-39580) | none | "stereo"/"3SID" in the info line; 3SID is 14-bit on SID1 only, 8-bit on SID2/3 (P:39460-39475) | `rp_metadata` tag text |
| Output channel layout | `GetResidAudioBuffer` returns up to 3 buffers (a0, a1, a2) | One buffer per SID | Group 6 (scopes) and the HW-kind output (library plays through Paula/AHI itself, Selecta never sees the audio) |

### Group 4: subsong and transport

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| `SetModule` (P:39649) | `SetModule(a0 = file image)` | Hands the PSID image to the library (playsid.asm:942); Hippo also copies the header name into the title (P:39655-39662) | `rp_open`: keep image alive until close; SID `rp_open` :37 shows the metadata fields |
| Subsong count/default (P:39664-39676) | none | `maxsongs = sidh_number-1`, default song from header `sidh_defsong` | `sel_rp_info` subsong count: SID `rp_open` :37, `default_subsong` in `sid_state` :19 |
| `.sanko` + `StartSong` (P:39716-39752) | `SetVertFreq(d0)` (PAL/NTSC), `StartSong(d0 = subsong)` (playsid.asm:1148) | Selects clock and starts a subsong; also used for next/previous subsong | `rp_start(state, subsong, rate, voices)` (SID :75); clock from header flags |
| `.song` (P:39760) | re-run `.sanko` | Subsong change without reinit | `rp_start` again, engine already drives it |
| `.end` (P:39767) | `StopSong` + `FreeEmulResource` | Stop | `rp_hw_release` |
| `.stop` / `.cont` (P:39774, 39779) | `PauseSong`, `ContinueSong` (playsid.asm:1248, 1264) | Pause/resume | `rp_hw_pause(state, paused)` (KP :326) |
| `.eteen` (P:39795) | `ForwardSong` (playsid.asm:1290); skipped when `GetOperatingMode` is a reSID/special mode | Fast forward, classic mode only | `rp_seek` (KP :343): classic mode only; reSID mode returns "not seekable" and the UI greys the bar (SID uses render-and-discard, :97, not possible here) |
| `RewindSong` (playsid.asm:1310) | not called by Hippo | exists in the library | optional, decision Q3 |
| `.volume` (P:39802-39811) | `SetVolume(d0)` (playsid.asm:9275), only when reSID variant | Master volume | Output sink volume if the engine forwards it; else none (Q4) |
| Position | none in Hippo: `kokonaisaika` is the length, elapsed is wall clock from the task | Elapsed time | `rp_hw_poll` (KP :328) end-of-song; `SEL_RP_POSITION` (KP :344) from a tick counter |

### Group 5: HVSC Songlengths and STIL

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| `loadSLIndex` (P:62349) / `createSLIndex` | none | Builds an MD5 to offset index of Songlengths.md5 in memory | NOT ported. Selecta already syncs Songlengths.md5 into the catalogue: `sel_hvsc_sync` (HVSC `hvsc_sync.h:42`), `sel_hvsc_parse_time_ms` (:38), tags `subtunes` and `stil.<field>` / `stil.<n>.<field>` (:18-21), net fetch I162a (`hvsc_net.c`). Reuse, never a second parser |
| `sid_readSongLength` (P:40897) | none | `getStilHash(filename)` then index lookup, fills `sidSongLengthData` (word per subsong, seconds) | New: the playsid plugin reads the catalogue length tag for the item; the SID adapter's own comment (:1-4) already says the length database supplies lengths. Need to confirm the tag name and whether per-subsong lengths exist (Q5) |
| `sid_getSongLength` (P:40977) | none | Sets `kokonaisaika` and the `pf_end` flag so end-of-song fires | `rp_hw_poll` returns "ended" when elapsed >= catalogue length (HW slots never tick, replayer.h :131); `max_song_ms` as fallback |
| `sid_freeSongLengthData` (P:40970), `freeSLData` (P:62374) | none | frees | none (catalogue owns it) |
| `getSTILInfo` (P:61472) / `doGetSTILInfo`, `freeSTILData` (P:61842); insertion in info (P:23648-23737) | none | Looks up STIL text by path (path must be HVSC-structured) and appends composer/title trivia to the info window | NOT ported: catalogue `stil.*` tags already exist; the info view (`mui-songinfo`) shows tags; add the `stil.*` rows to the song-info panel if absent (check `src/uimodel` songinfo) |

### Group 6: scopes (PSID PatternScope and the reSID audio feed)

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| Enable feed (P:39270-39278) | `SetDisplaySignal(d0 = mask 0, a0 = task)`, `SetDisplayEnable(1)` (playsid.asm:904, 910) | Library fills display buffers, no signals sent | Plugin start |
| Buffer fetch (P:39680-39700) | `GetResidAudioBuffer` (playsid.asm:9359): a0/a1/a2 = per-SID buffers, d0 = length, d1 = period | reSID output samples for the oscilloscope; Hippo fakes PS3M scope variables (`ps3m_sampleDataModulo`) | New. Today only VOICE/RENDER kinds feed scopes: `engine.c:8079` `sel_viz_begin` fills per-voice scope only when `sl->rp_render == NULL` and the mixer exists. HW-kind has none, so a new replayer hook (proposed `SEL_RP_VIZ_VOICES`, reading up to 3 buffers into `sel_viz_frame.scope[k]`, viz.h :17-37) is needed |
| `sid_followPositions`, PSID PatternScope (P:1600, 754 `taskPatternScope`, 16375 `rPatternScope`) | none (reads SID register mirror via library base, P:30434 `playsidbase+50`) | Register-state pattern scope | Defer: Selecta has no register-scope viz kind; row notes it as optional (Q6) |
| Classic (non-reSID) mode | none | No audio buffer; Hippo shows no wave scope | Scope grey for classic mode |

### Group 7: CPU check

| Hippo routine (line) | playsid call | What it does | Selecta target |
|---|---|---|---|
| `.cpuCheck` (P:39380-39386) | `AttnFlags & AFB_68020` | reSID needs 68020+ (`ier_hardware`) | `SEL_RP_MIN_CPU 68020` for reSID modes. Mode is a pref, but min CPU is a static tag: either two descriptors (`playsid` min 68000 classic, `playsid_resid` min 68020/68040) or the probe returns NO for reSID when `sel_machine` cpu < 68020 (REG :62-88 applies the tag; pref-dependent check goes in `rp_probe`) |
| `MeasureRESIDPerformance` (P:39403) | `MeasureResidPerformance(d0 = oversample mode, d1 = int filter, d2 = ext filter)` -> d0 ms measured, d1 reference limit (playsid.asm:10277); d1 halved for 2SID, divided by 3 for 3SID (P:39409-39425) | Benchmarks a frame; if measured > limit, `.performanceRequest` (P:39780) asks "Your Amiga could be too slow! Continue anyway / Stop" | New: benchmark once per (oversample, filter, SID count) combination (cached like Hippo's `.perfMode`), confirm through the host dialog path (UI dialog verbs in `src/uimodel`, `mui-dialogs`); on Stop fail `rp_start` |

## 3. Proposed build rows

Order: b is the foundation, the rest hang off it.

| ID | Group | Row | Done when | Rig check |
|---|---|---|---|---|
| I434b | 0, 1, 4 (core) | Native `playsid` replayer plugin, HW kind, copy of the KP skeleton: probe NO when playsid.library missing, SetModule/StartSong/Stop/Pause/Continue/Forward, mode pref `playsid.mode` (all 8 modes), priority above nuade/libsidplayfp on the Amiga tiers, no_replayer fallback when library absent. Subsong next/previous through `rp_start` | One reachability test drives the Amiga entry (vamos or stub library with call-count sentinel) and asserts SetModule/StartSong ran, mode pref moves `SetOperatingMode` arg, absent library falls through to the next owner. Host build still plays SID via libsidplayfp (test sid_decode unchanged) | Rig 2 (256 MB), real .sid from HVSC (not a test tune), classic mode: sound, pause, continue, subsong change, ffwd. Purpose: "<session>: playsid classic" |
| I434c | 2 | reSID filter, boost, oversample prefs (3 PREFS rows + settings page rows, full English labels) | Each pref changes the `SetRESIDFilter/Boost/Mode` argument seen by the sentinel; defaults match Hippo (internal, off, normal) | Rig 2 with 68040/060 profile: reSID 6581 tune, toggle filter and boost, hear and log arguments |
| I434d | 3 | 2SID/3SID detection and labels | `ps_sid_count` unit test over a mono, a 2SID and a 3SID header (three real HVSC files); info label shows "2SID"/"3SID" | Rig: one 2SID and one 3SID tune plays; label correct |
| I434e | 4 (seek rules) | Seek only in classic mode, reSID modes report not seekable, UI greys the bar; optional RewindSong | Test: seek in reSID mode returns the not-seekable result, in classic calls `ForwardSong` | Rig: classic ffwd works, reSID bar greyed |
| I434f | 5 | Length and STIL from the catalogue (HVSC data from I162a): end-of-song through `rp_hw_poll`, per-subsong lengths if the catalogue has them, `stil.*` rows in song info | Test with a fixture catalogue: tune ends at the HVSC length +/- 1 s through the engine; info panel lists STIL composer/title; no second parser (grep proves no Songlengths parsing in the plugin) | Rig: synced HVSC subset, play a tune with known length, it advances at that time |
| I434g | 6 | Scope feed for HW kind: new replayer hook delivering `GetResidAudioBuffer` buffers into `sel_viz_frame` (up to 3 voices), grey in classic mode | Test: viz frame `voice_count` = SID count and scope non-zero for a stub buffer; MUI scope plugin shows it (existing scope test) | Rig: reSID tune shows moving scopes on AGA and RTG |
| I434h | 7 | CPU gate and performance request: min CPU for reSID, `MeasureResidPerformance` once per combination, "too slow" confirm dialog | Test: stub returning measured > limit raises the dialog; Stop aborts start; 3SID divides the limit by three, 2SID halves it | Rig 1 profile 68020 (too slow dialog appears) and 68060 (no dialog) |
| I434i | licence | Ship or not ship playsid.library (see section 4); package manifest and README text | Decision recorded; package test lists the file or the README line | none |

Hardware modes (SIDBlaster, ZorroSID, USBSID-Pico, Trinity) cannot be proven on the rig; I434b delivers the mode mapping, the rig proves only classic and reSID. State this in the row, do not claim hardware verified.

## 4. Licence of playsid.library

- Not found: no LICENSE, COPYING or licence header in `code/playsid.library/` (only `config-tool/inih/LICENSE.txt`, New BSD, for an INI parser used by the config tool). `playsid.asm` header: "(C) 1990-1994 Hakan Sundell and Ron Birk"; README lists reSID by Dag Lem as the engine. The `resid-68k` directory in this checkout is empty (submodule not fetched), and the upstream repo (github.com/koobo/playsid.library) was not read (no network use in this task).
- `code/LICENSE.md` is MIT, Copyright 2017 Kari-Pekka Koljonen. It covers HippoPlayer's own code, not necessarily the vendored library.
- Known from outside the tree, UNVERIFIED here: reSID is GPL-2 (Dag Lem). Selecta already ships GPL-2 libsidplayfp (`third_party/libsidplayfp`), so GPL code in the package is not a new category, but the original PlaySID code by Sundell and Birk has no stated licence, and redistribution rights are unclear.
- Hippo's own distribution ships `dist/playsid.library`, so shipping is practised upstream, but that is not a licence.
- Update (Johan 2026-11-01): Johan has permission from the playsid.library author, and the current version is maintained by a friend of his. That clears the PlaySID code (Sundell and Birk, and the current maintainer). It does not cover reSID by Dag Lem, which stays GPL-2: a bundled copy needs the GPL-2 notice and source offer, as libsidplayfp already does. The permission is not yet written down in the repo; store the statement (mail or message) under `thoughts/shared/licences/` before any bundling.
- Conclusion: may NOT be assumed shippable. Safe default that meets the row: Selecta does not bundle it, it opens `LIBS:playsid.library` if the user installed it, and falls back otherwise. Bundling needs the upstream licence read (koobo repo plus the author's statement) and Johan's decision.

## 5. Open questions

- Q1: Bundle playsid.library, or require the user's copy in LIBS:? (section 4; recommended: do not bundle until upstream licence is read.)
- Q2: AHI/paula14 output for reSID: follow the global Selecta output pref (I871 owner-per-output) or keep the library's own `PlaySIDreSIDAHI` setting? The library plays through its own audio path, so a HW-kind plugin cannot share the Selecta sink; this clashes with "dual AHI/Paula output for every Amiga format" (I888). Needs a decision.
- Q3: Add RewindSong (library has it, Hippo does not use it)?
- Q4: Does the engine forward master volume to HW-kind replayers? Hippo calls `SetVolume` for the reSID variant.
- Q5: Does the catalogue hold per-subsong lengths from Songlengths.md5 (`subtunes` is a count; check `stil.<n>` and length tags in `hvsc.c`)? If not, I434f first extends the hvsc sync, not the plugin.
- Q6: Register-based PSID PatternScope: port later or never? Selecta has no such viz kind.
- Q7: Hardware modes cannot be rig-verified (no SIDBlaster/ZorroSID/USBSID-Pico/Trinity on the rig); accept mapping-only proof?

## Answers (Johan 2026-11-01)

- Q1 bundle: no. Load `playsid.library` from `LIBS:` when the user has it installed; when absent, fall through to the next SID owner. I434i is closed by this answer.
- Q2 output: map Selecta's choice onto the library. The per-format `amiga.format_output` pref (I888a), else the system output, selects the library's own AHI mode or Paula 14-bit. Selecta effects do not apply to SID audio on this path.
- Q3 RewindSong: not added (HippoPlayer parity; Hippo never calls it). Default chosen, not asked.
- Q6 register PatternScope: port it as a new viz kind on MUI, AGA and RTG (row I434j).
- Q7 hardware modes (SIDBlaster, ZorroSID, USBSID-Pico, Trinity): implement them fully; Johan tests them on his real A1200. A host test proves each pref value reaches `SetOperatingMode`; the A1200 check is the done gate for the hardware he owns.
- Q4 (master volume to HW-kind) and Q5 (per-subsong lengths) are answered from the code by the build rows.
