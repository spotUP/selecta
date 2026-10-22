---
date: 2026-10-22
topic: Feature inventory of modern streaming and library players versus the Selecta design
tags: [research, features, spotify, apple-music, plexamp, subsonic, library-presentation]
status: final
---

# Modern streaming and library players: feature inventory

Design doc compared against: `thoughts/shared/plans/2026-10-22-architecture-design.md` (sections 1a feature bar, 3 engine commands, 6 visualizers, 6a loudness, 8 headless/remote, 9 library, 9b view model, 9c playlist abstraction).

Source marking: a URL means the page was fetched (WebFetch) or came back as a result of a WebSearch whose snippet supports the row (marked "search"). "(unverified)" means from memory only. Several vendor help pages returned 403/404/"not found" (Plex support, Spotify crossfade/volume articles, Roon help, Apple column browser), so those rows are unverified or backed by search results only.

## Spotify

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Home / Jump back in | Home shelves of recent contexts and made-for-you mixes, built from listening history | Play history with context (playlist/album/query), timestamps | (unverified) | yes, 1a "Home view: recently played, jump back in", 9 |
| Liked Songs | One flat per-user list of hearted tracks, treated as a playlist | Per-song liked flag + timestamp | (unverified) | yes, 1a ratings/liked list, 9.2 `liked:` filter |
| Queue: Play next / Add to queue | Play next inserts after current; Add to queue appends; queue is separate from the context | Queue list with a cursor, source context | https://support.spotify.com/us/article/spotify-connect/ (device context only); queue behaviour (unverified); queue multi-select + reshuffle: https://engadget.com/2183106/spotify-adds-playlist-folders-bulk-editing-and-background-downloads/ (search) | yes, 9c queue ("play-next inserts at cursor"), 3 QUEUE_ADD |
| Playlist folders | Playlists grouped in nestable folders, organisation only | Folder tree table, playlist ids | https://engadget.com/2183106/spotify-adds-playlist-folders-bulk-editing-and-background-downloads/ (search: nest, Free and Premium, mobile) | yes, 9c folder tree, 1a |
| Smart Shuffle | Mixes recommended tracks into a shuffled playlist, marked as additions | Recommendation source, seed = playlist | https://support.spotify.com/us/article/smart-shuffle/ | partial, 1a radio with DEViLBOX steering (9.2); no "inject into playlist" mode |
| Reshuffle | Button re-randomises the shuffled queue | Queue + RNG seed | https://engadget.com/2183106/spotify-adds-playlist-folders-bulk-editing-and-background-downloads/ (search) | partial, 1a random mode; no explicit reshuffle command in 3 |
| Radio / Autoplay | After the context ends, similar tracks are appended; "start radio from song" | Seed song, similarity source | (unverified) | yes, 1a radio, 9c "radio mode appends from seed query" |
| Connect device hand-off | Any app controls playback on another device; audio streams from the cloud to that device | Device registry, account auth, same-network discovery | https://support.spotify.com/us/article/spotify-connect/ | yes, 1a Device hand-off (M5), 8 |
| Crossfade | Fades end of one track into the next, 0-12 s setting | Two decoder instances, track end time | (unverified; support page not found) | yes, 1a crossfade + gapless (M1) |
| Loudness normalisation | Per-track gain to a target loudness (Volume level setting) | Per-track integrated loudness | (unverified; support page not found) | yes, 6a, 1a (M3) |
| Collaborative playlists | Several users add/remove tracks in one playlist | Shared playlist id, per-member edit rights, sync | (unverified; Blend page only links to it) | partial, 1a "Collaborative lists" (hippoplayer.se publish, DEViLBOX votes) |
| Blend | Shared playlist auto-built from up to 10 friends' listening, refreshed daily | Per-user listening history, invite link | https://support.spotify.com/us/article/blend/ | no (single-user history only; closest is party voting 1a M5) |
| Canvas | Short looping video behind the now-playing track | Per-track video asset | (unverified) | partial, 1a GPU visualizers + generated artwork per format/chip (6) |
| Lyrics | Time-synced lyrics scroll with playback | Synced lyric text per track | (unverified; Spotify page not fetched) | no (tracker "module message" in 1a Tracker info panel is static text only) |
| Share links | URL for track/playlist/profile, opens app or web | Stable ids, resolver | (unverified) | yes, 1a `selecta://` + devilbox URLs (song, playlist, position) |
| Offline | Downloaded tracks play without network; iOS now downloads in background | Local cache with licence/expiry, download manager | https://engadget.com/2183106/spotify-adds-playlist-folders-bulk-editing-and-background-downloads/ (search) | partial, 9.3 cache + "kept" songs + prefetch; no explicit "download this playlist" command |

## Apple Music / iTunes

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Library | Local files + cloud library in one database, imported from folders | Tags, file hashes, play counts | https://support.apple.com/guide/music/welcome/mac | yes, 9.2 SQLite catalogue |
| Column browser | Genre / Artist / Album panes above the song list filter it, each pane narrows the next | Distinct values per tag, counts | (unverified; Apple page not found); description of Swinsian copy: https://soundtrackcentral.com/topics/8627/recommended-music-player-app-for-mac-for-local-music-collection (search) | yes, 9b drilldown (Miller columns, iTunes column browser) |
| Cover Flow (heritage) | 3D carousel of album covers, flip through to pick | Cover art per album at several sizes | (unverified; removed from iTunes 11) | yes, 9b cover flow, 1a view styles (RTG tier) |
| Lossless / Hi-Res Lossless | ALAC streams up to 24-bit/48 kHz and 24-bit/192 kHz; per-song quality picker | Lossless catalogue, output device capability | https://support.apple.com/guide/music/play-lossless-audio-mus90b573cbb/mac | yes (native replayers render at source quality, 1a resampler choice; no lossy stage) |
| Playlist folders | Folders nest playlists and smart playlists; no individual songs inside; deleting a folder deletes contents | Folder tree | https://support.apple.com/guide/music/organize-playlists-in-folders-musb6032bb56/mac | yes, 9c |
| Playing Next (Up Next) | Queue of upcoming items; Play Next / Add to Queue; drag reorder; History link | Queue list, history | https://support.apple.com/guide/music/musb1e6d1c76/mac (search result, not fetched) | yes, 9c queue + history in 1a |
| Autoplay | For subscribers, appends similar songs after the queue; setting syncs across devices | Seed, similarity, synced setting | https://support.apple.com/guide/music/musb1e6d1c76/mac (search) | yes, 1a radio |
| Smart playlists | Field/operator/value rules, match all or any, nesting (Option+Nest), size limit, "live updating" | Tag/stat fields, rule evaluator | https://support.apple.com/guide/music/create-edit-and-delete-smart-playlists-mus1712973f4/mac | yes, 1a smart playlists = saved queries, 9.2 one query language |
| Limit on smart playlists | Cap by count, duration, size and pick by "most recently added" etc. | Sort key + limit | https://support.apple.com/guide/music/create-edit-and-delete-smart-playlists-mus1712973f4/mac | partial, 9c "library view = saved query (filter + group + sort)"; limit clause not stated |
| Star ratings | 1-5 stars per song, optional album rating averaged from songs; used in smart playlists | Per-song rating | https://support.apple.com/guide/music/rate-songs-and-other-items-musb8c86898e/mac | yes, 1a ratings + `rating:` filter |
| Match only checked items | Per-song checkbox excludes a song from playback and smart lists | Per-song enabled flag | https://support.apple.com/guide/music/create-edit-and-delete-smart-playlists-mus1712973f4/mac | partial, files table has `hidden` (9.2); not a playback-skip flag |
| Multiple libraries | Switch between separate library files | Library path setting | https://support.apple.com/guide/music/welcome/mac | no (one DB per install; navidrome-style multi-library absent) |

## Tidal

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Hi-res FLAC streaming | Streams lossless/hi-res FLAC up to 192 kHz; MQA being dropped | Lossless catalogue | https://www.whathifi.com/tidal/specs (search) | no (not a streaming service; local replayers only) |
| Tidal Connect | App is only a controller; audio streams from servers straight to the device | Device discovery, account | https://www.whathifi.com/features/tidal-connect-everything-you-need-to-know (search) | yes, 1a Device hand-off (M5) |
| Dolby Atmos / immersive | Multichannel mixes decoded on supported devices | Atmos masters, capable output | https://www.whathifi.com/tidalmasters/review (search) | no |
| Track / artist radio | Endless station seeded from a song or artist | Seed, similarity | https://aphnetworks.com/news/22974-tidal-launches-tidal-connect-first-hifi-casting-technology (search, artist radio only) | yes, 1a radio |
| Mixes, My Mix, editorial playlists | Curated and auto-generated lists | Editorial + usage data | (unverified) | partial, hippoplayer.se published lists (9.1 `published_lists`), no generated mixes |
| Offline | Download tracks/playlists in selected quality | Quality tier, local storage | (unverified) | partial, 9.3 |

## YouTube Music

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Smart Downloads | Auto-downloads music from listening history, up to ~500 songs on Wi-Fi; must reconnect every 30 days | History, storage budget | https://support.google.com/youtubemusic/answer/6313535?hl=en | partial, 9.3 prefetch + LRU budget; no history-driven auto-download |
| Song / Video switch | Toggle between audio track and music video at the same position | Linked video asset per song | https://www.digitalmusicnews.com/?p=265798 (search) | no |
| Song radio / mixes | Radio from any song or playlist, personalised "Supermix" etc. | Seed + history | (unverified) | yes (radio) / no (Supermix) |
| Library of uploads | User uploads own files to cloud library | Upload, matching | (unverified) | no (local + source catalogues instead) |
| Queue, Up Next, Autoplay | Add to queue / play next, autoplay beyond queue | Queue | (unverified) | yes, 9c |

## Roon

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Core / Remote / Output split | Core does library, DSP, streaming; Remotes control; Outputs play via RAAT | Network protocol, Core state | https://community.roonlabs.com/t/hardware-architecture-for-roon-system/5423 (search) | yes, 2 layering, 8 (engine + protocol + UI plugins) |
| Zones and zone grouping | Each output is a Zone; zones group for synced playback | Output registry, clock sync | https://community.roonlabs.com/t/understanding-raat-and-grouping/65415 (search) | partial, 1a network outputs + Connect hand-off; no multi-zone grouping |
| Remote control from any device | Phone/tablet apps drive Core, optional private zone on the remote | Protocol, auth | https://whathifi.com/advice/roon-everything-you-need-to-know (search) | yes, 1a web remote, 8 protocol |
| Library-centric metadata | Rich metadata (credits, composers, reviews, genres) merged over files, linked entities navigable | Metadata service, entity ids | (unverified) | partial, 1a "Composer pages", "Similar and also-by", HVSC/STIL panel |
| Focus filters | Filter library by genre/format/etc. to narrow browsing | Tag facets | (unverified) | yes, 9b facets / drilldown |
| Signal path display | Shows every stage from file to DAC | Pipeline description from engine | (unverified) | partial, 5 output/DSP chain exists; no UI readout stated |
| DSP engine (EQ, upsampling, convolution) | Core-side processing before output | DSP chain config | (unverified) | partial, 1a DSP chain plugins (EQ, reverb, limiter); no convolution |

## Plexamp

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Sonic analysis | Server neural net extracts ~50 audio parameters per track; similarity = nearness in N-dim space; CPU heavy, takes hours to days | Raw audio, analysis store (vector per track) | https://support.plex.tv/articles/sonic-analysis-music/ (search); https://techcrunch.com/2021/08/12/plexs-new-feature-matches-your-sonically-similar-music-to-make-playlists (search) | partial, 1a BPM detect, 6a loudness analysis (render-time); no similarity vector |
| Sonically similar / Track Radio | Shows nearest tracks/albums/artists; radio built from that | Sonic vectors | https://support.plex.tv/articles/sonic-analysis-music/ (search) | partial, 1a "Similar and also-by" from DEViLBOX genre + ratings, not audio features |
| Mixes / Sonic Adventure | Path of sonically-near tracks between two chosen songs (also `findSonicPath` in OpenSubsonic) | Sonic vectors | https://opensubsonic.netlify.app/docs/endpoints/ | no |
| Super Shuffle | Not confirmed by any source found | n/a | search found no mention | n/a (unconfirmed) |
| Sweet Fades | Smart fades between tracks; listed in free tier with gapless, pre-caching, loudness leveling | Track end/start analysis | https://plex.tv/plexamp/ (search) | partial, 1a crossfade; no "smart" fade by analysis, but loop-detect/auto-stop fade exists |
| Loudness leveling | Per-track level matching | Loudness per track | https://plex.tv/plexamp/ (search) | yes, 6a |
| Gapless + advanced pre-caching | Preload next track | Next-track prediction, cache | https://plex.tv/plexamp/ (search) | yes, 1a gapless, 9.3 prefetch next song |
| Guest DJ | Selectable DJ modes (Stretch, Contempo, Groupie) choose the next track | Sonic server, listening data | https://plex.tv/plexamp/ (search) | partial, 1a radio with steering; no named DJ modes |
| Sonic Sage | Builds playlists from a plain-language prompt via an OpenAI key | LLM key, library metadata | https://plex.tv/plexamp/ (search) | partial, 8 MCP server + HTTP API exists so an LLM client could do it; not a built-in feature |

## Jellyfin / Finamp

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Tag-driven library | Album per folder; embedded tags and cover win; disc tags group multi-disc | Embedded tags, folder layout | https://jellyfin.org/docs/general/server/media/music/ | yes, 9.2 tags from replayers + path parsing |
| Synced lyrics | `.lrc/.elrc/.txt` next to the file; click a line to seek | Lyric file per track | https://jellyfin.org/docs/general/server/media/music/ | no |
| Offline downloads (Finamp) | Download (optionally transcoded) songs for offline | Local store, transcode | https://www.apkmirror.com/apk/jmshrv/finamp-github-version/finamp-github-version-0-9-19-release/ (search) | partial, 9.3 |
| ReplayGain, gapless, playback reporting (Finamp) | Uses server ReplayGain tags; reports progress back | Gain tags, reporting API | same Finamp page (search) | yes, 6a / scrobble in 1a |
| Remote / server-client split | Clients talk to Jellyfin server API | HTTP API | (unverified) | yes, 8 protocol |
| Instant mix | Mix built from a seed item | Seed, similarity | (unverified; search found nothing for Finamp) | yes (radio) |

## Navidrome and the Subsonic / OpenSubsonic API

Navidrome is the closest architectural analogue: a headless server speaking an API, with many interchangeable clients (Selecta: engine + protocol + UI plugins).

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Server/client split | Server owns library, transcoding, users; clients are any Subsonic app | HTTP API | https://www.navidrome.org/docs/overview/ | yes, 2, 8 |
| Smart playlists | Rule-based playlists that update dynamically, plus `.m3u` import/sync | Rule engine | https://www.navidrome.org/docs/overview/ | yes, 1a, 9c |
| Scrobbling | Sends play history to Last.fm, ListenBrainz, Maloja | Credentials, play events | https://www.navidrome.org/docs/overview/ | yes, 1a scrobbling (desktop) |
| Transcoding | On-the-fly conversion per user/player, Opus supported | Encoder, per-client profile | https://www.navidrome.org/docs/overview/ | no (engine renders PCM; no transcode path) |
| Multi-user / multi-library | Per-user counts, playlists, favourites; libraries with access control | User table | https://www.navidrome.org/docs/overview/ | no (single user) |
| Sharing | Public links to albums, songs, playlists | Share table, tokens | https://www.navidrome.org/docs/overview/ | partial, 1a `selecta://` links (no public hosted share) |
| Jukebox mode | Server plays on its own audio device, client controls | Server audio out | https://www.navidrome.org/docs/overview/ | yes, this is the headless engine (8) |
| API: playback | `stream`, `download`, `hls`, `getTranscodeStream`, `getTranscodeDecision`, `reportPlayback`, `getNowPlaying` | Song id, format/bitrate params | https://opensubsonic.netlify.app/docs/endpoints/ | partial, 3 OPEN/PLAY/SEEK + events cover it; `reportPlayback` ~ POSITION events |
| API: queue | `getPlayQueue`, `savePlayQueue`, `getPlayQueueByIndex`, `savePlayQueueByIndex` (persist queue + position per user, resume on another client) | Queue ids, current index, position | https://opensubsonic.netlify.app/docs/endpoints/ | partial, 9c queue; persistence across devices not stated (hand-off in 1a would need it) |
| API: playlists | `getPlaylists`, `getPlaylist`, `createPlaylist`, `updatePlaylist`, `deletePlaylist` | Playlist ids, song ids | https://opensubsonic.netlify.app/docs/endpoints/ | yes, 9c |
| API: search | `search`, `search2`, `search3` (artists, albums, songs, paged) | Index | https://opensubsonic.netlify.app/docs/endpoints/ | yes, 9.2 query language |
| API: scrobble / annotation | `scrobble`, `setRating`, `star`, `unstar`, `getStarred`, `getStarred2` | Play events, rating, starred set | https://opensubsonic.netlify.app/docs/endpoints/ | yes, 1a ratings/liked/scrobble |
| API: browse | `getMusicFolders`, `getMusicDirectory`, `getIndexes`, `getArtists`, `getArtist`, `getAlbum`, `getSong`, `getGenres`, `getSongsByGenre`, `getAlbumList(2)` (newest/random/etc.), `getRandomSongs` | Catalogue | https://opensubsonic.netlify.app/docs/endpoints/ | yes, 9.2 browse dimensions, 9b |
| API: info / similar | `getArtistInfo(2)`, `getAlbumInfo(2)`, `getSimilarSongs(2)`, `getTopSongs`, `getSonicSimilarTracks`, `findSonicPath` | Metadata + similarity | https://opensubsonic.netlify.app/docs/endpoints/ | partial, 1a similar + also-by |
| API: lyrics, cover art | `getLyrics`, `getLyricsBySongId` (synced), `getCoverArt`, `getAvatar` | Lyrics, art | https://opensubsonic.netlify.app/docs/endpoints/ | partial (cover art via 9b grid/cover flow); lyrics no |
| API: bookmarks | `getBookmarks`, `createBookmark`, `deleteBookmark` (position in a track) | Song id, position | https://opensubsonic.netlify.app/docs/endpoints/ | yes, 1a cue points + bookmarks |
| API: radio | `getInternetRadioStations` + create/update/delete | Station list | https://opensubsonic.netlify.app/docs/endpoints/ | yes, 1a streaming radio stations (M5) |
| API: shares, jukebox, scan | `getShares/createShare/updateShare/deleteShare`, `jukeboxControl`, `startScan`, `getScanStatus` | Share tokens, scan state | https://opensubsonic.netlify.app/docs/endpoints/ | partial (jukebox = engine; scan = source `listing` in 9.1; shares no) |
| API: system, users, podcasts, chat | `ping`, `getLicense`, `getOpenSubsonicExtensions`, `tokenInfo`, user CRUD, podcasts, chat | n/a | https://opensubsonic.netlify.app/docs/endpoints/ | no (single user; podcasts and chat out of scope) |

## MusicBee

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Library + auto-tagging | Scans folders, fixes messy tags automatically, CD ripping/tagging | Tag lookup service | https://www.getmusicbee.com/ | partial, 1a "local notes + tag edits"; no online auto-tag |
| Gapless, volume normalisation, logarithmic volume | Standard playback quality set | Gain data | https://www.getmusicbee.com/ | yes, 1a, 6a |
| EQ 10/15 band + DSP, upmix, resample | Built-in DSP chain, stereo to 5.1 upmix | DSP config | https://www.getmusicbee.com/ | partial, 1a DSP chain (EQ, reverb, stereo expander); no 5.1 upmix |
| WASAPI / ASIO output | Exclusive-mode outputs | Output API | https://www.getmusicbee.com/ | yes, 5 output plugins |
| Skins, plugins, visualizers, developer API | Add-on categories incl. theater modes | Plugin ABI | https://www.getmusicbee.com/ | yes, 1a theming + bitmap skins, 10 plugin ABI |
| Device sync with format conversion | Syncs playlists/podcasts to phones, converts on the fly | Device profile, converter | https://www.getmusicbee.com/ | partial, 1a module converters; no device sync |
| Podcasts, web radio, last.fm | Extra sources and scrobbling | Feeds, API keys | https://www.getmusicbee.com/ | partial (radio + scrobble yes; podcasts no) |

## Swinsian

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Smart playlists | Rules on rating, artist, bit rate, play count; can use membership in other playlists; imports iTunes smart playlists | Tag + stats fields | https://swinsian.com/blog/ and https://eshop.macsales.com/blog/47113-shuffle-your-music-player-with-five-itunes-alternatives-for-macs (search) | partial, 1a smart playlists; playlist-membership rule and iTunes import not stated |
| iTunes-style library browser | Optional column browser and album art view; customisable columns | Tags | https://soundtrackcentral.com/topics/8627/recommended-music-player-app-for-mac-for-local-music-collection (search) | yes, 9b |
| Duplicate finder | Finds duplicate tracks | Hash/tags | https://eggfreckles.net/notes/swinsian (search) | yes, 1a duplicate detection by md5 |
| Multi-track tag editing | Batch tag edits | Tag writer | https://eggfreckles.net/notes/swinsian (search) | partial, 1a local tag edits (batch not stated) |
| 10/31-band EQ, gapless, AirPlay | Output and DSP basics | DSP config | https://mac.softpedia.com/get/Audio/Swinsian.shtml (search) | yes, 1a DSP chain, AirPlay output |

## Poweramp

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Folder-based playback | Plays from folder hierarchy as well as library; shuffle across a folder tree | Directory tree | https://apkmirror.com/apk/max-mp/poweramp/poweramp-v3-build-819-play-release (search) | yes, 9b tree view, 9.1 local source + archives as folders |
| Crossfade, fade, gapless/preload options | Configurable fade lengths and next-track preload | Two decoder instances | same page (search) | yes, 1a |
| ReplayGain | Applies gain tags | Gain per track | same page (search) | yes, 6a |
| 10-32 band EQ, parametric, 64-bit engine | Graphical + parametric EQ presets | DSP config | https://www.apkmirror.com/?p=46773 (search) | partial, 1a DSP chain EQ; integer-only Amiga tier uses no 64-bit float path (12) |
| Direct Volume Control | Bypasses system mixer to adjust level in the audio path | Output API | https://www.analyticsinsight.net/specifications/poweramp (search) | partial, 5 output plugins; not stated |

## Library presentation

| Feature | How it works (one line) | Data it needs | Source | In Selecta design? |
|---|---|---|---|---|
| Column browser (iTunes) | Genre/Artist/Album panes above the list; selecting a value filters the next pane | Distinct values per tag with counts | (unverified; Apple page not found) | yes, 9b drilldown |
| Facets (foobar2000, foo_facets) | Linked lists with multiple columns, album art, statistics, search over the media library | Per-field value lists, any tag | https://www.foobar2000.org/components/view/foo_facets | yes, 9b (facets named explicitly) |
| Cover Flow | 3D carousel of covers | Cover art cache | (unverified) | yes, 9b cover flow (RTG; text UI needs sixel/kitty) |
| Grid / tile view | Cover tiles with title lines, sorted and grouped | Cover art cache, grouping key | https://soundtrackcentral.com/topics/8627/recommended-music-player-app-for-mac-for-local-music-collection (search, "album art view") | yes, 9b grid |
| Tree view | Hierarchy by folder / artist / album, lazy children | Hierarchical paths | https://jellyfin.org/docs/general/server/media/music/ (folder-driven layout) | yes, 9b tree, source > format > artist > file |
| Drilldown / Miller columns | Successive columns each narrowing the next (Finder column view) | Grouping keys | (unverified) | yes, 9b |
| Smart playlists | Saved rules, live updating, optional limit | Rule evaluator | https://support.apple.com/guide/music/create-edit-and-delete-smart-playlists-mus1712973f4/mac | yes, 9c views |
| Query languages | Typed field filters (`artist:`, `rating:`) + free text; foobar2000/MusicBee style | Parser, field map | https://www.navidrome.org/docs/overview/ (smart playlist rules, no syntax described); syntax (unverified) | yes, 9.2 one query language |
| Nested rule groups | Rule groups nested via Option+Nest | AST in rule engine | https://support.apple.com/guide/music/create-edit-and-delete-smart-playlists-mus1712973f4/mac | partial, 9.2 filters stated; nesting / OR not stated |

## Gaps

Features seen in the players above that the design does not cover at all (status "no"):

1. Synced lyrics (Spotify, Jellyfin `.lrc`, OpenSubsonic `getLyricsBySongId`).
2. Audio-feature similarity vectors and sonic paths (Plexamp sonic analysis, Sonic Adventure, `getSonicSimilarTracks`/`findSonicPath`); design has only tag/rating-based similarity.
3. Multi-user and multi-library, user accounts (Navidrome, Apple "multiple libraries").
4. Server-side transcoding and `getTranscodeDecision`-style negotiation.
5. Blend-style generated shared playlists from several users' histories.
6. Persisted cross-device play queue (OpenSubsonic `savePlayQueue`); needed to make Connect hand-off resume. Design lists hand-off but no queue persistence command.
7. Multi-zone grouping with synced clocks (Roon zones).
8. Podcasts, chat, user admin endpoints (OpenSubsonic); likely out of scope but undeclared.
9. Song/video switch, Atmos/immersive audio, hosted public share links.
10. Per-song "enabled/skip" checkbox (Apple "match only checked").
11. Named DJ modes / LLM playlist prompt as a built-in feature (Guest DJ, Sonic Sage); the MCP server in 1a makes an external one possible.

Partial and worth a decision: smart shuffle (inject suggestions into a playlist), reshuffle command, history-driven auto-download, smart-playlist limit and nested OR rules, signal-path readout, 5.1 upmix.

## Verification notes

- Fetched and confirmed: OpenSubsonic endpoint list, Navidrome overview, Jellyfin music docs, foo_facets page, MusicBee homepage, Apple Music pages (smart playlists, lossless, folders, ratings, guide index), Spotify Connect, Blend, Smart Shuffle (one line only).
- From WebSearch results only (snippets, pages not opened): Plexamp, Roon, Tidal, YouTube Music, Swinsian, Poweramp, Finamp, Spotify folders/offline/queue, Apple Playing Next.
- Failed (403/404/not found): Plex support sonic-analysis page, Roon help zones, Spotify crossfade page, Apple column browser page. Rows depending on them are "(unverified)".
- Not checked: Spotify Canvas, lyrics, loudness normalisation, Tidal official help, YouTube Music radio details.
