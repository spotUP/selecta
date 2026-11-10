---
date: 2026-11-10
topic: Demozoo API, terms, matching and bulk data for the I1303 slide show cover art
row: I1303 step (a)
---

# Demozoo API research (I1303 a)

Key result first: since 1 November 2026 the API needs an API key (anonymous calls return 403), the API has no search and no "featured in" field, and the site search is disallowed for robots. A nightly SQL dump (about 203 MB) exists and is the realistic route for matching. The music entry page carries an exact Modland path, so matching by Modland path is exact.

Requests made to demozoo.org: 11 (API probes, 2 entry pages, robots.txt, FAQ, dump HEAD to data.demozoo.org). GitHub raw files fetched from demozoo/demozoo branch `main`.

## 1. API shape

Base URL and access rule, from the repo template https://github.com/demozoo/demozoo/blob/main/users/templates/users/api_keys.html (raw: https://raw.githubusercontent.com/demozoo/demozoo/main/users/templates/users/api_keys.html):

> Demozoo provides a JSON API at https://demozoo.org/api/v1/, giving full programmatic access to production, scener, party and BBS data. ... From 1st November 2026, anonymous access to the API will be disabled and you will need to create an API key to access the API from code.

Live check today (2026-11-10), with a generic User-Agent and no key:
- `GET https://demozoo.org/api/v1/productions/24870/` returns HTTP 403 `{"detail":"Authentication credentials were not provided."}`.
- `GET https://demozoo.org/api/v1/` returns 403 as well.
- `GET https://demozoo.org/api/v1/music/` returns 404: there is no music endpoint.

So I could not fetch the real JSON for ids 258347 and 24870. The field names below come from the serializer source, not from live JSON (unverified against a live response).

Router, from https://raw.githubusercontent.com/demozoo/demozoo/main/api/urls.py: `productions`, `production_types`, `releasers`, `platforms`, `party_series`, `parties`, `bbses`, plus `v1/auth_test/`. Music entries are productions with `supertype` "music" (https://demozoo.org/music/258347/ is the music-flavoured URL of production 258347; the model has `supertype = models.CharField(... SUPERTYPE_CHOICES ...)`, productions/models.py line 119, and `if self.supertype == "music"` at line 106).

Endpoints:
- (i) Search by title/author: only filters on `GET /api/v1/productions/`, see question 2.
- (ii) One music entry: `GET /api/v1/productions/258347/` (same detail serializer, supertype `music`).
- (iii) "Featured in": NOT in the API. The model has `SoundtrackLink` with `related_name="soundtrack_links"` on the production and `related_name="appearances_as_soundtrack"` on the music entry (productions/models.py lines 611-616), but `ProductionSerializer.Meta.fields` is exactly: `url, demozoo_url, id, title, author_nicks, author_affiliation_nicks, release_date, supertype, platforms, types, credits, download_links, external_links, release_parties, competition_placings, invitation_parties, screenshots, tags` (https://raw.githubusercontent.com/demozoo/demozoo/main/api/serializers.py). Neither soundtrack list is there. The relation is visible only in the HTML page (see question 4) or in the SQL dump (table behind `SoundtrackLink`).
- (iv) Screenshots: field `screenshots`, each item has exactly: `original_url, original_width, original_height, standard_url, standard_width, standard_height, thumbnail_url, thumbnail_width, thumbnail_height`. Real URL forms seen on the production page https://demozoo.org/productions/24870/: original `https://media.demozoo.org/screens/o/85/cb/1c69.7936.png`, standard `https://media.demozoo.org/screens/s/85/cb/1c69.7936.png` (width 375, height 300), thumbnail `https://media.demozoo.org/screens/t/85/cb/1c69.7936.png` (187 x 150 on the music page). Only one screenshot is shown inline on the page; the page has a link "all_screenshots" to `/productions/24870/screenshots/`. Whether production 24870 has "several" in the API is unverified (not countable without a key).
- (v) Video and external links: fields `download_links` and `external_links`, each item `{link_class, url}` (`ProductionExternalLinkSerializer`, fields `["link_class", "url"]`). Link classes are Python class names from https://raw.githubusercontent.com/demozoo/demozoo/main/common/utils/groklinks.py: `YoutubeVideo` (line 1104), `ModlandFile` (854), `SceneOrgFile` (680), `AmpModule` (455), `PouetProduction` (381), `KestraBitworldRelease` (657). On the page 24870 the external links include two YouTube watch URLs: `https://www.youtube.com/watch?v=cjJHZV2EhXg` and `https://www.youtube.com/watch?v=nN1LBz8TygM`. Vimeo and others were not checked. The video is a YouTube link, not a file: playing it silently inside Selecta needs a YouTube player or a separate download route, which this research did not cover.

Auth form: `Authorization: Api-Key 1234aBCd.yourkeyhere` header (recommended), or query parameter `api_key` (api/permissions.py `URLKeyParser`). Keys are created by a logged-in user on the "Demozoo API" page of the site.

## 2. Search

No search endpoint. `ProductionFilter` (api/filters.py) offers: `title` with `iexact` (exact, case-insensitive, no substring), `platform`, `production_type`, `released_before/since`, `added_before/since`, `updated_before/since`, `author` (releaser id), `has_screenshot`, `competition_placing_min`, `link_url`, `supertype`. Ordering fields: `id, sortable_title, release_date_date, supertype`. So `?supertype=music&title=april in paris fix` is possible, and `?link_url=<modland url>` filters by a link: `filter_link_url` calls `ProductionLink.grok(value)` and matches `link_class` plus parameter. That makes a Modland URL an exact lookup key: `GET /api/v1/productions/?link_url=https://ftp.modland.com/pub/modules/Protracker/Skope/april%20in%20paris%20fix.mod` (not tried live, needs a key; unverified).

Site search is at `/search/`. robots.txt (https://demozoo.org/robots.txt):

> User-agent: * / Disallow: /search/ / Disallow: /account/ / Disallow: /beep-boop/ / Crawl-delay: 10

So machine use of the site search is disallowed by robots.txt. Do not use it.

## 3. Rate limits, User-Agent, auth, terms, licence

- Rate: api_keys.html: "Don't be a resource hog! If you're making repeated API requests, please leave a 1 second pause between them." robots.txt for HTML pages: Crawl-delay 10.
- Required User-Agent: not stated (looked at api_keys.html, robots.txt, FAQ https://demozoo.org/pages/faq/). A plain client name worked for page fetches.
- Authentication: API key required from 1 November 2026 (see 1). Key creation needs a Demozoo account (SceneID or password login).
- Terms of use for the API, rules for hotlinking or caching screenshots, licence of data and screenshots: not stated. Looked at: api_keys.html, FAQ, README, robots.txt, GitHub licence endpoint (returned Not Found for the repo, so no detected licence file in the repo root either; /LICENSE on main is 404). Page footer says "(c) 2026 Demozoo staff and contributors". The FAQ says only: "The entire website is open source, and we post a daily dump of our database, in the interest of an open data model." That is an intent statement, not a licence grant. Screenshots are third-party artwork of scene productions; their rights are not addressed. Screenshot host is `media.demozoo.org`.
- Consequence for the design: caching screenshots in library.db is not forbidden anywhere I found, but no permission is stated either. Safest: ask Demozoo (contact via Discord or the maintainer) before shipping; until then store only URLs plus a local cache, never redistribute images. This is a judgement for Johan.

## 4. Matching

The music entry page https://demozoo.org/music/258347/ shows (HTML, title "april in paris fix by Skope", Amiga OCS/ECS, released 15 April 2001):
- Download link class `modland` with exact path: `https://ftp.modland.com/pub/modules/Protracker/Skope/april%20in%20paris%20fix.mod`, plus mirrors (exotica.org.uk, ziphoid, amigascne.org, antarctica.no) with the same path tail.
- External link: Kestra BitWorld `https://janeway.exotica.org.uk/release.php?id=52059`.
- Credits: "Skope - Music".
- Featured in: `/productions/24870/` = "April in Paris, Up Rough Soundsystem, Apr 2001, Soundtrack".

So the best key is the Modland path (`pub/modules/<Format>/<Author>/<file>`): exact, and Selecta already knows Modland paths for library files from Modland. Weaker keys: title + author nick (the `title` filter is exact, authors are nicks with variants), and file hash (not exposed). The `ModlandFile`/`AmpModule` link classes also give Amiga Music Preservation ids if the library stores them. Not every music entry has a Modland link; the proportion is unknown (not counted).

## 5. Bulk data

Yes. `https://data.demozoo.org/demozoo-export.sql.gz`, announced on api_keys.html: "A public SQL dump of the whole database (minus sensitive information such as password hashes) is also available at https://data.demozoo.org/demozoo-export.sql.gz, updated nightly." HEAD request today: HTTP 200, `content-length: 202910918` (about 203 MB gzipped), `last-modified: Tue, 10 Nov 2026 02:38:03 GMT`, served from S3 via CloudFront. The README says the dev setup "fetch[es] the latest public export" and that passwords in it are blanked. Licence: not stated beyond "open data model". The dump contains the soundtrack links, so a server job can build a table Modland path -> music entry -> production -> screenshot URLs offline, no per-song requests and no API key. Screenshot files still come from media.demozoo.org.

## 6. Coverage

No page or API count stated. Not crawled. The dump would give both counts in one query (music supertype rows, rows with a `soundtrack_links` entry, rows with a `modland` link).

## Unverified

- Live JSON for ids 258347 and 24870 (API returns 403 without a key); field names are from source on `main`, which may differ from the deployed version.
- Number of screenshots of production 24870, and whether `link_url` filter works with a Modland URL.
- Where the API key creation page lives (URL not checked; the template exists as `users/api_keys.html`, view `users/views/api_keys.py`).
- Any written terms on screenshot reuse (none found in api_keys.html, FAQ, README, robots.txt; the "screenshots" article linked from the FAQ was not opened).
- Video: only YouTube watch links found; no direct video file field.
- Counts of music entries, featured-in rate, Modland link rate.

## Dump measurements (I1303a)

Source: https://data.demozoo.org/demozoo-export.sql.gz, 202,910,918 bytes, LastModified 2026-11-10T02:38Z, PostgreSQL pg_dump (COPY blocks, 266 tables). Streamed with gunzip -c, never unpacked. Measured 2026-11-10.

Tables and columns (from COPY headers):
- `public.productions_production (id, title, notes, release_date_date, release_date_precision, created_at, updated_at, supertype, has_bonafide_edits, demozoo0_id, data_source, scene_org_id, unparsed_byline, include_notes_in_search, sortable_title, has_screenshot, search_document, search_title, locked, hide_from_search_engines)`; supertype values: music 136,689, production 190,461, graphics 64,516, empty 14.
- `public.productions_soundtracklink (id, production_id, soundtrack_id, "position", data_source)`: production_id = the host (demo), soundtrack_id = the music entry. One table gives the exact song-to-production mapping. data_source e.g. `janeway`.
- `public.productions_productionlink (id, link_class, parameter, production_id, is_download_link, description, demozoo0_id, file_for_screenshot, is_unresolved_for_screenshotting, thumbnail_url, thumbnail_width, thumbnail_height, embed_data_last_fetch_time, embed_data_last_error_time, video_width, video_height, has_bad_image, source)`: one table for downloads AND external links. Modland = link_class `ModlandFile`, parameter is a path starting `/pub/modules/...` (percent-encoded, no host), is_download_link t. YouTube = `YoutubeVideo`, parameter = video id. Other classes seen: CsdbMusic 6,761 rows, DeepSIDTrack 323, NectarineSong 5,947, ModarchiveModule 3,979, AmpModule 16,932, SoundcloudTrack 2,077, BaseUrl 119,580 (6 carry a modland.com URL).
- `public.productions_screenshot (id, production_id, original_url, original_width, original_height, thumbnail_url, thumbnail_width, thumbnail_height, standard_url, standard_width, standard_height, source_download_id, data_source, janeway_id, janeway_suffix)`.
- Author naming: `public.productions_production_author_nicks (id, production_id, nick_id)`, `public.productions_production_author_affiliation_nicks (id, production_id, nick_id)`, `public.demoscene_nick (id, releaser_id, name, abbreviation, differentiator)`, `public.demoscene_releaser (id, name, is_group, ...)`, `public.demoscene_nickvariant (id, nick_id, name, search_title)`. Also `public.productions_credit (id, production_id, nick_id, role, category, ...)`.

Counts (one pass; sets by id):
- music productions: 136,689
- music with a Modland link (class ModlandFile or modland.com URL): 50,233 (ModlandFile class alone 50,227; any production with one: 50,321)
- music that is a soundtrack of >= 1 production: 58,126; hosting productions: 48,189 distinct, of which 45,957 have a screenshot
- music that is a soundtrack of >= 1 production having a screenshot: 55,020
- music with a YouTube link: 3,708 (all productions: 36,329)
- music with Modland link AND soundtrack of any production: 32,961
- music with Modland link AND featured in a production with screenshot (the slide-show set): 31,028
- music with Modland, soundtrack and YouTube: not measured; Modland + YouTube (any): 803
- HVSC/SID: no HVSC link class. Classes `CsdbMusic` (6,755 music rows, parameter = CSDb music id) and `DeepSIDTrack` (316 music rows) exist. 9,322 music `ModlandFile` rows matched a loose "hvsc/sid" test in the script; what they are (HVSC mirror paths vs. other) is UNVERIFIED.

Check 258347 / 24870: both present as expected.
- 258347 "april in paris fix", supertype music, has_screenshot f; ModlandFile `/pub/modules/Protracker/Skope/april%20in%20paris%20fix.mod` (download t); also KestraBitworldRelease 52059; author_nicks row (434717, 258347, nick 3384).
- 24870 "April in Paris", has_screenshot t; soundtracklink row (51933, production 24870, soundtrack 258347, pos 1, janeway); 14 screenshots, e.g. `https://media.demozoo.org/screens/o/85/cb/1c69.7936.png` 640x512 and `.../6b/11/eba5.jw26928h.png` 360x284; YoutubeVideo nN1LBz8TygM and cjJHZV2EhXg; author nick 475.
- Note: the song's own production has no screenshot (f); the picture comes only through the soundtrack link.

Image fetch (one request, UA `Selecta/0.1 ( https://hippoplayer.se )`, no Referer): HTTP/2 200, image/png, 8,524 bytes, server AmazonS3 behind CloudFront, `x-cache: Miss from cloudfront`, last-modified 2013-02-08, etag present. No Cache-Control, no Access-Control-Allow-Origin header returned (request sent without Origin, so CORS for browsers is UNVERIFIED; irrelevant for a native fetch). Image deleted.

Licence / terms: none found. data.demozoo.org index is an S3 ListBucketResult XML with a single key (the dump); /README, /README.txt, /README.md, /LICENSE return 404; first 3 MB of the dump has no licence comment. Terms of use remain unverified (check demozoo.org pages by hand; not crawled here).

Unverified: the HVSC meaning of ModlandFile rows above; CORS; licence; whether `supertype=music` includes non-tracker music (yes, includes streamed/other formats; only Modland-linked rows are tracker files).
