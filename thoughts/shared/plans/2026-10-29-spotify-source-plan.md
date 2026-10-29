---
date: 2026-10-29
topic: Spotify as a Selecta source (I654): login, playback backends, Amiga tier, song keys, playlists, terms, build rows
tags: [plan, source, spotify, soloist, librespot, cspot, connect, m5]
status: plan, not implemented; decisions D-SP-1..D-SP-6 open for Johan (section 8)
rows: I654 in thoughts/spot/todos.md; build rows I654a-I654i proposed in section 7 (not yet in the ledger)
---

# Spotify source (I654)

Implements design doc `2026-10-22-architecture-design.md` section 9.1 (sources, song key), 9c (playlists), 8 (device
hand-off, line ~334) and 12 (`sel_machine` tiers). Builds on research SP1
(`thoughts/shared/research/2026-10-22_spotify-open-source-client.md`) and SP2
(`thoughts/shared/research/2026-10-22_cspot-amiga-port.md`), and follows the playlist rule of ledger row I653.

Pattern reused: the YouTube source plan (`2026-10-26-youtube-source.md`, I285). It already designs a desktop-only
live source with an external helper process, a `search` and `resolve` hook on `sel_source`, the `SOURCE_SEARCH "ss"`
command, pinned `songs` rows, helper-state stats and a fake-helper test harness. Spotify uses the same hooks and the
same command; it does not invent a second live-source mechanism. Login reuses the integration login shape
(`SEL_INT_LOGIN` in `include/selecta/integration.h`, driven by `apps/selecta-cli/lastfm.c` and
`apps/common/integration_host.c`): a two-step "ask for a code/URL, then exchange it" flow. None of the YouTube rows
(I285a...) is built yet (`SOURCE_SEARCH` is not in `include/selecta/commands.def` as of this plan); whichever of I285a
or I654c lands first builds the shared hooks, the other reuses them.

## 0. What changed since SP1 (read this first)

1. **Spotify shipped an official headless Connect client, Soloist, on 13 Aug 2026**
   (https://developer.spotify.com/blog/2026-08-13-introducing-spotify-soloist). Facts read from its docs
   (https://developer.spotify.com/documentation/soloist/concepts/overview, `/features`):
   - Platforms: Linux, Apple Silicon Mac, x86-64 Windows. No Intel Mac.
   - Audio output: Core Audio (macOS), WASAPI (Windows), PipeWire/PulseAudio (Linux). **No pipe or PCM output is
     documented.** Audio does not pass through the host application.
   - Control: local WebSocket API, off by default, `--ws ADDR:PORT` (port 0 picks a free one, written to `ws.addr` /
     `ws.port`); play, pause, seek, volume, shuffle, repeat, playback events, queue access (up to 80 upcoming tracks per
     a news summary, unverified on the primary page). No authentication on the socket: bind 127.0.0.1 only.
   - Pairing: `--pair` once, stores device identity and session in its data directory. A single-track mode restores
     the stored session and exits when the item finishes.
   - Each build runs 90 days from its build date, then exits with code 10.
   - An API key from a Premium account is needed for setup; afterwards Free and Premium accounts can connect
     (news summary https://alternativeto.net/news/2026/8/spotify-introduces-soloist-a-headless-terminal-client-for-raspberry-pi-and-linux-systems/ ;
     not confirmed on the primary docs page, unverified). Archives must not be redistributed (search summary of the
     developer terms, unverified on the terms page).
2. **librespot-class clients lost playback on 10 Aug 2026** (librespot issue #1737, still open when read on
   2026-10-29; no merged fix or release listed; workaround PRs #1744 desktop client ID and #1745 device authorization
   flow, merge status not shown). Maintainer kingosticks, 17 Aug 2026: Spotify "only closed off access to their private
   endpoints we were using for playback purposes". go-librespot: no 2026 release found (unverified, GitHub releases
   page did not render). Whether go-librespot plays today is **unknown**; row I654a measures it.
3. **Measured 68060 decode speed exists now** (`thoughts/shared/reports/m3-I212b-060-timing-report.md`, real A1200
   68060 with 192 MB fast RAM): stb_vorbis decodes at real-time factor about 17.5 (17 times too slow), minimp3 about
   14.5. Tremor (integer Vorbis, what cspot uses) is not measured on 68k by anyone we found.

Consequence: SP1's single recommendation (go-librespot sidecar) no longer stands alone. There are now two desktop
backends with opposite trade-offs, and the plan builds the source so either can sit behind it (section 2).

## 1. Login and account (desktop)

Two separate credentials, never typed into Selecta:

| Credential | Used for | Who holds it | Flow |
|---|---|---|---|
| Web API token (OAuth 2.0 Authorization Code with PKCE, no client secret) | search, track metadata, cover art, user playlists, Liked Songs, `/me` (product = premium/free) | Selecta: refresh token in the OS secret store | browser consent page, loopback redirect `http://127.0.0.1:<port>/callback` |
| Playback device session | audio | the backend process (Soloist data dir, or go-librespot `state.json` in `~/Library/Application Support/go-librespot`) | Soloist `--pair`, or go-librespot device code at spotify.com/pair |

What the user sees (desktop rich UI, Sources page row `Spotify`):

1. Row reads `Spotify - not connected` with a `Log In` button and one line: "Needs a Spotify account. Playback needs
   Spotify Premium." (Premium is required by every librespot-class client per their READMEs; Soloist's Free support is
   unverified, see 0.1.)
2. `Log In` opens the default browser on Spotify's consent page. Selecta listens once on a loopback port; the browser
   lands on a Selecta page "Logged in as <display name>. You can close this tab." The row turns to
   `Spotify - <display name> (Premium)` or `(Free: search and playlists only)`.
3. Playback pairing, the first time a Spotify song is played: a panel shows the backend's pairing step (Soloist: a
   `Pair this device` button that runs `--pair`; go-librespot: the device code and a `spotify.com/pair` link), then
   `Paired` and playback starts.
4. Failure states are visible text on the row, never only log lines (YouTube plan D-YT-9 pattern):
   `backend not found` (install hint with link), `backend expired` (Soloist exit code 10: "Download the new Soloist
   build"), `login expired` (Log In again), `Premium needed`, `Spotify refused playback` (the #1737 class; text says
   the fault is on Spotify's side and other sources are unaffected).
5. `selecta-cli spotify login` does the same in a terminal (prints the URL, waits for the redirect), like
   `selecta-cli lastfm login`.

Client ID: Spotify developer apps in development mode serve only an allow-listed set of users, and extended quota is
granted to organisations (from memory of the developer policy, unverified for 2026). So a client ID baked into Selecta
would not work for the public. Default: each user registers their own Spotify developer app and pastes the client ID
once (the same per-user model Soloist uses for its API key). Decision D-SP-3.

Secret store: nothing in the repo stores secrets today (searched `keychain`, `SecItem`, `libsecret`, `sel_secret`;
the Last.fm session key sits in config as `scrobble.lastfm_session`, plugins/integrations/lastfm/lastfm.c:313). New
`sel_secret_get/set/del` in `src/core/<os>/` (macOS Keychain via Security.framework, Linux libsecret, Windows
Credential Manager) with a 0600 file fallback in the config dir. Last.fm moves onto it in the same row so there is
one secret path.

## 2. Desktop client choice

| Option | Audio path | Licence | Legal standing | Works today | Build impact |
|---|---|---|---|---|---|
| **Soloist** (Spotify, official) | Soloist plays to Core Audio itself; Selecta controls it over WebSocket. No Selecta DSP, visualizers, loudness, crossfade, recording or Amiga proxy for Spotify songs. | Spotify proprietary, user downloads it (no redistribution) | Sanctioned by Spotify | Yes (launched 13 Aug 2026; not tested by us) | None to link; a WebSocket client in the source (check `src/library/net/` for an existing WS client before writing one; none known) |
| **go-librespot** sidecar | PCM s16le into a named pipe, Selecta's engine plays it like any stream: all DSP, viz, loudness, crossfade, Amiga proxy work | GPL-3.0, separate process the user installs (Homebrew formula exists), so Selecta stays MIT | Reverse-engineered; librespot README itself says use is "probably forbidden"; Spotify developer terms forbid reverse engineering and stream ripping | Unknown since 10 Aug 2026 (#1737 class) | None to link; process supervisor + pipe reader + REST/WebSocket client on port 3678 |
| librespot (Rust, MIT) sidecar | pipe backend, same as above | MIT | same as go-librespot | Broken per #1737 as of the last read | same; no device-code login unless #1745 merged |
| librespot linked via C FFI | in-process | MIT | same | same | Rust 1.85+ toolchain in CMake, our own cdylib over an API that broke in 0.5, 0.7 and 0.8. Rejected (SP1 shape a). |
| Web API + Connect remote of the user's own Spotify app | the Spotify desktop app plays | n/a | sanctioned | yes | smallest; but Selecta becomes a remote only |

**Recommendation: build the `spotify` source with a backend seam, ship Soloist first, add go-librespot second as an
opt-in "route Spotify through Selecta's engine" backend gated by the I654a live test.**

Why Soloist first: it is the only path that is both working and permitted, and it is maintained by the party that
broke librespot in August. Why keep go-librespot: Johan's 2026-10-22 decision wants real Spotify audio in Selecta's
engine and an Amiga proxy route, and only a PCM backend gives that. The seam costs one small vtable:

```c
/* src/library/sources/spotify_backend.h (desktop only) */
typedef struct sel_spotify_backend {
    const char* id;                        /* "soloist", "librespot" */
    int pcm;                               /* 1: audio comes back as PCM through `open_pcm`; 0: plays externally */
    int  (*start)(void* st, const char* exe, char* err, size_t n);         /* spawn + supervise */
    int  (*pair)(void* st, char* out, size_t n);                           /* code/URL or "paired" */
    int  (*play)(void* st, const char* track_id, uint32_t start_ms);
    int  (*control)(void* st, const char* verb, int32_t arg);              /* pause, resume, seek, volume */
    int  (*poll)(void* st, sel_spotify_status* out);                       /* position, state, ended, error */
    int  (*open_pcm)(void* st, sel_live** out);                            /* pcm backends only */
    void (*stop)(void* st);
} sel_spotify_backend;
```

External-playback mode (Soloist, `pcm = 0`) is new for the engine: the engine has always rendered every song itself.
It needs a defined state: the engine's transport, position, queue and history stay authoritative; output and DSP are
bypassed for that song; position comes from backend events; `ended` advances the queue. This is a design change to
section 3 of the architecture doc and gets an Opus design row (I654e) before any build.

## 3. Fast Amigas: verdict

**The Amiga is a remote and proxy tier for Spotify. Native cspot on a 68060 is not realistic; on Vampire or Emu68 it
is an unproven 8-14 week port with no gate passed.**

Evidence:
- Measured: stb_vorbis on a real 68060 runs 17.5 times slower than real time, minimp3 14.5 times
  (`m3-I212b-060-timing-report.md`). Float traps to 68060.library are the likely cause (inferred, not profiled).
- Tremor (integer) on 68060: no measurement anywhere. Aminet VorbisPlayer (Tremor low-quality mode) claims
  "realtime playback for you 68060 users (just about anyway)", no clock or bitrate given. SP2's estimate for 160 kbps
  on a 68060/50 is 70-100% of the CPU (estimate, unverified), leaving nothing for TLS, the UI or AHI mixing.
  AmigaAMP on 68060/50 + AHI uses 42-47% CPU for 44.1 kHz MP3 (https://amigaamp.de/AHIresults.shtml), a cheaper codec.
- The 68060 lacks hardware 32x32->64 multiply (SP2, from memory of the MC68060 manual, unverified), which full
  Tremor needs; Vampire 68080 and Emu68 have it.
- RAM is not the blocker: SP2 estimates 2-4 MB fast RAM peak for a port (estimate); the real 060 box has 192 MB.
  The ESP32 target README says 160-520 KB is not enough and needs PSRAM.
- Licence: cspot GPL-3.0-or-later, its platform library bell has no licence file (SP2 section 1).
- Liveness: nobody has reported cspot working or broken since 10 Aug 2026 (SP2; issue #181 unanswered).

Amiga routes, in order:
1. **Remote** (all Amigas, Soloist or librespot backend): the Amiga UI drives a desktop Selecta over the remote
   protocol (section 8 of the design); Spotify songs play on the desktop. Shows title, artist, cover on RTG.
2. **PCM proxy** (all networked Amigas, librespot backend only): device hand-off of a `spotify:` key to an Amiga makes
   the Amiga ask a desktop Selecta on the LAN for the decoded stream; the desktop runs the backend and serves s16le
   (optionally 22.05 kHz to halve the bandwidth: 44.1 kHz stereo s16 is 176 KB/s) over TCP; the Amiga's PCM source is
   a raw-sample reader. A314 transport is a later variant (SP1 prior art section).
3. **Native cspot** on Vampire/Emu68 only, behind `SEL_MACHINE_CPU` and a `SEL_RP_MIN_CPU`-style tag, and only after
   two gates pass: cspot CLI plays on a desktop today (1 day), and Tremor LQ measured on the target (row I654i).

## 4. Song keys, library DB, search, playlists, queue, hand-off, offline

**Key.** `spotify:<id>`, `<id>` = the 22-character base62 Spotify track id (`[0-9A-Za-z]{22}`). Source id `spotify`.
- `parse(path)`: accepts exactly 22 base62 characters; returns no format/artist (metadata comes from the Web API).
- `url(path)` -> `https://open.spotify.com/track/<id>`.
- `path_in(url)` accepts `https://open.spotify.com/track/<id>[?...]`, `https://open.spotify.com/intl-xx/track/<id>`
  and the Spotify URI `spotify:track:<id>`. This makes `OPEN <pasted Spotify link>` work through the YouTube plan's
  one new OPEN branch (any source whose `path_in` claims a URL turns it into a key).
- Tracks only in v1. Episodes and audiobooks are rejected by `parse` (decision D-SP-6).

**Library DB.** Same as YouTube D-YT-3: no catalogue, `listing` NULL, no `LIBRARY_SYNC`. A searched, played, queued,
liked or playlisted track is pinned into `songs` (src/library/db/schema.c:14) with `source='spotify'`, `path=<id>`,
`name`, `artist` (joined artist names), `album`, `year`, `duration_ms`, `format='Spotify'`. Cover art: the album image
URL stored as the artwork key and fetched by the existing art cache (YouTube D-YT-8). These stored display fields are
what I653 needs: playlist rows show title, artist and cover when logged out or when Spotify is unreachable. No schema
change: every field already exists in `songs`.

**Search.** `SOURCE_SEARCH spotify <query>` (shared verb, YouTube D-YT-4) calls Web API `GET /v1/search?type=track`;
result rows use the shared `search result` reply. The desktop Search page gets a `Spotify` chip; the I652a inline
playlist search lists Spotify as a scope when logged in (I653 requirement). Local `LIBRARY_SEARCH` also finds pinned
Spotify rows through FTS5 (`source:spotify` in the query language).

**Playlists (I653).** A Spotify track in a Selecta playlist is a plain `playlist_entries.key = 'spotify:<id>'` added by
`PLAYLIST_ADD_SONG` unchanged. Import of the user's own Spotify playlists and Liked Songs uses the existing
`published_lists` / `read_list` hooks on `sel_source` (include/selecta/source.h): the Playlists page offers "Import
from Spotify", which copies a Spotify playlist into a Selecta playlist once (pinning every track). Two-way sync is out
of scope (decision D-SP-5). Note: Spotify's 27 Nov 2024 Web API change removed several endpoints for new apps; reading
Spotify-owned editorial/algorithmic playlists was reported among them (unverified); user-owned playlists are expected
to work.

**Queue.** Selecta's queue is the only queue. On reaching a `spotify:` row the engine calls `resolve`; with a PCM
backend the result is an `sel_live` stream (no URL, a pipe), with Soloist it calls `backend->play(id)` and enters
external-playback mode (section 2). Track end advances Selecta's queue; Soloist's own queue is not used (one
authority). Gapless and crossfade across a Spotify song: PCM backend only.

**Device hand-off (design section 8).** `SNAPSHOT_QUEUE` carries `spotify:` keys like any other. The target resolves
them through its own `spotify` source: logged-in desktop plays them; a desktop without login shows the rows with stored
display fields as `Not available: log in to Spotify` and skips them; an Amiga target uses the PCM proxy (section 3,
route 2) when a desktop Selecta with a PCM backend is on the LAN, else skips with the same state. Handing off to a
Spotify-native device (a phone running Spotify) is not Selecta hand-off; Web API `transfer playback` could do it later
(not in v1).

**Offline and cache.** Never: no audio on disk, no prefetch of audio, `LIBRARY_FETCH spotify:<id>` answers
`not fetchable`, record-to-file refuses `spotify:` keys, the loudness render job skips them (no offline render of
Spotify audio). Kept: metadata, cover art (art cache), playlists. Loudness normalisation for Spotify songs uses
Spotify's own normalisation (Soloist) or the adaptive leveler (PCM backend, section 6a); no stored gain.

## 5. Legal and terms, stated plainly

- Spotify Developer Terms (https://developer.spotify.com/terms) permit streaming only through the Spotify Platform
  on approved paths, and forbid reverse engineering the Platform and stream ripping. librespot, go-librespot, spotifyd
  and cspot are reverse-engineered clients; librespot's README says use is "probably forbidden". Shipping or
  defaulting to them is a terms violation by the user's account, with ban risk that nobody has quantified (unverified).
- Soloist is Spotify's own client: using it as documented is permitted. Its builds must not be redistributed
  (unverified), so Selecta links to Spotify's download page and never bundles it. A user-forum question whether
  driving Soloist from a third-party app is allowed has no official answer (unverified).
- Web API: allowed with a registered app; content must not be cached or downloaded; metadata and cover display follow
  Spotify's attribution rules (show "Spotify" as source, link back via `url()`). Exact branding rules not read for this
  plan (unverified); I654b reads them before UI work.
- Licences: Selecta is MIT (LICENSE). go-librespot GPL-3.0 and Soloist (proprietary) run as separate processes the
  user installs, so neither touches Selecta's licence. A native cspot port would be GPL-3 code inside an Amiga plugin:
  that plugin binary would have to be GPL-3, and bell has no licence (blocker until clarified).
- No audio leaves the Spotify-controlled process to disk. The Amiga PCM proxy streams decoded audio over the LAN to the
  same user's machine; it is still a third-party client path and inherits the go-librespot terms problem.

## 6. Tier summary (`sel_machine`, design section 12)

| Tier | Spotify feature |
|---|---|
| Desktop (macOS arm64, Linux, Windows x86-64) | full: login, search, playlists, playback via Soloist; via go-librespot when chosen and working |
| Desktop Intel Mac | Soloist not available; go-librespot backend only, or remote to another device |
| Amiga RTG/AGA, networked | remote control of a desktop Selecta; PCM proxy with a librespot desktop; rows show stored display fields |
| Amiga Vampire / Emu68 | as above; native cspot only after I654i gates |
| Amiga no network | `spotify:` rows visible in playlists with display fields, marked not available |

The `spotify` source registers on desktop only (`#if !SEL_AMIGA`, as YouTube D-YT-2). The Amiga build knows the
`spotify` id for display and hand-off (a stub source with `parse`/`url` only, so keys validate and links show).

## 7. Build rows

Every row: host build plus its tests; one reachability test per feature through the real entry point (the `selecta`
binary or `selecta-cli`), with a stats sentinel. Fakes: `tests/net/fake_soloist.py` and `tests/net/fake_librespot.py`
placed first on `PATH` (YouTube `fake_yt_dlp.py` pattern), a fixture Web API on `tests/net/fixture_server.py`.

| ID | Scope | Files | Reachability test (top-level entry, sentinel) | Model |
|---|---|---|---|---|
| I654a | Live gate (no code): on Johan's Mac with his Premium account, run Soloist (pair, play, WS events) and go-librespot (device-code login, pipe output) against live Spotify; record which plays today and Soloist's real WS protocol (message shapes). Decides D-SP-1 and the I654e/f order. | `thoughts/shared/research/2026-10-xx_spotify-backend-gate.md` | Written log of both runs; needs Johan for login (Blocked on him) | sonnet |
| I654b | `spotify` source skeleton: id, `parse`/`url`/`path_in`, desktop registration, Amiga display stub, pinned `songs` rows, `OPEN <open.spotify.com link>` routing, `not fetchable` / record refusal, failure states in `SOURCES` and `--stats` (`spotify_state=`). Read Spotify branding rules. | `src/library/sources/spotify.c/.h`, `src/library/source.c` (register), OPEN handler in `src/engine/engine.c`, `include/selecta/tags.def` | `selecta --exec "OPEN https://open.spotify.com/track/<fixture id>"` with fake backend; `--stats` asserts `spotify_resolves=1` and a pinned row with the fixture title | sonnet |
| I654c | Shared live-source hooks if I285a has not landed: `search`/`resolve` on `sel_source`, `SOURCE_SEARCH "ss"`, worker thread, `STAT_SOURCE_SEARCHES`. Then Spotify Web API client (search, track, `/me`, playlists, PKCE login, refresh) and `sel_secret` store; Last.fm session moves onto it; `selecta-cli spotify login`. | `include/selecta/source.h`, `include/selecta/commands.def`, `src/library/sources/spotify_api.c`, `src/core/{macos,linux,windows}/secret.c`, `apps/selecta-cli/spotify.c` | `selecta-cli spotify login` against the fixture OAuth server, then `selecta --exec "SOURCE_SEARCH spotify chip"`: `--stats` asserts `source_searches=1 spotify_api_calls>=2`, token read back from `sel_secret` (sentinel `secret_gets>=1`) | opus design of the hook signature only if I285a is not done, else sonnet |
| I654d | Desktop UI: Sources page row and login flow (section 1), Search page `Spotify` chip, I652a playlist search scope, Song Information `Source: Spotify` + link, pairing panel, failure texts. Spotify reference design applies. | `apps/` rich UI views (Sources, Search, Info), text UI `/search sp:` prefix | `dui-spotify-search` (SEL_DUI_REAL): Sources page Log In -> fixture -> row shows display name; Search chip, query, Enter, right-click row 2, Add to Playlist; `--stats` asserts `source_searches=1 playlist_adds=1` | sonnet |
| I654e | External-playback mode design (engine section 3 change): engine state, position source, queue advance, what DSP/viz/output show, hand-off semantics; then Soloist backend: supervise, `--pair`, WS client, `play`/`control`/`poll`, exit code 10 handling. | design addendum to `2026-10-22-architecture-design.md` section 3; `src/library/sources/spotify_soloist.c`, `src/engine/engine.c` | `selecta --exec "OPEN spotify:<id>" --exec "SEEK 30000"` with `fake_soloist.py`: `--stats` asserts `spotify_backend=soloist soloist_ws_msgs>=3 external_play=1`, fake log shows seek 30000; second case: fake exits 10 -> `spotify_state=expired` | opus (design + engine), sonnet builds the backend |
| I654f | go-librespot PCM backend: supervise, device-code pairing, REST/WS control, named pipe s16le read into `sel_live` (pipe `audio_output_pipe_wait_for_reader` set), engine plays it as a stream. Only if D-SP-1 keeps it. | `src/library/sources/spotify_librespot.c`, `src/library/stream_open.c` (pipe live source) | `selecta --exec "OPEN spotify:<id>" --render out.wav` with `fake_librespot.py` writing a 440 Hz sine to the pipe: `rendered_frames>0`, `spotify_backend=librespot pipe_bytes>0`, WAV peak at 440 Hz | sonnet |
| I654g | Playlists: `published_lists`/`read_list` for user playlists and Liked Songs, "Import from Spotify" on the Playlists page, pinning; the I653 done-test. | `spotify.c`, Playlists view | I653 test: `selecta --exec` adds `radio:<fixture>` and `spotify:<fixture>` to one playlist, plays both through the real entry point; `--stats` asserts `live_opens>=1 spotify_resolves>=1`; import of a fixture playlist creates N entries | sonnet |
| I654h | Hand-off and Amiga proxy: target-side resolve and `Not available` state; desktop PCM serve endpoint (librespot backend) and Amiga raw-PCM source over bsdsocket; rig check. | remote protocol in `src/protocol/`, Amiga PCM source plugin | host: two `selecta` instances, `DEVICE_TRANSFER` with a `spotify:` row, target asserts `spotify_resolves=1`; Amiga: rig 2 (`--ram 256`) plays a proxied fixture PCM stream, `--stats` `pcm_proxy_bytes>0` | opus design of the PCM proxy protocol, sonnet build |
| I654i | Only if D-SP-4 = spike: cspot desktop gate (1 day) and Tremor LQ timing on the real 68060 and on Emu68/Vampire (I212b method), write go/no-go. No port work in this row. | `thoughts/shared/reports/` timing report | timing table like I212b: RTF for Tremor LQ at 96 and 160 kbps per CPU | sonnet |

Order: I654a, I654b, I654c, I654d, I654e (or I654f first if D-SP-1 picks PCM-first), I654g, then I654f, I654h. I654i
independent and optional.

## 8. Decisions for Johan

- **D-SP-1 Playback backend.** (a) Soloist first, go-librespot as opt-in second backend (Recommended): works and is
  permitted today, but Spotify songs bypass Selecta's DSP, visualizers and Amiga proxy until the second backend lands.
  (b) go-librespot only: full engine integration and Amiga proxy, but breaks the developer terms and is possibly
  broken since 10 Aug 2026. (c) Soloist only: smallest and safest, Spotify songs never get DSP, viz or Amiga playback.
- **D-SP-2 Amiga Spotify tier.** (a) Remote control plus PCM proxy from a desktop Selecta (Recommended): every
  networked Amiga gets it, but needs a desktop running and the librespot backend for audio. (b) Remote control only:
  no third-party client involved, but the Amiga never plays Spotify audio itself. (c) Also fund a native cspot port
  for Vampire/Emu68: standalone, but 8-14 weeks (estimate) on a dormant GPL-3 codebase with an unlicensed dependency.
- **D-SP-3 Web API client ID.** (a) Each user registers their own Spotify developer app and pastes the client ID
  (Recommended): works for everyone under dev-mode limits, but one extra setup step. (b) Ship Selecta's client ID:
  zero setup, but dev mode limits it to an allow-listed few users unless Spotify grants extended quota.
- **D-SP-4 cspot measurement spike (I654i).** (a) Not now; revisit if Vampire/Emu68 users ask (Recommended): saves
  about a week, leaves the native question open. (b) Run the spike now: answers native feasibility, costs rig time
  and a Premium login on an Amiga build.
- **D-SP-5 Spotify playlists.** (a) One-shot import into Selecta playlists (Recommended): simple, offline display
  works, but later Spotify-side edits do not follow. (b) Live two-way sync: always current, but conflict handling and
  write scopes add a row of work and more terms surface.
- **D-SP-6 Podcasts and audiobooks.** (a) Tracks only in v1 (Recommended): one id space, smaller UI. (b) Include
  episodes: wider catalogue, but a second key form and chapter handling.
