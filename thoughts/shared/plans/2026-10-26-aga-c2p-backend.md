---
date: 2026-10-26
author: spot
ledger: I439, I450 (track B)
implements: thoughts/shared/plans/2026-10-24-m4-amiga-ui.md Phase 6 (M4-P6-1..M4-P6-9), sections 3.4 and 4.3
status: M4-P6-1 first light built; rig run pending (see 8)
---

# AGA chunky-to-planar backend for the desktop UI

Johan, I439: "open a hardware banging AGA chunky2planar screen using Kalms c2p to get the rich UI smooth on the
Amiga". This document fixes how the `aga` backend works, what first light (M4-P6-1) delivers, and what the later
Phase 6 items hang on.

"Hardware banging" here means: the display bitplanes are ours, in chip RAM, written by the fastest c2p routine for the
CPU, and swapped with no copy. It does NOT mean taking the machine over (LoadView(NULL), Forbid, own copper list for
the whole display, own input handler). Selecta must keep AHI, Wasabi, the replayers and Workbench running while the UI
is up, so the screen is an Intuition screen with a custom bitmap. The only part that goes below Intuition later is the
user copper list of section 5 (M4-P6-3), which Intuition supports on a screen's ViewPort.

## 1. Interface

`plugins/ui/desktop/amiga/backend_aga.h` has the same shape as `backend_rtg.h`: an opaque `desk_aga`, `desk_aga_*`
functions, and a macro map onto the `desk_sdl_*` names `desktop_ui.c` calls. `desktop_ui.c` keeps no Amiga types.

| Function | aga behaviour |
| --- | --- |
| `desk_aga_default_size` | 640x256 PAL HIRES / 640x200 NTSC HIRES, no interlace (Johan 2026-10-27; the earlier "Workbench size" rule is dropped). Width a multiple of 32 (Kalms needs it). Full screen by default (D-M4-3). The canvas takes this size from the backend; desktop_ui.c must never pass its 800x600 default to `desk_backend_open`. |
| `desk_aga_open(w, h, index8)` | Refuses `index8 == 0`: an AGA screen shows INDEX8 only (`DESK_BACKEND_INDEX8 1`, as rtg). Needs graphics/intuition v39 and `GFXF_AA_ALICE`. |
| `desk_aga_present(b, canvas)` | Palette upload when it changed, dirty-band c2p into the back buffer, `ChangeScreenBuffer`. |
| `desk_aga_depth` | 8, read back from the screen's bitmap: the reachability sentinel that the screen is AGA planar. |
| counters | `upload_pixels`, `presents`, `c2p_calls`, `c2p_kind`, `chip_held` (perf and budget tests). |
| `desk_aga_poll` / `next_input` | Window port, VBLANK timer, Ctrl-C. Input translation: see 2.6. |

### 1.1 Selection (open item M4-P6-1a)

Backends are chosen at compile time in `plugins/ui/desktop/desktop_ui.c` lines 23-36. First light could not edit that
file (another task holds it). The patch, to land with M4-P6-1a:

```c
#if defined(SELECTA_DESKTOP_AGA)
#include "amiga/backend_aga.h"
#define DESK_BACKEND_NAME "aga"
#define desk_backend_depth(b) desk_aga_depth(b)
#define DESK_BACKEND_INDEX8 1
#define desk_backend_open(w, h, i8) desk_aga_open((w), (h), (i8))
#elif defined(SELECTA_DESKTOP_RTG)
...
```

and in `plugins/ui/desktop/CMakeLists.txt` an option `SEL_DESKTOP_AGA` (default OFF until M4-P6-7) that adds
`amiga/backend_aga.c amiga/aga_planar.c ${AGA_C2P_040}` to the plugin and defines `SELECTA_DESKTOP_AGA=1`. The end
state (M4-P6-8) is runtime choice: the plugin links both backends (the library bases are renamed per backend, so they
do not collide) and opens rtg when a Picasso96/CGX mode of the asked size exists, else aga. That needs `desk_backend_*`
to become a small function table instead of macros; it is part of M4-P6-8, not first light.

## 2. Screen, buffers, present

### 2.1 Screen open

1. `OpenLibrary` graphics and intuition v39 (renamed bases `sel_aga_GfxBase` / `sel_aga_IntuitionBase`, same reason
   as backend_rtg.c: libnix would otherwise open them at program start for every binary).
2. Mode (Johan 2026-10-27): always `PAL_MONITOR_ID|HIRES_KEY` (640x256) or `NTSC_MONITOR_ID|HIRES_KEY` (640x200),
   chosen from `GfxBase->DisplayFlags`; no interlace, no Workbench clone, never 800x600. 320x256 PAL LORES is the only
   other valid size (confirmed by Johan). `ModeNotAvailable` must be 0. The desktop layout must fit
   640x256; the canvas is sized from the opened mode, not the other way round.
3. Chip RAM check before allocation: `AvailMem(MEMF_CHIP) >= desk_aga_chip_bytes(w, h)` and the largest chip block
   holds one buffer. Refusal prints a reason and returns NULL (the plugin then falls back to the MUI UI).
4. Two buffers, each one `AllocMem(w*h, MEMF_CHIP|MEMF_CLEAR)` block, planes contiguous:
   `Planes[i] = base + i * bplsize`, `bplsize = w/8 * h`. Contiguous planes are what Kalms' routines take (one base
   pointer, plane step = bplsize); no interleave, no modulo.
5. `OpenScreenTagList` with `SA_BitMap` = buffer 0, `SA_Depth 8`, `SA_DisplayID`, `SA_Quiet`, `SA_ShowTitle FALSE`,
   `SA_Exclusive`, `CUSTOMSCREEN`; a borderless backdrop window with `WA_RMBTrap` for input.
6. `AllocScreenBuffer(scr, NULL, SB_SCREEN_BITMAP)` and `AllocScreenBuffer(scr, &bm1, 0)`; both
   `dbi_SafeMessage.mn_ReplyPort` point at one port.

### 2.2 Double buffer

`back` is the buffer not on display. A present writes `back`, calls `ChangeScreenBuffer(scr, sb[back])` (retry on
`WaitTOF` when it returns 0), flips `back`, and clears `safe`. Before the next write, the backend waits for the safe
message: the buffer just hidden is no longer read by the display. No copy between buffers, ever.

### 2.3 Dirty rows, not dirty rectangles

The canvas keeps up to 64 dirty rectangles. A c2p routine converts whole rows (Kalms has no horizontal offset or
modulo), so `desk_aga_bands` turns the rectangles into sorted, disjoint full-width row bands, joins bands at most 8
rows apart (each call has a fixed set-up cost) and keeps at most 16. Because the back buffer was last written two
presents ago, the present converts `bands(now) U bands(previous)` (`desk_aga_bands_union`). The first two presents
convert the whole canvas (both buffers start empty).

Per band: `c2p1x1_8_c5_040_init(w, band_h, band_y, bplsize)` then `c2p1x1_8_c5_040(chunky + band_y*w, planes)`.
Kalms' `scroffsy` places the band at the same rows of the planes. Measured: the 040 routine agrees with the C
reference bit for bit for the full screen and bands at rows 0, 5, 17 and 63 (section 6).

Cost model for M4-P6-6 (to be measured on the rig, not assumed): a now-playing frame changes the progress bar, time
and level meters, a few bands of 10-30 rows; at 640 wide that is 6-20 KB of chunky per frame instead of 160 KB.

### 2.4 Chip RAM budget

| Size | One buffer | Screen (2 buffers) |
| --- | --- | --- |
| 640x256 | 160 KB | 320 KB |
| 640x512 | 320 KB | 640 KB |
| 320x256 (M4-P6-9 small) | 80 KB | 160 KB |

An A1200 has 2 MB chip RAM; AHI and Workbench typically leave 1.3-1.6 MB. 640x512 fits; the open check of 2.1.3 is
the guard. The canvas itself (w*h INDEX8) and the palette cube live in fast RAM (memory rule: the baseline has fast
RAM).

### 2.5 Palette

The INDEX8 canvas already quantises everything to 256 entries (`sel_gui_palette_budget`, region sub-palettes for art,
tiles, panels). The AGA screen shows exactly that palette: when `canvas->palette` or `colours` changed since the last
present, `desk_aga_rgb32_table` builds a LoadRGB32 table (256 entries, 32-bit left-justified components) and
`LoadRGB32` loads it. So the INDEX8 goldens already produced for rtg (`m4-dui-index8-*`) are the AGA goldens too: the
pixels are the same indices, only the transport differs. The golden gate (M4-P6-7) compares `desk_aga_p2c_ref` of the
shown buffer against the INDEX8 golden, which proves the c2p path lossless end to end.

### 2.6 Input (open item)

The IDCMP to `sel_gui_raw` translation (keys through keymap.library, mouse, wheel) is static inside backend_rtg.c.
Copying it would duplicate it. The right fix is to move it into `plugins/ui/desktop/amiga/input.c` with
`desk_amiga_input_drain(window, queue)`, used by both backends. backend_rtg.c is owned by the RTG task right now, so
first light queues no input; the right mouse button and Ctrl-C end the screen.

## 3. CPU variants (M4-P6-5)

| CPU (`SysBase->AttnFlags`) | Routine | State |
| --- | --- | --- |
| 68040, 68060 | `c2p1x1_8_c5_040` (copy speed on 040/40 and 060) | wired, verified |
| 68030 | `c2p1x1_8_c5_030` (smcinit: runtime bplsize by self-modifying code, `CacheClearU`) | vendored, not wired |
| 68020 | `c2p1x1_8_c5_gen` | vendored, not wired |
| any | `desk_aga_c2p_ref` (C) | fallback, used on 020/030 for now |

The 030 and generic routines take BPLSIZE as an assembly-time equate and step planes with `add.w #BPLSIZE,a1`,
which overflows above 32767 bytes (640x512 has bplsize 40960). Wiring them (M4-P6-5) means assembling one object per
supported size from CMake with `-DBPLSIZE=...` and patching the `add.w` to `add.l` (or `adda.l`) in the vendored copy,
recorded in third_party/kalms-c2p/VERSION. The 020 timing goal is set by M4-P6-6 on an `--a1200` rig.

## 4. Kalms licence verdict

Public domain. `third_party/kalms-c2p/readme.txt`, section Legal: every file except those under `others/` is public
domain, free to use, change and redistribute; each routine's header repeats it. Selecta is MIT, so vendoring is
allowed, no attribution required (kept anyway in LICENSE and VERSION). Only `normal/` routines are vendored; nothing
from `others/`. Upstream https://github.com/Kalmalyzer/kalms-c2p, commit d8ecf79a3325615305dd800ae7704b518e0d9dda
(2022-03-20), taken through the Up Rough vendor copy (include of `c2p_equates.i` redirected, equates IFND-guarded).

## 5. Copper gradients later (I441, M4-P6-3)

The screen is a normal ViewPort, so gradients go in as a user copper list: `UCopList` built with `CINIT` / `CWAIT` /
`CMOVE` on COLOR registers (and `BPLCON3`/`BPLCON4` bank bits for AGA 24-bit colour), attached to
`screen->ViewPort.UCopIns` under `Forbid` + `RethinkDisplay` (the documented way; MakeScreen/RethinkDisplay rebuilds
both double buffer copper lists). Rule for the UI: a gradient behind a band of rows uses palette entries the INDEX8
canvas reserves for "background" (one entry per gradient), so the canvas draws the index and the copper changes that
entry's colour per line. The canvas stays unaware; desktop_ui.c passes the gradient stops through a new
`desk_backend_gradient(b, y0, y1, index, rgb0, rgb1)` that rtg implements by drawing the ramp into the canvas instead.

## 6. Tests per Phase 6 item

| Item | Test | Where | State |
| --- | --- | --- | --- |
| M4-P6-1 c2p reference | `dui-aga-planar` (bands, union, c2p/p2c round trip, chip bytes, LoadRGB32 table) | host ctest, tests/dui/aga_planar_unit.c | passes |
| M4-P6-1 Kalms = reference | `selecta-aga-c2p-kalms` under `vamos -C 68040` prints `c2p_kalms040=match bands=5` | build/amiga, tests/dui/aga_c2p_kalms.c | passes by hand; ctest registration `dui-aga-c2p-kalms` behind the vamos lock (tests/CMakeLists.txt T9 pattern) open |
| M4-P6-1 first light | `rig-m4-aga-open`: rig 2, `selecta-aga-first-light` prints `backend=aga depth=8 w=640 h=256 chip_bytes=327680 c2p=kalms040 presents=50` and c2p_calls > presents (bands) | rig | pending: rig 2 busy (8) |
| M4-P6-1a selection | desktop_ui.c branch; `rig-m4-aga-open` again through `selecta-gui`, `backend=aga` in the start line | rig | open |
| M4-P6-2 blits / bands | `rig-m4-aga-blit`: upload_pixels per idle now-playing frame <= 15 % of w*h | rig | open |
| M4-P6-3 copper | `rig-m4-aga-copper`: gradient visible, palette entry unchanged in canvas | rig + screenshot subagent | open |
| M4-P6-4 scroll | `rig-m4-aga-scroll`: list scroll converts only the list band | rig | open |
| M4-P6-5 CPU variants | `dui-aga-c2p-kalms` for 030/gen under `vamos -C 68030` / `-C 68020` | vamos | open |
| M4-P6-6 perf | `rig-m4-aga-perf`: ms per present on 060 and `--a1200` 020, target from the M4 plan | rig | open |
| M4-P6-7 goldens | `m4-aga-golden-gate`: p2c of the shown buffer equals the INDEX8 goldens | rig dump + host compare | open |
| M4-P6-8 default / runtime pick | rtg when a card mode exists, else aga | rig | open |
| M4-P6-9 small | `rig-m4-aga-small`: 320x256 opens, 160 KB chip | rig | open |

## 7. Files

- `plugins/ui/desktop/amiga/aga_planar.{h,c}`: pure helpers (bands, union, C c2p/p2c, budget, LoadRGB32 table).
- `plugins/ui/desktop/amiga/backend_aga.{h,c}`: the backend.
- `plugins/ui/desktop/amiga/aga_first_light.c`: probe executable `selecta-aga-first-light`.
- `tests/dui/aga_planar_unit.c` (host), `tests/dui/aga_c2p_kalms.c` (68k, vamos).
- `third_party/kalms-c2p/`: vendored routines, LICENSE, VERSION.
- `plugins/ui/desktop/CMakeLists.txt`: vasm custom command for the 040 object, the two Amiga executables.

## 8. Rig status

vtcon's rig is an A1200 (AGA chipset, 68060 by default, `--a1200` for the 020) in FS-UAE, so rig 2 can run this.
On 2026-10-26 rig 2 was up and held by the RTG task (`--purpose "MusicPlayer I450: RTG first light with library"`,
Workbench on the card). Opening an exclusive AGA screen there would disturb that run, so first light on the rig waits
until it is free. Then: start or restart rig 2 with `--purpose "Selecta session: AGA c2p first light"`, copy
`build/amiga/plugins/ui/desktop/selecta-aga-first-light` to the rig, run it, and check the line of section 6.
