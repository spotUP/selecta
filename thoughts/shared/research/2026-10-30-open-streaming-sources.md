---
date: 2026-10-30
author: research agent
status: draft
row: I725
---

# Open streaming sources without DRM (I725, I725a)

Probes marked "verified" were run with curl from the Mac on 2026-10-30. Terms were read from the vendor pages
named per source; anything not read is marked UNVERIFIED. Not legal advice: licences differ per track.

## What Selecta already has (reuse)

- Source plugins `plugins/sources/`: asma, hvsc, keygenmusic, sndh, zxart. All are file-archive catalogues
  (download + sync + `*_net.c`). No radio, podcast or API-paging stream source exists.
- `include/selecta/net.h` `sel_net`: `get_file`, `get_range`, `head_length`, `pause`, `request` (one GET into a buffer,
  made for API paging), `get_file_checked`. `request` is the entry point for the JSON APIs below.
- HTTP client `src/library/net/http.c` (+ `tls.c`, `ftp.c`, `mirrors.c`, `netpolicy.c`). It sends a fixed
  `User-Agent: selecta` (http.c:364). radio-browser and Podcast Index refuse generic UAs and the Archive wants a
  descriptive one: needs a per-source UA parameter (small prerequisite row).
- Live streams `src/library/stream_open.{h,c}`: `sel_live_open` (download thread, bounded ring, endless radio), ICY
  in-band metadata parser already present (`Icy-MetaData: 1`, `StreamTitle=` parse, I196a). Desktop only: the Amiga build
  has no download thread until I199 (header comment), so live radio on the Amiga waits for I199.
- Decoders `plugins/replayers/`: minimp3 (MP3), stb_vorbis (Ogg Vorbis), dr_flac (FLAC), openmpt, gme, ym, nuade,
  kplayer. No AAC, no Opus. MP3 and Ogg work on both platforms; AAC/Opus streams are Mac-only until a decoder row.
- Amiga TLS: `tls.c` exists in the shared client; whether the Amiga build links it was not checked (no TLS match in
  `src/core/amiga/net.c`). Treat Amiga HTTPS as unproven; every source is rated for plain-HTTP availability.
- Amiga MP3 decode load per bitrate is I721's result; not re-measured here.

## Ranked table

| # | Source | Catalogue | Auth | Third-party player OK | Codec | Plain HTTP | Amiga |
|---|--------|-----------|------|-----------------------|-------|------------|-------|
| 1 | I725a generic stream (Icecast/Shoutcast URL) | any | none | n/a, user picks URL | MP3/Ogg/AAC | per station | MP3/Ogg after I199 |
| 2 | radio-browser.info | 62,253 stations (verified) | none, UA | yes, free | station's own | yes (verified) | MP3 stations |
| 3 | Internet Archive audio | 14.2 M audio items, 1.98 M with CC licence URL (verified) | none | yes, UA + 429 | MP3 VBR, Ogg, FLAC | yes (verified) | MP3/Ogg derivative |
| 4 | SomaFM | 51 channels (verified) | none | FORBIDS third-party clients (ToS 2026-09-04, verified) | MP3 128, AAC 128/64/32 | yes (verified) | MP3 128 |
| 5 | Demoscene radio (Nectarine, SceneSat) | 2+ stations | none | public streams | MP3 192, AAC 48, Opus | yes (verified) | MP3 192 heavy |
| 6 | Jamendo | vendor claim 600k+ tracks (UNVERIFIED) | client_id | yes, non-commercial only, no caching | MP3 96, VBR, Ogg, FLAC | timed out (UNVERIFIED) | mp31 96k if HTTP |
| 7 | Podcast RSS + Podcast Index | millions of feeds | free key+secret, UA | yes (open RSS) | MP3 mostly | per host | MP3 if not https |
| 8 | ccMixter | tens of thousands of remixes | none | no player clause found | MP3 | yes (verified) | yes |
| 9 | Audius | large, user uploads | app key | terms UNVERIFIED | unclear | https only | no |
| 10 | Free Music Archive | rebuilt 2026 | none found | old API 404 (verified) | - | - | blocked |
| 11 | SoundCloud | huge | app registration | NO (forbids webcast radio) | - | - | skip |
| 12 | Bandcamp | huge | no public API | NO (personal use only) | - | - | skip |

Musopen: `musopen.org/api/` returned 404, no public API found (UNVERIFIED beyond that probe).

## Per-source sections

### I725a Generic stream source
- Auth none. User or preset list supplies URL; `.pls`/`.m3u` resolve to a URL (SomaFM and Nectarine publish `.pls`).
- Terms: user chooses the stream; names no service.
- Codec: MP3 + Ogg Vorbis both platforms; AAC/Opus Mac-only until decoders.
- Reuse `sel_live` and its ICY parser; add saved list, playlist resolver, Amiga read path (I199).
- Snapcast client: separate TCP protocol (JSON-RPC + PCM/FLAC chunks), desktop-only first; not researched beyond existence.

### radio-browser.info
- `https://de1.api.radio-browser.info/json/...`; mirrors via DNS `all.api.radio-browser.info`. Free, no key. Verified
  `/json/stats`: 62,253 stations, 7,012 broken. Generic UA is refused, verified: "You must set a proper User-Agent
  string that identifies your application." Server AGPL-3.0 (secondary source).
- Terms: official ToS not retrieved (UNVERIFIED). Secondary: vote throttle 1 per station per IP per 10 min, click count
  once per station per IP per day.
- Endpoints: `/json/stations/search?name=&tag=&country=&codec=&hidebroken=true&order=votes&reverse=true&limit=`,
  `/json/stations/byname/{n}`, `/json/tags`, `/json/countries`, `/json/codec`. Fields `url_resolved`, `codec`, `bitrate`.
- Plain HTTP API answers 200 (verified). Filter `codec=MP3` for Amiga.

### Internet Archive audio
- `https://archive.org/advancedsearch.php?q=mediatype:audio&fl[]=identifier&rows=&page=&output=json`;
  `https://archive.org/metadata/{id}`; `https://archive.org/download/{id}/{file}`. No key.
  Verified counts: audio 14,208,759; with CC licenseurl 1,980,586; etree 296,552; 78rpm 305,889; netlabels 80,547.
- Terms: https://archive.org/services/docs/api/bots.html: "Honor `429 Too Many Requests` responses and `Retry-After`
  headers."; each automated request needs a descriptive User-Agent. No player ban found. The ToS page URL I tried was 404,
  so the ToS text itself is UNVERIFIED.
- Items carry individual rights (public domain, CC, uploads in copyright). Default filter: netlabels, CC licence, 78rpm.
- Codec: MP3 VBR and Ogg Vorbis derivatives, FLAC/WAV originals. Plain HTTP answers 200 (verified); download CDN
  redirect to https UNVERIFIED. Tracker modules also live in collections (same source can list them).

### SomaFM
- OUT (I725b dropped 2026-10-30): ToS https://somafm.com/contact/tos.html (updated 2026-09-04): "We can't grant permission for third-party SomaFM clients or applications, even noncommercial ones." Logos, descriptions and playlists are copyrighted; re-posting now-playing info is against their terms. A user may still paste a SomaFM .pls URL into the generic stream list (I725a).
- `https://somafm.com/channels.json` verified: 51 channels, each with `playlists[]` (`.pls` url, format mp3/aac/aacp,
  quality). Plain HTTP works (verified); Icecast KH `http://ice1.somafm.com/groovesalad-128-mp3` returns `audio/mpeg`.
- Terms: no third-party clause found. FAQ https://somafm.com/about/faq.html lists 128k MP3, 128k AAC, 64 and 32 kbit
  AAC-HE. Listener-supported: show a donate link; ask SomaFM before shipping a built-in list.
- Default MP3 128 (both platforms).

### Demoscene radio
- Nectarine (scenestream.net), via radio-browser: `http://nectarine.from-de.com/necta192` MP3 192 (bytes received over
  plain HTTP, verified), `http://necta.burn.net:8000/nectarine` MP3 192, `http://scenestream.io/necta48.aac` AAC 48,
  `https://scenestream.io/opus` Opus.
- SceneSat: `https://streams.scenesat.com/main/hq.mp3` (verified `audio/mpeg`, ICY headers), plain HTTP variant also
  streamed (verified), `.../mobile.aac`.
- Bitjam: no stream URL found (UNVERIFIED); radio-browser "bitjam" name search empty. CGM UK DemoScene MP3 256 too heavy
  for Amiga. Terms of these stations not read (UNVERIFIED); public streams.
- No separate source row: presets in I725a plus search via radio-browser.

### Jamendo
- `https://api.jamendo.com/v3.0/tracks/?client_id=...&format=json&limit=200&search=&audioformat=mp31|mp32|ogg|flac`.
  Fields `audio` (stream), `audiodownload`, `audiodownload_allowed`. Filters tags, fuzzytags, artist_name, order=popularity_total,
  `include=licenses,musicinfo`; `/radios` exists. `limit` max 200. `client_id` per app from devportal.jamendo.com.
- Terms https://devportal.jamendo.com/api_terms_of_use (page dated 2013): 3.3 "The API may be used freely for
  non-commercial uses." Commercial = "any use that is intended for or directed toward commercial advantage" (ads and
  affiliate count). 3.3: "Applications must not be specifically designed to cache the content." 4.1: credit the creators
  and "provide a direct backlink from each Content in the Application." No explicit third-party-player ban.
- Consequence: free non-commercial client; stream only, no library copy; creator credit + link per track.
- `mp31` = 96 kbit MP3 default (Amiga choice). Plain HTTP probe timed out (UNVERIFIED); assume https only until retested.

### Podcast RSS and Podcast Index
- RSS: open standard, `<enclosure url type length>`, mostly MP3, no auth. Enclosure hosts often redirect via trackers and https.
- Podcast Index (https://api.podcastindex.org): free key + secret, signed headers (`X-Auth-Key`, `X-Auth-Date`,
  `Authorization` SHA-1) and custom UA (secondary sources). `/api/1.0/search/byterm?q=`, `/api/1.0/episodes/byfeedid`.
  Official ToS not retrieved (UNVERIFIED).
- Needs Range playback (`get_range`) and redirect handling. Amiga only if the host serves plain HTTP MP3.

### ccMixter
- `http://ccmixter.org/api/query?f=json&limit=&tags=&search=` verified JSON (`upload_name`, `user_name`, `systags` with
  `non_commercial`, `mp3`); playlist `api/query/stream.m3u?f=m3u&ids=`. No key, plain HTTP works (verified).
- Terms https://ccmixter.org/terms: no framing or co-branding, do not "overburden, damage or impair" servers; content under
  the per-upload CC licence. No player clause found. Show licence per upload.

### Audius
- Free plan 10 requests/s, 500,000/month (https://docs.audius.org/sdk); app key via Settings "Manage Your Apps".
  `https://api.audius.co/v1/tracks/trending?app_name=selecta` verified JSON. Stream path not confirmed. Terms page not
  retrievable (UNVERIFIED); some tracks gated: skip those. Https only. Mac-only, low priority.

### Free Music Archive
- Rebuilt 2026 (secondary source). `freemusicarchive.org/api/get/tracks.json` and `/api` returned 404 (verified). No
  documented public API: skip.

### SoundCloud (do not build)
- https://developers.soundcloud.com/docs/api/terms-of-use: "Your app may employ session-based caching.", "your app must
  not offer offline access"; unacceptable: "any service that aggregates and streams User Content from multiple users into
  an on-demand listening service" and "to create any webcasting radio app." A general player is that.

### Bandcamp (do not build)
- No public streaming API (partner-gated, secondary source). https://bandcamp.com/terms_of_use: "The Service ... is
  provided only for your own personal, non-commercial use." Scraping track URLs is not an offered API. At most open the
  page in the system browser.

### ModArchive
- Site reachable (HTTP 200) but no streaming API found (UNVERIFIED). Fits the file-source pattern (asma/hvsc/zxart), not
  a stream source; separate row if wanted.

## Proposed build rows

I725a first: radios and podcasts reuse it. Each row names Mac or Amiga in its test steps.

0. I725p Per-source User-Agent and request headers on the shared HTTP client (blocks b, c, d, f). Confirm `Icy-MetaData` path reuse.
1. I725a Generic stream source: URL list, `.pls`/`.m3u` resolve, ICY title (parser exists), MP3 + Ogg; desktop on `sel_live`, Amiga with I199/I721.
2. I725b SomaFM presets from `channels.json` (MP3 128 default). Needs I725a.
3. I725c radio-browser directory source (search/tags/country/codec filter, UA, click ping). Covers Nectarine, SceneSat, other demoscene radios. Needs I725a, I725p.
4. I725d Internet Archive audio source (advancedsearch + metadata + download, licence filter, 429/Retry-After, UA, MP3/Ogg derivative). Needs I725p.
5. I725e Jamendo source (client_id, search/browse, mp31 stream, creator credit + backlink, no cache). Retest plain HTTP first.
6. I725f Podcast source: RSS parser + Podcast Index search + Range playback. Needs I725p.
7. I725g ccMixter source (query API, per-upload licence).
8. I725h AAC and Opus decoders (Mac) for SomaFM AAC, Nectarine Opus.
9. I725i Amiga HTTPS decision (link `tls.c` or Mac-side relay); gates Jamendo and podcasts on Amiga.
10. Parked: Audius (read terms first), Free Music Archive (no API), SoundCloud and Bandcamp (terms forbid).

## Open items

- Official ToS text not read: radio-browser, Internet Archive terms, Podcast Index, Audius, SomaFM, Nectarine, SceneSat.
- Jamendo plain-HTTP reachability; Archive download CDN redirect; whether Amiga links `tls.c`.
