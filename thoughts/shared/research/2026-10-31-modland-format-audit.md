---
date: 2026-11-01
topic: Modland format audit: which Modland format folders Selecta cannot play
status: done (60 of 60 formats tested)
---

# Modland format audit (I855)

Method: live library (`~/.config/selecta/library.db`, read-only), source `modland`, format = folder under `pub/modules/`. Top 60 folders by song count. One sample per folder (smallest file), downloaded from the built-in mirror and played with `selecta-cli play <file> --output null --seconds 1` (20 s timeout). Verdict `plays` = frames > 0.

Songs in library counted from the `songs` table (516219 Modland rows in total).

| Format | Songs in library | Sample file | Verdict | Error line |
|---|---|---|---|---|
| Protracker | 80788 | `pub/modules/Protracker/0men/gr@@ve-snu$.mod` | plays |  |
| HVSC | 61157 | `pub/modules/HVSC/DEMOS/0-9/10_Orbyte.sid` | no_replayer | error no_replayer |
| Fasttracker 2 | 44293 | `pub/modules/Fasttracker 2/12F/ahasverus.xm` | plays |  |
| Nintendo SPC | 36903 | `pub/modules/Nintendo SPC/4-Mat/Micro Machines/01 - main theme.spc` | plays |  |
| Nintendo DS Sound Format | 31118 | `pub/modules/Nintendo DS Sound Format/Akari Kaida/Mega Man Battle Network 5 - Double Team/01 theme of rockman exe 5 ds.mini2sf` | no_replayer | error no_replayer |
| Impulsetracker | 25572 | `pub/modules/Impulsetracker/006/sweat piano.it` | plays |  |
| Gameboy Sound Format | 23775 | `pub/modules/Gameboy Sound Format/Abe Hideki/Puyo Pop Fever/puyo pop - 01 title.minigsf` | no_replayer | error no_replayer |
| Spectrum | 22689 | `pub/modules/Spectrum/ASC Sound Master/3A Soft/breed!.asc` | no_replayer | error no_replayer |
| Playstation Sound Format | 16840 | `pub/modules/Playstation Sound Format/Akari Kaida/coop-Yoshino Aoki/Breath Of Fire 3/101 opening.psf` | no_replayer | error no_replayer |
| Video Game Music | 14172 | `pub/modules/Video Game Music/BBC Micro/Christopher Hyde/Zany Kong Junior/zany kong junior - gamestart.vgz` | open_failed | error open_failed |
| FM sound driver (FMP) | 12802 | `pub/modules/FM sound driver (FMP)/100 yen/ANDO_PCM.PVI` | no_replayer | error no_replayer |
| Screamtracker 3 | 11188 | `pub/modules/Screamtracker 3/006/nur ein traum.s3m` | plays |  |
| IFF-SMUS | 10396 | `pub/modules/IFF-SMUS/Allister Brimble/ACE II/ACE II.smus` | plays |  |
| MDX | 7469 | `pub/modules/MDX/Ayako Mori/Ghosts'N Goblins (ARCADE)/Map Scroll.mdx` | no_replayer | error no_replayer |
| Playstation 2 Sound Format | 6969 | `pub/modules/Playstation 2 Sound Format/Aki Hata/Futakoi Demo/02.psf2` | no_replayer | error no_replayer |
| Ultra64 Sound Format | 6463 | `pub/modules/Ultra64 Sound Format/Aaron Seeler/Quake 2/quake2.usflib` | no_replayer | error no_replayer |
| SNDH | 5484 | `pub/modules/SNDH/2a03fox/how_my_snowman_became_an_amputee.sndh` | no_replayer | error no_replayer |
| Saturn Sound Format | 5156 | `pub/modules/Saturn Sound Format/Akinaga Yoshida/Sangokushi Eiketsuden/02 palace.ssf` | no_replayer | error no_replayer |
| S98 | 5112 | `pub/modules/S98/Cat's Pro/Nova - Miirareta Shitai/00.s98` | no_replayer | error no_replayer |
| Sidplayer | 5032 | `pub/modules/Sidplayer/A. Brown/abc monday night football.mus` | no_replayer | error no_replayer |
| Nintendo Sound Format | 5015 | `pub/modules/Nintendo Sound Format/3-108/new rally-x.nsf` | plays |  |
| YM | 4962 | `pub/modules/YM/4-Mat/beastbusters - 1.ym` | plays |  |
| Dreamcast Sound Format | 4925 | `pub/modules/Dreamcast Sound Format/Daisuke Ishiwatari/Guilty Gear X (Naomi)/ggx-66-00-01.minidsf` | no_replayer | error no_replayer |
| RealSID | 3540 | `pub/modules/RealSID/$4753 Softcopy/$4753 digi-mixes.rsid` | no_replayer | error no_replayer |
| Ad Lib | 3496 | `pub/modules/Ad Lib/ADL/Frank Klepacki/dune ii - 00.adl` | no_replayer | error no_replayer |
| Slight Atari Player | 3230 | `pub/modules/Slight Atari Player/505/agent005.sap` | plays |  |
| Euphony | 3088 | `pub/modules/Euphony/Achiha Wataru/KAZENI.eup` | no_replayer | error no_replayer |
| Stereo Sidplayer | 2893 | `pub/modules/Stereo Sidplayer/ATOR/armies in the fire.mus` | no_replayer | error no_replayer |
| OctaMED MMD1 | 2728 | `pub/modules/OctaMED MMD1/A. Grandes/cherry coke.mmd1` | plays |  |
| MoonBlaster | 2512 | `pub/modules/MoonBlaster/A.C.E (J.A. Koets)/Ace In Space/Credits.MBM` | no_replayer | error no_replayer |
| Fasttracker | 2038 | `pub/modules/Fasttracker/4-Mat/bonus.mod` | plays |  |
| SGC | 2019 | `pub/modules/SGC/Aki Hata/Dynamite Headdy.sgc` | no_replayer | error no_replayer |
| Soundtracker | 1945 | `pub/modules/Soundtracker/AJ/push v2 by aj.mod` | plays |  |
| Deflemask | 1813 | `pub/modules/Deflemask/0xfroman/Product01.dmf` | no_replayer | error no_replayer |
| FamiTracker | 1784 | `pub/modules/FamiTracker/80something/Don't forget your phaser.ftm` | no_replayer | error no_replayer |
| Piston Collage | 1783 | `pub/modules/Piston Collage/0-Sin/obj1472-1.ptcop` | no_replayer | error no_replayer |
| SC68 | 1775 | `pub/modules/SC68/4-Mat/beast busters.sc68` | no_replayer | error no_replayer |
| PMD | 1510 | `pub/modules/PMD/L. Bird/G-Fight/bgm1.m` | no_replayer | error no_replayer |
| OctaMED MMD0 | 1441 | `pub/modules/OctaMED MMD0/A. C. Jones/a.c.'s ride.mmd0` | plays |  |
| YMST | 1396 | `pub/modules/YMST/4-Mat/beastbusters.ymst` | no_replayer | error no_replayer |
| AHX | 1390 | `pub/modules/AHX/451/aces high.ahx` | no_replayer | error no_replayer |
| MGSDRV | 1388 | `pub/modules/MGSDRV/Aidix/AD/Blast.MGS` | no_replayer | error no_replayer |
| FAC SoundTracker | 1310 | `pub/modules/FAC SoundTracker/A.C.E (J.A. Koets)/Jarredisk/Arpegiateur (1982).MUS` | no_replayer | error no_replayer |
| AY Emul | 1202 | `pub/modules/AY Emul/Agent X/agent-x nonamedzak 01.emul` | plays |  |
| Gameboy Sound System | 918 | `pub/modules/Gameboy Sound System/4-Mat/airforce delta.gbs` | plays |  |
| Quartet ST | 859 | `pub/modules/Quartet ST/607/Good Old Times.4v` | no_replayer | error no_replayer |
| Digibooster Pro | 845 | `pub/modules/Digibooster Pro/1541/blipp blopp.dbm` | plays |  |
| Musicline Editor | 817 | `pub/modules/Musicline Editor/ADE/cybernoid 2.ml` | no_replayer | error no_replayer |
| MuSICA | 808 | `pub/modules/MuSICA/Arita Masaki/ECHIKUSO - Menu.BGM` | no_replayer | error no_replayer |
| Multitracker | 790 | `pub/modules/Multitracker/4U4/spin.mtm` | plays |  |
| Delitracker Custom | 747 | `pub/modules/Delitracker Custom/Andre Bescond/g.nius intro.cus` | no_replayer | error no_replayer |
| TFMX | 736 | `pub/modules/TFMX/Axk/mdat.unicorn` | no_replayer | error no_replayer |
| OctaMED MMD3 | 660 | `pub/modules/OctaMED MMD3/ABC/straight-into-my-soul.mmd3` | plays |  |
| Super Nintendo Sound Format | 618 | `pub/modules/Super Nintendo Sound Format/Charles Deenen/Lord Of The Rings/lordofrings.snsflib` | no_replayer | error no_replayer |
| Psycle | 535 | `pub/modules/Psycle/3AtU/maikaferahoi.psy` | no_replayer | error no_replayer |
| SoundSmith | 484 | `pub/modules/SoundSmith/A.Fass/Coconut Champagne` | no_replayer | error no_replayer |
| Oktalyzer | 449 | `pub/modules/Oktalyzer/Absys/les granges brulees.okta` | plays |  |
| HES | 421 | `pub/modules/HES/4-57/final fantasy 1 main theme.hes` | plays |  |
| KSS | 393 | `pub/modules/KSS/Airborne/all that she wants.kss` | plays |  |
| OPLLDRV | 373 | `pub/modules/OPLLDRV/Amues.C/Water Dragon's Jewel/About her past.OPX` | no_replayer | error no_replayer |

Caveats: one sample per folder (smallest file), so a folder verdict is per sample, not per song. The CLI prints only the verdict token on failure, so the error column carries no detail. No `needs companion` line appeared in any run.

Totals: 60 formats tested. 21 play, 38 no_replayer, 1 open_failed, 0 download_failed, 0 crash/timeout.

## Failing formats by song count

Covering row from the ledger (I429-I434, I696-I710; I427 is named in I707 and sits outside the range). "no row" = no row in those ranges names the format. The `?` marks a match by extension only, not checked against the plugin.

| # | Format | Songs in library | Sample ext | Verdict | Covering row |
|---|---|---|---|---|---|
| 1 | HVSC | 61157 | sid | no_replayer | I434 (sidplayfp) |
| 2 | Nintendo DS Sound Format | 31118 | | no_replayer | no row |
| 3 | Gameboy Sound Format | 23775 | | no_replayer | no row |
| 4 | Spectrum | 22689 | asc | no_replayer | no row |
| 5 | Playstation Sound Format | 16840 | | no_replayer | no row |
| 6 | Video Game Music | 14172 | vgz | open_failed | I431 (libvgm); open_failed on .vgz, not no_replayer |
| 7 | FM sound driver (FMP) | 12802 | pvi | no_replayer | I700 (98fmplayer, FMP) |
| 8 | MDX | 7469 | mdx | no_replayer | I699 (mdxmini) |
| 9 | Playstation 2 Sound Format | 6969 | | no_replayer | no row |
| 10 | Ultra64 Sound Format | 6463 | | no_replayer | no row |
| 11 | SNDH | 5484 | sndh | no_replayer | no row (I414 is the source row only) |
| 12 | Saturn Sound Format | 5156 | | no_replayer | no row |
| 13 | S98 | 5112 | s98 | no_replayer | no row (I431 names vgm/vgz only) |
| 14 | Sidplayer | 5032 | mus | no_replayer | I434? (C64, .mus is not SID; unverified) |
| 15 | Dreamcast Sound Format | 4925 | | no_replayer | no row |
| 16 | RealSID | 3540 | rsid | no_replayer | I434 (sidplayfp) |
| 17 | Ad Lib | 3496 | adl | no_replayer | I696 (AdPlug) |
| 18 | Euphony | 3088 | eup | no_replayer | I701? (.eup, EUPPlayer; unverified) |
| 19 | Stereo Sidplayer | 2893 | mus | no_replayer | I434? (same as Sidplayer; unverified) |
| 20 | MoonBlaster | 2512 | mbm | no_replayer | no row |
| 21 | SGC | 2019 | sgc | no_replayer | no row |
| 22 | Deflemask | 1813 | dmf | no_replayer | no row (I430 is Furnace, a different tracker) |
| 23 | FamiTracker | 1784 | ftm | no_replayer | no row |
| 24 | Piston Collage | 1783 | ptcop | no_replayer | I702 (libpxtone, .ptcop) |
| 25 | SC68 | 1775 | sc68 | no_replayer | I427 (outside the requested range; I707 names it) |
| 26 | PMD | 1510 | m | no_replayer | I700 (98fmplayer, PMD) |
| 27 | YMST | 1396 | ymst | no_replayer | no row |
| 28 | AHX | 1390 | ahx | no_replayer | no row |
| 29 | MGSDRV | 1388 | mgs | no_replayer | no row |
| 30 | FAC SoundTracker | 1310 | mus | no_replayer | no row |
| 31 | Quartet ST | 859 | 4v | no_replayer | no row |
| 32 | Musicline Editor | 817 | ml | no_replayer | no row |
| 33 | MuSICA | 808 | bgm | no_replayer | no row |
| 34 | Delitracker Custom | 747 | cus | no_replayer | no row |
| 35 | TFMX | 736 | unicorn | no_replayer | no row |
| 36 | Super Nintendo Sound Format | 618 | snsf | no_replayer | I705 (snsf9x) |
| 37 | Psycle | 535 | psy | no_replayer | no row |
| 38 | SoundSmith | 484 | | no_replayer | no row |
| 39 | OPLLDRV | 373 | opx | no_replayer | no row (nearest I696, unverified) |

Summary: 26 failing folders have no row, 13 have one (of which 3 are unverified by extension, and SC68 is outside the range). Plays: 21 folders: Protracker, Fasttracker 2, Nintendo SPC, Impulsetracker, Screamtracker 3, IFF-SMUS, Nintendo Sound Format, YM, Slight Atari Player, OctaMED MMD0, OctaMED MMD1, OctaMED MMD3, Fasttracker, Soundtracker, AY Emul, Gameboy Sound System, Digibooster Pro, Multitracker, Oktalyzer, HES, KSS. Plays are per sample.
