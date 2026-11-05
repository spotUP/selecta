---
date: 2026-11-05
author: claude
topic: S3btify feature gap versus Selecta
---

# S3btify feature gap versus Selecta

## What S3btify is

Freeware (no open-source licence, no public repo stated) desktop and phone music player that grew from a Sonos S1 replacement into a multi-room player with strong retro-format support. Platforms: Windows 10/11 (0.15.10), macOS 12+ arm64/Intel (0.15.5, unsigned), Linux deb/rpm/Flatpak, Android APK (0.14.23, Android Auto), iPhone/iPad as Safari web app, a web player, and a "Floodland BBS" terminal inside the player. Decoders: libopenmpt, sidplayfp, UADE (also WASM), ZXTune, vgmstream, WildMIDI (FreePats). Playback stays local; internet only for cover art (MusicBrainz, Cover Art Archive, Deezer, iTunes) and lyrics (LRCLIB, lyrics.ovh), all toggleable. Roadmap: Home Assistant, Google Nest Hub UI, signed apps, iPhone app.

Sources read: home page https://www.s3btify.com/?lang=en , changelog https://www.s3btify.com/nouveautes.html (version tags below are from it), guides https://www.s3btify.com/guides.html (only Android Auto, remote and SSH content was returned; guide pages were summarised by a fetch model, so treat details as unverified). Web search found nothing independent about s3btify. We did not install or run it.

Selecta evidence: ledger rows are in thoughts/spot/todos.md (IDs), plugin dirs under plugins/. Status is from grep only, not from running Selecta; "have" means a plugin or closed ledger row exists, not that the behaviour matches S3btify.

Sizes: S under a day, M a few days, L a week or more.

## Feature table

URL key: H = home page, N = nouveautes.html (version in brackets).

### Playback

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Loop count 1-10 for looping tracks | N 0.12.4 | have | I94 loops option (todos.md:241) | S |
| Per-track normalisation measured on first play, cached | N 0.14.20 | partial | M7 design (todos.md:51), I189b limiter (:393), src/analyse/loudness.h; no confirmed user setting | S |
| Sound boost +3 to +12 dB with limiter | N 0.14.20 | partial | limiter exists (I189b); no boost control found | S |
| Karaoke vocal removal (EQ engine or AI, GPU/NPU, FLAC/MP3 output) | H, N 0.14.9, 0.14.11 | missing | no hit for karaoke/vocal | L |
| Equalizer | N 0.9.3 | partial | F39 parametric EQ closed, but plugins/dsp has only crossfeed | M |
| Next track prepared 5 s before end | N 0.13.12 | have | gapless/crossfade in src/engine/engine.c, F29 | S |
| Original PAL timing for SID and Amiga | H | have | playsid, nuade plugins; timing not re-verified | S |
| Playback resumes after network drop | N 0.12.4 | missing | no hit; matters only for remote/stream use | M |
| Synced lyrics (.lrc, LRCLIB) | N 0.9.5 | partial | src/library/lyrics.c, F19; no LRCLIB or lyrics.ovh fetch found | M |
| Play counter, History playlist, Most played | N 0.8.5, 0.9.3, 0.9.5 | have | F16 play counts (todos.md:68) | S |

### Formats and replayers

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Tracker modules (libopenmpt) | H | have | plugins/replayers/openmpt | S |
| C64 SID (sidplayfp) | H | have | plugins/replayers/playsid, sid | S |
| Amiga custom formats (UADE) | H, N 0.10.0 | have | plugins/replayers/nuade | S |
| Console music via ZXTune (SNES, NES, GBA, DS, Mega Drive etc.) | N 0.12.6 | have | plugins/replayers/zxtune, gme, libvgm | S |
| vgmstream game audio streams (PS1/PS2/GC/Wii/PSP etc.) | N 0.12.6 | partial | zxtune patch references vgmstream; I709 open | M |
| MIDI via WildMIDI + FreePats (Gravis Ultrasound sounds) | N 0.12.6 | missing | I771 MIDI replayer open | M |
| Archived music indexed and played (zip, 7z, lha/lzh, tar, gz, bz2, xz) with archive badge | N 0.14.20 | partial | I159, I159a: zip, lha, lzx, PP20; no 7z/tar/gz/bz2/xz seen | M |
| UADE in WebAssembly (web player, terminal) | N 0.14.12 | partial | I694 web port plan done, port itself not confirmed | L |
| Subsongs for SID and Amiga | N v5 | have | engine subsong handling (closed rows) | S |

### Sources and online

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Amiga Collection Thunder / Scoopex online library | N 0.14.19 | missing | not in plugins/sources | M |
| Mods Anthology online library | N 0.13.3 | missing | not in plugins/sources | M |
| HVSC online library streamed per file | N 0.13.3 | have | plugins/sources/hvsc, I162 | S |
| Spotify CSV import (Exportify), unmatched-track handling | N 0.14.11 | missing | no hit | M |
| Cover fallbacks MusicBrainz, Cover Art Archive, Deezer, iTunes with per-source toggles | H, N 0.14.11 | partial | I1003 artwork scraper open | M |
| Demo/game cover art from CSDb and Demozoo for retro tracks | H, N 0.12.4 | missing | I411/I748 mention Demozoo, no cover scraping | M |
| Per-file demo covers, per-track covers in retro album lists | N 0.12.5, 0.12.10 | missing | same as above | M |
| Network libraries: NAS, DLNA/UPnP | H, v1 | have | plugins/sources/dlna, I894a1 | S |
| Download folder size limit and clear button | N 0.13.3, 0.14.9 | unverified-missing | no hit; counted as missing | S |

### Library

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Folder browser, multiple music folders, open files outside library | N 0.7.0, v3 | have | F16 folder watching, library browse | S |
| M3U import and export | H, N v5 | have | todos.md mentions M3U in 6 rows | S |
| Detailed technical file info sheet with exact sizes | H, N 0.12.5 | partial | NOTE/tag edit I172; info sheet not confirmed | S |
| Choose best of two copies; Hide duplicates option | H, N 0.14.19, 0.8.8 | missing | no hit in todos or library design | M |
| Filters by format, genre, year; retro-format filter | H, N 0.8.6-0.8.7 | partial | FTS5 search I146; filter chips planned in desktop-library-architecture design | M |
| Favourites with filters per family, drag reorder | N 0.14.1, 0.14.9 | partial | favourites exist in library design; per-family filter unconfirmed | S |
| Playlist search field filtering current playlist | N 0.14.21 | partial | search exists (I1011 area); in-playlist filter unconfirmed | S |
| Playlist Repeat column | N 0.14.9 | missing | no hit | S |
| FTS5 index, WAL, paged "All tracks" (300) | N 0.14.14, 0.14.17 | have | I146, I198b | S |
| Full backup and restore with path translation | N 0.9.3, 0.13.10 | missing | no backup feature in todos (only unrelated rows) | M |
| Server playlist backup to cloud | N 0.14.2 | missing | would depend on Floodland-style account | L |
| Spotify-style Home: Rediscover, Recently added, Most played | N 0.14.6 | partial | I678j Home chips and quick access | M |

### UI

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Mini player, always on top, remembers position | N 0.12.7, 0.15.10 | partial | I621 lists a mini-player button; src/uimodel/actions.c mentions it; no SDL always-on-top call in plugins/ui/desktop | M |
| Integrated custom title bar with snapping | N 0.14.15, 0.14.19 | missing | no hit | M |
| F11 full screen, window position and size remembered | N 0.14.17 | partial | full screen exists for Amiga UI (RULES), Mac F11 unconfirmed | S |
| Large Now Playing cover panel, Up next with cover in bar | N 0.12.7, 0.15.10 | partial | Now Playing strip I269; Up next not confirmed | S |
| Scrolling banner with track details | N 0.15.10 | missing | no hit | S |
| Change or paste album cover (Ctrl+V), random covers | N 0.14.17 | missing | I1003 scraper only | S |
| Mosaic covers for artists and playlists, gradient backgrounds | N 0.14.6, 0.14.17 | missing | no hit | M |
| Reopen on the page I left | N 0.14.1 | missing | no hit | S |
| English plus other UI languages, switch without stopping music | H, N 0.12.5 | missing | UI is English only | M |

### Visualisation

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Spectrum analyser (Open Cubic Player style) | N 0.14.16 | have | plugins/viz/spectrum, I85-I87 | S |
| Oscilloscope, one trace per voice up to 32 | N 0.14.10 | have | plugins/viz/scope, I918 | S |
| Full-screen animation, auto change, track info overlay | N 0.12.9, 0.12.11, 0.14.10 | partial | viz panes exist; full-screen cycling not seen | M |
| Track title as glowing dots | N 0.12.10 | missing | no hit | S |

### Platform and integration

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| AirPlay 2 output | N 0.11.0 | partial | raop plugin, I1039 in progress | L |
| Chromecast output | N 0.11.0 | missing | only research doc 2026-11-01-airplay-chromecast.md (I892) | L |
| DLNA renderer output | N 0.11.0 | missing | DLNA is source only | L |
| Sonos output, groups, per-speaker volume, wake-up | N 0.8.0, 0.8.5 | missing | no hit | L |
| HEOS, BluOS, MusicCast, WiiM/LinkPlay, Volumio outputs | N 0.11.1, 0.14.13 | missing | no hit | L |
| Multiroom / mirror mode | N 0.14.8 | missing | no hit | L |
| Phone/tablet server, 6-digit pairing, per-device keys | N 0.12.0 | missing | no hit | L |
| Android app | N 0.12.0 | missing | no hit | L |
| Android Auto | N 0.12.2-0.12.3 | missing | no hit | L |
| iPhone/iPad via Safari web app | H | missing | I694 web port plan only | L |
| Remote access (Cloudflare, Tailscale, VPN) | N 0.12.4 | missing | no hit | M |
| Web player in browser | H | partial | I694 plan | L |
| Home Assistant integration (roadmap) | H | missing | roadmap item on their side too | M |
| Last.fm, MPRIS, macOS Now Playing | (not in S3btify) | have | plugins/integrations | S |
| Update check at startup | N 0.13.3 | partial | MPS-17 open (todos.md:1442) | S |
| Windows installer, deb, rpm, Flatpak packaging | H | partial | release packaging not confirmed | M |

### Other

| Feature | S3btify detail | Status | Our evidence | Size |
|---|---|---|---|---|
| Floodland BBS terminal, chat, SSH remote control, stats | H, N 0.14.0-0.14.7 | missing | vendor-specific community feature, low fit | L |
| Account system (free account unlocks karaoke, server playlists) | H | missing | by design Selecta is local | L |
| Clear all caches with sizes | N 0.14.11 | missing | no hit | S |
| Headless server mode (--nogui) | N 0.12.9 | missing | no hit | M |
| Diagnostics command (--check) | N 0.6.0 | missing | no hit | S |

## Missing and partial, ordered by user value

Proposed ledger row titles.

1. Hide duplicates and best-of-two-copies in the library (missing, M)
2. Mini player: always on top, remembered position, Up next (partial, M)
3. Library filters by format, genre, year, retro family (partial, M)
4. Playlist/library backup and restore with path translation (missing, M)
5. Remaining console and game formats: vgmstream port (I709) and WildMIDI/FreePats MIDI (I771) (partial/missing, M each; rows exist)
6. Archive reading: 7z, tar, gz, bz2, xz plus archive badge (partial, M)
7. Cover art fallbacks MusicBrainz/CAA/Deezer/iTunes with toggles (partial, M; extends I1003)
8. Demo and game cover art from CSDb and Demozoo (missing, M; extends I1003, I411)
9. LRCLIB and lyrics.ovh lyrics fetch (partial, M)
10. Normalisation setting and sound boost +3..+12 dB with limiter (partial, S)
11. Equalizer as shipped plugin with presets (partial, M)
12. Online libraries: Amiga Collection Thunder/Scoopex and Mods Anthology (missing, M)
13. Spotify CSV import (Exportify) (missing, M)
14. Cover handling: paste with Ctrl+V, mosaic artist/playlist covers, gradient backgrounds (missing, M)
15. Home page: Rediscover, Recently added (partial, M; extends I678j)
16. Playlist search field and Repeat column (partial/missing, S)
17. Full-screen visualiser cycling and glowing-dot title (partial/missing, M)
18. Custom title bar, F11 full screen, remembered window state, reopen on last page (missing/partial, M)
19. UI language switching (missing, M)
20. Karaoke vocal removal (missing, L; needs an ML or DSP source separation decision, ask Johan)
21. Network output targets: Chromecast, DLNA renderer, AirPlay finish I1039 (L each; I892 research exists)
22. Sonos/HEOS/BluOS/MusicCast/WiiM/Volumio outputs and multiroom (missing, L)
23. Remote control: phone/tablet server, pairing, Android app, Android Auto, iPhone web, remote access (missing, L; large product decision)
24. Web player (partial, L; I694)
25. Update check, caches clear, --nogui server, --check diagnostics (small chores)
26. Home Assistant integration (missing, M; low priority)
Low fit, probably skip: Floodland BBS, account system, server playlist backup.
