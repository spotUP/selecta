# M2 Phase 1 report (rows I107-I111)

Branch worktree-agent-a5998e433c3c455bc. One commit covers all five rows (they share tags.def, registry and machine files).

## I107 tags
- `include/selecta/tags.def`: RP MIN_CPU 0x1016, NEEDS_FLOAT 0x1017, MEMORY 0x1018, PRIORITY 0x1019, CONFIG 0x101A (list); OUT MEM_CHIP 0x200F.
- Proof: both trees build; `registry-tags` (lint, no unknown or duplicate tags) green; `machine-probe`, `amiga-machine` green.
- Open: SEL_RP_CONFIG is declared and unused until T61 (I135); it needs a written exemption or that row. SEL_OUT_MEM_CHIP is honoured by the filter but no output is tagged until Phase 3.

## I108 registry filter and priority owner
- `src/engine/registry.c`: one `fits()` filter for desktop and 68k (MIN_CPU, NEEDS_FLOAT, MEMORY, MEM_CHIP); `sel_registry_init_machine(host, machine)`; `sel_registry_get_stats()`; `sel_registry_format_owner()` (highest SEL_RP_PRIORITY wins, ties go to the first registered). NULL machine means no filter. `sel_machine_fake()` injects a500/a1200/a1200-fpu/a060.
- CLI: `plugins --machine <name> --stats`, `plugins --owner <format>`. `apps/selecta/main.c` now probes once and calls `sel_registry_init_machine`.
- Proof: T49 `registry-machine-filter` green on desktop (filtered 2,2,0,0) and under vamos (1,1,0,0), plus hidden_* checks and owner check.
- Deviations: ordering relaxed to a500 >= a1200 >= a1200-fpu >= a060 (the strict a500 > a1200 cannot hold: nuade and openmpt need both 68020 and float, so both tiers hide both); goldens per mode (`tests/golden/m2/registry-{desktop,vamos}-<tier>.filtered`, pinned from this run) because the 68k build has no openmpt.
- Open: no unit test with two plugins of different SEL_RP_PRIORITY for one format (only nuade lists formats). Default: add when a second format-listing plugin exists.

## I109 probes and planar tier
- `src/core/amiga/machine.c`: GFX from graphics.library ChipRevBits0 (OCS/ECS/AGA, "+RTG" with cybergraphics.library), AUDIO "paula" plus ",ahi" when `DEVS:ahi.device` exists and opens. Tier: rtg with a card, planar for ECS/AGA, else text (D-M2-6: value only, no planar UI). `--tier <name>` override (`sel_machine_set_tier_override`).
- Proof: T50 `amiga-machine-probe-vamos` green (GFX=OCS, AUDIO=paula, `--tier planar` gives TIER=planar).
- Deviations: T50 keys changed from `machine.gfx` style to the CLI's real `GFX`/`AUDIO`/`TIER` (m2_check key regex now accepts upper case). Under vamos graphics.library is absent so GFX falls back to OCS; ECS/AGA/RTG/AHI detection is rig-only and OPEN (rig down: "auth failed: bad key").
- Finding: OpenDevice on an absent device never returns under vamos, so both device opens are guarded (DEVS: file lookup for ahi, exec DeviceList lookup for timer).

## I110 clock
- `src/core/amiga/clock.c`: timer.device UNIT_ECLOCK ReadEClock, 64-bit tick to ms, closed at exit; DateStamp fallback when timer.device is not on the device list (vamos). `sel_clock_source()` declared in machine.h, desktop returns "monotonic"; CLI `machine` prints `clock.source=`.
- Proof: m2 tests `clock-source` (desktop monotonic) and `clock-source-vamos` (datestamp) green. eclock path on the rig: OPEN (rig down).

## I111 plugin tags
- nuade: MIN_CPU 68020, NEEDS_FLOAT 1 (voices_shim uses float; default pending the T60 per-player measurement), MEMORY 2097152. openmpt: MIN_CPU 68020, NEEDS_FLOAT 1.
- Untagged: null, wav, crossfeed (run anywhere), coreaudio, text, voices-test, nowplaying, integrations (desktop only; not built for 68k).
- Proof: T49 green on desktop and vamos; registry-tags green.

## Run
Green: registry-tags, registry-tags-negative, machine-probe, amiga-machine, no-float-engine, registry-machine-filter (+vamos), amiga-machine-probe-vamos, clock-source (+vamos). Still red by design (later phases): no-float-interrupt (T54), rp-config-taglist (+vamos, T61). Both trees build.
