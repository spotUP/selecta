---
date: 2026-11-01
topic: Amiga replayer paths per format and output (HippoPlayer-68k native, Eagleplayer host, nuade, mixes)
tags: [amiga, replayer, per-tier-owners, hippoplayer, uade, eagleplayer, nuade, ahi, i871]
status: phase 1 (paper) done; phase 2 rig measurements open (section 5)
---

# Amiga replayer paths (I871, phase 1)

Ledger rows I868, I869, I870, I871. Paper evaluation only: no code, no builds, no rig runs. Implements the "Per-tier owners" paragraphs of `thoughts/shared/plans/2026-10-22-architecture-design.md:279-281`. Everything marked UNVERIFIED was not read from source in this pass; everything else names the file and line it came from.

Sources read: `~/Code/HippoPlayer-68k/code/puu016.s` (player table and flags), `code/eagleplayers/bin/`, `code/playsid.library/`, `code/LICENSE.md`; `~/Code/UpRough/uprough-uade` branch `feature/uade-music-backend` (HEAD ba3f76910, 2026-09-21) `amiga/uade_formats.txt`, `amiga/uade_arena_sizes.tsv`, `thoughts/spot/uade_ledger.md` (read only via `git show` and the working tree, branch untouched); `third_party/nuade` (`include/nuade/voice.h`, `players/`); Selecta `include/selecta/replayer.h`, `include/selecta/voices.h`, `src/engine/engine.c:584-596,1603-1655`, `plugins/replayers/{kplayer,nuade}`, `plugins/outputs/`; research docs R2 (`2026-10-22_devilbox-api-and-uprough-uade.md` section B), R10 (`2026-10-23_hippoplayer-native-players.md`), the Modland audit (`2026-10-31-modland-format-audit.md`). Modland counts below come from a read-only query of `~/.config/selecta/library.db` (`songs`, source `modland`, folder under `pub/modules/`, 516219 rows), the same method as the audit.

## 1. Five facts that change the question

1. **Half of HippoPlayer's players are Eagleplayers.** `puu016.s` has 93 player entries (`p_*` labels, 38064-53654). 46 of them jump into HippoPlayer's own DeliTracker/Eagleplayer host (`deliPlay`, `deliEnd`, ... ; `loadDeliPlayer` at puu016.s:53982, `deliPlay` at :54708, `DTP_Interrupt` and `EP_Get_ModuleInfo` handling at :54488-54651) and load unmodified Aminet Eagleplayer binaries from `code/eagleplayers/bin/` (README: "collected from Aminet and have not been modified"). Examples: Sonic Arranger, Sonix (IFF-SMUS), TFMX and TFMX Pro (the `p_tfmx_`/`p_tfmxpro_` entries), Hippel 7V, Hippel ST, Digital Mugician 1 and 2, Rob Hubbard, Richard Joseph, MaxTrax, Quartet, Quartet ST, Symphonie, DeliTracker Custom. So for those formats "HippoPlayer vs UADE Eagleplayer" is the same class of 68k binary; the real comparison is **HippoPlayer's Eagleplayer host vs UpRough's Eagleplayer host**, not two players.
2. **The other 47 HippoPlayer entries are own players**, all hardware-exclusive Paula pokers except the mixing ones. Only these carry HippoPlayer's AHI flag `pf_ahi`: Hippel-COSO (:43638), DigiBooster Pro (:44279, AHI only), PS3M `p_multi` (:45199), sample player (:45984), MIDI stream, Ogg, VGM (:51959-52255). xmaplay060 names itself "FastTracker2 xmaplay060 AHI 32ch" but does not set `pf_ahi` (its `ahiupdate` slot is `p_NOP`, ~:53333); it handles AHI internally (UNVERIFIED how). Every other HippoPlayer player, and every Eagleplayer on either host, writes Paula registers and has **no AHI path**.
3. **Selecta already has the integer voice path nuade needs.** `include/selecta/voices.h` is nuade's voice API with integer arguments ("no float crosses this boundary"); `plugins/replayers/nuade/voices_shim.c` converts nuade's float/double calls to it, and the adapter sets `SEL_RP_KIND_VOICE` and `SEL_RP_NEEDS_FLOAT` (`adapter.c:625,641`). The engine routes a VOICE replayer either to the output's own voice sink (Paula direct, no mixing) or to the software mixer (`engine.c:584,1654-1655`), which feeds AHI or paula14. So nuade on Paula output costs only the tick (no mixing), and nuade on AHI costs tick plus `sel_mixer`. No AHI *channel* sink (AHI_SetSound per voice) exists yet; `plugins/outputs/ahi` is a stream output (I124 open).
4. **All 125 nuade players are tick players.** `grep .render_raw third_party/nuade/players` returns no file, so every nuade player drives voices and gets scopes, voice state and samples from the same API on Mac, web and Amiga. 54 of the 125 player directories mention `float` or `double` (count of files by grep, not of hot code: which of them use float inside the tick is UNVERIFIED and is what I135 measures). nuade lacks: MOD-family trackers beyond `protracker4`, XM, IT, S3M, MTM, AHX/THX, TFMX, Art of Noise, OctaMED (only `mmdc`), DigiBooster 1 and Pro, Musicline, PreTracker, DeliTracker Custom (`ls third_party/nuade/players`).
5. **The UpRough host is a bare-metal demo host.** R2 B2/B4: fake ExecBase, takes the VBL vector and both CIA vectors, parks Paula, owns DMACON/INTENA; no OS cooperation, no AHI. Its allow-list keeps 53 players (`uade_formats.txt`, `keep` rows), of which 40 have measured arenas on a stock 68000 (`uade_arena_sizes.tsv`). The song-switch crash is now characterised (`thoughts/spot/uade_ledger.md`, uprough repo, lines ~40-90): deterministic, the 33rd re-entry of `contplayer` faults at PC=4 with a0=0; the cause is low-memory state (`$118-$12c`, `$180-$198`, message traps, Intuition remember list) that `_music_init` clears once and `contplayer` never re-clears. Fix options recorded there: cold restart of score per switch (safe, slower) or find the overflowing list (unknown cost). HippoPlayer's host runs inside an OS application already (HippoPlayer is multitasking-friendly), and supports UPS scopes, subsongs, seek per player flags; whether it has its own switch or leak problems is UNVERIFIED.

## 2. Per-format matrix

Counts: Modland songs in the library. Formats >= ~500 songs individually; Amiga-relevant module formats only. Console, PC-chip and streaming formats (SPC, NSF, GBS, PSF, VGM, MDX, ...) are out of this question: none of the three candidates is the natural owner, they stay with portable C emulator cores (note: HippoPlayer has its own VGM player with AHI, :52240, UNVERIFIED scope).

Column key. **Hippo own**: HippoPlayer player in source. **Hippo EP**: Eagleplayer binary run by HippoPlayer's host. **UpRough EP**: UADE Eagleplayer binary in the UpRough allow-list (`keep`). **nuade**: player directory in `third_party/nuade/players`. Flags from puu016.s `p_liput`: seek = `kelaus`, pos = `poslen`, end = `end`, sub = `song`, scope = `scope` (Poke = patched player, UPS = Eagleplayer UPS struct), ahi = `pf_ahi`.

| Format | Songs | Hippo own | Hippo EP | UpRough EP | nuade | Paula owner (proposal) | AHI / paula14 / AmiGUS owner (proposal) | Notes |
|---|---|---|---|---|---|---|---|---|
| Protracker (MOD) | 80788 | KPlayer `p_protracker` (pos, seek, sub, end, scope Poke; no ahi; 68000; built in Selecta as I219, pattern cells from C `modwalk.c`); PS3M also plays MOD (020+, ahi) | - | not kept (PTK-Prowiz upstream, dropped or absent, UNVERIFIED) | `protracker4` | **Decision D4**: KPlayer (built, 68000, smallest) or nuade `protracker4` (portable) | nuade `protracker4` via mixer, or PS3M (020+, own mixer); measure M2 | Biggest format; decide by M1/M2 |
| Fasttracker 2 (XM) | 44293 | xmaplay060 (pos, seek, end, scope; 020 gate; Paula needs 060) | - | - | - | xmaplay060, 68060 | xmaplay060 AHI/AmiGUS, 68020+ (decided, design :281) | No portable alternative on Amiga; openmpt class desktop only |
| Impulsetracker (IT) | 25572 | PS3M (020+, ahi) | - | - | - | PS3M mixed to Paula 8/14-bit, 020+ | PS3M | Decided design :281 (3) |
| Screamtracker 3 (S3M) | 11188 | PS3M | - | - | - | PS3M, 020+ | PS3M | Design :281 keeps S3M with "current owner" (openmpt class); openmpt on 68k is UNVERIFIED to be viable; **Decision D5** |
| IFF-SMUS (Sonix) | 10396 | - | `p_sonix` (end, scope UPS) | SonixMusicDriver (chip 65756 B, pool 19776 B, 68000) | `sonix` | nuade `sonix` if M1 passes, else Eagleplayer | nuade `sonix` | Second biggest Amiga-native format; the one where portability pays most |
| OctaMED MMD0-MMD3 | 5134 (1441+2728+305+660) | `p_med` via external `medplayer`/`octaplayer`/`octamixplayer.library` (pos, seek, sub, scope; 5-8 ch mix; no ahi) | - | Octa-MED owner-skipped; only MMDC kept | `mmdc` only | Hippo `p_med` (needs the OctaMED libraries shipped; licence UNVERIFIED) | none today; **Decision D3** | 8-channel MED needs mixing; openmpt desktop |
| Fasttracker 1 / Multitracker (MTM) | 2038 / 790 | PS3M | - | - | - | PS3M, 020+ | PS3M | 68000: gap |
| Soundtracker (15 instr.) | 1945 | PS3M handles old Soundtracker (R10, puu016.s:34110); KPlayer support UNVERIFIED | - | - | `protracker4` (15-instrument support UNVERIFIED) | same as MOD | same as MOD | |
| AHX / THX | 1390 | `thx.asm` (pos, seek, sub, end, scope Poke; 68000) | - | AbyssHighestExperience (412152 B any RAM, 48044 B chip) | - | Hippo `thx` | none; **Decision D3** | Hively (44 songs): no owner anywhere |
| Quartet ST | 859 | - | `p_quartetst` (scope UPS) | - | `quartet` (ST variant UNVERIFIED) | nuade if it plays ST files, else Hippo EP | nuade | Atari ST format |
| Digibooster Pro | 845 | DBM0 (AHI only, 020+) | - | - | - | none (DBM0 has no Paula path, R10) | DBM0 | |
| Musicline Editor | 817 | `p_mline` via external `mline.library` (sub) | - | refused (speed_hack) | - | Hippo `p_mline` (library licence UNVERIFIED) | none; Decision D3 | |
| DeliTracker Custom | 747 | - | `p_delicustom` (the module is the player) | `custom` kept, no binary needed | impossible (68k code) | Eagleplayer host | none (pokes Paula by design) | Mac/web need UADE emulation; Amiga-only path |
| TFMX (all variants) | 736 | `tfmx.asm`, `tfmx7c.s` (pos, seek, sub, end, scope Poke) | `p_tfmx_`, `p_tfmxpro_` (UPS) | TFMX, TFMX-Pro (pool 79792 B, chip 122188 B), 7V, TFHD variants | - | Hippo own `tfmx`/`tfmx7c` | none; Decision D3 | Which Hippo entry takes which variant UNVERIFIED (R10) |
| **Tail, each < 500** | ~2.5k-3k (sum UNVERIFIED) | Oktalyzer, SoundFX, SoundMon 2/3, FC 1.3/1.4, Art of Noise 4/8, SidMon 1/2, Delta 1, JamCracker, Hippel, Hippel-COSO (ahi), Fred, Mark II, PumaTracker, GMC, Medley, Future Player, Ben Daglish, CustomMade, Dave Lowe, StarTrekker AM, VSS, GlueMon, Beathoven, Music Assembler, DigiBooster 1, PreTracker | Sonic Arranger, Mugician 1/2, Hippel 7V, Hippel ST, Rob Hubbard, Richard Joseph, MaxTrax, Jason Page, Tim Follin, Symphonie, and ~30 more | 53 kept (list in uade_formats.txt) | 125 players; covers most of the Hippo own tail except AoN, DigiBooster, PreTracker | nuade where it covers and M1 passes; Hippo own otherwise; Eagleplayer host for the rest | nuade where it covers; else none | Oktalyzer 449+111, SoundFX 358, Sonic Arranger 251, SoundMon 335, FC 327, AoN 211, Symphonie 174, DigiBooster 168 |
| SID (HVSC, Sidplayer, RealSID, Stereo Sidplayer) | 72622 | `p_sid`/`p_psid` via `playsid.library` (submodule has `resid-68k`, `sidblast`, `usbsid`) | - | - | - | out of this question: I434a | | Listed so it is not forgotten |

Exposed data per candidate (applies to every row):

| Data | Hippo own | Eagleplayer (either host) | nuade |
|---|---|---|---|
| Pattern cells + live row | Only where Selecta has a C module walker (KPlayer: `modwalk.c`); player itself exposes position only (`poslen`) | position via `EP_GetPositionNr` (puu016.s:54651) when the player has it; no cells | per player, through the adapter (`rp_tracker_cells` today for tracker players; per-player coverage UNVERIFIED) |
| Per-voice scope | `quadscopePoke` players (patched source) | UPS players only (`quadscopeUps`) | every player (voice state is the API) |
| Samples / instruments | none from the player; needs a C parser | none | `RP_SAMPLE_GET` already exported |
| Song info | `EP_Get_ModuleInfo`-style author/name for some | `EP_Get_ModuleInfo` | nuade song info |
| Same data on Mac and web | no | no (UADE emulation on desktop could give the same) | yes |

Controls per candidate: Hippo own and Hippo EP players declare stop, continue, volume for nearly all, subsongs, seek and song end per player (flags list above, full dump in scratchpad not kept). UpRough glue exposes start, stop, set_pos, select; pause, master volume and fade are not in its API (R2 B2). nuade: subsongs, seek by re-render (cost UNVERIFIED on 68k), volume and pause in the engine.

Licence: HippoPlayer-68k MIT (`code/LICENSE.md`, 2017 K-P Koljonen). Eagleplayer binaries in either set: Aminet freeware, per-binary terms UNVERIFIED. OctaMED player libraries, `mline.library`: external third-party, redistribution UNVERIFIED. UADE `score.s`: UADE licence, UNVERIFIED (UADE is GPL-2 in general; check `amigasrc/score` header before shipping). nuade: MIT (`third_party/nuade/LICENSE`, 2026 Daniel Collin).

Portability: only nuade gives one player, one data path and one bug fix for Mac, web and Amiga. Hippo own players and Eagleplayers are Amiga-only code; on the desktop the same formats run through a different player, so pattern/scope data and sound can differ per platform.

## 3. Recommended path

1. **nuade is the default Amiga owner for every format it covers, on every output**, if I135 shows its tick fits the budget on a 68020 without FPU. On Paula it is Paula-direct through the existing `sel_voices` sink with no mixing, so its only extra cost over a native player is C tick code; on AHI, paula14 and AmiGUS it goes through `sel_mixer`. It is the only candidate that reaches AHI for 4-voice formats without new machinery, and it gives the same scopes, samples and pattern data as the desktop. 68k assembly goes only into hot functions the profile names (I870 steps 2-3).
2. **HippoPlayer own players own what nuade does not cover** and stay Paula-direct (plus their own AHI where they have it): XM (xmaplay060), IT/S3M/MTM/FT1 (PS3M), AHX, TFMX, Art of Noise, OctaMED (`p_med`), DigiBooster 1 and Pro, Musicline, PreTracker; and they take a nuade format back on any tier where nuade fails the M1 budget (68000 especially).
3. **One Eagleplayer host for the long tail** (DeliTracker Custom and composer formats neither of the above covers). Recommended host: **port HippoPlayer's DeliTracker host** (`loadDeliPlayer`/`deliPlay`, puu016.s:53982-54708), because it already runs under the OS, already drives 46 Eagleplayers with subsongs, seek and UPS scopes, and is MIT; use UpRough's work as the binary set and arena data, not as the host, since its host owns the machine like a demo and carries the 33-switch crash. This reverses Johan's stated choice for I869, so it is decision D1.

Net effect on the existing decision D11 ("native outranks nuade on Amiga for every format both cover"): reversed for the ~20 overlap formats, subject to M1. Reason: on Paula output both are Paula-direct, so the native advantage is tick cost only, while nuade adds AHI, cross-platform data and one code path.

## 4. Decisions for Johan (recommended first)

- **D1. Eagleplayer host.** (a) Port HippoPlayer's host (recommended): OS-friendly, scopes and controls exist, MIT; cost is porting puu016.s host code out of a 56k-line ISO-8859 file. (b) UpRough UADE glue (current I869 wording): 53 players vetted with arena sizes on 68000, but bare-metal (takes VBL/CIA, no OS), switch crash to fix (cold restart per switch is the known-safe fix), no pause/volume/fade API. (c) Both, per format: double maintenance.
- **D2. Overlap formats (FC, SoundMon, SidMon, JamCracker, Hippel, Delta, SoundFX, Oktalyzer, Sonic Arranger, Sonix, ...).** (a) nuade first where M1 passes, Hippo native as fallback (recommended): portability and AHI for free; risk: slower tick, float. (b) Keep D11, native first: lowest CPU on 68000, Amiga-exact; cost: no AHI for these formats, different data on Mac and Amiga.
- **D3. Paula-only formats with no portable player (AHX, TFMX, Art of Noise, OctaMED, Musicline, DeliTracker Custom).** (a) Paula only on the Amiga; AHI users get no owner for them for now (recommended for v1). (b) Write or upstream C players into nuade (AHX and TFMX have known C implementations elsewhere, UNVERIFIED licences): portable, large work. (c) A virtual-Paula layer that captures a player's register writes into `sel_voices` so any Paula poker reaches AHI: covers every HW player at once, but needs per-player patching of `$dff0xx` writes (HippoPlayer's `quadscopePoke` patches prove it is doable per player) and loses DMA-latch exactness.
- **D4. MOD on the Amiga.** (a) KPlayer on Paula (built, I219), nuade `protracker4` on AHI/paula14 (recommended until M2): two players for the largest format, pattern data already from `modwalk.c` for both. (b) nuade `protracker4` everywhere if M2 shows it within budget: one player. (c) PS3M for AHI on 020+: own mixer, heavier.
- **D5. S3M on the Amiga.** (a) PS3M on 68020+ (recommended): the design's "keep current owner" means openmpt, whose 68k viability is unproven. (b) openmpt class as decided: needs a measurement first.
- **D6. Third-party binaries to ship** (Aminet Eagleplayers, OctaMED player libraries, `mline.library`). (a) Ship after a per-binary licence check (recommended). (b) Ship only sources we have (Hippo own, nuade): drops OctaMED, Musicline and the Eagleplayer tail.

### Johan's answers (2026-11-01)

- D1: (a) port HippoPlayer's DeliTracker host. UpRough is the binary set and arena data, not the host.
- D2: (a) nuade first where M1 passes; HippoPlayer native is the fallback. D11 is reversed for the overlap formats, subject to M1.
- D4: (a) KPlayer on Paula, nuade `protracker4` on AHI/paula14 until M2.
- D5: (a) PS3M on 68020+ for S3M.
- D3 and D6: open. They wait for the I872 follow-up (per-Eagleplayer AHI verdict and the nuade AHI/Paula effort estimate are not yet in this doc).

## 5. Open questions only a rig measurement settles (phase 2)

All silent output, real song (`virgill-that_meme_again.mod` for MOD), rig 2 with `--ram 256`, one batch at the end.

- **M1 (I135). nuade tick cost.** Per nuade player for the formats in section 2: worst and mean tick time over 60 s of a real song, CIA timer delta around `rp_tick`, on `--a1200 --nofpu` (68020 14 MHz), 68030, 68060, and 68000 for the four most-played formats (protracker4, sonix, futurecomposer, soundmon). Pass: worst tick under 25% of a 50 Hz frame on 68020 without FPU. Record which players trap to float emulation.
- **M2. MOD: KPlayer vs nuade `protracker4` vs PS3M.** CPU per tick on 68020 and 68060, Paula output for KPlayer and nuade, AHI (Paula 8-bit and 14-bit AHI modes, 22050 Hz) for nuade-via-`sel_mixer` and PS3M. Also `sel_mixer` cost per 4-voice frame block alone.
- **M3. Eagleplayer host.** Same binaries (SonixMusicDriver, TFMX-Pro, JochenHippel-7V) on HippoPlayer's host and on the UpRough glue: host overhead per tick, chip RAM, and a 200-switch soak (UpRough fails at 33 today; HippoPlayer's host unknown).
- **M4. Accuracy.** Per overlap format: render of the Amiga output (UAE audio capture, or the paula-shadow register log for VOICE players) vs the UADE host reference render; report correlation and the first diverging tick for Hippo own, Eagleplayer and nuade.
- **M5. xmaplay060 on AHI, 68020/030.** A 16-channel and a 32-channel XM at 22050 Hz on the Paula AHI driver: does it sustain without buffer underrun, CPU share.
- **M6. Chip RAM per Hippo own player.** Peak MEMF_CHIP use per format with a large real module, to set beside `uade_arena_sizes.tsv`.
- **M7. AHI channel sink vs AHI stream** (only if D2 is (a)): CPU of nuade with AHI doing the mixing (AHI_SetSound/SetFreq/SetVol per voice) vs `sel_mixer` plus AHI stream, 68020, Paula AHI driver.
