---
date: 2026-11-09
row: I1003a
status: phase 1 implemented (library.db only, no tag embedding)
---

# Artwork scraper, phase 1 (I1003a)

Goal: an album with no cover gets one from the internet, in the background, and the existing draw path shows it. Phase 1 stores
the image in library.db. Phase 2 (I1003b) adds embedding into files, the Amiga TLS path in practice and the UI toggle.

## Sources and terms of use

1. MusicBrainz web service `GET /ws/2/release/?query=release:"<album>" AND artist:"<artist>"&limit=1&fmt=json`. Free for
   non-commercial and open software; needs an identifying User-Agent and at most 1 request per second per client.
2. Cover Art Archive `GET /release/<mbid>/front-500` (redirects to archive.org storage; the HTTP client follows up to 5 redirects).
   No key, no stated rate limit; we send one request per album.
3. iTunes Search API `GET /search?term=<artist album>&entity=album&limit=1` as the fallback. Public, no key, loosely limited
   (about 20 requests per minute per IP); we send at most one search and one image request per album and only when
   MusicBrainz/CAA gave nothing. The 100x100 artwork URL is rewritten to 600x600.

User-Agent on every request: `Selecta/<version> ( https://hippoplayer.se )`. No email address.
Images are stored for the user's own library display only; nothing is redistributed.

## Matching

Only albums with a non-empty name AND artist (from the tags) are looked up. The query is exact on both fields. MusicBrainz hit
needs `score >= 90` and a 36-character release id. The iTunes top hit must contain the album name (case-insensitive); a wrong
cover is worse than none. The image must start with a PNG or JPEG signature and is capped at 1 MiB.

## Cache and misses

- A found cover is a normal `artwork(album_id, 'cover', data)` row, the same row the embedded-cover reader writes, so the grid,
  Now Playing and the Amiga draw path need no change. Albums that already have a `cover` row are never looked up.
- A definite miss (no release, low score, no iTunes match, not an image) is the row `artwork(album_id, 'scrape_miss', <epoch
  seconds as ASCII>)`. No schema migration. Retried after `SEL_ART_SCRAPE_RETRY_DAYS` (30).
- A network failure (no connection, HTTP 429, 5xx, TLS unavailable) records nothing and ends the pass; the next kick (a scan or the
  next start) tries again.
- The embedded cover reader overwrites a scraped cover on rescan only when the file has its own cover (existing behaviour of the
  `cover` row).

## Thread

- With threads: the library job thread (`job.c`), coalesced like the md5 hash job, one album per turn under `job_lock`, yielding
  to the fetch job. Database access goes through `sel_library_request(lib, SEL_LIB_REQ_ART, ...)`, the same single-writer path as
  the scan. Never the UI thread.
- MusicBrainz pacing: `scrape_mb_ms` keeps at least 1000 ms between MusicBrainz requests (`sel_net_now_ms`, `sel_pal_sleep_ms`).
- Without threads (`SEL_LIBRARY_NO_THREAD`, Amiga): `sel_library_scrape_poll` does one album per call from the engine idle tick,
  next to `sel_library_hash_poll`. Known cost: the HTTP call blocks that tick. See open items.

## Amiga TLS path

MusicBrainz, CAA and iTunes are https-only. `sel_http_request` returns `SEL_HTTP_E_TLS_UNAVAILABLE` where no TLS is built in; the
scraper treats that as a network failure (no miss recorded, pass ends), so the Amiga build is inert until I899g (AmiSSL) lands.
The code path is the same portable C on both platforms; there is no Amiga-only branch.

## Config knobs

- Pref `library.scrape_artwork` (`on` default, `off`): applied at engine start and on every PREF_SET.
- Environment `SELECTA_ART_SCRAPE=off` (or `0`) turns it off regardless of the pref.
- Base URLs for tests: `SELECTA_ART_MB_URL`, `SELECTA_ART_CAA_URL`, `SELECTA_ART_ITUNES_URL`.
- `selecta-cli library scan` never scrapes unless `--scrape` is given (it sets `SELECTA_ART_SCRAPE=off` and disables the library
  flag otherwise), so no other CLI test run can reach a real server.

## Stats

`sel_library_scrape_stats`: `albums` (looked up), `scraped` (artwork_scraped), `misses`, `errors`, `requests` (the sentinel: HTTP
requests sent). `selecta-cli library scan --scrape` prints `artwork_scraped= scrape_albums= scrape_misses= scrape_errors=
scrape_requests=`.

## Test

`tests/net/art_scrape.cmake` (ctest `art-scrape`): scan of three tagged mp3 files without a cover against the fixture server
(`musicbrainz.test`, `coverartarchive.test`, `itunes.test`). Proves: artwork rows for the MusicBrainz and the iTunes album,
`scrape_miss` row for the third, `artwork_scraped=2`, request counts per host in the fixture log (the sentinel), a second scan
sends nothing, and `SELECTA_ART_SCRAPE=off` stores nothing and sends nothing (revert proof).

## Open items for I1003b

Embed the cover into the music file; AmiSSL on Amiga (I899g) and a real Amiga check; a UI toggle for `library.scrape_artwork`;
the no-thread poll blocks the idle tick during an HTTP call (needs a split request or a helper task); iTunes pacing beyond one
album per turn; showing scrape progress.

## Demozoo slide show (I1303)

Research: `thoughts/shared/research/2026-11-10-demozoo-api.md`. Decisions by Johan, 2026-11-10, through the question tool:

- **D-DZ-1 Lookup route: nightly dump processed on hippoplayer.se.** The Demozoo API has no search endpoint and no "featured in" field, and needs an account key since 2026-11-01. A server job reads `https://data.demozoo.org/demozoo-export.sql.gz` (about 203 MB) and builds a small song-to-production table. Selecta asks hippoplayer.se, never Demozoo, for the mapping. No key in the app, no per-song Demozoo request.
- **D-DZ-2 Images: fetched from media.demozoo.org by the app, cached locally.** Nothing is mirrored or redistributed by hippoplayer.se. Demozoo states no licence or hotlink terms; this is the accepted risk. Same cache, miss handling, size cap and User-Agent as the phase 1 scraper above.
- **D-DZ-3 Video: stills slide show plus a "Watch demo" link.** Demozoo videos are YouTube links, not files. The screenshots cycle as the cover; a button opens the link in the system browser. No embedded playback. Amiga and text UIs: stills only (text UI: the link as text).

### Server table (hippoplayer.se)

One row per music entry that has at least one production with a screenshot:

| Field | Content |
|---|---|
| `match_key` | Normalised Modland path from the entry's download link (`pub/modules/...`, percent-decoded, lower case), or empty |
| `title`, `author` | Fallback match, both lower case |
| `music_id`, `production_id`, `production_title`, `production_type`, `year` | Demozoo identifiers and display fields |
| `screenshots` | List of `original_url` with width and height, in Demozoo order |
| `video_url` | First YouTube external link, or empty |

Match order: exact Modland path, then exact title plus author. No fuzzy match in v1. Several productions for one song: the earliest release first. The same table serves the "Used in" list of I1301.

Endpoint shape follows the existing hippoplayer.se contract (`~/Code/hippoplayer.se`, read before I1303b): one request per song key, small JSON answer, cacheable, answer for "no match" is cached as a miss.

### Client

- Lookup runs on the scraper thread when a song without album art starts. Request carries the source key only; no personal data.
- Result and images go into the art cache; a miss is stored with the phase 1 retry rule.
- Slide show: Now Playing cover slot cycles the screenshots, one every 6 s with the existing crossfade; order as served; one screenshot means a still. Pauses with playback.
- Knobs: `art.demozoo` on/off (default on), `art.slideshow_seconds` (default 6). Both must move output (reachability).
- Images are served at device-pixel size (I1302 lands first or together).
- Amiga: same lookup over the Amiga TLS path above; images scaled to the cover slot; exempt from crossfade if the tier cannot afford it (row names it).

### Unverified

Live JSON field names of the dump tables, how many Modland songs match, and whether media.demozoo.org refuses hotlinked requests. I1303a measures all three before the server job is written.
