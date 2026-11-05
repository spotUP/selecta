# bbmp own-format audit (I710)

Date 2026-11-05. Source: github.com/creep-ltx/bbmp docs/replayers.md, csrc/, src/bbmp/backends/formats.py (read via gh api, no local clone). Extensions from the live library DB (Modland rows), because bbmp stores no extensions.

Verification: every covered verdict used two passes. Pass 1 = extension grep over plugins/replayers/*/adapter.c and third_party/nuade player tables. Pass 2 = probe/magic read. The evidence column names which pass was used; rows marked 'ext grep only' have no pass 2 (AdPlug sub-format claims, nuade bulk players) and stay provisional. Extension collisions (sng, mus, sm1, sm2, mad, dtm, mon, mod) were not accepted from the grep: the colliding plugins probe for other formats' magic, so those formats are uncovered.

## Covered by a Selecta plugin

| Format | Extensions | bbmp source | Licence | Selecta plugin | Evidence |
|---|---|---|---|---|---|
| AY Amadeus / Flash Tracker / Fuxoft AY Language | amad,fls,fxm | csrc/ay/fxm_engine.cpp, fls_engine.cpp | Ay_Emul credit + Ayumi MIT | bbmp_ay (I709a) | plugins/replayers/bbmp_ay/adapter.c (ext grep + probe) |
| AY STRC | strc | csrc/ay/stc_cr.cpp | GPL 3+; Ayumi MIT | bbmp_ay | plugins/replayers/bbmp_ay/adapter.c (ext grep + probe) |
| Vortex Tracker II | vt2,pt3 | csrc/ay (at3/vt2) | as ZXTune | zxtune | plugins/replayers/zxtune/adapter.c:150-151 (ext grep; content probe by ZXTune) |
| Capcom QSF | qsf,miniqsf | csrc/chip/qsf_engine | per replayers.md | qsf | plugins/replayers/qsf/adapter.c:373 (ext grep) |
| SNSF | snsf,minisnsf | bbmp-snsf helper | per replayers.md | snsf | plugins/replayers/snsf/adapter.c (ext grep) |
| SNDH / SC68 | sndh,sc68 | csrc/psgplay | GPL 2 only | sndh, psgplay | plugins/replayers/sndh/adapter.c:231; psgplay/adapter.c:218 (ext grep) |
| SunVox | sunvox | csrc (sunvox helper) | BSD engine (I706a) | sunvox, sunvox_lib | plugins/replayers/sunvox/adapter.c:167; sunvox_lib/adapter.c:298 (ext grep); I706 open rows |
| Organya / Organya 2 | org | csrc/chip/organya_impl | per replayers.md | organya | plugins/replayers/organya/adapter.c:313 (ext grep) |
| Piston Collage (+ Protected) | ptcop,pttune | bbmp-pxtone | per replayers.md | pxtone | plugins/replayers/pxtone/adapter.c (ext grep) |
| Euphony | eup,fmb,pmb | bbmp-chip | per replayers.md | eupmini | plugins/replayers/eupmini/adapter.c (ext grep); I701 open |
| FM sound driver (FMP) ovi/opi/ozi | ovi,opi,ozi | csrc/chip/fmp_engine | per replayers.md | 98fmplayer (partial: .pvi not) | plugins/replayers/98fmplayer/adapter.c:82 name_is_fmp, :106 rp_probe (ext grep + probe) |
| PMD | m,m2,mz | csrc/chip/pmdc_engine | GPL 3+; ymfm BSD 3 | 98fmplayer | plugins/replayers/98fmplayer/adapter.c:85 name_is_pmd, :106 (ext grep + probe: extension is part of the test) |
| MDX | mdx | bbmp-chip | per replayers.md | mdxmini | plugins/replayers/mdxmini/adapter.c:238 (ext grep) |
| Klystrack | kt | csrc/retrovert klystron | per replayers.md | klystron (untracked WIP, I708) | plugins/replayers/klystron/adapter.c:232 (ext grep); not yet committed |
| Ixalance | ixs | csrc/retrovert/ixs_engine.cpp | MIT | none (I708) | no ixs in any plugin |
| CHIPNSFX | chp | csrc/chipnsfx | GPL 3+ | chipnsfx | plugins/replayers/chipnsfx/adapter.c:194 (ext grep) |
| TIATracker | ttt | csrc/tiatracker | GPL 2 only | tiatracker | plugins/replayers/tiatracker/adapter.c:163 ext; :26-43 probe (JSON '{' + "channels") (ext grep + probe) |
| Furnace / Deflemask / FamiTracker / TFM Music Maker | fur,dmf,ftm,tfm,tfe | csrc/furnace | per replayers.md | furnace, zxtune(tfe) | plugins/replayers/furnace/adapter.c:27; zxtune/adapter.c:150 (ext grep) |
| Digital Sound Interface Kit | dsm | played as another (libopenmpt) | BSD | openmpt | third_party/libopenmpt/soundlib/Load_dsm.cpp:4,212,519 magic (ext grep + probe read) |
| Jesper Olsen | jo | csrc/amiga/jo_engine.cpp | GPL 3+ | nuade (jesperolsen) | third_party/nuade/players/jesperolsen/jesperolsen_nuade.c:14-16 jo_probe (ext grep + probe) |
| Zoundmonitor | sng,smp,ss | uade | as UADE | nuade (zoundmonitor) | third_party/nuade/players/zoundmonitor/zoundmonitor_nuade.c:15-17 zm_probe (ext grep + probe) |
| Archimedes Tracker / Coconizer / MegaTracker | musx,coc,mgt | bbmp-xmp | MTPng GPL for Mega | xmp | plugins/replayers/xmp/adapter.c:16,30 strstr type (ext grep + probe) |
| Pierre Adane / Synth Pack / PokeyNoise / SoundMon 1 / Digital Mugician 3 | various | uade | as UADE | nuade | third_party/nuade/players/{pierreadane,synthpack,pokeynoise,soundmon,digmug} (ext grep via nuade table) |
| EdLib D01, Ken's AdLib music | d01,ksm | bbmp d01 converter | LGPL (AdPlug) | adplug | plugins/replayers/adplug/adapter.c:25 AP_EXTENSIONS (ext grep only) |

## Not covered (one ledger row per engine)

| Row | Format | Extensions | bbmp source | Licence | Selecta plugin | Evidence |
|---|---|---|---|---|---|---|
| I710a | AXS | axs | csrc/axs/bbmp-axs.cpp | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710b | BeRoTracker | brt | csrc/berotracker | per replayers.md (BeRo) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710c | BoyScout | bsf | csrc/retro/boyscout_engine | library free to use | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710d | Buzz | bmx | csrc/pc/buzz_engine | public domain | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710e | Buzzic and Buzzic 2 | buz,buz2 | csrc/synth/buz_engine | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710f | Cubic Tiny XM | mxm | csrc/pc/mxm_engine.cpp | GPL 3+; libopenmpt BSD 3 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710g | Cybertracker C64 | ct | csrc/retro/cybertracker_engine | engine GPL 3+; tracker no terms (downloaded, not shipped) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710h | DarkWave Studio | dwp | csrc/darkwave | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710i | JayTrax | jxs | csrc/synth/jxs_engine | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710j | Ken's Digital Music | kdm,kwv | csrc/synth/kdm_engine | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710k | MaxTrax | mxtx | csrc/amiga/maxtrax_engine | GPL 3+ (ScummVM) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710l | Megadrive CYM | cym | bbmp-console/vgm family | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710m | MikMod UNITRK | uni | csrc/pc/unitrk_engine | LGPL 2.1 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710n | Musicline Editor | ml | csrc/musicline/mline_load.cpp, mline_play.cpp | LGPL (from mlineplayer102.asm) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710o | NerdTracker 2 | ned | csrc/retro/ned_engine | giftware, non-commercial only | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710p | Noise Tracker GS 1 | (no extension) | csrc/iigs/ntgs_cr | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710q | NoiseTrekker, NoiseTrekker 2, ProTrekkr, ProTrekkr 2.0 | ntk,ptk | csrc/synth/ptk_engine | BSD 2-clause | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710r | Onyx Music File | omf | csrc/retro/jangle_engine | GPL 3+; libopenmpt BSD 3 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710s | PiECE music driver | pmd | csrc/piece/piece.cpp | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710t | Picatune | smufi | csrc/retro/smufi_engine | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710u | Psycle | psy | csrc/psycle | GPL 2+ (some plugins third-party) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710v | SBStudio | pac | csrc/synth/sbstudio_engine | ISC | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710w | SGC and GBR | sgc,gbr | bbmp-nez (libnezplug) | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710x | Gameboy Tracker (mgb) | mgb | csrc/retro/gbt_engine | GPL 3+; GBSoundSystem MIT | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710y | SPU | spu | csrc/console/spu_engine | P.E.Op.S. via AOSDK, GPL 2 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710z | WonderSwan | wsr | csrc/console/wsr_engine | Mednafen WonderSwan, GPL 2 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710aa | Shroom | sho | csrc/retro/shroom_engine | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ab | Skale Tracker | skm | csrc/synth/skale_engine | BSD | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ac | Sound Club and Sound Club 2 | sn,sn2 | csrc/misc/sclub_cr.cpp | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ad | SoundSmith | w,(no extension) | csrc/iigs/ss_cr | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ae | Stonetracker | spm,sps | csrc/amiga/stonetracker_player.asm | archive redistributable with readme | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710af | TSS | tss,tsd | csrc/retro/tss_engine | BSD 3-clause | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ag | The 0ok Amazing Synth Tracker | t0ast | csrc/t0ast | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ah | VGM Music Maker | vge | csrc/retro/vgemm | own code | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ai | Vic-Tracker | vt | csrc/retro (victracker) | BSD tracker; dasm GPL 2 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710aj | Westwood SND | snd | csrc/retro (wwsnd) | GPL 3+ (after ScummVM) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ak | FAC SoundTracker | mus,sm1,sm2 | csrc/fac/fac_cr.cpp | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710al | GoatTracker and GoatTracker 2 | sng | csrc/misc/goattracker, gt1_engine, gt2_engine | GPL 2 | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710am | SCC-Musixx | sng | csrc/retro/smx_engine | GPL 3+; emu2212 MIT | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710an | Synder SNG-Player (+ Stereo) | sng | csrc/synder/bbmp-synder.cpp | GPL 3+ | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ao | Monotone | mon | csrc/pcmusic/monotone.cpp | Unlicense (public domain) | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ap | Digitrekker | dtm | csrc/pcmusic/digitrekker.cpp | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710aq | PlayerPro | mad | csrc/pc/playerpro_engine | public domain | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710ar | Pollytracker | mod | bbmp polly (played as C64 SID conversion) | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710as | Ad Lib helpers: EdLib Packed, FM Tracker, MUS (samurai), Pixel Painters FMF/FTF, Shadowlands, SIG2, SIS (lem), Vibrants | (no Modland rows) | csrc/adlib bbmp-adlib.cpp + cal/fmt/lem/mus/ppf/sig/vib_cr | GPL 3+ converters; AdPlug LGPL plays the result | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I710at | FMP .pvi | pvi | csrc/chip/fmp_engine | per replayers.md | none | ext grep over all adapter.c + nuade tables: no match; probe check where extension collides |
| I433 | ArkosTracker, Starkos | aks,sks | Arkos Tracker 3 loader/player | MIT (JUCE GPL) | none | no extension match in any plugin; existing row |
| I709b | Beepola | bbsong | csrc/ay/beepola_engine.cpp | GPL 3+ | none | no extension match in any plugin; existing row |
