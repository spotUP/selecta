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
