---
date: 2026-10-30
author: spotUP (agent, sonnet)
row: I737b
topic: features of HippoPlayer (68k source) that Selecta lacks
status: draft
---

# HippoPlayer gaps in Selecta

Upstream: `/Users/spot/Code/HippoPlayer-68k` (local, read only). Paths are relative to its `code/` directory. `puu016.s` is the main source (66538 lines, ISO-8859: grep needs `-a`). `dist/hippoplayerupdate.readme` and `.doc` are the change history. The 2026-10-22 parity doc (`2026-10-22_parity-hippoplayer.md`, 216 rows, 25 NEW) was read first; this table lists what is still a gap now, after checking the ledger and the code. Replayer plugins are covered by rows I421, I426-I434, I696-I710, so formats appear as one line. Search in HippoPlayer is a shell script handed to the UHC tools (`uhcmirrorsearch`); the server side is not in the tree.

Selecta status: missing / partial / covered. "Unverified" = grep only, no run. Evidence paths are in the Selecta repo. F-rows are design picks, not running code.

| Gap | Where in HippoPlayer | Selecta status | Row (existing or proposed) |
|---|---|---|---|
| **Online sources and search** | | | |
| AmigaRemix (remix MP3 search plus stream) | `puu016.s:10387`, `:58399`, `:59122` | Missing: no `plugins/sources/amigaremix` | I408 |
| Remix.Kwed.org (remix MP3 search plus stream) | `puu016.s:10394`, `:59126` | Missing | I409 |
| Aminet (`mods/` tree search, LHA modules) | `puu016.s:10388`, `:58387`, `:59157` | Missing | I405 |
| Modules.pl | `puu016.s:10392`, `:59114` | Missing | I406 |
| AMP (Amiga Music Preservation) | `puu016.s:10389`, `:58420`, `:59188` | Missing (403 to curl) | I407 |
| Radio stations search (name column, 1000 result cap, ICY name and description) | `puu016.s:10393`, `:59178`, `:58760`, `:60966` | Partial: `plugins/sources/radiobrowser` (top 100 on Sync), no station search verb | I410, I725c; search needs I406's SOURCE_SEARCH |
| Modland, HVSC | `puu016.s:10390-10391` | Covered: `src/library/sources/modland.c`, `plugins/sources/hvsc` | done |
| Shared (community) playlists | `puu016.s:35494`, readme:785 | Partial: `src/library/sources/hippo.c`, `hippo_sync.c`; list not shown in UI | I273, I163a, I417 |
| Recent playlists search (search mode on `asciiarena.se`) | `puu016.s:58379`, `:59134`, `:59182-59187`, `:58468` | Missing: no hit for `asciiarena` in `src` or `plugins` | Proposed: Recent playlists search mode |
| Turran FTP (`ftp2.grandis.nu/turran/FTP/collection/music/`) | `puu016.s:10396`, `:58415`, `:59130`, `:59161`; readme:810 | Missing; no ledger row mentions it | Proposed: Turran FTP source |
| Search result limit as a bound (SEARCHLIMIT=10000, stations 1000) | `puu016.s:59150`, `:59179` | Missing: no `SOURCE_SEARCH` verb yet | I406 (D-OS-3) |
| Last search source remembered | readme:97, `puu016.s:10386` | Unverified: only `library.sources_off` (`include/selecta/prefs.def:60`) | Proposed: fold into I406 |
| Stream byte counter in info box while downloading | `dist/hippoplayerupdate.doc:117` | Unverified: `src/library/stream_open.c` buffers, no readout found | Proposed: Stream progress readout |
| Radio stream without end detection | `puu016.s:46059-46076`, `:46194-46200` | Covered: `src/library/stream_open.c`, `net/http.c` | I195a-f |
| Metadata source catalogue: 410 TSV files naming the sites, collections, groups, BBSes and CD sets the song databases came from | `audacious-uade-tools/songdb/sources/{site,collection,group,bbs,cd}/*.tsv` | Partial: songdb lookup by hash exists (`src/library/db/songdb.c`, I418); the TSVs are not mined for provenance or for further online sources | Proposed: Provenance field from songdb source lists; survey `site/` TSVs for more sources |
| Demozoo metadata | `audacious-uade-tools/songdb/sources/metadata/demozoo_*.tsv` | Missing | I411 |
| Song length and metadata databases (300000+ modules) | readme:271-281 | Covered | I174, I418 |
| **Playback** | | | |
| Continue on error: skip an unplayable module | `puu016.s:346` (`prefs_cerr`) | Missing: no pref, no skip-on-failure code in `src/engine` or `include/selecta` | Proposed: Skip unplayable songs setting |
| Early load of next module | `puu016.s:373` (`prefs_early`) | Partial: `library.prefetch` for remote only (`prefs.def:48`) | I168 |
| Play timeout: auto-advance after N seconds | `puu016.s:351` (`prefs_timeout`), `:334` | Unverified: no engine timeout pref in `prefs.def`; `src/engine/timer.h` has sleep and alarm only | Proposed: Per-song play timeout |
| Alarm | `puu016.s:358`, `:997` | Covered: `src/engine/timer.h` | I96 |
| Fade on stop or skip | `puu016.s:339` | Covered | F6, I93-I95 |
| Function keys F1-F10 load a stored program | `puu016.s:367` (`prefs_fkeys`) | Missing: no hit in `src/uimodel/keybind.c` | F11 (design only); Proposed: F-key programs build row |
| Module comment file | `puu016.s:10201`, `:24199` | Partial: NOTE and tag edit | I172, I139 |
| Sample list and export | `puu016.s:23906-23909` | Covered | I98 |
| Doublebuffer, VBlank timer, Nasty audio options (Paula path) | `puu016.s:348`, `:350`, `:347` | Missing (Amiga tier only) | Proposed: Paula output tuning |
| LED filter | `puu016.s:349` | Covered: `output.led` (`prefs.def:33`) | I127 |
| Stereo level, mix rate, boost, buffer | `puu016.s:361`, `:352`, `:385` | Partial: `mix.stereo_width`, `output.buffer` (`prefs.def:32-38`); no rate or boost pref | Unverified; no new row |
| TFMX rate, MED mode, MPEGA quality, cyber calibration | `puu016.s:329`, `:386`, `:401-405` | Missing; nuade owns TFMX and MED | Proposed: Per-format output tuning (only if a replayer needs it) |
| **Formats and replayers** | | | |
| UADE and nuade formats, Eagleplayers, DeliTracker customs | readme:1429-1439 | Covered | I422, I426-I434 |
| PS3M family and xmaplay | `pl/`, readme:160 | Covered / open | I427a-f, I696-I710 |
| vgm2wav families (AY, GBS, GYM, HES, KSS, NSF, SAP, SPC, VGM), mdx2wav | `dist/hippoplayerupdate.doc:228-232` | Partial: `plugins/replayers/gme` | I699, I709, I421 |
| MIDI playback (serial delimidi, Timidity, GMPlay) | readme:645, `dist/hippoplayerupdate.doc:213-222` | Missing: no MIDI replayer in `plugins/replayers`; no ledger row | Proposed: MIDI file replayer |
| Sample files as playable items: WAV, AIFF, IFF 8SVX | readme format list | Partial: MP3, Ogg, FLAC exist; WAV is writer only (`plugins/outputs/wav`); no reader | Proposed: PCM sample file replayer |
| PlaySID, reSID, 2SID/3SID, STIL | `puu016.s:18600-18602`, `:18675-18677`, `:1583` | Partial | F14, I212 family |
| Hardware SID outputs: SIDBlaster, USBSID-Pico, ZorroSID | doc:174-180 | Missing; design only | F26; Proposed: build row |
| Hardware audio: AmiGUS, Trinity, MHI | doc:241-265 | Missing; design only | F26; Proposed: build rows per device |
| Archives on load: LhA, LZX, Zip, gzip | `puu016.s:362-364`, `:14315-14362` | Covered | `plugins/shared/archive/*` |
| Packed modules: PowerPacker, XPK, XFD | `puu016.s:338`, `:375`, `:64585-64592` | Partial: PP20 done, XPK and xfd open | I159b |
| **Playlist** | | | |
| Four list modes (main, favorites, file browser, search) | readme:25-29 | Covered | parity doc rows |
| Module programs (HiPPrg) save, load, join | `puu016.s:13523` | Partial: `src/library/sources/hippo_prg.c`; publish URL wrong | I417 |
| Playlist capacity 131071 items | readme:61 | Unverified: no size test | Proposed: Playlist size test |
| **UI and scopes** | | | |
| Quadra, filled quadra, hippo, bars, spectrum scopes | `puu016.s:64842-64983` | Partial: `plugins/viz/{scope,levels,spectrum}` | I269, I280, I292 |
| Patternscope 4-18 channels, mini4 font | `puu016.s:64954-64969` | Partial | I278, I526 |
| Several scopes at once | readme:497 | Missing | Proposed: Multiple simultaneous viz panes |
| External scope plug-ins (`scopes/`) | `scopes/` | Missing; plugin loader in design | I427b, I427c |
| Public screen, own screen, iconify, AppIcon | `puu016.s:365`, `:4133`, `:5020` | Missing; design only | F27; Proposed: Amiga screen and AppIcon build row |
| CD32 and CDTV remote keys | readme:984 | Missing; no row | Proposed: CD32 pad and CDTV keys (Amiga tier) |
| **Scripting and CLI** | | | |
| ARexx port, 32 commands | `puu016.s:25820-25885` | Covered | D7, `plugins/scripting/arexx` |
| Tool types and project-icon launch | readme:1407 | Covered: `src/engine/options.c` | F27 |
| **Not applicable** | | | |
| Keyfile and registration, anti-cracker, reqtools, Kickstart 1.2 baseline, UHCBIN env, PIPE: names | `keyfile0.s` | Out of scope | skip |

## Counts

- Proposed new rows: 17 (listed below).
- Existing ledger rows cited as covering a gap: I405-I411, I273, I163a, I417, I421, I426-I434, I699, I709, I159b, F26, F27.
- Parity doc listed these as NEW, but they are covered: alarm (I96), fade, LED filter, archives, sample list.

## Proposed new rows

1. Turran FTP source.
2. Recent playlists search mode.
3. Skip unplayable songs setting.
4. Per-song play timeout.
5. F-key stored programs build row (F11 is design only).
6. MIDI file replayer.
7. PCM sample file replayer (WAV, AIFF, 8SVX).
8. Stream progress readout.
9. Provenance field from songdb source lists, plus survey of `site/` TSVs for further sources.
10. Hardware output build rows (SID devices, AmiGUS, Trinity, MHI) from F26.
11. Amiga screen and AppIcon build row from F27.
12. Paula doublebuffer, VBlank and Nasty options.
13. Per-format output tuning (TFMX, MED, MPEGA).
14. Multiple simultaneous viz panes.
15. CD32 pad and CDTV keys.
16. Playlist size test.
17. Last search source setting (fold into I406).

## Unverified

- Play timeout: no pref found, but a verb may expose it; check `src/uimodel`.
- Stereo rate and boost prefs: only `mix.stereo_width` and `output.buffer` found.
- UHC mirror protocol (`uhc.amigaremix.com`, `uhc.remix.kwed.org`): the search scripts are in `puu016.s:59100-59195`, the server is not in the tree, so I408 and I409 must capture the real sites' API.
