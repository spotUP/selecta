---
date: 2026-10-22
topic: Feature inventory of terminal, client/server and web players versus the Selecta design
tags: [research, text-ui, protocol, remote, integration, cmus, mpd, musikcube, quodlibet, mpris]
status: final
---

# Feature inventory: terminal, client/server and web players

Design doc judged against: `thoughts/shared/plans/2026-10-22-architecture-design.md` (sections 1a feature bar, 3 engine commands/events, 7 UI plugins, 8 headless and remote, 9 library, 9b view model, 9c playlist abstraction).
Source column: URL fetched in this session, local file:line, or "(unverified)" = from memory, not fetched. Last column: yes / partial / no + section.

Not fetched: cmus man pages (404 on guessed raw URLs; cmus.txt was fetched), ncmpcpp view list (README names no screens), Strawberry smart-playlist details, Subsonic API, UPnP/AirPlay/Chromecast specs. Those rows are marked unverified.

## cmus

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Seven views (1 library tree, 2 sorted library, 3 playlist, 4 queue, 5 browser, 6 filters, 7 settings) | Number keys switch full-screen views | Library DB, playlists, queue, fs listing | https://raw.githubusercontent.com/cmus/cmus/master/Doc/cmus.txt | partial: 9b tree/regular/drilldown styles, 9c queue; no filters-view or settings-view as named screens (7) |
| Tree view artist/album | Artist selection shows all tracks of artist | artist/album tags | https://cmus.github.io/ | yes: 9b tree |
| Live filter (type to filter) | Plain text narrows list as typed | Tag text | https://cmus.github.io/ | yes: 1a typed search filters (9) |
| Filter expression language, short form (`~a beatles (!~y1960-1965 \| ~d>600)`) | `~` letter keys, ranges, `& \| !` | Typed tags | cmus.txt URL above | partial: one query language, syntax not specified (1a, 9) |
| Filter expression long form (`artist="*beatles*"&album="R*"`) | Field op value with globs | Typed tags | cmus.txt URL above | partial (1a, 9) |
| Named filters + push filter | `F` pushes a filter, filters view stores them | Filter text store | cmus.txt URL above | yes: smart playlists = saved queries (1a, 9c) |
| Play queue (`add -q`, `-Q` prepend, `lqueue`, `tqueue`, `stop_after_queue`) | Queue consumed before playlist; stop after queue | Queue list | cmus.txt URL above | yes: 9c queue; stop-after-queue no (3) |
| Playlist ops (`pl-create`, `pl-import`, `pl-export`, `win-mv-after/before`, `pl_sort`) | Editable playlists, reorder, import/export | Playlist store | cmus.txt URL above | yes: 9c, formats 1a |
| Mark / unmark / invert selection | Multi-select then act | Selection in view | cmus.txt URL above | yes: 9b selection |
| Vi-style command mode, tab completion, `bind`/`unbind`/`showbind` | `:` prompt, user key bindings to commands | Command table | https://cmus.github.io/ | partial: user key bindings in every UI (1a); command prompt not specified (7) |
| `:seek +1m`, `vol +10%`, relative values | Relative seek and volume args | Position, volume | cmus.txt URL above | partial: SEEK/VOLUME exist, relative form not stated (3) |
| Status line (`format_statusline`, aaa_mode, continue, follow, repeat, shuffle flags, show_remaining_time) | Configurable format string with `%a %t %l %d %y` | Track tags, mode flags | cmus.txt URL above | partial: text UI exists (7), format string not specified |
| Follow mode (`f`) | Cursor follows playing track | Playing track id | cmus.txt URL above | no (9b gap) |
| Play scope modes (play_library, play_sorted, aaa_mode all/artist/album) | Choose whether next = library or playlist, stop at album end | Mode flags | cmus.txt URL above | no (3 REPEAT only) |
| Next/prev album (`player-next-album`) | Jump album boundary | Album grouping | cmus.txt URL above | no |
| `run`/`shell`, external commands on selection | Pipe selected files to tool (tag editor) | File paths | https://cmus.github.io/ | no |
| `cmus-remote` over UNIX socket | Same command grammar sent to socket; `-C` raw command, `-Q` status | Socket path, optional TCP password | https://github.com/cmus/cmus/blob/master/Doc/cmus-remote.txt | yes: selecta-cli + newline text protocol (8) |
| cmus-remote targets (`-l` library, `-P` playlist, `-q` queue, `-c` clear) | Same add/clear verb, target flag | Target enum | cmus-remote.txt URL above | partial: QUEUE_ADD/CLEAR, PLAYLIST_LOAD (3) |
| cmus-remote toggles (`-u` pause toggle, `-U` pause only if playing, `-R`, `-S`) | Toggle vs idempotent verbs | Mode flags | cmus-remote.txt URL above | partial: PAUSE only (3) |

## mpd (protocol) and ncmpcpp (client)

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Text protocol over TCP/UNIX socket | Line commands, `OK`/`ACK` reply, command lists | Socket | https://mpd.readthedocs.io/en/latest/protocol.html | yes: newline text protocol from commands.def (3, 8) |
| Transport: `play playid pause stop next previous seek seekid seekcur` | Position vs stable id variants | Queue pos or song id | protocol URL above | partial: no stable song ids stated (3) |
| Status: `status currentsong stats clearerror` | Pull snapshot | State struct | protocol URL above | yes: `sel_engine_state()` (3) |
| Options: `consume crossfade mixrampdb mixrampdelay random repeat single setvol volume getvol replay_gain_mode replay_gain_status` | Per-mode toggles | Option state | protocol URL above | partial: crossfade, repeat, volume, loudness (1a, 6a); consume, single no |
| Queue edit: `add addid clear delete deleteid move moveid swap swapid shuffle` | By position, range or id | Queue | protocol URL above | partial: QUEUE_ADD/REMOVE/CLEAR/PLAY_NEXT (3); move, swap, shuffle no |
| Queue view: `playlistinfo playlistid playlistfind playlistsearch plchanges plchangesposid` | Full list, search, and diff since version | Queue version counter | protocol URL above | partial: QUEUE_CHANGED event (3); version diff no |
| `prio prioid`, `rangeid` | Random-mode priority; play only a time range of a song | Per-entry priority, range | protocol URL above | no (cue points 1a are close) |
| `addtagid cleartagid` | Tags on remote queue entries | Tag map | protocol URL above | no |
| Stored playlists: `listplaylists listplaylist listplaylistinfo load save playlistadd playlistclear playlistdelete playlistmove playlistlength rename rm searchplaylist` | Server-side named playlists | Playlist files | protocol URL above | yes: PLAYLIST_LOAD/SAVE (3), 9c |
| Search: `find findadd search searchadd searchaddpl count searchcount list` | Case sensitive/insensitive, add results to queue/playlist, grouped unique values | Library index | protocol URL above | partial: typed filters (1a, 9); findadd and grouped `list` not stated |
| Filter expression `((artist == "x") AND (date >= "2000"))` with `contains starts_with =~ !~ eq_cs eq_ci`, `sort`, `window START:END` | Parenthesised expression language and range windows | Tags | protocol URL above | partial (1a, 9, 9b visible window) |
| Browse: `lsinfo listall listallinfo listfiles` | Directory tree by path | Library paths | protocol URL above | yes: 9b tree |
| Art: `readcomments readpicture albumart` | Binary chunked cover fetch | Artwork bytes | protocol URL above | partial: artwork panel (7), not in protocol |
| `getfingerprint` | Chromaprint of file | Audio | protocol URL above | no (duplicate detect uses md5, 1a) |
| `update rescan` | Library scan job | Roots | protocol URL above | yes: LIBRARY_* events (3) |
| `idle`/`noidle` with subsystems: database update stored_playlist playlist player mixer output options partition sticker subscription message neighbor mount | Client blocks until a named subsystem changes, then re-reads | Subsystem bitmask | protocol URL above | partial: events.def pushes typed events (3); idle-style coarse subsystem wait not stated |
| Outputs: `outputs enableoutput disableoutput toggleoutput outputset` | List sinks, per-sink on/off, plugin attributes | Output list | protocol URL above | partial: OUTPUT command and OUTPUT_CHANGED (3, 5); multi-output enable no |
| Partitions: `listpartitions partition newpartition delpartition moveoutput` | Independent queues/players on one server | Partition table | protocol URL above | no (device hand-off 1a is different) |
| Mounts/neighbors: `mount unmount listmounts listneighbors` | Attach remote storage | Mount table | protocol URL above | no |
| Stickers: `sticker get set delete list inc dec find`, `stickernames` | Key/value on song, playlist; numeric inc | Sticker store | protocol URL above | partial: ratings, notes, play counts (1a, 9); generic key/value no |
| Client messaging: `subscribe unsubscribe channels readmessages sendmessage` | Pub/sub between clients | Channel names | protocol URL above | no |
| Reflection: `commands notcommands config tagtypes protocol urlhandlers decoders` | Client discovers capabilities | Command table | protocol URL above | partial: reference docs generated from commands.def (3); runtime listing no |
| Connection: `password ping close binarylimit` | Auth, keepalive | Password | protocol URL above | partial: auth not specified (8) |
| ncmpcpp: visualizer (wave and frequency spectrum, fftw optional) | Reads MPD fifo or output | PCM tap | https://github.com/ncmpcpp/ncmpcpp | yes: 6 visualizers, scopes 1a |
| ncmpcpp: tag editor (taglib) | Edit tags of files | Tag write path | ncmpcpp URL above | partial: local notes + tag edits (1a) |
| ncmpcpp: media library, playlist editor, browser, search engine | Separate screens over MPD list/search/lsinfo | MPD commands | ncmpcpp URL above | yes: 9b, 9c |
| ncmpcpp: lyrics and last.fm fetch via curl | Fetch from web | Artist, title | ncmpcpp URL above | no |
| ncmpcpp screen list (help, clock, outputs, lyrics, selected-items adder, music-dir browser) | (unverified) | | (unverified) | partial |

## musikcube

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Terminal UI + server + library in one binary | Same process plays, indexes, serves | Library | https://github.com/clangen/musikcube | yes: engine + UI plugins + remote (7, 8) |
| WebSocket metadata API (port 7905) + HTTP audio stream (7906), optional transcode | Control and queries over WS, audio pulled over HTTP | Password challenge | musikcube repo URL above | partial: remote layer (8); streaming audio to client not stated |
| Password auth, no TLS (stated warning) | `authenticate` request first | Plain password | musikcube repo URL above | partial (8) |
| Android remote app (musikdroid) | Remote UI streams from server | WS + HTTP | musikcube repo URL above | partial: web remote (1a, 8) |
| Message types: `get_playback_overview pause_or_resume stop previous next toggle_shuffle toggle_repeat toggle_mute set_volume seek_to seek_relative play_at_index` | Request/response JSON with `type`, `name`, `id` | JSON | https://github.com/clangen/musikcube/wiki/remote-api-documentation | yes: HTTP/JSON API (1a, 8) |
| `play_all_tracks play_tracks play_tracks_by_category play_snapshot_tracks` | Start playback from a query, category or snapshot | Query / ids | remote-api URL above | partial: no play-by-query command (3) |
| `query_tracks query_tracks_by_category list_categories query_category query_albums query_tracks_by_external_ids` | Paged library queries | Category names | remote-api URL above | partial (9) |
| `query_play_queue_tracks snapshot_play_queue invalidate_play_queue_snapshot` | Immutable queue snapshots so paging is stable | Snapshot id | remote-api URL above | no (9b visible window is close) |
| Broadcasts `play_queue_changed`, `playback_overview_changed` | Server pushes changes, client never pushes | Event names | remote-api URL above | yes: events (3) |
| Playlists: `save_playlist rename_playlist delete_playlist append_to_playlist remove_tracks_from_playlist` | Server-side playlists | Playlist id | remote-api URL above | yes (3, 9c) |
| `run_indexer` | Trigger library scan remotely | none | remote-api URL above | yes (3, 9) |
| Output drivers `list_output_drivers set_default_output_driver`, gain settings, `get/set_equalizer_settings`, `get/set_transport_type` | Remote device/DSP config | Driver names, EQ bands | remote-api URL above | partial: OUTPUT, DSP plugins (5, 1a); remote EQ no |
| Format support via libopenmpt, libgme (tracker, game music) | Plugin decoders | | musikcube repo URL above | yes: replayer plugins (4) |

## termusic, moc

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Server/client split (`termusic-server`, TUI) over protobuf | UI is a client of the player daemon | Protobuf schema | https://github.com/tramhao/termusic | yes: engine headless, UI plugin (2, 8) |
| Backends rusty/MPV/GStreamer | Swappable decoder+output | Backend choice | termusic URL above | yes: replayer + output plugins (4, 5) |
| Cover art in Kitty/iTerm2, sixel/ueberzug optional | Terminal graphics protocols | Terminal capability reply | termusic URL above | yes: sixel/kitty (7) |
| Tag editor, podcast RSS, YouTube/NetEase/Migu/KuGou download | Screens + downloaders | Tags, feeds | termusic URL above | partial: tag edit (1a); podcasts and downloads no |
| MPRIS on Linux | D-Bus media control | Metadata | termusic URL above | yes: 1a Integration |
| Themes folder + config editor, `tui.toml`/`server.toml` | Runtime theme selection | Theme files | termusic URL above | yes: D8 tokens (7) |
| Speed control (soundtouch) | Time-stretch | PCM | termusic URL above | partial: PITCH/TEMPO (3) |
| moc: two-panel UI (file browser + playlist), server/client (`mocp`), equalizer, themes | `mocp -p/-s/-f/...` controls running server | Socket | https://moc.daper.net/ | yes: selecta-cli (8) |
| moc: OnSongChange hook with parameters | Server runs external command on change | Track tags | https://moc.daper.net/ | partial: Lua/ARexx scripting (1a); no event-hook command config |
| moc key set and `mocp` flag list | (unverified) | | (unverified) | n/a |

## Strawberry / Clementine, Audacious, Quod Libet

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Library with tag editing and AcoustID/MusicBrainz tag fetch | Fingerprint then lookup | Chromaprint, MBID | https://github.com/strawberrymusicplayer/strawberry | partial: notes and tag edits (1a); fingerprint no |
| Smart and dynamic playlists (listed, no detail on page) | Smart = saved rule set; dynamic = auto-refilling queue by rule (Clementine behaviour, unverified) | Rule tree | strawberry URL above; behaviour (unverified) | yes: smart playlists, radio (1a, 9c) |
| Transcoding to MP3/AAC/FLAC/Vorbis/Opus/Speex/WavPack/ASF | Convert for export or device | Encoder set | strawberry URL above | partial: render to WAV/FLAC (1a); lossy no |
| Device sync (USB, MTP, iPod) | Copy+convert to device | Device profile | strawberry URL above | no |
| Subsonic streaming, Tidal/Qobuz (unofficial) | Remote library as source | Server URL, creds | strawberry URL above | partial: source model (9); Subsonic source no |
| Scrobbling to Last.fm, ListenBrainz, Subsonic | Submit plays | Credentials | strawberry URL above | yes: 1a scrobbling |
| Audacious: Winamp Classic skins or Qt/GTK interface | Skin loader and native toolkit UIs | Skin zip | https://audacious-media-player.org/ | partial: bitmap skin loader later (D8, 7) |
| Audacious: equalizer, effects, lyrics, VU, file browser, playback history, Now Playing (macOS) plugins | Plugin per feature | | audacious URL above | partial: DSP plugin kind (1a, 5), history in 1a |
| Audacious plugin types: input, output, effect, general, visualization, container, transport (7 types) | (unverified) | | (unverified) | yes: mapping to plugin kinds (4, 5, 6, 10) |
| Quod Libet free text | Matches visible columns | Text | https://quodlibet.readthedocs.io/en/latest/guide/searching.html | yes: 1a |
| QL boolean: `&(a, b)`, `\|(a, b)`, `!a`, `!\|(a, b)` | Prefix operators with arg lists | Parsed tree | QL URL above | partial (query syntax unspecified, 1a) |
| QL tag search: `artist = delerium`, `!=`, multi-tag `artist, performer = "Boa"`, internal `~format`, `~dirname`, `~filename` | Tag field op value | Tags + `~` internals | QL URL above | partial |
| QL exact `"..."`, `"..."c` case-sensitive; regex `/.../`, `c`, `d` (ignore accents) | Quoting and regex flags | Regex engine | QL URL above | no |
| QL numeric `#(skipcount > 100)`, `#(10 <= track < 100)`, `#(added < 1 day)`, `#(2:00 < length < 3:00)`; units s,min,h,day,month(30),year(365), kB MB GB | Chained comparisons, relative time | Numeric tags, play stats | QL URL above | partial: rating, year (1a) |
| QL `~playlists=chilled`, `&(#(rating>=0.75), ~playlists="")` | Query on playlist membership | Membership index | QL URL above | no |
| QL `@(saved: Unrated)`, `@(python: ...)` | Nested saved queries, plugin queries | Saved query store | QL URL above | partial: saved queries (9c); nesting no |

## Web players

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| DEViLBOX main views: tracker, grid, pianoroll, matrix, sunvox, dj, drumpad, vj | `activeView` switch | UI store | /Users/spot/Code/DEViLBOX/src/stores/useUIStore.ts:35 | n/a (editor, not player) |
| DEViLBOX DJ decks: transport, cue points, loops, nudge, pitch slider, quantize, beat grid, scopes, channel toggles, vinyl/turntable views, stems | Two decks over tracker engine | Beat data | /Users/spot/Code/DEViLBOX/src/components/dj/ (file list) | partial: cue points, channel mute, tempo (1a); decks/crossfader no |
| DEViLBOX Auto DJ (transition bars, shuffle, filter, skip) | Automix between playlist tracks | Playlist, bar length | /Users/spot/Code/DEViLBOX/src/components/dj/DJAutoDJPanel.tsx:4 | partial: crossfade + gapless (1a); bar-synced automix no |
| DEViLBOX playlist store, crates, Modland browser, file browser, cache panel | Playlists with seed entries, crate panel, Modland tree | Playlist store | /Users/spot/Code/DEViLBOX/src/stores/useDJPlaylistStore.ts; src/components/dj/DJModlandBrowser.tsx | yes: 9, 9c, 1a |
| DEViLBOX jukebox panel | Play list panel | | /Users/spot/Code/DEViLBOX/src/components/jukebox/JukeboxPanel.tsx | yes: radio/queue (1a) |
| DEViLBOX remote control button, MCP bridge | Remote control and MCP handlers (djHandlers.ts) | Bridge messages | /Users/spot/Code/DEViLBOX/src/components/dj/DJRemoteControlButton.tsx; src/bridge/MCPBridge.ts | yes: MCP server (1a) |
| DEViLBOX global keys: Ctrl+Z/Y undo, Ctrl+S save, Esc, `?` help, Ctrl+K, Ctrl+Shift+E/J/P/A/D/T/V/X/F/M panels | Window keydown router | Key map | /Users/spot/Code/DEViLBOX/src/App.tsx:494-619 | partial: user key bindings (1a) |
| DEViLBOX DJ keys: Deck A left hand (q play, w, e slip cue, r set cue, a/d nudge, s, z/x/c loop, 1-4 hot cue), Deck B right hand; beat jumps z x c v / m , . /; Tab repitch lock | Per-view keyboard router | Deck state | /Users/spot/Code/DEViLBOX/src/components/dj/DJKeyboardHandler.tsx:70-196 | no (decks not in design) |
| bbmp tabs: 1 Browse, 2 Search, 3 Queue, 4 Playlists, 5 Files, 6 History, 7 Info, 8 Artist, 9 Stats, 0 Settings | TabbedContent | Index, library | /private/tmp/claude-501/-Users-spot-Code-MusicPlayer/1d27ca56-a355-4a5d-b8ae-938ae20e489e/scratchpad/ext/bbmp/src/bbmp/tui/app.py:156-159,215-250 | partial: Info, History, Stats screens (1a home, history); Stats no |
| bbmp keys: space pause, x stop, n/p, `[` `]` subsong, `,` `.` seek 10s, +/- volume 5, r radio, Ctrl+R radio all, `/` search, b "by", s keep, f favourite, ! sounds wrong, R repeat, S shuffle, o on-and-on, c collection, m artist, A album, g go to playing, i info, w web, d demozoo, u update catalog, a enqueue, N play next, l add to playlist, L save queue, e export, Del remove, F2 rename, J/K move, v visuals, ? help, q quit | Textual Binding table | Key ids | bbmp app.py:114-154 | yes: parity target (1a, 7) |
| bbmp "sounds wrong" list (`!`) | Flags a file as misplayed | Wrong list file | bbmp app.py:129 | no |
| bbmp "On and on" forever mode, Activity modal, Stats periods (week) | Endless playback; listening stats | History | bbmp app.py:136,155,187 | partial: loop/auto-stop (1a); stats no |
| bbmp lookup web / demozoo | Open external page for song | URL templates | bbmp app.py:142-143 | no |
| bbmp MPRIS option (`mpris=True`), export dir | Optional D-Bus; export playlist/song | | bbmp app.py:166-171 | yes: 1a Integration |
| Nectarine / scenestream: streams page `/demovibes/streams/`, "closest stream" advice | Multiple geo streams, URLs on a streams page (not fetched) | Stream list | http://scenestream.net/ | partial: radio source (1a) |
| Nectarine: song requests, voting, favourites for registered users, "requested by" shown | Site features, no public API found on page | Account | http://scenestream.net/ | partial: party mode voting (1a); no request-to-station |
| Nectarine now-playing JSON | (unverified) | | (unverified) | n/a |
| SceneSat: 24/7 audio, video from live events, archive, chat, listener count, show schedule (Europe/Stockholm) | Web site; stream URLs not found on fetched page | Stream URL | https://scenesat.com/ | partial: stream decoders (1a) |

## Integration

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| MPRIS `org.mpris.MediaPlayer2.Player` methods Next Previous Pause PlayPause Stop Play Seek(us) SetPosition(TrackId, pos) OpenUri | D-Bus methods | Position in microseconds, track id | http://specifications.freedesktop.org/mpris/latest/Player_Interface.html | yes: 1a Integration (plugin per OS, M5) |
| MPRIS Player properties PlaybackStatus LoopStatus Rate Shuffle Metadata Volume Position MinimumRate MaximumRate CanGoNext CanGoPrevious CanPlay CanPause CanSeek CanControl; signal Seeked | Read/write props, PropertiesChanged | Metadata `a{sv}` (xesam:*, mpris:trackid, mpris:length, mpris:artUrl) | Player_Interface URL above | partial: needs STATE/POSITION/SONG_OPENED mapping (3) |
| MPRIS `org.mpris.MediaPlayer2` root (Quit, Raise, CanQuit, CanRaise, HasTrackList, Identity, DesktopEntry, SupportedUriSchemes, SupportedMimeTypes) | (unverified) | | (unverified) | partial |
| MPRIS `TrackList` (GetTracksMetadata, AddTrack, RemoveTrack, GoTo; TrackAdded, TrackRemoved, TrackListReplaced) and `Playlists` (ActivatePlaylist, GetPlaylists; PlaylistChanged) | Optional queue and playlist exposure | Queue ids | name confirmed on Player page; members (unverified) | partial: QUEUE_CHANGED (3) |
| macOS `MPNowPlayingInfoCenter.default().nowPlayingInfo` keys: Title, Artist, AlbumTitle, Artwork, PlaybackDuration, ElapsedPlaybackTime, PlaybackRate, DefaultPlaybackRate, IsLiveStream, MediaType; `playbackState` | App publishes dictionary; update on state change | Tags, position, rate | https://developer.apple.com/documentation/mediaplayer/mpnowplayinginfocenter (page returned only topic list; keys (unverified) from memory) | yes: 1a |
| macOS `MPRemoteCommandCenter` commands play pause togglePlayPause nextTrack previousTrack changePlaybackPosition skipForward skipBackward | Media keys and headphones route here | Handlers | same Apple URL, (unverified) | yes: media keys, 1a |
| Windows SMTC: properties PlaybackStatus, AutoRepeatMode, ShuffleEnabled, PlaybackRate, IsPlay/Pause/Stop/Next/Previous/FastForward/Rewind/Record/ChannelUp/ChannelDown Enabled, DisplayUpdater; method UpdateTimelineProperties; events ButtonPressed, PlaybackPositionChangeRequested, PlaybackRateChangeRequested, AutoRepeatModeChangeRequested, ShuffleEnabledChangeRequested | Enable buttons, push display + timeline, handle events | Metadata, timeline | https://learn.microsoft.com/en-us/uwp/api/windows.media.systemmediatransportcontrols | yes: 1a |
| Media keys (play/pause, next, prev, stop) | OS global hotkey or MPRIS/Now Playing/SMTC routing | Key events | covered by the three rows above | yes: global hotkeys (1a) |
| Last.fm `track.updateNowPlaying` and `track.scrobble`, POST `http://ws.audioscrobbler.com/2.0/`, signed; scrobble when length >30 s and played >= half or 4 min; batch up to 50, send cached first | Now-playing at start, scrobble at end | artist, track, timestamp, optional album, duration | https://www.last.fm/api/scrobbling | yes: 1a scrobbling (desktop); offline queue + 50 batch rule not stated |
| ListenBrainz `POST /1/submit-listens`, header `Authorization: Token <t>`; listen when half or 4 min; max 1000 listens/request, 10240 bytes/listen; `playing_now` type; UTC epoch `listened_at`; `duration`/`duration_ms` | JSON payload; listen_type values single, playing_now, import (values (unverified), page defers to JSON docs) | MBIDs optional, tags | https://listenbrainz.readthedocs.io/en/latest/users/api/core.html | yes: 1a |
| Chromecast output | Cast protocol receiver with HTTP media URL; engine must serve a stream | HTTP stream endpoint, device discovery (mDNS) | (unverified) | partial: network outputs 1a, 5 |
| AirPlay output | RAOP/AirPlay 2 audio push; discovery via Bonjour | PCM or ALAC stream | (unverified) | partial: 1a, 5 |
| UPnP/DLNA output (renderer) | SetAVTransportURI against HTTP URL, SSDP discovery | HTTP stream endpoint | (unverified) | partial: 1a, 5 |

## Gaps (features in no part of the design)

- Stable song ids in the queue (mpd `playid`, `deleteid`, `moveid`) and queue version diff (`plchanges`).
- Queue move / swap / shuffle commands; consume mode, single mode; stop-after-queue; follow mode; play scope (library vs playlist vs album-only); next/prev album.
- Query language syntax itself (only "one query language" is stated): regex, case flags, numeric ranges and relative time, `~playlists` membership, nested saved queries (Quod Libet, mpd, cmus all have these).
- Stable queue snapshots for paging over a remote link (musikcube).
- Sticker-style generic key/value store with increment (mpd).
- Multiple partitions / independent players; per-output enable/disable (mpd outputs).
- Client-to-client messaging channels (mpd).
- Fingerprint lookup, lossy transcoding, device sync (Strawberry).
- Stats screen, "sounds wrong" flag, external lookup keys (bbmp).
- Hook on song change running a command (moc OnSongChange) outside Lua/ARexx.
- Podcasts, lyrics (ncmpcpp, termusic, Audacious).
- DJ decks and bar-synced automix (DEViLBOX DJ view).
- Offline scrobble queue with 50-per-batch flush.

## Protocol ideas (for commands.def / events.def)

1. Subsystem `IDLE` command: client sends `IDLE player queue library output options`, engine replies with the changed subsystem names, then client pulls state (mpd `idle`); cheap for Amiga and for polling clients, complements pushed events.
2. Stable entry ids on queue and playlist rows plus `QUEUE_VERSION` in state and a `QUEUE_CHANGES since=N` command (mpd `plchanges`).
3. `QUEUE_MOVE`, `QUEUE_SWAP`, `QUEUE_SHUFFLE range`, `QUEUE_ADD_AT pos`, and ranges `START:END` with open end (mpd).
4. Option commands as one family: `OPTION consume|single|random|repeat|crossfade on|off|oneshot` (mpd `single oneshot`).
5. `PLAY_QUERY query` and `QUEUE_ADD_QUERY query` (mpd `findadd`, musikcube `play_tracks_by_category`) so a remote never has to pull a result list to play it.
6. `COMMANDS` / `TAGTYPES` reflection command generated from commands.def, so clients (web remote, MCP) self-discover (mpd `commands`).
7. `SNAPSHOT_QUEUE` returning a snapshot id for stable paging (musikcube).
8. Sticker verbs `STICKER_SET/GET/INC/FIND` for ratings, play counts, notes (mpd).
9. Command lists with all-or-nothing semantics (`command_list_begin`) for batch edits; useful for Lua/ARexx mapping.
10. Position-relative arguments in SEEK/VOLUME (`+10`, `-10%`) as in cmus `:seek +1m`, `vol +10%`; idempotent `PAUSE_PLAYBACK` vs toggle `PAUSE` (cmus `-U` vs `-u`).
11. Events for scrobble/MPRIS: `SEEKED` (MPRIS signal), `SCROBBLE_DUE` once threshold (>30 s and half or 4 min) is reached, so integration plugins do not reimplement the rule.
12. Auth: `AUTH password` first command; musikcube warns plaintext without TLS, so state in section 8 that remote is LAN-only unless wrapped.
13. Binary chunked artwork fetch `ALBUMART uri offset` (mpd) for web remote and text UI sixel.
