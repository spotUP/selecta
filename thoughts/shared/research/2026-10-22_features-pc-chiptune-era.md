---
date: 2026-10-22
topic: Feature inventory of PC-era module and chiptune players versus the Selecta design
tags: [research, features, players, chiptune, tracker, library, visualizers]
status: final
---

# PC-era module and chiptune player features

Method: 5 pages fetched (OpenCubicPlayer README, libsidplayfp README, XMPlay page; Deadbeef and zxtune GitHub URLs returned 404). Everything not backed by a fetched page is from prior knowledge and marked "(unverified)". Design doc = `thoughts/shared/plans/2026-10-22-architecture-design.md` (sections 1-17 read by heading, keywords grepped: no hits for dsp, crossfade, fade, replaygain, skin, interpolation, songlength, stil, facet, smart, column, cover, register).

Source shorthand: OCP = https://github.com/mywave82/opencubicplayer ; XMP = https://www.un4seen.com/xmplay.html ; SPF = https://github.com/libsidplayfp/libsidplayfp ; D = design section.

## 1. Cubic Player / Open Cubic Player
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Track (pattern) viewer | Text view of pattern rows during play (README names "Track viewer", key t; no separate pattern view listed) | Pattern cells per row/channel | OCP | yes, D4 `tracker_info`/`tracker_cells` hooks |
| Channel viewer | Per-channel text view: note, instrument, volume, pan, mute | Voice info per channel | OCP | yes, D4 `voice_info`, D6 note/instrument taps |
| Note dots | Dots on a note/channel grid for active notes | Note + volume per voice | OCP | partial, D6 gives note/instrument taps; no dot view |
| Oscilloscope | Per-channel/mixed scope | Scope buffers per voice | OCP | yes, D6 |
| FFT analyzer, text + hi-res with history | Spectrum bars, peak history | FFT bands | OCP | yes, D6 FFT bands |
| Peak power (VU) view | Level meters | VU taps | OCP | yes, D6 VU |
| Phase viewer | Stereo phase plot (OCP key b) | L/R sample stream | OCP | no (needs L/R mix tap, not in D6 list) |
| File selector + playlist | Browser with playlist, Tab to switch, Insert/Delete | Dir listing | OCP | yes, D9 + D17 |
| Toggleable file info columns (filename, title) | alt+i toggles columns | Per-file metadata | OCP | partial, D4 metadata sink exists; column config in UI not specified |
| Metadata editing | alt+e edit tags | Writable tag store | OCP | no (non-goal on editing songs, D1; tag editing not addressed) |
| Modland browsing | Built-in curl browser of modland.com with local DB | Modland index | OCP | yes, D9 (Modland via DEViLBOX, local flat index) |
| Multi-format via libs (GME, AdPlug, TiMidity++, SID, YM, AY, SNDH, HVL) | One host, many backends | Replayer adapters | OCP | yes, D1 non-goal "adapt, never fork", D4 |
| Loop detection | Not described on README; classic Cubic Player showed loop/pass count (unverified) | Order/pattern revisit tracking | OCP (not stated) | partial, D4 "repeat count" in engine; no detection |
| Mute/solo per channel | Channel viewer toggles | Voice mute mask | (unverified) | yes, D4 mutes in engine |

## 2. XMPlay
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Input format plugins | Plugins add formats (retro game, tracker) | Plugin ABI | XMP | yes, D4/D10 |
| Archive plugins + nested ZIP/GZIP | Play files inside archives | Archive decrunch | XMP | yes, D9 "archive decrunch" (nesting unspecified) |
| Output plugins (DirectSound, ASIO, WASAPI) | Selectable backend | Output ABI | XMP | yes, D5 |
| DSP: 9-band EQ, reverb | Built-in effects after decode | PCM float stream | XMP | no (no DSP stage in D3/D5) |
| Crossfade and gapless | Overlap/skip silence between tracks | Two decode instances or lookahead | XMP | no |
| ReplayGain | Per-track/album gain | Gain tags or scan | XMP | no |
| Interpolation options | Selectable (linear, cubic, etc.) for tracker mixing (page lists keyword only) | Mixer setting | XMP (keyword only) | no (mixer choice not exposed; D4 says engine mixer) |
| Chip panel | Per-chip/channel info and mutes for chip formats (unverified) | Voice/chip state | (unverified) | partial, D4 voice_info |
| Mini mode | Compact window (keyword only on page) | UI layout | XMP | partial, D7 UI plugins permit it, not specified |
| Skins | Customizable skins | Skin format | XMP | no (UI plugins D7 only) |
| Track library with folder monitoring | Library auto-updates | FS watcher + DB | XMP | partial, D9 catalog; watching not specified |
| Streaming (HTTP/FTP/Shoutcast/HLS), WAV writing | Network input, render to file | Stream source, file sink | XMP | partial, D9 sources via fetch(); WAV writer: no |
| Clipboard URL monitoring, scrobbling | Desktop integration plugins | n/a | XMP | no |

## 3. Winamp (all unverified; no page fetched)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| in_ input plugins | Format decoders | Plugin ABI | (unverified) | yes, D4 |
| out_ output plugins | Audio sinks | Output ABI | (unverified) | yes, D5 |
| dsp_ plugins | Modify PCM in chain | PCM stream | (unverified) | no |
| vis_ plugins (e.g. Milkdrop) | Draw from FFT/waveform | Spectrum + 576-sample waveform | (unverified) | yes for the contract (D6 viz plugins + surfaces); Milkdrop-class needs GPU surface: partial |
| gen_ general plugins (Media Library) | Library UI, queries, playlists | Metadata DB | (unverified) | partial, D9/D17 |
| Classic and modern skins | Bitmap skins | Skin format | (unverified) | no |
| Crossfade | Output-side overlap | Overlap buffers | (unverified) | no |
| Playlist editor (sort, queue, reorder, M3U/PLS) | Editable list | sel_playlist | (unverified) | yes, D17 (editing ops not listed) |
| Equalizer + preamp | 10-band | PCM | (unverified) | no |

## 4. ModPlug Player / OpenMPT (player side; unverified)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Interpolation filters (none, linear, cubic, sinc/polyphase) | Mixer resampler choice | libopenmpt render param | (unverified) | no, but libopenmpt adapter could pass it; D4 silent |
| Stereo separation | Slider 0-200 | libopenmpt param | (unverified) | yes, D4 (engine-owned) |
| Channel manager | Per-channel mute/solo, quick toggles | Mute mask | (unverified) | yes, D4 mutes |
| Pattern display | Scrolling pattern with highlight | tracker_cells | (unverified) | yes, D4 |
| Volume ramping, amplification, DSP (reverb, bass expansion, noise reduction) | Post-mix effects | PCM | (unverified) | no |
| Repeat count / loop and subsong selection | Plays N loops | Song position | (unverified) | partial, D4 repeat count; subsong in D17 |

## 5. Audio Overload (Richard Bannister; unverified)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Console/arcade/computer set formats (PSF/PSF2, SSF, DSF, USF, GSF, SPC, QSF, SID, etc.) via emulation | Each format = emulator core | Format cores | (unverified) | partial, D4 RENDER kind can host them; no cores listed |
| Tag display (title, artist, game, year, ripper) from PSF-style tags | Tag block in file | Parsed tags | (unverified) | yes, D4 metadata sink |
| Playlist with default length/fade from tags | Length+fade stored in tag | length, fade tags | (unverified) | no (no length/fade metadata) |
| Per-format info pane | Shows hardware, ripper | Metadata | (unverified) | partial |

## 6. SIDPLAY2 / sidplayfp
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| ReSID/ReSIDfp engine, 6581 vs 8580 chip model | User selects model, or file default | Model enum | SPF (needs libresidfp for full quality) | no (D1 names libsidplayfp as adapted lib; model selection not specified) |
| Filter curve settings | 6581 filter curve/strength, on/off | Filter params | SPF (not stated in README) (unverified) | no |
| Songlengths.md5 | MD5 of tune -> per-subsong length | HVSC Songlengths file | (unverified) | no |
| STIL | Tune info text per file/subsong (README confirms libstilview) | HVSC STIL.txt | SPF (file list only) | no |
| PSID/RSID info | Header: title, author, released, load/init/play, clock PAL/NTSC, SID count, model | PSID header | SPF (PSID mentioned only) | partial, D4 metadata sink could carry it |
| HVSC browser | Tree browse of collection | HVSC path index | (unverified) | yes, D9 (HVSC via DEViLBOX) |
| Fade-out at song end | Default play time then fade | Length, fade ms | (unverified) | no |
| Hardware SID (exSID, USBSID) | Output to real chip | Device driver | SPF | no (D5 outputs are PCM/voice sinks) |
| Subsong select | Next/prev tune within file | Subsong count | (unverified) | yes, D4 `start(subsong)`, D17 |

## 7. Deadbeef (unverified; GitHub URL 404 in fetch)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Plugin system (decoders, DSP, output, GUI, misc) | Shared-object plugins with API version | Plugin ABI | (unverified) | yes, D10 |
| Configurable playlist columns with title-formatting strings | Format string per column (%artist% etc.) | Metadata fields | (unverified) | no (D7 UI plugin concern, no format-string layer) |
| Converter (transcode selection to files) | Run encoder over selection | File sink | (unverified) | no |
| DSP chain (EQ, resampler, crossfeed) | Ordered plugin list | PCM | (unverified) | no |
| ReplayGain | Tag/scan | Gain data | (unverified) | no |
| Multiple playlists as tabs, search filter | Tabs + filter | Playlist objects | (unverified) | partial, D17 |
| Chiptune decoders (gme, sid, ayfly, mpt) | Plugins | Adapters | (unverified) | yes, D4 |

## 8. foobar2000 chiptune components (unverified)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| foo_gep (Game Emu Player), foo_openmpt, foo_sid | Decoder components | Adapters | (unverified) | yes, D4 |
| Facets | Browser panel splitting library by a field, nested | Indexed metadata fields | (unverified) | partial, D9 browse dimensions (format, artist, genre) fixed, not user-defined |
| ReplayGain scanner | Track/album gain tags | PCM analysis | (unverified) | no |
| DSP chain manager | Ordered DSP, per-output | PCM | (unverified) | no |
| Columns UI, Playlist view with title formatting | Layout and format strings | Fields | (unverified) | no |
| Masstagger | Scripted batch tag edit | Writable tags | (unverified) | no |
| Media library search with query syntax (field:value, AND/OR/NOT) | Query language | Indexed fields | (unverified) | partial, D9 `search(query)` and FTS5 (D9a) prefix search; no field syntax |
| Auto playlists (query-driven) | Live playlist from query | Query + library | (unverified) | no |
| Per-subsong length handling, loop count + fade settings (gme/openmpt options) | Preferences | Settings | (unverified) | no |

## 9. Game_Music_Emu / VGMPlay front ends (unverified)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Track info (game, system, author, copyright, length, intro/loop length) | gme_track_info struct | Metadata | (unverified) | partial, D4 metadata sink; intro/loop length no |
| Per-voice mute | gme_mute_voices | Mask | (unverified) | yes, D4 |
| Fade / loop count | Play N loops then fade; default length when unknown (150 s) | length, loop_length, fade | (unverified) | no |
| Tempo control | gme_set_tempo | Float | (unverified) | no |
| Equalizer / treble, stereo depth | gme_set_equalizer, stereo depth | Params | (unverified) | no |
| VGMPlay chip register view / per-chip volume and mute, loop count, fade, GD3 tags | Shows YM2612/SN76489 etc. channels | Chip state + GD3 | (unverified) | partial, D6 voice taps; no register view |

## 10. zxtune (unverified; GitHub URL 404 in fetch)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| AY/YM and many ZX formats (PT3, STC, PSG, ASC, TFM, etc.) | Native cores | Cores | (unverified) | partial, no cores listed (D4 allows) |
| Archive/container browsing (ZXZip, Hrust, etc., multi-module containers) | Open container, list modules inside | Container parsers | (unverified) | partial, D9 archives; modules-inside-container as browse level not stated |
| Format auto-detect by content | Probe all plugins | Prefix probe | (unverified) | yes, D4 staged probe |
| Chip parameters (AY/YM layout ABC/ACB, clock, interpolation) | Settings | Params | (unverified) | no |
| Playlist w/ properties, zxtune-ay on Amiga? n/a | | | | |

## 11. Chip register panels (all unverified)
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| SID: 3 voices, freq, pulse width, ADSR, waveform bits, filter cutoff/res/mode, volume | Live register readout | Register shadow of SID | (unverified) | no |
| AY / YM2149: 3 tone, noise, mixer, envelope shape/period, volumes | Same | AY registers (14) | (unverified) | no |
| Paula: 4 channels, period, volume, DMA on/off, sample ptr/len | Same | Paula state (Amiga: real; else emulated) | (unverified) | partial, D4 `voice_info` with period and D6 note taps; no register view |
| SN76489: 3 tone + noise, attenuation | Same | Register shadow | (unverified) | no |
| Common requirement | Replayer exposes read-only chip state | New optional hook (e.g. `regs(st, chip, out)`) | n/a | no; add to D4 display hooks |

## 12. Library presentation features across players
| Feature | How it works | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Column browser (iTunes/foobar style: genre > artist > album) | Cascading filter panes | Indexed fields, counts | (unverified) | partial, D9a collection > format > artist > file is a fixed tree |
| Facets | User picks fields, nested filters | Field index | (unverified) | no |
| Cover flow / album art grid | Image grid | Art per item (rare for modules) | (unverified) | no (module art mostly absent) |
| Grid / icon view | Alternate layout | None | (unverified) | n/a UI plugin concern, D7 |
| Smart playlists | Saved query re-evaluated | Query + ratings/play counts | (unverified) | no (ratings exist D9; play counts do not) |
| Query language | Field/operator/boolean syntax | Field schema | (unverified) | partial, FTS5 prefix only |
| Ratings | 1-5 stars | Rating store | n/a | yes, D9 DEViLBOX ratings |

## Gap summary (not in design at all)
DSP chain; crossfade/gapless; fade-out and loop-count policy with length metadata; ReplayGain; interpolation/resampler choice; Songlengths + STIL; SID chip model/filter settings; chip register panel hook; phase view; tag editing; format-string columns; user facets; smart playlists + field query language; skins; converter/WAV export; library folder watching; play counts.
