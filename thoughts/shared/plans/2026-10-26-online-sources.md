---
date: 2026-10-26
topic: Online sources for Selecta: every service HippoPlayer can load songs from, then other services
tags: [plan, source, aminet, modules-pl, amp, hvsc, modland, radio, amigaremix, kwed, m4, parity]
status: inventory and plan, not implemented; product choices open (questions-2026-10-27.md, "Online sources (I405+)")
rows: I405-I412 in thoughts/spot/todos.md; F33 reopened
---

# Online sources (I405+)

Johan: every online music service HippoPlayer can load songs from becomes a Selecta source, then other online services.
Implements design doc `2026-10-22-architecture-design.md` section 9 "Library: sources, catalogue, cache" (9.1 sources,
9.2 catalogue and cache), line ~348 (F33: Aminet, Modules.pl and AMP as `sel_source` modules) and the parity rule
(v1 supports everything bbmp and HippoPlayer do). Format copied from `2026-10-26-youtube-source.md`.

F33 was ticked in the ledger but `sel_source_register_builtin` (src/library/source.c:48-59) registers only local,
modland, hvsc, hippo. F33 is reopened; this plan is the work.

## 1. Inventory: what HippoPlayer loads online

Primary source: the HippoPlayer repository https://github.com/koobo/HippoPlayer (cloned 2026-10-26, shallow) and its
shipped docs `dist/hippoplayerupdate.doc` and `dist/hippoplayerupdate.readme` (version 2.67). Finding that shapes the
whole plan: **the repository contains no search or download code for these services.** `grep -rniI` for modland,
aminet.net, UHC and similar over all `.s`/`.S`/`.i` files finds nothing outside the player docs. HippoPlayer runs the
searches by calling the **UHCTools** scripts (readme line 1063: "uses the excellent UHCTools to do the actual work";
dist/hippoplayerupdate.doc:88 "Requires UHCTools installed"). UHCTools (https://aminet.net/util/misc/UHCTools.readme,
v2.2, Patrik Axelsson / David Eriksson) downloads its own scripts at install time, so the per-service endpoints live in
those downloaded scripts, which are not in the repo and were not fetched. Every endpoint below is therefore either
probed directly from the service on 2026-10-26 (marked PROBED) or UNVERIFIED.

| # | Service | What HippoPlayer does | Citation | Endpoint and format (Selecta view) | Status |
|---|---------|-----------------------|----------|------------------------------------|--------|
| 1 | Modland | Search (Ctrl+M), results filtered to Hippo-playable formats, download to temp, play | readme:1057,1074-1076; fix lines 212, 751, 877 (whitelist); doc:93 | `https://ftp.modland.com/allmods.zip` listing and `pub/modules/...` files (already used by `modland_net.c`) | EXISTS |
| 2 | Aminet | Search; only hits under the `mods` subhierarchy are returned; download to temp | readme:1058,1077-1078; doc:93 | Search page `https://aminet.net/search?query=<q>&path[]=<dir>` returns HTML (PROBED 200); OpenSearch template `http://aminet.net/search?query={searchTerms}` (PROBED, /info/adt/opensearch.xml); full index `https://aminet.net/INDEX` (PROBED 200, 992 KB, fixed columns `name  dir  size  age  description`); file URL `https://aminet.net/<dir>/<name>` (UNVERIFIED shape); the `mods` dir name inside INDEX could not be confirmed (my grep of INDEX for `mus/` and `mods/` rows returned nothing, UNVERIFIED) | MISSING |
| 3 | Modules.pl | Search (S-button popup), download to temp | readme:1059; doc:93 | Site has a search form posting to `/cmds.php?cmd=search` (PROBED in homepage HTML); a GET `cmds.php?cmd=search&search=chip` returned the 404 page (PROBED), so the real request shape (POST? fields?) is UNVERIFIED. Radio stream `http://radio.modules.pl:8500/listen.pls` (PROBED link in HTML) | MISSING |
| 4 | HVSC | Search (S-button), also STIL and Songlengths for PSID | readme:1060, 429-434, 608-619; doc:93 | `hvsc.c64.org/download/C64Music/...` (already used by `hvsc_net.c`; fixtures in tests/net/fixtures/hvsc.c64.org) | EXISTS |
| 5 | AMP (Amiga Music Preservation) | Search source added in a 2.x release (readme:184-186: "may be slow due to HTTPS") | readme:184-186; doc:93 | `https://amp.dascene.net/` answered **403** to curl with a browser user agent (PROBED, Cloudflare style challenge page). Search form, result page and download URL shape UNVERIFIED. Needs a header or browser-like client; cannot be assumed to work from our HTTP client | MISSING |
| 6 | amigaremix.com | MP3 search source via UHCTools, streamed | readme:908-910; doc:93 | Homepage PROBED 200 HTML (116 KB); no search endpoint found in the HTML (UNVERIFIED). MP3 streams | MISSING |
| 7 | remix.kwed.org | MP3 search source via UHCTools, streamed | readme:908-911; doc:93 | Homepage PROBED 200 HTML; list pages use query string `?page=1&chart=&view=title&search=` (PROBED in homepage links); download URL UNVERIFIED | MISSING |
| 8 | Radio stations | Station search, MP3 streams, infobox shows data downloaded | readme:903-912, 969; doc:101-116 | Station directory behind UHCTools is UNKNOWN. Direct stream example from Modules.pl above. UNVERIFIED | MISSING |
| 9 | Shared playlists (hippoplayer.se) | "Shared lists" source: fetch list of community playlists, `.prg` files; fix to fetch more than 25 items (readme:785) | readme:913-917, 785, 839; doc:96 | `https://hippoplayer.se/api/v1/playlists` JSON and `/playlist/<file>.prg` (fixtures in tests/net/fixtures/hippoplayer.se, PROVENANCE.md) | EXISTS as `hippo` |
| 10 | Local file browser, URL playlists | Remote files stored in playlists as URLs; `.m3u` limited support | readme:487, 1080-1081 | Not a service. Covered by local source plus playlist import | n/a |

Count check (second pass, from the doc's own list at hippoplayerupdate.doc:93-96): Modland, Aminet, Modules.pl, HVSC, AMP,
amigaremix.com, remix.kwed.org, radio stations, Shared Playlists = 9 sources. Table rows 1-9 match. Of those, 3 exist
(Modland, HVSC, Shared Playlists via `hippo`) and 6 are missing.

## 2. bbmp and other services

bbmp (the second parity target) is already researched in `thoughts/shared/research/2026-10-22_bbmp-sources.md`; the
first version of this plan missed that doc and marked bbmp UNVERIFIED. Primary source: the bbmp repo
(github.com/creep-ltx/bbmp), `src/bbmp/collection/*.py` and `docs/collections.md`. bbmp has six collections; Selecta
registers two of them (`sel_source_register_builtin` in src/library/source.c: local, modland, hvsc, hippo). The other
four appear only in the comment on `sel_source.id` in src/library/source.h.

| bbmp collection | Catalogue and transport (bbmp file) | Selecta |
|-----------------|-------------------------------------|---------|
| modland | `allmods_md5.zip` + `allmods.zip`, FTP + HTTPS mirrors (modland.py, transports.py) | exists (src/library/sources/modland*.c) |
| hvsc | Songlengths.md5 + STIL, hosted by Modland (hvsc.py) | exists (src/library/sources/hvsc*.c) |
| asma | JSON list, https://asma.atari.org `/asma/`, Atari SAP (asma.py) | missing: I413 |
| sndh | yearly `files/sndh<year>_lf.zip`, central directory read with 2 HTTP Range requests, https://sndh.atari.org `/sndh/sndh_lf/`, no md5 (sndh.py) | missing: I414 |
| zxart | paged JSON `/api/language:eng/export:{what}/start:{start}/limit:{n}`, https://zxart.ee, ZX AY and beeper (zxart.py) | missing: I415 |
| keygenmusic | JSON list, https://keygenmusic.tk `/kgm/` (keygenmusic.py) | missing: I416 |

Shared mechanics (base.py, docs/collections.md) already match Selecta's source layer: song key `<id>:<path>`, one
catalogue per source, md5-checked cache with LRU trim and kept songs, companions, prefetch, mirror ranking. Each new
source follows the modland/hvsc pattern (`<id>.c`, `<id>_net.c`, `<id>_sync.c`) and D-OS-7 (real fixture + PROVENANCE
row before the parser). On the Amiga each goes through the DEViLBOX mirror `/api/mirror/<source>/<path>` (design 16, D9).

Other services, beyond both parity targets. Candidate services, all "after HippoPlayer set":

| Service | Why | Endpoint knowledge |
|---------|-----|--------------------|
| The Mod Archive | Biggest module archive; the devilbox genre fixture already carries `source modarchive` | `api.modarchive.org` answered 200 with an empty body (PROBED); the documented XML API needs a key (UNVERIFIED, account/key question) |
| Demozoo | Scene productions, links to music | public JSON API at demozoo.org (UNVERIFIED, not probed) |
| Nectarine radio | Demoscene radio, long-running | stream list UNVERIFIED |
| SceneSat, other radio | Streams | UNVERIFIED |
| ZXArt, Bitfire, C64.com | Chip music beyond SID | UNVERIFIED |

## 3. Registry comparison

| Source id | Exists in src/library/sources | Missing |
|-----------|-------------------------------|---------|
| `local` | local.c | |
| `modland` | modland.c, modland_net.c, modland_sync.c | |
| `hvsc` | hvsc.c, hvsc_net.c, hvsc_sync.c | |
| `hippo` | hippo.c, hippo_prg.c, hippo_sync.c, hippo_publish.c | |
| `aminet` | | I405 |
| `modulespl` | | I406 |
| `amp` | | I407 |
| `amigaremix` | | I408 |
| `kwed` | | I409 |
| `radio` | | I410 |
| `modarchive` and others | | I411 |

## 4. Decisions

Johan's answers 2026-10-26 (OS-1 to OS-5):

- OS-1 order: the bbmp four first (I413 asma, I414 sndh, I415 zxart, I416 keygenmusic; bbmp's Python is the reference),
  then Aminet, Modules.pl, AMP, then amigaremix, kwed, radio, then the other services.
- OS-2/OS-3 Amiga: every source runs on Amiga and Mac, MP3 streams included. MP3 decode cost on 68020 still needs a
  measured number; it is a done-when on the MP3 rows, not a reason to drop them.
- OS-4 AMP: honest Selecta User-Agent; if still 403, mark AMP unavailable and ask the AMP admins for an endpoint.
- OS-5 radio: public directory (radio-browser.info is the candidate; check its terms before I410 starts), not a curated
  list.
- OS-6 Mod Archive key: default kept (defer to after the Hippo set, ask for a key then).


| ID | Decision | Why |
|----|----------|-----|
| D-OS-1 | One `sel_source` per service, own file pair `src/library/sources/<id>.c` plus `<id>_net.c` for fetching, registered in `sel_source_register_builtin`. No shared "web scraper" source. | Matches modland/hvsc/hippo (source.h:46-60). Each service has its own page format. |
| D-OS-2 | Fetch layer is the existing `sync_net.c` / `net/http.h` stack. Parsers are pure functions taking a buffer and a sink (`tests/` unit level), never touching the network. | Parsers testable from recorded fixtures (I140). |
| D-OS-3 | Services with a bulk index (Aminet INDEX, Modland allmods) are catalogue sources with `listing` and `LIBRARY_SYNC`. Services with only a search page (Modules.pl, AMP, amigaremix, kwed) are search sources and need the engine `SOURCE_SEARCH` command from D-YT-4 of the YouTube plan. Build that command once, in the first search source, and let YouTube reuse it. | One search protocol, not one per source. |
| D-OS-4 | Search sources return transient rows; a played or queued result is pinned into the catalogue (D-YT-3 pattern). | Same behaviour as YouTube. |
| D-OS-5 | HippoPlayer relies on UHCTools; Selecta does its own HTTP. We do not call UHCTools scripts. | Mac has none; Amiga stack already has HTTP/HTTPS (modland_net). |
| D-OS-6 | Aminet is limited to the module directories (HippoPlayer: "mods" subhierarchy) and only archives that unpack to a supported module. Archive step reuses `archive.c` (LHA). | readme:1077-1078. |
| D-OS-7 | Every endpoint marked UNVERIFIED gets a first sub-step "capture a recorded fixture and write it into PROVENANCE.md"; no parser is written against a guessed format. | Rule: confirm from a primary source. |
| D-OS-8 | Streams (radio, amigaremix, kwed MP3) play through the existing `sel_live_open` path from the YouTube plan, not download-then-play. Amiga support for MP3 stream depends on decoder cost (open question). | HippoPlayer streams too (readme:903-906). |
| D-OS-9 | Sites that block non-browser clients (AMP returned 403) are handled in the fetch layer with an honest `User-Agent` that names Selecta; if still blocked the source reports "unavailable" and stays registered but empty. No evasion of bot protection. | Scope and etiquette; ask Johan (question OS-4). |
| D-OS-10 | Order of work follows Hippo parity value then ease: Aminet, Modules.pl, AMP, amigaremix, kwed, radio, then the other services. | Questions file OS-1. |

## 5. Per-source plan

Common to all: tier "Amiga 68020" means the source runs in the 68k build over the existing net stack (TLS via the
platform library); "mirror route" means the devilbox proxy (`devilbox.uprough.net/api/...`) can fetch for a client that
cannot do TLS or parse HTML.

### 5.1 aminet (I405) - listing source
- Fetch: `INDEX` (about 1 MB; filter to the module dirs while streaming, never hold it whole on a 68020), file via `<dir>/<name>`; cache the filtered index in the catalogue (`LIBRARY_SYNC`), archives in the file cache and unpacked with `archive.c`.
- Amiga: fits. Filtered rows are a few thousand; stream parse in a small buffer. Mirror route: a devilbox `/api/aminet/list` returning the filtered rows if TLS is the blocker (UNVERIFIED need).
- Test: `selecta` top level, `source add aminet`, sync against fixture `tests/net/fixtures/aminet.net/INDEX` (first N lines, real), then play one `.lha` module from a fixture archive; sentinel = the aminet parser call counter. Parser unit tests: `tests/library/aminet_index_test.c`.

### 5.2 modulespl (I406) - search source
- Fetch: POST/GET search (UNVERIFIED), result HTML parse, download URL (UNVERIFIED). Stream `listen.pls` as a radio entry.
- Amiga: HTML parse is cheap; TLS needed (site served over https). Mirror route recommended if HTML parse grows.
- Test: `selecta` top level, `search modulespl chip` against a recorded response; parser unit tests `tests/library/modulespl_parse_test.c`.

### 5.3 amp (I407) - search source
- Fetch: UNVERIFIED entirely; site returned 403 (D-OS-9). First sub-step: find out whether a polite client is allowed (readme says Hippo reaches it, slowly over HTTPS).
- Amiga: Hippo manages it on an Amiga, so the endpoint works from a 68k client; slow.
- Test: as 5.2 with `tests/library/amp_parse_test.c`.

### 5.4 amigaremix (I408) and kwed (I409) - search sources, MP3
- Fetch: UNVERIFIED search endpoints; kwed list query string `?page=1&chart=&view=title&search=<q>` seen on the homepage. MP3 via `sel_live_open`.
- Amiga: depends on MP3 decode cost and stream buffering (open question OS-3).
- Test: as 5.2; parser tests `amigaremix_parse_test.c`, `kwed_parse_test.c`.

### 5.5 radio (I410) - station source
- Fetch: station directory UNKNOWN (UHCTools internals). Candidate: a small curated list file shipped in the repo plus Modules.pl `listen.pls`. Product choice (question OS-5).
- Test: `selecta` top level opens a stream from a local fixture server (`fixture.test`) and the sentinel shows `sel_live_open` ran.

### 5.6 other services (I411) - Mod Archive, Demozoo, Nectarine, SceneSat
- After the HippoPlayer set. Mod Archive needs an API key (question OS-6). One row per service is created when Johan picks.

### 5.7 parity check (I412)
- Done: the bbmp table in section 2 (from the bbmp repo and the 2026-10-22 research doc). Four missing collections became rows I413 to I416.

## 6. Tests

| Test | Where it lives | Proves |
|------|----------------|--------|
| One reachability test per source | `tests/m3/<id>_source` driving the `selecta` binary: add source, sync or search from the recorded fixture, play one result | Source registered (sentinel: `sel_source_get("<id>")` non-NULL and parser call count > 0), catalogue rows or search rows appear, playback starts |
| Parser unit tests | `tests/library/<id>_*_test.c`, fed fixture bytes | Row fields: path, size, title, URL, edge cases (empty result, HTML entities, ISO-8859-1) |
| Fixtures | `tests/net/fixtures/<host>/...`, each with a PROVENANCE.md row (real GET with date; synthetic marked as such) | No live network in tests (row I140) |
| Registry check | extend the existing source-registry test to list the new ids | F33 cannot be ticked while a source is missing |

F33 stays open until I405-I407 (the three named in the design doc) are done and ticked.
