---
date: 2026-10-22
topic: bbmp feature parity checklist (ledger F13b)
tags: [parity, bbmp, feature-bar, checklist]
status: final
---

# bbmp parity checklist

Source: bbmp clone (README.md 863 lines, docs/collections.md, docs/testers.md, src/bbmp/cli.py, tui/app.py, settings.py). docs/replayers.md (36 `##` sections) and docs/formats.md (sections: Verified against the original, Plays no fault known, Plays with faults or gaps, Doesn't play) are format lists, covered by design doc section 5 and the format audit; only their roles are listed here.

Section numbers follow the REAL headings of the design doc: 3 engine (state, commands, events: playback), 4 replayer SDK, 5 output/DSP, 6 visualizers, 6a loudness, 7 UI, 8 headless/remote, 9.1 sources, 9.2 catalogue, 9.3 cache/companions/prefetch, 9b view model, 9c playlists, 17 playlist formats. NOTE: the task brief called 6 "playback"; in the doc file 6 is Visualizers and playback is 3.

README = /private/tmp/.../scratchpad/ext/bbmp/README.md (section named, not line). Paths below are under src/bbmp/.

## Playback

| Feature | Where in source | Selecta section |
|---|---|---|
| Pause / resume (Space) | tui/app.py:114 | 3 |
| Stop (x, Shift+Space) | tui/app.py:115 | 3 |
| Next / previous song (n / p) | tui/app.py:116-117 | 3 |
| Next / previous subsong ( ] / [ ) | tui/app.py:118-119 | 3 |
| Seek +-10 s (. / ,) | tui/app.py:120-121 | 3 |
| Seek works in every format; emulators run at full speed to the position (Amiga ~0.1 s/min, C64 ~2 s/min); SunVox jumps to the line before | README "Gapless playback and seeking" | 4 (replayer seek contract) |
| Volume up/down +-5 (+ / -), persisted | tui/app.py:122-123, settings.py:volume | 3 |
| Repeat cycle: off / whole queue / current song (R) | tui/app.py:134 | 3 |
| "On and on" (o): current song loops until another starts, this song only, playlists unchanged | tui/app.py:136 | 3 |
| Shuffle queue on/off (S) | tui/app.py:135 | 3 |
| Radio (r): endless random songs from selected format/artist/platform/chip/collection | tui/app.py:124 | 3 + 9.2 |
| Radio from everything (Ctrl+R); in a platform/chip view, all of that | tui/app.py:133 | 3 + 9.2 |
| Gapless: next song chosen and opened in background, also with repeat/shuffle/radio; ended songs play to last sample, only skip cuts | README "Gapless" | 3 |
| Song length rule: self-ending songs end; looping songs play once and fade into repeat where replayer can; endless formats (SID, SNDH, ZXTune, VGM, S98) stop at known length (HVSC list, file) else 3 min with 4 s fade; max 1 h | ending.py:24-25 | 3 + 4 |
| Songlength database use (HVSC lengths, zxart lengths) | README "How long a song plays", collections | 9.2 |
| Silence never played at song start or end | README "How long a song plays" | 3 |
| Subsongs: start song first then the rest in order like an album | README "Subsongs" | 3 + 4 |
| Silent subsongs skipped at once | README "Subsongs" | 3 |
| Setting: play all subsongs or first only | settings.py:subsongs | 3 |
| Loudness normalisation to -18 LUFS (ITU-R BS.1770), measured after first play and stored | README "The same loudness" | 6a |
| Loudness: first-second pre-measure, 5 s follow-up, only turned down afterwards, max +12 dB boost, no clipping | README "The same loudness" | 6a |
| Loudness shown on Info tab with applied gain; setting to switch off | settings.py:level | 6a + 7 |
| Visualiser cycle (v): spectrum, gradient spectrum, mirrored spectrum, spectrogram, oscilloscope, channel scopes, channel bars, tracker pattern, off (nine) | tui/app.py:154, visuals.py | 6 |
| Demo-effect visualisers (copper bars, starfield, plasma, sine scroller with song name, 20 s each) behind setting | settings.py:demo_effects | 6 |
| Visualisers skip modes the song lacks (no channels / no pattern) | README keys table | 6 |
| Channel scope shapes guessed from notes (replayers give loudness not sound) | README keys table | 6 (unverified how Selecta gets per-channel data) |
| Last visualiser remembered | settings.py:visualizer | 7 |
| Reopen where I left off: queue, song, position restored paused | settings.py:resume, session.json | 3 + 9c |
| Known-not-to-play marker `!` per song, radio skips them, retry on deliberate play clears mark | README "Songs known not to play", trouble.py, known_trouble.py | 4 + 9.2 |
| Songs silent 30 s or failing to open noted locally; unreachable server not held against song | README "Songs known not to play" | 4 |
| Census of up to 40 songs per format shipped (218 known-bad songs) | README, tools/format_census.py | 4 (NEW tooling, see format audit) |
| Per-format status line: verified / faults / does not play; `+` needs extra file | README "What is known about a format" | 7 + 4 |
| Sandboxed audio decode in separate processes streaming raw PCM over a pipe | README "Replayers are shut in" | 4 |
| Audio output via PulseAudio; null output for tests | README, audio.py | 5 |
| Export-grade loudness levelling (whole-song measure) | README "Exporting" | 6a |

## Queue and playlists

| Feature | Where in source | Selecta section |
|---|---|---|
| Queue tab (3); Enter in queue jumps to song | tui/app.py:230, README keys | 7 + 9c |
| Enqueue song at end (a); on an artist, all their songs | tui/app.py:145 | 9c |
| Play next (N) | tui/app.py:146 | 9c |
| Enter plays from selected song onward | README keys | 9c |
| Remove song from list (Delete/Backspace); on playlist name deletes playlist | tui/app.py:150 | 9c |
| Move song up/down in playlist (Shift+Up/Down, K/J) | tui/app.py:152-153 | 9c |
| Add song to playlist (l) | tui/app.py:147 | 9c |
| Save queue as playlist (L) | tui/app.py:148 | 9c |
| Rename playlist (F2) | tui/app.py:151 | 9c |
| Playlists tab (4) lists favorites and playlists; `a` on a playlist name queues it all | tui/app.py:232 | 9c + 7 |
| Playlists mix songs from any collection and local files | docs/collections.md "What is shared" | 9c |
| Import .m3u/.m3u8: Modland/mirror URLs, Modland paths, relative local files | cli.py:448, README Playlists | 17 |
| Import via Files tab (Enter on .m3u) | README Playlists | 17 + 7 |
| `bbmp import --name` | cli.py:819-821 | 17 |
| Import Modland's published playlists (favourites, music disks, demo-party compos) | collection `published_lists()/read_list()`, README | 9.1 + 17 |
| Moved-song re-resolve only when unambiguous (one same-name file, same artist, or artist-less-then); reports omitted count | README Playlists, collection `parse()` | 9.2 + 17 |
| Playlist songs never evicted from cache | README Playlists | 9.3 |
| `bbmp playlists` list with counts | cli.py:471,823 | 8 (CLI) + 9c |
| Exported playlists carry collection URLs (`url(path)`, `path_in(url)`) | docs/collections.md | 17 |
| Queue persisted in session.json | paths, README "Where things are kept" | 9c |

## Browse and search

| Feature | Where in source | Selecta section |
|---|---|---|
| Browse tab: three-column ranger/yazi style (parent, current, preview) | README "Browsing", tui/browse.py | 9b + 7 |
| First list: All plus each collection | README "Browsing" | 9b |
| Per-collection entries: Formats, Platforms, Chips, Artists, New | README "Browsing" | 9b |
| Formats -> artists -> songs drill | README "Browsing" | 9b |
| Platforms (Amiga, C64, Mega Drive...) -> formats -> artists -> songs (b key) | tui/app.py:126 | 9b |
| Sound chips (SID, Paula, YM2612, OPL...) -> same (b again); chip data shipped (known_chips.py, song_chips.py) | tui/app.py:126, known_chips.py | 9.2 + 9b |
| Artists A-Z with everything across formats | README "Browsing" | 9b |
| New: songs added/changed by day, last 60 days kept | README "Sizes, and what is new" | 9.2 |
| HVSC layout view (Musicians, Games, Demos) plus HVSC-as-format in Modland | docs/collections.md "hosted" | 9.1 + 9b |
| Sizes shown at every Browse level (collection, format, artist, song) | README "Sizes" | 9.2 + 9b |
| Narrow terminal drops the parent column; Browse reopens where left | README "Browsing", settings.py:browse | 7 |
| Go to playing song in Browse (g) | tui/app.py:139 | 9b |
| Dedup of same file across collections by md5, shown "▸ title ... A + B"; right arrow expands one row per collection, Enter plays chosen copy | README Collections, docs/collections.md | 9.2 |
| Song in several places of one collection shown in all | README Collections | 9.2 |
| Search tab (2) / box (/) | tui/app.py:125,225 | 7 + 9.2 |
| Query syntax: words, `artist:` `format:` `title:` `album:` `platform:` `chip:`, quoted phrases, `-exclude` | README keys table | 9.2 |
| Full-text search via SQLite FTS | README "Where things are kept" | 9.2 |
| Search scope: next collection or all (c) | tui/app.py:137 | 9.2 + 9b |
| Search box Enter/Down/Esc leaves box for results | tui/widgets.py:52-53 | 7 |
| Artist page (m, tab 8): song count, formats, albums/games, collaborators ("Worked with", navigable), play counts | pages.py, tui/artist.py | 9.2 + 7 |
| Artist page across collections, notes same name may be two people | docs/collections.md | 9.2 |
| Album/game page (A) incl. archive as album for local archive songs | pages.py, README | 9.2 |
| No biographies by design; web search (w) and Demozoo search (d) open in browser | tui/app.py:142-143 | 7 |
| Song info page (i, tab 7): title, author, tracker, channel/pattern counts, game/copyright tags, ripper, message, instrument and sample names | pages.py, tui/playing.py | 4 (metadata) + 7 |
| Info tab shows format notes, why a song fails, loudness | README | 7 |
| Page Up / Page Down in lists (remappable) | tui/widgets.py:44-45 | 7 |
| Collection filter setting "Browse and search": all or one | settings.py:collection | 9b |
| Remembered browse view (settings.view/browse) | settings.py | 9b |

## Collections and network

| Feature | Where in source | Selecta section |
|---|---|---|
| Six collections: Modland, HVSC, Keygen Music, ASMA, ZX-Art, SNDH | collection/*.py | 9.1 |
| Collection plugin interface (id, listing, parse, transport, url, path_in, published_lists, hosted_by) | docs/collections.md | 9.1 |
| User chooses collections at first start and in Settings; unused costs nothing; disabling keeps saved music/favorites/playlists | README "Choosing collections", settings.py:collections | 9.1 |
| HVSC standalone (own 5 MB list, own servers) or hosted by Modland | docs/collections.md | 9.1 |
| Song key `collection:path` stable identity | docs/collections.md "How a song is saved" | 9.2 |
| Catalog per collection (SQLite) opened as one joined view | catalogs.py | 9.2 |
| Catalog update (u, `bbmp update [coll]`), progress, run in separate process to free memory | apart.py, cli.py:62 | 9.2 |
| Auto-update interval: day/week/2weeks/month/never; `bbmp collections --every` | settings.py:update_every | 9.2 |
| Startup freshness check: ask server list date, notify when newer | README "Sizes, and what is new" | 9.2 |
| Refuse bad listings (empty or far fewer songs) and keep old catalog | README "Keeping music" | 9.2 |
| Multi-server Mirrors: fastest first, 3 s first-byte, 30 s body, unreachable asked last | docs/collections.md "Several servers" | 9.1 |
| Checksum verification of every download, bad server passed over | README Collections, collection md5 | 9.1 + 9.3 |
| Home mirror setting / BBMP_MIRROR, tried first, laid out as `<id>/<root>`; one 3 s wait then last | docs/collections.md, settings.py:mirror | 9.1 |
| FTP and HTTP transports | collection/transports.py | 9.1 |
| Background download of the song under the cursor (instant start) | README Collections | 9.3 |
| Download size ceilings (256 MB song, list, 1 GB archive) vs bad mirrors and zip bombs | capped.py | 9.3 |
| New-since list per collection (`bbmp new -d -c`) | cli.py:145,752 | 9.2 + 8 |
| `bbmp collections` (--servers, --offline, --use) | cli.py:95,743-750 | 9.1 |
| ZX-Art lengths and ASMA/SNDH newer-than-Modland versions | README Collections | 9.1 |
| Catalog snapshot text format (xz) for rebuilds | snapshot.py | 9.2 (unverified need) |
| Not prepared upstream: songs inside collection archives, MP3/Ogg/FLAC | docs/collections.md "What is not prepared" | NEW |

## Cache and files

| Feature | Where in source | Selecta section |
|---|---|---|
| Play-on-demand cache per collection laid out as server | README "Where things are kept" | 9.3 |
| Size limit 0.5/1/2/5/10 GB/none (default 2 GB); LRU eviction | settings.py:cache_limit_mb | 9.3 |
| Favorites, queue songs, playlist songs, kept music never evicted | README Settings | 9.3 |
| Clear cache now | README Settings | 9.3 |
| Companion files (TFMX smpl, PSF libs, IFF-SMUS instruments, Mark Cooksey, MDX/PC-98 banks, junk filtering) hidden and fetched with the song | companions.py | 9.3 |
| Files tab (5): browse music folder, play file and queue its folder, tree open/close | tui/app.py:236 | 7 + 9.1 |
| Local files as first-class songs (favorites, history, stats, resume) | README Files | 9.1 |
| Format detection from extension via Modland extension table | README Files | 4 + 9.1 |
| Play from CLI: folders recursive, files, archives (`bbmp play`, `bbmp path`) | cli.py:360 | 8 + 9.1 |
| Archives: zip, 7z, LHA, RAR; unpacked once whole into cache | archives.py | 9.1 + 9.3 |
| Archive unpack inside sandbox (7z) | README Files | 4 |
| Music folder setting | settings.py:music_dir | 9.1 |
| Extras folder (ROMs/BIOS/samples) with name+size check, BBMP_EXTRAS | extras.py, README "Extra files" | 4 |
| Extras list (`bbmp extras`, Settings) | cli.py:208 | 4 + 8 |
| Downloads of vendor files (soundclub, cybertracker, msxdrivers, amigaplayers, sc68replays, beepoladata, ixalancesamples, organyabank) with checksum, asked at first start | download_pins.py, downloads.py, cli.py:163-181 | 4 |
| `bbmp download all|name`, `--remove`, `--again`; `bbmp downloads` | cli.py:757-762 | 4 + 8 |
| Log of replayer stderr | README Where things are kept | 3 (unverified) |
| Disk space check before bulk save | README Keeping music | 9.3 |

## Favorites, kept, history

| Feature | Where in source | Selecta section |
|---|---|---|
| Favorite toggle (f); favorites list on Playlists tab | tui/app.py:128 | 9c |
| Favorite identity same across HVSC-via-Modland and direct | docs/collections.md | 9.2 |
| History tab (6) of every play | tui/app.py:238 | 9.2 + 7 |
| Play counts as listening statistics (counted after 20 s) | README Statistics | 9.2 |
| Stats tab (9): plays, listening time, plays/day, top songs/artists/formats, week/month/year/all | stats_view.py | 7 |
| Keep (s): song, album/game, artist, format, whole collection, playlist (kept as it changes), favorites | keep.py, tui/app.py:127 | 9.3 |
| Keep shows size and free disk, refuses if no fit | README Keeping music | 9.3 |
| Keep fetch background, parallel, multi-server; resumes after restart | README Keeping music | 9.3 |
| Kept marker `⤓` in lists | README Keeping music | 7 |
| Kept music played from disk without servers, exempt from cache limit | README Keeping music | 9.3 |
| Kept set reconciled on catalog update (added fetched, changed refetched, moved moved locally, removed stays) | README Keeping music | 9.3 |
| Kept folder visible and laid out as server in ~/Music; Files tab shows it | README Keeping music | 9.1 + 9.3 |
| Kept folder relocatable (Settings, `bbmp keep --folder`), moves with database | cli.py:778 | 9.3 |
| Fetch-only-at-night setting (00-07) and fetch speed cap | settings.py:keep_when, keep_speed | 9.3 |
| HVSC whole-collection via the single 85 MB archive with per-tune verification | README Keeping music | 9.3 |
| `bbmp keep` CLI: missing, song, album, playlist, collection, format, artist, --stop, --list, --check | cli.py:520, 766-780 | 8 + 9.3 |
| "Saved on this computer" list in Settings | README Settings | 7 |
| Activity panel (t): progress of save/export, failures, stop/continue | tui/activity.py, tui/app.py:157 | 7 |
| Export (e) to FLAC, MP3, Ogg Vorbis (~190 kbit), Opus (~128), WAV, tagged title/artist/album/format | export.py, tui/exporting.py | 5 |
| Export targets: song, playlist, artist's songs in format, all artist, queue; numbered folder `Queue/01 - Artist - title.mp3` | README Exporting | 5 + 9c |
| Export subsongs as one file each, silent subsongs dropped; `--one` start song only | cli.py:810 | 5 |
| Export background with Stop exporting; `bbmp export` options (-o, -f, --wav, --all, --playlist, --artist, --no-level, -c) | cli.py:374, 802-817 | 5 + 8 |
| Export levelling on/off; export format setting | settings.py:export_format | 6a + 5 |
| Reports unplayable songs skipped in export | README Exporting | 5 |

## TUI keys and tabs

| Feature | Where in source | Selecta section |
|---|---|---|
| Tabs 1-0: Browse, Search, Queue, Playlists, Files, History, Info, Artist, Stats, Settings (+ Help tab) | tui/app.py:215-260 | 7 |
| All keys remappable by id via settings `keys`, comma lists | tui/app.py:114-158, settings.py:keys | 7 |
| Help (?) tab listing all keys | tui/help.py, tui/app.py:131 | 7 |
| Quit (q) | tui/app.py:132 | 7 |
| Collection switch (c), view switch (b), go-to-playing (g) | tui/app.py:126,137,139 | 7 + 9b |
| Right/left: enter/leave in Browse, expand/collapse copies | tui/widgets.py:42-43,607 | 7 |
| Update catalogs (u) | tui/app.py:144 | 7 |
| Sounds wrong (!) key with note dialog | tui/app.py:129, wrong.py | NEW |
| Activity (t) | tui/app.py:157 | 7 |
| Theme preview as you scroll the list, Enter keeps, Esc reverts | tui/settings_tab.py | 7 |
| 26 themes incl. 4 Amiga (Workbench 1.3, 3.1, copper, guru meditation) and Textual's | theme.py, amiga_themes.py | 7 |
| Visualisers and meters take theme colours | README Themes | 6 + 7 |
| Dialogs: choose collections, confirm, text prompt, playlist picker | tui/dialogs.py, tui/saving.py | 7 |
| First-start flow: choose collections, build catalogs, offer downloads, offer `bbmp` command | README Running, settings.py:asked_* | 7 |
| Guard/safety for destructive TUI actions | tui/guard.py (unverified content) | 7 (unverified) |
| Visualiser area, level meters, now-playing bar | tui/playing.py, visuals.py | 6 + 7 |
| Headless Textual pilot tests, null audio | README Tests | 13 |

## CLI options

| Feature | Where in source | Selecta section |
|---|---|---|
| `bbmp` (TUI), `bbmp <files/folders/archives>` plays them in the TUI | cli.py:main | 7 + 8 |
| `--version` | cli.py:735 | 8 |
| `update [collections...]` | cli.py:62,738 | 8 |
| `collections [--servers --offline --use NAME... --every]` | cli.py:95,743-750 | 8 |
| `new [-d N] [-c coll]` | cli.py:145,752 | 8 |
| `downloads`, `download NAMES [--remove --again]` | cli.py:163,181 | 8 |
| `extras` | cli.py:208 | 8 |
| `keep [NAME...] [--stop --list --check --playlist --folder]` | cli.py:520 | 8 |
| `search WORDS [-n limit] [-c coll]` (x marks unplayable) | cli.py:234 | 8 |
| `formats [names] [-n notes] [-c]` | cli.py:269 | 8 + 4 |
| `replayers [names] [-d details]` (versions, licences, composition) | cli.py:247,793 | 4 |
| `play WHAT [-c]` headless, accepts search words | cli.py:360 | 8 |
| `export WHAT [-o -f --wav --all --one --playlist --artist --no-level -c]` | cli.py:374 | 8 + 5 |
| `import FILES [--name]`, `playlists` | cli.py:448,471 | 8 + 17 |
| `radio [format] [artist] [-c]` | cli.py:649 | 8 |
| `info` (catalogs, cache, sandbox, replayers present) | cli.py:677 | 8 |
| `wrong` (list file and path) | cli.py:664 | NEW |
| `command` (install `bbmp` launcher from AppImage/Flatpak) | cli.py:653 | 12 (packaging) |
| MPRIS control (media keys, playerctl, desktop widget) | mpris.py | 8 |

## Settings

| Feature | Where in source | Selecta section |
|---|---|---|
| Songs with subsongs: all / first only | settings.py:subsongs | 3 + 7 |
| Repeat (off/all/one), Shuffle | settings.py:repeat,shuffle | 3 |
| Same loudness for all songs | settings.py:level | 6a |
| Reopen where I left off | settings.py:resume | 3 |
| Browse and search scope (all / one collection) | settings.py:collection | 9b |
| Visualizer choice, Demo effects with v | settings.py:visualizer,demo_effects | 6 |
| Theme | settings.py:theme | 7 |
| Home mirror address | settings.py:mirror | 9.1 |
| Cache size limit, Clear cache now | settings.py:cache_limit_mb | 9.3 |
| Collections I have | settings.py:collections | 9.1 |
| Update automatically interval | settings.py:update_every | 9.2 |
| Export songs as (format) | settings.py:export_format | 5 |
| Folder for saved music | settings.py:keep_dir | 9.3 |
| Fetch saved music (any time / night) and Fetching speed | settings.py:keep_when,keep_speed | 9.3 |
| Music folder (Files tab) | settings.py:music_dir | 9.1 |
| Downloads and Extra files panels | README Settings | 4 + 7 |
| Saved-on-this-computer panel | README Settings | 9.3 + 7 |
| Key remapping (`keys`) | settings.py:keys | 7 |
| Volume persisted | settings.py:volume | 3 |
| Settings saved the moment they change, JSON file | settings.py | 7 |
| Env vars: BBMP_MIRROR, BBMP_EXTRAS, BBMP_KEPT, BBMP_SANDBOX (0/landlock), BBMP_BUNDLE, BBMP_CHIPS; legacy MODLAND_* fallbacks | README, grep of src/bbmp | 7 (config/env) |

## Replayer helpers

| Feature | Where in source | Selecta section |
|---|---|---|
| Replayer breadth: libopenmpt, UADE, GME, libsidplayfp, ZXTune, libvgm, StSound, psgplay, sc68, AdPlug, mdxmini, 98fmplayer, libkss, EUPPlayer, Furnace, libpxtone, organya, Highly Quixotic, snsf9x, SunVox, libxmp, Retrovert (Klystrack, iXalance), plus ~36 own replayers (docs/replayers.md) | README "How it plays everything", docs/replayers.md | 5 (task says replayers) / 4 |
| Format-to-backend table generated by census | backends/formats.py, tools/format_census.py | 4 |
| Each replayer a separate process streaming raw audio; crash ends only that song | README | 4 |
| bubblewrap sandbox: no network, read-only system, hidden home, 2 GB memory cap | sandbox.py | 4 |
| Landlock + seccomp fallback launcher `bbmp-shut` (ABI table), warning when no sandbox | README, sandbox.py | 4 |
| `bbmp info` reports active sandbox and refusals | cli.py:677 | 4 + 8 |
| `tools/try_helper.py` helper test harness | README | 13 |
| Format notes (format-notes.toml), replayer facts, platform table, known chips | format_notes.py, replayer_facts.py, platform_table.py | 4 |
| Licence-separated helper programs (GPL2-only, non-commercial) | README build table | 4 (licensing) |
| Build-from-source of all replayers with setup script and `--check` | tools/setup.sh | 12 |
| Vendor-file downloads and extras (see Cache and files) | extras.py, downloads.py | 4 |

## Misc

| Feature | Where in source | Selecta section |
|---|---|---|
| Packages: Flatpak, AppImage, macOS Apple Silicon | docs/testers.md, README | 12 |
| Flatpak filesystem override for extra folders | README | 12 |
| Demo video recorder (scripted headless tour, captions) | tools/record_demo.py, record_overview.py | 13 (NEW tooling) |
| Testers guide (keys needed, `!` reporting, `bbmp info`) | docs/testers.md | NEW |
| Local-only: sends nothing by itself | README, testers.md | 1 (non-goal/principle) |
| Mac/Linux data locations table (catalogs, user.db, session.json, settings, cache, kept, log) | README "Where things are kept", paths.py | 9.2 + 9.3 |
| GPL-3.0-or-later licence | COPYING | 1 (unverified Selecta licence) |
| Stats opt on skipped-song filtering (20 s rule) | README Statistics | 9.2 |
| Ratings / DEViLBOX | grep: none found in bbmp | not in bbmp |

## Counts

- Total rows: 223
- Mapped to a Selecta section: 218 (a few marked unverified)
- NEW: 4
- Not in bbmp (ratings / DEViLBOX, recorded for completeness): 1

## NEW list

- Not prepared upstream: songs inside collection archives, MP3/Ogg/FLAC (docs/collections.md "What is not prepared") - NEW
- Sounds wrong (!) key with note dialog and sounds-wrong.txt (tui/app.py:129, wrong.py) - NEW
- `bbmp wrong` command (cli.py:664) - NEW
- Testers guide (docs/testers.md) - NEW
