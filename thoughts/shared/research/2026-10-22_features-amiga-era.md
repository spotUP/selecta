---
date: 2026-10-22
topic: Feature survey of Amiga-era module/chiptune players versus the Selecta design
tags: [research, features, hippoplayer, eagleplayer, deli, uade, chipmachine, amigaamp]
status: final
---

# Amiga-era player features vs Selecta design

Design doc: `/Users/spot/Code/MusicPlayer/thoughts/shared/plans/2026-10-22-architecture-design.md` (sections 1-17). Cited as "S<n>".
Local sources read by grep only. HippoPlayer: `/Users/spot/Code/HippoPlayer-68k/code/puu016.s` (66538 lines, Latin-1, use grep -a) and `code/dist/hippoplayerupdate.doc` (319 lines, user guide, "doc:"). Eagleplayer: no .guide files exist in the 2.04 drop, so sources are `Src/Engines/*.s`, `Include/misc/*.i`, Readme.txt, and the earlier research doc `2026-10-22_eagleplayer-architecture.md`.
Design-doc gaps found by grepping it: it has no mention of shuffle/random, song length/duration, fade, filter, favorites, streaming/radio, hotkeys, ARexx, skins, cover, facet, grid views (only text grid), stealth/iconify. "Partial" is used where a hook exists but the feature is not stated.

## 1. HippoPlayer

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Module info window / infobox | Separate resizable info window plus infobox show name, author, format, length, size, stats; metadata from DB when absent | MI_* style tags from replayer; metadata db | doc:80-100,247-260; puu016.s:688 (about_moodi, InfoWindow ~1256) | partial: SONG_OPENED carries metadata (S3, S4 `metadata` sink); no info window spec |
| Sample list / sample view | Window listing module samples; ARexx SAMPLES; sample output text | sample_count, sample_get | puu016.s:678,820,25883 | partial: optional sample_count/sample_get hooks (S4), no UI listed |
| Scopes: Quadra, Hippo, Spectrum, Patternscope (4-18 ch), PSID register scope | Several scope types run at once, each toggled in prefs, size and priority configurable, positions remembered | per-voice buffers, FFT bands, note/instr per voice, SID registers | doc:147-167; puu016.s:417-418,93 (FEATURE_SPECTRUMSCOPE) | yes: S6 taps and viz plugins (scope/spectrum/vu/pattern, S11); SID register scope no |
| Pattern view | Patternscope shows pattern data for many channels side by side | tracker_info, tracker_cells | doc:156; puu016.s:417 | yes: S4 display hooks, S7 pattern view |
| Playlist ops | Add, delete, move, sort, insert, find (F, Shift+F), autosort, dividers, drag-and-drop recursive folder add, module programs (save/load) | ordered entries | doc:60-85,300-310; puu016.s:398,1696 (divider), 13528 | partial: S17 playlists and QUEUE_*; sort/find/divider no |
| List modes: main / favorites / file browser / search | Shift+M cycles four list views | favorites store, fs, sources | doc:44-55 | partial: browse via library S9; favorites no |
| Favorites with remembered subsong | Ctrl+F toggles; saved to S:HippoFavorites.prg, resume subsong | per-item flag + subsong | doc:56-66 | no (S17 entries carry subsong, no favorite flag; RATE stars exist S3) |
| Modland / HVSC / AMP / Aminet / Modules.pl / radio search | Search view queries sources, Ctrl+M Modland, add or Ctrl+G download | source API, HTTP | doc:86-110 | yes for Modland/HVSC/AMP via S9 sources; Aminet/radio no |
| HTTP download / MP3 and radio streaming | UHCTools transport; stream mp3 without download; infobox shows bytes | socket, buffer | doc:100-120 | partial: S9 `fetch`, bsdsocket in S8; streaming no |
| Shared playlists | Community playlists fetched from server | hippoplayer.se API | doc:93 | yes: S17 hippoplayer.se source |
| Stealth / iconify | Hotkey commodity, iconify (case '1' iconify), startup on/off | commodities.library | puu016.s:345,3501; prefs_hotkey | no (headless engine makes it moot; UI concern) |
| Screen modes / pubscreen | Open on pubscreen, fonts, big buttons, tooltips, position slider | UI prefs | puu016.s:365,407; doc:126-145 | partial: UI plugins S7; no pubscreen spec |
| Subsongs | Subsong prev/next, per-favorite memory, ARexx SONGPLAY | subsong count | puu016.s:25850 | yes: SUBSONG n (S3) |
| Song length detection / DB | au-songlengths.db (470k) and au-metadata.db (300k) from xxh32 keyed tsv; HVSC Songlengths.md5; timeout mode | content key, db | doc:268-285; puu016.s:1583 | no for song length; xxh32 key convention exists for genre (S9) |
| Filter (LED) / fade / stereo factor | prefs_filter, prefs_fade, prefs_stereofactor | engine DSP | puu016.s:339,349,435 | partial: stereo separation in engine (S4), no filter/fade |
| Random / play modes | repeat, through, repeat module, module, random (pm_*); play-mode button; ARexx RANDPLAY | queue | puu016.s:1714-1723; doc:142 | partial: REPEAT mode command (S3), random no |
| Preferences | ~120 prefs (timeout, rates, AHI, scopes, fonts, save state) | prefs store | puu016.s:320-460 | no (no config/prefs section) |
| External replayers / Eagleplayers | Player group file plus external Eagleplayers loaded on demand | plugin loader | puu016.s:281,11321; doc:287-295 | yes: S9a, S10 |
| Output: Paula, AHI, PS3M, xmaplay, AmiGUS, MHI, MIDI, PlaySID/reSID | choose mixer/hardware backend per format | outputs | doc:170-265; puu016.s:277 | yes (Paula/AHI): S5; AmiGUS/MHI/MIDI no |
| Hotkeys | Single-key and Ctrl combos (Z scope, A add, F find, DEL) | key map | doc:300-312 | no |
| ARexx | PLAY CLEAR CONT STOP EJECT LOADPRG ADD DEL VOLUME REW FFWD MOVE SORT INSERT QUIT CHOOSE(NEXT/PREV) SONGPLAY GET RANDPLAY HIDE ZIP PUBSCREEN TIMEOUT LOADPREFS SAMPLES TOGGLEMODE FAVORITE | ARexx port | puu016.s:25745-25890 | partial: remote protocol of same commands (S8), no ARexx port |
| Archive handling (LHA/ZIP/LZX, XFD) | prefs_arc*, xfd decrunch | xfdmaster, lha | puu016.s:271,380-390 | yes: S9 "archive decrunch", S11 archives |

## 2. Eagleplayer 2.04

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Analyzers (FFT, Levelgraph, Levelmeter, Analyzer) | Engines read UPS per-voice structs and amplifier buffers | per-voice Adr/Len/Per/Vol | Src/Engines/Analyzer.s, FFT-Analyzer.s, Levelgraph.s, Levelmeter.s | yes: S6 |
| Scopes (Quadrascope, Stereoscope(AGA), SpaceScope+) | Engine draws waveform from voice/AS data | voice buffers | Src/Engines/Quadrascope.s, StereoscopeAGA.s, SpaceScope+.s | yes: S6 |
| Amplifiers (8 Bit, 14 Bit, Toccata, AmplifierManager) | Software mixers fed by AS_ audio structs | AS_ struct, note structs | Src/Engines/14 Bit Amplifier.s, Toccata-Amplifier.s | yes: paula14, mixer (S5, S9a) |
| Module info engine | Window of MI_* tags (name, author, subsongs, length, samples, bitrate, year, genre) | MI_ tags | Src/Engines/Moduleinfo.s; Include/misc/EaglePlayer.i MI_TagBase | partial: metadata sink S4 |
| Converters (Noiseconverter, PT-clone to ProTracker) | Convert exotic module to PT; plugin dir Noiseconverters | module bytes | Src/Engines/Noiseconverter.s, Noiseconverters/ | no (non-goal: editing/export S1) |
| Rippers (Eagleripper) | Scan memory/disk for modules | raw data | Src/Engines/Eagleripper.s | no |
| Sample saver | Save samples as IFF/WAV | sample_get | Src/Engines/Samplesaver.s | partial: S1 "sample export is cheap to keep" |
| Format loaders | Load non-module formats via plugin | file bytes | Src/Engines/Formatloader.s | partial: replayer probe/open S4 |
| XFD / archive decrunch (Extractor, EP_Decrunch) | xfdmaster, powerpacker, crunchmania decrunch; Extractor handles archives with modules | xfd libs | Src/Engines/Extractor.s, Src/EP_Decrunch.s | yes: S9, S11 |
| Pattern scroller | Scrolling pattern display from EP_Patterninfo | pattern info tags | Src/Engines/Patternscroll.s, Patternscroll++.narrow.s | yes: S4 tracker hooks |
| Replayer config | EP_LoadPlConfig / SavePlConfig, user config per player | per-player options | EaglePlayer.i EP_TagBase | no (per-replayer config not in design) |
| Playerlist / playerloader | Player files with ID+tag array loaded from directory, staged Check1/Check2 | plugin dir | Src/Engines/Playerloader.s; DeliPlayer.i L161 | yes: S9a, S10 |
| Userprogs / Manager / time | Manager engine, Time engine (elapsed/total), PublicScreenSelector | engine state | Src/Engines/Manager.s, Time.s, PublicScreenSelector.s | partial: POSITION event S3; no manager |
| Engines (task vs tag) and GUIs (EagleAMP, EMPY, Eagleplayer1x classic, BIFAT) | Pluggable GUIs on one core; message port EAGLEPLAYERPORT | port | Src/Engines/EagleAMPGui.s, EMPYGui.s; Readme.txt:7-12 | yes: S7 UI plugins |
| Directory list viewer | Browse dirs for modules | fs | Src/Engines/DirListViewer.s | yes: S9 local source |
| Toccata/AHI-style alt output | Toccata amplifier | sound card | Src/Engines/Toccata-Amplifier.s | partial: S5 outputs |

## 3. DeliTracker (web)

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Genies | Plugin add-ons, e.g. DeliDecrunch (extract first file in archive, play), DiskChanger (module list per CD) | plugin dir | https://se.aminet.net/mus/play/DeliTracker232.readme ; DiskChanger.readme | partial: plugin kinds S10; archive play yes S9 |
| Module lists | List of modules with auto-load per medium | list files | DeliTracker232.readme | yes: S17 |
| Cue list | Not found in sources searched | - | (unverified) | no (unverified feature) |
| Replayer ABI | PLAYERHEADER: bra, 8-byte ID, tag array; Check1/Check2, Interrupt at 50 Hz, DTN_NoteStruct notes-to-mixer | tags | Include/misc/DeliPlayer.i L36,161,251 (local) | yes: S4, S9a |
| 150+ formats, built-in PT players, external DeliPlayers dir | drop-in players | files | readme above | yes: S10 |
| Random play, songend detection, scopes, ARexx, pubscreen, hotkey commodity, tooltypes | UI niceties | - | readme above | partial: songend event S3; others no |
| xpk/LHA decrunch, timer.device PAL/NTSC speed | - | libs | readme above | partial |
| 14-Bit genie | Mixed 14-bit output for 020+ | CPU | readme above | yes: paula14 S5 |

## 4. ProTracker / NoisePlayer era

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| ProTracker replay (tick at 50 Hz VBL/CIA, 4 voices, Paula period) | Voice-driven replayer writes Paula regs | periods, samples | general knowledge (unverified) | yes: VOICE kind, set_period S4 |
| Sample name list / song name / pattern position display | Standard display of 31 samples, order, row | MOD header | general knowledge (unverified) | yes: S4 hooks |
| PT-clone conversion (NoiseConverter) | Rewrite exotic variants to PT | module | Eagleplayer Noiseconverters (local) | no |
| Channel mute, filter toggle, vu bars | Per-channel mute, LED filter, bars | state | general knowledge (unverified) | partial: MUTE voice S3; filter no |

## 5. UADE and front ends

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Hardware emulation of Amiga + Eagleplayer replayers | 68k+Paula emulation runs original players | amiga rom-less core, eagleplayers | https://www.mankier.com/1/uade123 | yes: nuade adapter S1, S4 |
| Paula filter model (a500 / a1200 / none), LED force | Emulated analog filters | engine DSP | uade123 man page | no |
| Panning, headphones, gain, resampler (default/sinc/none) | Postprocessing options | output stage | uade123 man page | partial: stereo separation S4; rest no |
| Subsong switch, one subsong per file, subsong timeout (512 s), silence timeout, jump to second | song end by timeouts, silence, or player end | timers | uade123 man page | partial: SUBSONG, SEEK (S3), SONG_ENDED; timeouts no |
| song.conf / eagleplayer.conf / uade.conf | Per-song and per-format overrides, ignore_player_check, ep options | config | uade123 man page | no |
| Format detect by content / ignore check | Probe by content vs name | probe | uade123 man page | yes: probe S4 |
| Shuffle key, prev/next song, restart | uade123 action keys | playlist | uade123 man page | no for shuffle (not in doc) |
| Song length db and content db (xxh32 key), used by Audacious/Hippo | md5/xxh32 keyed length and metadata | key | doc:268-285 (Hippo consumers); audacious-uade-tools dir | partial: xxh32 key exists S9, no length db |
| nuade position | position(): order/row/tick | player state | nuade `include/nuade/player.h:152-153` | yes: S4, POSITION event S3 |
| nuade voice_info | per-display-voice note/instrument/volume | player state | player.h:154-156 | yes: VOICE event S3 |
| nuade events (Events mode) | voice stream entries around track_pos, stateless | player | player.h:157-160 | partial: hook list names events, S4 |
| nuade tracker_info / tracker_cells | pattern layout and per-channel cells for row range | player | player.h:162-170 | yes: S4 |
| nuade grid_ticks | tick-player pseudo pattern from scan | scan | player.h:172-177 | yes: named S4 |
| nuade sample_count / sample_get | sample export, non-PCM entries counted | player | player.h:108-109 | yes: S4 |

## 6. Chipmachine (sasq64)

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Cross-collection search | One incremental search over Modland, HVSC, Amiga Remix, SNESMusic, SAP, Pouet, CSDb | merged index | https://github.com/sasq64/chipmachine | partial: search per source S9; one merged cross-collection index not stated |
| Incremental search-as-you-type | Results update per keystroke; space separates words | prefix index | same | yes: FTS5 prefix search S9a |
| Queue (Shift-Enter) and favorites (F7) | Enter plays, Shift-Enter queues, F7 favorite | queue | same | partial: QUEUE_ADD S3; favorites no |
| Shuffle | Not described on README page (unverified) | - | same | no |
| Plugin formats (OpenMPT, GME, ADplug, UADE, Vice, Hively) | one engine many libs | - | same | yes: S1 non-goals/adapt list |
| Streaming (Shoutcast, FFmpeg, podcast) | stream sources | network | same | no |
| Two-screen UI (Player F1, Search F2) | - | - | same | partial: UI plugins S7 |

## 7. AmigaAMP and AmiNetRadio

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| AHI output | Playback through ahi.device | AHI | https://aminet.net/package/mus/play/AmigaAMP | yes: S5 |
| Decoder engines loadable | Built-in and loadable decoders | plugin | same | yes: S10 |
| Skins (WinAMP wsz via xadmaster, ANR, MUI) | Datatype-driven skins | skin files | same; https://www.exotica.org.uk/wiki/AmiNetRadio | no (UI plugins can differ, no skin spec) |
| Streaming over HTTP/HTTPS, XSPF streaming playlists (AmiSSL) | stream decode | sockets, TLS | amiga-news AN-2021-12-00067 | no (XSPF read yes S17; stream no) |
| ARexx macros (3.30) | scripted control | ARexx | Aminet AmigaAMP3 page | no |
| MHI to AHI fallback | decoder output fallback | - | same | partial: routing policy S5 |
| AmiNetRadio: Shoutcast search, stream to disk, m3u/pls | directory search plus recording | network | exotica.org.uk AmiNetRadio (search snippet, page fetch blocked) (unverified) | partial: PLS/M3U yes S17; shoutcast, record no |
| CD audio via AHI | CD track playback | device | same (unverified) | no |

## Views that appear anywhere (column browser, cover flow, grid, facets)

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Column browser (Finder/iTunes style: format > artist > file) | Cascading lists, each narrows the next | format, artist, album columns | not found in any Amiga player; bbmp model (design S9a) | partial: browse model collection > format > artist > file (S9a); the UI form is not specified |
| Cover flow | Album art carousel | cover art | no Amiga-era player had it (unverified) | no |
| Grid views | Thumbnail/tile browse | art or icons | no Amiga-era player found (unverified); only a text grid exists in design (S6) | no |
| Facets (genre, rating, year, format counts) | Filter counts per attribute | genre from DEViLBOX, rating, year | metadata db fields (Hippo doc:268-285); design S9 genre, rating | partial: genre and star rating sources exist (S9), facet UI/query not specified |

## Biggest gaps (no in design)

Shuffle/random, song-length database and timeouts, Paula filter/fade/headphones/resampler DSP, favorites, per-replayer and per-song config, hotkeys and ARexx port, streaming (Shoutcast, HTTP mp3), skins, cross-collection merged search, facet/column/cover/grid UI forms.
