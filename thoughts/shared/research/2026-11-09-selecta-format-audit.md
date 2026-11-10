---
date: 2026-11-09
row: I1242
status: done
---
# Selecta format audit: every claimed extension through the CLI

Method: extension list dumped from the SEL_RP_EXTENSIONS hook of every module in build/sdl/modules (103 dylibs; 71 export an extension list), via a scratch dlopen program, giving 392 unique extensions. platform.c k_chips/k_uade_prefix were not used as the source (they classify, they do not claim). Playback: `selecta-cli render <file> --output <wav> --seconds 6 --resample linear` (writes a WAV, no audio device), then the WAV is checked for non-zero samples. Up to 7 sample files tried per extension; the verdict is for the best one. Environment: SELECTA_PLUGIN_PATH=build/sdl/modules (see Found below). nice 19, 25 s timeout per file.

## Summary

Total extensions: 392. PLAY 296, PLAY-SHORT 0, FAIL 28, HANG 0, NO SAMPLE 68.

Caveat: FAIL here means "none of the samples I could find played". Many fails use DEViLBOX/fixture files that may be the wrong sub-type (instrument files, companion files, other formats sharing the extension) or prefix-style names found by loose matching; treat each as "needs a known-good sample", not proof the replayer is broken. Samples under <scratch> were downloaded from the Modland mirror by library path.

Default-path finding: with no env var the CLI loaded only 39 built-in modules (builtin_loaded=39, dyn_loaded=0); adplug, xmp, all bbmp_*, furnace, libvgm, pxtone, qsf etc. were absent and their files gave no_replayer. First pass without SELECTA_PLUGIN_PATH: 149 PLAY / 123 FAIL / 120 NO SAMPLE. Whether the shipped app loads them from its plugin dir was not checked.

## FAIL and HANG by cause

### no_replayer: extension claimed, but no replayer accepted this file (wrong/companion/instrument-only sample, or detect rejects it) (12)

| ext | claimed by | sample | detail |
|---|---|---|---|
| ay | bbmp_ay,gme | <scratch>/dl/ay/ay.pt3 | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| ct | bbmp_cybertracker | ~/Code/DEViLBOX/public/data/songs/iff-smus/Red Leopard/instruments/Ct.lead1.instr | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/iff-smus/Red Leopar |
| dsm | openmpt | <scratch>/dl/dsm/dsm.pdx | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| jpn | nuade | <scratch>/dl/jpn/jpn.road | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| md | nuade | third_party/furnace/extern/SAASound/README.MD | rc=1 error no_replayer /Users/spot/Code/MusicPlayer/third_party/furnace/extern/SAASoun |
| med | openmpt | <scratch>/dl/med/j2.med | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| mp2 | minimp3 | <scratch>/dl/mp2/mp2.usflib | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| sm1 | nuade | tests/m3/fixtures/.cache/2NDR-V.SM1 | rc=1 error no_replayer tests/m3/fixtures/.cache/2NDR-V.SM1 |
| sm2 | nuade | tests/m3/fixtures/.cache/2NDR-V.SM2 | rc=1 error no_replayer tests/m3/fixtures/.cache/2NDR-V.SM2 |
| soc | nuade | ~/Code/DEViLBOX/public/data/songs/hippel-st-coso/ghostbattle titletune.soc | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/hippel-st-coso/ghos |
| ss | nuade | ~/Code/DEViLBOX/public/data/songs/sonix/snx/A-10 Tank Killer/instruments/clap.ss | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/sonix/snx/A-10 Tank |
| st | nuade | ~/Code/DEViLBOX/public/data/songs/sawteeth/okolaNUKE.st | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/sawteeth/okolaNUKE. |

### open_failed: a replayer claimed the file and its loader rejected it (10)

| ext | claimed by | sample | detail |
|---|---|---|---|
| adl | adplug | ~/Code/DEViLBOX/public/data/songs/Ad Lib/ADL/Paul Mudra/eob2 - crimson1.adl | rc=1 error open_failed /Users/spot/Code/DEViLBOX/public/data/songs/Ad Lib/ADL/Paul Mud |
| mcr | nuade | <scratch>/dl/mcr/mcr.aquablast | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| ned | bbmp_ned | ~/.cache/selecta/files/modland/pub/modules/NerdTracker 2/Memblers/nudrom1.ned | rc=1 error open_failed /Users/spot/.cache/selecta/files/modland/pub/modules/NerdTracke |
| pap | nuade | ~/Code/DEViLBOX/public/data/songs/formats/Ivanhoe_ivanhoe - hiscore.pap | rc=1 error open_failed /Users/spot/Code/DEViLBOX/public/data/songs/formats/Ivanhoe_iva |
| pn | nuade | <scratch>/dl/pn/pn.tetris | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| sdr | nuade | <scratch>/dl/sdr/sdr.nobuddiesland 1 | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| sho | bbmp_shroom | tests/m3/fixtures/.cache/netke.sho | rc=1 error open_failed tests/m3/fixtures/.cache/netke.sho |
| sjs | nuade | <scratch>/dl/sjs/sjs.jb | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |
| spm | stonetracker | ~/Code/DEViLBOX/public/data/songs/stonetracker/hypnosphere.spm | rc=1 error open_failed /Users/spot/Code/DEViLBOX/public/data/songs/stonetracker/hypnos |
| tpu | nuade | <scratch>/dl/tpu/tpu.timelock title | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f |

### silent output: replayer opened the file, 6 s of all-zero samples (6)

| ext | claimed by | sample | detail |
|---|---|---|---|
| bds | nuade | ~/Code/DEViLBOX/public/data/songs/formats/terramex.bds | silent: 264600 frames all zero |
| dsr | nuade | ~/Code/DEViLBOX/public/data/songs/formats/batmanreturns.dsr | silent: 264600 frames all zero |
| gray | nuade | ~/Code/DEViLBOX/public/data/songs/fred-gray/eco.gray | silent: 264600 frames all zero |
| kss | gme,libkss | ~/Code/DEViLBOX/public/data/songs/kss/gradius 2.kss | silent: 264600 frames all zero |
| mptm | openmpt | third_party/libopenmpt/test/test.mptm | silent: 264600 frames all zero |
| tiny | nuade | ~/Code/DEViLBOX/public/data/songs/sonix/tiny/The Games - Winter Edition/france.tiny | silent: 264600 frames all zero |

## NO SAMPLE (68)

a2t (adplug), bfc (nuade), bp2 (nuade), cin (nuade), coc (xmp), dl_deli (nuade), dlm1 (nuade), dmu2 (nuade), dwold (nuade), dzp (nuade), fc13 (nuade,openmpt), fc3 (nuade), fc4 (nuade), gm (nuade), got (adplug), ha2 (adplug), hsp (adplug), hsq (adplug), is10 (nuade), j2b (openmpt), jcb (nuade), jcbo (nuade), jpnd (nuade), jpold (nuade), mcmd_org (nuade), mdi (adplug), mdy (adplug), mg (nuade), mii (nuade), minisnsf (snsf), mkf (adplug), mkii (nuade), mkiio (nuade), mmcmp (openmpt), mod_adsc4 (nuade), mon_old (nuade), msc (adplug), mth (nuade), mw (nuade), mz (98fmplayer), nsfe (gme), nst (openmpt), oxm (openmpt), powt (nuade), ppm (openmpt), psid (sid), pt36 (openmpt), qc (nuade), rac (adplug), rix (adplug), rjp (nuade), rkb (nuade), s-c (nuade), s7g (nuade), sct (nuade), sd1 (nuade), sj (nuade), sop (adplug), st26 (openmpt), stk (openmpt), tfm (furnace), tmk (nuade), trc (nuade), tro (nuade), umx (openmpt), wlf (adplug), xpk (openmpt), zm (nuade)

## Full verdict table

| ext | claimed by | verdict | sample | detail | tries |
|---|---|---|---|---|---|
| 667 | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/composer-667/speed fighter.667 | peak=14540 frames=264600 | 1 |
| 669 | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/fonetag.669 | peak=12687 frames=264600 | 1 |
| a2m | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/AdLib Tracker 2/Brendan Bailey/battletoads briefing theme.a2m | peak=14672 frames=264600 | 1 |
| a2t | adplug | NO SAMPLE |  |  | 0 |
| aam | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/agony-level2.aam | peak=32729 frames=264600 | 1 |
| abk | nuade | PLAY | <scratch>/dl/abk/abk.mod | peak=4159 frames=264600 | 1 |
| adl | adplug | FAIL | ~/Code/DEViLBOX/public/data/songs/Ad Lib/ADL/Paul Mudra/eob2 - crimson1.adl | rc=1 error open_failed /Users/spot/Code/DEViLBOX/public/data/songs/Ad Lib/ADL/Paul Mud | 7 |
| adlib | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Martin Fernandez/_unknown/Wolfenstein 3D/salute.adlib | peak=5769 frames=264600 | 1 |
| adsc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/audio-sculpture/turtle-ready.adsc | peak=26694 frames=264600 | 2 |
| agd | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Herad Music System/Stephane Picq/dune - water.agd | peak=18036 frames=264600 | 1 |
| amad | bbmp_ay | PLAY | ~/Code/DEViLBOX/public/data/songs/ay-amadeus/aztec theme.amad | peak=7941 frames=264600 | 1 |
| amc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/krymini jingle 1.amc | peak=20151 frames=264600 | 1 |
| amd | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/AMusic/Karl Sommer/getready.amd | peak=23870 frames=264600 | 1 |
| amf | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/dsmi_pan.amf | peak=11232 frames=264600 | 1 |
| ams | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/kernel.ams | peak=16615 frames=264600 | 1 |
| aps | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/action.aps | peak=17424 frames=264600 | 1 |
| arp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/pseudofever.arp | peak=28448 frames=264600 | 1 |
| ash | nuade | PLAY | <scratch>/dl/ash/ash.mod | peak=31069 frames=264600 | 1 |
| ast | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/actionamics/dynablaster.ast | peak=20903 frames=264600 | 1 |
| avp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/activision-pro/gettysburg.avp | peak=25598 frames=264600 | 1 |
| axs | bbmp_axs | PLAY | tests/m3/fixtures/.cache/spectral.axs | peak=8597 frames=264600 | 1 |
| ay | bbmp_ay,gme | FAIL | <scratch>/dl/ay/ay.pt3 | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| bam | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Bob's AdLib Music/_unknown/clown.bam | peak=3617 frames=264600 | 1 |
| bd | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/mickey_mouse.bd | peak=15069 frames=264600 | 1 |
| bds | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/formats/terramex.bds | silent: 264600 frames all zero | 2 |
| bfc | nuade | NO SAMPLE |  |  | 0 |
| bgm | libkss | PLAY | <scratch>/dl/bgm/Bossus.BGM | peak=6026 frames=264600 | 1 |
| bmf | adplug | PLAY | <scratch>/dl/bmf/bmf.mod | peak=32768 frames=264600 | 1 |
| bmx | bbmp_buzz | PLAY | tests/m3/fixtures/.cache/axel2.bmx | peak=1757 frames=264600 | 1 |
| bp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/bp-soundmon-2/nicktune1.bp | peak=27331 frames=264600 | 1 |
| bp2 | nuade | NO SAMPLE |  |  | 0 |
| bp3 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/bp-soundmon-3/testmod.bp3 | peak=15746 frames=264600 | 1 |
| brt | bbmp_berotracker | PLAY | ~/.cache/selecta/files/modland/pub/modules/BeRoTracker/BeRo/reloaded dreams.brt | peak=10768 frames=264600 | 1 |
| bsf | bbmp_boyscout | PLAY | tests/m3/fixtures/.cache/numb.bsf | peak=3704 frames=264600 | 1 |
| bsi | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/future-composer-bsi/newtek.bsi | peak=32696 frames=264600 | 1 |
| bss | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/beathoven_smallest.bss | peak=15481 frames=264600 | 1 |
| buz | bbmp_buzzic | PLAY | tests/m3/fixtures/.cache/sample1.buz | peak=2286 frames=264600 | 1 |
| buz2 | bbmp_buzzic | PLAY | tests/m3/fixtures/.cache/outdream.buz2 | peak=9205 frames=264600 | 1 |
| bye | nuade | PLAY | <scratch>/dl/bye/bye.mod | peak=8660 frames=264600 | 1 |
| c67 | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/amnesia_credits.c67 | peak=12880 frames=264600 | 1 |
| cal | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Shadowlands/Matt Furniss/shadowlands (roofworlds).cal | peak=2037 frames=264600 | 1 |
| cba | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/chiptracker.cba | peak=6040 frames=264600 | 1 |
| cff | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Boom Tracker/_unknown/soom.cff | peak=4085 frames=264600 | 1 |
| chp | chipnsfx | PLAY | tests/m3/fixtures/chp_test.chp | peak=28949 frames=264600 | 1 |
| cin | nuade | NO SAMPLE |  |  | 0 |
| cmf | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Creative Music File/_unknown/algato.cmf | peak=3021 frames=264600 | 1 |
| coc | xmp | NO SAMPLE |  |  | 0 |
| core | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/core-design/dynamite dux.core | peak=18796 frames=264600 | 1 |
| ct | bbmp_cybertracker | FAIL | ~/Code/DEViLBOX/public/data/songs/iff-smus/Red Leopard/instruments/Ct.lead1.instr | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/iff-smus/Red Leopar | 6 |
| cym | libvgm | PLAY | tests/m3/fixtures/.cache/sample.cym | peak=29472 frames=264600 | 1 |
| d00 | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/EdLib D00/Matthias Hartung/jungle.d00 | peak=6912 frames=264600 | 1 |
| d01 | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/EdLib D01/JCH/gorgeous.d01 | peak=12061 frames=264600 | 1 |
| dat | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/paul-robotham/dawnpatrol-sad.dat | peak=2013 frames=264600 | 2 |
| dbm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/digibooster-pro/invisibility.dbm | peak=12014 frames=264600 | 1 |
| dfm | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Digital FM/Vincent Voois/hammer.dfm | peak=13989 frames=264600 | 1 |
| dh | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/david-hanney/tearaway thomas unused.dh | peak=12625 frames=264600 | 1 |
| digi | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/digibooster/the day after.digi | peak=8319 frames=264600 | 1 |
| dl | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/dave-lowe/incredibleshrinkingsphere.dl | peak=19211 frames=264600 | 1 |
| dl_deli | nuade | NO SAMPLE |  |  | 0 |
| dlm1 | nuade | NO SAMPLE |  |  | 0 |
| dln | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/dave-lowe-new/m-bison.dln | peak=10894 frames=264600 | 1 |
| dm | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/crusaders1.dm | peak=14880 frames=264600 | 1 |
| dm1 | nuade | PLAY | <scratch>/dl/dm1/dm1.s3m | peak=8653 frames=264600 | 1 |
| dm2 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/anthrox_intro.dm2 | peak=26112 frames=264600 | 1 |
| dmf | furnace,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/deflemask/ARJr/1-Up (SOUND).dmf | peak=17139 frames=264600 | 1 |
| dmo | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Twin TrackPlayer/Benjamin Gerardin/nancy, 3 heures du mat'.dmo | peak=14885 frames=264600 | 1 |
| dmu | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/flight.dmu | peak=16052 frames=264600 | 1 |
| dmu2 | nuade | NO SAMPLE |  |  | 0 |
| dns | nuade | PLAY | <scratch>/dl/dns/dns.mod | peak=28777 frames=264600 | 1 |
| dp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/locomotion.dp | peak=12270 frames=264600 | 1 |
| dro | libvgm | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/DOSBox/_unknown/dune1.dro | peak=7268 frames=264600 | 1 |
| dsc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/digital-sonix-and-chrome/David Hanlon/dragon'sbreath fanfares.dsc | peak=13177 frames=264600 | 1 |
| dsm | openmpt | FAIL | <scratch>/dl/dsm/dsm.pdx | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| dsr | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/formats/batmanreturns.dsr | silent: 264600 frames all zero | 2 |
| dss | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/digital-sound-studio/zrimay.dss | peak=12199 frames=264600 | 2 |
| dsym | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/drwho_final4.dsym | peak=13464 frames=264600 | 1 |
| dtm | adplug,bbmp_digitrekker,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/DeFy AdLib Tracker/Hector/dtm track 2.dtm | peak=15022 frames=264600 | 1 |
| dum | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/bob4e.dum | peak=7620 frames=264600 | 1 |
| dw | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/david-whittaker/garfield2+.dw | peak=28397 frames=264600 | 1 |
| dwold | nuade | NO SAMPLE |  |  | 0 |
| dwp | bbmp_darkwave | PLAY | tests/m3/fixtures/.cache/c101.dwp | peak=28034 frames=264600 | 1 |
| dz | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/darius-zendeh/crash and burn.dz | peak=19128 frames=264600 | 1 |
| dzp | nuade | NO SAMPLE |  |  | 0 |
| ea | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/skythebest.ea | peak=13824 frames=264600 | 1 |
| edl | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/EdLib Packed/_unknown/lilletet.edl | peak=8072 frames=264600 | 1 |
| emod | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/synth_corn.emod | peak=20478 frames=264600 | 1 |
| ems | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/perception.ems | peak=3948 frames=264600 | 1 |
| etx | openmpt | PLAY | <scratch>/dl/etx/nbg01.etx | peak=9166 frames=264600 | 1 |
| eup | eupmini | PLAY | ~/Code/DEViLBOX/public/data/songs/euphony/Requiem/REQUIEM.eup | peak=4110 frames=264600 | 1 |
| ex | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/fashion-tracker/ivory tover ii.ex | peak=5803 frames=264600 | 1 |
| far | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/m31.far | peak=13215 frames=264600 | 1 |
| fc | nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/anthrox.fc | peak=20128 frames=264600 | 1 |
| fc13 | nuade,openmpt | NO SAMPLE |  |  | 0 |
| fc14 | nuade,openmpt | PLAY | <scratch>/dl/fc14/fc14.awesome.xm | peak=14674 frames=264600 | 1 |
| fc3 | nuade | NO SAMPLE |  |  | 0 |
| fc4 | nuade | NO SAMPLE |  |  | 0 |
| flac | dr_flac | PLAY | third_party/libopenmpt/test/test.flac | peak=11520 frames=264600 | 1 |
| fls | bbmp_ay | PLAY | tests/m3/fixtures/.cache/simpletongift1.fls | peak=13429 frames=264600 | 1 |
| fmf | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Pixel Painters FMF/_unknown/laser light - sb ll fx.fmf | peak=5189 frames=264600 | 1 |
| fmt | bbmp_adlib,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/fm-tracker/fm dance.fmt | peak=11157 frames=264600 | 2 |
| fp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/imploder_drums.fp | peak=13857 frames=264600 | 1 |
| fred | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/bomb jack.fred | peak=18160 frames=264600 | 1 |
| fst | openmpt | PLAY | <scratch>/dl/fst/FST.sid | peak=16053 frames=264600 | 1 |
| ftf | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Pixel Painters FTF/_unknown/xatax - sng e0p1.ftf | peak=3714 frames=264600 | 1 |
| ftm | furnace,nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/staticoscillations.ftm | peak=5560 frames=264600 | 1 |
| fuchs | xmp | PLAY | <scratch>/dl/fuchs/lowtheme.fuchs | peak=32425 frames=264600 | 1 |
| fur | furnace | PLAY | ~/Code/DEViLBOX/public/data/songs/furnace/misc/this_is_kind_of_better_PV1000.fur | peak=32766 frames=264600 | 1 |
| fw | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/forgotten-worlds/forgotten worlds theend.fw | peak=20608 frames=264600 | 1 |
| fxm | bbmp_ay | PLAY | tests/m3/fixtures/.cache/belegost.fxm | peak=27453 frames=264600 | 1 |
| gbr | bbmp_nez | PLAY | <scratch>/dl/gbr/golf.gbr | peak=1792 frames=264600 | 1 |
| gbs | gme | PLAY | ~/Code/DEViLBOX/public/data/songs/gbs/airforce delta.gbs | peak=29449 frames=264600 | 1 |
| gdm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/breakin's chipsong.gdm | peak=23427 frames=264600 | 1 |
| glue | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/memphis.glue | peak=26471 frames=264600 | 1 |
| gm | nuade | NO SAMPLE |  |  | 0 |
| gmc | nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/knights_of_sky.gmc | peak=30017 frames=264600 | 1 |
| got | adplug | NO SAMPLE |  |  | 0 |
| gray | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/fred-gray/eco.gray | silent: 264600 frames all zero | 2 |
| gt2 | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/graoumf-tracker-2/living-on-video.gt2 | peak=28706 frames=264600 | 1 |
| gtk | openmpt | PLAY | <scratch>/dl/gtk/o.gtk | peak=9959 frames=264600 | 1 |
| gym | gme,libvgm | PLAY | ~/Code/DEViLBOX/public/data/songs/gym/level1.gym | peak=6539 frames=264600 | 1 |
| ha2 | adplug | NO SAMPLE |  |  | 0 |
| hd | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/blazing_thunder.hd | peak=16256 frames=264600 | 1 |
| hes | gme | PLAY | ~/Code/DEViLBOX/public/data/songs/hes/impossamole.hes | peak=13476 frames=264600 | 1 |
| hip | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/hippel/warp (ingame 2).hip | peak=7874 frames=264600 | 1 |
| hip7 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/ghostbattle_gameover.hip7 | peak=10370 frames=264600 | 1 |
| hipc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/hippel-coso/wings of death (level 3).hipc | peak=12949 frames=264600 | 1 |
| hot | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/primemover_09.hot | peak=20691 frames=264600 | 1 |
| hsc | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/HSC AdLib Composer/Hannes Seifert/1869/honour.hsc | peak=23697 frames=264600 | 1 |
| hsp | adplug | NO SAMPLE |  |  | 0 |
| hsq | adplug | NO SAMPLE |  |  | 0 |
| ice | openmpt | PLAY | <scratch>/dl/ice/ice.mod | peak=27399 frames=264600 | 1 |
| imf | adplug,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Apogee/Bobby Prince/Wolfenstein 3D/salute.imf | peak=5541 frames=264600 | 1 |
| ims | adplug,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/redoctober-sub-docking.ims | peak=16387 frames=264600 | 1 |
| is | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/fantasi8.is | peak=16128 frames=264600 | 1 |
| is10 | nuade | NO SAMPLE |  |  | 0 |
| is20 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/stereo_feeling.is20 | peak=32004 frames=264600 | 1 |
| it | openmpt | PLAY | ~/.cache/selecta/files/modland/pub/modules/Impulsetracker/Yuri Sucupira/pingus - rough journey.it | peak=2788 frames=264600 | 2 |
| j2b | openmpt | NO SAMPLE |  |  | 0 |
| jam | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/jamcracker/freehand-spreadtro.jam | peak=30529 frames=264600 | 1 |
| jb | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/Scott Johnston/sjs.jb | peak=11921 frames=264600 | 1 |
| jbm | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Johannes Bjerregard Module/Johannes Bjerregard/demo4.jbm | peak=5329 frames=264600 | 1 |
| jc | nuade | PLAY | <scratch>/dl/jc/jc.mod | peak=15077 frames=264600 | 1 |
| jcb | nuade | NO SAMPLE |  |  | 0 |
| jcbo | nuade | NO SAMPLE |  |  | 0 |
| jd | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/wildwheels_ingame.jd | peak=25410 frames=264600 | 1 |
| jmf | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/spacestation.jmf | peak=19423 frames=264600 | 1 |
| jo | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/lollypop-subgame_01.jo | peak=11299 frames=264600 | 1 |
| jpn | nuade | FAIL | <scratch>/dl/jpn/jpn.road | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| jpnd | nuade | NO SAMPLE |  |  | 0 |
| jpo | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/steve-turner/rainbow islands.jpo | peak=12125 frames=264600 | 1 |
| jpold | nuade | NO SAMPLE |  |  | 0 |
| jt | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/insects_in_space.jt | peak=7454 frames=264600 | 1 |
| jxs | bbmp_jxs | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/popcorn.jxs | peak=10493 frames=264600 | 1 |
| kdm | bbmp_kdm | PLAY | tests/m3/fixtures/.cache/kdmsong.kdm | peak=32445 frames=264600 | 2 |
| kh | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/kris-hatlelid/fiendish freddys - songs.kh | peak=17969 frames=264600 | 2 |
| kim | nuade | PLAY | <scratch>/dl/kim/kim.mod | peak=19427 frames=264600 | 1 |
| ksm | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Ken's AdLib Music/Ken Silverman/ken's labyrinth - wingame0.ksm | peak=10074 frames=264600 | 1 |
| kss | gme,libkss | FAIL | ~/Code/DEViLBOX/public/data/songs/kss/gradius 2.kss | silent: 264600 frames all zero | 2 |
| kt | klystron | PLAY | tests/m3/fixtures/.cache/ringmod.kt | peak=11422 frames=264600 | 1 |
| laa | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/LucasArts/Michael Land/dott - chron-o-john station2.laa | peak=16689 frames=264600 | 1 |
| lds | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Loudness Sound System/Peter Varga/fuzzy's world - fuzzya.lds | peak=11664 frames=264600 | 1 |
| lem | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Sound Interface System/Frank Klepacki/xmas lemmings - m3.lem | peak=17261 frames=264600 | 1 |
| liq | xmp | PLAY | <scratch>/dl/liq/liq.mod | peak=4896 frames=264600 | 1 |
| lme | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/leggless-music-editor/the punisher gameover.lme | peak=24576 frames=264600 | 1 |
| m | 98fmplayer | PLAY | tests/m3/fixtures/pmd_test.m | peak=9169 frames=264600 | 2 |
| m15 | openmpt | PLAY | <scratch>/dl/m15/m15.s98 | peak=9275 frames=264600 | 1 |
| m2 | 98fmplayer | PLAY | <scratch>/dl/m2/M2.BGM | peak=14492 frames=264600 | 1 |
| ma | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/music-assembler/thanatos.ma | peak=22528 frames=264600 | 1 |
| mad | adplug,bbmp_playerpro | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Mlat Adlib Tracker/Mem/enya2.mad | peak=8157 frames=264600 | 1 |
| max | nuade | PLAY | <scratch>/dl/max/max.mod | peak=13656 frames=264600 | 1 |
| mbm | libkss | PLAY | <scratch>/dl/mbm/Thing.MBM | peak=3213 frames=264600 | 1 |
| mc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/grand_national-title.mc | peak=11830 frames=264600 | 1 |
| mcmd | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/ep-trmonsterbusiness.mcmd | peak=7027 frames=264600 | 1 |
| mcmd_org | nuade | NO SAMPLE |  |  | 0 |
| mco | nuade | PLAY | <scratch>/dl/mco/mco.overlander | peak=9573 frames=264600 | 1 |
| mcr | nuade | FAIL | <scratch>/dl/mcr/mcr.aquablast | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| md | nuade | FAIL | third_party/furnace/extern/SAASound/README.MD | rc=1 error no_replayer /Users/spot/Code/MusicPlayer/third_party/furnace/extern/SAASoun | 1 |
| mdi | adplug | NO SAMPLE |  |  | 0 |
| mdl | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/mayday.mdl | peak=10277 frames=264600 | 1 |
| mdx | mdxmini | PLAY | ~/Code/DEViLBOX/public/data/songs/mdx/1943kaia.mdx | peak=24000 frames=264600 | 1 |
| mdy | adplug | NO SAMPLE |  |  | 0 |
| med | openmpt | FAIL | <scratch>/dl/med/j2.med | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| mg | nuade | NO SAMPLE |  |  | 0 |
| mgb | bbmp_mgb | PLAY | <scratch>/dl/mgb/chipmunks.mgb | peak=13646 frames=264600 | 1 |
| mgs | libkss | PLAY | tests/m3/fixtures/kss_test.mgs | peak=3781 frames=264600 | 1 |
| mgt | xmp | PLAY | tests/m3/fixtures/xmp_test.mgt | peak=16384 frames=264600 | 1 |
| mii | nuade | NO SAMPLE |  |  | 0 |
| miniqsf | qsf | PLAY | tests/m3/fixtures/.cache/mm1_title.miniqsf | peak=7617 frames=264600 | 1 |
| minisnsf | snsf | NO SAMPLE |  |  | 0 |
| mk2 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/mark-ii/astarrsonix.mk2 | peak=13212 frames=264600 | 1 |
| mkf | adplug | NO SAMPLE |  |  | 0 |
| mkii | nuade | NO SAMPLE |  |  | 0 |
| mkiio | nuade | NO SAMPLE |  |  | 0 |
| mkj | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/MK-Jamz/_unknown/mobydick.mkj | peak=7508 frames=264600 | 1 |
| ml | bbmp_musicline | PLAY | ~/Code/DEViLBOX/public/data/songs/musicline-editor/drax.ml | peak=15437 frames=264600 | 1 |
| mm4 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/easystyleofmsx.mm4 | peak=13312 frames=264600 | 1 |
| mm8 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/house2.mm8 | peak=3965 frames=264600 | 1 |
| mmcmp | openmpt | NO SAMPLE |  |  | 0 |
| mmdc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/octamed-mmdc/knightmare - ingame 3.mmdc | peak=9792 frames=264600 | 1 |
| mms | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/bottle popper.mms | peak=29587 frames=264600 | 1 |
| mo3 | openmpt | PLAY | <scratch>/dl/mo3/life.mo3 | peak=2514 frames=264600 | 1 |
| mod | openmpt,sid | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/shortsong1.mod | peak=29311 frames=264600 | 1 |
| mod_adsc4 | nuade | NO SAMPLE |  |  | 0 |
| mok | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/silmarils/bostonbombclub.mok | peak=20615 frames=264600 | 2 |
| mon | bbmp_monotone,nuade | PLAY | tests/m3/fixtures/.cache/understa.mon | peak=7000 frames=264600 | 1 |
| mon_old | nuade | NO SAMPLE |  |  | 0 |
| mp2 | minimp3 | FAIL | <scratch>/dl/mp2/mp2.usflib | rc=1 error no_replayer /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| mp3 | minimp3 | PLAY | tests/m3/stream/sine.mp3 | peak=2757 frames=264600 | 1 |
| mpk | libkss | PLAY | <scratch>/dl/mpk/Super Hot Ice.MPK | peak=13316 frames=264600 | 1 |
| mptm | openmpt | FAIL | third_party/libopenmpt/test/test.mptm | silent: 264600 frames all zero | 1 |
| msc | adplug | NO SAMPLE |  |  | 0 |
| mso | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/medley/combat.mso | peak=8263 frames=264600 | 1 |
| mt2 | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/mad-tracker-2/finde somewhere to hide.mt2 | peak=6078 frames=264600 | 1 |
| mth | nuade | NO SAMPLE |  |  | 0 |
| mtk | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/MPU-401 Trakker/Subz3ro/waswars.mtk | peak=11301 frames=264600 | 1 |
| mtm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/anonymous in 4ce.mtm | peak=9162 frames=264600 | 1 |
| mtr | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Master Tracker/Kane Wood/sergius...stay with.mtr | peak=12647 frames=264600 | 1 |
| mug | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/digital-mugician-2/snickle.mug | peak=29546 frames=264600 | 1 |
| mug2 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/snickle.mug2 | peak=29546 frames=264600 | 1 |
| mus | adplug,bbmp_adlib,bbmp_fac,nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/MUS/Nick Jones/first samurai.mus | peak=10337 frames=264600 | 2 |
| musx | xmp | PLAY | <scratch>/dl/musx/Acid.musx | peak=32443 frames=264600 | 1 |
| mw | nuade | NO SAMPLE |  |  | 0 |
| mxm | bbmp_mxm | PLAY | tests/m3/fixtures/.cache/pulse-sink.mxm | peak=1013 frames=264600 | 1 |
| mxtx | bbmp_maxtrax | PLAY | ~/Code/DEViLBOX/public/data/songs/maxtrax/contraptionzack-march.mxtx | peak=19983 frames=264600 | 1 |
| mz | 98fmplayer | NO SAMPLE |  |  | 0 |
| ned | bbmp_ned | FAIL | ~/.cache/selecta/files/modland/pub/modules/NerdTracker 2/Memblers/nudrom1.ned | rc=1 error open_failed /Users/spot/.cache/selecta/files/modland/pub/modules/NerdTracke | 7 |
| nsf | gme | PLAY | tests/dui/fixtures/subsongs3.nsf | peak=19029 frames=264600 | 1 |
| nsfe | gme | NO SAMPLE |  |  | 0 |
| nst | openmpt | NO SAMPLE |  |  | 0 |
| ntk | bbmp_ptk | PLAY | tests/m3/fixtures/.cache/unionjack.ntk | peak=8618 frames=264600 | 1 |
| ogg | stb_vorbis | PLAY | tests/m3/stream/exact.ogg | peak=2928 frames=264600 | 1 |
| okt | nuade,openmpt | PLAY | <scratch>/dl/okt/beatlesound 8-ch.okt | peak=12831 frames=264600 | 1 |
| okta | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/45.okta | peak=15998 frames=264600 | 1 |
| omf | bbmp_onyx | PLAY | tests/m3/fixtures/.cache/battle_squadron.omf | peak=32768 frames=264600 | 1 |
| one | nuade | PLAY | <scratch>/dl/one/One.sid | peak=17691 frames=264600 | 1 |
| opi | 98fmplayer | PLAY | ~/.cache/selecta/files/modland/pub/modules/FM sound driver (FMP)/VDB (Bentoya)/Super Mario - 4 BGM.OPI | peak=2693 frames=264600 | 1 |
| opx | libkss | PLAY | <scratch>/dl/opx/Crisis.OPX | peak=4669 frames=264600 | 1 |
| org | organya | PLAY | tests/m3/fixtures/org_test.org | peak=32768 frames=264600 | 1 |
| osp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/synth-pack/centerbase soft.osp | peak=13627 frames=264600 | 2 |
| ovi | 98fmplayer | PLAY | ~/.cache/selecta/files/modland/pub/modules/FM sound driver (FMP)/Mahjong Story/Metroid/Items.OVI | peak=10268 frames=264600 | 1 |
| oxm | openmpt | NO SAMPLE |  |  | 0 |
| ozi | 98fmplayer | PLAY | ~/.cache/selecta/files/modland/pub/modules/FM sound driver (FMP)/Kabaki/Super Mario 64 - Invincible State.OZI | peak=2232 frames=264600 | 1 |
| pac | bbmp_sbstudio | PLAY | tests/m3/fixtures/.cache/silvermoon.pac | peak=22223 frames=264600 | 1 |
| pap | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/formats/Ivanhoe_ivanhoe - hiscore.pap | rc=1 error open_failed /Users/spot/Code/DEViLBOX/public/data/songs/formats/Ivanhoe_iva | 3 |
| pis | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Beni Tracker/The Hardliner/bentroit.pis | peak=4028 frames=264600 | 1 |
| plm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/parity_error.plm | peak=13179 frames=264600 | 1 |
| plx | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Palladix/Jochen Hess/logical - 50fa0.plx | peak=4203 frames=264600 | 1 |
| pmd | bbmp_piece | PLAY | tests/m3/fixtures/.cache/prelude.pmd | peak=1431 frames=264600 | 1 |
| pn | nuade | FAIL | <scratch>/dl/pn/pn.tetris | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| powt | nuade | NO SAMPLE |  |  | 0 |
| ppm | openmpt | NO SAMPLE |  |  | 0 |
| ps | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/jug-hiscore.ps | peak=22405 frames=264600 | 2 |
| psa | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/the punisher - title.psa | peak=18816 frames=264600 | 1 |
| psf | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/soundfactory/im maien.psf | peak=25462 frames=264600 | 1 |
| psid | sid | NO SAMPLE |  |  | 0 |
| psm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/Epic Pinball - Song 0 - Title.psm | peak=18815 frames=264600 | 1 |
| psy | bbmp_psycle | PLAY | tests/m3/fixtures/.cache/acidate.psy | peak=9900 frames=264600 | 1 |
| pt | nuade | PLAY | <scratch>/dl/pt/graphics are none.pt | peak=14336 frames=264600 | 1 |
| pt36 | openmpt | NO SAMPLE |  |  | 0 |
| ptcop | pxtone | PLAY | ~/Code/DEViLBOX/public/data/songs/piston-collage/obj1263-1.ptcop | peak=8136 frames=264600 | 1 |
| ptk | bbmp_ptk | PLAY | tests/m3/fixtures/.cache/wonderland.ptk | peak=32767 frames=264600 | 1 |
| ptm | nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/rew_vibr.ptm | peak=22736 frames=264600 | 1 |
| pttune | pxtone | PLAY | ~/Code/DEViLBOX/public/data/songs/piston-collage-protected/obj0983-1.pttune | peak=32767 frames=264600 | 1 |
| puma | nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/pumatracker/liquid kids - lv1a.puma | peak=15746 frames=264600 | 1 |
| pvp | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/zarathrusta title.pvp | peak=8779 frames=264600 | 1 |
| qc | nuade | NO SAMPLE |  |  | 0 |
| qpa | nuade | PLAY | <scratch>/dl/qpa/qpa.mod | peak=19604 frames=264600 | 1 |
| qsf | qsf | PLAY | tests/m3/fixtures/qsf_test.qsf | peak=22616 frames=264600 | 1 |
| rac | adplug | NO SAMPLE |  |  | 0 |
| rad | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Reality AdLib Tracker/_unknown/rip4.rad | peak=12386 frames=264600 | 1 |
| raw | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Raw OPL Capture/Ken Allen/jones in the fastlane - no job.raw | peak=1883 frames=264600 | 1 |
| rh | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/centurion_battle.rh | peak=17575 frames=264600 | 1 |
| rho | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/battleships.rho | peak=16512 frames=264600 | 1 |
| riff | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/riffraff_smallest.riff | peak=16183 frames=264600 | 1 |
| rix | adplug | NO SAMPLE |  |  | 0 |
| rj | nuade | PLAY | <scratch>/dl/rj/RJ.sid | peak=9627 frames=264600 | 1 |
| rjp | nuade | NO SAMPLE |  |  | 0 |
| rk | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/astra_2.rk | peak=24191 frames=264600 | 1 |
| rkb | nuade | NO SAMPLE |  |  | 0 |
| rol | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Visual Composer/_unknown/messedup.rol | peak=2038 frames=264600 | 1 |
| rtm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/odyssey.rtm | peak=9693 frames=264600 | 1 |
| s-c | nuade | NO SAMPLE |  |  | 0 |
| s3m | openmpt | PLAY | third_party/libopenmpt/test/test.s3m | peak=1224 frames=264600 | 1 |
| s7g | nuade | NO SAMPLE |  |  | 0 |
| s98 | libvgm | PLAY | ~/Code/DEViLBOX/public/data/songs/s98/hiouden 10.s98 | peak=12067 frames=264600 | 1 |
| sa | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/almighty.sa | peak=28200 frames=264600 | 1 |
| sa2 | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Surprise! AdLib Tracker 2.0/Erik/synthsong.sa2 | peak=18436 frames=264600 | 1 |
| sap | gme | PLAY | ~/Code/DEViLBOX/public/data/songs/sap/chop suey.sap | peak=29641 frames=264600 | 1 |
| sas | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/speedy-a1-system/digital voyage.sas | peak=16320 frames=264600 | 1 |
| sat | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Surprise! AdLib Tracker/Erik/synthsong.sat | peak=18436 frames=264600 | 1 |
| sb | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/street_fighter.sb | peak=17220 frames=264600 | 1 |
| sc | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/north_sea_inferno.sc | peak=9487 frames=264600 | 1 |
| sc68 | sc68 | PLAY | ~/Code/DEViLBOX/public/data/songs/sc68/aprentice title.sc68 | peak=13743 frames=264600 | 1 |
| sci | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Sierra/Ken Allen/Kings Quest 1/kq1 flutey.sci | peak=4079 frames=264600 | 1 |
| scn | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/surf_ninjas.scn | peak=15134 frames=264600 | 1 |
| scr | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/count_duckula.scr | peak=10287 frames=264600 | 1 |
| sct | nuade | NO SAMPLE |  |  | 0 |
| scumm | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/zaktitle.scumm | peak=25990 frames=264600 | 1 |
| sd1 | nuade | NO SAMPLE |  |  | 0 |
| sd2 | nuade | PLAY | <scratch>/dl/sd2/sd2.mod | peak=22497 frames=264600 | 1 |
| sdata | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/MusicMaker V8 Old/- unknown/moveback.sdata | peak=14810 frames=264600 | 1 |
| sdb | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Herad Music System/Stephane Picq/dune - water.sdb | peak=18036 frames=264600 | 1 |
| sdr | nuade | FAIL | <scratch>/dl/sdr/sdr.nobuddiesland 1 | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| sfx | nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/operation_stealth.sfx | peak=16128 frames=264600 | 1 |
| sfx2 | openmpt | PLAY | <scratch>/dl/sfx2/sfx2.dmf | peak=8135 frames=264600 | 1 |
| sg | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/irrepressible intro.sg | peak=24503 frames=264600 | 1 |
| sgc | bbmp_nez | PLAY | <scratch>/dl/sgc/Ryu Kyu.sgc | peak=2556 frames=264600 | 1 |
| sho | bbmp_shroom | FAIL | tests/m3/fixtures/.cache/netke.sho | rc=1 error open_failed tests/m3/fixtures/.cache/netke.sho | 2 |
| sid | nuade,sid | PLAY | tests/m3/fixtures/sid_basic.sid | peak=11839 frames=264600 | 1 |
| sid1 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/anarchy.sid1 | peak=6604 frames=264600 | 1 |
| sid2 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/sidmon-2/ice7-intro.sid2 | peak=20623 frames=264600 | 1 |
| sig | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Sound Images Generation 2/Allister Brimble/dizzy kwik snax.sig | peak=3151 frames=264600 | 1 |
| sj | nuade | NO SAMPLE |  |  | 0 |
| sjs | nuade | FAIL | <scratch>/dl/sjs/sjs.jb | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| skm | bbmp_skale,openmpt | PLAY | tests/m3/fixtures/.cache/s4.skm | peak=32768 frames=264600 | 1 |
| sm | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/rackney'sisland_4.sm | peak=12360 frames=264600 | 1 |
| sm1 | nuade | FAIL | tests/m3/fixtures/.cache/2NDR-V.SM1 | rc=1 error no_replayer tests/m3/fixtures/.cache/2NDR-V.SM1 | 1 |
| sm2 | nuade | FAIL | tests/m3/fixtures/.cache/2NDR-V.SM2 | rc=1 error no_replayer tests/m3/fixtures/.cache/2NDR-V.SM2 | 1 |
| sm3 | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/sound-master-ii-v3/doofus 3.sm3 | peak=11833 frames=264600 | 1 |
| smod | nuade,openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/adept.smod | peak=22448 frames=264600 | 1 |
| smpro | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/sound-master-ii-v1/futureshock-level 3.smpro | peak=17097 frames=264600 | 1 |
| smufi | bbmp_smufi | PLAY | tests/m3/fixtures/.cache/danceordie.smufi | peak=15907 frames=264600 | 1 |
| smus | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/iff-smus/Chris Grigg/California Games/jaws.smus | peak=12083 frames=264600 | 1 |
| sn | bbmp_sclub | PLAY | tests/m3/fixtures/.cache/xtc.sn | peak=15778 frames=264600 | 1 |
| sn2 | bbmp_sclub | PLAY | tests/m3/fixtures/.cache/country_funk.sn2 | peak=20363 frames=264600 | 1 |
| snd | bbmp_wwsnd | PLAY | ~/Code/DEViLBOX/public/data/songs/sndh/husak.jak/fractal.snd | peak=10313 frames=264600 | 1 |
| sndh | psgplay,sc68 | PLAY | tests/fixtures/sndh/oculus_ice.sndh | peak=16413 frames=264600 | 1 |
| sng | adplug,bbmp_smx,bbmp_synder,goattracker,nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/goat-tracker-ultra/Jammer/$3GarysGlitteringSaliva_4x.sng | peak=23694 frames=264600 | 2 |
| snk | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/everton_fc_-_jingles.snk | peak=14432 frames=264600 | 1 |
| snsf | snsf | PLAY | tests/m3/fixtures/snsf_test.snsf | peak=32767 frames=264600 | 1 |
| snx | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/sonix/snx/Rise Of The Dragon/hand mutate.snx | peak=15326 frames=264600 | 1 |
| soc | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/hippel-st-coso/ghostbattle titletune.soc | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/hippel-st-coso/ghos | 1 |
| sonic | nuade | PLAY | <scratch>/dl/sonic/sonic.dmf | peak=18227 frames=264600 | 1 |
| sop | adplug | NO SAMPLE |  |  | 0 |
| spc | gme | PLAY | ~/.cache/selecta/files/modland/pub/modules/Nintendo SPC/_unknown/Thoroughbred Breeder II/thoroughbred breeder ii (superhorse).spc | peak=27167 frames=264600 | 1 |
| spl | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/boulderdash.spl | peak=2488 frames=264600 | 1 |
| spm | stonetracker | FAIL | ~/Code/DEViLBOX/public/data/songs/stonetracker/hypnosphere.spm | rc=1 error open_failed /Users/spot/Code/DEViLBOX/public/data/songs/stonetracker/hypnos | 2 |
| spu | bbmp_spu | PLAY | tests/m3/fixtures/.cache/dezaemon17.spu | peak=19958 frames=264600 | 1 |
| sqx | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Herad Music System/Stephane Picq/kgb - burokrat.sqx | peak=4085 frames=264600 | 1 |
| ss | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/sonix/snx/A-10 Tank Killer/instruments/clap.ss | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/sonix/snx/A-10 Tank | 7 |
| st | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/sawteeth/okolaNUKE.st | rc=1 error no_replayer /Users/spot/Code/DEViLBOX/public/data/songs/sawteeth/okolaNUKE. | 2 |
| st26 | openmpt | NO SAMPLE |  |  | 0 |
| stk | openmpt | NO SAMPLE |  |  | 0 |
| stm | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/slideshow i.stm | peak=7939 frames=264600 | 1 |
| stp | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/noname.stp | peak=12542 frames=264600 | 1 |
| strc | bbmp_ay | PLAY | ~/Code/DEViLBOX/public/data/songs/ay-strc/mega mix 1.strc | peak=30392 frames=264600 | 1 |
| stx | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/futurebrain.stx | peak=9996 frames=264600 | 1 |
| sun | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/msx-cracktro.sun | peak=20748 frames=264600 | 1 |
| sunvox | sunvox,sunvox_lib | PLAY | tests/m3/fixtures/.cache/sunvox_tune.sunvox | peak=32768 frames=264600 | 1 |
| symmod | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/symphonie/pas 2 jade.symmod | peak=9805 frames=264600 | 1 |
| syn | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/space_sound.syn | peak=30552 frames=264600 | 1 |
| synmod | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/syntracker/under the stars.synmod | peak=15266 frames=264600 | 1 |
| t0ast | bbmp_t0ast | PLAY | tests/m3/fixtures/.cache/lost_in_time.t0ast | peak=10161 frames=264600 | 1 |
| tcb | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/tcb-tracker/m-demo3 3.tcb | peak=12228 frames=264600 | 1 |
| tf | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/follin_smallest.tf | peak=8860 frames=264600 | 1 |
| tfm | furnace | NO SAMPLE |  |  | 0 |
| thm | nuade | PLAY | <scratch>/dl/thm/THM.sid | peak=15560 frames=264600 | 1 |
| tiny | nuade | FAIL | ~/Code/DEViLBOX/public/data/songs/sonix/tiny/The Games - Winter Edition/france.tiny | silent: 264600 frames all zero | 7 |
| tme | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/64_conversion.tme | peak=15033 frames=264600 | 1 |
| tmk | nuade | NO SAMPLE |  |  | 0 |
| tpu | nuade | FAIL | <scratch>/dl/tpu/tpu.timelock title | rc=1 error open_failed /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/ed7142ab-f | 1 |
| trc | nuade | NO SAMPLE |  |  | 0 |
| tro | nuade | NO SAMPLE |  |  | 0 |
| tron | nuade | PLAY | <scratch>/dl/tron/tron.mod | peak=27826 frames=264600 | 1 |
| tronic | nuade | PLAY | <scratch>/dl/tronic/tronic.mod | peak=11587 frames=264600 | 1 |
| tsd | bbmp_tss | PLAY | tests/m3/fixtures/.cache/tss_galg_title.tsd | peak=30000 frames=264600 | 1 |
| tss | bbmp_tss | PLAY | tests/m3/fixtures/.cache/tss_gradius_st1.tss | peak=27073 frames=264600 | 1 |
| ttt | tiatracker | PLAY | tests/m3/fixtures/tia_test.ttt | peak=15338 frames=264600 | 1 |
| tw | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/blinkys scary school-sfx.tw | peak=10902 frames=264600 | 1 |
| two | nuade | PLAY | <scratch>/dl/two/Two.sid | peak=9131 frames=264600 | 1 |
| ult | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/seasons.ult | peak=5580 frames=264600 | 1 |
| umx | openmpt | NO SAMPLE |  |  | 0 |
| uni | bbmp_unitrk | PLAY | tests/m3/fixtures/.cache/cuatra.uni | peak=18916 frames=264600 | 1 |
| unic | openmpt | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/african dreams.unic | peak=18789 frames=264600 | 1 |
| vge | bbmp_vgemm | PLAY | tests/m3/fixtures/.cache/example_fm_psg_samples.vge | peak=32768 frames=264600 | 1 |
| vgm | libvgm | PLAY | ~/Code/DEViLBOX/public/data/songs/vgm/BeyondSN.vgm | peak=25694 frames=264600 | 1 |
| vgz | libvgm | PLAY | ~/Code/DEViLBOX/public/data/songs/vgm/03 title screen.vgz | peak=15484 frames=264600 | 1 |
| vib | bbmp_adlib | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Vibrants/JO/copper.vib | peak=6487 frames=264600 | 1 |
| vss | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/voodoo/voo8.vss | peak=21334 frames=264600 | 1 |
| vt | bbmp_victracker | PLAY | tests/m3/fixtures/.cache/mystic.vt | peak=12072 frames=264600 | 1 |
| wb | nuade | PLAY | ~/Code/DEViLBOX/public/data/songs/formats/wicked.wb | peak=4124 frames=264600 | 1 |
| wlf | adplug | NO SAMPLE |  |  | 0 |
| wow | openmpt | PLAY | <scratch>/dl/wow/Wow.sndh | peak=20567 frames=264600 | 1 |
| wsr | bbmp_wsr | PLAY | tests/m3/fixtures/.cache/chaosgear.wsr | peak=22944 frames=264600 | 1 |
| xad | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Exotic AdLib/Void (AU)/mystical runes bbstro tune.xad | peak=12408 frames=264600 | 1 |
| xm | openmpt | PLAY | ~/.cache/selecta/files/keygenmusic/!Others/RED - Swiftdogs PC Thrust 1.x.xx.2009 crk.xm | peak=13823 frames=264600 | 2 |
| xmf | openmpt | PLAY | <scratch>/dl/xmf/max.xmf | peak=14845 frames=264600 | 1 |
| xms | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/AMusic XMS/Subz3ro/later for you.xms | peak=14320 frames=264600 | 1 |
| xpk | openmpt | NO SAMPLE |  |  | 0 |
| xsm | adplug | PLAY | ~/Code/DEViLBOX/public/data/songs/Ad Lib/Extra Simple Music/_unknown/easy1.xsm | peak=10230 frames=264600 | 1 |
| ym | ym | PLAY | ~/Code/DEViLBOX/public/data/songs/ym/17th.ym | peak=30019 frames=264600 | 1 |
| zm | nuade | NO SAMPLE |  |  | 0 |

## I1242a triage

Method: per failing extension, a known-good sample (Modland copy of library db, DEViLBOX data, openmpt fixtures) rendered 6 s through `selecta-cli render ... --config-dir ~/.config/selecta`, WAV peak checked. Harness gap: the audit ran without `--config-dir`, so replayers declaring system files (bbmp_ned, bbmp_shroom, bbmp_cybertracker, stonetracker) failed open; engine.c sysfiles_ready (~2766).

Counts (28): BUG 3 (adl, bds, gray) + 1 gap (st) + 1 unverified remote-companion issue; BAD SAMPLE / harness 18; NO GOOD SAMPLE 6.

| ext | verdict | sample used | cause |
|---|---|---|---|
| adl | BUG | Modland Westwood dune ii - 08/17.adl, eob2 - azure.adl | plugins/replayers/bbmp_wwsnd/adapter.c:37-44 rp_probe returns SEL_PROBE_YES for any file with word 1000 (or 300/500/600) at offset 120, no extension check. Westwood ADL v2/v3 has program offset 1000 there, so wwsnd wins over adplug (extension match) and rp_open fails. Only sel_adplug plus sel_bbmp_wwsnd bisect confirms; adplug alone plays it (peak non-zero). |
| bds | BUG (cause not found) | terramex.bds, chubbygristle.bds | nuade Ben Daglish player runs (ticks=251 voice_ops=2268) but WAV peak 0. Unverified cause. |
| gray | BUG (cause not found) | victoryroad.gray, stargoose.gray | nuade fredgray runs, peak 0, mixed_frames below total (133632 of 264600). Unverified cause. |
| st | BUG (missing replayer) | DEViLBOX okolaNUKE.st (Sawteeth) | no Sawteeth replayer in plugins/ or third_party/nuade/players (grep). |
| dsr | BAD SAMPLE | Modland Desire powerdrive 1.dsr (peak 17221) | batmanreturns.dsr silent only |
| kss | BAD SAMPLE | outrun.kss (5959) | gradius 2.kss |
| mptm | BAD SAMPLE | libopenmpt test.mptm (13328) | |
| md | BAD SAMPLE | die hard ii.md (16180) | README.MD |
| ss | BAD SAMPLE | DEViLBOX pauker_rap_2.ss (16256) | audit used a Sonix instrument |
| dsm | BAD SAMPLE | rollin - reincarnation.dsm (23382) | dsm.pdx |
| med | BAD SAMPLE | MED.*.mmd2 via openmpt (9585) | j2.med |
| jpn | BAD SAMPLE | smp.empiresoccer94 + jpn.empiresoccer94 (11077) | needs companion smp.<name> |
| pap | BAD SAMPLE | smp.set + ivanhoe - level5 (castle).pap (12858) | needs smp.set; pierreadane/pa_module.c:202-240 |
| mcr | BAD SAMPLE | mcs.aquablast + mcr (11299) | markcooksey_old.c:53 prefix mcs. |
| pn | BAD SAMPLE | pn.jetsetwilly.info + song (9316) | pokeynoise.c:30-51 needs .info |
| sdr | BAD SAMPLE | smp.nobuddiesland end 1 + sdr (24257) | synthdream/sdr_module.c:16,240 |
| sjs | BAD SAMPLE | sjs.beasti + smp.beasti (8229) | scottjohnston/sj_samples.c:29,209 |
| tpu | BAD SAMPLE | tpu.timelock title + smp (16683) | needs smp.<name> |
| ned | BAD SAMPLE (harness) | benny hill theme.ned (10940), zanac (8290) with --config-dir | audit lacked --config-dir |
| tiny | BAD SAMPLE (unverified) | DEViLBOX sonix/tiny/It Came From The Desert/end.tiny: peak 0 | Sonix loads instruments beside the song (sonix_nuade.c:73); instruments absent in the copied folder. Not re-tested with instruments. |
| ay | BAD SAMPLE (unverified) | none | gme and bbmp_ay both claim ay; audit sample was ay.pt3 (a PT3). No real .ay rendered (zxart not downloaded). |
| ct | NO GOOD SAMPLE | Modland Cybertracker wolfentanz.ct | system_file_missing bbmp_cybertracker:cybertracker101.d64 not installed in config dir (setup). |
| sho | NO GOOD SAMPLE | none | system_file_missing bbmp_shroom:mariopants-notes.wav (setup). |
| spm | NO GOOD SAMPLE | none | system_file_missing stonetracker:StonePlayer_Hard.bin (setup). |
| sm1, sm2 | NO GOOD SAMPLE | Modland FAC SoundTracker .SM1/.SM2 | those are FAC drum kits (bbmp_fac adapter.c:5), nuade soundmaster claims sm1/sm2 (soundmaster_nuade.c:90) but no Sound Master sample in db. |
| soc | NO GOOD SAMPLE | Modland jambala end.soc (COSO header) | .soc routes to Wanted Team (hippel_nuade.c:13-31) which refuses COSO; as .hipc it parses but peak 0. No true Wanted Team sample. |
| mp2 | NO GOOD SAMPLE | none | minimp3 claims mp2 (adapter.c:453); no real MPEG layer 2 file found. |

Counts check: rows above give BUG 4 incl. st, BAD SAMPLE 17 (dsr kss mptm md ss dsm med jpn pap mcr pn sdr sjs tpu ned tiny ay), NO GOOD SAMPLE 8 rows (ct sho spm sm1 sm2 soc mp2 and ct) = 7 extensions listed as 8 names with sm1/sm2; total 4 + 17 + 7 = 28 extensions.

Unverified open issue: plugins/replayers/nuade/adapter.c:603 `g_companions = "mdat.*>smpl.*;*.tfx>*.sam"` and the rp_content_companions hook (548-600, IFF SMUS only) mean a remote Modland key of jpn, tpu, sdr, sjs, mcr, pn or pap would not fetch its smp.*/mcs.*/pn.*.info/smp.set companion. Not tested through a remote key.

## I1242h online audit

Question (Johan): did the audit load the songs from their online sources? The first audit did not; this one does.

Method: `selecta-cli render "<source key>" --output <wav> --seconds 6 --resample linear --config-dir <copy of ~/.config/selecta> --db <empty throwaway db>`, `SELECTA_CACHE_DIR` = a fresh empty dir, `SELECTA_PLUGIN_PATH` = build/sdl/modules. The CLI CAN play a source key, but only when a library is open (engine.c:2120 fetches via sel_library_fetch only when `e->lib != NULL`); `--db` opens one, without `--db` a key is not fetched (no fetch, no companion rules). So the fetch, cache and companion-rule path (sel_companion_rules) was exercised for real, sequential, 0.4 s between songs. Per extension up to 3 songs spread across the Modland catalogue (library db copy), stop at the first that plays; PLAY = WAV peak above 0. Then the same for every other source that carries a claimed extension (hvsc, asma, sndh, zxart, aminet, keygenmusic; hippo and modulespl carried none of the 392 extensions as song keys). Resumable TSV (source, ext, tries, key, verdict, replayer, detail, seconds): `<scratch>/oa/verdicts.tsv`, driver `<scratch>/oa/bin/run.py` (skips recorded rows). Not part of the tree; summary below is the record.

Counts, Modland (392 extensions): PLAY 275, FAIL 36, HANG 3, NO SAMPLE 78 (no song with that extension or prefix in the catalogue). Local audit: PLAY 296, FAIL 28, NO SAMPLE 68. Other sources (99 ext/source pairs): PLAY 89, FAIL 10.

Cause classes of the online FAILs and HANGs:
- companion missing (fetch got the song, not its smp.*/mcs.*/info/smp.set companions): jpn, mcr, pap, pn, sdr, sjs, tpu. Root cause: nuade declares only `mdat.*>smpl.*;*.tfx>*.sam` plus a content hook for SMUS (plugins/replayers/nuade/adapter.c:603, :698), so the cache never fetches these companions. Confirmed in the cache dir: only `jpn.*` fetched, no `smp.*` beside it. New fix row.
- system file missing (setup, same as local): ct, sho, spm.
- replayer: no replayer claims the file online either (ast, ay, bds, max, med, mp2, mp3, psf, sm1, sm2 and zxart m mc psm sct sj stp); several are the known I1242a bad-sample/bug cases, not fetch problems.
- open failed after fetch (adl = I1242c probe bug; others need a per-case look: adsc dat dum ksm miniqsf minisnsf mus osp rol sci sdata, asma trc, zxart tfm, aminet mad).
- hang (40 s timeout): see the table.

Table: every FAIL/HANG and every Modland extension whose online verdict differs from the local audit (local column empty for non-Modland sources; NO SAMPLE rows equal to the local verdict are left out).

| source | ext | online | local audit | tries | cause | key / detail |
|---|---|---|---|---|---|---|
| modland | a2t | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | adl | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Ad Lib/ADL/Paul Mudra/eob2 - crimson1.adl error open_failed modland:pub/modules/Ad Lib/ADL/P |
| modland | adsc | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Audio Sculpture/_unknown/turtle-title.adsc error open_failed modland:pub/modules/Audio Sculpt |
| modland | ast | FAIL | PLAY | 3 | replayer: none claims the file | pub/modules/All Sound Tracker/Feilei/tiny opl3 demo.ast error no_replayer modland:pub/modules/All Sound Tr |
| modland | ay | FAIL | FAIL | 1 | replayer: none claims the file | pub/modules/Spectrum/Pro Tracker 3/Klim/ay.pt3 error no_replayer modland:pub/modules/Spectrum/Pro |
| modland | bds | FAIL | FAIL | 3 | replayer: none claims the file | es/IFF-SMUS/Scott T. Etherton/Mindroll/Instruments/bds.instr error no_replayer modland:pub/modules/IFF-SMUS/Sco |
| modland | bfc | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | bp2 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | chp | NO SAMPLE | PLAY | 0 |  |   |
| modland | cin | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | coc | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | ct | FAIL | FAIL | 3 | system file missing (setup) | pub/modules/Cybertracker C64/Pater Pi/wolfentanz.ct error system_file_missing bbmp_cybertracker:cybert |
| modland | dat | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | odules/Paul Robotham/Paul Robotham/Space 1889/space 1889.dat error open_failed modland:pub/modules/Paul Robotha |
| modland | dl_deli | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | dlm1 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | dmu2 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | dsm | PLAY | FAIL | 1 | render silent or other | d Interface Kit RIFF/Karsten Koch/flying tigers - tiger1.dsm peak=28570 |
| modland | dsr | PLAY | FAIL | 1 | render silent or other | pub/modules/Desire/Kev Bruce/powerdrive 1.dsr peak=17221 |
| modland | dum | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Infogrames/_unknown/gobliiins32.dum error open_failed modland:pub/modules/Infogrames/_ |
| modland | dwold | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | dzp | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | fc13 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | fc3 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | fc4 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | flac | NO SAMPLE | PLAY | 0 |  |   |
| modland | fur | NO SAMPLE | PLAY | 0 |  |   |
| modland | gm | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | got | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | gray | PLAY | FAIL | 3 | render silent or other | pub/modules/Impulsetracker/Silicon/gray.it peak=6536 |
| modland | ha2 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | hsp | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | hsq | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | is10 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | j2b | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | jcb | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | jcbo | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | jpn | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Jason Page/Jason Page/jpn.virocop-11 error open_failed modland:pub/modules/Jason Page/J |
| modland | jpnd | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | jpold | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | ksm | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | n's AdLib Music/Ken Silverman/ken's labyrinth - labsng28.ksm error open_failed modland:pub/modules/Ad Lib/Ken's |
| modland | kss | PLAY | FAIL | 1 | render silent or other | pub/modules/KSS/Soundwave/equinoxe7.kss peak=8265 |
| modland | max | FAIL | PLAY | 3 | replayer: none claims the file | pub/modules/Maximum Effect/Old  Tramp/MAX.AlloAllo level5 error no_replayer modland:pub/modules/Maximum Effe |
| modland | mcmd_org | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mcr | FAIL | FAIL | 2 | open failed after fetch (companion missing or bad sample) | s/Mark Cooksey Old/Mark Cooksey/mcr.mike reads comp pop quiz error open_failed modland:pub/modules/Mark Cooksey |
| modland | md | PLAY | FAIL | 1 | render silent or other | pub/modules/Mike Davies/Mike Davies/die hard ii.md peak=16180 |
| modland | mdi | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mdy | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | med | FAIL | FAIL | 3 | replayer: none claims the file | pub/modules/Music Editor/_unknown/whomp!.med error no_replayer modland:pub/modules/Music Editor |
| modland | mg | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mii | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | miniqsf | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | Capcom Q-Sound Format/_unknown/The Punisher/punish10.miniqsf error open_failed modland:pub/modules/Capcom Q-Sou |
| modland | minisnsf | FAIL | NOSAMPLE | 3 | open failed after fetch (companion missing or bad sample) | mat/Robert Prince/Wolfenstein 3D/wolfenstein3d-0003.minisnsf error open_failed modland:pub/modules/Super Ninten |
| modland | mkf | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mkii | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mkiio | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mmcmp | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mod_adsc4 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mon_old | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mp2 | FAIL | FAIL | 1 | replayer: none claims the file | tra64 Sound Format/Yasunori Mitsuda/Mario Party 2/mp2.usflib error no_replayer modland:pub/modules/Ultra64 Soun |
| modland | mp3 | FAIL | PLAY | 1 | replayer: none claims the file | tra64 Sound Format/Ichiro Shimakura/Mario Party 3/mp3.usflib error no_replayer modland:pub/modules/Ultra64 Soun |
| modland | mptm | PLAY | FAIL | 1 | render silent or other | pub/modules/OpenMPT MPTM/Kleeder/neighbourhood cool guy.mptm peak=17922 |
| modland | msc | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mth | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mug2 | NO SAMPLE | PLAY | 0 |  |   |
| modland | mus | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Stereo Sidplayer/DC/rush 2112-discovery.mus error open_failed modland:pub/modules/Stereo Sidpl |
| modland | mw | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | mz | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | ned | PLAY | FAIL | 1 | render silent or other | pub/modules/NerdTracker 2/Datschge/trance.ned peak=7199 |
| modland | nsfe | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | nst | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | ogg | NO SAMPLE | PLAY | 0 |  |   |
| modland | osp | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | les/Synth Pack/Karsten Obarski/Dyter-07/dyter07 ongame04.osp error open_failed modland:pub/modules/Synth Pack/K |
| modland | oxm | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | pap | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | e Adane Packer/Pierre-Eric Loriaux/Pang/pang - ingame 12.pap error open_failed modland:pub/modules/Pierre Adane |
| modland | pn | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/PokeyNoise/Rob Hubbard/pn.jetsetwilly error open_failed modland:pub/modules/PokeyNoise/R |
| modland | powt | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | ppm | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | psf | FAIL | PLAY | 3 | replayer: none claims the file | ation Sound Format/_unknown/Summon Night/summonknight-10.psf error no_replayer modland:pub/modules/Playstation  |
| modland | psid | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | pt36 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | qc | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | qsf | NO SAMPLE | PLAY | 0 |  |   |
| modland | rac | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | rix | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | rjp | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | rkb | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | rol | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Ad Lib/Visual Composer/_unknown/rain.rol error open_failed modland:pub/modules/Ad Lib/Visua |
| modland | s-c | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | s7g | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | sc68 | FAIL | PLAY | 3 | render silent or other | pub/modules/SC68/_unknown/guitar fever.sc68 rc=0 |
| modland | sci | FAIL | PLAY | 3 | open failed after fetch (companion missing or bad sample) | d Lib/Sierra/Mark Seibert/Conquest Of Camelot/cc1 march2.sci error open_failed modland:pub/modules/Ad Lib/Sierr |
| modland | sct | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | sd1 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | sdata | FAIL | PLAY | 2 | open failed after fetch (companion missing or bad sample) | pub/modules/MusicMaker V8 Old/_unknown/moveback.sdata error open_failed modland:pub/modules/MusicMaker V |
| modland | sdr | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | modules/Synth Dream/Laurens Tummers/sdr.nobuddiesland game 1 error open_failed modland:pub/modules/Synth Dream/ |
| modland | sho | FAIL | FAIL | 3 | system file missing (setup) | pub/modules/Shroom/Ryan Novak/untitled.sho error system_file_missing bbmp_shroom:mariopants-n |
| modland | sid1 | NO SAMPLE | PLAY | 0 |  |   |
| modland | sj | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | sjs | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/SoundPlayer/Scott Johnston/sjs.tune1 error open_failed modland:pub/modules/SoundPlayer/ |
| modland | sm1 | FAIL | FAIL | 3 | replayer: none claims the file | pub/modules/FAC SoundTracker/Robosoft/VOYAGE.SM1 error no_replayer modland:pub/modules/FAC SoundTra |
| modland | sm2 | FAIL | FAIL | 3 | replayer: none claims the file |  SoundTracker/Roderik Muit/Quasar disk magazines/DRUMKIT.SM2 error no_replayer modland:pub/modules/FAC SoundTra |
| modland | smus | HANG | PLAY | 3 | hang (40 s timeout) | pub/modules/IFF-SMUS/_unknown/Stay Home/stay home.alt.smus rc=124 |
| modland | snsf | NO SAMPLE | PLAY | 0 |  |   |
| modland | snx | NO SAMPLE | PLAY | 0 |  |   |
| modland | soc | FAIL | FAIL | 3 | replayer: none claims the file | /modules/Hippel ST COSO/Jochen Hippel/stormlord original.soc error no_replayer modland:pub/modules/Hippel ST CO |
| modland | sop | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | spm | FAIL | FAIL | 3 | system file missing (setup) | b/modules/Stonetracker/Jerome Angelot/stone or not stone.spm error system_file_missing stonetracker:StonePlayer |
| modland | ss | HANG | FAIL | 3 | hang (40 s timeout) | b/modules/IFF-SMUS/_unknown/Sleepy/Instruments/HeavyMetal.ss rc=124 |
| modland | st | HANG | FAIL | 3 | hang (40 s timeout) | pub/modules/Slight Atari Player/Andy (other)/st.sap rc=124 |
| modland | st26 | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | stk | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | stp | FAIL | PLAY | 3 | replayer: none claims the file | /Sound Tracker Pro/_unknown/blackraven2-final-demo movie.stp error no_replayer modland:pub/modules/Spectrum/Sou |
| modland | stx | NO SAMPLE | PLAY | 0 |  |   |
| modland | tfm | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | tiny | FAIL | FAIL | 3 | render silent or other | pub/modules/Protracker/Zalt/tiny.mod rc=0 |
| modland | tmk | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | tpu | FAIL | FAIL | 3 | open failed after fetch (companion missing or bad sample) | pub/modules/Dirk Bialluch/Dirk Bialluch/tpu.timelock title error open_failed modland:pub/modules/Dirk Bialluc |
| modland | trc | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | tro | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | umx | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | unic | NO SAMPLE | PLAY | 0 |  |   |
| modland | wlf | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | xpk | NO SAMPLE | NOSAMPLE | 0 |  |   |
| modland | zm | NO SAMPLE | NOSAMPLE | 0 |  |   |
| asma | smus | FAIL |  | 1 | render silent or other | Composers/Liebich_Tomasz/Smus.sap rc=0 |
| asma | trc | FAIL |  | 1 | open failed after fetch (companion missing or bad sample) | Composers/Golewski_Filip/TRC.sap error open_failed asma:Composers/Golewski_Filip/TR |
| zxart | m | FAIL |  | 3 | replayer: none claims the file | 58344/Mutant_-_SymMut_(1999).m error no_replayer zxart:58344/Mutant_-_SymMut_(199 |
| zxart | mc | FAIL |  | 2 | replayer: none claims the file | 75541/Mc.Floyd_-_X-RAY_3_(1997).pt2 error no_replayer zxart:75541/Mc.Floyd_-_X-RAY_3_( |
| zxart | psm | FAIL |  | 3 | replayer: none claims the file | 79487/Scratcher_-_Trashe_coming_soon_(1996).psm error no_replayer zxart:79487/Scratcher_-_Trashe_c |
| zxart | sct | FAIL |  | 1 | replayer: none claims the file | 417423/Gasman_-_dreamcatcher_(2008)_(Forever_9,_3).sct error no_replayer zxart:417423/Gasman_-_dreamcatch |
| zxart | sj | FAIL |  | 3 | replayer: none claims the file | 80172/SJ.Lex_-_NEDOPIS.pt3 error no_replayer zxart:80172/SJ.Lex_-_NEDOPIS.pt3 |
| zxart | stp | FAIL |  | 3 | replayer: none claims the file | 82137/Voxon_-_Blackpon.stp error no_replayer zxart:82137/Voxon_-_Blackpon.stp |
| zxart | tfm | FAIL |  | 1 | open failed after fetch (companion missing or bad sample) | usmann_-_Minimoog_r10_(2013)_(ZX_AAA_Demo_Compo_2013,_3).tfm error open_failed zxart:57609/Husmann_-_Minimoog_r |
| aminet | mad | FAIL |  | 1 | open failed after fetch (companion missing or bad sample) | mods/smpl/Mad.lha error open_failed aminet:mods/smpl/Mad.lha |

Run finished (all jobs done). The fetch cache and WAVs were deleted; the TSV stays in the session scratchpad oa/verdicts.tsv.
