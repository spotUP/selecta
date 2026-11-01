---
date: 2026-11-01
topic: Amiga replayer paths per format and output (HippoPlayer-68k native, Eagleplayer host, nuade, mixes)
tags: [amiga, replayer, per-tier-owners, hippoplayer, uade, eagleplayer, nuade, ahi, i871]
status: phase 1 (paper) done; I872 follow-up (sections 6-9) done; phase 2 rig measurements open (section 5)
---

# Amiga replayer paths (I871, phase 1)

Ledger rows I868, I869, I870, I871. Paper evaluation only: no code, no builds, no rig runs. Implements the "Per-tier owners" paragraphs of `thoughts/shared/plans/2026-10-22-architecture-design.md:279-281`. Everything marked UNVERIFIED was not read from source in this pass; everything else names the file and line it came from.

Sources read: `~/Code/HippoPlayer-68k/code/puu016.s` (player table and flags), `code/eagleplayers/bin/`, `code/playsid.library/`, `code/LICENSE.md`; `~/Code/UpRough/uprough-uade` branch `feature/uade-music-backend` (HEAD ba3f76910, 2026-09-21) `amiga/uade_formats.txt`, `amiga/uade_arena_sizes.tsv`, `thoughts/spot/uade_ledger.md` (read only via `git show` and the working tree, branch untouched); `third_party/nuade` (`include/nuade/voice.h`, `players/`); Selecta `include/selecta/replayer.h`, `include/selecta/voices.h`, `src/engine/engine.c:584-596,1603-1655`, `plugins/replayers/{kplayer,nuade}`, `plugins/outputs/`; research docs R2 (`2026-10-22_devilbox-api-and-uprough-uade.md` section B), R10 (`2026-10-23_hippoplayer-native-players.md`), the Modland audit (`2026-10-31-modland-format-audit.md`). Modland counts below come from a read-only query of `~/.config/selecta/library.db` (`songs`, source `modland`, folder under `pub/modules/`, 516219 rows), the same method as the audit.

## 1. Five facts that change the question

1. **Half of HippoPlayer's players are Eagleplayers.** `puu016.s` has 93 player entries (`p_*` labels, 38064-53654). 46 of them jump into HippoPlayer's own DeliTracker/Eagleplayer host (`deliPlay`, `deliEnd`, ... ; `loadDeliPlayer` at puu016.s:53982, `deliPlay` at :54708, `DTP_Interrupt` and `EP_Get_ModuleInfo` handling at :54488-54651) and load unmodified Aminet Eagleplayer binaries from `code/eagleplayers/bin/` (README: "collected from Aminet and have not been modified"). Examples: Sonic Arranger, Sonix (IFF-SMUS), TFMX and TFMX Pro (the `p_tfmx_`/`p_tfmxpro_` entries), Hippel 7V, Hippel ST, Digital Mugician 1 and 2, Rob Hubbard, Richard Joseph, MaxTrax, Quartet, Quartet ST, Symphonie, DeliTracker Custom. So for those formats "HippoPlayer vs UADE Eagleplayer" is the same class of 68k binary; the real comparison is **HippoPlayer's Eagleplayer host vs UpRough's Eagleplayer host**, not two players.
2. **The other 47 HippoPlayer entries are own players**, all hardware-exclusive Paula pokers except the mixing ones. Only these carry HippoPlayer's AHI flag `pf_ahi`: Hippel-COSO (:43638), DigiBooster Pro (:44279, AHI only), PS3M `p_multi` (:45199), sample player (:45984), MIDI stream, Ogg, VGM (:51959-52255). xmaplay060 names itself "FastTracker2 xmaplay060 AHI 32ch" but does not set `pf_ahi` (its `ahiupdate` slot is `p_NOP`, ~:53333); it opens AHI itself (`_LVOAHI_AllocAudioA` at `pl/xmaplay060.s:478`, `AHI_LoadSound`/`AHI_SetSound` at :589/:833). Every other HippoPlayer own player writes Paula registers and has no AHI path. **Correction (I872):** the earlier wording "every Eagleplayer on either host has no AHI path" was wrong: 6 of HippoPlayer's 44 Eagleplayer binaries and UADE's SonixMusicDriver are EaglePlayer 2 *amplifier* players that never touch Paula for notes but call the host's `ENPP_PokeAdr/Len/Per/Vol` and `ENPP_DMAMask`; a host that routes those calls to `sel_voices` gives them AHI without touching the binary (section 6).
3. **Selecta already has the integer voice path nuade needs.** `include/selecta/voices.h` is nuade's voice API with integer arguments ("no float crosses this boundary"); `plugins/replayers/nuade/voices_shim.c` converts nuade's float/double calls to it, and the adapter sets `SEL_RP_KIND_VOICE` and `SEL_RP_NEEDS_FLOAT` (`adapter.c:625,641`). The engine routes a VOICE replayer either to the output's own voice sink (Paula direct, no mixing) or to the software mixer (`engine.c:584,1654-1655`), which feeds AHI or paula14. So nuade on Paula output costs the tick (no mixing) plus, **correction (I872)**, a chip-RAM allocation and copy of the sample on every note trigger (`plugins/shared/paula/paula_voices.c:50-76`, called from `p_play` :108-122 and `p_queue`), which section 7 fixes first; nuade on AHI costs tick plus `sel_mixer`. No AHI *channel* sink (AHI_SetSound per voice) exists yet; `plugins/outputs/ahi` is a stream output (I124 open).
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
- D4: (a) KPlayer on Paula, nuade `protracker4` on AHI/paula14 until M2. Checked on Johan's question "doesnt kplayer support ahi for mods already?": no. `p_protracker` flags (puu016.s:38080) carry no `pf_ahi`, and the format check skips KPlayer when `ahi_use` is set or `ptmix` = 2 (puu016.s:33981-33984), so HippoPlayer plays MOD on AHI through PS3M (`p_multi`, which has `pf_ahi`). In Selecta KPlayer is `SEL_RP_KIND_HW` (adapter.c:321): it writes Paula itself, and `--output ahi` only hands Paula over to it. PS3M stays the M2 comparison for AHI.
- D5: (a) PS3M on 68020+ for S3M.
- D3 and D6: open; re-framed with the I872 evidence in section 9.

## 5. Open questions only a rig measurement settles (phase 2)

All silent output, real song (`virgill-that_meme_again.mod` for MOD), rig 2 with `--ram 256`, one batch at the end.

- **M1 (I135). nuade tick cost.** Per nuade player for the formats in section 2: worst and mean tick time over 60 s of a real song, CIA timer delta around `rp_tick`, on `--a1200 --nofpu` (68020 14 MHz), 68030, 68060, and 68000 for the four most-played formats (protracker4, sonix, futurecomposer, soundmon). Pass: worst tick under 25% of a 50 Hz frame on 68020 without FPU. Record which players trap to float emulation.
- **M2. MOD: KPlayer vs nuade `protracker4` vs PS3M.** CPU per tick on 68020 and 68060, Paula output for KPlayer and nuade, AHI (Paula 8-bit and 14-bit AHI modes, 22050 Hz) for nuade-via-`sel_mixer` and PS3M. Also `sel_mixer` cost per 4-voice frame block alone.
- **M3. Eagleplayer host.** Same binaries (SonixMusicDriver, TFMX-Pro, JochenHippel-7V) on HippoPlayer's host and on the UpRough glue: host overhead per tick, chip RAM, and a 200-switch soak (UpRough fails at 33 today; HippoPlayer's host unknown).
- **M4. Accuracy.** Per overlap format: render of the Amiga output (UAE audio capture, or the paula-shadow register log for VOICE players) vs the UADE host reference render; report correlation and the first diverging tick for Hippo own, Eagleplayer and nuade.
- **M5. xmaplay060 on AHI, 68020/030.** A 16-channel and a 32-channel XM at 22050 Hz on the Paula AHI driver: does it sustain without buffer underrun, CPU share.
- **M6. Chip RAM per Hippo own player.** Peak MEMF_CHIP use per format with a large real module, to set beside `uade_arena_sizes.tsv`.
- **M7. AHI channel sink vs AHI stream** (only if D2 is (a)): CPU of nuade with AHI doing the mixing (AHI_SetSound/SetFreq/SetVol per voice) vs `sel_mixer` plus AHI stream, 68020, Paula AHI driver.

## 6. Per-Eagleplayer AHI verdict (I872 deliverable 1)

### 6.1 The mechanism: EaglePlayer 2 amplifier players

DeliTracker players write Paula registers (`$dff0a0-$dff0d8`, DMACON `$dff096`) themselves; no host can turn those writes into AHI without trapping them. EaglePlayer 2 added a second kind, the **amplifier player**: it declares the `EP_InitAmplifier` tag and, instead of poking Paula, calls the host's jump table off `a5`: `ENPP_DMAMask` (start/stop voices), `ENPP_PokeAdr`, `ENPP_PokeLen`, `ENPP_PokePer`, `ENPP_PokeVol`, `ENPP_PokeCommand` (filter). EaglePlayer then hands those calls to the selected "amplifier" engine (Paula, or a mixing engine that can output through AHI), which is how EaglePlayer 2 plays such players on AHI.

HippoPlayer's host **implements the interface but routes it to Paula**: the jump table `eagleJumpTableStart` (puu016.s:55338-55404, first entry at offset -6, so PokeAdr = -252, PokeLen = -258, PokePer = -264, PokeVol = -270, DMAMask = -246) maps `funcENPP_PokeAdr/Len/Per/Vol` to writes at `$dff0a0+16*ch` plus scope bookkeeping (puu016.s:55493-55607), `funcENPP_DMAMask` to `$dff096` (:55467), and `funcENPP_Amplifier`/`ENPP_FindAmplifier` are empty `rts` (:55626, :55747). The host calls `EP_InitAmplifier` on load (puu016.s:54388), and has a special case loading InStereo 1.0 into chip RAM because "it is an amplifier eagleplayer" (:48986). So in a port of this host (D1), replacing the five `funcENPP_Poke*`/`DMAMask` bodies with `sel_voices` calls (`play`/`queue`/`set_period`/`set_volume`/`stop`) gives every amplifier player the same VOICE path nuade uses: Paula direct through the Paula sink, or AHI / paula14 through `sel_mixer`, with scopes from the voice state. Cost: about 100-150 lines of 68k glue in the ported host; samples stay where the player put them (chip or fast; the AHI path reads any RAM).

**How amplifier players were found.** Text strings do not show it (the calls are jump-table offsets). Each binary was scanned for `jsr d16(a5)` (`$4EAD`) with the five ENPP offsets above, and for `$00DFF0xx` absolute addresses (Paula writes). Method: python byte scan, run on `~/Code/HippoPlayer-68k/code/eagleplayers/bin/*` and on the UADE 3.05 player binaries in `~/Code/DEViLBOX/third-party/uade-3.05/players/` (same files UpRough vendors; the UpRough branch itself vendors only 3 binaries, `git ls-tree feature/uade-music-backend amiga/vendor/uade-3.05/players`). A crunched binary would hide both patterns; none of the hits or misses looked crunched, but that is UNVERIFIED per binary.

**HippoPlayer's own AHI players prove the other route.** `pl/Hippelcoso_ahi5.s` ("replayer for paula & AHI", :2) and `pl/bpsm_ahi0.S` are source rewrites: every Paula write goes to `setLen/setPeriod/setVol` helpers, DMACON is emulated (`setdma`, "$dff096 emulation", :542), and the helpers call `AHI_SetSound/SetFreq/SetVol` per channel (:377-517). That is the virtual-Paula idea done per player in source; for a binary-only Eagleplayer the same needs patching every `$dff0xx` write in the binary (or an MMU trap), per binary, with DMA-latch timing lost.

Verdict key: **yes** = amplifier player, AHI through the ported host with no binary change. **partial** = Paula poker, but the format reaches AHI through the nuade C player of the same format (verdict on the format, not the binary). **no** = Paula poker with no portable equivalent: AHI only by per-binary patching (D3 c) or a new C player (D3 b).

### 6.2 HippoPlayer Eagleplayer set (44 binaries, `code/eagleplayers/bin/`)

| Player binary | Format | Modland | AHI | How | Evidence (scan) |
|---|---|---|---|---|---|
| activision pro | Activision Pro | 26 | yes | amplifier | ENPP Adr/Len/Per/Vol/DMAMask/Amp; 27 `$dff0` refs (Paula fallback path too) |
| in stereo 1.0 | InStereo! 1.0 | 3 | yes | amplifier | ENPP Adr/Len/Per/Vol/DMAMask; puu016.s:48986 |
| in stereo 2.0 | InStereo! 2.0 | 6 | yes | amplifier | `$VER: InStereo 2.0 Amplifier module`; ENPP Poke* + DMAMask |
| mugician.amp | Digital Mugician 1 | 86 | yes | amplifier | ENPP Poke* + DMAMask + Amp; 3 `$dff0` refs |
| synth 4.0 | Synthesis 4.0 | 36 | yes | amplifier | ENPP Poke* + DMAMask |
| tim follin ii | Tim Follin II | UNVERIFIED (<20) | yes | amplifier | ENPP Poke* + DMAMask; 0 `$dff0` refs |
| aprosys | AProSys | 28 | partial | nuade `aprosys` | no ENPP; 39 `$dff0` |
| david whittaker | David Whittaker | 121 | partial | nuade `davidwhittaker` | no ENPP; 35 |
| earache | EarAche | 3 | partial | nuade `earache` | no ENPP; 32 |
| face the music | Face The Music | 95 | partial | nuade `facethemusic` | no ENPP; 35 |
| jason brooke | Jason Brooke | 18 | partial | nuade `jasonbrooke` | no ENPP; 40 |
| jason page | Jason Page | 64 | partial | nuade `jasonpage` | no ENPP; 29 |
| jeroen tel | Jeroen Tel | 8 | partial | nuade `jeroentel` | no ENPP; 19 |
| jochen hippel 7v | Hippel 7V | 6 | partial | nuade `hippel7v` | no ENPP; 47 |
| kris hatlelid | Kris Hatlelid | 18 | partial | nuade `krishatlelid` | no ENPP; 34 |
| mark cooksey | Mark Cooksey (+Old) | 8+7 | partial | nuade `markcooksey`, `markcooksey_old` | no ENPP; 41 |
| mugician ii | Digital Mugician 2 | 7 | partial | nuade `digmug` (dmu2, mug2) | no ENPP; 44 |
| musicmaker4 / musicmaker8 | MusicMaker V8 | 69 (+7 old) | partial | nuade `musicmaker` (mm4, mm8) | no ENPP; 3 / 5 `$dff0` (Paula access path UNVERIFIED) |
| quartet | Quartet | UNVERIFIED | partial | nuade `quartet` (qpa) | no ENPP; 51 |
| richard joseph / richard joseph player | Richard Joseph | 152 | partial | nuade `richardjoseph`, `richardjosephrj` | no ENPP; 19 / 24 |
| rob hubbard | Rob Hubbard | 14 | partial | nuade `robhubbard` | no ENPP; 24 |
| sonicarranger | Sonic Arranger | 251 | partial | nuade `sonicarranger` | no ENPP; 38 |
| sonix music driver | IFF-SMUS / Sonix | 10396 | partial | nuade `sonix`; or swap in UADE's SonixMusicDriver, which is an amplifier build (6.3) | no ENPP; 43 |
| soprol | Sound Programming Language | UNVERIFIED | partial | nuade `spl` | no ENPP; 27 |
| sound master | Sound Master | 16+19+9 | partial | nuade `soundmaster` | no ENPP; 18 |
| soundcontrol | SoundControl | 18 | partial | nuade `soundcontrol` | no ENPP; 75 |
| special fx | Special FX (+ST) | 31+36 | partial | nuade `specialfx` | no ENPP; 8 |
| steve turner | Steve Turner | UNVERIFIED | partial | nuade `steveturner` | no ENPP; 7 |
| synth pack | Synth Pack | 19 | partial | nuade `synthpack` | no ENPP; 47 |
| syntracker | SynTracker | 16 | partial | nuade `syntracker` | no ENPP; 44 |
| tme | Musical Enlightenment | 15 | partial | nuade `musicalenlightenment` | no ENPP; 25 |
| wally beben | Wally Beben | 24 | partial | nuade `wallybeben` | no ENPP; 28 |
| chiptracker | ChipTracker | 45 | no | none | no ENPP; 27 |
| eaglestone | StoneTracker | 64 | no | none | no ENPP; 0 `$dff0` (Paula access via a base register, UNVERIFIED) |
| jochen hippel st | Hippel ST | 21 | no (UNVERIFIED) | nuade `hippelhip` may cover ST, UNVERIFIED | no ENPP; 19 |
| maxtrax | MaxTrax | 82 | no | none | no ENPP; 3 `$dff0` |
| quartet st | Quartet ST | 859 | no | nuade `quartet` lists only `qpa`; ST variant UNVERIFIED | no ENPP; 25 |
| rob hubbard 2 | Rob Hubbard 2 | UNVERIFIED | no (UNVERIFIED) | nuade `robhubbard` coverage of RH2 UNVERIFIED | no ENPP; 0 `$dff0` |
| tcb tracker | TCB Tracker | 24 | no | none | no ENPP; 15 |
| TFMX | TFMX 1.5 | 736 (all TFMX) | no | none (Hippo own `tfmx.asm` is also Paula only) | no ENPP; 47 |
| TFMX Pro | TFMX Pro | (in 736) | no | none | no ENPP; 61 |
| delimidi | DeliMIDI (MIDI out) | UNVERIFIED | n/a, counted no | drives MIDI, no audio | 0 `$dff0` |

(42 table rows; `musicmaker4/8` and the two Richard Joseph binaries share a row, so 44 binaries: **6 yes, 28 partial, 10 no**.)

### 6.3 UpRough `keep` rows (53, `amiga/uade_formats.txt` on `feature/uade-music-backend`)

- **yes (1):** SonixMusicDriver (smus, snx, tiny; 10396 songs). UADE 3.05's binary is an amplifier build (ENPP Adr/Len/Per/Vol/DMAMask/Amp, 6 `$dff0` refs), unlike HippoPlayer's `sonix music driver`. In a ported host it is the Eagleplayer fallback for Sonix on AHI if nuade `sonix` fails M1.
- **partial (35), nuade player of the same format exists:** AM-Composer, AudioSculpture, BenDaglish, BenDaglish-SID, DeltaMusic1.3, DeltaMusic2.0, EarAche, Fred (`fred_np`), FutureComposer-BSI (`fcbsi`), FutureComposer1.3, FutureComposer1.4 (`futurecomposer`: fc, fc13, fc14, smod), FuturePlayer, GlueMon, InStereo, InStereo2.0 (UADE builds are Paula pokers, 28/29 `$dff0`, no ENPP), JamCracker, JochenHippel (`mcmd`; hip/sog via `hippelhip` UNVERIFIED), JochenHippel-7V, JochenHippel-CoSo (`hippel` = "Jochen Hippel COSO", hipc/soc), Medley, MMDC, Mugician, MugicianII (`digmug`), MusicAssembler, PumaTracker, QuadraComposer, SIDMon1.0, SIDMon2.0, SonicArranger, SoundMon2.0, SoundMon2.2 (`soundmon`), SUN-Tronic, Synth (`synthesis`), SynTracker, ZoundMonitor. Every one scanned: no ENPP call.
- **no (17):** AbyssHighestExperience (AHX 1390), ADPCM_mono (a sample format: trivial in C), ArtOfNoise-4V and -8V (211), custom (747; the module is 68k code: Amiga-only by nature), CustomMade (10; no nuade dir), DIGI-Booster (168), Jochen_Hippel_ST (21), MusiclineEditor (817), PreTracker (64), TFMX, TFMX-1.5-TFHD, TFMX-7V, TFMX-7V-TFHD, TFMX-Pro, TFMX-Pro-TFHD (736 together), YM-2149 (Atari ST chip; a C emulator class format, not Paula).

**Totals over 97 entries (44 + 53): 7 yes, 63 partial, 27 no.** Song-weighted, the "no" set that matters is AHX 1390, Quartet ST 859, Musicline 817, DeliTracker Custom 747, TFMX 736; everything else in "no" is under 250 songs.

## 7. nuade AHI and Paula output effort (I872 deliverable 2)

### 7.1 What exists today

| Piece | Where | State |
|---|---|---|
| Integer voice API | `include/selecta/voices.h:32-47` (`play`, `queue`, `stop`, `set_pitch_hz` Q8, `set_period`, `set_volume` Q16, `set_pan`, `is_playing`, `get_state`) | done; no float crosses it |
| nuade float to integer shim | `plugins/replayers/nuade/voices_shim.c:7-30,64-75` (`to_q8`, `to_q16`, `to_pan`) | done; converts per call, so every pitch/volume call does one double/float multiply (soft-float on a 68020 without FPU) |
| nuade adapter | `plugins/replayers/nuade/adapter.c:149-153` (`rp_start` takes `const sel_voices*`), `:212-214` (`rp_tick`), `:625` `SEL_RP_KIND_VOICE`, `:641` `SEL_RP_NEEDS_FLOAT` | done |
| Engine routing | `src/engine/engine.c:1602-1603,1654-1655` (direct to `voice_sink` when voice_count fits `voice_max`, else `sel_mixer_voices`), table at `:7881-7890` | done |
| Paula voice sink | `plugins/shared/paula/paula_voices.c` (256 lines; period from Q8 at :8-19, `p_play` :108) exported by `plugins/outputs/paula/paula_out.c:103-125` (`SEL_OUT_VOICES`) | done, but **copies the sample to chip RAM on every `play`/`queue`** (`copy_sample` :50-76: `mem_alloc(SEL_MEM_SAMPLE)` + memcpy or S16-to-S8 loop, freed on the next note) |
| Software mixer | `src/mixer/mixer.c` (141 lines, 16.16 integer, `uint64_t` divides in `sel_mixer_period_step` :23-27, one `sel_resample` call per output sample per voice :103) | done, portable C, no 68k path |
| paula14 output | `plugins/outputs/paula14/` (mixed path) | done |
| AHI output | `plugins/outputs/ahi/common/ahi_out.c` (stream output, stereo only :27), `amiga/ahi.c` | stream code exists; ledger I124 still open for the rig sentinel |
| AHI channel sink | none | missing |
| AmiGUS output | none in Selecta; HippoPlayer has `pl/amigus_proto.s` (484 lines) as a reference | missing |

### 7.2 What is missing, per output

**Paula direct (nuade, 4-voice formats).** Works end to end today. Missing for it to be a fair M1 candidate: (P1) chip-sample cache in the Paula sink: keep the chip copy keyed by `sel_sample.data`+length (or skip the copy when the replayer already allocated it as `SEL_MEM_SAMPLE`, which `voices.h:16` says it should) and free on song stop; about 60-100 lines, 0.5-1 day, host test: chip allocations per song equal distinct samples. (P2) float out of the tick for players M1 flags: shim conversions become integer fast paths when nuade passes values that are already integral (period-based players), and per-player fixed-point rewrites for the heavy ones; float-mentioning lines per player: `soundmon` 33, `sonicarranger` 29, `sonix` 8, `oktalyzer` 8, `soundfx` 7, `futurecomposer` 1, `jamcracker` 1, `protracker4`/`quartet`/`hippel` 0 (grep of `players/<p>/*.c`; hot-path use UNVERIFIED until I135). Size 50-300 lines per player, 0.5-2 days each, done upstream in nuade (MIT) so Mac and web keep one code path. (P3) 68000: no FPU and no fast multiply; the four most-played nuade formats need their M1 number on 68000 before claiming that tier.

**AHI (nuade, any voice count).** Two routes. (A1) `sel_mixer` + AHI stream: exists in code; cost is `sel_mixer` per sample on the 68k. Missing: an assembly inner loop for `sel_mixer_mix` (no interpolation and linear modes, 4-8 voices, 8-bit and 16-bit sources), about 200-300 lines of 68020 asm plus a C reference test that compares output bit-exactly, 2-3 days; the uint64 divide in `sel_mixer_period_step` runs per period change and should become a table or a 32-bit reciprocal (30 lines). (A2) AHI channel sink: a `sel_voices` implementation over `AHI_LoadSound` (one AHI sound per distinct sample, `AHIST_SAMPLE`), `AHI_SetSound/SetFreq/SetVol` in the AHI sound hook, voice state for scopes from the last calls; AHI does the mixing in its own optimised driver code. About 400-600 lines C, 3-5 days, plus rig work for sound-slot limits and sample registration on song switch. Same sink also serves the 7 amplifier Eagleplayers (section 6) and any future virtual-Paula player. Which route wins is M7.

**paula14.** Rides on A1 (it is a mixed output); no extra work beyond the A1 asm loop.

**AmiGUS.** Nothing exists. As a stream device it is one more mixed output (A1 path): 200-400 lines plus rig/hardware access, 2-4 days, and the hardware register interface is UNVERIFIED (use `pl/amigus_proto.s` as the reference). Whether its wavetable part can take `sel_voices` directly (a channel sink like A2) is UNVERIFIED.

**Risks.** Float in the tick on 68020 without FPU (soft-float traps or library calls, I135). Chip RAM: the Paula path needs every playing sample in chip (512 KB on an A500, 2 MB on A1200); the cache in P1 holds each sample once, versus native players that keep the whole module in chip. The AHI paths can keep samples in fast RAM. 68020 budget: M1 pass bar is 25% of a 50 Hz frame; `sel_mixer` at 22050 Hz with 4 voices is ~88k resample calls per second in C today, which is the reason for the asm loop or for A2.

## 8. Per-format winner (I872 deliverable 3)

Criteria (I869): accuracy, CPU on 68000/68020/68060, AHI, seek/subsong/position/scope data, licence. Accuracy verdicts are UNVERIFIED until M4; where the table names a winner on accuracy it is "reference-class player" by provenance, not measured. "AHI" on a stock Amiga also plays through Paula via AHI's paula.audio driver, so an AHI-only player still reaches Paula on 68020+.

| Format (Modland) | Paula winner | AHI winner | Why |
|---|---|---|---|
| Protracker MOD (80788) | **Hippo own** KPlayer (decided D4) | nuade `protracker4` via mixer, PS3M as M2 rival | KPlayer: 68000-capable asm, built (I219), pos/seek/sub/scope; nuade has 0 float lines here, so it is the cheapest nuade player for the AHI path. Hippo beats nuade on Paula on CPU and on being built; neither EP host is a candidate |
| FT2 XM (44293) | **Hippo own** xmaplay060 (060) | xmaplay060 (020+, own AHI code, xmaplay060.s:478) | only candidate on the Amiga |
| IT (25572), S3M (11188), FT1 (2038), MTM (790) | **Hippo own** PS3M (020+) | PS3M | only candidate (D5); 68000 has no owner |
| IFF-SMUS / Sonix (10396) | nuade `sonix` if M1 passes, else Hippo EP (Sonix binary on the ported host) | nuade `sonix`; fallback UADE SonixMusicDriver (amplifier) on the ported host | nuade gives cross-platform data and AHI; the UADE amplifier binary is a ready AHI fallback, HippoPlayer's own Sonix binary is not |
| OctaMED MMD0-3 (5134) | **Hippo own** `p_med` (needs OctaMED player libraries, D6) | none (D3) | only Amiga player with 5-8 channels; nuade has only `mmdc` |
| Soundtracker 15-instr (1945) | as MOD (KPlayer 15-instrument support UNVERIFIED; PS3M handles it) | as MOD | |
| AHX/THX (1390) | **Hippo own** `thx.asm` (68000, pos/seek/sub/scope) | none (D3) | Hippo beats the UADE AHX binary on data (seek, pos, Poke scopes) and needs no host |
| Quartet ST (859) | Hippo EP `quartet st` on the ported host | none unless nuade `quartet` handles ST (UNVERIFIED) | Paula poker, no ENPP |
| DigiBooster Pro (845) | DBM0 via AHI paula.audio (020+) | **Hippo own** DBM0 | only candidate |
| Musicline (817) | **Hippo own** `p_mline` (`mline.library`, D6) | none (D3) | only candidate besides the UADE binary, which UpRough marks "CANNOT WORK NATIVELY" |
| DeliTracker Custom (747) | Hippo EP host (the module is the player) | none by nature | Amiga-only |
| TFMX all (736) | **Hippo own** `tfmx.asm`/`tfmx7c.s` | none (D3) | Hippo own has pos/seek/sub/Poke scopes; Hippo EP and UADE TFMX binaries are Paula pokers with fewer controls |
| Tail < 500 covered by nuade (FC, SoundMon, SidMon, Delta, JamCracker, Hippel COSO/7V, SoundFX, Oktalyzer, Sonic Arranger, Mugician, Richard Joseph, Whittaker, ...) | nuade if M1 passes; Hippo own where it has one, else Hippo EP | nuade | D2 |
| Tail amplifier EPs (InStereo 1/2, Synthesis, Activision Pro, Mugician 1, Tim Follin II) | nuade (all six have nuade players) | nuade; amplifier EP as fallback | both paths reach AHI; nuade wins on data and portability |
| Tail with no nuade and no amplifier (ChipTracker, StoneTracker, MaxTrax, TCB, AoN, DigiBooster 1, PreTracker, CustomMade) | Hippo own (AoN 4/8, DigiBooster16, PreTracker via `pl/`) else Hippo EP | none (D3) | |

Where HippoPlayer beats both nuade and the Eagleplayers: MOD on Paula (KPlayer), XM, IT/S3M/MTM/FT1, OctaMED, AHX, TFMX, DigiBooster Pro, Musicline, Art of Noise, PreTracker. Reason in every case: either the only Amiga player for the format, or an asm player with patched seek/position/scope that a binary Eagleplayer lacks, at 68000 cost.

## 9. Plan (I872 deliverable 4)

### 9.1 Optimum owner per format and output

As section 8. Summary rule: Hippo own where it is the only or the asm-with-controls player (MOD-Paula, XM, PS3M family, OctaMED, AHX, TFMX, DBM, Musicline, AoN, PreTracker); nuade for every format it covers when M1 passes (both outputs); ported HippoPlayer host for the Eagleplayer remainder (Paula; AHI only for amplifier players); nothing on AHI for Paula-only formats in v1 (D3).

### 9.2 Ordered rows (ledger-ready; IDs are placeholders)

| ID | Scope | Test | Depends |
|---|---|---|---|
| N1 | Paula sink chip-sample cache (`paula_voices.c` `copy_sample`): one chip copy per distinct sample per song, freed at stop | host test on paula-shadow: chip `mem_alloc` count over a real song == distinct samples; output register log unchanged | - |
| N2 | M1 + M2 rig batch (I135): nuade tick cost per format on 68000/020-noFPU/030/060, KPlayer vs nuade vs PS3M on MOD | verdict file per player, `SEL_RP_NEEDS_FLOAT` set from it | N1 |
| N3 | Port HippoPlayer's DeliTracker host (`loadDeliPlayer`..`deliPlay`, ENPP table, puu016.s:53982-55800) as an Amiga `SEL_RP_KIND_HW` replayer plugin; Paula only; SonixMusicDriver, TFMX Pro, Hippel 7V first | rig: 3 songs play with scopes; 200-switch soak (M3) | D1 |
| N4 | `sel_mixer` 68020 asm inner loop + 32-bit period step; C reference compare | host bit-exact test vs C; rig CPU per 4-voice block (M2 part) | N2 |
| N5 | AHI output: close I124 (stream) on the rig; then M7 decides whether N5b (AHI channel sink, `sel_voices` over AHI_SetSound) is built | rig sentinel `ahi_bufs=`; M7 numbers | N4 |
| N6 | Amplifier EPs to VOICE: ported host's `funcENPP_Poke*`/`DMAMask` call `sel_voices` when the output is mixed, so the 7 amplifier players reach AHI/paula14 | rig: InStereo 2.0 and UADE SonixMusicDriver on `--output ahi`; scope data from voice state | N3, N5 |
| N7 | nuade float removal for the players N2 flags (likely `soundmon`, `sonicarranger`), upstream in nuade | per-player render equal on Mac before/after; rig tick under bar | N2 |
| N8 | Hippo own ports in owner order (I868): PS3M, xmaplay060, thx, tfmx/tfmx7c, `p_med`, DBM0, AoN, `p_mline` | per player: rig plays a real song, pos/seek work; one reachability test through the engine | N2 (for MOD/Sonix decisions only) |
| N9 | Licence pass for the binaries that remain after nuade coverage (D6): ~10 Eagleplayers (ChipTracker, StoneTracker, MaxTrax, TCB, Quartet ST, Hippel ST, Rob Hubbard 2, TFMX, TFMX Pro, SonixMusicDriver) + OctaMED libraries + `mline.library` + UADE `score` if used | written verdict per binary in thoughts/ | D6 |
| N10 | AmiGUS output (mixed path first) | rig/hardware | N4, hardware access |
| N11 | (only if D3 b) C players for AHX and TFMX into nuade | Mac render vs UADE reference; Amiga M1 | D3 |

### 9.3 Re-scoped I868 / I869 / I870 text

- **I868** (Hippo own ports): "Port the HippoPlayer own players that own a format outright on the Amiga: PS3M (IT/S3M/MTM/FT1, 020+), xmaplay060 (XM), thx (AHX), tfmx/tfmx7c (TFMX), `p_med` (OctaMED), DBM0 (DigiBooster Pro, AHI), AoN 4/8, `p_mline`, PreTracker, DigiBooster 1; KPlayer stays MOD on Paula (D4). For formats nuade covers, a Hippo own player is ported only if N2 shows nuade fails the M1 bar on that tier (D2)."
- **I869** (Eagleplayer host): "Port HippoPlayer's DeliTracker/EaglePlayer host (D1) as an Amiga HW replayer plugin; binary set from the UpRough allow-list and HippoPlayer's `eagleplayers/bin`, arena data from `uade_arena_sizes.tsv`. Owns only formats neither a Hippo own player nor nuade covers (section 8 tail + DeliTracker Custom + Quartet ST). Route `ENPP_Poke*` to `sel_voices` so the 7 amplifier players also play on AHI (N6)."
- **I870** (nuade on AHI with asm): "nuade is the default Amiga owner for every format it covers, Paula direct and AHI, gated by M1. Work: N1 chip-sample cache, N4 `sel_mixer` asm loop, N5 AHI (stream; channel sink if M7), N7 float removal per flagged player, then 68k asm for hot nuade functions the M1 profile names."

### 9.4 Open decisions D3 and D6 (recommended first)

- **D3. AHI for Paula-only formats with no portable player (AHX 1390, Quartet ST 859, Musicline 817, DeliTracker Custom 747, TFMX 736, OctaMED 5134, AoN 211).**
  (a) **Recommended:** v1 plays them on Paula only; AHI comes free only where section 6 found it (7 amplifier Eagleplayers via N6, ~100-150 lines). Decisive trade-off: zero per-player work now, at the cost that an AHI-only user (graphics card, no Paula use) gets no sound for these ~10k songs in v1; on a stock machine Paula still plays them.
  (b) Add C players to nuade for the two biggest (AHX, TFMX; OctaMED is larger work): AHI plus Mac/web parity for ~2.1k songs; each is a multi-week port and licences of existing C implementations are UNVERIFIED.
  (c) Virtual Paula per binary/source (HippoPlayer did it for 2 players by rewriting source, `pl/Hippelcoso_ahi5.s`): covers anything eventually, but each player is its own rewrite or binary patch, loses DMA-latch timing, and scales worst.
- **D6. Third-party binaries to ship.**
  (a) **Recommended:** ship after a per-binary licence check, scoped by section 6 to what remains after nuade coverage: about 10 Eagleplayers plus the OctaMED player libraries and `mline.library` (N9). Decisive trade-off: a few days of licence reading keeps OctaMED (5134 songs), Musicline (817), Quartet ST (859), TFMX via EP fallback and DeliTracker Custom on the Amiga; without it they have no Amiga owner.
  (b) Ship only sources we hold (HippoPlayer MIT, nuade MIT): no licence risk, but OctaMED and Musicline drop entirely and the EP tail shrinks to nothing (DeliTracker Custom needs no binary but needs the host).

### 9.5 Johan's answers (2026-11-01)

- **D6:** ship every third-party binary (Eagleplayers, OctaMED player libraries, `mline.library`, UADE binaries) as a Selecta plugin. N9 changes from a licence gate to a record of each binary's origin and licence text, shipped with the plugin.
- **D3:** open. Johan asked which players would need a port; the list went back to him.

### 9.6 D3 evidence: portable players outside nuade (2026-11-01)

Johan: "check the devilbox project it has c players for many of them nuade should have it too" and "openmpt covers many of them as well". A check of `~/Code/DEViLBOX` and `third_party/nuade` (file headers only; full table in the session scratchpad `d3-c-players.md`) shows the "no portable player" claim of sections 8 and 9.4 is wrong for 8 of the 15 D3 formats. The claim holds for nuade (only Rob Hubbard), not for DEViLBOX.

| Format | Portable player | Size, licence |
|---|---|---|
| AHX / THX | `hively-wasm/common/hvl_replay.c` (HivelyTracker replayer) | 2272 lines C, BSD-style |
| TFMX, all variants | `tfmx-wasm/lib/libtfmxaudiodecoder` | ~6200 lines C++, GPL-2+ (conflicts with nuade MIT; fine as a separate plugin under D6) |
| Art of Noise 4/8 | `artofnoise-wasm/src/aon_player.c` | 2376 lines C, MIT |
| PreTracker | `pretracker-wasm/src/pretracker/pretracker.c` | 3354 lines C, MIT |
| Musicline | `third-party/musicline_playback-main/musicline/*.cpp` | 7533 lines C++, licence UNVERIFIED |
| MaxTrax | `maxtrax-wasm/src/generated/maxtrax/maxtrax.c` | 10924 lines C transpiled from 68k, licence UNVERIFIED |
| Hippel ST | `hippelst-wasm/src/hippelst_play.c` | 3786 lines C transpiled, licence UNVERIFIED |
| Rob Hubbard 2 | `robhubbard-wasm/src/robhubbard_synth.c`; nuade has `robhubbard` and `robhubbardst` (MIT) | same variant UNVERIFIED |
| OctaMED MMD0-3, DigiBooster 1 (and Pro) | libopenmpt loaders (`third_party/libopenmpt`, BSD, C++), inside OpenMPT's mixer, not a Paula-style replayer | desktop yes; Amiga only on the 060 tier at most (C++, float mixer), UNVERIFIED |
| Quartet ST, ChipTracker, TCB Tracker, CustomMade | none; UADE only in DEViLBOX | |
| StoneTracker | `stonetracker-wasm/src/st_machine.c` runs the original 68k binary in a mini machine: emulation, not a port | |
| DeliTracker Custom | none by nature (the module is 68k code); see ledger I886 for a wrapper | |

Consequence for D3: option (b) is no longer "large work from scratch" for AHX, AoN, PreTracker, TFMX, Musicline, MaxTrax and Hippel ST; it is a port of an existing replayer into a nuade-style tick player (voices out, so AHI, scopes and pattern data come for free). Licence checks (N9) come first for the four UNVERIFIED ones. D3 is re-asked with this evidence.

### 9.7 Johan's answers to the re-asked D3 and the Custom wrapper (2026-11-01)

- **D3: port into tick players.** Each portable replayer of section 9.6 (AHX, Art of Noise, PreTracker, TFMX, Musicline, MaxTrax, Hippel ST; OctaMED and DigiBooster from the libopenmpt loaders) becomes a nuade-style tick player shipped as its own plugin, so AHI, scopes and pattern data work on Mac and Amiga. The N9 licence record comes first for Musicline, MaxTrax and Hippel ST.
- **DeliTracker Custom on the Mac: "uade plays them".** Selecta has no UADE engine today (`third_party` has only nuade; `src/platform/platform.c` only knows UADE file prefixes). The Mac path is a UADE plugin (68k CPU plus Paula emulation; DEViLBOX's `uade-wasm` and `musashi-host` are the reference), which also covers Quartet ST, ChipTracker, TCB Tracker and CustomMade. UADE is GPL-2 and ships as a separate plugin (D6).
- **DeliTracker Custom on the Amiga: AHI redirect in v1.** A virtual-Paula wrapper captures the module's `$dff0xx` writes into `sel_voices` (MMU write trap on 68030+, design by Opus); a 68020 without an MMU stays Paula only.
- **Per-format output choice (Johan 2026-11-01): "it should be user selectable per format on the amiga. ahi/paula."** On the Amiga, every format whose owner can drive both outputs (nuade tick players, the ported D3 players, Custom through the virtual-Paula wrapper, amplifier Eagleplayers) gets a user setting AHI or Paula, per format. The default follows the system output. A format whose owner is Paula only shows the setting disabled with the reason. No such per-format pref exists in Selecta today (grep of src, include, apps); it is new work, row I887.
- **Goal (Johan 2026-11-01): "the goal is to get all formats on the amiga to output trough both ahi and paula".** No format stays Paula only or AHI only; the I887 setting is never disabled in the end state. This supersedes the "Paula only for v1" option of D3 and widens the virtual-Paula wrapper from Custom to every Paula-poking player that has no tick-player port (Hippo own players, the 27 "no" Eagleplayers, Quartet ST, ChipTracker, TCB, CustomMade). The reverse direction needs work too: AHI-only owners (xmaplay060 on 020-040, PS3M on AHI) need a Paula route through `sel_mixer`'s paula14 output. Order of preference per format: (1) tick player (nuade or a D3 port), both outputs for free; (2) native player with its own Paula and AHI paths; (3) virtual-Paula wrapper for the rest. Open design point for the I888 design: how the wrapper reaches AHI on a 68020 without an MMU (per-player `$dff0xx` patching like HippoPlayer's `quadscopePoke`, or a 68k interpreter).

## 10. Dual output for every format (I888)

Goal (Johan 2026-11-01, 9.7): every format on the Amiga plays through both AHI and Paula, user selectable per format (I887). Preference per format: (1) tick player (nuade or a D3 port), both outputs for free through `sel_voices` / `sel_mixer`; (2) native player with its own Paula and AHI paths; (3) virtual-Paula wrapper for the rest. This section answers the four I888 questions. Code references are from `main` at 3f57f6d9.

### 10.1 What "Paula" and "AHI" mean per replayer kind

The engine already decides the path from the replayer kind (`include/selecta/engine.h:390-393`, `sel_engine_route`, `src/engine/engine.c:1573-1615` `follow_route`): VOICE kind with `voice_count <= voice_max` goes DIRECT to the output's voice sink (Paula hardware voices, `plugins/shared/paula/paula_voices.c`); every other song goes to `sel_mixer` and, on a voice-only output, the engine swaps to a PCM output via `pcm_output_for` (`ahi` when the machine lists it, else `paula14`; I532). So per kind:

| Kind | Paula | AHI |
|---|---|---|
| VOICE, 4 voices (nuade, D3 ports, amplifier EPs, wrapped pokers) | DIRECT: hardware voices | MIXER: `sel_mixer` then `ahi` (A1), later the AHI channel sink (A2, 7.2) |
| VOICE > 4 voices, RENDER (xmaplay060 render mode, PS3M render, openmpt, ym) | MIXER then `paula14` (14-bit PCM through Paula) | MIXER then `ahi` |
| HW (KPlayer, Hippo own asm players, EP host, Custom) | HANDOVER: the player pokes Paula itself | only through a second owner of the format, or the virtual-Paula wrapper (10.3), which turns it into VOICE kind |

Consequence: "Paula" for a software-mixed format (XM, IT, S3M, OctaMED 5-8 channels) is `paula14`, not four hardware voices. That is the only Paula route those formats can have.

### 10.2 Format x route table

Route key: **T** = tick player (VOICE kind), both outputs from the routing above. **N** = native player(s) with both paths (one owner per output, or a render mode into `paula14`). **W** = virtual-Paula wrapper for AHI, the unchanged binary for Paula. Cost: **S** under 2 days, **M** 2-5 days, **L** over 5 days.

| Format (songs) | Route | Paula owner | AHI owner | Work | Cost |
|---|---|---|---|---|---|
| Protracker MOD (80788), Soundtracker (1945) | N | KPlayer (HW) | nuade `protracker4` (T) | router picks owner by output (I888b) | S |
| FT2 XM (44293) | N | 060: xmaplay060 Paula code; 020-040: xmaplay060 render mode into `paula14` | xmaplay060 own AHI | render-to-buffer entry in the I868 port of `xmaplay060.s` (RENDER kind) | M |
| IT (25572), S3M (11188), FT1 (2038), MTM (790) | N | PS3M own Paula mixing (HippoPlayer's non-AHI path, `.renderPS3M`, puu016.s:28718) or render into `paula14` | PS3M AHI | expose both modes in the I868 PS3M port | M |
| IFF-SMUS / Sonix (10396) | T | nuade `sonix` | nuade `sonix` (fallback: UADE SonixMusicDriver amplifier on the EP host, N6) | none beyond I888a/b | S |
| OctaMED MMD0-3 (5134), DigiBooster 1 (168) and Pro (845) | T | libopenmpt loader into a tick player (D3); Hippo `p_med` stays as HW alternative | same tick player | D3 port | L |
| AHX/THX (1390) | T | hvl_replay port; Hippo `thx.asm` stays as HW alternative | hvl tick player | D3 port, 2272 lines C | M |
| TFMX all (736; 9 EP entries) | T | libtfmxaudiodecoder port; Hippo `tfmx.asm` HW alternative | same | D3 port, GPL plugin | L |
| Musicline (817) | T | musicline_playback port (licence first, N9) | same | D3 port | L |
| Art of Noise 4V/8V (211) | T | aon_player port | same | D3 port | M |
| PreTracker (64) | T | pretracker port | same | D3 port | M |
| MaxTrax (82) | T | maxtrax transpiled C port (licence first) | same | D3 port, 10.9k lines | L |
| Hippel ST (21; 2 EP entries) | T | hippelst port (licence first) | same | D3 port | M |
| Rob Hubbard 2 | T | nuade `robhubbard`/`robhubbardst` (variant coverage UNVERIFIED; fallback W) | same | check, else W table | S |
| YM-2149 | T (RENDER) | `ym` plugin into `paula14` | `ym` into `ahi` | none | S |
| ADPCM_mono | T | trivial C RENDER | same | small C | S |
| Every nuade-covered EP format (63 "partial" + 7 "yes" entries, section 6) | T | nuade player | nuade player; amplifier EPs also via N6 | none beyond I888a/b; M1 gates the CPU tier | S |
| DeliTracker Custom (747) | W | the module itself on the EP host (HW) | wrapper: per-module patch table on 68020, MMU trap on 68030+ | wrapper core once, then tables | L once, S per batch |
| Quartet ST (859) | W | EP `quartet st` (HW) | wrapper, patch table for the binary | table | S |
| ChipTracker (45), StoneTracker (64), TCB Tracker (24), CustomMade (10) | W | EP binary (HW) | wrapper, one table per binary | table each | S each |
| DeliMIDI | n/a | MIDI out, no audio | - | - | - |

**Counts.** Over the 97 Eagleplayer entries of section 6: T 90 (all 70 yes/partial, plus 20 of the 27 "no": AHX, ADPCM, AoN x2, DIGI-Booster, Hippel ST x2, Musicline, PreTracker, TFMX x8, YM, MaxTrax, Rob Hubbard 2), W 6 (custom, CustomMade, ChipTracker, StoneTracker, Quartet ST, TCB), n/a 1 (DeliMIDI). The section 8 rows outside the EP lists: N 7 (MOD, Soundtracker, XM, IT, S3M, FT1, MTM), T 2 (OctaMED, DigiBooster Pro). W is about 1750 songs (Quartet ST 859, Custom 747, the rest 143); everything else reaches both outputs without a 68k trap.

### 10.3 Virtual-Paula wrapper

**Where it plugs in.** The wrapper is a replayer-side layer, not an output: it turns Paula register writes into `sel_voices` calls (`include/selecta/voices.h`: `play`, `queue`, `stop`, `set_period`, `set_volume`), so the song is VOICE kind and the existing routing (10.1) sends it to `sel_mixer` then `ahi` (or, later, the AHI channel sink A2). It wraps the I869 Eagleplayer host (the port of HippoPlayer's DeliTracker host, puu016.s:53982-55800, row N3), which also runs Custom modules. When the per-format choice is Paula, the wrapper is not used: the unchanged binary runs as HW kind with HANDOVER, exactly as today.

**Core (portable C, host-testable).** A shadow register file for AUD0-3 (`LCH/LCL`, `LEN`, `PER`, `VOL`, `DAT`) plus `DMACON`, `INTENA`, `INTREQ`, `ADKCON`, with Paula semantics:
- `DMACON` write with bit 15 set and a channel bit rising: latch LC/LEN, `play()` a `sel_sample` built over that chip address (S8, length = LEN*2, the sample memory is read live, so no copy), then raise the channel's audio interrupt request (Paula raises AUDx IRQ when it latches).
- `LC`/`LEN` written while the channel's DMA runs: the next latch, so `queue()` a sample at the new address (loop / repeat part, the common "set repeat after start" idiom).
- `PER`/`VOL` writes: `set_period` / `set_volume` (Q16 = vol * 1024), applied in write order.
- `DMACON` clear of a channel bit: `stop()`.
- `INTENA`/`INTREQ` audio bits (7-10): kept virtual; never reach the real chip. When a voice reaches its sample end the backend calls a new end hook (below), the core sets the virtual `INTREQ` bit and, if the virtual `INTENA` bit is set, calls the player's audio interrupt handler, which the wrapper captured by patching `exec.SetIntVector` / `AddIntServer` for `INTB_AUD0-3` while the player's init runs.
- `ADKCON` modulation bits (AM/FM between channels): logged, played unmodulated; a player that sets them is flagged in its table (none known; UNVERIFIED).
- Non-audio registers (`VHPOSR`, `DMACONR`, `INTREQR`, colours, CIA): pass through to the real hardware, so DMA-wait loops still work and only cost time.

New API piece: `sel_voices` has no sample-end notification. Add an append-only `set_end_hook(voices, voice, fn, ctx)` (append rule in `voices.h:44`), implemented in `sel_mixer`'s voice backend (`src/mixer/mixer.c`, `src/engine/voices.c`) and, for completeness, the Paula sink. With AHI the hook fires at mixer-block granularity (up to one block late; UNVERIFIED whether any W player cares); with the later A2 sink it comes from AHI's `SoundFunc`, which AHI calls exactly at sound boundaries.

**Capture on a 68020 without MMU: patch tables plus line-A traps, no interpreter.**
1. Offline, on the Mac, a trace tool runs the player binary (or the Custom module) on `musashi-host` (`~/Code/DEViLBOX/musashi-host/src/amiga_host.c`, 572 lines, the same machine `eagle_runner.c` and `stonetracker-wasm/src/st_machine.c` use) through whole songs and logs every access to `$DFF000-$DFF1FF`: PC as hunk + offset, register, size, instruction form. Dynamic tracing catches base-register (`lea $dff000,a6` then `move.w d0,$a8(a6)`) and computed addresses that a byte scan for `$00DFF0xx` misses. The tool emits a patch table (`.vpt`) keyed by binary hash, or by module md5 for Custom (Modland's 747 Custom songs are a closed set).
2. On the Amiga, after `LoadSeg` and before the player's init, the wrapper applies the table: at each traced site it replaces the opcode word with a line-A word (`$Axxx`, index into the site table); instruction length is unchanged, the extension words stay. A site that writes an AUDx data register through an absolute long address is instead retargeted (the 32-bit operand rewritten to the shadow file), so the hot LC/LEN/PER/VOL writes cost nothing extra. Then `CacheClearU()` (68020 instruction cache).
3. The line-A handler (vector at VBR + `$28`) looks up the site, decodes the original instruction from the table (only the forms the tracer accepted: `move`, `clr`, `or`/`and` to the custom base; anything else rejects the binary at trace time), performs the access on the shadow core or, for non-audio registers, on the real chip, and returns past the instruction.
4. Verification is offline too: the Mac run of the patched binary on musashi-host, with the line-A handler emulated, must give the same voice event stream as the unpatched binary's Paula register log. A table ships only when that matches over every traced song.

Cost on a 14 MHz 68020: a 4-channel tick writes about 20-40 custom registers; with absolute AUDx writes retargeted, roughly 5-20 trap per tick remain (DMACON, INTENA, INTREQ, base-relative writes). At about 150-250 cycles per line-A trap plus lookup, that is about 2-5k cycles per tick, 0.1-0.25 M cycles per second at 50 Hz, about 1-2% of the CPU (UNVERIFIED; measure on the rig with the trap counter). The `sel_mixer` cost on the AHI side (7.2 A1) dominates, as for every VOICE player.

Limit: a binary or module with no table (an unknown Custom file) cannot be redirected on a 68020 without an MMU; there the I887 setting shows Paula only, with the reason "no patch table for this module".

**Rejected for 68020: a 68k interpreter.** Musashi (C) on a 68k host costs roughly 50-150 host cycles per emulated instruction (UNVERIFIED); a player tick of 2-10k instructions becomes 0.1-1.5 M cycles per tick, 5-75 M cycles per second at 50 Hz, which is more than the whole 14 MHz 68020 at the low end. Musashi stays the Mac path (UADE plugin, 9.7) and the offline trace tool above.

**Capture on 68030/040/060 with an MMU: write trap, no table needed.** While the wrapper runs the player (its tick, its init, and its audio interrupt handler), the page holding `$DFF000` is marked invalid through `mmu.library` (Thor Richter; ships as a third-party plugin dependency under D6); the access fault handler decodes the faulting access, feeds the same core, passes non-audio registers through, and resumes. Outside the player's call the page is valid again (two ATC flushes per tick). Other interrupts that touch custom registers during the window fault too and are passed through (cost only). Cost: 20-40 faults per tick at about 300-600 cycles each on a 25 MHz 68030, about 0.3-1.2 M cycles per second, 1-5% (UNVERIFIED). Patch tables still win where they exist (cheaper, no `mmu.library`), so the MMU path is the fallback for modules without a table. 68EC020/EC030/EC040 count as "no MMU".

**Existing code it reuses.** `sel_voices` and `sel_mixer` (VOICE path), `follow_route` / `pcm_output_for` (output swap), the I869 EP host and its ENPP table (N6 amplifier glue calls the same core, so amplifier players and pokers share one Paula-to-voices translation), HippoPlayer's `Hippelcoso_ahi5.s` (`setdma`, the DMACON emulation, :542) as the behavioural reference, and musashi-host for tracing. HippoPlayer's `pf_quadscopePoke` (36 players, puu016.s:1743) feeds only scope data (`ns_start/ns_length/ns_period/ns_tempvol`, :465-470) and lacks the repeat LC/LEN and DMA order, so it is not a sound source.

### 10.4 Per-format setting: storage and router (I887)

Reuse, no new mechanism:
- **Storage:** one prefs row `SEL_PREF(AMIGA_FORMAT_OUTPUT, "amiga.format_output", "", SEL_PREF_PLAIN)` in `include/selecta/prefs.def`, value `"Protracker=paula,Fasttracker 2=ahi"`. Keys are Modland format names, the same names `SEL_RP_FORMATS` and `src/library/formats_table.c` use. A missing key means "follow the system output" (today's behaviour). Saved in `prefs.conf` by `sel_prefs_save` like every other pref; set from the UI through the existing prefs set command.
- **Parser:** the engine already parses `"k=v,k=v"` for `rp.config` (`rp_list_find`, `src/engine/engine.c:1912`). Move it to a shared helper that writes into a caller buffer and use it for both; no second parser.
- **Format of a song:** the library's format column when the song comes from the library, else `sel_format_by_ext` (`src/library/source.h:66`) on the extension or the Amiga prefix (the same prefix rule `sel_registry_claims_name` uses).
- **Router:** two existing functions take the wanted output. `find_replayer` (`engine.c:1803`) keeps its priority order but skips a candidate whose kind cannot serve the wanted output (10.1: AHI needs VOICE or RENDER; Paula takes any kind). This is how MOD picks KPlayer on Paula and nuade `protracker4` on AHI. In `start_subsong` (`engine.c:1615`) a wanted AHI passes `has_sink = 0` to `sel_engine_route` so a 4-voice song goes to the mixer, and `pcm_output_for` (`engine.c:1532`) takes the wanted output first in its `k_pcm` order (Paula wanted: `paula14`; AHI wanted: `ahi`). `switch_output` and `home_output` stay as they are.
- **Sentinel:** a `format_output_applied` counter in the engine stats (`include/selecta/engine.h:506`, next to `rp_config_applied`), bumped when the pref changed the owner or the output.
- **UI:** MUI/AGA/RTG settings page lists the formats the machine's registry claims, each with System / Paula / AHI. A format with no candidate for one output shows that choice disabled with the reason (end state: none, see I888q).

### 10.5 Ordered rows (ledger-ready)

| ID | Scope | Reachability test (entry point, sentinel) | Depends |
|---|---|---|---|
| I888a | Pref `amiga.format_output`; shared k=v helper replacing `rp_list_find`; song format name (library column, else `sel_format_by_ext`/prefix); `format_output_applied` stat | `tests/engine/pcm_route_test.c` new arg `pref`: AUDIO `paula,ahi`, RENDER song, pref `Protracker=paula`: switch callback gets `paula14`, not `ahi`; stat == 1; rp.config test still passes | - |
| I888b | Router: `find_replayer` filters by wanted output; `has_sink = 0` for AHI; `pcm_output_for` order from the want | `pcm_route_test` arg `owner`: two fake replayers claim Protracker (HW pri 2, VOICE pri 1): pref ahi opens the VOICE one (open-call count) on `ahi`; pref paula opens the HW one and the handover callback runs | I888a |
| I888c | MUI/AGA/RTG settings rows per format (System/Paula/AHI, disabled with reason), written through the prefs set command | uimodel test: selecting AHI for Protracker sets the pref string; rig check at batch end (Amiga) | I888a |
| I888d | `sel_voices` append-only `set_end_hook`; mixer voice backend and Paula sink call it at sample end; ABI golden updated | voices_test output: one-shot sample fires the hook once, looping sample once per wrap; `tests/abi/sel_voices.*` golden | - |
| I888e | Virtual-Paula core in C (shadow regs, DMACON latch to `play`, LC/LEN rewrite to `queue`, PER/VOL, virtual INTENA/INTREQ, audio IRQ from the end hook) | host test: a recorded Paula register log of a real song in, voice event stream out, compared with musashi-host's Paula state per tick | I888d |
| I888f | Mac trace tool on musashi-host: run binary + module, log `$DFF000-$DFF1FF` accesses by hunk/offset, reject unsupported forms and self-modifying code, write `.vpt` | run on the HippoPlayer `chiptracker` binary with a real song: table non-empty, every site classified; verify mode (patched vs unpatched event stream) equal | I888e |
| I888g | Amiga trap layer: apply `.vpt` after LoadSeg, line-A handler at VBR+$28, absolute AUDx retarget, `CacheClearU` | rig (Amiga, 68020 no MMU config): ChipTracker on AHI, `vp_traps` per tick sentinel > 0, CPU per tick logged | I888e, I888f, N3 |
| I888h | Wrapper in the EP host plugin: VOICE kind when AHI is wanted and a table (or MMU) is available, else HW; capture `SetIntVector`/`AddIntServer` audio handlers; ENPP amplifier calls (N6) go through the same core | top level: Selecta on the rig with pref `ChipTracker=ahi` plays the song on `ahi`, `format_output_applied` 1, `vp_events` > 0; same song with `paula` runs HW (handover count) | I888b, I888g |
| I888i | Patch tables for the EP set: Quartet ST, ChipTracker, StoneTracker, TCB, CustomMade (Rob Hubbard 2 if nuade misses it) | per table: I888f verify mode equal over every Modland song of the format; one rig song per table on AHI at batch end | I888f |
| I888j | Patch tables for DeliTracker Custom, keyed by module md5, in batches of ~100 modules | verify mode per module; batch report of rejected modules (they stay Paula only on 68020) | I888f |
| I888k | MMU path (030+): `mmu.library` dependency, page invalid during player calls, fault handler into the core; used when no table exists | rig 030 and 060 configs: an unknown Custom module plays on AHI, `vp_faults` per tick sentinel, CPU logged | I888h |
| I888l | MOD/Soundtracker both ways: KPlayer on Paula, nuade `protracker4` on AHI | rig: one real MOD with pref paula then ahi; owner name in status | I888b |
| I888m | PS3M family (IT/S3M/FT1/MTM) Paula route: PS3M's own Paula mixing or render into `paula14`, chosen by the want (part of the I868 PS3M port) | rig: one IT with pref paula plays with output `paula14` (or PS3M Paula mode), one with ahi | I888b, I868 PS3M |
| I888n | xmaplay060 render mode for 020-040 Paula (RENDER kind into `paula14`) | rig 020 config: one XM with pref paula plays through `paula14` | I888b, I868 xmaplay060 |
| I888o | D3 tick-player ports as separate rows if not in the ledger: AHX (M), AoN (M), PreTracker (M), Hippel ST (M), TFMX (L), Musicline (L), MaxTrax (L), OctaMED + DigiBooster via libopenmpt loaders (L) | per port: Mac render vs reference; rig song on both outputs | N9 licences, I885 |
| I888p | YM and ADPCM_mono on both outputs (RENDER into `paula14` / `ahi`) | rig: one YM with pref paula, one with ahi | I888b |
| I888q | Coverage invariant: host test walks every format the registry claims (plus the W tables) and fails on any format with no owner for Paula or for AHI; the known-gap list in the test shrinks to empty as rows land | host test counts from the registry, not a grep | I888b |

### 10.6 Answers (Johan 2026-11-01)

1. Unknown DeliTracker Custom module on a 68020 without MMU: no module stays unknown. Prescan every Custom module in the Modland set on the Mac (I888f trace tool) and cache its patch table; the set is small. A module with no table after the prescan is a bug in the prescan, not a Paula-only case. Row I888j covers the full set, not batches with rejects left over.
2. Ship `mmu.library` with Selecta for the 030+ fallback: yes. Record its origin and licence per D6 before the first release that bundles it (row I888k).
3. "Paula" for software-mixed formats (XM, IT, S3M, OctaMED 5-8 channels, YM) means 14-bit `paula14` output: confirmed.
4. XM on the Amiga stays on HippoPlayer's XM player (`xmaplay060.s`, 8bitbubsy's FT2 port) for both outputs: its own AHI on 020+, its Paula code on 060, its render mode into `paula14` on 020-040 (row I888n). Johan: "far superior", "it supports ahi as well". Never route Amiga XM through nuade or libopenmpt.
