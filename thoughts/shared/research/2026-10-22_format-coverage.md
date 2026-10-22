---
date: 2026-10-22
topic: Format coverage - union of bbmp and HippoPlayer formats with a Selecta plugin owner each
tags: [selecta, formats, plugins, bbmp, hippoplayer, nuade, milestones]
status: final
---

# Format coverage: bbmp + HippoPlayer union

Design doc sections 5 (Output plugins, DSP and routing) and 14 (Milestones). One row per format. Sources: bbmp `docs/formats.md` (438 formats, all rows, including the 22 it does not play), HippoPlayer `hippoplayerupdate.readme` "List of supported formats" plus `code/eagleplayers/bin` (external players), nuade `docs/players.md` (125 players, 137 UADE players ported).

Owner rules used to fill the table:
- bbmp backend `uade` and nuade ports the module: nuade adapter (M1). Not ported or skipped: native UADE fallback (M4); the two MOD packers nuade skips on purpose go to the openmpt class (M2).
- bbmp backend `openmpt`: openmpt class (M2), except where nuade has a native player (Pumatracker, Future Composer 1.3/1.4, Face The Music, Game Music Creator, Startrekker AM, Audio Sculpture): nuade adapter (M1).
- bbmp backend `gme`: gme class (M2). `sidplayfp`: sidplayfp class (M2).
- Every other bbmp backend (zxtune, adplug, xmp, vgm, kss, furnace, and the 30-odd one-format helpers in csrc/) has no Selecta class: none-yet.
- Hippo column: "internal" = in the HippoPlayer binary (listed in the readme, no file in eagleplayers/bin); "external" = an eagleplayer in eagleplayers/bin or a vgm2wav/mdx2wav/delimidi helper.

| Format | bbmp backend | HippoPlayer replayer | nuade player | Selecta plugin owner | Milestone |
|---|---|---|---|---|---|
| HVSC | sidplayfp | internal (playsid.library: PSID/2SID/3SID) | no | sidplayfp class (M2) | M2 |
| Spectrum - Pro Tracker 3 | zxtune | none | no | none-yet | none |
| Spectrum - Pro Tracker 2 | zxtune | none | no | none-yet | none |
| Musicline Editor | musicline | none | no | none-yet | none |
| FAC SoundTracker | fac | none | no | none-yet | none |
| Sound Club | soundclub | none | no | none-yet | none |
| SCC-Musixx | sccmusixx | none | no | none-yet | none |
| Spectrum - Fuxoft AY Language | fxm | none | no | none-yet | none |
| Synder SNG-Player Stereo | synder | none | no | none-yet | none |
| Synder SNG-Player | synder | none | no | none-yet | none |
| NerdTracker 2 | ned | none | no | none-yet | none |
| AXS | axs | none | no | none-yet | none |
| Gameboy Tracker | mgb | none | no | none-yet | none |
| Sound Club 2 | soundclub | none | no | none-yet | none |
| Picatune | picatune | none | no | none-yet | none |
| BeRoTracker | berotracker | none | no | none-yet | none |
| TSS | tss | none | no | none-yet | none |
| Buzzic 2 | buz | none | no | none-yet | none |
| Ad Lib - Pixel Painters FMF | pixelpainters | none | no | none-yet | none |
| Ad Lib - Pixel Painters FTF | pixelpainters | none | no | none-yet | none |
| The 0ok Amazing Synth Tracker | t0ast | none | no | none-yet | none |
| Digital Sound Interface Kit | dsm | none | no | none-yet | none |
| Ad Lib - Sound Images Generation 2 | sig | none | no | none-yet | none |
| Buzzic | buz | none | no | none-yet | none |
| Monotone | monotone | none | no | none-yet | none |
| Ad Lib - EdLib D01 | d01 | none | no | none-yet | none |
| Ad Lib - Sound Interface System | lem | none | no | none-yet | none |
| Ad Lib - FM Tracker | fmtracker | none | no | none-yet | none |
| Ad Lib - MUS | samurai | none | no | none-yet | none |
| Ad Lib - Shadowlands | shadowlands | none | no | none-yet | none |
| Ad Lib - Vibrants | vibrants | none | no | none-yet | none |
| Protracker | openmpt | internal (ProTracker / PS3M) | no | openmpt class (M2) | M2 |
| Fasttracker 2 | openmpt | internal (FastTracker 2) | no | openmpt class (M2) | M2 |
| Nintendo SPC | gme | external (SPC via vgm2wav) | no | gme class (M2) | M2 |
| Nintendo DS Sound Format | zxtune | none | no | none-yet | none |
| Impulsetracker | openmpt | internal (Impulse Tracker) | no | openmpt class (M2) | M2 |
| Gameboy Sound Format | zxtune | none | no | none-yet | none |
| Playstation Sound Format | zxtune | none | no | none-yet | none |
| FM sound driver (FMP) | fmp | none | no | none-yet | none |
| SNDH | sndh | none | no | none-yet | none |
| Screamtracker 3 | openmpt | internal (ScreamTracker 3) | no | openmpt class (M2) | M2 |
| Video Game Music - Sega Megadrive | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Slight Atari Player | zxtune | external (SAP via vgm2wav) | no | none-yet | none |
| Playstation 2 Sound Format | zxtune | none | no | none-yet | none |
| Spectrum - ST Song Compiler | zxtune | none | no | none-yet | none |
| S98 | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| Sidplayer | sidplayfp | none | no | sidplayfp class (M2) | M2 |
| Nintendo Sound Format | gme | external (NSF / NSFE via vgm2wav) | no | gme class (M2) | M2 |
| YM | ym | none | no | none-yet | none |
| Dreamcast Sound Format | zxtune | none | no | none-yet | none |
| AY Emul | gme | external (AY via vgm2wav) | no | gme class (M2) | M2 |
| RealSID | sidplayfp | none | no | sidplayfp class (M2) | M2 |
| Spectrum - ASC Sound Master | zxtune | none | no | none-yet | none |
| OctaMED MMD1 | openmpt | internal (MED / OctaMED) | no | openmpt class (M2) | M2 |
| Spectrum - Sound Tracker Pro | zxtune | none | no | none-yet | none |
| Video Game Music - Sega Master System | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Fasttracker | openmpt | internal (FastTracker multichannel) | no | openmpt class (M2) | M2 |
| MoonBlaster | kss | none | no | none-yet | none |
| Soundtracker | openmpt | internal (ProTracker / PS3M) | no | openmpt class (M2) | M2 |
| Deflemask | furnace | none | no | none-yet | none |
| FamiTracker | furnace | none | no | none-yet | none |
| Piston Collage | pxtone | none | no | none-yet | none |
| SC68 | sc68 | none | no | none-yet | none |
| Spectrum - SQ Tracker | zxtune | none | no | none-yet | none |
| PMD | pmdc | none | no | none-yet | none |
| Euphony | eup | none | no | none-yet | none |
| Stereo Sidplayer | sidplayfp | none | no | sidplayfp class (M2) | M2 |
| OctaMED MMD0 | openmpt | internal (MED / OctaMED) | no | openmpt class (M2) | M2 |
| AHX | uade | internal (THX/AHX) | no | native UADE fallback (Amiga, M4) | M4 |
| MGSDRV | kss | none | no | none-yet | none |
| YMST | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| Spectrum - Vortex | zxtune | none | no | none-yet | none |
| Gameboy Sound System | gme | external (GBS via vgm2wav) | no | gme class (M2) | M2 |
| Digibooster Pro | openmpt | internal (DIGI Booster / PRO) | no | openmpt class (M2) | M2 |
| MuSICA | kss | none | no | none-yet | none |
| Multitracker | openmpt | internal (MultiTracker / TakeTracker) | no | openmpt class (M2) | M2 |
| IFF-SMUS | uade | external (Sonix Music Driver) | yes | nuade adapter (M1) | M1 |
| OctaMED MMD3 | openmpt | internal (MED / OctaMED) | no | openmpt class (M2) | M2 |
| Video Game Music - Sega Game Gear | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Spectrum - Sound Tracker Pro 2 | zxtune | none | no | none-yet | none |
| Super Nintendo Sound Format | snsf | none | no | none-yet | none |
| Quartet ST | uade | external (Quartet / Quartet ST) | no | native UADE fallback (Amiga, M4) | M4 |
| Spectrum - Digital Studio | zxtune | none | no | none-yet | none |
| Ad Lib - Creative Music File | adplug | none | no | none-yet | none |
| Spectrum - Pro Sound Creator | zxtune | none | no | none-yet | none |
| Oktalyzer | openmpt | internal (Oktalyzer) | no | openmpt class (M2) | M2 |
| SGC | sgc | none | no | none-yet | none |
| OPLLDRV | kss | none | no | none-yet | none |
| TFMX | uade | external (TFMX / TFMX Pro) | no | native UADE fallback (Amiga, M4) | M4 |
| V2 | zxtune | none | no | none-yet | none |
| X-Tracker | openmpt | none | no | openmpt class (M2) | M2 |
| SoundFX | openmpt | internal (SoundFX) | no | openmpt class (M2) | M2 |
| Organya | organya | none | no | none-yet | none |
| Ad Lib - Raw OPL Capture | adplug | none | no | none-yet | none |
| Piston Collage Protected | pxtone | none | no | none-yet | none |
| OctaMED MMD2 | openmpt | internal (MED / OctaMED) | no | openmpt class (M2) | M2 |
| Capcom Q-Sound Format | qsf | none | no | none-yet | none |
| Ad Lib - LucasArts | adplug | none | no | none-yet | none |
| Ad Lib - AdLib Tracker 2 | adplug | none | no | none-yet | none |
| TFM Music Maker | furnace | none | no | none-yet | none |
| Megadrive GYM | gme | external (GYM via vgm2wav) | no | gme class (M2) | M2 |
| Ad Lib - Visual Composer | adplug | none | no | none-yet | none |
| SoundSmith | soundsmith | none | no | none-yet | none |
| Ad Lib - HSC AdLib Composer | adplug | none | no | none-yet | none |
| Screamtracker 2 | openmpt | none | no | openmpt class (M2) | M2 |
| BP SoundMon 2 | uade | internal (BP SoundMon v2/v3) | yes | nuade adapter (M1) | M1 |
| Art Of Noise | uade | internal (Art of Noise 4ch/8ch) | no | native UADE fallback (Amiga, M4) | M4 |
| Ad Lib - Apogee | adplug | none | no | none-yet | none |
| Ad Lib - Loudness Sound System | adplug | none | no | none-yet | none |
| Ad Lib - EdLib Packed | edlib | none | no | none-yet | none |
| Future Composer 1.3 | openmpt | internal (Future Composer v1.0-1.4) | yes | nuade adapter (M1) | M1 |
| Symphonie | openmpt | external (Symphonie Pro) | no | openmpt class (M2) | M2 |
| Composer 669 | openmpt | none | no | openmpt class (M2) | M2 |
| Digibooster | openmpt | internal (DIGI Booster / PRO) | no | openmpt class (M2) | M2 |
| Digital Symphony | openmpt | none | no | openmpt class (M2) | M2 |
| WonderSwan | wsr | none | no | none-yet | none |
| Spectrum - Pro Tracker 1 | zxtune | none | no | none-yet | none |
| Future Composer 1.4 | openmpt | internal (Future Composer v1.0-1.4) | yes | nuade adapter (M1) | M1 |
| Spectrum - CHIPNSFX | chipnsfx | none | no | none-yet | none |
| Ad Lib - AMusic | adplug | none | no | none-yet | none |
| Digital Sound And Music Interface | openmpt | none | no | openmpt class (M2) | M2 |
| Video Game Music - Sega 32X | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Music Editor | xmp | none | no | none-yet | none |
| Ad Lib - EdLib D00 | adplug | none | no | none-yet | none |
| BP SoundMon 3 | uade | internal (BP SoundMon v2/v3) | yes | nuade adapter (M1) | M1 |
| David Whittaker | uade | external (David Whittaker) | yes | nuade adapter (M1) | M1 |
| Digital Tracker DTM | openmpt | none | no | openmpt class (M2) | M2 |
| Octalyser | openmpt | internal (Oktalyzer) | no | openmpt class (M2) | M2 |
| SidMon 2 | uade | internal (Sidmon v1/v2) | yes | nuade adapter (M1) | M1 |
| Spectrum - Digital Music Maker | zxtune | none | no | none-yet | none |
| Beepola | beepola | none | no | none-yet | none |
| Ad Lib - Sierra | adplug | none | no | none-yet | none |
| His Master's Noise | openmpt | none | no | openmpt class (M2) | M2 |
| Polytracker | openmpt | none | no | openmpt class (M2) | M2 |
| Delta Music 2 | uade | internal (Delta Music v1/v2) | yes | nuade adapter (M1) | M1 |
| Hippel ST COSO | uade | external (Jochen Hippel ST) | no | native UADE fallback (Amiga, M4) | M4 |
| Face The Music | openmpt | external (Face The Music) | yes | nuade adapter (M1) | M1 |
| Spectrum - Flash Tracker | fls | none | no | none-yet | none |
| Ultratracker | openmpt | none | no | openmpt class (M2) | M2 |
| Epic Megagames MASI | openmpt | none | no | openmpt class (M2) | M2 |
| GoatTracker | goattracker | none | no | none-yet | none |
| MO3 | openmpt | none | no | openmpt class (M2) | M2 |
| Digital Mugician | uade | external (Mugician II / mugician.amp) | yes | nuade adapter (M1) | M1 |
| Spectrum - Fast Tracker | zxtune | none | no | none-yet | none |
| MaxTrax | maxtrax | external (MaxTrax) | no | none-yet | none |
| Richard Joseph | uade | external (Richard Joseph) | yes | nuade adapter (M1) | M1 |
| Ad Lib - Surprise! AdLib Tracker 2.0 | adplug | none | no | none-yet | none |
| Video Game Music - Sega SG-1000 | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Spectrum - Sound Tracker 1.1 | zxtune | none | no | none-yet | none |
| Noise Tracker GS 1 | noisetracker | none | no | none-yet | none |
| Video Game Music - BBC Micro | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| TIATracker | tiatracker | none | no | none-yet | none |
| MusicMaker V8 | uade | external (MusicMaker 4/8) | yes | nuade adapter (M1) | M1 |
| Starkos | sks | none | no | none-yet | none |
| General DigiMusic | openmpt | none | no | openmpt class (M2) | M2 |
| Pretracker | uade | internal (PreTracker) | no | native UADE fallback (Amiga, M4) | M4 |
| Shroom | shroom | none | no | none-yet | none |
| Digitrakker | openmpt | none | no | openmpt class (M2) | M2 |
| Graoumf Tracker 2 | openmpt | none | no | openmpt class (M2) | M2 |
| SidMon 1 | uade | internal (Sidmon v1/v2) | yes | nuade adapter (M1) | M1 |
| Infogrames | uade | external (Rob Hubbard 2 / Infogrames) | yes | nuade adapter (M1) | M1 |
| Game Music Creator | openmpt | internal (Game Music Creator) | yes | nuade adapter (M1) | M1 |
| Coconizer | xmp | none | no | none-yet | none |
| Spectrum - Pro Sound Maker | zxtune | none | no | none-yet | none |
| Video Game Music - MSX1 | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| Graoumf Tracker | openmpt | none | no | openmpt class (M2) | M2 |
| JamCracker | uade | internal (JamCracker) | yes | nuade adapter (M1) | M1 |
| Startrekker AM | xmp | internal (StarTrekker / Audio Sculpture) | yes | nuade adapter (M1) | M1 |
| Chiptracker | openmpt | external (ChipTracker) | no | openmpt class (M2) | M2 |
| Dave Lowe | uade | internal (Dave Lowe) | yes | nuade adapter (M1) | M1 |
| Ben Daglish | uade | internal (Ben Daglish) | yes | nuade adapter (M1) | M1 |
| HivelyTracker | zxtune | none | no | none-yet | none |
| Video Game Music - MSX2 | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| Ad Lib - ADL | adplug | none | no | none-yet | none |
| MultiMedia Sound | openmpt | none | no | openmpt class (M2) | M2 |
| Pumatracker | openmpt | internal (PumaTracker) | yes | nuade adapter (M1) | M1 |
| Spectrum - Chip Tracker | zxtune | none | no | none-yet | none |
| Ad Lib - Screamtracker 3 AdLib | openmpt | none | no | openmpt class (M2) | M2 |
| Asylum | openmpt | none | no | openmpt class (M2) | M2 |
| Spectrum - Global Tracker | zxtune | none | no | none-yet | none |
| Ad Lib - Ken's AdLib Music | adplug | none | no | none-yet | none |
| Dave Lowe New | uade | internal (Dave Lowe) | yes | nuade adapter (M1) | M1 |
| ProTrekkr 2.0 | protrekkr | none | no | none-yet | none |
| Organya 2 | organya | none | no | none-yet | none |
| Spectrum - Sound Tracker 1.3 | zxtune | none | no | none-yet | none |
| Farandole Composer | openmpt | none | no | openmpt class (M2) | M2 |
| Jesper Olsen | jesperolsen | none | no | none-yet | none |
| Composer 670 (CDFM) | openmpt | none | no | openmpt class (M2) | M2 |
| Special FX ST | uade | external (Special FX) | no | native UADE fallback (Amiga, M4) | M4 |
| Synthesis | uade | internal (Synthesis) | yes | nuade adapter (M1) | M1 |
| JayTrax | jxs | none | no | none-yet | none |
| Spectrum - ZXS | zxtune | none | no | none-yet | none |
| Spectrum - Extreme Tracker | zxtune | none | no | none-yet | none |
| AY Amadeus | fxm | none | no | none-yet | none |
| Sound Images | uade | none | yes | nuade adapter (M1) | M1 |
| Cybertracker C64 | cybertracker | none | no | none-yet | none |
| Stonetracker | stonetracker | internal (StoneTracker) | no | none-yet | none |
| Gameboy Sound System GBR | gbr | external (GBS via vgm2wav) | no | none-yet | none |
| SoundPlayer | uade | none | yes | nuade adapter (M1) | M1 |
| Special FX | uade | external (Special FX) | yes | nuade adapter (M1) | M1 |
| Digital Sound Studio | uade | none | yes | nuade adapter (M1) | M1 |
| Ad Lib - Bob's AdLib Music | adplug | none | no | none-yet | none |
| Spectrum - Multitrack | zxtune | none | no | none-yet | none |
| Ad Lib - Beni Tracker | adplug | none | no | none-yet | none |
| Ad Lib - Surprise! AdLib Tracker | adplug | none | no | none-yet | none |
| AProSys | uade | external (AProSys) | yes | nuade adapter (M1) | M1 |
| Maniacs Of Noise | uade | internal (Maniacs of Noise) | yes | nuade adapter (M1) | M1 |
| Ad Lib - Mlat Adlib Tracker | adplug | none | no | none-yet | none |
| Ad Lib - Twin TrackPlayer | adplug | none | no | none-yet | none |
| Spectrum - PSG | zxtune | none | no | none-yet | none |
| Activision Pro | uade | external (Activision Pro) | yes | nuade adapter (M1) | M1 |
| MikMod UNITRK | unitrk | none | no | none-yet | none |
| Unique Development | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| Velvet Studio | openmpt | none | no | openmpt class (M2) | M2 |
| Digital Sound Interface Kit RIFF | openmpt | none | no | openmpt class (M2) | M2 |
| Ixalance | ixalance | none | no | none-yet | none |
| Onyx Music File | onyx | none | no | none-yet | none |
| TCB Tracker | openmpt | external (TCB Tracker) | no | openmpt class (M2) | M2 |
| Wally Beben | uade | external (Wally Beben) | yes | nuade adapter (M1) | M1 |
| Maximum Effect | uade | none | yes | nuade adapter (M1) | M1 |
| MIDI-Loriciel | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| Pierre Adane Packer | uade | none | yes | nuade adapter (M1) | M1 |
| Synth Dream | uade | none | yes | nuade adapter (M1) | M1 |
| Video Game Music - Sega Mega CD | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Westwood SND | wwsnd | none | no | none-yet | none |
| Ad Lib - Digital FM | adplug | none | no | none-yet | none |
| ArkosTracker | aks | none | no | none-yet | none |
| Video Game Music - Vectrex | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| Ad Lib - Master Tracker | adplug | none | no | none-yet | none |
| Sound Master II v3 | uade | external (Sound Master) | yes | nuade adapter (M1) | M1 |
| Soundtracker 2.6 | openmpt | internal (ProTracker / PS3M) | no | openmpt class (M2) | M2 |
| Spectrum - General Sound | zxtune | none | no | none-yet | none |
| Jason Brooke | uade | external (Jason Brooke) | yes | nuade adapter (M1) | M1 |
| SoundControl | uade | external (SoundControl) | yes | nuade adapter (M1) | M1 |
| Ad Lib - MK-Jamz | adplug | none | no | none-yet | none |
| Anders Oland | uade | none | yes | nuade adapter (M1) | M1 |
| Klystrack | klystrack | none | no | none-yet | none |
| PokeyNoise | uade | none | yes | nuade adapter (M1) | M1 |
| Quadra Composer | uade | none | yes | nuade adapter (M1) | M1 |
| Synth Pack | uade | external (Synth Pack) | yes | nuade adapter (M1) | M1 |
| GlueMon | uade | internal (GlueMon) | yes | nuade adapter (M1) | M1 |
| Sound Master | uade | external (Sound Master) | yes | nuade adapter (M1) | M1 |
| SynTracker | uade | external (SynTracker) | yes | nuade adapter (M1) | M1 |
| Ad Lib - Herad Music System | adplug | none | no | none-yet | none |
| Ad Lib - Palladix | adplug | none | no | none-yet | none |
| Atari Digi-Mix | ym | none | no | none-yet | none |
| Speedy System | uade | none | yes | nuade adapter (M1) | M1 |
| The Musical Enlightenment | uade | internal (The Musical Enlightenment) | yes | nuade adapter (M1) | M1 |
| Darius Zendeh | uade | none | yes | nuade adapter (M1) | M1 |
| Electronic Music System | uade | none | yes | nuade adapter (M1) | M1 |
| Fashion Tracker | uade | none | yes | nuade adapter (M1) | M1 |
| NovoTrade Packer | uade | none | no | openmpt class (M2) | M2 |
| OctaMED MMDC | uade | internal (MED / OctaMED) | yes | nuade adapter (M1) | M1 |
| Paul Shields | uade | none | yes | nuade adapter (M1) | M1 |
| ProTrekkr | protrekkr | none | no | none-yet | none |
| Sean Conran | uade | none | yes | nuade adapter (M1) | M1 |
| DSMI Compact | openmpt | none | no | openmpt class (M2) | M2 |
| Follin Player II | uade | external (Tim Follin II) | yes | nuade adapter (M1) | M1 |
| Images Music System | openmpt | none | no | openmpt class (M2) | M2 |
| Ken's Digital Music | kdm | none | no | none-yet | none |
| Liquid Tracker | xmp | none | no | none-yet | none |
| Paul Robotham | uade | none | yes | nuade adapter (M1) | M1 |
| TFMX ST | uade | external (TFMX) | no | native UADE fallback (Amiga, M4) | M4 |
| Ad Lib - Ultima 6 | adplug | none | no | none-yet | none |
| BoyScout | boyscout | none | no | none-yet | none |
| Kris Hatlelid | uade | external (Kris Hatlelid) | yes | nuade adapter (M1) | M1 |
| Peter Verswyvelen | uade | none | yes | nuade adapter (M1) | M1 |
| Rob Hubbard | uade | external (Rob Hubbard) | yes | nuade adapter (M1) | M1 |
| Spectrum - SQ Digital Tracker | zxtune | none | no | none-yet | none |
| Spectrum - Vortex Tracker II | vt2 | none | no | none-yet | none |
| CBA | openmpt | none | no | openmpt class (M2) | M2 |
| Digital Sonix And Chrome | uade | external (Sonix Music Driver) | yes | nuade adapter (M1) | M1 |
| Protracker IFF | openmpt | internal (ProTracker / PS3M) | no | openmpt class (M2) | M2 |
| SoundFactory | uade | none | yes | nuade adapter (M1) | M1 |
| Spectrum - Furnace | furnace | none | no | none-yet | none |
| Vic-Tracker | victracker | none | no | none-yet | none |
| Video Game Music - Colecovision | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| Video Game Music - Sega SC-3000 | gme | external (VGM via vgm2wav) | no | gme class (M2) | M2 |
| Ad Lib - SoundFX Macs Opera | adplug | none | no | none-yet | none |
| CustomMade | uade | internal (CustomMade) | yes | nuade adapter (M1) | M1 |
| Electronic Music System v6 | uade | none | yes | nuade adapter (M1) | M1 |
| Lionheart | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| SCUMM | uade | none | yes | nuade adapter (M1) | M1 |
| Zoundmonitor | zoundmonitor | none | no | none-yet | none |
| Ad Lib - Exotic AdLib | adplug | none | no | none-yet | none |
| Ad Lib - Johannes Bjerregard Module | adplug | none | no | none-yet | none |
| Arpeggiator | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| Astroidea XMF | openmpt | none | no | openmpt class (M2) | M2 |
| Delta Music | uade | internal (Delta Music v1/v2) | yes | nuade adapter (M1) | M1 |
| Disorder Tracker 2 | openmpt | none | no | openmpt class (M2) | M2 |
| Future Player | uade | internal (Future Player) | yes | nuade adapter (M1) | M1 |
| Imago Orpheus | openmpt | none | no | openmpt class (M2) | M2 |
| Mike Davies | uade | none | yes | nuade adapter (M1) | M1 |
| Music Assembler | uade | internal (Music Assembler) | yes | nuade adapter (M1) | M1 |
| NoiseTrekker 2 | protrekkr | none | no | none-yet | none |
| PlayerPro | playerpro | none | no | none-yet | none |
| Ron Klaren | uade | none | yes | nuade adapter (M1) | M1 |
| Sound Master II v1 | uade | external (Sound Master) | yes | nuade adapter (M1) | M1 |
| Sound Programming Language | uade | external (soprol) | yes | nuade adapter (M1) | M1 |
| Spectrum - ProDigiTracker | zxtune | none | no | none-yet | none |
| VGM Music Maker | vgemm | none | no | none-yet | none |
| Video Game Music - Nintendo Virtual Boy | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| David Hanney | uade | none | yes | nuade adapter (M1) | M1 |
| Dynamic Studio Professional | openmpt | none | no | openmpt class (M2) | M2 |
| EasyTrax | openmpt | none | no | openmpt class (M2) | M2 |
| FM Tracker | openmpt | none | no | openmpt class (M2) | M2 |
| Jeroen Tel | uade | external (Jeroen Tel) | yes | nuade adapter (M1) | M1 |
| Mark Cooksey | uade | external (Mark Cooksey) | yes | nuade adapter (M1) | M1 |
| Ad Lib - Faust Music Creator | adplug | none | no | none-yet | none |
| Ad Lib - FM-Kingtracker | adplug | none | no | none-yet | none |
| Digital Mugician 2 | uade | external (Mugician II / mugician.amp) | yes | nuade adapter (M1) | M1 |
| Funktracker | zxtune | none | no | none-yet | none |
| Jason Page Old | uade | external (Jason Page) | yes | nuade adapter (M1) | M1 |
| SunTronic | uade | none | yes | nuade adapter (M1) | M1 |
| Voodoo Supreme Synthesizer | uade | internal (Voodoo Supreme Synthesizer) | yes | nuade adapter (M1) | M1 |
| Ad Lib - AMusic XMS | adplug | none | no | none-yet | none |
| Ad Lib - Boom Tracker | adplug | none | no | none-yet | none |
| Ashley Hogg | uade | none | yes | nuade adapter (M1) | M1 |
| Core Design | uade | none | yes | nuade adapter (M1) | M1 |
| Digital Tracker MOD | openmpt | none | no | openmpt class (M2) | M2 |
| Hippel 7V | uade | external (Jochen Hippel 7v) | yes | nuade adapter (M1) | M1 |
| InStereo! 2.0 | uade | external (In Stereo 1.0 / 2.0) | yes | nuade adapter (M1) | M1 |
| Janko Mrsic-Flogel | uade | none | yes | nuade adapter (M1) | M1 |
| Megatracker | xmp | none | no | none-yet | none |
| Spectrum - ASC Sound Master 0 | zxtune | none | no | none-yet | none |
| Steve Barrett | uade | none | yes | nuade adapter (M1) | M1 |
| AM Composer | uade | none | yes | nuade adapter (M1) | M1 |
| Audio Sculpture | xmp | internal (StarTrekker / Audio Sculpture) | yes | nuade adapter (M1) | M1 |
| Dynamic Synthesizer | uade | none | yes | nuade adapter (M1) | M1 |
| Mark Cooksey Old | uade | external (Mark Cooksey) | yes | nuade adapter (M1) | M1 |
| Pollytracker | polly | none | no | none-yet | none |
| Spectrum - Sample Tracker | zxtune | none | no | none-yet | none |
| Ad Lib - Extra Simple Music | adplug | none | no | none-yet | none |
| Art And Magic | uade | none | yes | nuade adapter (M1) | M1 |
| Fred Gray | uade | none | yes | nuade adapter (M1) | M1 |
| MCMD | uade | none | yes | nuade adapter (M1) | M1 |
| Medley | uade | internal (Medley Sound) | yes | nuade adapter (M1) | M1 |
| Paul Summers | uade | none | yes | nuade adapter (M1) | M1 |
| Quartet PSG | uade | external (Quartet / Quartet ST) | no | native UADE fallback (Amiga, M4) | M4 |
| Riff Raff | uade | none | yes | nuade adapter (M1) | M1 |
| Ad Lib - DeFy AdLib Tracker | adplug | none | no | none-yet | none |
| Ben Daglish SID | uade | internal (Ben Daglish) | yes | nuade adapter (M1) | M1 |
| DarkWave Studio | darkwave | none | no | none-yet | none |
| Desire | uade | none | yes | nuade adapter (M1) | M1 |
| Dirk Bialluch | uade | none | yes | nuade adapter (M1) | M1 |
| EarAche | uade | external (EarAche) | yes | nuade adapter (M1) | M1 |
| Extreme Tracker | openmpt | none | no | openmpt class (M2) | M2 |
| Howie Davies | uade | none | yes | nuade adapter (M1) | M1 |
| InStereo! | uade | external (In Stereo 1.0 / 2.0) | yes | nuade adapter (M1) | M1 |
| Leggless Music Editor | uade | none | yes | nuade adapter (M1) | M1 |
| Professional Sound Artists | uade | none | yes | nuade adapter (M1) | M1 |
| Real Tracker | openmpt | none | no | openmpt class (M2) | M2 |
| Rob Hubbard ST | uade | external (Rob Hubbard) | yes | nuade adapter (M1) | M1 |
| Sean Connolly | uade | none | yes | nuade adapter (M1) | M1 |
| Soundtracker Pro II | openmpt | none | no | openmpt class (M2) | M2 |
| Spectrum - VGM | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| Speedy A1 System | uade | none | yes | nuade adapter (M1) | M1 |
| Ad Lib - DOSBox | adplug | none | no | none-yet | none |
| Ad Lib - MPU-401 Trakker | adplug | none | no | none-yet | none |
| Ad Lib - SNG Player | adplug | none | no | none-yet | none |
| Cubic Tiny XM | mxm | none | no | none-yet | none |
| Delta Packer | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| Digitrekker | digitrekker | none | no | none-yet | none |
| FuchsTracker | xmp | none | no | none-yet | none |
| Future Composer BSI | uade | internal (Future Composer v1.0-1.4) | yes | nuade adapter (M1) | M1 |
| Magnetic Fields Packer | uade | none | no | openmpt class (M2) | M2 |
| Powertracker | uade | none | no | native UADE fallback (Amiga, M4) | M4 |
| Thomas Hermann | uade | none | yes | nuade adapter (M1) | M1 |
| Tomy Tracker | uade | none | yes | nuade adapter (M1) | M1 |
| Unis 669 | openmpt | none | no | openmpt class (M2) | M2 |
| Actionamics | uade | none | yes | nuade adapter (M1) | M1 |
| Ad Lib - AdLib Tracker | adplug | none | no | none-yet | none |
| AY STRC | strc | none | no | none-yet | none |
| BP SoundMon 1 | uade | internal (BP SoundMon v2/v3) | yes | nuade adapter (M1) | M1 |
| Composer 667 | openmpt | none | no | openmpt class (M2) | M2 |
| Digital Mugician 3 | uade | external (Mugician II / mugician.amp) | yes | nuade adapter (M1) | M1 |
| Megadrive CYM | cym | none | no | none-yet | none |
| Startrekker FLT8 | openmpt | internal (StarTrekker / Audio Sculpture) | no | openmpt class (M2) | M2 |
| Video Game Music - Sharp X68000 | vgm | external (VGM via vgm2wav) | no | none-yet | none |
| MDX | mdx | external (mdx2wav) | no | none-yet | none |
| Ultra64 Sound Format | zxtune | none | no | none-yet | none |
| Saturn Sound Format | zxtune | none | no | none-yet | none |
| Sam Coupe COP | zxtune | none | no | none-yet | none |
| Psycle | psycle | none | no | none-yet | none |
| Delitracker Custom | uade | internal (DeliTracker Custom) | yes | nuade adapter (M1) | M1 |
| HES | gme | external (HES via vgm2wav) | no | gme class (M2) | M2 |
| KSS | kss | external (KSS via vgm2wav) | no | none-yet | none |
| Sonic Arranger | uade | external (Sonic Arranger) | yes | nuade adapter (M1) | M1 |
| SunVox | sunvox | none | no | none-yet | none |
| SPU | spu | none | no | none-yet | none |
| MoonBlaster (edit mode) | kss | none | no | none-yet | none |
| Ad Lib - Reality AdLib Tracker | adplug | none | no | none-yet | none |
| Mad Tracker 2 | openmpt | none | no | openmpt class (M2) | M2 |
| Archimedes Tracker | xmp | none | no | none-yet | none |
| Hippel COSO | uade | internal (Hippel / Hippel-COSO) | yes | nuade adapter (M1) | M1 |
| OpenMPT MPTM | openmpt | none | no | openmpt class (M2) | M2 |
| Spectrum - Snapshot | zxtune | none | no | none-yet | none |
| Hippel | uade | internal (Hippel / Hippel-COSO) | yes | nuade adapter (M1) | M1 |
| FredMon | uade | internal (FredMon) | yes | nuade adapter (M1) | M1 |
| Buzz | buzz | none | no | none-yet | none |
| MPK | kss | none | no | none-yet | none |
| Spectrum |  | none | no | none-yet | none |
| Ad Lib - Martin Fernandez | adplug | none | no | none-yet | none |
| PiECE music driver | piece | none | no | none-yet | none |
| Mark II | uade | internal (Mark II) | yes | nuade adapter (M1) | M1 |
| Beathoven Synthesizer | uade | internal (Beathoven) | yes | nuade adapter (M1) | M1 |
| Jason Page | uade | external (Jason Page) | yes | nuade adapter (M1) | M1 |
| Skale Tracker | skale | none | no | none-yet | none |
| Hippel ST | uade | external (Jochen Hippel ST) | no | native UADE fallback (Amiga, M4) | M4 |
| Sam Coupe SNG | zxtune | none | no | none-yet | none |
| GoatTracker 2 | goattracker | none | no | none-yet | none |
| Silmarils | uade | none | yes | nuade adapter (M1) | M1 |
| SBStudio | sbstudio | none | no | none-yet | none |
| Forgotten Worlds | uade | none | yes | nuade adapter (M1) | M1 |
| NoiseTrekker | protrekkr | none | no | none-yet | none |
| MusicMaker V8 Old | uade | external (MusicMaker 4/8) | yes | nuade adapter (M1) | M1 |
| Renoise | none (does not play) | none | no | none-yet | none |
| Renoise Old | none (does not play) | none | no | none-yet | none |
| Ace Tracker | none (does not play) | none | no | none-yet | none |
| Picatune2 | none (does not play) | none | no | none-yet | none |
| DreamStation | none (does not play) | none | no | none-yet | none |
| Synder Tracker | none (does not play) | none | no | none-yet | none |
| All Sound Tracker | none (does not play) | none | no | none-yet | none |
| Aero Studio | none (does not play) | none | no | none-yet | none |
| Ad Lib - Drum Traker | none (does not play) | none | no | none-yet | none |
| MVX Module | none (does not play) | none | no | none-yet | none |
| RamTracker | none (does not play) | none | no | none-yet | none |
| Tunefish | none (does not play) | none | no | none-yet | none |
| GT Game Systems | none (does not play) | none | no | none-yet | none |
| MegaStation MIDI | none (does not play) | none | no | none-yet | none |
| Beaver Sweeper | none (does not play) | none | no | none-yet | none |
| Compoz | none (does not play) | none | no | none-yet | none |
| Athtune | none (does not play) | none | no | none-yet | none |
| SVAr Tracker | none (does not play) | none | no | none-yet | none |
| MegaStation | none (does not play) | none | no | none-yet | none |
| MVS Tracker | none (does not play) | none | no | none-yet | none |
| AND XSynth | none (does not play) | none | no | none-yet | none |
| Cybertracker | none (does not play) | none | no | none-yet | none |
| MIDI (General MIDI) | none | external (delimidi, serial out) / internal MIDI | no | none-yet | none |
| MP3 | none | internal (mpega) / external (MHI hardware) | no | stream decoder mp3/ogg/flac (M3) | M3 |
| WAV / AIFF / IFF sample files | none | internal (sample player) | no | stream decoder mp3/ogg/flac (M3) | M3 |
| Ogg Vorbis (design only) | none | none | no | stream decoder mp3/ogg/flac (M3) | M3 |
| FLAC (design only) | none | none | no | stream decoder mp3/ogg/flac (M3) | M3 |
| Eaglestone (Hippo external player, format unconfirmed) | none | external (eaglestone) | no | none-yet | none |
| Synth 4.0 (Hippo external player, format unconfirmed) | none | external (synth 4.0) | no | none-yet | none |

## Counts

Commands run from the research directory against this file ($F). Table rows end in a milestone cell, header row excluded.

```
grep -c '^| .* | \(M[1-4]\|none\) |$' $F                 -> 445 formats total (438 bbmp + 7 non-bbmp rows)
grep -c '^| .* | M1 |$' $F                                -> 119 nuade adapter (M1)
grep -c '^| .* | M2 |$' $F                                -> 81  (63 openmpt class, 14 gme class, 4 sidplayfp class)
grep -c '^| .* | M3 |$' $F                                -> 4   stream decoder (MP3, WAV/AIFF/IFF, Ogg, FLAC)
grep -c '^| .* | M4 |$' $F                                -> 17  native UADE fallback
grep -c '^| .* | none-yet | none |$' $F                   -> 224 no owner yet
grep -c '^| .* | yes | .* | M[1-4] |$' $F                 -> 119 rows nuade plays (all of them M1 owners)
grep -c '| external (.*) | [a-z]* | ' $F                   -> 76  Hippo external player
grep -c '| internal (.*) | [a-z]* | ' $F                   -> 61  Hippo internal replayer
grep -c '| none (does not play) |' $F                      -> 22  bbmp lists but cannot play
```

119 + 81 + 4 + 17 + 224 = 445, matches the total.

By source: bbmp 438 rows (31 verified, 348 plays, 37 faults, 22 not playing, per formats.md; 438 re-counted from the parsed tables of the four sections). HippoPlayer covers 137 table rows (61 internal, 76 external, regex-matched, see Notes); 5 rows come from outside bbmp: MIDI, Eaglestone, Synth 4.0 (HippoPlayer), MP3 and WAV/AIFF/IFF (HippoPlayer sample files; bbmp has none), plus Ogg Vorbis and FLAC (design only, in neither player). Those are the 7 non-bbmp rows.

Counts were verified a second time from the structure that produces them: the 438 comes from parsing every table row of formats.md (a script, `rows.json`), the nuade 125 players from `ls nuade/players | wc -l`, the nuade doc totals (137 ported, 8 not ported, 31 skipped) from `docs/players.md`.

## No owner yet

- backend zxtune (41): Spectrum - Pro Tracker 3; Spectrum - Pro Tracker 2; Nintendo DS Sound Format; Gameboy Sound Format; Playstation Sound Format; Slight Atari Player; Playstation 2 Sound Format; Spectrum - ST Song Compiler; Dreamcast Sound Format; Spectrum - ASC Sound Master; Spectrum - Sound Tracker Pro; Spectrum - SQ Tracker; Spectrum - Vortex; Spectrum - Sound Tracker Pro 2; Spectrum - Digital Studio; Spectrum - Pro Sound Creator; V2; Spectrum - Pro Tracker 1; Spectrum - Digital Music Maker; Spectrum - Fast Tracker; Spectrum - Sound Tracker 1.1; Spectrum - Pro Sound Maker; HivelyTracker; Spectrum - Chip Tracker; Spectrum - Global Tracker; Spectrum - Sound Tracker 1.3; Spectrum - ZXS; Spectrum - Extreme Tracker; Spectrum - Multitrack; Spectrum - PSG; Spectrum - General Sound; Spectrum - SQ Digital Tracker; Spectrum - ProDigiTracker; Funktracker; Spectrum - ASC Sound Master 0; Spectrum - Sample Tracker; Ultra64 Sound Format; Saturn Sound Format; Sam Coupe COP; Spectrum - Snapshot; Sam Coupe SNG
- backend adplug (40): Ad Lib - Creative Music File; Ad Lib - Raw OPL Capture; Ad Lib - LucasArts; Ad Lib - AdLib Tracker 2; Ad Lib - Visual Composer; Ad Lib - HSC AdLib Composer; Ad Lib - Apogee; Ad Lib - Loudness Sound System; Ad Lib - AMusic; Ad Lib - EdLib D00; Ad Lib - Sierra; Ad Lib - Surprise! AdLib Tracker 2.0; Ad Lib - ADL; Ad Lib - Ken's AdLib Music; Ad Lib - Bob's AdLib Music; Ad Lib - Beni Tracker; Ad Lib - Surprise! AdLib Tracker; Ad Lib - Mlat Adlib Tracker; Ad Lib - Twin TrackPlayer; Ad Lib - Digital FM; Ad Lib - Master Tracker; Ad Lib - MK-Jamz; Ad Lib - Herad Music System; Ad Lib - Palladix; Ad Lib - Ultima 6; Ad Lib - SoundFX Macs Opera; Ad Lib - Exotic AdLib; Ad Lib - Johannes Bjerregard Module; Ad Lib - Faust Music Creator; Ad Lib - FM-Kingtracker; Ad Lib - AMusic XMS; Ad Lib - Boom Tracker; Ad Lib - Extra Simple Music; Ad Lib - DeFy AdLib Tracker; Ad Lib - DOSBox; Ad Lib - MPU-401 Trakker; Ad Lib - SNG Player; Ad Lib - AdLib Tracker; Ad Lib - Reality AdLib Tracker; Ad Lib - Martin Fernandez
- backend none (does not play) (22): Renoise; Renoise Old; Ace Tracker; Picatune2; DreamStation; Synder Tracker; All Sound Tracker; Aero Studio; Ad Lib - Drum Traker; MVX Module; RamTracker; Tunefish; GT Game Systems; MegaStation MIDI; Beaver Sweeper; Compoz; Athtune; SVAr Tracker; MegaStation; MVS Tracker; AND XSynth; Cybertracker
- backend vgm (8): S98; Video Game Music - MSX1; Video Game Music - MSX2; Video Game Music - Vectrex; Video Game Music - Colecovision; Video Game Music - Nintendo Virtual Boy; Spectrum - VGM; Video Game Music - Sharp X68000
- backend kss (7): MoonBlaster; MGSDRV; MuSICA; OPLLDRV; KSS; MoonBlaster (edit mode); MPK
- backend xmp (6): Music Editor; Coconizer; Liquid Tracker; Megatracker; FuchsTracker; Archimedes Tracker
- backend furnace (4): Deflemask; FamiTracker; TFM Music Maker; Spectrum - Furnace
- backend protrekkr (4): ProTrekkr 2.0; ProTrekkr; NoiseTrekker 2; NoiseTrekker
- backend soundclub (2): Sound Club; Sound Club 2
- backend fxm (2): Spectrum - Fuxoft AY Language; AY Amadeus
- backend synder (2): Synder SNG-Player Stereo; Synder SNG-Player
- backend buz (2): Buzzic 2; Buzzic
- backend pixelpainters (2): Ad Lib - Pixel Painters FMF; Ad Lib - Pixel Painters FTF
- backend ym (2): YM; Atari Digi-Mix
- backend pxtone (2): Piston Collage; Piston Collage Protected
- backend organya (2): Organya; Organya 2
- backend goattracker (2): GoatTracker; GoatTracker 2
- backend musicline (1): Musicline Editor
- backend fac (1): FAC SoundTracker
- backend sccmusixx (1): SCC-Musixx
- backend ned (1): NerdTracker 2
- backend axs (1): AXS
- backend mgb (1): Gameboy Tracker
- backend picatune (1): Picatune
- backend berotracker (1): BeRoTracker
- backend tss (1): TSS
- backend t0ast (1): The 0ok Amazing Synth Tracker
- backend dsm (1): Digital Sound Interface Kit
- backend sig (1): Ad Lib - Sound Images Generation 2
- backend monotone (1): Monotone
- backend d01 (1): Ad Lib - EdLib D01
- backend lem (1): Ad Lib - Sound Interface System
- backend fmtracker (1): Ad Lib - FM Tracker
- backend samurai (1): Ad Lib - MUS
- backend shadowlands (1): Ad Lib - Shadowlands
- backend vibrants (1): Ad Lib - Vibrants
- backend fmp (1): FM sound driver (FMP)
- backend sndh (1): SNDH
- backend sc68 (1): SC68
- backend pmdc (1): PMD
- backend eup (1): Euphony
- backend snsf (1): Super Nintendo Sound Format
- backend sgc (1): SGC
- backend qsf (1): Capcom Q-Sound Format
- backend soundsmith (1): SoundSmith
- backend edlib (1): Ad Lib - EdLib Packed
- backend wsr (1): WonderSwan
- backend chipnsfx (1): Spectrum - CHIPNSFX
- backend beepola (1): Beepola
- backend fls (1): Spectrum - Flash Tracker
- backend maxtrax (1): MaxTrax
- backend noisetracker (1): Noise Tracker GS 1
- backend tiatracker (1): TIATracker
- backend sks (1): Starkos
- backend shroom (1): Shroom
- backend jesperolsen (1): Jesper Olsen
- backend jxs (1): JayTrax
- backend cybertracker (1): Cybertracker C64
- backend stonetracker (1): Stonetracker
- backend gbr (1): Gameboy Sound System GBR
- backend unitrk (1): MikMod UNITRK
- backend ixalance (1): Ixalance
- backend onyx (1): Onyx Music File
- backend wwsnd (1): Westwood SND
- backend aks (1): ArkosTracker
- backend klystrack (1): Klystrack
- backend kdm (1): Ken's Digital Music
- backend boyscout (1): BoyScout
- backend vt2 (1): Spectrum - Vortex Tracker II
- backend victracker (1): Vic-Tracker
- backend zoundmonitor (1): Zoundmonitor
- backend playerpro (1): PlayerPro
- backend vgemm (1): VGM Music Maker
- backend polly (1): Pollytracker
- backend darkwave (1): DarkWave Studio
- backend mxm (1): Cubic Tiny XM
- backend digitrekker (1): Digitrekker
- backend strc (1): AY STRC
- backend cym (1): Megadrive CYM
- backend mdx (1): MDX
- backend psycle (1): Psycle
- backend sunvox (1): SunVox
- backend spu (1): SPU
- backend buzz (1): Buzz
- backend  (1): Spectrum
- backend piece (1): PiECE music driver
- backend skale (1): Skale Tracker
- backend sbstudio (1): SBStudio
- Hippo-only: MIDI (General MIDI), Eaglestone, Synth 4.0 (no bbmp row)

## Notes

- Owner values are limited to the six in the brief. Everything bbmp plays through zxtune, adplug, xmp, vgm, kss, furnace, protrekkr, ym, or its one-off helper backends has no Selecta class, hence 221 none-yet bbmp rows. The largest block is zxtune (41 rows: ZX Spectrum trackers, DS/PS1/PS2/Saturn/Dreamcast/N64 sound formats) and adplug (40 rows: Ad Lib/OPL). A decision for the design: add a zxtune/adplug/furnace class, or accept those formats as out of scope. Not decided here.
- libgme also supports KSS and SAP and some VGM variants, so the KSS family (7 rows), Slight Atari Player and the vgm-backend rows might be owned by the gme class (M2) with no new plugin. Unverified; I did not check Selecta's gme build options or bbmp's reason for using separate backends.
- 112 of 131 UADE-backend rows (17 go to M4, 2 packers to the openmpt class) come out as nuade "yes" (grep: `| uade | .* | yes | nuade adapter`), by hand name match of the bbmp/Modland name against `nuade/players` directories and `docs/players.md`; 125 nuade player directories serve 137 ported UADE players (some serve several). Unverified: Delitracker Custom, FredMon (assumed `fred_np`), SoundPlayer (inside `scottjohnston`), Digital Mugician 1-3 (`digmug`), Digital Sound Studio (`dss`), Follin Player II (`timfollin`), IFF-SMUS (`sonix`), CustomMade (`ronklaren` covers only rk/rkb).
- nuade "no" and unverified: Unique Development, Lionheart, Arpeggiator, Powertracker, Delta Packer. No player of those names in `nuade/players` and not in `docs/players.md`; they may be served under another name. Placed on the M4 UADE fallback.
- nuade skips on purpose: AHX, Art of Noise, PreTracker, TFMX family (native players exist elsewhere, owner here is the UADE fallback M4, though the design may prefer native ones); the MOD packers (NovoTrade, Magnetic Fields) are assigned to the openmpt class on nuade's note that they are unpacked then played by libopenmpt. Other nuade skips (MED, ProTracker family, DIGI-Booster, Symphonie, ChipTracker, TCB Tracker, Images Music System, MultiMedia Sound) are bbmp openmpt rows and land in the openmpt class (M2).
- nuade not-ported non-Amiga formats (Hippel ST, Quartet ST/PSG, Special FX ST, YM/YMST, MIDI-Loriciel, CustomMade `cm`) are M4 fallback rows where bbmp has them. Sierra AGI, ADPCM mono and Paul Tonge are nuade/UADE-only: there is no matching bbmp row ("Ad Lib - Sierra" is the OPL music, a different thing), so they are NOT in the table. If the union should include UADE-only formats, add 3 rows (owner: native UADE fallback, M4).
- HippoPlayer: `hippoplayerupdate.readme` says its format list "could be incomplete" and the doc says "dozens of Eagleplayer-based formats". Internal-vs-external was decided by whether a name appears in `code/eagleplayers/bin` (45 entries) or is a vgm2wav/mdx2wav helper; I did not read puu016.s, so an "internal" label is inferred (listed in the readme, no file in `bin`).
- HippoPlayer formats whose bbmp/Modland name I could not match with confidence: Eaglestone and Synth 4.0 (external players, format not confirmed; rows added, none-yet), TakeTracker (folded into MultiTracker), Old SoundTracker (folded into Soundtracker), VectorDean (= Richard Joseph 2, folded into Richard Joseph), mugician.amp / Mugician II (folded into Digital Mugician), MusicMaker 4 (folded into MusicMaker V8 rows), Rob Hubbard 1 vs 2.
- Hippo label matches are regex on the Modland name; a bbmp row can get "none" in the Hippo column when Hippo plays it under a name I did not map (the 308 or so none cells are a lower bound of Hippo coverage, not a statement it cannot play them).
- bbmp "Doesn't play" (22 formats, 531 songs) are all none-yet and unplayable by anything; they cost nothing but are listed so the union is complete. Of these, Renoise, Ace Tracker, RamTracker and MegaStation MIDI would need a General MIDI synth or a new replayer.
- Ogg Vorbis and FLAC rows are in the design (M3) but in neither player's list.
- "Mark Cooksey Old" etc. and every other bbmp row keep the bbmp backend name from the Replayer column of formats.md; the 22 "Doesn't play" rows have no replayer.
