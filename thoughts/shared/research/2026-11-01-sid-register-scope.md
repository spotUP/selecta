---
date: 2026-11-01
topic: SID register PatternScope for playsid (HippoPlayer parity) as a new viz plugin on MUI, AGA and RTG (ledger I434j)
tags: [design, amiga, sid, playsid, hippoplayer, viz, mui, aga, rtg, i434j]
status: design (code reading only; no build, no test, no rig)
---

# SID register PatternScope (I434j)

Parent: ledger I434j, answer Q6 of `thoughts/shared/research/2026-11-01-playsid-inventory.md` (Johan 2026-11-01: port it as a new viz on MUI, AGA and RTG). Builds on I434g (RP_VIZ_VOICES). Rows I434j1..I434j5 at the end are buildable one by one by a sonnet agent; each names files, the test and the rig text.

Sources read: HippoPlayer `~/Code/HippoPlayer-68k/code/puu016.s` (P below; ISO-8859, grep needs `LC_ALL=C grep -a`), `code/playsid.library/{playsidbase.i,playsid.asm}`. Selecta: `include/selecta/{viz.h,replayer.h,tags.def,prefs.def}`, `src/engine/engine.c`, `plugins/replayers/playsid/{adapter.c,playsid_api.h,playsid_amiga.c}`, `plugins/viz/scope/scope.c`, `plugins/shared/viz/surface.h`, `plugins/ui/desktop/desktop_ui.c`, `plugins/ui/mui/map/mui_viz.{h,c}`, `plugins/ui/mui/mui_vizarea_class.c`, `tests/engine/playsid_test.c`.

## 0. Correction to the inventory

The inventory (Group 6) says the register scope reads `playsidbase+50`. That is wrong. `playsidbase+50` is the `DisplayData` pointer (`dd_Sample1..4`, playsidbase.i:35-59), the classic-mode wave feed, and the only comment naming it (P:30434) sits on a commented-out `;SIDscope`. The real PSID PatternScope (P:40348-40890) reads:

| What | Where (library base = `_SIDBase`) | Hippo line |
|---|---|---|
| C64 memory pointer | `move.l 346(a0),a0` (`psb_C64Mem`, not in the public playsidbase.i) | P:40350 |
| SID 1 registers | `C64Mem + $D400`, 25 bytes ($00-$18) | P:40351 |
| SID 2 / SID 3 registers | `C64Mem + $D420` / `C64Mem + $D440` (hard-coded, not the header address) | P:40364, 40377 |
| Envelope level voices 1-3 / 4-6 / 7-9 | words at base+448,450,452 / 454,456,458 / 460,462,464 ("secret locations", `psb_Envelope1..9`), range 0..63 per Hippo's comment at P:~40740 | P:40355-40358 |

These offsets are private to the library version Hippo ships. The plugin reads them only after the existing version check passes (`ps_ops.open`); row I434j1 adds a library-version guard (lib_Version/lib_Revision read from the base, minimum = the version in `code/playsid.library`) and returns 0 chips below it.

## 1. HippoPlayer behaviour (what parity means)

- Update: `sidScopeUpdate` (P:40137) runs from the vertical blank, 50 Hz, before the reSID buffer fetch, so the register view works in classic AND reSID modes (unlike the wave scope). SID 2/3 update only when the tune is 2SID/3SID (`p_sid\.sidIsStereo` / `sidIsThree`).
- Decode (`patternScopeSIDUpdate`, P:40390-40525), per chip:
  - Filter resonance = `$17 >> 4` (0..15); cutoff = `($16 << 3) | ($15 & 7)` (0..$7FF); modes LP/BP/HP = `$18` bits 4/5/6; voice routed to filter = `$17` bits 0/1/2.
  - Per voice (7 registers apart): control register `$04` bits 0..7 = Gate, Sync, Ring, Test, Triangle, Saw, Pulse, Noise. Frequency = word `$01:$00`, scaled so low notes are visible: below $3FFF x4, below $7FFF x2, else as is. Pulse width = word `$03:$02 & $FFF`.
- Afterglow (`.updateValue`, P:40506-40525): every on/off flag has a counter. Flag on: counter 0. Flag drops from on to off: counter = 4, then minus 1 per update. `.drawBlock` draws a shrinking block for counter 4..1, so one-frame gate pulses stay visible.
- Layout (P:40531-40890), per chip a 320 x 64 pixel band (40 columns x 8 text rows of 8 px; 2SID 128 px, 3SID 192 px):

```
row 0  Voice1 Flt[]   Voice2 Flt[]   Voice3 Flt[]        (col 0 / 13 / 26 per voice; Flt block at voice col +11)
row 1  Fr [64 px bar]  ...                                (bar at voice col +3; freq >> 10)
row 2  Pw [64 px bar]  ...                                (pulse width >> 6)
row 3  Gate[] Tri[]                                        (blocks at voice col +5 and +11)
row 4  Sync[] Saw[]
row 5  Ring[] Pul[]
row 6  Test[] Noi[]
row 7  Filter [64 px bar]  Reso [16 step bar]  LP[] BP[] HP[]   (cutoff >> 5 at col 7, reso at col 21, blocks at 28/33/38)
       + per voice a vertical envelope bar, 55 px tall, at voice col +12, bottom at pixel row 54 (env * 55 / 64)
```

Labels for chip 2/3 read Voice4-6 / Voice7-9.

## 2. Data path (Selecta)

The register view is not a wave, so it does not go into `scope[][]`. It gets its own field in the frame and its own optional replayer hook, mirroring how I434g added RP_VIZ_VOICES.

### 2.1 Frame contract: `include/selecta/viz.h`

Add before `sel_viz_frame` (viz.h:23):

```c
#define SEL_VIZ_SID_CHIPS 3u
#define SEL_VIZ_SID_REGS 25u   /* $D400-$D418 */
typedef struct sel_viz_sid_chip {
    uint8_t reg[SEL_VIZ_SID_REGS]; /* raw register mirror at block close */
    uint8_t env[3];                /* envelope level per voice, 0..63 */
    uint8_t glow[3][9];            /* per voice: control bits 0..7 (Gate..Noise), [8] = routed to filter; 0..4 */
    uint8_t mode_glow[3];          /* LP, BP, HP; 0..4 */
} sel_viz_sid_chip;
```

and in `sel_viz_frame` (after `note_on`, viz.h:32):

```c
    uint32_t sid_chips;                        /* 0: no register feed this block; 1..3 */
    sel_viz_sid_chip sid[SEL_VIZ_SID_CHIPS];
```

Raw registers cross the boundary, not decoded values: decoding stays in the plugin (one place), the afterglow is computed at block rate on the producer side because it is time-based (50 Hz, Hippo parity) and the viz plugins are stateless and drawn at a lower, host-dependent rate. Size: about 3 x 58 bytes per frame, triple buffered; no effect on the 20 ms block.

`sel_viz_tap` (viz.h:41) gets the producer-side history: `uint8_t sid_prev_ctrl[SEL_VIZ_SID_CHIPS][4]` (control of voices 0..2 plus `$17`/`$18` bits as needed) and `sel_viz_sid_chip sid_glow_state[SEL_VIZ_SID_CHIPS]` (or just the glow arrays). Pure function next to `sel_viz_vu_step` (src/engine/viz.c:70), declared in viz.h:

```c
/* Afterglow of one chip, once per block (HippoPlayer .updateValue): a flag that is on sets its counter to 0; a flag that
 * drops from on to off sets it to 4; an off flag counts its counter down to 0. `prev` is the previous block's reg. */
void sel_viz_sid_glow(sel_viz_sid_chip* now, const uint8_t prev_reg[SEL_VIZ_SID_REGS], const sel_viz_sid_chip* prev);
```

`sel_viz_begin` must zero `sid_chips` (the frame is reused from the triple buffer).

### 2.2 Replayer hook: `include/selecta/tags.def`, `include/selecta/replayer.h`

- tags.def after `SEL_TAG(RP, VIZ_VOICES, 0x1022, PTR)` (tags.def:83): `SEL_TAG(RP, VIZ_SID, 0x1023, PTR)` with a comment like the VIZ_VOICES one. Check 0x1023 is free first (`grep -n 0x1023 include/selecta/tags.def`).
- replayer.h after `sel_rp_viz_voices_fn` (replayer.h:141):

```c
/* RP_VIZ_SID (I434j): optional, HW kind. SID register mirror for the register scope. The engine calls it on the audio
 * thread once per 20 ms visualisation block while playing, with room for `max_chips` rows of SEL_VIZ_SID_REGS bytes in
 * `regs` and 3 envelope levels per chip in `env`. Returns the chips filled (0..max_chips), 0 when there is no mirror. */
typedef uint32_t (*sel_rp_viz_sid_fn)(void* state, uint8_t* regs, uint8_t* env, uint32_t max_chips);
```

### 2.3 Engine: `src/engine/engine.c`

- Slot field next to `rp_viz_voices` (engine.c:246): `sel_rp_viz_sid_fn rp_viz_sid;`.
- `prepare_slot` lookup next to engine.c:2363: `sl->rp_viz_sid = sl->hw ? (sel_rp_viz_sid_fn)sel_tag_find(hooks, SEL_RP_VIZ_SID, 0) : NULL;`
- `viz_close` (engine.c:8167): after the RP_VIZ_VOICES block and independent of it (classic mode has no wave but has registers): when `sl->hw && sl->rp_viz_sid && e->state.play == SEL_PLAYING`, call it into a stack `uint8_t regs[3][25], env[3][3]`, copy into `f->sid[k].reg/env`, run `sel_viz_sid_glow` against `e->viz` history, store history, set `f->sid_chips`. Otherwise `sid_chips = 0` and the history is cleared (so a new song does not start with glow).

### 2.4 playsid plugin: `plugins/replayers/playsid/`

- `playsid_api.h` `ps_ops` (playsid_api.h:24-48): add
  `uint32_t (*read_sid_regs)(uint8_t* regs, uint8_t* env, uint32_t chips); /* copies C64Mem+$D400/$D420/$D440 and psb_Envelope1..9; returns chips copied, 0 when the library version is unknown */`
- `playsid_amiga.c`: implement from `g_base` (playsid_amiga.c:9): `C64Mem = *(UBYTE**)((UBYTE*)g_base + 346)`, envelope words at `g_base + 448 + 2*i` (store low byte, clamp 63). Before the first read: `g_base->lib_Version` check (2.4 above; record the minimum in a `#define PS_REGS_MIN_VERSION`). NULL C64Mem returns 0.
  Implementer check (read only, 1-2 greps in playsid.asm around `getSid2Address`, asm:1042, and the SID write handler): confirm 2SID/3SID writes land in C64Mem at $D420/$D440 as Hippo assumes. If the library mirrors them at the header address (`psb_Sid2Address`) instead, read from that address; write the finding into the row result.
- `adapter.c`: `rp_viz_sid(void* state, uint8_t* regs, uint8_t* env, uint32_t max_chips)` next to `rp_viz_voices` (adapter.c:305): returns 0 unless `s->playing && !s->paused`; chips = `min(ps_sid_count(s->image, s->size), max_chips)`; works in every operating mode (classic and reSID; hardware modes too, since the 6502 emulation still writes C64Mem: unverified, Johan's A1200 check). Counter `sel_playsid_reg_fills` added to the sentinel list at adapter.c:42. Register with `set(&g_hooks[18], SEL_RP_VIZ_SID, SEL_TAG_FN(rp_viz_sid));` next to adapter.c:405 and grow `g_hooks` by one (check the array size and terminator).

### 2.5 Viz plugin: `plugins/viz/sidregs/sidregs.c` (new)

One shared plugin for every UI, as the existing three (scope, levels, spectrum): no per-UI drawing code. Id `viz_sidregs`, display name `SID PatternScope` (HippoPlayer's name; see open question 1). Registered in `plugins/CMakeLists.txt:45` by adding `sidregs` to the `foreach(viz scope levels spectrum)` list.

- Uses `plugins/shared/viz/surface.h` (`vs_clear`, `vs_rect`, `vs_line`, `vs_descriptor`).
- Text: the viz helper set has no text (searched `plugins/shared/viz`, `src`, `plugins`: the only fonts are the desktop UI's atlas fonts, `plugins/ui/desktop/amiga_font.c`, host-side and UI-owned). New header `plugins/shared/viz/glyphs.h`: a 4x6 bitmap (one byte per row, 3-4 bits used) for exactly the characters the labels need: `V o i c e F l t r P w G a T S y n R u s N H B L 1-9` and space; `vs_text(s, x, y, scale, str, px)` draws each lit horizontal run of a glyph row with one `vs_rect` (not per pixel). Characters without a glyph draw nothing; a test lists every label string and asserts each character has a glyph.
- Decoding helpers as `static` pure functions in the plugin (freq scaling, cutoff, resonance, pulse width), copied in meaning from section 1, unit tested through the render output.
- Layout: the Hippo grid scaled to the surface. Chip band height `bh = s->h / sid_chips`; grid 40 columns x 8 rows per band: `cw = s->w / 40`, `rh = bh / 8`. Bars: horizontal bars are 8 columns wide at full value (Hippo's 64 px at 8 px per column), filled length proportional, track in `grid`, fill in `primary`. Resonance bar: 2 columns, 16 steps. Blocks: one cell (`cw x rh`, inset 1 px): on = `primary`; glow 4..1 = `secondary` with height `rh * glow / 5` anchored at the bottom (Hippo draws shrinking blocks); off = `grid` outline. Envelope: vertical bar at voice column +12, from row 0 to row 7 of the band, height `env * band_h / 64`, `primary`. Labels in `accent` (the text token), scaled glyphs `scale = max(1, rh / 7)`.
- Degraded sizes: `rh < 6` or `cw < 3` (e.g. the Now Playing strip): no labels, bars and blocks only. `rh < 2`: draw only the three frequency bars and envelope bars per chip.
- `sid_chips == 0` (not a SID, libsidplayfp owner, paused, stopped): clear to background, draw the labels greyed in `grid`, return 0 (no frame counted, same convention as scope.c when `f->seq == 0`).
- Return value: primitives drawn (rects + runs), the frame sentinel the hosts already count.

### 2.6 UI wiring (no per-UI drawing)

| UI | File:line | Change |
|---|---|---|
| Desktop UI on AGA (`SELECTA_DESKTOP_AGA`, INDEX8) and RTG (`SELECTA_DESKTOP_RTG`), and Mac | `plugins/ui/desktop/desktop_ui.c:6204` `#define VIZ_MAX 3u`, :6209 ids, :563 `viz_fn[3]` (and `viz_id`, `viz_name`, `viz_btn` arrays sized 3) | VIZ_MAX 4, add `"viz_sidregs"` to the ids, size every viz array by `VIZ_MAX`. The Visualizer page grows to four panes (6339-6409 already loops `viz_n`); check the `viz_btn[c / 3u][c % 3u]` indexing at :11325, which hard-codes 3 |
| MUI 3.8 | `plugins/ui/mui/map/mui_viz.h` `MUI_VIZ_PLUGINS 3u`; `mui_viz.c:98` `k_viz_ids` | MUI_VIZ_PLUGINS 4, add `"viz_sidregs"`; the cycle gadget takes its entries from `name_[]` already. `mui_vizarea_class.c` unchanged |
| Pref | `include/selecta/prefs.def:13` `ui.desktop.viz` | Accepts `viz_sidregs` (shared by desktop and MUI); no new pref |

Selection: the user picks "SID PatternScope" in the visualiser selector like the other three. No automatic switch (see open question 2).

## 3. Cost on 68020 AGA

Producer (audio thread, per 20 ms block, only while a playsid slot plays): copy 75 register bytes and 9 envelope words from chip RAM-free library memory, 3 x 30 glow counter steps. Under 1500 cycles, below 0.2% of a 14 MHz 68020 at 50 blocks per second. No allocation, no lock.

Consumer (UI thread, plugin draw): the draw rate is the host governor's, not the plugin's: MUI on a palette screen 10 per second, RTG 25 per second (`mui_budget_interval_ms`, mui_viz.h); the desktop UI on AGA/RTG through `sel_uim_budget_for`. Per draw at a 320 x 64 band per chip (Hippo's size), INDEX8:

| Part | Work | Estimate 68020/14 MHz |
|---|---|---|
| clear | 20 KB per chip band | about 1.5 ms per chip |
| labels | about 24 strings, about 100 glyphs, about 400 runs as `vs_rect` | about 1.5 ms |
| bars, blocks, envelopes | 3 voices x (2 bars + 9 blocks + 1 vbar) + filter row = about 45 rects, under 3 K pixels | about 0.5 ms |
| total | one chip | about 3.5 ms; 3SID about 9 ms |

Budget: at most 4 ms per chip band per draw on 68020/14 MHz AGA, so at most 4% CPU at 10 draws per second for one SID, 11% for 3SID. The planar conversion and blit are the host's and already paid by the other viz plugins at the same surface size. Host-checkable proxy (no timer on the host): primitives per chip at 320 x 64 at most 600 (asserted in the plugin test). If the rig shows the budget broken, the fix is in the plugin (skip label drawing when the colours and size are unchanged needs a host contract that the surface persists; do not add that without measuring).

## 4. Rows (sonnet, one by one, in this order)

Every row: host tests for the touched area only (`nice -n 19`, `--maxWorkers=2` equivalent for ctest: `-j2`), revert check (comment out the hook registration or the new call, the new test must fail), Amiga cross-build of the touched targets. No rig per row: one rig batch at the end (I434j5).

| ID | Files | Done when (test) |
|---|---|---|
| I434j1 | `include/selecta/viz.h`, `include/selecta/tags.def`, `include/selecta/replayer.h`, `src/engine/viz.c`, `src/engine/engine.c` (:246, :2363, :8167), `plugins/replayers/playsid/{playsid_api.h,adapter.c,playsid_amiga.c}`, `tests/engine/playsid_test.c` | New case `viz_sid_test` in `tests/engine/playsid_test.c` beside `viz_scope_test` (:138): the ops stub gets `read_sid_regs` filling a known pattern and counting calls. Through the engine entry (same path as `viz_scope_test`): for 1/2/3SID headers (same header pokes at 0x7a/0x7b) and in classic AND reSID mode, the acquired frame has `sid_chips` = SID count, `sid[k].reg` equals the stub bytes, `env` equals the stub levels; sentinel `sel_playsid_reg_fills` and the stub call count rise. Glow: stub sets Gate on for one block then off, the next frames show glow 4, 3, 2, 1, 0. Paused: `sid_chips == 0` and the sentinel does not move. Pure `sel_viz_sid_glow` unit test in the existing viz unit test file (grep `sel_viz_vu_step` under `tests/`) |
| I434j2 | `plugins/shared/viz/glyphs.h` (new), `plugins/viz/sidregs/sidregs.c` (new), `plugins/CMakeLists.txt:45`, plugin test next to the existing viz plugin tests (grep `viz_spectrum` under `tests/` for the place; else `tests/ui/viz_sidregs_test.c` registered like `mui_viz_test.c`) | Render a hand-built frame into a 320 x 64 INDEX8 surface: Gate on puts `primary` in the Gate cell of voice 1; freq word $FFFF fills the Fr bar to 8 columns, $0800 (x4 scaling) fills 1/4; glow 2 draws `secondary` at 2/5 cell height; `sid_chips = 0` returns 0; `sid_chips = 3` on 320 x 192 draws Voice7 band at y 128; primitives per chip at most 600; every label character has a glyph; a 160 x 16 strip draws no label pixels in `accent` |
| I434j3 | `plugins/ui/desktop/desktop_ui.c` (:563, :6204-6222, :11325) | The desktop reachability test for viz (`tests/dui/viz.cmake`, which asserts the rendered plugin id and a primitive count) gets a run with `ui.desktop.viz = viz_sidregs` on the mem backend in INDEX8 mode (the AGA path, `--format index8`) and RGBA32 (RTG/Mac), fed by the engine playing the playsid stub (or a frame injector if viz.cmake already uses one: read it first); asserts `np_plugin == viz_sidregs`, `viz_frames` rises and the primitive count is non-zero. Visualizer page shows 4 panes (`viz_panes == 4`) |
| I434j4 | `plugins/ui/mui/map/mui_viz.h`, `plugins/ui/mui/map/mui_viz.c:98`, `tests/ui/mui_viz_test.c` | `mui_viz_test.c` find_plugin stub (:70) also answers `viz_sidregs`; `mui_vizset` lists 4 entries with "SID PatternScope" in `name_[]`; selecting it and stepping `mui_vizset_step` with a frame with `sid_chips = 1` raises `mui_viz.frames`; with `sid_chips = 0` it does not. `mui_p8_test.c:450` pref round trip accepts `viz_sidregs` |
| I434j5 | none (rig batch) | Rig check below passes; result and frame-time numbers written into the ledger row |

Rig check text for I434j5 (Amiga, rig 2, `rig.py start --ram 256 --purpose "<session>: I434j SID PatternScope check"`; no audio unless Johan asks; stop the rig when done):

1. Real HVSC tune, single SID (for example `MUSICIANS/H/Hubbard_Rob/Commando.sid`, any real HVSC file, never a test tune), playsid classic mode, desktop UI on AGA: select "SID PatternScope". PASS: Voice1-3 labels, Fr/Pw bars move with the music, Gate blocks flash with glow, envelope bars rise and fall, filter row moves on a filter tune. `viz_frames` rises (read the desktop stat line, text check).
2. Same tune, reSID 6581 mode: same view still moves (register view does not depend on the wave buffer).
3. A real 2SID HVSC tune: two bands, Voice4-6 on the second, both moving.
4. Same on the desktop UI on RTG, and on the MUI UI (MUI 3.8, palette screen): `mui_viz.frames` rises.
5. Frame time: note the UI frame time or idle CPU with the view on versus the Oscilloscope at the same size on the 68020 profile; must stay within the section 3 budget.
6. Not a SID (a .mod): the view shows greyed labels, no movement, no crash.

## 5. Decisions (Johan, 2026-11-01)

1. Selector: SID PatternScope is its own entry in the visualiser selector (no automatic switch inside the later I890 PatternScope).
2. Mac/host SID playback (libsidplayfp) also feeds `SEL_RP_VIZ_SID`, in a later row I434j6 after I434j5.

## 6. I434j1 finding: where 2SID/3SID land (read from playsid.asm)

2SID and 3SID registers land at `C64Mem + $D420` / `$D440`, as Hippo assumes, not at the header address. `getSid2Address` (asm:1042-1069) stores the header address in `psb_Sid2Address` only to build an MMU table (`MakeMMUTable2`, asm:4331-4345) that maps the I/O page at that address onto the `$D420` handler offsets (`$80+$20..`). The 6502 emulation then runs the `.D420` handler (asm:4810), which writes `(a0,$D420)` with `a0 = C64Mem`; the envelope latches use `$D420-$D404` the same way (asm:5102). `getSid3Address` (asm:1077) does the same for `$D440`. So `a_read_sid_regs` reads `C64Mem+$D400+$20*n` and never needs `psb_Sid2Address`. `PSIDLIB_VERSION` is 1 (playsid_libdefs.i:17), so `PS_REGS_MIN_VERSION` is 1. Still unverified on hardware: that the offsets 346 and 448 match the shipped library binary (Hippo's source says so; I434j5 rig check).
