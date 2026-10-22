---
date: 2026-10-22
topic: HippoPlayer feature parity checklist (ledger F13a)
tags: [parity, hippoplayer, feature-bar, selecta]
status: final
---

# HippoPlayer parity checklist

Sources (all under /Users/spot/Code/HippoPlayer-68k/code): `doc` = dist/hippoplayerupdate.doc, `rdme` = dist/hippoplayerupdate.readme (changelog v2.45-2.67), `s` = puu016.s, `g` = gadgets/prefs_sivu*.s. Selecta column maps to architecture-design sections (6 playback, 7 UI/visuals, 8 remote, 9 library/playlist, 17 playlist formats, 5 replayers, D7 ARexx) or NEW. No original HippoPlayer 2.x user guide is in the tree; the prefs pages come from the gadget files and may omit options only visible at runtime. Items marked (unverified) were seen as a string or comment only.

## Playback
| Feature | Where in source | Selecta |
|---|---|---|
| Play / restart chosen module (LMB), random module (RMB) | s:65525-65526 | 6 |
| Stop / continue (pause) | s:65533, s:25856-25857 | 6 |
| Stop and eject current module | s:65536 | 6 |
| Next / previous module | s:65539-65542 | 6 |
| Next / previous subsong | s:65563-65566 | 6 |
| Subsong remembered per favorite and per playlist entry | doc:65, rdme:812 | 9 |
| Skip forward / faster playback (LMB), even faster (RMB), toggle off by pressing again | s:65588-65591 | 6 |
| Skip backward (rewind) | s:65594 | 6 |
| Pattern skip forward/back via < > keys | s:9330 block (keys $38/$39) | 6 |
| Play modes: list repeat, list once, module repeat, module once, list random | s:65616-65621 | 6 |
| Play mode button next to position slider (cycle modes without prefs) | doc:142, rdme:367 | 7 |
| Position slider, jump to pattern position (pattern-skip formats and local MP3) | doc:138-141 | 6 |
| Song length display in title bar / slider | doc:277, rdme:376 | 6 |
| Song-end detection from length databases for formats lacking native support | doc:279-280 | 6 |
| Volume up / down | s:25862, hotkeys V/B | 6 |
| Fade volume option (file prefs) | g:prefs_sivu2 "Fade volume" | 6 |
| Filter control (Amiga LED filter) option | g:prefs_sivu2 "Filter control" | 5 (Amiga tier) |
| Nasty audio / VBlank timing options | g:prefs_sivu2 | 5 (Amiga tier) |
| Protracker replayer selection (PT replayer, PS3M alternative) and Protracker tempo mode | g:prefs_sivu2, doc:254-255 | 5 |
| Continue on error (skip unplayable module and go on) | g:prefs_sivu0 | 6 |
| Early load (preload next module) | g:prefs_sivu0 | 6 |
| Play timeout (auto-advance after N seconds) | g:prefs_sivu0 "Timeout", s:25878 | 6 |
| Alarm (timed start) | g:prefs_sivu0 "Alarm", s:3330 do_alarm | NEW |
| Startup module / startup playlist (auto-play on launch) | s:3287-3300, rdme:701 | 9 |
| Restore saved state list on start (Keep list on exit) | s:3309, doc:79 | 9 |
| Multimedia keyboard keys: stop, play/pause, prev, next, shuffle, repeat | s:9230-9235 | 6 |
| Sample files WAV, AIFF, IFF, MP3 as playable items | rdme format list | 5 |
| Mixing rate, buffer size, stereo level, volume boost (PS3M) | g:prefs_sivu4 | 6 |
| Stereo level / master volume / mixing rate for AHI | g:prefs_sivu5 | 6 |
| Surround / stereo separation control | g:prefs_sivu4 "Stereo level (surround)" | 6 |
| Doublebuffering of sample data (output) | g:prefs_sivu3 | NEW |
| MPEGA quality and frequency divider (MP3 decode quality) | g:prefs_sivu6 | 5 |
| MED output mode, MED FastMemPlay | g:prefs_sivu6, s:65119 | 5 |
| TFMX rate, sample buffer, force sample rate, 14-bit output, cybercalibration | g:prefs_sivu6 | 5 (Amiga tier) |
| Mono / dual / triple SID selection | doc:184 | 5 |
| Subsong / song-end detection for vgm2wav and mdx2wav formats | doc:234 | 5 |

## Playlist
| Feature | Where in source | Selecta |
|---|---|---|
| Main playlist, up to 131071 items | rdme:61 | 9 |
| Four list modes (main, favorites, file browser, search), toggle button, SHIFT+M, RMB popup | doc:48-54, s:65601-65606 | 9 |
| Add modules (LMB add, RMB insert after chosen) | s:65545-65546 | 9 |
| Add selected entry from browser/search to main list ([A]) | doc:103-104, s:65549 | 9 |
| Remove module (LMB), remove from list and from disk (RMB) | s:65553-65554 | 9 |
| Remove divided section via RMB on divider | s:65555-65556 | 9 |
| Clear list; clear list and add files | s:65559-65560 | 9 |
| Sort list | s:65573, s:25866 | 9 |
| Auto sort option | g:prefs_sivu0 | 9 |
| Find module / find next | s:65574-65575, doc:308-309 | 9 |
| Move module (pick up, press again to insert) | s:65578-65580 | 9 |
| Dividers (add via RMB, "Divider / dir" option adds dividers per directory) | s:65581, g:prefs_sivu0 | 9 |
| Insert module (hotkey T) | s:9330 block | 9 |
| Module programs: save / load playlist files (HiPPrg header) | s:13523, s:65584-65585 | 17 |
| Join module program ([ key) | s:9330 block | 17 |
| Function keys F1-F10 load and play stored programs | s:9330 block | NEW |
| MS-DOS line endings accepted in program files | rdme:587 | 17 |
| .m3u import (URL entries only) | rdme:487 | 17 |
| Favorites list (RMB toggles, bold display, CTRL+F, persisted in S:HippoFavorites.prg) | doc:60-66 | 9 |
| Confirmation before clearing/editing favorites | doc:67 | 9 |
| File browser: directory navigation, parent entry, BACKSPACE, filter pattern | doc:74-78 | 9 |
| Drag and drop of folders onto the window, recursive scan | doc:81-82 | 9 |
| Module comment file (hotkey O/comment) (unverified) | s:9330 block ("comment file") | NEW |
| Hide window hotkey (H) | s:9330 block | 7 |
| Select-next / select-prev without playing (numpad 8 / 2) | s:9330 block | 9 |
| Add urls from command line and ARexx | rdme:892 | 8 |
| Recent Playlists search mode | rdme:477 | 9 |
| Archives: unpack LhA, LZX, Zip, gzip on load | s:14315-14362, g:prefs_sivu3 | NEW |
| Packed modules: XPK identify, PowerPacker, XFDmaster | g:prefs_sivu3, s:64585-64592 | NEW |
| File match pattern for loading | g:prefs_sivu3, doc:78 | 9 |

## Download/Network
| Feature | Where in source | Selecta |
|---|---|---|
| Search sources: Modland, Aminet, Modules.pl, HVSC, AMP, amigaremix.com, Remix.Kwed.org, Radio stations, Shared lists | doc:93-96, s:10388-10395 | 8 |
| Turran FTP search source | rdme:810 | 8 |
| Shared (community) playlists browsing (fetch more than 25) | doc:94, rdme:785 | 8 |
| Search source remembered in prefs | doc:97 | 7 |
| Download remote file to chosen directory (CTRL+G, ESC cancels) | doc:105-107 | 8 |
| Stream MP3 directly | doc:113 | 8 |
| Internet radio stations (ICY headers) | doc:114, s:60970 | 8 |
| Infobox shows downloaded byte count | doc:117 | 7 |
| Modland whitelist filtering of extensions | rdme:751 | 8 |
| HVSC-path based STIL and song length lookups | doc:188-190 | 8 |
| Network transport via external UHCTools (UHCBIN) | doc:88, s:59589 | NEW |
| Recent playlists history | rdme:477 | 9 |

## Display/Screen
| Feature | Where in source | Selecta |
|---|---|---|
| Resizable main and info windows | doc:123-124 | 7 |
| Window shrink/expand (zip) via ~ key and ARexx ZIP | s:9330 block, s:25876 | 7 |
| Any font for list and for infobox/buttons | doc:126-128 | 7 |
| Big buttons | doc:129, s:64839 | 7 |
| Button tooltips | doc:131, s:64823 | 7 |
| Public screen selection (PUBSCREEN) | s:25877, s:5020 | NEW |
| Own screen option ("Screen" prefs) (unverified) | g:prefs_sivu1 "Screen" | NEW |
| Filebox display and zoom (RMB on prefs button zooms file box) | s:65569-65570, g:prefs_sivu1 | 7 |
| Disable info scroll | g:prefs_sivu1 | 7 |
| Info-line display modes (time, position/length, clock/memory, name) via keys 7/8/9/0 | s:9330 block | 7 |
| Module info window with scroll (mousewheel) | rdme:589 | 7 |
| Info window: SID chip info, instrument names, channel count | rdme:427, doc:206 | 7 |
| Info window: author, publisher, album, year metadata | doc:277-278 | 7 |
| Info window: STIL composer trivia | doc:188 | 7 |
| AHI indicator in infobox | rdme:777 | NEW |
| Keep window positions and open states | doc:163 | 7 |
| About window (HELP key, RMB on info button) | s:65530, s:9330 block | 7 |
| Prefs window | s:64709 | 7 |

## Scopes and info
| Feature | Where in source | Selecta |
|---|---|---|
| Quadra scope | s:64871 | 7 |
| Filled quadra scope | s:64903 | 7 |
| Hippo scope | s:64928 | 7 |
| Bars scope | s:64842 | 7 |
| Spectrum analyzer (frequency vs dB) | doc:155, s:64983 | 7 |
| Patternscope 4-18 channels, XL double-height rows, mini4 font | doc:156-159, s:64954-64969 | 7 |
| Patternscope for PSID (SID register view) | doc:160-161 | 7 |
| Several scopes simultaneously | doc:150 | 7 |
| Scope size, scope priority, scope bars settings | doc:154, doc:165, g:prefs_sivu1 | 7 |
| Toggle scopes with Z | doc:167 | 7 |
| External scope plug-ins (scopes directory) (unverified) | code/scopes | 7 |
| Module info window (I key) | s:9330 block | 7 |
| Per-sample listing (SAMPLES ARexx, samples window) | s:25883, s:23906-23909 | 7 |
| Synth sample / sample size counts for Eagleplayers | s:23906-23909 | 7 |

## Preferences
| Feature | Where in source | Selecta |
|---|---|---|
| Prefs page Play: timeout, alarm, startup, function keys, priority, hotkeys, doubleclick, continue on error, early load, divider/dir, auto sort | g:prefs_sivu0 | 7 |
| Prefs page Show: screen, filebox, play mode button and position slider, disable info scroll, fonts, scope type/bars/priority, module info | g:prefs_sivu1 | 7 |
| Prefs page File: mode, player group path, filter control, fade volume, nasty audio, VBlank timing, PT replayer, Protracker tempo | g:prefs_sivu2 | 5 |
| Prefs page Modules/Programs/Archives: LhA, LZX, Zip paths, doublebuffering, XPK, XFD, match pattern | g:prefs_sivu3 | 9 |
| Prefs page PS3M: play mode, priority, buffer, rate, boost, stereo, config file, AmiGUS mixer | g:prefs_sivu4 | 6 |
| Prefs page AHI: mode, enable AHI, disable non-AHI replayers, rate, master volume, stereo | g:prefs_sivu5 | 6 |
| Prefs page Play-1/Play-2: TFMX rate, MPEGA, MED, 14-bit etc. | g:prefs_sivu6 | 5 |
| PlaySID mode: Normal, reSID 6581, reSID 8580, reSID Auto, SIDBlaster, USBSID-Pico, ZorroSID, Trinity | doc:174-180, s:18600-18602 | 5 |
| reSID filter (Internal, Internal+Ext, All off) and oversample 2x/3x/4x | doc:182, s:18675-18677 | 5 |
| reSID boost | s:65063 | 5 |
| Enable xmaplay with Paula / AHI / AmiGUS output | doc:196-204 | 5 |
| MIDI mode: serial (delimidi), Timidity, GMPlay | doc:213-222 | 5 |
| Enable MHI (external MP3 decoder hardware) | doc:241-242, s:65078 | NEW |
| AmiGUS hardware mixer (32 channel) | doc:249-256 | NEW |
| Trinity accelerator audio (SID, AY VGM) | doc:259-265 | NEW |
| Load / save prefs (LOADPREFS) | s:25882, s:14315 | 7 |
| Favorites enable and Keep list on exit | doc:63, s:64792-64807 | 9 |
| Hotkeys enable (global hotkey) | g:prefs_sivu0 | 7 |
| Doubleclick play toggle | s:17408-17414, s:21496 | 7 |
| Task priority setting | g:prefs_sivu0 | NEW |
| HippoPlayer.group location setting | doc:35-36 | NEW |

## ARexx
Port name "HippoPlayer" (s:2461). Command table at s:25820-25885.
| Feature | Where in source | Selecta |
|---|---|---|
| PLAY | s:25854 | D7 |
| CLEAR | s:25855 | D7 |
| CONT | s:25856 | D7 |
| STOP | s:25857 | D7 |
| EJECT | s:25858 | D7 |
| LOADPRG | s:25859 | D7 |
| ADD | s:25860 | D7 |
| DEL | s:25861 | D7 |
| VOLUME | s:25862 | D7 |
| REW | s:25863 | D7 |
| FFWD | s:25864 | D7 |
| MOVE | s:25865 | D7 |
| SORT | s:25866 | D7 |
| INSERT | s:25867 | D7 |
| QUIT | s:25868 | D7 |
| CHOOSE | s:25869 | D7 |
| CHOOSENEXT | s:25870 | D7 |
| CHOOSEPREV | s:25871 | D7 |
| SONGPLAY | s:25872 | D7 |
| GET | s:25873 | D7 |
| RANDPLAY | s:25874 | D7 |
| HIDE | s:25875 | D7 |
| ZIP | s:25876 | D7 |
| PUBSCREEN | s:25877 | D7 |
| TIMEOUT | s:25878 | D7 |
| PS3MMODE | s:25879 | D7 |
| PS3MBOOST | s:25880 | D7 |
| PS3MRATE | s:25881 | D7 |
| LOADPREFS | s:25882 | D7 |
| SAMPLES | s:25883 | D7 |
| TOGGLEMODE | s:25884 | D7 |
| FAVORITE | s:25885 | D7 |

## ToolTypes/CLI
| Feature | Where in source | Selecta |
|---|---|---|
| CLI start with module/URL arguments (argv parser, appended to list) | s:2209-2300, s:3309 | 8 |
| Workbench start: icon-clicked/project files become arguments | s:3287, s:1973 | 8 |
| Startup module/playlist argument fallback | s:3287-3300 | 9 |
| Single-instance handoff via ARexx port (arguments forwarded) (unverified) | s:2461, s:3287 | D7 |
| ToolType parsing by name (FindToolType/GetDiskObject not found by grep; the 2.x tooltype set is not in this tree) (unverified) | grep s: no hits | NEW |
| UHCBIN environment variable for network tool | s:59589, s:60637 | NEW |
| Pipe-based streaming names (PIPE:hippoStream etc.) | s:60039-60043 | NEW |

## Hotkeys
Key table is a comment block starting near s:9357 (line numbers below are approximate, +-30 lines, unverified); numeric keypad block follows. Global hotkey enabled via prefs.
| Feature | Where in source | Selecta |
|---|---|---|
| R prefs | s:9357 | 7 |
| Return play; * / numpad-plus-return play random | s:9359-9360 | 6 |
| Up / Down list move | s:9361-9362 | 9 |
| Left / Right prev / next song | s:9364-9365 | 6 |
| K / L prev / next module | s:9366-9367 | 6 |
| S add divider (also sort with CTRL+S per code, see doc:306 search) | s:9369 | 9 |
| N new (clear) | s:9371 | 9 |
| D / BACKSPACE / DEL delete module | s:9373-9386 | 9 |
| Space stop/continue | s:9375 | 6 |
| ESC exit | s:9376 | 7 |
| TAB eject | s:9377 | 6 |
| < > pattern skip | s:9378-9379 | 6 |
| A add modules | s:9380 | 9 |
| V / B volume down / up | s:9382-9383 | 6 |
| HELP about | s:9387 | 7 |
| C clear list | s:9388 | 9 |
| ` window shrink / expand | s:9389 | 7 |
| 7/8/9/0 info-line display modes | s:9390-9393 | 7 |
| I module info; SHIFT+I (unverified) | s:9394 | 7 |
| F1-F10 stored program load and play | s:9395 | NEW |
| W save module program; O load module program | s:9396-9397 | 17 |
| H hide window; [ join program | s:9398-9399 | 7 |
| M move; T insert; S sort | s:9401-9403 | 9 |
| Z scope toggle | s:9405 | 7 |
| F find module; SHIFT+F / CTRL+F find next/favorite | s:9407, doc:308-309 | 9 |
| G list repeat / H random play mode keys | s:9411-9412 | 6 |
| CTRL+F favorite; CTRL+S search; CTRL+G get; CTRL+M Modland search; SHIFT+M list mode | doc:302-306, s:9330 block | 8 |
| Numpad: 4/6 prev/next song, 8/2 select prev/next, 7/9 play prev/next, 1/3 rewind/ffwd, 5 stop/cont, 0 add, * random, -/+ volume, Enter, . load program, [ ] / delete/move/insert | s:9420-9440 | 6 |

## External players and formats
| Feature | Where in source | Selecta |
|---|---|---|
| Built-in Protracker replayer (kpl) and PS3M multichannel (MOD/XM/S3M/MTM/etc.) | README.md, doc:39 | 5 |
| HippoPlayer.group bundle of compressed replayers | doc:34-42 | 5 |
| Eagleplayer plug-ins from PROGDIR:eagleplayers/ | s:54173, doc:287-295 | 5 |
| DeliTracker custom players | rdme format list | 5 |
| Symphonie Pro, Sound Master, Sound Programming Language, Impulse Tracker (v1.00/2.00/2.14), PreTracker, Mark Cooksey, Art of Noise | doc:287-291 | 5 |
| Impulse Tracker needs "impulse" + ImpulseDecruncher | doc:293, s:45875 | 5 |
| xmaplay high-quality XM | doc:192-207 | 5 |
| PlaySID library and reSID (PSID, RSID unsupported message, 2SID, 3SID) | s:64590, s:32113 | 5 |
| vgm2wav formats: AY, GBS, GYM, HES, KSS, NSF, NSFE, SAP, SPC, VGM/VGZ | doc:228-232 | 5 |
| mdx2wav MDX (X68000) | doc:231-232 | 5 |
| MIDI (serial out, Timidity, GMPlay) | doc:213-222, s:51875 | 5 |
| MP3 (MPEGA, MHI, streaming) | doc:241, s:64585 | 5 |
| OGG via oggv_dec (unverified) | rdme:112 | 5 |
| MED/OctaMED via medplayer/octaplayer libraries | s:64587-64589 | 5 |
| Native formats: ProTracker family, ST/NoiseTracker, FastTracker 1/2, MultiTracker, ScreamTracker 3, StarTrekker, Oktalyzer, MED, THX/AHX, TFMX, Future Composer, Sidmon 1/2, Digital Mugician, DIGI Booster, JamCracker, Sonic Arranger, SoundFX, Delta Music, BP SoundMon, Music Assembler, Musicline, Hippel, Rob Hubbard, David Whittaker, Jeroen Tel, and ~100 more | rdme:2182-2273 | 5 |
| Song length database (au-songlengths.db from songlengths.tsv) | doc:271-281 | 6 |
| Metadata database (au-metadata.db), 300k+ modules | doc:271-281 | 9 |
| HVSC Songlengths.md5 | doc:185-186 | 6 |
| STIL.txt trivia | doc:185-190 | 7 |
| Hardware outputs: SIDBlaster, USBSID-Pico, ZorroSID, Trinity, AmiGUS, MHI | doc:178-179 | NEW |

## Misc
| Feature | Where in source | Selecta |
|---|---|---|
| reqtools.library file requesters | doc:38 | NEW |
| Keyfile / registration (disabled in 2.46+) | README.md, s:64712 | NEW |
| Compressed state save of playlist on exit | s:3309 | 9 |
| Prefs file S:HippoPlayer.prefs / PROGDIR | s:14315 | 7 |
| Optional S:HippoPlayer.PS3M config file | s:96 | NEW |
| Window iconify/hide to Workbench (AddWorkbenchClient / AppIcon) | s:4133 | NEW |
| Mouse wheel scrolling in lists and info window | rdme:589 | 7 |
| Kickstart 1.2+ support, 68000 baseline | README.md | NEW |
| Disk module delete from list (RMB delete) | s:65554 | 9 |

## Counts
- Total rows: 216
- Rows mapped: 191
- Rows NEW: 25

## NEW list
- Alarm (timed start)
- Doublebuffering of sample data (output)
- Function keys F1-F10 load and play stored programs
- Module comment file (hotkey O/comment) (unverified)
- Archives: unpack LhA, LZX, Zip, gzip on load
- Packed modules: XPK identify, PowerPacker, XFDmaster
- Network transport via external UHCTools (UHCBIN)
- Public screen selection (PUBSCREEN)
- Own screen option ("Screen" prefs) (unverified)
- AHI indicator in infobox
- Enable MHI (external MP3 decoder hardware)
- AmiGUS hardware mixer (32 channel)
- Trinity accelerator audio (SID, AY VGM)
- Task priority setting
- HippoPlayer.group location setting
- ToolType parsing by name (FindToolType/GetDiskObject not found by grep; the 2.x tooltype set is not in this tree) (unverified)
- UHCBIN environment variable for network tool
- Pipe-based streaming names (PIPE:hippoStream etc.)
- F1-F10 stored program load and play
- Hardware outputs: SIDBlaster, USBSID-Pico, ZorroSID, Trinity, AmiGUS, MHI
- reqtools.library file requesters
- Keyfile / registration (disabled in 2.46+)
- Optional S:HippoPlayer.PS3M config file
- Window iconify/hide to Workbench (AddWorkbenchClient / AppIcon)
- Kickstart 1.2+ support, 68000 baseline
