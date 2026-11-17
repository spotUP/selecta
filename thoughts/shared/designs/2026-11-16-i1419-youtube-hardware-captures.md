---
date: 2026-11-16
row: I1419
status: design, open decisions for Johan
depends: I1303 (Demozoo lookup), I1384b (slide show, Watch demo), I1384e (web view in the cover slot)
---

# YouTube hardware captures in the slide show (I1419)

Johan 2026-11-16: "a LOT of music is available capped from hardware on youtube we should add that to screenshot/video slideshow" and "the user should be able to switch between playing the song directly or play the youtube video".

Already decided by Johan (ledger row, 2026-11-16, not re-opened here):

- Embedded player in the slide show (web view in the SDL app); replayer audio stops while the video plays.
- Matching: Demozoo/Pouet/CSDb YouTube links first, YouTube search fallback (artist + title + platform, strict score, user confirms).
- Amiga hides the feature (RULES 4a exemption, below).

Done when (ledger): a known song with a hardware capture shows the matching video; no wrong-song match in a sample of 20.

## 1. What exists and is reused

Nothing below is built fresh where a working piece exists. Searched: `src/` and `plugins/` for "Watch demo", "slideshow", "dz_video", "youtube", "webview"; hippoplayer.se `cygnus-server/src/server` for "youtube", "demozoo"; `thoughts/shared/api/` there.

| Piece | Where | Use here |
|---|---|---|
| Server YouTube finder | hippoplayer.se `cygnus-server/src/server/YouTubeRoutes.ts` (`GET /api/youtube/for-tune?title=&author=&format=&length=`), scorer `youtubeMatch.ts` (`MATCH_THRESHOLD = 5`), cache `db/youtubeCacheDb` | The lookup. Already tries DeepSID mirror links (about 2,000 SID files, exact), then a curated corpus of channels (`youtube.premiumSources`), then the cache, then one YouTube search, inside a daily budget (`DEFAULT_DAILY_BUDGET = 90` searches). API key in server config `youtube.apiKey`. |
| Demozoo lookup | hippoplayer.se `GET /api/demozoo/lookup` (contract `thoughts/shared/api/demozoo-lookup-v1.md`), client `sel_library_demozoo_kick` (`src/library/db/db.h:618`, `art_scrape.c`) | Source of the Demozoo link and the `platform=` family tokens (`amiga`, `c64`, `atari`, ...). |
| Per-song cache rows | `artwork(album_id, kind, data)` under `sel_art_dz_id(key)`; kinds `cover`, `dz1..dz7`, `dz_video`, `dz_miss`; `art_kinds()` in `src/library/db/art.c:232` is the one place that says which kinds make the slide show | Candidate video and miss rows. |
| Per-song user data | `stickers(key, name, value)`, op in `src/library/db/user.c:726` | The user's confirmed video and the audio-source switch. |
| Watch demo button | `plugins/ui/desktop/desktop_ui.c:13466` (draw), `:11803` (click, `desk_open_url`) | The new button sits beside it, same drawing code. |
| Native view over the SDL window | `plugins/ui/desktop/macos/route_picker.m` (AVRoutePickerView added to the SDL window's content view, placed in window points) | Pattern for the WKWebView. I1384e (muted Demozoo video in the cover slot) needs the same view; one web view serves both. |
| Text UI video line | `plugins/ui/text/text_ui.c:2554` (reads `dz_video`, prints the link) | Same for the capture link. |

Gap found in the reuse: the scorer was written for "any recording", not "hardware capture". `KNOWN_CHANNELS` gives a bonus to `remix64` and `amigaremix`, which publish remixes, the opposite of a capture. `buildSearchQuery` appends `amiga` to everything that is not SID, so Atari songs are searched as Amiga. Both are fixed by a `kind=capture` mode (section 3), not a second scorer.

## 2. Sources and match confidence

In order; the first hit wins. Each answer carries `source` and a confidence tier.

| Order | Source | Tier | Cost |
|---|---|---|---|
| 1 | Demozoo music entry's own YouTube link (in Demozoo a music entry is a production; its external links sit in the same link table the nightly job already reads for productions) | exact | free, nightly dump |
| 2 | DeepSID mirror link (SID only; already in `/for-tune`) | exact | free |
| 3 | CSDb release YouTube link (SID only) | exact | free if dumped server-side; not built (decision D4) |
| 4 | Curated corpus: playlists and channels that publish single tunes captured from hardware (`youtube.premiumSources`) | trusted, scored | free after ingest (1 quota unit per 50 videos) |
| 5 | Server cache of an earlier search | as stored | free |
| 6 | YouTube search (`search.list`, 100 quota units of the default 10,000 per day) | scored | the scarce one |

The Demozoo production `video_url` (today's Watch demo) is a demo video, not a capture of the song: the song may start anywhere and other music may play. It stays Watch demo and is never offered as the audio source.

Pouet: production links are demos, same problem as above. Not a source in v1 (decision D4).

Confidence:

- **exact** (1 to 3): stored as the song's video with no question.
- **scored** (4, 6): only a candidate with score at or above the threshold is returned. In `kind=capture` mode the scorer adds: the machine is named (`amiga`, `a500`, `a1200`, `c64`, `6581`, `8580`, `atari`, `st`, `ste`, `ym2149`) +2; "real hardware", "hardware", "real amiga", "original" +2; "remix", "cover", "orchestral", "piano", "remake", "reimagined", "arrangement", "live" -6 (an outright reject in practice); remix channels moved out of the bonus list; length within 15 % of the song's known length +2 (exists). Threshold stays 5 for corpus hits and rises to 7 for search hits. A scored hit is shown as "Suggested video" and needs the user's Yes before the switch can use it (Johan: "user confirms").
- **miss**: no candidate. Cached by the server (exists) and on the client as `yt_miss` with the epoch, retried after `SEL_ART_SCRAPE_RETRY_DAYS` (30), like `dz_miss`.

The 20-song sample of the done-criterion is drawn from songs with a scored (not exact) answer, since exact answers cannot be wrong-song by construction; 0 wrong songs among confirmed-or-shown suggestions is the bar.

## 3. Where the lookup runs: hippoplayer.se

Server-side, extending `/api/youtube/for-tune`; never from the client to Google.

- The YouTube Data API key stays in the server config; no key ships in the app (same rule as D-DZ-1 for Demozoo).
- Quota is one pool for every user. One search answers every later user of the same song through the server cache, which is the only way 90 searches a day go far.
- Request (additive to the existing endpoint, contract to be written as `thoughts/shared/api/youtube-for-tune-v1.md` in hippoplayer.se, shape like `demozoo-lookup-v1.md`):
  `GET /api/youtube/for-tune?title=&author=&format=&length=&platform=<family>&kind=capture[&search=1]`
  - `platform` uses the Demozoo family tokens (`sel_dz_family`, `demozoo/demozooPlatforms.ts`): picks the machine word for the query (`amiga`, `c64 sid`, `atari st`) and the scorer's machine words.
  - `search=1` allows step 6. Without it the server answers from steps 1 to 5 only (free). See decision D1.
  - Answer adds `tier` (`exact` / `scored`), `embeddable` (from `videos.list part=status`, same 1-unit call that already fetches durations), and keeps `found`, `source`, `videoId`, `title`, `channel`, `score`, `reasons`.
  - A non-embeddable video is treated as a miss for the switch (it cannot play in the web view) but its link is still shown for the browser.
- Request carries title, author, format, length, platform: no personal data, no song path.
- nginx: `nginx/hippoplayer.se.conf` has `location ^~ /api/demozoo/` but no `/api/youtube/` location; the route is mounted at `/api/youtube` in `YouTubeRoutes.ts:451`. Add the location block (unverified whether another location already proxies it; check on deploy).
- Rate limit like `demozooLimiter`.

Client: the lookup runs on the library job thread right after the Demozoo lookup of the same song (`sel_library_demozoo_kick` path), same User-Agent, same `SELECTA_ART_SCRAPE=off` and a new pref `art.youtube` (default on) that must move output (reachability rule).

## 4. Storage: no migration

Two existing tables, split by what the data is:

| Data | Table | Row |
|---|---|---|
| Server answer (cache, may be wiped and refetched) | `artwork` under `sel_art_dz_id(key)` | `yt_video` = `<videoId>\t<tier>\t<source>\t<score>\t<embeddable>\t<video title>`; `yt_miss` = epoch seconds |
| User's verdict (user data, never wiped by a cache clear) | `stickers` keyed by the canonical song key (`song_key.h`) | `video.yt` = confirmed videoId, or `none` when the user said "Not this video"; `video.audio` = `youtube` or absent (absent = replayer) |

`art_kinds()` gains one bit next to `SEL_ART_KIND_VIDEO` (say `SEL_ART_KIND_CAPTURE`) for a `yt_video` row, so the UIs keep asking one place. The kinds are TEXT and both tables exist, so the schema version does not move and `SCHEMA_FINAL` is untouched. If Johan prefers a dedicated table (decision D6), it is `SCHEMA_V23` appended to `g_migrations` in `src/library/db/schema.c` and mirrored in `SCHEMA_FINAL` (ctest `library-schema-final` guards the pair).

Rule: the song "has a capture" when `video.yt` holds an id, or when `yt_video` is tier `exact` and `video.yt` is not `none`. A scored `yt_video` with no `video.yt` is only a suggestion.

## 5. Mac UI

Now Playing, under the title block where Watch demo sits (`desktop_ui.c:13466`):

- **Hardware video** button beside Watch demo when the song has a capture. Full words, accent style like Watch demo.
- **Suggested video** button instead when only a scored candidate exists. Click shows the embed (paused, title and channel under it) with two buttons: **Use This Video** (writes `video.yt`) and **Not This Video** (writes `video.yt = none`, hides the suggestion for this song).
- **Play from** switch, a two-segment control: **Replayer** / **YouTube Video**. Shown only when the song has a confirmed, embeddable capture. Writes `video.audio` for this song only.

Slide show: when the switch is on YouTube Video, the web view takes the cover slot (the slide show stills stop); otherwise the slide show runs as today and, with I1384e, may show the muted Demozoo video. Both use the one web view; the capture always wins over the muted demo video.

Both sources could play: they never play together.

- Switch to YouTube Video: the engine pauses the replayer at its position (pause, not stop, so the queue and position survive), the embed loads and plays unmuted.
- Switch back to Replayer: the embed pauses, the engine resumes.
- Transport (play/pause, seek bar, volume) drives whichever source is active. For the embed through the IFrame Player API (`playVideo`, `pauseVideo`, `seekTo`, `setVolume`) called with `evaluateJavaScript`; the embed's `onStateChange` reports time and end back.
- Video ended: the queue advances as at the end of a song. The next song uses its own `video.audio` (per-song switch, no global mode).
- Offline, embed error, or video removed: a line "The video could not be played" and the switch falls back to Replayer for this play (sticker kept).
- Seek position when switching: decision D3.

Web view (Mac first): WKWebView added to the SDL window's content view and placed in window points over the cover slot, the `route_picker.m` pattern; new files under `plugins/ui/desktop/macos/`, plain C interface for `backend_sdl.c`. It loads a small local HTML page holding the IFrame Player API with `youtube-nocookie.com`. The page is loaded with an https base URL (`https://hippoplayer.se/`) so the embed gets an origin and referrer: embeds without one have been refused by YouTube (player configuration error); unverified for WKWebView, the first build of I1384e or this row measures it. `mediaTypesRequiringUserActionForPlayback = none` so a user's switch starts sound without a second click. Linux (WebKitGTK) and Windows (WebView2): later rows; until then those builds show Hardware video as a browser link (`desk_open_url`) and hide the switch.

## 6. Text UI and Amiga (RULES 4a)

- **Text UI**: no video playback (a terminal has no video path). Shows the link as text under the stills, the `text_ui.c:2554` pattern: `Hardware video: https://youtu.be/<id>`. Suggestions are not shown (no way to judge without seeing the video). Switch exempt. Exemption text for the row: "text UI: no video output; the confirmed capture is shown as a link".
- **Amiga MUI / AGA / RTG**: hidden (Johan's choice). Exemption for the row: "Amiga cannot play YouTube: no browser able to run the YouTube player on AmigaOS 3, no VP9/H.264 decode at playable speed on 68020 to 68060, and the API terms allow playback only through YouTube's own embedded player. The Amiga UI shows nothing for this feature; the slide show and Watch demo stills run as today." No lookup runs on the Amiga (saves the request).

## 7. Legal and terms

- **No downloading, no audio extraction, no stream caching.** YouTube's Terms of Service forbid downloading content unless YouTube shows a download link; the API Developer Policies forbid separating, isolating or modifying the audio or video of a video. No yt-dlp, no ffmpeg, no "audio only" mode, no playing the capture with a hidden player.
- **Playback only through the official embedded player**, visible and not covered (the policies name a minimum size, 200 x 200 pixels; the cover slot is larger). Our own controls may drive it through the IFrame Player API. YouTube's branding and its link to the video stay as the embed draws them.
- **Stored data**: the app stores the video id, title, channel and our score. The policies limit how long API data may be kept without refreshing (unverified: the 30-day rule and whether ids are exempt; check the current Developer Policies before I1419b). If ids must be refreshed, the server re-checks each stored id once a month with `videos.list` (1 unit each) and drops removed or non-embeddable videos; the server cache today keeps answers forever.
- **Risk carried into I1384e (not this row)**: a muted demo video with the replayer's audio over it can read as modifying the audio of the video. Name it in I1384e before that row builds (unverified interpretation).
- Demozoo/CSDb/DeepSID links are public data used as on Demozoo today (D-DZ-1); nothing is mirrored.

## 8. Rows this splits into

- I1419a (server, hippoplayer.se): `kind=capture` scorer mode, `platform=`, `search=`, `tier`, `embeddable`, Demozoo music-entry links in the nightly job, nginx location, contract doc, tests in `youtubeMatch.test.ts`.
- I1419b (library): client lookup on the job thread, `yt_video`/`yt_miss` rows, stickers, `art_kinds` bit, pref `art.youtube`, ctest.
- I1419c (Mac): web view (shared with I1384e), buttons, switch, transport routing, end-of-video advance.
- I1419d (text UI): link line.
- I1419e (Amiga): exemption written in the row, nothing built.
- I1419f: 20-song wrong-match sample, done-criterion.

## 9. Decisions

Johan's answers, 2026-11-16: D2 auto-accept scored hits at threshold + 3 (not the recommendation: a wrong song can play unasked, so Not This Video must be one click from the switch). D3 same position, best effort. D5 replayer by default. D4, D6, D7 take the recommended defaults (not asked). D1 search on button only (Find Hardware Video, when no free answer exists), chosen after the quota explanation. Johan first asked why there is a budget. The YouTube Data API v3 gives each key 10,000 quota units a day and search.list costs 100 units, so about 100 searches a day; the server keeps 90 to leave room for videos.list checks.


Each with the recommended option first.

- **D1 When YouTube search runs.** (Recommended) Only when the user presses "Find Hardware Video" on a song with no free answer: the 90 searches a day go to songs people care about. Alternative: automatic on play; finds more without a click, but the budget is spent in minutes by everyone's shuffle and most searches are never looked at.
- **D2 Which answers need a Yes.** (Recommended) Exact links (Demozoo music entry, DeepSID, CSDb) are used directly; every scored hit (corpus or search) needs Use This Video. Alternative: auto-accept scored hits at threshold + 3; fewer clicks, but a wrong song plays unasked.
- **D3 Position when switching mid-song.** (Recommended) Start the other source at the same time position, best effort (captures often have a second or two of lead-in, so it lands close, not exact). Alternative: always start from 0; exact, but switching halfway restarts the song.
- **D4 Sources in v1.** (Recommended) Demozoo music-entry links, DeepSID, curated corpus, search; CSDb and Pouet later rows. CSDb adds exact SID links beyond DeepSID's 2,000 but needs its own dump job; Pouet links are demos, not song captures. Alternative: CSDb in v1; more exact SID hits, one more server job now.
- **D5 Default for a song with a confirmed capture.** (Recommended) Replayer, the switch is opt-in per song: the app stays a player, and nothing streams video unasked. Alternative: YouTube Video by default when a capture exists; shows the feature, but costs bandwidth and moves away from the replayer as the core.
- **D6 Storage.** (Recommended) Existing `artwork` + `stickers` rows, no migration. Alternative: a dedicated `song_videos` table (SCHEMA_V23); clearer queries, but a migration and a second place for per-song data.
- **D7 "Not This Video" reach.** (Recommended) Local only in v1 (sticker `none`). Alternative: also report to hippoplayer.se so a wrong cached match is dropped for everyone; better shared quality, but needs an endpoint and abuse protection.
