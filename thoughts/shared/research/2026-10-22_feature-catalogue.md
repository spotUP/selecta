---
date: 2026-10-22
topic: Master feature catalogue (union of all feature surveys and HippoPlayer parity) for design doc section 1a
tags: [features, catalogue, parity, feature-bar, selecta]
status: final
---

# Selecta master feature catalogue (M1e)

Inputs: Amiga, PC, Modern, Term (the four `features-*` research docs of 2026-10-22), Hippo (`parity-hippoplayer.md`, NEW rows only; mapped rows are merged into the matching feature). Design doc: `thoughts/shared/plans/2026-10-22-architecture-design.md` (S<n> = section n). Ledger: `thoughts/spot/todos.md`.

Status values: `picked F<n>` = covered by ledger F-item and a section 1a row; `design S<n>` = no F ID but a design doc section specifies it (grep-checked); `parity H` / `parity B` = required for HippoPlayer / bbmp parity and not otherwise homed; `unhomed` = no F ID, no 1a row, no design section (design doc grepped by keyword). "(declined)" = design doc S16 puts it out of scope. Milestone is a guess where the design doc gives none. F-item map: F1 playback, F2 library, F3 visual, F4 remote/social, F5 chip/format, F6 output/DSP, F7 playlist, F8 files, F9 DJ/live, F10 metadata, F11 tracker extras, F12 integration, F15 replayer config, F16 folder watch + play counts, F17 phase view, F18 headphones.

## Playback
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Crossfade and gapless | Modern: Spotify, Plexamp, Poweramp; PC: XMPlay, Winamp | Two replayer instances; RAM-scaled | engine | text | M1 | picked F1 | Modern "Crossfade", PC "Crossfade and gapless" |
| Smart fade between songs (analysis-driven, Sweet Fades) | Modern: Plexamp | Render analysis of intro/outro energy | engine + analysis | text | M5 | unhomed | Modern "Sweet Fades" |
| MixRamp overlap tags | Term: mpd | mixrampdb / delay per song | engine | text | M5 | unhomed | Term "Options" |
| Loudness normalisation / ReplayGain | Modern: Spotify, Poweramp; PC: XMPlay, Deadbeef | Render analysis on desktop, shared via DEViLBOX | engine mixer + library | text | M3 | picked F1 | PC "ReplayGain", Modern "Loudness" |
| Loop detect, auto-stop, fade-out at song end | Amiga: Hippo, UADE; PC: Cubic, sidplay, GME | Songlengths, order/pattern revisit, length+fade tags | engine | text | M1 | picked F1 + H B | Amiga "Song length detection", PC "Loop detection", "Fade-out" |
| Song length database (Songlengths.md5, UADE content db, xxh32 key) | Amiga: UADE; PC: sidplayfp | HVSC Songlengths, per-song key | library + engine | text | M1 | picked F1 | Amiga "Song length db", PC "Songlengths.md5" |
| Subsong silence timeout and subsong timeout | Amiga: UADE | Engine silence detector, 512 s default | engine | text | M1 | picked F1 (loop detect) | Amiga "Subsong switch" |
| Subsongs (select, next, prev, one-per-file) | Amiga: Hippo, UADE; PC: sidplay | Subsong count from replayer | engine | text | M0 | picked F1 + H B | Amiga "Subsongs" |
| Random / shuffle | Amiga: Hippo, Chipmachine, UADE; Modern: Spotify | Queue / library | engine + queue | text | M0 | picked F1 + H B | Amiga "Random / play modes" |
| Repeat modes (off, song, list) and OPTION family (consume, single, random) | Term: cmus, mpd | Engine option state | engine | text | M0 | design S3 | Term "Options" |
| Stop-after-current and stop-after-queue | Term: cmus, mpd | Option flag | engine | text | M1 | design S3 | Term "Play queue" |
| Reshuffle command (re-roll a shuffled order) | Modern: Spotify | Queue | engine | text | M3 | unhomed | Modern "Reshuffle" |
| Smart Shuffle (inject suggestions into a list) | Modern: Spotify | DEViLBOX genre + ratings | library | text | M3 | unhomed | Modern "Smart Shuffle" |
| Channel mute / solo | Amiga: Hippo, PT; PC: OCP, OpenMPT, GME | Voice mask in engine | engine | text | M1 | picked F1 + H | PC "Mute/solo per channel", "Channel manager" |
| Pitch and tempo control | PC: GME; Term: termusic (soundtouch) | Replayer SEL_RP_CAN_PITCH | engine | text | M1 | picked F1 + H | PC "Tempo control", Term "Speed control" |
| Resampler / interpolation choice (none, linear, cubic, sinc) | PC: XMPlay, OpenMPT; Amiga: UADE | Mixer setting by tier | mixer | text | M0 | picked F5 | PC "Interpolation filters" |
| Paula LED filter, filter model (A500/A1200), fade, stereo factor | Amiga: Hippo, UADE | Mixer param | mixer | text | M2 | picked F5 + H | Amiga "Filter (LED)", "Paula filter model" |
| Stereo separation / width | PC: OpenMPT, GME | Mixer param | mixer | text | M2 | picked F5 | PC "Stereo separation" |
| Seek (time, relative +/-, pattern:order:row) | Term: cmus; Tracker | Engine position; SEEK verb | engine | text | M1 | picked F11 | Term "seek +1m" |
| Idempotent PAUSE_PLAYBACK vs toggle PAUSE | Term: cmus | Command table | protocol | text | M1 | design S3 | Term "cmus-remote toggles" |
| Volume (relative, logarithmic) | Term: cmus; Modern: MusicBee | Engine mixer | engine | text | M0 | design S3 | Modern "logarithmic volume" |
| Direct (hardware) volume control, bit-perfect path | Modern: Poweramp | Output plugin capability | output plugin | text | M5 | unhomed | Modern "Direct Volume Control" |
| Play scope (library / sorted / playlist / album-only, album-artist-all) | Term: cmus aaa_mode | Library view | engine + library | text | M3 | unhomed | Term "Play scope modes" |
| Next / previous album | Term: cmus | Album grouping | engine + library | text | M3 | unhomed | Term "Next/prev album" |
| Follow mode (cursor follows the playing song) | Term: cmus | Playing key | view model | text | M1 | unhomed | Term "Follow mode" (design S3 line 150 mentions follow in a different sense: unverified) |
| Sleep timer and alarm (timed start) | Hippo; Modern: Poweramp | Wall clock | engine | text | M1 | picked F6 + H | Hippo "Alarm (timed start)" |
| Gapless / preload and cache pre-fetch of next song | Modern: Plexamp, Poweramp | Library prefetch | engine + library cache | text | M3 | picked F2 | Modern "Gapless and advanced pre-caching" |
| Playback history / back-stack and previous restart | Amiga: UADE; Term: bbmp | History table | library + engine | text | M3 | picked F2 | Amiga "Shuffle key, prev/next" |
| Song-change hook running a command | Term: moc OnSongChange | Event stream | scripting | text | M4 | design S16 (D7 Lua) | Term "moc OnSongChange" |
| CD audio playback | Amiga: AmigaAMP | CD device | output plugin | planar | later | unhomed | Amiga "CD audio via AHI" |
| Transport commands as stable verbs (play, pause, stop, next, prev, seek by id) | Term: mpd, musikcube | Stable entry ids | protocol | text | M0 | design S3 | Term "Transport" |

## Library
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Cross-collection merged search with incremental search-as-you-type | Amiga: Chipmachine, bbmp | FTS5 prefix index per source | library | text | M3 | picked F2 + B | Amiga "Cross-collection search" |
| Typed search filters and one query language (regex, numeric ranges, relative time, exact, nesting) | Term: cmus, mpd, Quod Libet; PC: foobar | Field schema, SQLite | library | text | M3 | picked F2 + B | Term "Filter expression", PC "Query syntax" |
| Smart playlists = saved queries (auto playlists, nested saved queries) | Modern: Apple, Navidrome, Swinsian; PC: foobar | Query + ratings + play counts | library | text | M3 | picked F2 | Modern "Smart playlists" |
| Smart-playlist limit and ordering rules | Modern: Apple | Query + limit | library | text | M3 | design S9 (9c views, partial) | Modern "Limit on smart playlists" |
| Home view: recently played, jump back in, Modland changelog | Modern: Spotify, Apple | History + Modland changelog | library + view model | text | M3 | picked F2 | Modern "Home / Jump back in" |
| Ratings (stars), liked list | Modern: Spotify, Apple; Hippo favourites | Library table, DEViLBOX sync | library | text | M3 | picked F2 + B | Modern "Star ratings", "Liked Songs" |
| Favourites with remembered subsong, kept songs | Amiga: Hippo, Chipmachine | Per-song flag + subsong | library cache | text | M3 | picked F2 + H B | Amiga "Favorites with remembered subsong" |
| History, play counts, scrobbling (Last.fm, ListenBrainz, offline queue) | Modern: Navidrome; Term: integration | Play log; 50-per-batch flush | library + scrobble plugin | text | M3 | picked F2, F16 | Term "Last.fm", Modern "Scrobbling" |
| Sources: Modland, HVSC, ASMA, SNDH, ZXArt, Keygenmusic, local, hippoplayer.se | Hippo; bbmp | Per-source index | library sources | text | M3 | picked F2 + H B | Amiga "Modland / HVSC / AMP" |
| Aminet and Modules.pl search sources | Amiga: Hippo | HTTP search APIs | library source | text | M3b | unhomed | Amiga "Modland / HVSC / AMP / Aminet / Modules.pl / radio search" |
| Cache budget, prefetch, companions, offline / smart downloads | Modern: Spotify, Finamp, YT Music; bbmp | Cache table, LRU | library cache | text | M3 | picked F2 | Modern "Offline", "Smart Downloads" |
| History-driven auto-download | Modern: YT Music | Play history | library cache | text | M3 | unhomed | Modern "Smart Downloads" |
| Local folder scan + folder watching (FSEvents, inotify, rescan) | PC: XMPlay; Term: mpd update | OS watcher | library | text | M3 | picked F16 | PC "Track library with folder monitoring" |
| Library tree browse (source > format > artist > file) | Amiga: Hippo; Term: cmus | Catalogue | view model | text | M1 | design S9b | Term "Tree view" |
| Duplicate detection by md5, best-source choice | Modern: Swinsian | files.md5 | library | text | M3 | picked F10 | Modern "Duplicate finder" |
| Hidden / per-song enabled flag (match only checked) | Modern: Apple | files.hidden | library | text | M3 | unhomed (files.hidden exists S9.2, no UI) | Modern "Match only checked items" |
| Generic per-song key/value store (stickers: notes, play counts, scripts) | Term: mpd | Library table | library + protocol | text | M3 | design S3 | Term "Stickers" |
| Per-song overrides (resampler, filter, replayer settings) | Amiga: UADE song.conf | SEL_RP_CONFIG tags | library | text | M2 | picked F15 | Amiga "song.conf" |
| Multiple libraries / multi-user accounts | Modern: Apple, Navidrome | Accounts | library | text | later | unhomed (single-user by D) | Modern "Multi-user" |
| Library stats (counts per period, per format) | Term: bbmp Stats | Play log | library + UI | text | M3 | unhomed | Term "bbmp Stats periods" |
| Podcasts | Modern: MusicBee, Navidrome | RSS | library source | text | out | unhomed (likely out of scope) | Modern "Podcasts" |

## Playlist/Queue
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Queue with play-next, add, clear, reorder; stable entry ids and QUEUE_CHANGES | Modern: Spotify; Term: mpd | Entry ids, version | queue | text | M1 | design S3, S9c | Term "Queue edit" |
| Playlist ops: sort, find, insert, dividers, stealth entries, drag recursive add | Amiga: Hippo | Ordered entries | playlist | text | M1 | picked F7 + H | Amiga "Playlist ops" |
| Playlist formats M3U, PLS, XSPF, HippoPlayer .prg round-trip | Hippo; PC: Winamp | Codecs | playlist codecs | text | M1 | picked F7 + H | Amiga "Shared playlists" |
| Playlist folders tree | Modern: Spotify, Apple | Library table | library | text | M3 | picked F7 | Modern "Playlist folders" |
| Multiple playlists as tabs, playlist search filter | PC: Deadbeef | Playlist objects | playlist + UI | text | M1 | design S9c | PC "Multiple playlists" |
| Radio across collections (shuffle, genre/ratings steering, start radio from song, autoplay) | Amiga: Chipmachine; Modern: Spotify, Apple, Tidal | DEViLBOX genre + ratings | library | text | M3 | picked F7 + B | Modern "Radio / Autoplay" |
| Instant mix / song radio / mixes | Modern: Jellyfin, YT Music | Seed query | library | text | M3 | picked F7 | Modern "Instant mix" |
| Streaming radio stations (Nectarine, SceneSat, Shoutcast) + stream decoders | Amiga: AmigaAMP, Hippo; Term: web | HTTP, mp3/ogg decoders | sources + replayers | text | M5 | picked F7 + H | Amiga "Streaming (Shoutcast...)" |
| Stream-to-disk recording of radio | Amiga: AmiNetRadio | Stream buffer | sources + engine | text | M5 | unhomed | Amiga "AmiNetRadio: stream to disk" |
| Module program save/load and F1-F10 stored program hotkeys | Hippo | .prg files | playlist + shell | text | M1 | unhomed | Hippo "Function keys F1-F10" |
| Mark / unmark / invert selection | Term: cmus | View model selection | view model | text | M1 | design S9b | Term "Mark / unmark" |
| Shared / collaborative playlists | Modern: Spotify | hippoplayer.se, DEViLBOX | library | text | M3 | picked F4 | Modern "Collaborative playlists" |
| Blend (generated playlist from several users' histories) | Modern: Spotify | Multi-user history | library | text | later | unhomed | Modern "Blend" |
| Mixes / editorial playlists | Modern: Tidal | hippoplayer.se published lists | library | text | M3 | design S17 | Modern "Mixes" |
| Persisted cross-device play queue (hand-off resume) | Modern: OpenSubsonic savePlayQueue | Queue snapshot | protocol + library | text | M5 | unhomed | Modern "Gaps" item 6 |
| Command lists with all-or-nothing batch | Term: mpd | Protocol | protocol | text | M5 | unhomed | Term "Protocol ideas" 9 |

## Visuals
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Pattern scroller / track viewer | Amiga: Hippo, Eagleplayer; PC: OCP, OpenMPT | Replayer tracker_info / tracker_cells | viz + replayer hooks | text | M1 | picked F3 + H | PC "Track viewer" |
| Sample / instrument list with click-to-play | Amiga: Hippo, Eagleplayer; Hippo | sample_count / sample_get | viz + UI | text | M1 | picked F3 + H | Amiga "Sample list" |
| Per-voice scopes (Quadra, Hippo, Stereoscope, SpaceScope) | Amiga: Hippo, Eagleplayer; PC: OCP | Per-voice buffers | viz | text | M1 | picked F3 + H | Amiga "Scopes" |
| Spectrum / FFT analyzer, level graph, level meter | Amiga: Eagleplayer; PC: OCP, Winamp | FFT bands or integer bands | viz | text | M1 | picked F3 + H | Amiga "Analyzers" |
| VU / peak power meters | PC: OCP; Amiga: PT | VU taps | viz | text | M1 | picked F3 | PC "Peak power view" |
| Channel viewer and note dots | PC: OCP | voice_info note/instr taps | viz | text | M1 | picked F3 | PC "Channel viewer", "Note dots" |
| Phase viewer (stereo phase scope) | PC: OCP | L/R mix tap | viz | text | M1 | picked F17 | PC "Phase viewer" |
| Chip register panels (SID, AY/YM, Paula, SN76489) with new regs() hook | PC: VGMPlay, sidplay; Hippo PSID scope | Register shadow from replayer tags | viz + replayer hook | text | M4 | picked F3 | PC "Chip register panels" |
| GPU visualizers (Milkdrop/projectM class), Canvas-style looping visuals | PC: Winamp; Modern: Spotify | Spectrum + 576-sample waveform | viz | GPU | M5 | picked F3 | PC "vis_ plugins", Modern "Canvas" |
| Generated artwork per format/chip | Modern: Apple/Spotify art; Selecta | Format, chip | viz | GPU | M5 | picked F3 | Modern "Canvas" |
| Cover art display (embedded, Kitty/iTerm2/sixel) | Term: termusic, ncmpcpp; Modern | Art per item | UI | RTG | M4 | design S7 | Term "Cover art in Kitty/iTerm2" |
| Artwork fetch over protocol (chunked ALBUMART) | Term: mpd | Art bytes | protocol | text | M5 | design S3 | Term "Art" |
| Multiple scope windows at once with remembered positions | Amiga: Hippo | UI prefs | viz + UI | planar | M4 | parity H | Amiga "Scopes" |
| Signal path display (decoder > DSP > output) | Modern: Roon | Engine routing state | UI | text | M5 | unhomed | Modern "Signal path display" |
| Fullscreen / video-style visual mode | PC: Winamp | Viz surface | viz | GPU | M5 | unhomed | PC "vis_ plugins" (grep: no fullscreen in design) |
| Synced lyrics / tracker module message as lyrics | Modern: Spotify, Jellyfin; Term: ncmpcpp | .lrc, module message | UI + library | text | M5 | unhomed | Modern "Lyrics", Term "ncmpcpp lyrics" |

## Chip/format
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| SID 6581/8580 model and filter curve per song | PC: sidplayfp; bbmp | Replayer config tag | replayer tags | text | M5 | picked F5 + B | PC "ReSID engine" |
| Hardware SID output (exSID, SIDBlaster, USBSID-Pico, ZorroSID, Trinity) | PC: sidplayfp; Hippo | Device driver | output plugin | text | M5 | unhomed | PC "Hardware SID", Hippo "Hardware outputs" |
| HVSC STIL, composer, release metadata panel | PC: sidplayfp; bbmp | STIL.txt, Songlengths | library + UI | text | M3 | picked F5 + B | PC "STIL" |
| AY/YM layout (ABC/ACB), clock, interpolation parameters | PC: zxtune | Replayer config tags | replayer tags | text | M5 | design S4 (F15 config) | PC "Chip parameters" |
| Per-replayer config (Eagleplayer EP_LoadPlConfig, UADE eagleplayer.conf) | Amiga: Eagleplayer, UADE | SEL_RP_CONFIG tags | replayer SDK | text | M2 | picked F15 + H | Amiga "Replayer config" |
| Format auto-detect by content, staged probe | Amiga: UADE; PC: zxtune | Prefix probe | replayer SDK | text | M0 | design S4 | PC "Format auto-detect" |
| Format coverage = union bbmp + HippoPlayer (445 formats) | Hippo; bbmp | format-coverage doc | replayers | text | M2-M5 | design S9a (F14) | Amiga "150+ formats" |
| Console/arcade/computer sets (PSF, SSF, USF, GSF, SPC, VGM) via emulator cores | PC: Audio Overload, GME, VGMPlay | Cores as RENDER replayers | replayers | text | M5 | design S4 | PC "Audio Overload" |
| External replayers (Eagleplayer, DeliTracker genies) loadable | Amiga: Eagleplayer, DeliTracker | Plugin dir | replayer loader | text | M2 | design S9a, S10 | Amiga "External replayers" |
| GME/VGM intro length, loop length, GD3 tags | PC: GME, VGMPlay | Format tags | replayer tags | text | M5 | picked F1 (loop detect) | PC "Track info" |
| Per-chip volume and mute (VGM) | PC: VGMPlay | Chip state | engine | text | M5 | picked F1 (mute) | PC "VGMPlay front ends" |
| Hardware mixer outputs (AmiGUS 32 ch, MHI MP3 decoder, Trinity audio) | Hippo | Hardware APIs | output plugins | planar | M4 | unhomed | Hippo "AmiGUS", "MHI", "Trinity" |
| Paula direct output (VOICE players) vs mixer path | Amiga: Hippo, Eagleplayer | Routing policy | output | planar | M2 | design S5 | Amiga "Output: Paula" |
| 14-bit / Toccata / 8-bit amplifier output modes | Amiga: Eagleplayer, DeliTracker | Mixer | mixer | planar | M2 | design S5 (paula14) | Amiga "Amplifiers" |

## Output/DSP
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| DSP chain plugins (EQ 9-31 band, reverb, stereo expander, limiter, bass expansion) | PC: Winamp, XMPlay, foobar; Modern: MusicBee | PCM stream | DSP plugin kind | text | M5 | picked F6 | PC "DSP chain" |
| Parametric EQ, upsampling, convolution, upmix (5.1) | Modern: Roon, Poweramp, MusicBee | PCM float | DSP plugin kind | GPU/RTG | M5+ | unhomed | Modern "DSP engine", "upmix" |
| Headphones mode (crossfeed) | Amiga: UADE; PC: Deadbeef | PCM | DSP plugin | text | M1 | picked F18 + H | Amiga "headphones" |
| Record / render to WAV or FLAC faster than realtime | PC: XMPlay; Hippo | Same path as golden tests | engine + wav output | text | M0 | picked F6 | PC "WAV writing" |
| Lossy transcode / render to MP3, AAC, Opus | Modern: Navidrome, Strawberry | Encoder | converter | text | M5 | unhomed | Term "Transcoding" |
| Output plugins: AHI, AmiGUS, WASAPI, ASIO, CoreAudio, DirectSound | PC: XMPlay; Amiga: AmigaAMP | Output ABI | output plugins | text | M0-M2 | design S5 | PC "Output plugins" |
| Network outputs: Chromecast, AirPlay, UPnP/DLNA, Roon zones | Modern: Roon, Tidal | Renderer protocols | output plugins | text | M5 | picked F6 | Term "Chromecast output" |
| Multi-zone grouping with synced clocks | Modern: Roon | Clock sync | output + protocol | text | later | unhomed | Modern "Zones" |
| Per-output enable/disable, independent partitions | Term: mpd | Output list | engine | text | M5 | unhomed | Term "Partitions", "Outputs" |
| Doublebuffered sample data output | Hippo | Output buffers | output | planar | M2 | unhomed | Hippo "Doublebuffering" |
| Atmos / immersive audio | Modern: Tidal | Streamed content | none | - | out | unhomed (declined candidate) | Modern "Dolby Atmos" |
| Bit depth, sample rate, device buffer settings | PC: XMPlay | Output config | output | text | M0 | design S5 | PC "Output plugins" |

## Remote/social
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Device hand-off (Connect style) | Modern: Spotify, Tidal | Queue + position | protocol | text | M5 | picked F4 | Modern "Connect" |
| Web remote served by the engine | Term: musikcube, mpd clients | HTTP/JSON | protocol | text | M5 | picked F4 | Term "musikcube" |
| Newline text protocol, selecta-cli, HTTP/JSON, MCP server | Term: cmus-remote, mpd | commands.def / events.def | protocol | text | M0/M5 | picked F12 | Term "cmus-remote" |
| IDLE subsystem subscription, SEEKED, SCROBBLE_DUE events | Term: mpd | Event table | protocol | text | M1 | design S3 | Term "idle" |
| Remote auth (password, LAN-only) | Term: musikcube, mpd | Config | protocol | text | M5 | design S8 | Term "Password auth" |
| Collaborative lists: hippoplayer.se publish, DEViLBOX votes and comments | Hippo; Modern: Spotify | hippoplayer.se API, DEViLBOX | library | text | M3 | picked F4 + H | Modern "Collaborative playlists" |
| Share links selecta:// and devilbox URLs | Modern: Spotify | Song key + position | library | text | M3 | picked F4 | Modern "Share links" |
| Hosted public share URLs | Modern: Navidrome | Server | remote | text | later | unhomed | Modern "Sharing" |
| Party mode voting queue / song requests | Term: Nectarine; DEViLBOX | Web remote | protocol + library | text | M5 | picked F9 | Term "Nectarine song requests" |
| Client-to-client messaging channels | Term: mpd | Protocol | protocol | text | later | unhomed | Term "Client messaging" |
| Mounts, neighbours (network storage discovery) | Term: mpd | OS APIs | sources | text | later | unhomed | Term "Mounts/neighbors" |
| Discord rich presence | Modern | Event stream | integration plugin | text | M5 | picked F12 | Term "Integration" |
| Named AI DJ / LLM playlist prompts (Guest DJ, Sonic Sage) | Modern: Plexamp | MCP server | external client | text | M5 | design S8 (MCP; built-in DJ unhomed) | Modern "Guest DJ", "Sonic Sage" |
| Audio-feature similarity and sonic paths | Modern: Plexamp | Render analysis vectors | library + analysis | text | M5 | unhomed | Modern "Sonic analysis" |

## Files/sources
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Archive decrunch (lha, lzx, zip, gzip, xpk, PowerPacker; xfd/xad on Amiga), archives as folders | Amiga: Hippo, Eagleplayer, DeliTracker; PC: XMPlay | xfdmaster, lha libs | library archives | text | M3 | picked F8 + H B | Amiga "Archive handling", Hippo "Archives", "Packed modules" |
| Nested archives / multi-module containers as browse level | PC: XMPlay, zxtune | Container parsers | library archives | text | M3 | picked F8 (nesting unverified) | PC "Archive/container browsing" |
| Companion fetch (sample sets, multi-file formats) | bbmp | Modland path rules | library cache | text | M3 | picked F8 + B | Amiga "Modland search" |
| Module converters (NoiseConverter, PT-clone) | Amiga: Eagleplayer; PC: Deadbeef | Module bytes | converter plugins | text | M5 | picked F8 | Amiga "Converters" |
| Rippers (Eagleripper) | Amiga: Eagleplayer | Raw memory | tool | text | out | unhomed (non-goal candidate) | Amiga "Rippers" |
| Sample saver / sample export | Amiga: Eagleplayer, UADE | sample_get | engine | text | M1 | picked F11 | Amiga "Sample saver" |
| OS file integration: drag-drop, AppWindow/AppIcon, open-with, iconify | Hippo; Amiga | OS APIs | UI plugins per OS | planar | M4 | picked F8 + H | Hippo "Window iconify" |
| File requester (reqtools / ASL / native) | Hippo | OS requester | UI plugin | planar | M4 | unhomed | Hippo "reqtools.library" |
| HTTP/HTTPS download, plain-HTTP DEViLBOX mirror for Amiga | Amiga: Hippo, AmigaAMP | bsdsocket / TLS | library fetch | text | M3 | design S16 | Amiga "HTTP download" |
| Network transport via external tool (UHCBIN), pipe streaming names | Hippo | External process | sources | text | n/a | unhomed (superseded by S9 fetch; confirm drop) | Hippo "UHCTools", "PIPE:" |
| Clipboard URL monitoring | PC: XMPlay | OS clipboard | shell | text | M5 | unhomed | PC "Clipboard URL monitoring" |
| Directory/file browser view | Amiga: Eagleplayer; Term: ncmpcpp | Local source | view model | text | M1 | design S9b | Amiga "Directory list viewer" |

## Metadata
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Composer pages across sources, author normalisation | bbmp; DEViLBOX | DEViLBOX songdb | library | text | M3 | picked F10 | Modern "Library-centric metadata" |
| Similar and also-by | Modern: Roon, Plexamp | DEViLBOX genre + ratings | library | text | M3 | picked F10 | Modern "Sonically similar" |
| Local notes + tag edits, push to DEViLBOX | PC: OCP, foobar; Term: ncmpcpp | Writable tag store | library | text | M3 | picked F10 | PC "Metadata editing" |
| Batch / multi-track tag editing (masstagger) | PC: foobar; Modern: Swinsian | Tag store | library | text | M3 | unhomed | PC "Masstagger" |
| Auto-tagging via fingerprint (AcoustID, MusicBrainz) | Term: Strawberry; Term: mpd getfingerprint | Fingerprint service | library | text | later | unhomed | Term "Strawberry", "getfingerprint" |
| Module info / infobox (name, author, format, length, size, stats) | Amiga: Hippo, Eagleplayer; Hippo | MI tags / SEL_META_* | UI + replayer tags | text | M1 | picked F11 + H B | Amiga "Module info window" |
| Module comment file / module message | Hippo; Tracker | Comment file, replayer tags | library + UI | text | M1 | picked F11 (message); comment file unhomed | Hippo "Module comment file" |
| Per-format info pane (game, system, ripper, year from PSF-style tags) | PC: Audio Overload, GME | Replayer tags | UI | text | M5 | design S4 (metadata tags) | PC "Tag display" |
| Metadata sources: DEViLBOX genre, votes, ratings, loudness | DEViLBOX | DEViLBOX server | library overlay | text | M3 | design S9 | Term "Web players" |
| External lookup (Demozoo / scene databases) | Term: bbmp | Song key to URL | library + UI | text | M4 | unhomed | Term "bbmp lookup web / demozoo" |
| "Sounds wrong" flag (report mis-played songs) | Term: bbmp | Flag store, DEViLBOX | library | text | M3 | unhomed | Term "bbmp sounds wrong list" |
| Facets (genre, rating, year, format counts), user-defined | PC: foobar; Modern: Roon | Field index | library + view model | text | M3 | design S9b | PC "Facets" |
| Lyrics and synced lyrics fetch | Modern: Spotify, Jellyfin; Term: ncmpcpp | .lrc / online | library | text | M5 | unhomed (see Visuals) | Modern "Synced lyrics" |
| Last.fm / MusicBrainz artist info fetch | Term: ncmpcpp, Strawberry | Online | library | text | later | unhomed | Term "ncmpcpp lyrics and last.fm" |

## Tracker extras
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Module info panel (format, channels, patterns, instruments, size, length, message) | Hippo; bbmp | SEL_META tags | replayer tags + UI | text | M1 | picked F11 + H B | Amiga "Module info window" |
| Seek by pattern / order position | Tracker players | POSITION order/row | engine | text | M1 | picked F11 | Term "seek" |
| Sample export to WAV / 8SVX | Amiga: Eagleplayer | sample_get | engine | text | M1 | picked F11 | Amiga "Sample saver" |
| Global hotkeys and user key bindings in every UI | Amiga: Hippo, DeliTracker; Term: cmus bind | Key map | shell | text | M1 | picked F11 + H | Amiga "Hotkeys" |
| Vi-style command mode with tab completion | Term: cmus | Command table | text UI | text | M1 | unhomed | Term "Vi-style command mode" |
| Run external command / shell on selection | Term: cmus | Process spawn | shell + scripting | text | M4 | unhomed | Term "run/shell" |
| Position-relative arguments (+10, -10%) | Term: cmus | Command parser | protocol | text | M1 | design S3 | Term "seek +1m" |
| Cue list / user-timed programs (Userprogs, manager) | Amiga: DeliTracker, Eagleplayer | Engine state | engine | text | M3 | picked F9 (cue points) | Amiga "Cue list", "Userprogs" |
| Task priority, double-buffer tuning | Hippo | OS | settings | planar | M2 | unhomed | Hippo "Task priority setting" |

## Integration
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| MPRIS (Linux), macOS Now Playing + remote commands, Windows SMTC, media keys | Term: integration | STATE / POSITION / SONG_OPENED | integration plugins | text | M5 | picked F12 | Term "MPRIS", "MPNowPlayingInfoCenter" |
| selecta-cli protocol client | Term: cmus-remote, mocp | Protocol | CLI | text | M0 | picked F12 | Term "cmus-remote" |
| HTTP/JSON API and MCP server | Term: musikcube, Plexamp | Protocol | protocol | text | M5 | picked F12 | Term "musikcube" |
| Lua runtime and ARexx port mapping 1:1 to commands | Amiga: Hippo, AmigaAMP; DeliTracker | Command/event protocol | scripting plugin | text | M4 | picked F12 + H | Amiga "ARexx" |
| Discord rich presence | Modern | Event stream | integration plugin | text | M5 | picked F12 | Term "Discord" |
| Scrobbling to Last.fm / ListenBrainz / Subsonic | Term; Modern | SCROBBLE_DUE | integration plugin | text | M3 | picked F2 | Term "Last.fm" |
| Subsonic / OpenSubsonic server API compatibility | Modern: Navidrome | API mapping | protocol | text | later | unhomed | Modern "Navidrome" |
| Subsonic / Tidal / Qobuz as sources | Term: Strawberry | Account | library source | text | later | unhomed | Term "Subsonic streaming" |
| Device sync (USB, MTP) with format conversion | Modern: MusicBee; Term: Strawberry | Converter | tool | text | out | unhomed | Term "Device sync" |
| Scripted hook on song change | Term: moc | Event stream | scripting | text | M4 | design S16 (D7) | Term "OnSongChange" |
| Reflection command (commands, tagtypes) generated from commands.def | Term: mpd | commands.def | protocol | text | M5 | unhomed (not found in design S3 by grep) | Term "Reflection" |

## UI/views
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Switchable view styles: regular, tree, drilldown, cover flow, grid, compact | Modern: Apple, foobar; Term: cmus | View model | view model + UI | text (cover flow/grid RTG) | M1 | picked F3 | Modern "Library presentation" |
| Column browser / Miller columns | Modern: Apple; PC: foobar | Indexed fields | UI | text | M1 | picked F3 | Modern "Column browser" |
| Multiple screens (Browse, Search, Queue, Playlists, Files, History, Info, Stats) | Term: bbmp, cmus 7 views, ncmpcpp | View model | text UI | text | M1 | parity B | Term "bbmp tabs" |
| Configurable columns / title-format strings and status-line format | PC: foobar, Deadbeef; Term: cmus | Format-string engine | UI | text | M3 | unhomed | PC "Configurable playlist columns" |
| Skins (Winamp wsz, AmigaAMP, MUI themes) and design tokens | PC: Winamp; Amiga: AmigaAMP | Skin loader | UI plugins | planar | M5 | design S16 (D8) | PC "Skins" |
| Mini mode / compact player | PC: XMPlay | Layout | UI | text | M4 | design S9b (compact style) | PC "Mini mode" |
| Live filter (type to filter) | Term: cmus | Query | UI | text | M3 | picked F2 | Term "Live filter" |
| Multiple UIs switchable on the fly (text, SDL, MUI, GadTools, web) | Amiga: Eagleplayer GUIs; Roon | UI_SWITCH | shell | text | M1 | design S7 | Amiga "Engines and GUIs" |
| Two-screen player/search layout | Amiga: Chipmachine | View model | UI | text | M1 | design S7 | Amiga "Two-screen UI" |
| Home / Jump-back-in screen | Modern | History | UI | text | M3 | picked F2 | Modern "Home" |
| Tag editor screen | Term: ncmpcpp | Tag store | UI | text | M3 | picked F10 | Term "ncmpcpp tag editor" |
| Stats, Activity modal, clock, outputs screens | Term: bbmp, ncmpcpp | Play log | UI | text | M3 | unhomed (see Library stats) | Term "ncmpcpp screen list" |
| Public screen / own screen selection, screen modes | Hippo, Amiga | Intuition | Amiga UI plugin | planar | M4 | unhomed | Hippo "Public screen selection" |
| AHI indicator in infobox | Hippo | Output name | UI | planar | M2 | unhomed | Hippo "AHI indicator" |
| Help overlay, command palette (Ctrl+K) | Term: DEViLBOX | Command table | UI | text | M3 | unhomed | Term "DEViLBOX global keys" |
| Undo / redo of queue edits | Term: DEViLBOX | Edit log | queue | text | M3 | unhomed | Term "Ctrl+Z/Y" |
| Two-deck DJ view, bar-synced Auto DJ | Term: DEViLBOX | Decks | engine | text | out | design S16 (declined) | Term "DEViLBOX DJ decks" |

## Settings/system
| Feature | Origin | Data needed (source) | Selecta component | Tier | MS | Status | Source doc |
|---|---|---|---|---|---|---|---|
| Preferences store (global, per-UI, per-source) | Amiga: Hippo, UADE | Config file | shell | text | M1 | unhomed | Amiga "Preferences" |
| ToolType / environment configuration | Hippo; Amiga | Tooltypes | shell | planar | M2 | unhomed | Hippo "ToolType parsing" |
| Machine-scaled features from sel_machine (RAM, CPU, RTG) | Selecta | sel_machine | shell | all | M1 | design S16 | Amiga (design decision) |
| 68000 / Kickstart 1.2 reduced build | Hippo | Build flag | build | text | later | design S16 | Hippo "Kickstart 1.2+ support" |
| HippoPlayer.group / module location setting | Hippo | Config | settings | planar | M2 | unhomed | Hippo "HippoPlayer.group" |
| Optional PS3M config file | Hippo | Config | settings | planar | M2 | unhomed | Hippo "PS3M config" |
| Keyfile / registration | Hippo | none | none | - | out | unhomed (drop; disabled since 2.46) | Hippo "Keyfile" |
| Stealth / iconify to Workbench, commodity | Amiga: Hippo, DeliTracker | commodities | UI plugin | planar | M4 | picked F8 | Amiga "Stealth / iconify" |
| Daemon / headless service mode | Term: musikcube, mpd, termusic | Engine | engine | text | M0 | design S8 | Term "musikcube" |
| Kiosk / lock mode | Selecta | Config | shell | text | M5 | design S9 (lock; unverified) | design grep "lock" |

## Counts

Counted from the tables: rows per area with a line scan of each table (header rows excluded); status totals with `grep -c` on the status cell. Not counted from a structure that produces the rows (the catalogue is hand-merged from the five input docs), so row totals are a measurement of this file, not of the inputs.

| Area | Rows |
|---|---|
| Playback | 31 |
| Library | 21 |
| Playlist/Queue | 16 |
| Visuals | 16 |
| Chip/format | 14 |
| Output/DSP | 12 |
| Remote/social | 14 |
| Files/sources | 12 |
| Metadata | 14 |
| Tracker extras | 9 |
| Integration | 11 |
| UI/views | 17 |
| Settings/system | 10 |
| Total | 197 |

- picked F<n>: 89 (of which also marked parity H and/or B: 32)
- design S<n> (specified in a design section, no F ID): 43
- parity H / B only: 2
- unhomed: 63
- check: 89+43+2+63 = 197 vs total 197

## Unhomed

Every row with status unhomed, grouped by area. Source doc in brackets. Hippo NEW rows are included; "(declined)" candidates are flagged in the table.

### Playback
- Smart fade between songs (analysis-driven, Sweet Fades) [Modern "Sweet Fades"]
- MixRamp overlap tags [Term "Options"]
- Reshuffle command (re-roll a shuffled order) [Modern "Reshuffle"]
- Smart Shuffle (inject suggestions into a list) [Modern "Smart Shuffle"]
- Direct (hardware) volume control, bit-perfect path [Modern "Direct Volume Control"]
- Play scope (library / sorted / playlist / album-only, album-artist-all) [Term "Play scope modes"]
- Next / previous album [Term "Next/prev album"]
- Follow mode (cursor follows the playing song) [Term "Follow mode" (design S3 line 150 mentions follow in a different sense: unverified)]
- CD audio playback [Amiga "CD audio via AHI"]

### Library
- Aminet and Modules.pl search sources [Amiga "Modland / HVSC / AMP / Aminet / Modules.pl / radio search"]
- History-driven auto-download [Modern "Smart Downloads"]
- Hidden / per-song enabled flag (match only checked) [Modern "Match only checked items"]
- Multiple libraries / multi-user accounts [Modern "Multi-user"]
- Library stats (counts per period, per format) [Term "bbmp Stats periods"]
- Podcasts [Modern "Podcasts"]

### Playlist/Queue
- Stream-to-disk recording of radio [Amiga "AmiNetRadio: stream to disk"]
- Module program save/load and F1-F10 stored program hotkeys [Hippo "Function keys F1-F10"]
- Blend (generated playlist from several users' histories) [Modern "Blend"]
- Persisted cross-device play queue (hand-off resume) [Modern "Gaps" item 6]
- Command lists with all-or-nothing batch [Term "Protocol ideas" 9]

### Visuals
- Signal path display (decoder > DSP > output) [Modern "Signal path display"]
- Fullscreen / video-style visual mode [PC "vis_ plugins" (grep: no fullscreen in design)]
- Synced lyrics / tracker module message as lyrics [Modern "Lyrics", Term "ncmpcpp lyrics"]

### Chip/format
- Hardware SID output (exSID, SIDBlaster, USBSID-Pico, ZorroSID, Trinity) [PC "Hardware SID", Hippo "Hardware outputs"]
- Hardware mixer outputs (AmiGUS 32 ch, MHI MP3 decoder, Trinity audio) [Hippo "AmiGUS", "MHI", "Trinity"]

### Output/DSP
- Parametric EQ, upsampling, convolution, upmix (5.1) [Modern "DSP engine", "upmix"]
- Lossy transcode / render to MP3, AAC, Opus [Term "Transcoding"]
- Multi-zone grouping with synced clocks [Modern "Zones"]
- Per-output enable/disable, independent partitions [Term "Partitions", "Outputs"]
- Doublebuffered sample data output [Hippo "Doublebuffering"]
- Atmos / immersive audio [Modern "Dolby Atmos"]

### Remote/social
- Hosted public share URLs [Modern "Sharing"]
- Client-to-client messaging channels [Term "Client messaging"]
- Mounts, neighbours (network storage discovery) [Term "Mounts/neighbors"]
- Audio-feature similarity and sonic paths [Modern "Sonic analysis"]

### Files/sources
- Rippers (Eagleripper) [Amiga "Rippers"]
- File requester (reqtools / ASL / native) [Hippo "reqtools.library"]
- Network transport via external tool (UHCBIN), pipe streaming names [Hippo "UHCTools", "PIPE:"]
- Clipboard URL monitoring [PC "Clipboard URL monitoring"]

### Metadata
- Batch / multi-track tag editing (masstagger) [PC "Masstagger"]
- Auto-tagging via fingerprint (AcoustID, MusicBrainz) [Term "Strawberry", "getfingerprint"]
- External lookup (Demozoo / scene databases) [Term "bbmp lookup web / demozoo"]
- "Sounds wrong" flag (report mis-played songs) [Term "bbmp sounds wrong list"]
- Lyrics and synced lyrics fetch [Modern "Synced lyrics"]
- Last.fm / MusicBrainz artist info fetch [Term "ncmpcpp lyrics and last.fm"]

### Tracker extras
- Vi-style command mode with tab completion [Term "Vi-style command mode"]
- Run external command / shell on selection [Term "run/shell"]
- Task priority, double-buffer tuning [Hippo "Task priority setting"]

### Integration
- Subsonic / OpenSubsonic server API compatibility [Modern "Navidrome"]
- Subsonic / Tidal / Qobuz as sources [Term "Subsonic streaming"]
- Device sync (USB, MTP) with format conversion [Term "Device sync"]
- Reflection command (commands, tagtypes) generated from commands.def [Term "Reflection"]

### UI/views
- Configurable columns / title-format strings and status-line format [PC "Configurable playlist columns"]
- Stats, Activity modal, clock, outputs screens [Term "ncmpcpp screen list"]
- Public screen / own screen selection, screen modes [Hippo "Public screen selection"]
- AHI indicator in infobox [Hippo "AHI indicator"]
- Help overlay, command palette (Ctrl+K) [Term "DEViLBOX global keys"]
- Undo / redo of queue edits [Term "Ctrl+Z/Y"]

### Settings/system
- Preferences store (global, per-UI, per-source) [Amiga "Preferences"]
- ToolType / environment configuration [Hippo "ToolType parsing"]
- HippoPlayer.group / module location setting [Hippo "HippoPlayer.group"]
- Optional PS3M config file [Hippo "PS3M config"]
- Keyfile / registration [Hippo "Keyfile"]

