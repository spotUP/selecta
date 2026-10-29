---
date: 2026-10-29
topic: Selecta in the browser (I694): Emscripten build, web audio, storage, network through the DEViLBOX server, phone layout, public service
tags: [plan, web, wasm, emscripten, port, phone, m5]
status: plan, not implemented; written by the session (Opus) 2026-10-29; decisions D-WEB-1..4 answered (section 9a)
rows: I694 in thoughts/spot/todos.md; build rows I694a-I694n in section 8
---

# Selecta for the web (I694)

Implements design doc `2026-10-22-architecture-design.md` sections 4 (plugin tags), 9.1 (sources), 12 (`sel_machine`
tiers) for a third target: the browser. Johan's scope (2026-10-29):

- Port: core, library (SQLite), the C replayers (nuade covers Amiga formats), sources, the `sel_gui` desktop UI.
- Not ported: Amiga native UIs (MUI, AGA), 68k native players, remote control and device hand-off.
- Main use: Selecta on Johan's phone.
- Hosting: the DEViLBOX setup (same host, nginx, Docker, deploy pipeline).
- Network sources: the DEViLBOX approach, per-source server routes, no open proxy.
- Spotify: go-librespot on the server, PCM streamed to the browser (risk in section 7).
- Audience: public from the start.
- Phone UI: a new compact single-column layout in the shared uimodel; the Mac also uses it for narrow windows.

## 1. What exists today (measured 2026-10-29)

| Area | Today | Web gap |
|---|---|---|
| Plugins | All linked statically, no `dlopen` | None; the web build lists its plugin set in CMake |
| Threads | pthreads in `src/library/stream_open.c`, `src/library/db/worker.c`, `src/library/db/job.c`, `plugins/ui/desktop/posix/art_thread.c`, `apps/selecta-cli/main.c` | Emscripten pthreads need SharedArrayBuffer, so COOP/COEP headers |
| Network | `include/selecta/net.h` table `sel_net` (get_file, get_range, head_length, pause, request); `src/core/posix/net.c` raw non-blocking sockets + poll; TLS in `src/library/net/tls.c`; FTP in `src/library/net/ftp.c` | No raw sockets in a browser. New `src/core/web/net.c` over `emscripten_fetch`. TLS and FTP move to the server |
| Platform | `src/core/{amiga,linux,macos,posix}`: config_mkdir, dir, machine, pal, watch | New `src/core/web/`: OPFS dir, machine tier, no watch |
| Database | SQLite source `third_party/sqlite` (`selecta_sqlite`) | Compiles to WASM; persist on OPFS (needs a worker, section 4) |
| UI | `plugins/ui/desktop/backend_sdl.c` on SDL3; event loop `SDL_WaitEventTimeout` / `SDL_PollEvent` | SDL3 has an Emscripten port; the blocking loop must become a per-frame callback |
| Audio out | `include/selecta/output.h`: output pulls int16 stereo through `sel_pcm_pull`; CoreAudio calls it from the render callback, no locks, no allocation | Same pull model fits an AudioWorklet; new `plugins/outputs/webaudio` |
| Replayers | dr_flac, gme, kplayer, minimp3, nuade, openmpt, stb_vorbis, ym | All plain C/C++; nuade already builds to WASM in DEViLBOX (`~/Code/DEViLBOX/nuade-wasm`) |
| Sources | asma, hvsc, keygenmusic, sndh, zxart, modland, hippo, local | Remote ones go through server routes; local is picker only (section 5) |
| Integrations | lastfm, mpris, nowplaying | lastfm through the server; mpris/nowplaying replaced by the Media Session API |
| UIs | desktop (`sel_gui`), mui, text | desktop only. Text UI over xterm.js is possible later; not in v1 |

DEViLBOX facts reused here (from `~/Code/DEViLBOX`):

- `vite.config.ts:207-210,269-270`: `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy:
  credentialless` already served, for scsynth pthreads. Selecta copies these headers.
- `server/src/index.ts:94-114`: Express server with per-source routes (`/api/modland`, `/api/hvsc`, `/api/modarchive`,
  `/api/deepsid`, `/api/loudness`, `/api/ratings`, `/api/auth`), `cors`, and `express-rate-limit` per route group. No open
  proxy: each route knows its upstream. Selecta already calls loudness, lyrics, ratings and login there.
- `nginx-host-config.conf`: `devilbox.uprough.net` proxies to the Docker container on port 8080.
- `.github/workflows/deploy.yml`: builds the Docker image, publishes a release, then calls the pull-based deploy webhook
  `https://devilbox.uprough.net/_webhook/hooks/deploy`.

## 2. Architecture in the browser

```
main thread                         worker "engine" (pthread pool)        AudioWorklet thread
-----------                         ------------------------------        -------------------
canvas + input events  --frame-->   Selecta core, library, replayers  --> SAB ring (int16 stereo)
Media Session, <audio> unlock       SQLite on OPFS (sync access)          worklet reads ring, never blocks
file picker / drag-drop             sel_net over emscripten_fetch
```

- **Build**: Emscripten, CMake toolchain file, new preset `web` (output `build/web`). Flags: `-pthread`,
  `-sPROXY_TO_PTHREAD` (main() runs on a worker, so blocking calls in core and SQLite stay legal), `-sOFFSCREENCANVAS_SUPPORT`
  for SDL rendering from that worker, `-sALLOW_MEMORY_GROWTH`, `-sFETCH`, `-sWASMFS` with the OPFS backend.
- **Why PROXY_TO_PTHREAD, not ASYNCIFY**: the core blocks (database worker, stream open, net waits). ASYNCIFY rewrites the
  whole call graph, grows the binary by 30-50 percent and slows every replayer. With PROXY_TO_PTHREAD the existing thread
  code runs as written. The cost is the COOP/COEP requirement, which DEViLBOX already meets.
- **Main loop**: `backend_sdl.c` waits in `SDL_WaitEventTimeout`. On a worker this still blocks the worker, not the page,
  so it works under PROXY_TO_PTHREAD. The spike (I694a) confirms SDL3 input events reach the proxied worker; if they do
  not, the loop body moves into `emscripten_set_main_loop` (one function split, no ASYNCIFY).
- **Audio**: `plugins/outputs/webaudio` implements OUT_OPEN / OUT_SET_SOURCE / OUT_CLOSE. A feeder thread calls
  `sel_pcm_pull` and writes a SharedArrayBuffer ring; the worklet copies from the ring and converts to float. Same contract
  as CoreAudio: the worklet takes no locks and allocates nothing. Rate: the AudioContext rate (48000 on phones).
- **Machine tier**: `src/core/web/machine.c` reports a modern tier (`sel_machine`), with `hardwareConcurrency` and device
  memory as inputs, so the UI picks the rich tier.

## 3. Phone audio (iOS first)

- iOS suspends a Web Audio context when the screen locks or the page goes to the background. Audio from an `<audio>`
  element with a MediaStream source keeps playing. Plan: worklet output goes into a `MediaStreamAudioDestinationNode`,
  that stream plays through a hidden `<audio>` element, and the Media Session API (`navigator.mediaSession`) shows title,
  artist, cover and play/pause/next/previous on the lock screen.
- Start of audio needs a user gesture (tap Play); the first tap unlocks both the AudioContext and the `<audio>` element.
- This behaviour changes between iOS releases. It is proven only on a real phone (I694h), never by a desktop browser.
- Android Chrome keeps the AudioContext alive with an active media session; same code path.

## 4. Storage

- **Library database**: SQLite in WASM on OPFS. OPFS sync access handles exist only in workers, which fits the engine
  worker. One tab owns the database; a second tab opens read-only or shows "Selecta is open in another tab".
- **Settings, theme, cache**: same OPFS root, through `src/core/web/dir.c` and `config_mkdir.c` (WASMFS paths).
- **Downloads cache** (modland, hvsc files): OPFS, bounded by the existing cache size setting. Browsers can evict OPFS
  data under storage pressure; request `navigator.storage.persist()` at first run.
- **Per-user storage (public service)**: the device copy is the primary copy. Server sync of playlists, ratings and play
  history goes through the DEViLBOX login and the existing `/api/playlists` and `/api/ratings` routes. A new server route
  is added only where no route exists (decision D-WEB-4).

## 5. Local files

- Phone: the file picker (`<input type=file multiple>`) and drag-drop on desktop browsers. Picked files are copied into
  OPFS, then indexed like the local source does. No folder scan.
- Chromium desktop: folder scan through the File System Access API (`showDirectoryPicker`), handle kept in IndexedDB,
  rescan on permission grant. Safari and Firefox have no equivalent; the folder button is hidden there.

## 6. Network sources through the server

- `src/core/web/net.c` implements the `sel_net` table over `emscripten_fetch`. `get_range` maps to a `Range` header,
  `head_length` to a HEAD request, `pause` to an abort. Same error codes as `posix/net.c`, so callers do not change.
- The browser never fetches a third-party host directly (CORS, and FTP is impossible). Each remote source gets a server
  route on the DEViLBOX server, following the existing pattern. Routes that already exist: modland, hvsc, modarchive,
  deepsid, loudness, ratings, lyrics, auth. New routes needed (list confirmed by I694c): asma, keygenmusic, sndh, zxart,
  hippo, lastfm scrobble.
- `src/library/net/netpolicy.c` and `mirrors.c` get a web rule: rewrite an upstream URL to its server route. The
  rewrite lives in one function, not at each call site.
- Public-service rules for these routes: a host allowlist per route (no URL parameter can name an arbitrary host), rate
  limits per IP with `express-rate-limit` as the AI and loudness routes already do, a response size cap, and a server-side
  cache for popular files so a public user base does not hammer modland or zxart.

## 7. Spotify on the web

- go-librespot runs on the server; the server streams PCM (or Ogg/Opus to save phone data) to the browser, which plays it
  through the same ring as local audio.
- **Risk, read before building**: in a public service each listener needs their own server-side Spotify session. The
  server then holds user Spotify credentials and streams Spotify audio out of Spotify's own clients. That conflicts with
  the Spotify Developer Terms (no proxying or redistribution of streams) and puts the service at risk of a takedown and
  the user accounts at risk of a ban. librespot playback has also been broken since 10 Aug 2026 (issue #1737); go-librespot
  is the only backend (D-SP-1).
- Plan: Spotify on the web ships only behind decision D-WEB-2. The safe default is "Johan's own account only, not offered
  to public users".

## 8. Build rows

Each row has a finish line; done means its reachability test drives the web build's entry point.

| ID | Row | Done when |
|---|---|---|
| I694a | Emscripten spike: `web` CMake preset with toolchain, build core + nuade + sel_gui desktop UI, list every compile and link failure in `thoughts/shared/research/2026-10-29-web-spike.md` | Failure list written; the empty app window opens in Chrome over a local COOP/COEP server |
| I694b | `src/core/web/`: dir, config_mkdir, machine, pal stubs, no watch; WASMFS + OPFS mount | Settings file survives a page reload (test reads it back) |
| I694c | `src/core/web/net.c` over emscripten_fetch, same `sel_net` contract; host test suite for `sel_net` runs against it under node | net tests pass on the web backend; list of server routes needed |
| I694d | `plugins/outputs/webaudio`: SAB ring + AudioWorklet, feeder thread calls `sel_pcm_pull` | A mod plays in desktop Chrome from a bundled file; callback counter in stats moves |
| I694e | SQLite on OPFS, single-owner lock, second-tab message | Library survives reload; second tab shows the message |
| I694f | Local files: picker, drag-drop, Chromium folder scan | Picked file appears in the library and plays |
| I694g | Server routes on the DEViLBOX server for the remaining sources, allowlist, rate limits, cache | Each source browses and plays from the web build |
| I694h | Phone audio: MediaStream to `<audio>`, Media Session metadata and buttons | Johan, iPhone: music keeps playing with the screen locked; lock screen shows title and buttons |
| I694i | Compact single-column layout in the uimodel: bottom player bar, full-screen Now Playing, 44 px touch targets, touch scroll with momentum; Mac narrow windows use it too | Layout tests at 390x844 and a narrow Mac window; Johan phone look |
| I694j | Touch input in `backend_sdl.c`: SDL finger events to the same actions as mouse, long press for the context menu | Tap, scroll, long press work on the phone |
| I694k | Deploy: Docker stage builds the WASM, nginx serves it with COOP/COEP under the DEViLBOX host, deploy through the existing release + webhook | Live URL loads on the phone |
| I694l | Licence: UADE and nuade are GPL; the public build ships a source offer (link to the exact source commit) and lists every third-party licence in an About page | About page lists all licences; source link resolves |
| I694m | PWA: manifest, icon, service worker cache of the WASM, "Add to Home Screen" | App opens from the home screen without the browser bar |
| I694n | Spotify on the web as a Connect remote (D-WEB-2 final): Web API device control and playlist id sync, no audio path | After I694a-m |

Order: I694a first (its failure list may add rows). Then I694b, I694c, I694d in parallel tracks; I694e after I694b; I694f,
I694g after I694c; I694h and I694j after I694d; I694i can start at once (uimodel work, Mac-testable); I694k, I694l, I694m
before the first public URL.

## 9. Open decisions (D-WEB)

- D-WEB-1: URL of the web app (path under the DEViLBOX host, or its own subdomain).
- D-WEB-2: Spotify on the public web service (Johan only, or not at all, or for every user).
- D-WEB-3: Where the web build sits among the tracks (third agent track, or inside the Mac track).
- D-WEB-4: Server sync for public users in v1 (device only, or sync through the DEViLBOX login).
- D-WEB-5: Text UI in the browser (later, or v1).
- D-WEB-6: Minimum browsers (current Safari iOS and Chrome; Firefox best effort).

Answers are written into section 9a when Johan gives them.

## 9a. Johan's answers (2026-10-29)

- D-WEB-1: own subdomain on the DEViLBOX host and nginx (for example `selecta.uprough.net`), so the app has its own
  origin, its own OPFS storage and its own COOP/COEP headers.
- D-WEB-2: first answer "no Spotify on the web"; Johan then asked for proper licensing research before deferring it.
- D-WEB-2 (final, 2026-10-29, after research thoughts/shared/research/2026-10-29-spotify-licensing.md): Spotify Connect remote only. The web build sends play, pause, seek and queue commands through the Web API to the user's own Spotify device and syncs playlist ids; no Spotify audio passes through Selecta (no Web Playback SDK, no server-side librespot). Consequences: no crossfade, DSP or visualizer on Spotify rows; Spotify rows in mixed playlists hand off to the Connect device; attribution, link back and cover art per Developer Policy II.4/II.5; stored rows keep ids and minimal display fields only (Terms IV.3).
  I694n waits for I694o (thoughts/shared/research/2026-10-29-spotify-licensing.md); the decision is asked again with the findings.
- D-WEB-3: web rows run inside the Mac track, queued after the current Mac rows. No third agent track.
- D-WEB-4: public user data syncs through the DEViLBOX login (playlists, ratings, history), using existing routes where
  they exist.
- D-WEB-5 (text UI) and D-WEB-6 (browsers): not asked; the defaults stand: text UI later, current Safari iOS and Chrome,
  Firefox best effort.
