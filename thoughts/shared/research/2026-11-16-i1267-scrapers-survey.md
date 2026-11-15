# I1267 scrapers and datasets survey (2026-11-16)

Method: WebFetch/WebSearch/gh api. Pages that returned 403 (IGDB docs, MobyGames API page) are marked UNVERIFIED. Not legal advice.

## 1. Google Custom Search JSON API
- Overview https://developers.google.com/custom-search/v1/overview : "Custom Search JSON API provides 100 search queries per day for free." "Additional requests cost $5 per 1000 queries, up to 10k queries per day." "Existing Custom Search JSON API customers have until January 1, 2027 to transition". Closed to new customers.
- Storing: the overview has no storage terms. The API is covered by the Google APIs Terms of Service, https://developers.google.com/terms section 5.e.1 (Prohibitions on Content): "Scrape, build databases, or otherwise create permanent copies of such content" and "keep cached copies longer than permitted by the cache header". Programmable Search terms (https://cse.google.cz/cse/docs/tos.html, per search snippet, not fully read) also say Results may not be framed, cached or modified. Image files found are third-party copyrighted regardless.
- Verdict: NOT allowed to store/re-serve results or images. Pre-fetching 50k queries to build our own database is exactly the "build databases / permanent copies" clause. Cost would be ~$250 (50k x $5/1000) but at 10k/day cap it takes 5 days minimum, 100 free/day irrelevant; and it would breach terms. Do not do it. (The exact quota applying to Johan's key may differ; check his console.)

## 2. Survey
Image rights: almost every box art / screenshot is third-party copyright; "open" below refers to the project's own stated terms only.

| Source | URL | Code licence / data licence | Platforms | Key | Bulk mirror? | Activity | Screenshots? |
|---|---|---|---|---|---|---|---|
| libretro-thumbnails | https://github.com/libretro-thumbnails/libretro-thumbnails (old libretro/libretro-thumbnails archived 2018) | no licence file; README: "files in the repository have many different sources" ; PNG <=512px | Amiga, C64, Atari 8-bit, ST, 2600/5200/7800/Lynx/Jaguar, DOS, consoles (dirs present) | none | Public git repo, clonable; no grant, no prohibition. Johan chose it (D7). Tolerated, not licensed | pushed 2026-09 | Yes: Named_Boxarts, Named_Snaps, Named_Titles per system = exact-platform screenshots |
| libretro-database | https://github.com/libretro/libretro-database | CC-BY-SA-4.0 (dat files, names/hashes) | broad | none | Yes for metadata (attribution + share-alike) | 2026-11-15 | no images; gives canonical names to match libretro thumbnails |
| Skyscraper | https://github.com/muldjord/skyscraper | GPL-3.0 code; data from modules | via modules | IGDB needs Twitch id/secret; ScreenScraper account recommended | Tool only, mirroring governed by each module's source | 2026-02 | module-dependent |
| ScreenScraper | https://www.screenscraper.fr | Contributors agree CC licence, "downloadable by software via ScreenScraper's API" (signup page) | very broad incl. Amiga, C64, Atari | account; Skyscraper docs: 20k req/day registered; donor tiers 50k-100k/day | Per-account quota, no sharing accounts; CC wording is unspecified (which CC?) and the images are still third-party. Ask the operators | active | Yes (ss, box, title) per system |
| TheGamesDB | https://thegamesdb.net / api.thegamesdb.net | not stated; (c) TheGamesDB | many | API key by request form | no terms found; Skyscraper: 3000 req/IP/month | active | boxart/screens; platform-tagged |
| IGDB | https://api-docs.igdb.com (403, UNVERIFIED) | free for non-commercial under Twitch Developer Service Agreement; commercial needs partnership (search snippets) | modern focus, weak Amiga/C64 | Twitch client id + 2FA | Unverified; assume no bulk | active | screenshots yes, platform tag weak |
| MobyGames API | https://www.mobygames.com/info/api (403, UNVERIFIED) | free API is for not-for-profit use (search snippet); MobyPro for commercial | excellent Amiga/C64/Atari | account key | Skyscraper: 1 req/10 s, "use sparingly"; no mirroring | active | yes, per platform |
| OpenRetro | via Skyscraper docs | not stated | best Amiga WHDLoad DB | none | no terms found | - | yes |
| OpenVGDB | https://github.com/OpenVGDB/OpenVGDB | none stated | consoles; no Amiga/C64 | none | unclear | last push 2021 | covers only |
| LaunchBox Games DB | https://gamesdb.launchbox-app.com | no terms found; Metadata.zip (XML, daily) per forum, metadata only | hundreds incl. C64 | none for zip | no licence for images; do not mirror | active | images fetched individually |
| Demozoo | https://demozoo.org ; code https://github.com/demozoo/demozoo | code: no spdx via gh; data: archival SQL dump + API exist, licence not confirmed | Amiga/C64/Atari scene productions | none | Johan decided cache images (D7); confirm dump terms with the maintainers | 2026-11-14 | scene screenshots, per production |
| Hall of Light / Lemon64 / Atarimania / GB64 | per design doc | all refuse bots; no dump | Amiga / C64 / Atari | - | email permission (Johan D1). Not before | - | yes, exact platform |

## 3. Recommendation
- Mirror in bulk now: libretro-database dats (CC-BY-SA, metadata) as the match index; libretro-thumbnails images (public, platform-exact box/snap/title, matches D7; risk: no licence, so serve with takedown contact and source credit, and remove on request).
- Per request only (cache match results, not image files): Atarimania, ScreenScraper (within its quota, ask operators about bulk), TheGamesDB, MobyGames (non-commercial key, 1 req/10 s), IGDB (only after reading its terms), Demozoo images per D7 after maintainers confirm.
- Never: Google Custom Search results or images stored/re-served (Google APIs ToS 5.e.1); any scraping of Hall of Light, Lemon64, Atarimania, GB64 before they agree; LaunchBox images; fan-made or other-platform images.

Not checked: full current text of Google Programmable Search terms, IGDB/MobyGames/TheGamesDB API terms (403 or not stated), ScreenScraper CC variant, Demozoo dump licence, GB64 and Hall of Light dumps.
