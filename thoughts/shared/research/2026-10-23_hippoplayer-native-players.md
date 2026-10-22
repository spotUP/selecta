---
date: 2026-10-23
topic: HippoPlayer-68k native 68k players inventory and per-tier Amiga owners
tags: [hippoplayer, amiga, replayer, per-tier-owners, r10]
status: complete
---

Checkout: /Users/spot/Code/HippoPlayer-68k/code (paths below relative to it). Ledger row R10, implements the "Per-tier owners" paragraph of the architecture design (2026-10-22-architecture-design.md:276). Budget-limited: per-player output paths were read from file headers and register use, not from full disassembly; unconfirmed items are marked UNVERIFIED.

## 1. Summary

- `pl/` holds 42 directory entries: 33 player sources, 3 variant or unbuilt sources, 2 AmigUS support files, and 4 build/data entries (Makefile, compress_im, compress_shr, p61a_fix, bin/ with the two prebuilt binaries p61a and pretracker.bin, .gitignore). Details in section 2.
- Makefile (pl/Makefile (line ~5, approximate)-12, 104-170) assembles every player with `-m68000` except six built with `-m68020`: Aon8, dbpro (DigiBooster Pro), digi (DigiBooster 1.x), ps3m, sampleplay, xmaplay060. Everything else is a plain 68000 Paula player.
- The main HippoPlayer source is `puu016.s` (not `pl/`). It gates CPU in three places: `p_digiboosterpro` (puu016.s:44305), `p_aon8` (:45081), `p_multi`/PS3M (:45266), the sample player (:39366) and `p_xmaplay` (:53419) each refuse with `ier_hardware` when `AFB_68020` is not set. `p_digibooster` (:44090) has no gate: DigiBooster16.s detects the CPU itself and picks a 000 or 020 path (DigiBooster16.s:14-15, 362-380).
- Most native players write Paula directly (4 voices, 8-bit samples, period/volume registers). Software mixing happens only in: PS3M (14-bit stereo, 020+), xmaplay060 (8-bit or 14-bit, 32 channels), Aon8 (8 voices mixed to Paula), DigiBooster16 (8 channel mix routine), sampleplayer6 (WAV/AIFF/MP3 decode, 8-bit or 14-bit), DBM0 (DigiBooster Pro, via AHI only).
- Conflict with the fixed decision, for the record only: the HippoPlayer release notes say xmaplay "doesn't require 68060 anymore" (dist/hippoplayerupdate.readme:158-161), and the code gate is 68020 (puu016.s:53419), but the Paula path is documented as "needs a 68060" for speed (dist/hippoplayerupdate.doc:201, readme:170) and xmaplay060.s:10 says 020 is "WAY too slow". The decision (xmaplay060 owns XM, 68060 only on Paula) stands as a performance tier, not a hard CPU gate. With AHI or AmiGUS output xmaplay is listed as runnable on slower CPUs (readme:171-175: "AHI dependent", AmiGUS "68000"). This is recorded under open questions, not reopened.

## 2. Per-file table

Output path key: PAULA = writes the custom-chip voices ($dff0a0 block) directly, 4 voices, 8-bit; MIX = software mix into a buffer played by Paula; AHI = ahi.device. CPU is the Makefile build flag plus any gate in puu016.s. No FPU instruction was found in any `pl/` source (grep of fmove/fadd/fmul/fdiv/fsub on the non-space-named files returned 0; the files that errored on the byte-oriented grep were re-checked with LC_ALL=C and showed no FPU use; UNVERIFIED for xmaplay060.s and Hippo_PS3M3.s, which were not scanned instruction by instruction, though their headers describe integer mixing). Sizes are bytes.

| File | Format(s) | CPU / FPU | Output path | Size | Evidence |
|---|---|---|---|---|---|
| aon4channel.s | Art of Noise 4 channel | 68000, no FPU | PAULA (reads $dff0a0 block) | 62336 | aon4channel.s:156-158; Makefile (line ~105, approximate) |
| aon8_player8ch_18feb19953.s | Art of Noise 8 channel | 68020 build and gate, no FPU | MIX 8 voices into buffers at a Paula-period mix rate (period 135, about 26 kHz) played on Paula voices; sample width UNVERIFIED (8-bit expected) | 78699 | aon8:114-124; Makefile (line ~106, approximate); puu016.s:45081 |
| beathoven.s | Beathoven Synthesizer | not built by Makefile, 68000 code, no FPU | PAULA (UNVERIFIED, source not read past header) | 6013 | beathoven.s header; absent from Makefile TARGETS |
| Benn Daglishv3.s | Ben Daglish (incl. SID variant files) | 68000 | PAULA (UNVERIFIED) | 21383 | Makefile (line ~100, approximate) (bin/bendaglish) |
| bpsm.S | BP SoundMon 2 and 3 (2.x via bpsm) | 68000 | PAULA | 24118 | Makefile (line ~111, approximate); scope structs in file head |
| bpsm_ahi0.S | BP SoundMon with AHI experiment | unbuilt variant, 68020 by AHI use (UNVERIFIED) | AHI | 26847 | bpsm_ahi0.S:6-41; not in Makefile |
| CustomMade_v1.s | CustomMade (Ron Klaren) | 68000 | PAULA (UNVERIFIED) | 22394 | Makefile (line ~109, approximate) |
| Dave Lowe_v3.asm | Dave Lowe, Dave Lowe New | 68000 | PAULA (UNVERIFIED) | 22772 | Makefile (line ~101, approximate) |
| DBM0player_v2.16.s | DigiBooster Pro (DBM0, 2.16) | 68020 build and gate | AHI only (opens ahi.device, 16-bit sample mode flag Mode16BitEn, error text if AHI missing); no Paula path found | 79408 | DBM0:666-694; Makefile (line ~111, approximate) area (bin/dbpro); puu016.s:44305 |
| Delta1.0.s | Delta Music 1.0 | 68000 | PAULA (UNVERIFIED) | 14389 | Makefile (line ~113, approximate) |
| DigiBooster16.s | DigiBooster 1.0 to 1.6, 4 and 8 channel, packed and unpacked | 68000 path and 68020 path chosen at run time by `oldcpu`; built `-m68020`; no puu016 gate | MIX (8 channels cost "0.25 frame" on a stock A1200); output mode UNVERIFIED | 65110 | DigiBooster16.s:6-20, 362-380; Makefile (line ~112, approximate) |
| fc10.s | Future Composer 1.0 to 1.3 | 68000 | PAULA (UNVERIFIED) | 25183 | Makefile (line ~114, approximate) |
| FC14.s | Future Composer 1.4 | 68000 | PAULA ($dff0a0, 4 voices) | 20451 | FC14.s:314-345 |
| future_player.s | Future Player (Paul van der Valk) | 68000 | PAULA (UNVERIFIED) | 32515 | Makefile (line ~117, approximate) |
| GameMusicCreator_v1.0.s | Game Music Creator | 68000 | PAULA (UNVERIFIED) | 18753 | Makefile (line ~118, approximate) |
| gluemon13.s | GlueMon | 68000 | PAULA (UNVERIFIED) | 17293 | Makefile (line ~119, approximate) |
| Hippelcoso.s | Hippel / Hippel-COSO, older Paula-only version | superseded, unbuilt | PAULA | 24427 | Makefile builds Hippelcoso_ahi5.s as hippelcoso |
| Hippelcoso_ahi5.s | Hippel / Hippel-COSO, Paula and AHI | 68000 build; AHI path needs the AHI CPU class (UNVERIFIED) | PAULA or AHI chosen by `use ahi` argument | 37725 | Hippelcoso_ahi5.s:2-41; Makefile (line ~120, approximate) |
| Hippelcoso_ahi5_test.s | Same, test harness | unbuilt | as above | 33274 | header lines 4-24 |
| Hippo_PS3M3.s | PS3M: MOD, FastTracker multichannel (FTM), MTM, S3M, XM (rejected here), IT, plus old Soundtracker | 68020 build and gate, MC68020 macro, no FPU found (UNVERIFIED) | MIX, 14-bit stereo with surround, 8-bit fallback; Paula, AHI and AmiGUS output | 184673 | Hippo_PS3M3.s:5, 46-48, 631-653; puu016.s:45266, 45679-45700 |
| imploder.s | Medley (Paul van der Valk, .mso) | 68000 | PAULA (UNVERIFIED) | 32360 | imploder.s:66 (Medley Imploder4 module) |
| okta.s | Oktalyzer 4 and 8 channel | 68000 | PAULA (8 channel Oktalyzer maps 8 to 4 Paula voices, UNVERIFIED) | 33451 | Makefile (line ~123, approximate) |
| Puma_Replay.S | PumaTracker | 68000 | PAULA ($DFF0A0 voice blocks) | 17647 | Puma_Replay.S:742 |
| sampleplayer6.s | WAV, AIFF, IFF sample files, MP1/MP2/MP3 (mpega decode) | 68020 build and gate, no FPU found | MIX/decode to Paula in 8-bit or 14-bit (calibrated or ordinary), plus AHI and AmiGUS | 161396 | sampleplayer6.s:19-34; puu016.s:39366, 45968; Makefile (line ~125, approximate) |
| SidMon_v2.0.s | SidMon 1 and 2 | 68000 | PAULA (UNVERIFIED) | 17545 | Makefile (line ~127, approximate) |
| Sonic Arranger_v1.asm | Sonic Arranger | 68000, not in Makefile TARGETS (coverage doc lists it "external") | PAULA ($DFF0A0 loops, 4 voices) | 56712 | Sonic Arranger_v1.asm:541-615, 1604 |
| SoundFX_Optimised.s | SoundFX | 68000 | PAULA ($dff0a0) | 12671 | SoundFX_Optimised.s:163-273 |
| SoundMon3.s | BP SoundMon 3 | 68000 | PAULA (UNVERIFIED) | 21459 | Makefile (line ~128, approximate) |
| StarTrekker_v1.2_AM.s | StarTrekker / Audio Sculpture (AM) | 68000 | PAULA (UNVERIFIED) | 20085 | StarTrekker header:53 |
| tfmx.asm | TFMX (Chris Huelsbeck) | 68000 | PAULA (Paula base pointer, volume writes) | 41795 | tfmx.asm:948-1773 |
| tfmx7c.s | TFMX 7 voice ("7c") | 68000 | PAULA, 7 voices on 4 hardware voices by mixing UNVERIFIED | 43007 | tfmx7c.s:203 |
| thx.asm | AHX / THX (Dexter and Pink) | 68000, built as raw binary | PAULA (AHX synthesises waveforms into chip RAM voice buffers; dmacon write at thx.asm:396) | 70472 | thx.asm:14-19, 396 |
| vss.s | Voodoo Supreme Synthesizer | 68000 | PAULA (UNVERIFIED) | 29335 | vss.s:61 |
| xmaplay060.s | FastTracker 2 XM | built `-m68020`, code gate 68020 (AHI path), performance tier 68060 for Paula; no FPU instructions seen | MIX, 32 channels, 32-bit mix with linear interpolation, 8-bit or 14-bit output | 223501 | xmaplay060.s:3-29; puu016.s:53419; readme:158-175 |
| jamcrackerproreplay.asm | JamCracker Pro | 68000 | PAULA (UNVERIFIED) | 16313 | Makefile (line ~121, approximate) |
| amigus_proto.s / .i | AmiGUS mixer helper, not a format | 68000 | AmiGUS card | 11657 / 870 | amigus_proto.s:701-721 |
| Makefile, compress_im, compress_shr, .gitignore | build and Shrinkler scripts | n/a | n/a | 5804 / 93 / 128 / 36 | pl/Makefile |
| p61a_fix, bin/ | The Player 6.1A (binary fix data, no source here), prebuilt p61a and pretracker.bin | 68000 (UNVERIFIED) | PAULA | 6944 | Makefile (line ~76, approximate)-77, 217-219 |

Count: 42 entries covered. 3 sources are variants or old versions (Hippelcoso.s, Hippelcoso_ahi5_test.s, bpsm_ahi0.S). Beathoven and Sonic Arranger have sources but are not Makefile targets (they ship as eagleplayers or are built elsewhere; UNVERIFIED which).

## 3. Per-format tier owners

Selecta tiers: T0 = 68000/68010 OCS/ECS, T1 = 68020 (A1200 AGA, 14 MHz), T2 = 68030, T3 = 68040, T4 = 68060. Rows use the formats in 2026-10-22_format-coverage.md (grep'd by name; its owner column quotes lines). "nuade" means the player ported in nuade (nuade adapter, M1); "native" means the HippoPlayer-68k player in `pl/`. Proposal rule: where a native Paula player and nuade both exist, a native player is the Amiga owner for every tier (it runs on 68000 and is smaller), and nuade/openmpt/UADE remain the desktop or fallback owner; this is a proposal, the choice is the user's.

| Format (coverage doc row) | Coverage doc owner today | Proposed Amiga owner, per tier | Notes |
|---|---|---|---|
| Protracker, Soundtracker, ProTracker IFF, Soundtracker 2.6 (plain M.K. .mod) | openmpt class (M2) | Open decision D-M2-2. Candidates: PS3M (T1+, 14-bit, mixing, heavy) or a gap on T0. No pure-Paula MOD player exists in `pl/`; The Player 6.1A (p61a) only plays P61 converted files, not .mod. Medley (imploder.s) is not MOD. | Nearest 68000 candidate is The Player via a converter, not a direct player. Keep as open. |
| S3M, MTM, FastTracker multichannel (FTM), IT | openmpt class (M2) | PS3M T1+ (68020). T0: gap. | MOD, S3M, MTM keep current owners per fixed decision. IT is handled by `id_it` inside PS3M (puu016.s:45688), so PS3M is a candidate owner for IT on 68020+; coverage doc currently has openmpt class. Proposal only. |
| Fasttracker 2 / XM | openmpt class (M2) | T4 (68060): xmaplay060. T1, T2, T3: GAP (by user choice). T0: GAP. | Fixed decision. xmaplay060 with AHI or AmiGUS output is documented as lighter (see open questions). |
| DigiBooster 1.x (digi, 4 and 8 ch) | openmpt class (M2) | T0: DigiBooster16.s for 4 channel (UNVERIFIED that the 000 path runs at speed); T1+: DigiBooster16.s | Native player picks 000 or 020 path at run time. |
| DigiBooster Pro (DBM0) | openmpt class (M2) | T1+ with AHI: DBM0player (AHI only). T0: GAP. | Needs ahi.device, 68020 gate. |
| Oktalyzer | openmpt class (M2) | T0+: okta.s | 8 channel on 4 voices (UNVERIFIED). |
| SoundFX | openmpt class (M2) | T0+: SoundFX_Optimised.s | |
| AHX / THX | native UADE fallback (M4) | T0+: thx.asm (Paula) | Closes an M4 fallback row. Coverage row 89 marks nuade "no" for AHX. |
| Art of Noise 4ch | native UADE fallback (M4) | T0+: aon4channel.s | |
| Art of Noise 8ch | native UADE fallback (M4) | T1+: Aon8 (68020 gate). T0: GAP. | |
| TFMX, TFMX ST | native UADE fallback (M4) | T0+: tfmx.asm and tfmx7c.s (7 voice) | Pro and ST variants: which file takes which is UNVERIFIED. |
| Hippel ST COSO, Hippel ST | native UADE fallback (M4) | T0+: Hippelcoso_ahi5.s covers Hippel-COSO; whether it also plays the ST variant is UNVERIFIED (ST is listed "external" Jochen Hippel ST eagleplayer) | |
| Pretracker | native UADE fallback (M4) | T0+: prebuilt pretracker.bin (binary only, no source in `pl/`) | Cannot be rebuilt or reviewed from this checkout. |
| Hippel, Hippel COSO, Hippel 7V | nuade adapter (M1) | native Hippelcoso (Amiga, T0+) preferred; nuade fallback | 7V player is external; not in `pl/`. |
| BP SoundMon 1, 2, 3 | nuade adapter (M1) | native bpsm.S / SoundMon3.s, T0+ | |
| Future Composer 1.3, 1.4, BSI | nuade adapter (M1) | native fc10.s, FC14.s, T0+ | |
| SidMon 1, 2 | nuade adapter (M1) | native SidMon_v2.0.s, T0+ | |
| Delta Music, Delta Music 2 | nuade adapter (M1) | native Delta1.0.s (v1 only, UNVERIFIED for v2) | Delta Packer row: no native. |
| JamCracker | nuade adapter (M1) | native jamcrackerproreplay.asm, T0+ | |
| Startrekker AM, Audio Sculpture | nuade adapter (M1) | native StarTrekker_v1.2_AM.s, T0+ | Startrekker FLT8 stays openmpt class (not in `pl/`). |
| Dave Lowe, Dave Lowe New | nuade adapter (M1) | native Dave Lowe_v3.asm | |
| Ben Daglish, Ben Daglish SID | nuade adapter (M1) | native Benn Daglishv3.s | |
| Pumatracker | nuade adapter (M1) | native Puma_Replay.S | |
| GlueMon | nuade adapter (M1) | native gluemon13.s | |
| CustomMade | nuade adapter (M1) | native CustomMade_v1.s | |
| Future Player | nuade adapter (M1) | native future_player.s | |
| Voodoo Supreme Synthesizer | nuade adapter (M1) | native vss.s | |
| Medley | nuade adapter (M1) | native imploder.s | |
| Game Music Creator | nuade adapter (M1) | native GameMusicCreator_v1.0.s | |
| Sonic Arranger | nuade adapter (M1) | native Sonic Arranger_v1.asm | Not a Makefile target. |
| Beathoven Synthesizer | nuade adapter (M1) | native beathoven.s | Not a Makefile target. |
| Digital Mugician 1, 2, 3 | nuade adapter (M1) | no source in `pl/` (eagleplayer mugician / puu016 `digitalmugician2`); nuade owns | |
| MP3 / WAV / AIFF / IFF samples | stream decoder (M3) | T1+: sampleplayer6.s (68020 gate); T0: GAP for MP3 | Ogg and FLAC: no native player. |
| OctaMED MMD0-MMD3 | openmpt class (M2) | GAP in `pl/` (HippoPlayer MED is an internal player in puu016, not a separate file) | Unverified whether it is extractable. |

## 4. Gaps

- XM on 68000, 68020, 68030, 68040: gap by the user's decision. XM on 68060: xmaplay060.
- Plain M.K. .mod on T0 (68000): no pure-Paula MOD player in `pl/`; D-M2-2 is open. PS3M needs a 68020.
- S3M, MTM, IT on T0: no owner (PS3M is 68020+).
- DigiBooster Pro on T0 and without AHI: no owner.
- Art of Noise 8 channel on T0: no owner (Aon8 gated to 68020).
- MP3 decoding on T0: sample player is 68020+.
- OctaMED, Hippel 7V, Digital Mugician: no source file in `pl/`.
- Pretracker, The Player (P61A): binaries only in `pl/bin`; no source to wrap or audit.
- OCS versus AGA: no `pl/` file requires AGA; the 68020 players need only the CPU. UNVERIFIED for AHI-using players (they need AHI and a sound card or Paula driver, not AGA).

## 5. Open questions

1. D-M2-2: which player owns plain M.K. .mod on the Amiga, given the gap above.
2. Prefer native HippoPlayer players over nuade for the 20-odd formats both cover? (Proposal in section 3: native on Amiga, nuade on desktop.) User's call.
3. Should xmaplay060 with AHI or AmiGUS output be allowed on 68020 to 68040? The code gate is 68020 (puu016.s:53419) and the release notes list AHI as "AHI dependent" (readme:171). This would make XM on T1 to T3 not a gap when AHI exists. Contradicts nothing decided so far because the decision concerns Paula output, but the user must say.
4. PS3M handles IT and old Soundtracker (puu016.s:45688-45700, :34110-34120); keep openmpt as the only IT owner, or add PS3M on 68020+?
5. Licensing of HippoPlayer-68k players for redistribution: LICENSE.md in the checkout was not read (UNVERIFIED).
6. Wasm/desktop side unaffected: this file covers Amiga owners only.
