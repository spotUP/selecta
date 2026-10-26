---
date: 2026-10-26
topic: YouTube and YouTube Music as a Selecta source (I285), yt-dlp helper design
tags: [plan, source, youtube, yt-dlp, stream, opus, aac, m5]
status: design, approved direction (Johan 2026-10-26: "yt-dlp helper"), not implemented
rows: I285 in thoughts/spot/todos.md; implementation rows to be added when the plan is scheduled
---

# YouTube source (I285)

Johan, 2026-10-25: "we need to be able to search and play youtube and youtube music". On 2026-10-26 he chose, from the
three options put to him, the **yt-dlp helper**: Selecta calls `yt-dlp` as an external program for search and for the
stream URL, and decodes the audio itself. Desktop only. The user installs yt-dlp. This document fixes the design so the
implementation rows can be written against it.

Implements design doc `2026-10-22-architecture-design.md` section 9.1 (sources; a source is one `sel_source`
registered in `src/library/source.c`), 9.2 (catalogue and cache), 13 (tests through the top-level entry point), 14
(milestone table: this is M5+, not parity with bbmp or HippoPlayer) and the F51 rule at line 127/341: a decoder with
licensing or patent weight is a plugin so the licence stays isolated.

## 1. Decisions

| ID | Decision | Why |
|----|----------|-----|
| D-YT-1 | Helper is `yt-dlp`, found on `PATH`, overridden by config key `yt_dlp` or env `SELECTA_YT_DLP`. No bundled copy, no self-update. | Johan's choice. yt-dlp tracks YouTube's changes weekly; bundling it means shipping stale extractors. |
| D-YT-2 | Desktop only: the source registers with no `SEL_SRC_TRANSPORTS` tag and `sel_source_register_builtin` skips it on `SEL_AMIGA`. The Amiga never sees `youtube:` rows (no mirror route, no DEViLBOX proxy). | yt-dlp is a Python program; no 68k port. Design 9.1 names the Amiga path through DEViLBOX `/api/mirror/<source>/<path>`; a mirror would have to transcode and store YouTube audio, which the terms (section 6) forbid. |
| D-YT-3 | Results are **not catalogue rows**. `listing` is NULL; the source has no `LIBRARY_SYNC`. A search returns transient rows kept in an in-memory result set with an id; a played or queued result is pinned into the `songs` table as source `youtube`, path `<video id>`, so history, ratings and playlists work. | YouTube has no finite catalogue to mirror; the DB keeps only what the user touched. |
| D-YT-4 | New engine command `SOURCE_SEARCH "ss"` (`<source id> <query>`) with a reply `search result <n> <source>:<path>|<title>|<artist>|<duration ms>|<thumb url>` per row and a final `search done <source> <count>`. `LIBRARY_SEARCH` stays local-DB only. | Live search is asynchronous and network-bound; mixing it into `LIBRARY_SEARCH` would block the local search every pattern depends on. Other live sources (hippoplayer.se search, Modland live) reuse the verb. |
| D-YT-5 | Two search scopes under one source id: `youtube:` (plain YouTube, `ytsearch`) and `ytmusic` (YouTube Music, `https://music.youtube.com/search?q=<query>#songs` through yt-dlp). Both produce `youtube:<video id>` keys; the scope is a search argument, not a second source. | One video id space; YouTube Music results are YouTube videos with better metadata (artist, album). Two sources would duplicate every row. |
| D-YT-6 | Stream URL per play: `yt-dlp -f bestaudio -g --no-playlist <id>` right before `OPEN`; URL is cached in memory for 5 hours (YouTube signs URLs with ~6 h expiry). Playback goes through `sel_live_open` (src/library/stream_open.c) like any `https://` stream. No stream-to-disk (`sel_live_open_record*` refuses `youtube:` sources). | Reuses the download thread and ring; no second HTTP path. Recording is a terms problem (section 6). |
| D-YT-7 | Two new stream decoder plugins: `plugins/replayers/opus` (libopus + libogg demux, plus WebM/Matroska block reader for `bestaudio` which is usually WebM/Opus) and `plugins/replayers/aac` (AAC-LC for the `m4a`/MP4 fallback, via the `faad2`-free single-file decoder `minimp4` + an AAC-LC decoder; candidate: `fdk-aac` is out on licence, use `libhelix-aac` (RealNetworks RPSL) or `ffmpeg`'s decoder is out; decide at implementation after a licence read, D-YT-7a). Both tagged `SEL_RP_NEEDS_FLOAT`, both under `third_party/` as separate directories with their own LICENSE files. The core never links them. | F51: encoder/decoder licensing stays in the plugin. minimp3 and stb_vorbis already follow this shape (plugins/CMakeLists.txt:63-64). |
| D-YT-7a | If no acceptable AAC-LC decoder exists, prefer `-f bestaudio[ext=webm]` and let the AAC plugin wait; Opus covers every modern YouTube upload. Record the gap in the plan when hit. | Opus is present on all YouTube audio since 2015. |
| D-YT-8 | Thumbnails: the `thumb url` from the result row is fetched by the existing art cache (`sel_gui_art_cache`) through the normal HTTP client with the same ring budget as any artwork; no extra disk store. | One art path. |
| D-YT-9 | Helper failures are user-visible states, not log lines: `yt-dlp not found` (install hint), `yt-dlp too old` (version below the floor read from `yt-dlp --version`, floor set at implementation), `search failed: <first line of stderr>`, `stream expired` (re-resolve once, then surface). Each surfaces in `SOURCES` output and in `--stats` as `youtube_state=<name>`. | Johan tests on the Mac without logs open. |
| D-YT-10 | Metadata written to `songs`: title, artist (channel or YouTube Music artist), album (YouTube Music only), duration, video id as path, thumbnail URL as artwork key, `uploaded` date. No lyrics, no comments. | What the UI shows in a row and the Song Information panel. |

## 2. Source module

`src/library/sources/youtube.c` (+ `youtube.h`), registered from `sel_source_register_builtin` under `#if !SEL_AMIGA`.

- `id` `"youtube"`, `name` `"YouTube"`, `home` `https://www.youtube.com`, `about` one sentence naming yt-dlp.
- `tags`: none of `SEL_SRC_HAS_LISTS`, `SEL_SRC_HAS_MD5`, `SEL_SRC_TRANSPORTS`; add a new tag `SEL_SRC_LIVE_SEARCH` so the
  Search page knows which source chips can take a live query.
- `listing` NULL, `published_lists` NULL, `read_list` NULL.
- `parse(path)`: a video id is 11 characters of `[A-Za-z0-9_-]`; anything else is rejected.
- `url(path)` returns `https://www.youtube.com/watch?v=<id>`; `path_in(url)` accepts `youtube.com/watch?v=`,
  `youtu.be/`, `music.youtube.com/watch?v=` and returns the id. This makes `OPEN https://youtu.be/<id>` work by
  routing the URL through source lookup before the plain-stream path (one new branch in the OPEN handler: if any
  registered source's `path_in` claims the URL, the song key becomes `<source>:<path>`).
- New optional function pointers on `sel_source` (shared by any live source):
  - `int (*search)(void* st, const char* scope, const char* query, sel_search_sink* out, sel_progress* p);`
  - `int (*resolve)(void* st, const char* path, char* url, size_t n, unsigned* expires_s);`
  The engine's `SOURCE_SEARCH` calls `search`; the play path calls `resolve` when a key's source has one, and feeds the
  result to `sel_live_open`.

### 2.1 Helper process

`src/library/sources/yt_dlp.c`: one function `yt_dlp_run(const char* const* argv, sel_proc_sink* out, int timeout_ms)`
built on the existing `pipe:` popen path in `stream_open.c:80` extended to take an argv array and capture stderr to a
small buffer (the first line is the user-facing error). Timeouts: search 20 s, resolve 15 s. One helper process at a
time per source; a new search cancels the previous one (kill, reap).

Commands:

- Search, YouTube: `yt-dlp --dump-single-json --flat-playlist --no-warnings "ytsearch25:<query>"`.
- Search, YouTube Music: `yt-dlp --dump-single-json --flat-playlist --no-warnings
  "https://music.youtube.com/search?q=<url-encoded query>#songs"`.
- Resolve: `yt-dlp -f "bestaudio[ext=webm]/bestaudio" -g --no-playlist <id>` (first line is the URL).
- Version check at first use: `yt-dlp --version`.

JSON is read with the project's existing JSON reader (grep `sel_json` under src/ before adding one; `hippo_publish.c`
and `sync_net.c` already parse hippoplayer.se replies). Fields used: `entries[].id`, `title`, `uploader` or
`artists[0].name`/`channel`, `album`, `duration`, `thumbnails[last].url`, `upload_date`.

## 3. Engine and commands

- `include/selecta/commands.def`: `SEL_CMD(SOURCE_SEARCH, "ss", "playlists")`. Reply lines as in D-YT-4. The search
  runs on a worker thread (the same `sel_live` download thread class), results are posted to the engine queue as
  `search result` events, so the control socket never blocks.
- Stats (`include/selecta/tags.def`): `STAT_SOURCE_SEARCHES`, `STAT_SOURCE_RESOLVES`, `STAT_YT_DLP_RUNS`
  (call-count sentinel for every helper spawn), `STAT_YOUTUBE_STATE`. Rendered by `--stats` as
  `source_searches=<n> source_resolves=<n> yt_dlp_runs=<n> youtube_state=<ok|missing|old|failed>`.
- `OPEN youtube:<id>`: resolve, then the existing `https://` play path; the queue, Play Next and history treat the key
  like any song key. `PLAYLIST_ADD_SONG` works unchanged (pins the row, D-YT-3).
- `SOURCES` output gains the live-search tag and the helper state for `youtube`.

## 4. Desktop UI

- Search page: the source chip row (`--view search`, chips already exist for local sources) gets a `YouTube` chip and a
  `YouTube Music` chip (two scopes, D-YT-5). A typed query with one of them selected sends `SOURCE_SEARCH youtube
  <scope>:<query>`; results fill the same result list as local search, with a thumbnail, title, artist, duration. The
  shared song menu from I305 applies (Play, Play Next, Add to Queue, Add to Playlist). While the helper runs the chip
  shows the existing busy indicator; a `yt-dlp not found` state replaces the result list with the install hint text.
- Sources page: one row `YouTube` with the helper state and version; clicking it opens the Search page with the chip
  selected.
- Song Information panel: shows `Source: YouTube`, the channel/artist, and a link row with the public URL (D-YT-10).
- Text UI: `SOURCE_SEARCH` reachable through the existing `/search` prompt with a `yt:` or `ytm:` prefix; one line per
  result; no thumbnails.

## 5. Tests (one reachability test per protocol, per RULES)

- `tests/net/fake_yt_dlp.py`: a script put first on `PATH` as `yt-dlp`. It answers `--version` with a fixed version,
  the two search invocations with fixed JSON (3 entries each, deterministic ids), and `-g` with a URL on the fixture
  server (`tests/net/fixture_server.py`), which serves a short Opus/WebM file and a short AAC/m4a file from
  `tests/golden/stream/`. It writes every argv line to `$YT_DLP_LOG` so the test can assert the exact commands.
- `dui-youtube-search` (tests/dui/youtube_search.cmake, SEL_DUI_REAL): drives the real binary with `--view search`,
  input script selects the YouTube chip, types a query, presses Enter, waits for `search done youtube 3`, right-clicks
  row 2 and picks Play. `--stats` asserts `source_searches=1 source_resolves=1 yt_dlp_runs>=3 fetches=1` and the
  decoder frame count above 0 (sentinel: the Opus plugin ran). Repeat once with the YouTube Music chip, asserting the
  music.youtube.com URL appears in `$YT_DLP_LOG`.
- `net-youtube-missing`: `PATH` without the fake; `--exec "SOURCE_SEARCH youtube yt:test"` and `--stats` assert
  `youtube_state=missing` and the UI text contains the install hint.
- `stream-opus-decode`, `stream-aac-decode`: plugin-level goldens like the minimp3 and stb_vorbis ones
  (tests/m3 stream goldens), hash of the first second of PCM.
- `engine-source-search-cancel`: second search while the first helper is alive; asserts the first is reaped and only
  the second result set is reported.

## 6. Terms and privacy

- yt-dlp is installed by the user; Selecta ships no YouTube client code and never downloads a copy of the helper.
- Audio is streamed into the ring and never written to disk: `sel_live_open_record*` refuses `youtube:` keys,
  the cache budget never pins a YouTube stream, and `LIBRARY_FETCH` on a `youtube:` key is rejected with
  `not fetchable`.
- Searches leave the machine; the Search page says so once (the first time a live chip is picked, a one-line note in
  the result area: "Searches are sent to YouTube through yt-dlp").
- No account, no cookies, no login flow in v1 (yt-dlp `--cookies-from-browser` would unlock age-gated and premium
  items; out of scope, note in Open questions).

## 7. Open questions for Johan

- Q-YT-1: AAC-LC decoder licence (D-YT-7a). Options at implementation time: ship Opus only first (recommended, zero
  licence risk), or add an AAC plugin after a licence read.
- Q-YT-2: Playlists and albums from YouTube Music (a search result that is a playlist). v1 flattens to songs only;
  importing a YouTube playlist as a Selecta playlist is a follow-up row.
- Q-YT-3: Should pinned YouTube rows appear in the Library's Songs list (they are in `songs`)? Default yes, with the
  source column, filterable by `source:youtube` in the query language.

## 8. Rows to add when scheduled

I285a source module + yt_dlp runner + SOURCE_SEARCH + stats; I285b Opus plugin; I285c AAC plugin (after Q-YT-1);
I285d Search chips + Sources row + Song Information; I285e fake yt-dlp + dui-youtube-search + net-youtube-missing;
I285f text UI prefix. All desktop; none touches the Amiga build.
