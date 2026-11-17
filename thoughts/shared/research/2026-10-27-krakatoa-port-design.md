---
date: 2026-10-27
ledger: I559
status: draft
---

# Krakatoa port: research and design (I559)

Reference: https://github.com/pagustafsson/krakatoa (Per-Arne Gustafsson, full permission to port).
Studied copy: `index.html` (2552 lines, WebGL2, one file) and the Swedish README. Line numbers below are
from that file. Architecture sections this design builds on: `thoughts/shared/plans/2026-10-22-architecture-design.md`
sec 6 (Visualizers, line 288), sec 7 (visual tiers and F38 fullscreen, line 306), sec 12 (runtime scaling,
line 485), sec 13 (verification, line 495).

Johan's fixed decisions (2026-10-27, not reopened here):

1. Modern target = C host plus the Krakatoa shaders ported to the SDL3 GPU API (SDL_GPU), numbers 1:1.
2. Amiga 060 AGA and RTG = 68k asm, same look and method scaled down (small 2D grid, a few thousand
   particles). For 68020/030 this doc says whether a reduced version is feasible or the effect is hidden.
3. Music drives an automatic brush (beats and bass inject heat and smoke, levels set force, highs set
   swirl). The pointer still paints.
4. Lava and Smoke are both in scope from the start.

Ledger constraints: I559 starts with AGA fullscreen; done = AGA fullscreen visualizer runs on rig 2,
then Johan checks it on rig 3. It must respect I497 (RTG visualizers slow on a maxed 68060; measure frame
ms with `--stats`) and I556 (AGA dirty-band path converts full-width bands and converts each change twice
because of the double-buffer union).

Items marked **(to confirm)** name Selecta symbols this doc did not verify in code.

### 0. What Selecta has today (verified 2026-10-27)

- `include/selecta/viz.h`: `sel_viz_frame` at `SEL_VIZ_BLOCKS_PER_SECOND` 50 (one frame per 20 ms). Per voice
  (up to 32): `peak` and `vu` (Q15), `pitch_q8`, `instrument`, `note_on` (1 when a sample started in this block),
  `scope`. Post-mix: `pcm` ring (1024 stereo frames), `band[16]` (Q15, 43 Hz to 16 kHz log spaced,
  `spectrum_valid`), `correlation`. Delivered through the `sel_viz_tap` triple buffer.
- Viz hook: `sel_viz_render_fn(const sel_viz_frame*, sel_surface*, const sel_viz_colors*)`, integer only, UI
  thread, no state, no dt, no input, five host colours. The host owns the pixels and the palette.
- Tiers: `SEL_TIER_TEXT`, `SEL_TIER_PLANAR`, `SEL_TIER_RTG`, `SEL_TIER_GPU`.
- Existing viz plugins: `plugins/viz/levels`, `plugins/viz/scope`, `plugins/viz/spectrum`.
- Fullscreen visual mode (F38, arch line 324) is designed but not built: `desktop_ui.c` has only the F38 signal
  path text.
- `backend_sdl.c` uses `SDL_CreateRenderer`; no SDL_GPU anywhere in `plugins/`.
- `c2p1x1_8_c5_040` lives in `plugins/ui/desktop/amiga/backend_aga.c` / `.h`.

Consequence: Krakatoa needs a new viz hook (KRK-P0-1): a stateful fullscreen session with open (surface,
machine tier, prefs), frame (viz frame, dt), input (pointer, keys), close, and the right to own the
256-entry palette (INDEX8) or a GPU context (`SEL_TIER_GPU`). The current render hook stays for the small
visualizers.

---

## 1. Reference inventory

### 1.1 Platform requirements

- WebGL2 plus `EXT_color_buffer_float` (render to float), `OES_texture_float_linear` (linear filter of float).
- Formats: `rgba` = RGBA16F, `rgba32` = RGBA32F (particle state), `rg` = RG16F (velocity), `r` = R16F
  (pressure, divergence, curl). Smoke renders into float 3D textures (one layer per draw, vMRT).

### 1.2 Quality levels (`QUALITY`, lines 266-273)

| Level | sim (short side) | dye | particle tex | particles | accum scale | g (smoke Y) | gz | jac | rm (march scale) |
|---|---|---|---|---|---|---|---|---|---|
| low | 96 | 512 | 512x512 | 262 144 | 0.6 | 56 | 0.67 | 16 | 0.5 |
| medium | 128 | 1024 | 1024x1024 | 1 048 576 | 0.8 | 76 | 0.67 | 20 | 0.6 |
| high (default) | 160 | 1024 | 2048x1024 | 2 097 152 | 1.0 | 112 | 0.67 | 28 | 0.75 |
| ultra | 224 | 1440 | 2048x2048 | 4 194 304 | 1.0 | 128 | 0.67 | 30 | 0.9 |

README Smoke grids at 1920x1080: 100x56x36, 136x76x52, 200x112x76, 228x128x84. Default is high; medium
on a coarse pointer (touch). Smoke has an auto-quality governor: `VOL_FPS` 50/58, `governVolume` line 2222,
which steps the grid down below 50 fps and up above 58 fps.

### 1.3 Palettes (`PALETTES`, lines 275-281)

Aurora, Nebula, Magma, Spectrum, Ash. Each has `hot[4]` (cycled by particle age/hue), `cool`, `key`,
`bg[2]` (vertical gradient), `ember`, `smoke`, `tail` (all hex in the file). Keys 1-5 select.

### 1.4 Knobs (`P` defaults lines 287-297, `SLIDERS` lines 300-326)

| Knob | Default | Range | Used by | Effect (formula, line) |
|---|---|---|---|---|
| mode | particles (Lava) | Lava / Smoke | both | key M |
| brush (Size) | 0.03 | 0.012-0.08 | both | splat radius; vel radius 1.5*brush (1837); particle spawn sigma brush*0.85 (1888) |
| emission (Amount) | 1.0 | 0.2-2.5 | both | splat amt 0.2*emission*fr (1792); particle rate N/meanLife*emission; smoke uDRate 9*emission |
| force | 1.0 | 0.2-2.5 | both | FORCE 6000 drag push; smoke kv 0.7*force*Y, uPush 0.5*force*Y, idle jet 0.15*Y*force |
| swirl | 26 | 0-60 | both | Lava vorticity swirl*simScale; smoke uEps swirl*0.002*Y |
| turbulence | 1.0 | 0-3 | both | Lava curl noise turb*34*simScale, particle noise 0.03*turb; smoke uTurb 0.3*turb*Y |
| life (Lifetime) | 1.0 | 0.3-3 | both | Lava 3.3*v s, particle life [1.6,5.0]*life, dye diss 0.32/life, cooling 0.9/life; smoke volLife 7.8*(1-exp(-v/0.92)), decay V_TAU*life |
| smoke (Smoke field) | 0.55 | 0-1.5 | Lava | dyeGain in density and composite |
| particles (density) | 1.0 | 0-3 | Lava | point weight particles*0.3*(accum px/1.3e6)*(1048576/N) |
| light | 1.0 | 0-3 | both | light power light.level*light*2.4 (Lava), *1.6 (Smoke); air 0.02*level*light |
| shadows | 1.0 | 0-3 | both | uAbsorb 2.4*s, uNormal 5*s, uSelf 0.8*s; smoke uExtL V_EXT*3*s |
| bloom (Glow) | 0.9 | 0-2 | both | uBloomInt 0.16*bloom |
| coolTime (Cooling) | 0.5 s | 0.15-2 | Smoke | temperature decay exp(-dt/coolTime) |
| lift | 1.0 | 0-3 | Smoke | uBuoy 0.9*lift*Y |
| palette | 0 | 0-4 | both | keys 1-5 |
| showSmoke, showParticles | true | bool | Lava | layer toggles |
| cursorLight | true | bool | both | light follows brush/pointer, else fixed top light (2004) |
| auto (autopilot) | true | bool | both | `updateDemo` 1737-1758: Lava Lissajous wander on 3.4 of 4.6 s; Smoke lower-screen wander on 5.6 of 7 s |
| paused | false | bool | both | Space |

Per-effect settings are stored separately (`FX_KEYS`) in localStorage. Keys: Space pause, C clear,
H panel, A autopilot, M mode, 1-5 palette.

Fixed constants (not knobs): `SEG` 16 brush segments, `FORCE` 6000, bloom `thr` 0.75 / `knee` 0.6,
exposure 1.0, vignette 0.38 smoothstep(0.45,1.25), dither /96, `V_CAM` 2.4, `V_EXT` 9, `uEmis` 2.5,
`uFall` 26, `uDMax` 3, `uTRate` 9, `uKin` 0.6, velocity damp exp(-0.15 dt), light follow rates 14/s and 3/s,
detail `VT` table (line 2106), key light dir `V_KEY` (0.45,0.85,0.28) normalised.

### 1.5 Lava passes (2D, per frame; `step` line 1850, `updateParticles` 1888, `render` 2017, `postFX` 2074)

Sim grid: short side `q.sim` (160 at high), long side by aspect, so 284x160 = 45 440 cells at 16:9.
`simScale = q.sim/128`.

| # | Pass (FS line) | Inputs | Output (format) | Cost per frame at high |
|---|---|---|---|---|
| L1 | splat (426) x2 | vel/dye, 16 capsule segments | vel RG16F, dye RGBA16F | 1 grid + 1 dye pass, 16-segment loop per texel |
| L2 | curl (472) | vel | curl R16F | 1 grid pass |
| L3 | vorticity (478) | vel, curl | vel | 1 grid pass |
| L4 | forces (~490) | vel, time | vel (curl noise 2 octaves) | 1 grid pass, gradient noise |
| L5 | divergence (461) | vel | div R16F | 1 grid pass |
| L6 | pressure init | pressure | pressure*0.8 warm start | 1 grid pass |
| L7 | pressure Jacobi (503) | p, div | p R16F | `jac`=28 grid passes (20 in host comment at 1850 for some levels; uses q.jac) |
| L8 | gradient (509) | p, vel | vel | 1 grid pass |
| L9 | advect (446) | vel | vel, dissipation 0.2 | 1 grid pass |
| L10 | advectDye (453) | vel, dye | dye, diss 0.32/life, cooling 0.9/life toward `cool` | 1 dye pass (1024 short side) |
| L11 | pUpdate (518-559) | state RGBA32F, vel | state (xy pos, z age, w hue); ring-buffer respawn on segments; RK2 + curl noise | 2.1M fragments |
| L12 | points VS (671) + pFrag (561) | state | accum RGBA16F, additive | 2.1M points, 1 px |
| L13 | density (567) | dye, accum | dens (360 short side) | dye*gain + 4-tap accum |
| L14 | blur (575) x4 | dens | densBlur | 5-tap separable, two radii |
| L15 | light (585-619) | densBlur | lightT | normals from gradient, 28-step shadow march toward light, key light |
| L16 | composite (621) | dye, accum, light, dens | scene RGBA16F | bg gradient + scatter 0.35 + air |
| L17 | bloomPre (638) | scene | bloomLv[0] | threshold/knee |
| L18 | bloomBox (650) | bloom chain | bloom levels | 4-tap box down, additive up |
| L19 | final | scene, bloom | swapchain | ACES, vignette, gamma 1/2.2, IGN dither |

Host-side: `gather` (1792) turns pointer/autopilot motion into 16 segments; a still pointer gives a waving
jet. `updateLight` (2004) smooths the light toward the newest emitter.

### 1.6 Smoke passes (3D in a flat atlas; `step3D` line 1947, `render3D` after line 2106)

Grid X x Y x Z, Y = q.g (112 at high), Z = Y*gz. Scalars RGBA: x density, y temperature, z freshness.

| # | Pass (VFS line) | Output | Notes |
|---|---|---|---|
| S1 | advS (791) | tmp | semi-Lagrangian RK2 forward |
| S2 | advS2 (807) | scal | MacCormack correction with clamp/fallback, decay (dk, exp(-dt/coolTime), exp(-dt/(2.2 life))), emitter injection uDRate 9*emission, uDMax 3, uTRate 9; mass cap when no emitter (sum 841, mass 856) |
| S3 | advV (863) | vel | damp exp(-0.15 dt) |
| S4 | curl (871) | curl | |
| S5 | force (883) | vel | confinement uEps, buoyancy uBuoy (with 0.45*freshness), ABC-flow turbulence uTurb/uTF, emitter pull uKin 0.6, push uPush |
| S6 | div (917) | div | 4 cells packed per texel |
| S7 | jacobi (933) | pres | q.jac iterations, first uMul 0.85, packed RGBA |
| S8 | grad (942) | vel | |
| R1 | to3d | den3 TEXTURE_3D | atlas to hardware 3D texture for trilinear |
| R2 | light (968) | lit (half-res light volume) | 14 quadratic steps toward emitter + 14 toward key light, uFall 26, uExtL V_EXT*3*shadows |
| R3 | crd x2 | crdA, crdB | detail noise coordinates advected by flow, two phases, flowT 1.2 s |
| R4 | march (1003) / marchD | vMarchT (res scale rm) | camera V_CAM 2.4, Beer-Lambert V_EXT 9, step 1/Y, up to 400 steps, jitter, SHADE_MIN 0.01, emission 2.5, optional sub-grid detail |
| R5 | comp (1083) | scene | bg + 4-tap tent upsample |
| R6 | postFX | swapchain | shared with Lava (L17-L19) |

Brush in Smoke: 16 capsule emitters in 3D at mid-depth Z/2 (`V_BRUSH` 757); emitter velocity from segment
motion kv, clamped to 1.5*Y.

---

## 2. Modern design (Mac/Linux/Windows, SDL_GPU)

### 2.1 Layout

- `plugins/viz/krakatoa/` (beside `levels`, `scope`, `spectrum`).
  - `core/`: portable C99, no OS or SDL headers. Parameters, palettes, `P` defaults and ranges, autopilot,
    `gather` (brush segments), `updateLight`, the audio-to-brush mapper (sec 4), and the fixed-point Amiga
    reference simulation (sec 3.6) used by host tests.
  - `sdl/`: SDL_GPU host (device use, textures, pipelines, pass order) and `shaders/*.glsl`.
  - `amiga/`: 68k `.s` hot loops (vasm) plus AGA/RTG glue.
- Knobs persist through `sel_prefs` as `ui.krakatoa.<lava|smoke>.<knob>` (replaces localStorage FX_KEYS).

### 2.2 Shader route

Recommended: keep GLSL as the source, closest to the reference so a diff stays readable.
1. Convert each `FS` / `VFS` entry from GLSL ES 3.00 to Vulkan GLSL 450 with SDL_GPU binding rules
   (fragment samplers set 2, fragment uniforms set 3; vertex samplers set 0, vertex uniforms set 1).
2. Build step: `glslangValidator` to SPIR-V, then `shadercross` CLI to MSL (Mac) and DXIL (Windows).
   Commit nothing generated; embed blobs at build time.
3. No runtime shadercross dependency.

Rejected: hand-port to HLSL plus shadercross (rewrites every shader, loses line-by-line parity) and hand
MSL (three copies).

Translation points: `gl_VertexID` stays (`gl_VertexIndex`); point sprites use `SDL_GPU_PRIMITIVETYPE_POINTLIST`
with size 1, which matches `uPointSize` 1; `texelFetch` on RGBA32F state needs no float filtering; uniforms
via `SDL_PushGPUFragmentUniformData` per draw, matching the `uf(...)` calls; 3D textures via
`SDL_GPU_TEXTURETYPE_3D` with one colour target per layer (`layer_or_depth_plane`), matching vMRT.
At start, check `SDL_GPUTextureSupportsFormat` for RGBA16F/RGBA32F colour target and blend; on failure the
effect declares itself unavailable (no silent downgrade).

### 2.3 Pipeline per pass

One `SDL_GPUGraphicsPipeline` per FS/VFS entry (fullscreen triangle vertex shader `VS_QUAD` 379), plus
one point pipeline (VS_POINTS 671 + pFrag 561, additive blend ONE/ONE). Ping-pong pairs (`.read/.write/.swap`)
map to two `SDL_GPUTexture` each. Each pass = one render pass with the target bound, `LOAD_OP_DONT_CARE`
except additive targets (`CLEAR` for accum, `LOAD` for bloom up-chain). All of one frame goes in one
command buffer. Pass order exactly sec 1.5 / 1.6.

### 2.4 Fullscreen entry and window sharing

- Entry: F38 fullscreen visual mode (arch sec 7): the desktop UI hands the whole surface to the viz plugin
  and hides chrome. F38 fullscreen does not exist yet; it is built in KRK-P0-1.
  `plugins/ui/desktop/backend_sdl.c` uses `SDL_CreateRenderer` (verified). SDL_Renderer and a separate SDL_GPU device cannot both own one window.
  Recommended: create the desktop renderer with the `gpu` render driver and take its device
  (`SDL_GetGPURendererDevice`, SDL 3.4) so chrome and visualizer share one device **(to confirm SDL version
  in tree)**. Alternative: tear down the renderer on fullscreen entry and claim the window for SDL_GPU,
  then restore (slower toggle, two code paths).
- Input: the viz API has no input today; the fullscreen session hook adds pointer and key events (sec 0).
  F38 says "any key restores"; this conflicts with Krakatoa keys (sec 6, Q3).

### 2.5 Frame pacing with audio

- Present with `SDL_GPU_PRESENTMODE_VSYNC`; dt from the monotonic clock, clamped as the reference `tick`
  (line 2267) does.
- Audio features come from the engine taps (arch sec 6) as timestamped snapshots. The visualizer reads the
  snapshot whose timestamp matches the audible position (play position minus output latency), so beats land
  on the frame where they are heard.
- Smoke governor: kept (it is part of the reference). Tests pin the quality level.

---

## 3. Amiga design (68060 AGA and RTG; 68030 reduced; 68020 hidden)

### 3.1 Cycle budget, 68060 at 50 MHz

- 50 M cycles/s. Target 25 fps (every second PAL VBlank): 2.0 M cycles per frame.
- Reserve for playback and system: 20 percent, so 1.6 M cycles.
- Chip RAM output: a full 320x256x8 c2p is chip-bus bound at roughly 10-15 ms on 060 (measure: I556 data
  shows the desk c2p path is far slower because of dirty-band unions; this effect bypasses that path, see 3.4).
- Plan: render chunky at 160x128, c2p 2x1 into 320-pixel lines (40 960 bytes to chip), vertical doubling by
  copper bitplane modulo (each line shown twice). Estimated 6 ms = 0.3 M cycle-equivalents.
- Remaining compute budget: about 1.2-1.3 M cycles per frame.

### 3.2 Lava on 060 (verdict: feasible at 25 fps, 160x128 doubled to 320x256)

| Stage | Size | Cycles per unit (estimate) | Cycles per frame |
|---|---|---|---|
| Sim passes L2-L6, L8 (curl, vorticity, LUT curl noise, divergence, warm start, gradient) | 40x32 = 1280 cells | 80 | 102 k |
| Jacobi | 8 iterations x 1280 | 12 | 123 k |
| Advect velocity (bilinear) | 1280 | 40 | 51 k |
| Advect heat + smoke (two scalars, not RGB dye) | 1280 x 2 | 35 | 90 k |
| Splat (16 segments, bounding box only) | about 200 cells | 60 | 12 k |
| Particles: RK2 sample, LUT noise, age, saturating additive plot | 3000 | 80 | 240 k |
| Density + 4 blur passes | 80x64 = 5120 | 8 x 4 + 6 | 195 k |
| Light: gradient normals + one shadow sweep along the light direction (replaces the 28-step march) | 5120 | 25 | 128 k |
| Composite to palette index with ordered dither (upsample 80x64 to 160x128) | 20 480 px | 12 | 245 k |
| Bloom: 40x32 box blur, added before palette lookup; tonemap baked into palette | 1280 | 15 | 20 k |
| **Total** | | | **about 1.2 M** |

Fits the 1.2-1.3 M budget with no margin. Fallbacks if measured over: particles 2000, Jacobi 6.
Working set: 1280 cells x 16-bit x 5 arrays = 12.8 KB, larger than the 8 KB data cache; process in
row strips so the Jacobi pair (p, div) stays cache-resident (5 KB).

### 3.3 Smoke on 060 (verdict: 3D not feasible at 25 fps; recommend 2D slice of the same solver)

Thin 3D grid 32x24x4 = 3072 cells x about 475 cycles (S1-S8 with 8 Jacobi iterations) = 1.46 M cycles for
the sim alone, before marching. Even at half-rate sim (12.5 Hz) plus a 4-slice march (5120 x 4 x 15 = 307 k),
light (100 k) and composite (245 k) the total is about 1.4 M: over budget.

Recommended: Z = 1 slice of the same method (MacCormack scalars density/temperature/freshness, buoyancy
0.9*lift, confinement, emitter push/pull, decay constants, mass cap), 40x32 grid, cost close to Lava without
particles (about 0.9 M), lit with the Lava light sweep and coloured with the smoke palette (ember, smoke, tail).
Open question Q1.

### 3.4 AGA output (8-bit planar)

- The effect owns its screen in fullscreen mode: own double-buffered bitplanes, own copper list with the
  modulo doubling, own c2p. It does not use the desk dirty-band path (I556): every pixel changes every frame,
  so band tracking only adds the double-convert cost.
- c2p: reuse the existing `c2p1x1_8_c5_040` routine family from `plugins/ui/desktop/amiga/backend_aga*.c`
  **(to confirm entry point and whether a 2x1 variant exists; if not, add one in the same file family)**.
- Palette: 256 entries = 4 hue bands (palette `hot[4]`) x 64 intensity levels. Intensity levels carry the
  baked ACES tonemap, gamma, vignette is applied in chunky space (radial LUT), bloom adds before lookup.
  Light colour and palette changes rewrite the 256 entries once per frame through the copper or `LoadRGB32`
  (cheap, 768 bytes). Ordered 4x4 dither replaces IGN dither.

### 3.5 RTG output (chunky)

- Same chunky 160x128 buffer, 8-bit CLUT screen, no c2p; write pixel-doubled to VRAM (320x256) or let the
  board scale if a 320x256 mode exists. 16-bit hi-colour mode is a later row.
- I497: RTG visualizers are slow on rig 2. VRAM writes over the board bus may dominate; measure
  `write_ms` separately with `--stats` before tuning compute.

### 3.6 Fixed-point formats

- Velocity: signed 16-bit, 8.8 cells per frame. Clamp to +-4 cells (CFL for bilinear advection).
- Heat, smoke density, temperature, freshness: unsigned 16-bit 0.16.
- Pressure, divergence: signed 16-bit 4.12 stored, 32-bit accumulators.
- Particle state: x, y as 16.16 in screen space (160x128), age 8.8 s, hue index 8-bit.
- Every `exp(...)` and `1/x` in the reference is a per-frame constant: computed once per frame from a LUT
  (no FPU dependency, so 68030 and 68060 run the same code; no float under interrupt, arch sec 12).
- Divides: Jacobi divide by 4 = shift. Noise: 64x64 precomputed curl-noise vector table, scrolled by time.
- `muls.l` is fast on 060; on 030 prefer 16x16 `muls.w`.

### 3.7 C vs asm split

- C99 fixed-point core (`core/`) is the single source of truth for the algorithm. It runs on host for tests
  and on Amiga as the reference path.
- 68k asm (vasm) replaces only the hot loops: Jacobi, advection, particle update+plot, blur, light sweep,
  composite/palette index, c2p 2x1. Each asm routine has the same signature as its C twin; a rig test runs
  both on one frame state and compares outputs (exact match, integer math).

### 3.8 68020 and 68030

- 68030 at 50 MHz (about 0.35 of a 060 at the same clock): Lava reduced is feasible at 12.5 fps:
  24x20 grid, 500 particles, 80x64 chunky quadrupled, 6 Jacobi iterations. Estimate about 0.35 M 060-cycle
  units, about 1 M 030 cycles, inside the 4 M cycles of an 80 ms frame. **Unverified estimate.**
- 68030 Smoke: hidden.
- 68020 (A1200 base, 14 MHz): both hidden. Tier gate via `sel_machine` (`SEL_MACHINE_CPU`,
  `SEL_MACHINE_CPU_SPEED`) and the plugin's `SEL_VIZ_TIER` tag (arch sec 12).

---

## 4. Audio to brush mapping

Signals: `sel_viz_frame` (sec 0), 50 blocks per second. At 25 fps each visual frame consumes two blocks;
take the max of `band`, the OR of `note_on`. Band centres (log spaced, ratio about 1.48): 43, 64, 94, 140,
207, 306, 453, 671, 993, 1470, 2177, 3222, 4770, 7062, 10454, 15476 Hz. `spectrum_valid` 0 (machines that skip
the bands) falls back to the peak of the PCM ring for `L` and to `note_on` for beats; `B` and `H` then stay 0.5.

Derived per frame (all clamped to the reference slider ranges):

- `L` level: RMS of `pcm`, attack 10 ms, release 300 ms, normalised by a peak follower that decays
  over 4 s (auto gain). Range 0-1.
- `B` bass: mean of `band[0..3]` (43-140 Hz), same normalisation.
- `H` highs: mean of `band[12..15]` (4.8-15.5 kHz), same normalisation.
- Beat: v1 uses onset for all formats (Q5); later, tracker formats can use `note_on` on voices whose
  `pitch_q8` is low or whose `instrument` is percussive. Onset = `B` > 1.4 x its 1 s
  moving average, with a 200 ms refractory time. Beat strength `s` = (B - avg)/avg, clamped 0-1.

Mapping (knob values are the user's slider values; music modulates around them):

- force = P.force x (0.5 + 1.0 x L)
- swirl = P.swirl x (0.5 + 1.0 x H)
- emission = P.emission x (0.3 + 1.2 x B) + burst, burst = 3 x s x exp(-t/0.12 s) after each beat
- Heat injection on beat: one splat at the brush with amount 0.2 x emission x (1 + 2 s); Smoke: temperature
  injection uTRate x (1 + 2 s) for 120 ms.
- Brush path: the reference autopilot (`updateDemo` 1737-1758) moves the brush; its speed scales with tempo
  (tracker BPM/speed when known, else beat interval); each beat advances the Lissajous phase by a quarter turn.
- Pointer: while the pointer is down it is the brush (segments from `gather`); music still modulates force,
  swirl and emission. Pointer released for 2 s: autopilot resumes.
- The autopilot knob becomes "Music brush" (on by default); off = pointer only.

---

## 5. Reachability (definition of done)

One top-level test per target, with sentinel counters and the stated numbers:

- **Host (core)**: ctest `krakatoa_reach`: drives the viz registry (arch sec 13, fake `sel_machine` per tier)
  with a fake tap carrying a 120 BPM click track for 10 s at 25 fps. Asserts sentinels `krk_frames == 250`,
  `krk_jacobi_iters == 8 per frame`, particle count 3000 (060 tier), 500 (030 tier), effect absent at 020
  tier, `krk_beats == 20 +- 1`, and emission bursts per beat.
- **Mac SDL_GPU**: run the desktop app with `--ui desktop --backend sdl`, fullscreen visualizer Krakatoa,
  `--stats`, offscreen frame capture after 120 frames. Asserts pass count per frame = sec 1.5 table
  (Jacobi 28 at high), particle count 2 097 152 at high, frame not black (mean luminance within a band).
- **AGA rig 2 (I559 done)**: real song `virgill-that_meme_again.mod`, fullscreen Krakatoa Lava, `--stats --log`.
  Log must show grid 40x32, particles 3000, mean frame ms <= 40, c2p ms, beats > 0. Then Johan checks rig 3.
- **RTG rig 2**: same, plus `write_ms`.

Knob coverage (host test on the C core): perturb each knob from default to range end, hash the chunky frame,
assert the hash changes. Exemptions in writing: `coolTime`, `lift` in Lava; `smoke`, `particles`,
`showSmoke`, `showParticles` in Smoke (the reference ignores them in that mode too). Amiga-only: none; bloom
and shadows move the palette and light sweep.

---

## 6. Open questions (recommended option first)

- **Q1 Smoke on 060**: (a, recommended) 2D slice of the same solver at 40x32, 25 fps; (b) thin 3D 32x24x4 at
  12.5 fps sim, about 1.4 M cycles, likely below 25 fps display.
- **Q2 Smoke governor vs 1:1**: (a, recommended) keep `VOL_FPS` governor on modern (it is reference behaviour),
  tests pin quality; (b) fixed quality, user picks.
- **Q3 Keys in fullscreen**: F38 says any key restores; Krakatoa uses Space, C, M, 1-5, A, H.
  (a, recommended) Esc restores, other keys go to the effect; (b) any key restores, knobs only in a settings panel.
- **Q4 Start order**: ledger says AGA first. (a, recommended) KRK-P0 C core and host tests first (it is also
  the AGA reference), then AGA; (b) AGA asm directly.
- **Q5 Beat source for trackers**: (a, recommended) energy onset for all formats in v1, note-on heuristic later;
  (b) note-on heuristic from v1.
- **Q6 Per-channel brushes**: (a, recommended) one brush in v1, as the reference; (b) one emitter per tracker channel.
- **Q7 SDL window sharing**: (a, recommended) `gpu` render driver, shared device; (b) tear down renderer on entry.

Decisions (Johan 2026-10-27):
- Q1: (a) 2D slice of the same solver on 060.
- Q2: (a) keep the `VOL_FPS` governor on modern; tests pin quality.
- Q3: (a) Esc restores; other keys go to the effect. F38 text changes to Esc only.
- Q4: (a) KRK-P0 portable C core and host tests first, then AGA.
- Q5: (a) energy onset on the bass bands for all formats in v1; note-on later.
- Q6: (a) one brush in v1.
- Q7: (a) `gpu` render driver with one shared device (check that the tree has SDL 3.4 for SDL_GetGPURendererDevice).

---

## 7. Conflicts between the reference and Johan's decisions

1. Smoke auto-quality governor changes the grid at runtime; "numbers 1:1" needs a rule (Q2).
2. Reference depends on float HDR targets, ACES and float 3D textures. SDL_GPU has them; Amiga does not:
   the Amiga look is baked into palette LUTs (sec 3.4), so "same look" is approximate there.
3. Reference Smoke is 3D ray-marched; "same method scaled down" on 060 can keep the solver but not the third
   dimension (Q1).
4. Reference autopilot is a time-based wander; decision 3 replaces it with the music brush (sec 4).
5. F38 "any key restores" vs reference key bindings (Q3).
6. Ledger "start with AGA" vs the need for a host reference core (Q4).
7. Selecta viz contract: "the host owns the pixels and the colours", integer only, stateless render hook.
   Krakatoa needs its own palette, state, dt, input and (modern) floats on the GPU. Resolved by a new
   fullscreen session hook (KRK-P0-1); the existing contract stays for small visualizers.

---

## 8. Checklist

- KRK-P0-1 Viz API: stateful fullscreen session hook (open/frame with dt/input/close, palette or GPU ownership) in `include/selecta/viz.h`; F38 fullscreen mode in the desktop UI (enter, Esc restores per Q3).
- KRK-P0-2 `core/`: params, ranges, palettes, `sel_prefs` keys, gather, updateLight, autopilot.
- KRK-P0-3 `core/`: audio mapper (L, B, H, beat, formulas in sec 4) on engine taps.
- KRK-P0-4 `core/`: fixed-point Lava (sec 3.2, 3.6) in C.
- KRK-P0-5 `core/`: fixed-point Smoke 2D slice (pending Q1).
- KRK-P0-6 Host `krakatoa_reach` test plus knob coverage test (sec 5).
- KRK-P1-1 AGA fullscreen screen, copper doubling, own double buffer.
- KRK-P1-2 c2p 2x1 (reuse family) and palette writer.
- KRK-P1-3 68k asm hot loops for Lava with C-twin comparison on rig.
- KRK-P1-4 Rig 2 run with `--stats`, numbers per sec 5; Johan check rig 3 (I559 done).
- KRK-P2-1 AGA Smoke.
- KRK-P3-1 RTG output and `write_ms` (I497).
- KRK-P4-1 SDL_GPU device sharing (Q7) and shader build (glslang + shadercross).
- KRK-P4-2 Lava pipeline 1:1; KRK-P4-3 Smoke pipeline 1:1; KRK-P4-4 Mac reach test.
- KRK-P5-1 68030 reduced Lava; tier gating for 020.
