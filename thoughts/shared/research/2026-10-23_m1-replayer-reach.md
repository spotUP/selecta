---
date: 2026-10-23
topic: What plays plain .mod at M1; nuade inventory; libopenmpt as PCM adapter; duplicate-library linker warning
tags: [research, m1, nuade, openmpt]
status: complete
---

# R8: M1 replayer reach

Feeds architecture design section 4 (replayer SDK) and section 14 (M1). Documentary; no code changed.

## 1. Which nuade player loads plain M.K. .mod

Answer: none. Plain ProTracker "M.K." / "M!K!" / "4CHN" / "6CHN" / "8CHN" .mod is not accepted by any nuade player at pin 111c965.

- `protracker4` accepts only IFF: `parse_module` requires `FORM` at 0, `MODL` at 8, `VERS` at 12 (third_party/nuade/players/protracker4/protracker4.c:256-259); `pt4_probe` is just `parse_module` (protracker4.c:304-307). Header comment: "The module is an IFF file (FORM MODL)" (protracker4.c:4). The only 0x438 use is reading an "8CHN" tag inside an already-IFF module (protracker4.c:195,201).
- `audiosculpture` reads the tag at 0x438 (`MOD_TAG`, audiosculpture.c:26) but accepts only `FLT4` or `EXO4` (audiosculpture.c:733) and then needs a sample with length 0 and volume != 0 (audiosculpture.c:735-739; header include/nuade/players/audiosculpture.h:194). A bank file next to the module is also needed (players/audiosculpture/audiosculpture_nuade.c:17). So it does not play ordinary FLT4 files.
- Other ProTracker-adjacent players are not .mod loaders: `amos` converts an AMOS bank to an in-memory PT song (amos.c:4-5); `timetracker` is a packed "TMK." format (timetracker.c:4); soundfx/medley only use PT period tables.

How established: (a) grep of players/, src/, include/ for `M.K.`, `M!K!`, `FLT4/FLT8`, `nCHN`, `0x438`, `0x4D2E4B2E`, char-literal forms: zero hits except the two files above; (b) counted the player set from the generated registry source: `nuade_add_players` writes `nuade_registry_players[]` with one entry per dir (third_party/nuade/cmake/nuade_players.cmake, the `entries`/`count` block), and "all" globs `players/*/nuade_player.cmake` = 121 (see Q2). Each of those has a probe; I did not read all 121 probes line by line, so a player that detects by computed structure rather than a literal tag is not excluded by grep alone (unverified for the other 119). Practical test: run `selecta`/nuade with NUADE_PLAYERS=all against a real M.K. file (not done; needs a sample file).

Consequence for M1: nuade covers none of the most common tracker format. Plain .mod needs another replayer (libopenmpt, Q3).

## 2. nuade player inventory, floats, licences

- Count: 121, from two structures: `ls -d players/*/` = 121 and `ls players/*/nuade_player.cmake` = 121 (the glob the "all" mode uses, nuade_players.cmake `nuade_resolve_players`; registry array `nuade_registry_players[]` is generated from the same list). design doc says 119 formats for the nuade class; the 2-player gap is not reconciled here (unverified; likely multi-id formats such as markcooksey/markcooksey_old, hippel variants).
- Float/double: whole-word `float|double` in players/*/*.c|h: 32 of 121 players have hits, 89 are float-free. Hit players: actionamics, activisionpro, bendaglish, bendaglishsid, davidwhittaker, deltamusic1, deltamusic2, digmug, dss, facethemusic, fred_np, futurecomposer, gmc, instereo1, instereo2, jamcracker, medley, musicassembler, oktalyzer, quadracomposer, ronklaren, sidmon2, silmarils, sonicarranger, sonix, soundcontrol, soundfactory, soundfx, soundmon, synthesis, thomashermann, voodoo. Spot check: voodoo uses `float` for pan and for the voice API volume argument (voodoo.c:1611,1633: `nuade_voice_set_volume(..., (float)c->volume / 64.0f)`). So the nuade voice API itself takes float volume; the shim (plugins/replayers/nuade/voices_shim.c) must convert to our integer voices. I did not classify each hit as API-only versus arithmetic (unverified); matters for the 68020 no-FPU tier (design section 12).
- Licence: nuade itself MIT, "Copyright (c) 2026 Daniel Collin" (third_party/nuade/LICENSE:1-3). No per-player LICENSE/COPYING files (find). THIRD_PARTY_NOTICES.md: 
  - 25 players are ports of NostalgicPlayer (Thomas Neumann), MIT, text reproduced (notices lines 3-45), soundfx auto-slide credits Flod 4.1 (Christian Corti) as upstream (unverified licence of Flod).
  - NOT MIT, flagged: "Eagleplayer data" copied unchanged from UADE eagleplayers, "not covered by nuade's MIT license; the eagleplayers state no license": players/pokeynoise/pn_waves.c, players/davidhanney/dh_data.c, players/majortom/mt_data.c (notices lines ~61-71). These three players carry unlicensed data tables: risk for any redistribution; decision for M1: exclude or replace those three from the shipped set.
  - Correction 2026-10-23 (user): the whole nuade package, these three players included, is by one author under one licence. The flag is withdrawn; all 121 players are built.

## 3. libopenmpt as a desktop PCM replayer adapter

- On machine: Homebrew `/opt/homebrew/opt/libopenmpt`, version 0.8.4 (pkg-config `0.8.4+release.autotools`, Cellar 0.8.4_1). Static `libopenmpt.a` and dylib present. Not in the repo (no third_party/libopenmpt; plugins/replayers has only nuade). Other copies on disk, not Selecta's: ~/Code/DEViLBOX/third-party/openmpt-master and libopenmpt-render (and devilbox-* clones) , ~/Code/DEViLBOX/openmpt-soundlib-wasm.
- Licence: BSD-3-clause (Cellar share/doc/libopenmpt/LICENSE: "Copyright (c) 2004-2025, OpenMPT Project Developers and Contributors; (c) 1997-2003 Olivier Lapicque"). Bundled third-party deps in the full source tree have their own licences (unverified).
- C API in /opt/homebrew/opt/libopenmpt/include/libopenmpt/libopenmpt.h:
  - create: `openmpt_module_create_from_memory2` (:731); destroy `openmpt_module_destroy` (:737)
  - probe: `openmpt_probe_file_header` (:604) (returns SUCCESS/FAILURE/WAKEUP-more-data, matches our SEL_PROBE_YES/NO/UNSURE)
  - render: `openmpt_module_read_interleaved_stereo(mod, samplerate, count, int16_t*)` (:1100): int16 stereo, no float needed
  - metadata: `openmpt_module_get_metadata(mod, key)` (:1173)
  - subsongs: `openmpt_module_get_num_subsongs` (:1275), `openmpt_module_select_subsong` (:869)
  - position/length: `openmpt_module_get_position_seconds` (:953), `set_position_seconds` (:946, seek), `get_duration_seconds` (:926)
  - channels: `openmpt_module_get_num_channels` (:1282)
- Design doc: section 4 already defines the kind. `SEL_RP_KIND_RENDER = 2` exists in include/selecta/replayer.h:13; design doc table row "SEL_RP_KIND_RENDER ... plugin, int16 stereo into host buffers -> mixer sink" (design doc line 216) and the hook `SEL_RP_RENDER (state, int16_t* stereo, int16_t* const* per_voice, uint32_t frames)` (design doc line 246). Gap: `include/selecta/tags.def` has RP tags 0x1000-0x100D only (NAME, VERSION, KIND, EXTENSIONS, FORMATS, MAX_VOICES, NEEDS_CHIPMEM, PROBE, OPEN, CLOSE, START, TICK, METADATA, FLAGS; tags.def:28-41); there is no `SEL_RP_RENDER`, `SEL_RP_SUBSONG_COUNT`, `SEL_RP_DURATION`. Those must be added at M1 before an openmpt adapter (grep `RENDER` tags.def: no hit). `sel_rp_info` already has subsong_count (replayer.h:~28).
- Float: libopenmpt integer-stereo API suffices; internal mixing float is its own business and the Amiga tier is out of scope for the openmpt plugin (design: "Desktop" M1/M2).
- Cost note: Homebrew has it only for the host; linking the Homebrew dylib is fine for dev, a shipped build needs a vendored pinned copy (consistent with how nuade is vendored as a submodule). Decision for owner.

## 4. Duplicate nuade library linker warning

Reproduced in a fresh configure+build in the scratchpad (build/host was up to date and printed nothing; no repo files touched, no other heavy job was running): last line of the log (scratchpad/build2.log:80), when linking `apps/selecta-cli/selecta-cli`:

    ld: warning: ignoring duplicate libraries: 'plugins/replayers/nuade/libsel_plugin_nuade.a', 'plugins/replayers/nuade/libselecta_nuade.a'

Cause (plugins/replayers/nuade/CMakeLists.txt):
- :43 `sel_plugin_nuade` links PUBLIC `selecta_nuade`.
- :47 `selecta_nuade` links PUBLIC `sel_plugin_nuade`. This is a deliberate static-library cycle: the players call `nuade_voice_*` which are defined in the adapter's voices_shim.c (comment :44-45).
- :46 `LINK_INTERFACE_MULTIPLICITY 3` makes CMake repeat the pair 3 times on the link line, a workaround for GNU ld's single-pass archive resolution on the 68k build. Apple ld rescans archives and warns that the repeats are redundant. The warning is therefore the multiplicity hack firing on a linker that does not need it. (No other target links selecta_nuade twice; the only other mentions are the dedicated tests/nuade_ab library, which is separate by design.)

Minimal correct fix, root cause not suppression: remove the cycle instead of repeating it. The cycle exists because the framework's undefined symbols (`nuade_voice_*`, io shim) are provided by the plugin. Move `voices_shim.c` and `io_shim.c` into the `selecta_nuade` library (they only need include/selecta plus nuade headers, same as the adapter), keep `adapter.c` in `sel_plugin_nuade` linking PUBLIC `selecta_nuade`, and delete lines 46-47. Then the dependency is one-way, one pass resolves on GNU ld too, and no warning exists on any linker. Verification: rebuild host (warning gone) and the amiga build (cannot be run here; unverified) and the T7 sdk-isolation target (voices_shim currently counted as plugin code).
Cheaper but weaker option (not recommended, keeps the hack): wrap line 46 in `if(SEL_AMIGA)`; this removes the warning on host but leaves the cycle.
