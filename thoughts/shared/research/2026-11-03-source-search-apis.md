---
date: 2026-11-03
topic: Which source plugins can be searched by query, and how (All sources search, I1019)
row: I1024
---

# Source search APIs

Plugin count: 13. Counted twice: `ls -d plugins/sources/*/` = 13, and `ls plugins/sources/*/CMakeLists.txt` = 13
(plugins/sources/CMakeLists.txt globs `*/CMakeLists.txt`; cmake/selecta_registry.cmake does not list sources, so the glob
is the structure that builds them). The two match.

Reference pattern (modulespl.c:89-125): `ms_search` gets net + catalogue from the host, URL-encodes the query, one
`net->get_file_wait(host, url, path, first_byte_ms, ...)` into the plugin cache dir, a non-200 returns -1 (service
unreachable), the file is slurped, parsed, each hit goes to `out->row(ctx, path, title, artist, format)`, return value =
row count. `row` returns 0 to stop early; there is no separate cancel hook. Sink fields are only path, title, artist,
format: no duration, no composer column (composer must be folded into artist). The path is the song key after `<id>:`.

Surprise: `dlna.c:905` registers `NULL` for `search`. Only modulespl implements the hook today (the ledger said
modulespl and dlna). DLNA could get a UPnP ContentDirectory `Search` action, noted below as I1024h (unverified, needs a
live MediaServer; the NAS in memory is DS218+ at 192.168.0.198:50001).

## Table

Live checks 2026-11-03 from this machine with `curl -A selecta-survey`.

| source | service | remote search | fields | auth / limits | proposed row | notes |
|---|---|---|---|---|---|---|
| archiveorg | archive.org advancedsearch + metadata | yes: `https://archive.org/advancedsearch.php?q=<lucene>&fl[]=identifier&fl[]=title&fl[]=creator&rows=N&output=json` (verified, numFound 3250 for title:beethoven) | identifier, title, creator, licenseurl | none; asks for descriptive User-Agent, honours 429 Retry-After (host request_ex does) | I1024a | Song key needs per-item `/metadata/<id>` for the track list (plugin does 1 request per hit): a search row must be item-level or cap to few hits. HTTPS only, not Amiga-playable until I725i |
| jamendo | api.jamendo.com v3.0 | yes: `/tracks/?client_id=<id>&format=json&search=<q>&limit=N` (endpoint reached; answered "Invalid Client Id" without a key, response shape unverified live) | name, artist_name, id, license, audio url | client_id required (pref sources.jamendo_client_id); non-commercial terms | I1024b | Plugin sync already builds the same URL with a `search` param (jamendo.c:305); reuse its parser |
| podcast | Podcast Index | yes: `https://api.podcastindex.org/api/1.0/search/byterm?q=<q>` (podcast.c:300) | feed title, author, feed url; episodes need a second fetch of the feed | key + secret (prefs sources.podcast_index_key / _secret), X-Auth headers; terms unverified (site 403) | I1024c | Hits are shows, not songs; song rows need the feed fetch. Key required: skip when unset |
| radiobrowser | radio-browser.info | yes: `https://de1.api.radio-browser.info/json/stations/search?name=<q>&limit=N&hidebroken=true&order=clickcount&reverse=true` (verified, returns JSON array) | stationuuid, name, url_resolved, country, tags, codec | none; community servers, pick via all.api DNS; be polite | I1024d | Plugin already uses this endpoint (radiobrowser.c:246), pass `name=` instead of sync params. Key = `<uuid>/<name>` |
| ccmixter | ccMixter query API | yes: `http://ccmixter.org/api/query?f=json&search=<q>&limit=N&sort=rank` (verified; matches tags and text) | upload_id, upload_name, user (artist), license, files[] with download url | none; descriptive UA, 429 back-off | I1024e | Plugin sync already supports `search` (ccmixter.c:366) and parses this JSON; reuse |
| zxart | zxart.ee JSON API | yes: `https://zxart.ee/api/language:eng/export:zxMusic/start:0/limit:N/filter:zxMusicTitleSearch=<q>` (verified: Beyond gives totalAmount 10 vs 29731 unfiltered). Other filter spellings tried (zxMusicTitle=, title:, zxMusicSearch:) are silently ignored | id, title, authorIds, type (format, e.g. PT3), time, year | none; limits unverified | I1024f | Author name search unverified (authorIds are numeric; need `export:author` lookup). Docs page /eng/api/ gave nothing |
| aminet | aminet.net | partial: `https://aminet.net/search?query=<q>&type=advanced&sort=name` returns HTML (200 verified), no JSON API known | HTML table: name, dir, size, description | none; scraping | I1024g | Needs an HTML parser like modulespl_parse.c. Cheaper alternative: local search over synced rows (below) |
| dlna | UPnP MediaServer on LAN | possible: ContentDirectory `Search` SOAP action (`dc:title contains "<q>"`) | title, creator, res url | none; server must advertise SearchCapabilities (unverified) | I1024h | Plugin currently syncs via Browse/delta search (dlna.c:670). Only works when the server is on the network; no fixture exists yet |
| modulespl | modules.pl | already implemented | - | 10-11 s first byte | - | reference |
| hvsc | hvsc.de download | no: file index only | path (artist/title from folders) | - | - | local fallback only |
| asma | asma.atari.org | no: file index only | path | - | - | local fallback only |
| sndh | sndh.atari.org | no: file index only | path | - | - | local fallback only |
| keygenmusic | keygenmusic.tk | no: directory listing only | path | - | - | local fallback only |

Local fallback (all sync sources: hvsc, asma, sndh, keygenmusic, aminet, zxart, and any other with SYNCS): search the
synced `source_songs` rows (title/artist/format from `parse`) with a SQL LIKE. This is the library search that already
covers synced sources, so no per-source row is needed: confirm in I1019 that Library scope already includes synced
source rows; if not, that is one engine row, not seven plugin rows (unverified: I did not read the engine search path).
Unsearchable remotely, covered by that fallback: hvsc, asma, sndh, keygenmusic (reason: services publish only static
file listings, no query endpoint).

Recommended order (biggest catalogue first, then keyless before keyed within reason): archiveorg (millions of items),
jamendo (hundreds of thousands of tracks, key), podcast (millions of feeds, key), radiobrowser (tens of thousands of
stations), ccmixter, zxart (29.7k tracks), aminet, dlna. Rows b and c are blocked by a user-supplied key; ship d/e/f
first if keys are not set up. Each added source adds latency to All sources (bucket queue I1019a).

## I1024a archiveorg
Endpoint `GET https://archive.org/advancedsearch.php?q=mediatype%3Aaudio+AND+%28title%3A%22<q>%22+OR+creator%3A%22<q>%22%29+AND+licenseurl%3A%2Acreativecommons.org%2A&fl%5B%5D=identifier&fl%5B%5D=title&fl%5B%5D=creator&rows=25&output=json`
(keep the plugin's licence filter, archiveorg.c:463). Sink: path `<identifier>/<creator or unknown>/<title>` (same key as
sync, archiveorg.c:344), title, artist = creator, format `Internet Archive - <licence>`. Per-track rows need
`/metadata/<id>` per hit; cap at ~8 items per search to bound requests, or emit one row per item (decision for the row).
Fixture: tests/net/archiveorg.cmake already exists; the base is replaceable with SELECTA_ARCHIVEORG_BASE; add a canned
advancedsearch JSON with the query echoed, plus a 429 case.

## I1024b jamendo
`GET <base>/tracks/?client_id=<id>&format=json&audioformat=mp31&search=<q>&limit=25`. Mapping as jamendo.c:207-209:
path `<id>/<artist>/<name>`, artist = artist_name, format `Jamendo - <licence>`. No client id: return -1 with a stderr
line like sync. Fixture: tests/net/jamendo.cmake with SELECTA_JAMENDO_API pointing at the local server.

## I1024c podcast
`GET <api>/api/1.0/search/byterm?q=<q>` with X-Auth-Key / X-Auth-Date / Authorization (sha1 key+secret+date) as in
podcast.c:292-300. Mapping: one row per feed (path = show key, title = feed title, artist = author, format `Podcast`);
episodes stay lazy. Decide with Johan whether a feed counts as a song row. Fixture: SELECTA_PODCAST_INDEX_API to a
local server that checks the three auth headers.

## I1024d radiobrowser
`GET <base>/json/stations/search?name=<q>&hidebroken=true&order=clickcount&reverse=true&limit=25`. Mapping as
radiobrowser.c:149-150: path `<stationuuid>/<name>`, title = name, artist = `country - tags`, format = codec. Fixture:
tests/net/radiobrowser (SELECTA_RADIO_BROWSER) with one canned array.

## I1024e ccmixter
`GET <base>/api/query?f=json&sort=rank&limit=25&search=<q>` (tags default mp3 is dropped for search or kept: decide).
Mapping as ccmixter.c:287-288: path `<upload_id>/<user>/<title>`, artist = user name, format `ccMixter - <licence>`.
Fixture: tests/net/ccmixter.cmake (SELECTA_CCMIXTER_BASE); reuse the existing canned query JSON.

## I1024f zxart
`GET https://zxart.ee/api/language:eng/export:zxMusic/start:0/limit:25/filter:zxMusicTitleSearch=<q>` (percent-encode q).
Mapping: path as the sync key, title = title, artist from authorIds (needs the author table already synced, so look up
locally), format = type (PT3 etc.). Author-name search needs a separate request, unverified. Fixture: a local server
answering that path; tests/net has the zxart sync fixture pattern (zxart_net).

## I1024g aminet
`GET https://aminet.net/search?query=<q>&type=advanced&sort=name` returns HTML; write `aminet_parse.c` in the
modulespl_parse.c style. Mapping: path `<dir>/<name>`, title = name, format from dir (mods/...). Fixture: saved result
page in tests/net/fixtures. Lowest value because sync already holds the catalogue (see local fallback).

## I1024h dlna
SOAP `Search` on the ContentDirectory control URL with `upnp:class derivedfrom "object.item.audioItem" and dc:title contains "<q>"`.
Mapping: path as the sync key (dlna.c parse), title = dc:title, artist = upnp:artist/dc:creator, format from the res
protocolInfo. Unverified against the real NAS. Fixture: tests/net/dlna.cmake already runs a fake MediaServer; extend it
with a Search handler.
