---
date: 2026-10-22
topic: Playlist formats the player must read/write, HippoPlayer .prg format, hippoplayer.se playlist server API
tags: [playlist, hippoplayer, prg, m3u, hippoplayer.se, research]
status: final
---

# Playlist formats and the HippoPlayer playlist server

Paths: HiP = /Users/spot/Code/HippoPlayer-68k/code/puu016.s (master tree, no .git there; the Share/POST code is NOT in this tree, only on a branch, see 1.6). H = /Users/spot/Code/hippoplayer.se. Source files are Latin-1: grep needs `-a`/`LC_ALL=C`.

## Summary
- HippoPlayer's playlist ("module program", ".prg") is plain text: `HiPPrg\n\n` then one entry per line. Entry = path or http(s) URL, optionally followed by `#name=<display>` and/or `#song=<n>`. A divider line starts with a magic byte. M3U is read (not written) in a limited form.
- An entry's identity is its path/URL string. There is no id, hash or source field in the file. Subsong is the single digit after `#song=`.
- hippoplayer.se serves shared playlists from MySQL via an open API (no auth): list JSON, fetch raw .prg, POST raw .prg with metadata in the query string. A legacy tab-separated feed serves the 68k client through UHC tools.
- Voting is NOT on hippoplayer.se: it is `GET https://devilbox.uprough.net/api/hip/vote` and `/api/hip/votes` (DEViLBOX server), keyed by device token + module content hash. Server code not found on disk (see Unverified).
- Eagleplayer 2.04 has no playlist format. M3U/PLS/XSPF/Winamp/DeliTracker notes are from my own knowledge.

## 1. HippoPlayer 68k (.prg)

### 1.1 Layout
- Header: `"HiPPrg",10,10` (HiP:13523, `prgheader`; written at 13554-13561 via dos Write).
- Load check: first 4 bytes `HiPP` and bytes 4-5 `rg`, else requester "Not a module program!" (HiP:13133-13136, error string 13245-ish `unknown_module_program_error`). Header skipped by reading to the first LF then one more byte (HiP:13137-13139), so it assumes exactly two LFs.
- Body: one entry per LF-terminated line. CR is dropped on load (MS-DOS line ends accepted; readme line 587). Lines of 5 bytes or fewer are skipped unless a divider (HiP:13209-13220). Max entries MAX_MODULES = $1ffff = 131071 (HiP:118; readme line 61). No trailing count, no index.
- Entry kinds on load: starts with `http` = remote node (`configRemoteNode`, HiP:~59210); anything else = local path (name shown = part after last `/` or `:`, `nimenalku` HiP:13420-13432).
- Markers (parsed in the line-copy loop, HiP:13262-13305, and configRemoteNode HiP:59224-59255):
  - `#song=<d>`: `<d>` is ONE character, low nibble used (`and.b #$f,5(a3)` -> `l_favSong`), so subsongs 1-15 (0 = none). Written as `$30|n` (HiP:13617-13624), so n>9 would write a non-digit; unverified how >9 behaves.
  - `#name=<text>`: display name for a remote URL entry; text after it is the visible name, the URL is truncated at `#` (HiP:59224-59255). Writer emits it when `l_separateName` is set (HiP:13585-13594).
- Dividers: line starting with DIVIDER_MAGIC (Latin-1 0xF7; UTF-8 form c3 b7 accepted) or DIVIDER_MAGIC_ALT (Latin-1 0xA2; UTF-8 c2 a2) (HiP:138-140, `isDividerInA3` 13369-13412). The ALT form = "stealthy" divider (`l_divider` set to $f0). Writer prefixes 0xF7 for normal dividers (HiP:13572-13574, writes `DIVIDER_MAGIC`; the ALT/stealth form is NOT re-emitted, so a stealth divider is saved as normal: read from code, not tested).
- Encoding: no marker. Files seen are Latin-1 or UTF-8 (devilbox-r2f handoff); HiP treats bytes opaquely, only dividers have both forms.
- Compression: loader has an XPK path ("Could not load XPK compressed module program!", HiP:~12895-12930, `xpk_module_program_error`). Details not read; unverified layout.
- Extension: `.m3u` (by file extension, `getFileExtension4`) routes to M3U import (HiP:12929-12935, also 31819, 31868). `.prg` is the convention in the server (filename gets `.prg` appended).

### 1.2 Save
- `exportModuleProgramToFile` (HiP:13542-13650): opens (mode 1006 = new), writes header, then per list node: optional divider prefix, `l_filename` (full path/URL), optional `#name=...`, optional `#song=n`, LF. Buffer 1000 bytes per line (for long radio URLs). Save dialog "Save module program" (ReqTools), text "Saving module program..." (HiP:13445-13470). Variant `exportModuleProgramToFileWithSongMetaData` (d7=1, HiP:13535) is used for the favorites list: always writes `#song=`.
- For the normal list `#song=` is written only if the node is a favorite with a nonzero favorite song and that differs from the favorites-list entry (HiP:13597-13625).
- Callers: HiP:35340 (favorites save), 36406 (user save).

### 1.3 Load
- `loadprog`/`rloadprogDoIt` (HiP:12666-12690, ~12930), engages normal list mode first. Import core `importModuleProgramFromData` (HiP:13119): inputs a2 = list header, a3 = data, a4 = end. Callers also at 31872, 35271, 36256.
- `importModuleProgramFromDataSkipHeader` (HiP:13100): no `HiPPrg` check, optional per-line prefix (base URL) and filter callback; used for search results (`T:searchresults`, HiP:58698) and M3U (HiP:13050-13070). M3U filter drops lines whose first char is `#` (so `#EXTM3U`/`#EXTINF` are skipped, only URL/path lines kept; HiP:13064-13070). Readme line 487: "Limited support for .m3u ... if they contain urls".
- Per-entry data model (`l_*` fields): `l_filename`, `l_nameaddr`, `l_divider`, `l_favSong`, `l_separateName`, `l_remote` (HiP:~1701).

### 1.4 Play modes ("program" semantics)
- `playmode` constants (HiP:1715-1723): `pm_through`=2 (play list once), `pm_repeatmodule`=3, `pm_module`=4 (play module once), `pm_random`=5, `pm_max`=5; lower values (0,1) are list repeat modes (lines before 1715 not read). Random uses a table (`randomtable`, `randomValueMask`, HiP:1422-1423, 8170-8192, 8390+). The mode is a prefs/runtime setting, not stored in the .prg.
- "Module program" in the UI = the playlist the user built; Prg button / Load / Save module program. Favorites is a separate list in its own file (same format) (readme line 782).

### 1.5 Search/shared lists on the 68k side
- "Shared playlists" search runs `uhcmirrorsearch ... playlistsearch` (UHCSearch against UHC mirrors: `http://<mirror>/uhcsearch/?searchtype=playlistsearch`), result in `T:searchresults`; the 68k then fetches the chosen file as `<base>playlist/<file>` with aget (notes/thoughts/shared/research/2026-10-18-playlists-first-row-dropped.md, "Correction" section; HiP:59848-59955 base "http://asciiarena.se/").
- Index format returned: header `Filename\tPath\tTitle\tAuthor\tGenre`, LF, rows `filename,"playlist",title,author,genre`, Latin-1, trailing LF (same research note; H cygnus-server/src/server/playlistFeed.ts:42-46).

### 1.6 Share/vote branches (not in master tree)
- Share: branch playlists-submit-stacked, tip 47c1999, PR #38 koobo/HippoPlayer: after Save Program the user can Share; builds a hand-written HTTP/1.0 POST to hippoplayer.se port 80 via bsdsocket (details: /Users/spot/Code/HippoPlayer-68k/thoughts/spot/playlist-post.md sections 6-7; patches in thoughts/spot/superseded-branches/backup-playlists-submit/*.patch not read, only listed).
- Vote: /Users/spot/Code/HippoPlayer-68k/thoughts/spot/handoff-2026-10-22/pr-vote.md: thumbs-up icon, ctrl+v toggle, random device token in S:HippoPlayer.prefs, liked list S:HippoVotes.tsv, module key = same content hash as the genre lookup, info window "Votes:" line.

## 2. hippoplayer.se playlist server
Source: H cygnus-server/src/server/PlaylistRoutes.ts (375 lines), PlaylistStore.ts, playlistFeed.ts, playlistSubmit.ts; spec H thoughts/shared/api/playlists-v1.md (status in its frontmatter: implemented, not yet deployed as of 2026-10-18; /playlist/test6.prg was live per the 2026-10-18 rig note, so deployed since).

### 2.1 Public API v1 (no auth, CORS `*`, http and https)
| Method + path | Notes |
|---|---|
| GET /api/v1/playlists (PlaylistRoutes.ts:144) | JSON array newest first: `{id,title,author,genre,filename,uploaddate("YYYY-MM-DD"),url}`; url = `https://hippoplayer.se/playlist/<filename>` |
| GET /api/v1/playlists/:filename (:154), GET /playlist/:filename | raw .prg bytes, application/octet-stream, 404 unknown |
| POST /api/v1/playlists?title&author&genre&filename (:159) | body = raw .prg, any Content-Type, Content-Length needed. Body must be non-empty and start `HiPPrg` (playlistSubmit.ts:52-57). Reply one Latin-1 line + LF: `201 OK <id> <filename> <url>`; 400 `ERROR empty body...`/`ERROR not a HiPPrg file...`, 413 `ERROR file too large...`, 409 `ERROR no free filename...`, 500 `ERROR server error`. Success iff reply starts `OK `. |
- Limits: body 524288 bytes (playlistSubmit.ts:11), nginx 1m; title 200, author 100, genre 100, filename 200 chars; control chars -> space; non-Latin-1 -> `?`; query accepts UTF-8 or Latin-1 percent escapes, `+` = space (playlistSubmit.ts:15-27, 59-82). Filename: last path part, `? # % & * < > |` and quotes removed, spaces -> `_`, `.prg` appended, collision -> `name-2.prg`... up to 1000 tries. Port-80 POST deliberately not redirected to https (nginx/hippoplayer.se.conf:288-298).
- Never overwrites; submitter owns nothing (no user column).

### 2.2 Legacy feed (byte-identical to old asciiarena.se)
- GET /uhcplaylists.php (and /api/uhcplaylists, PlaylistRoutes.ts:116): tab-separated index (see 1.5); `?l=<n>` newest n rows (`parseIndexLimit`, playlistFeed.ts); `?file=<name>` one file. GET /playlist/<name> and /api/playlist/:filename (:129). asciiarena.se proxies old URLs here.
- GET /api/playlists (:199): tab list with full URLs `https://hippoplayer.se/cygnus/api/playlists/file/<name>`, no header row.

### 2.3 Admin (session cookie + Admin rank, used by H JS/PlaylistEditor.js)
- GET /api/me (:63) `{loggedIn,id,nick,rank}`; GET /api/playlists/list (:224) JSON; GET /api/playlists/file/:filename (:241); POST /api/playlists multipart `file` (:254, `_requireAdmin`); PATCH /api/playlists/:id (:284); PUT /api/playlists/:id/file (:313); DELETE /api/playlists/:id (:339). Request body shapes of PATCH/PUT not read.
- POST /api/check-urls (:78): URL reachability proxy (body shape not read).

### 2.4 Storage
- MySQL database `hippoplayer`, table `playlists` (PlaylistStore.ts:1-60): `id INT AUTO_INCREMENT, title VARCHAR(200), author VARCHAR(100), genre VARCHAR(100), filedata MEDIUMTEXT, filename VARCHAR(200) (indexed), uploaddate INT (unix)`. `filedata` holds the file as `data:...;base64,<b64>` (`encodePlaylistData`/`decodePlaylistData`, playlistFeed.ts:20-26). Migrated from asciiarena `uprough_ascii.hippo_playlists` with ids kept (src/tools/migratePlaylists.ts).
- Web client: H JS/HippoPlayer.js (submit call at ~10358-10362 per playlist-post.md; not re-read), JS/HippoPlayer_static.js:718-728 builds the submit URL.
- hippoPlayerHarness.ts (cygnus-server/src/server/hippoPlayerHarness.ts) is a test harness stubbing the player UI (PlaylistListBox, UhcSearches playlistsearch BasePath https://hippoplayer.se/, lines 69-75); no API of its own.

### 2.5 Vote API
- Not in H. Per PR text only: `GET https://devilbox.uprough.net/api/hip/vote` and `/api/hip/votes`; keyed by device token + module hash. Parameters and response shapes: UNVERIFIED (grep of /Users/spot/Code/devilbox-r2f/server/src and H cygnus-server/src found no route).

## 3. devilbox-r2f TypeScript importer
Files: /Users/spot/Code/devilbox-r2f/src/lib/dj/hippoPlaylist.ts (154 lines), tests in src/lib/dj/__tests__/, /Users/spot/Code/devilbox-genre/tools/convert-hippo-playlists.ts.
- Decode: try strict UTF-8, fall back to ISO-8859-1 (`decodePrgBytes`, :37-43).
- Parse (`parsePrgEntries`, :55-74): split on `\r?\n`; line 0 trimmed must equal `HiPPrg`; only lines matching `^https?://` are kept (local paths, blanks and 0xF7 divider lines are dropped); marker regex `#(name|song)=` (a bare `#` is part of many Modland names, e.g. "superchip #2.xm"); `url` = text before the marker; `#name=` captured up to next `#`. `#song=` is dropped (handoff next-step 3: 7 entries affected, e.g. Monkey Island subsongs).
- Source mapping (`sourceForUrl`, :87-100):
  - `^https?://ftp.modland.com/(pub/modules/.+)$` -> `modland:<%-decoded path>` (converter checks the Modland index table `modland_files.full_path` to choose raw vs decoded; unresolved keep URL path).
  - `^https?://(www.)?hvsc.c64.org/download/C64Music/(.+)$` -> `hvsc:<decoded path>` (checked against `hvsc_files.fullname`).
  - `https://hippoplayer.se/WB.html?playmod=<url>` (TAWS link) -> recursion on the module URL.
  - anything else -> the URL itself (http(s) URL is a valid source).
- Track fields: fileName = source, trackName = marker or basename, format = upper-cased extension (`.psid` -> SID; none -> MOD), bpm 0, duration 0. Playlist name = filename minus `.prg`, `_` -> space; id = `playlist-prg-<lowercase, non [a-z0-9.] -> '-'>`.
- Tests: hippoPlaylistKeepsEverySource.test.ts (every URL line becomes an entry carrying its source; 6 hippoplayer.se entries of "Anathema-Imbasille 3"; ids keep dots; bundled seed has no bare entry). hippoplayerSourcesLoadThroughTheProxy.test.ts (old `/data/Modules` URL maps to `/webdata/Modules`; proxy fetches over https; refuses unserved hosts and path escapes). Handoff 2026-10-11: 74 files, 4681 URL entries: 3591 Modland, 626 HVSC, 151 hippoplayer.se, 105 uhc.amigaremix.com, 100 uhc.remix.kwed.org, 99 www.modules.pl (zips), 1 files.scene.org (404), 8 live radio streams.
- Note: that corpus is URL-only; local Amiga paths (e.g. `Rushhours:Music/...BIG.MOD`, seen in the test6 playlist) are dropped by this importer.

## 4. Eagleplayer 2.04 (/Users/spot/Downloads/Eagleplayer2.04)
- Contents: Include, Readme.txt, Src. Grep for playlist/Playlist/EPPL/.pls (case-insensitive, whole tree): no playlist hits in Readme.txt or Src. Only hit for "EPPL" is `dc.b 'EPPLAYER'` identifier in Include/misc/DeliPlayer.i:171, the player-module header magic, not a playlist. Conclusion: no playlist format in this package. (Searched by text; binary files skipped.)

## 5. Other formats (from my own knowledge, not checked against sources)
- M3U/M3U8: line-based list of paths/URLs, relative paths resolved against the .m3u's directory. Extended form starts `#EXTM3U`, with `#EXTINF:<seconds>,<artist> - <title>` before each entry; `.m3u8` is UTF-8, plain `.m3u` is system codepage (Latin-1 in practice). Extra `#` lines are comments; many players add `#EXTALB`, `#EXTGENRE`, `#EXTVLCOPT`.
- PLS: INI-style, `[playlist]`, `File1=`, `Title1=`, `Length1=` (seconds, -1 unknown), `NumberOfEntries=N`, `Version=2`. Index-numbered, so identity is path + position.
- XSPF: XML, `<playlist version="1" xmlns="http://xspf.org/ns/0/"><trackList><track><location>` (URI, percent-encoded), `<title>`, `<creator>`, `<album>`, `<duration>` in ms, `<annotation>`, `<identifier>`, `<extension>` for app data. Paths must be file: URIs.
- DeliTracker/DeliPlayer: I recall no standalone playlist file with a documented layout in DeliTracker 2; DeliPlayer used a "prg"/list save with its own text form, but I cannot confirm layout. Unverified.
- Winamp/foobar: Winamp reads/writes M3U/M3U8/PLS (also `.b4s` XML); foobar2000 reads M3U/M3U8/PLS/CUE and has its own binary `.fpl` (proprietary, not documented) and subsong addressing by `file.ext|subsong` style special URLs (foo_input_* components, unverified detail).

## Format table
| format | read | write | identity of an entry | notes |
|---|---|---|---|---|
| HippoPlayer .prg (HiPPrg) | HiP yes (13119); devilbox importer URL lines only; hippoplayer.se stores verbatim | HiP yes (13542); hippoplayer.se stores verbatim | path or URL string; subsong via `#song=<1 digit>` | `HiPPrg\n\n` header, `#name=` for URL display name, 0xF7 dividers, Latin-1/UTF-8 mixed, max 131071 entries |
| M3U / M3U8 | HiP limited (by `.m3u` extension; `#` lines skipped, so no EXTINF titles) | not found in HiP | path/URL | readme 487: URLs only supported |
| hippoplayer.se legacy index (TSV) | HiP via UHCSearch | server | filename | `Filename\tPath\tTitle\tAuthor\tGenre`, Latin-1 |
| hippoplayer.se API v1 | web client, other clients | POST raw .prg | filename (server-unique), numeric id | no auth; JSON list; one-line text reply |
| devilbox seed JSON | n/a | converter output | `fileName` = `modland:`/`hvsc:`/URL source | per-track fields in section 3 |
| Eagleplayer | no playlist format found | - | - | - |
| PLS, XSPF, DeliTracker list, .fpl | from knowledge only | - | see section 5 | not verified here |

## Open questions / unverified
- XPK-compressed .prg layout and the exact loader code (HiP ~12880-12930) not read.
- `#song=` with subsong > 9: the writer stores `$30|n`; behavior for n >= 10 unchecked.
- Stealth divider (0xA2) round trip: writer appears to emit only 0xF7.
- Playmode values 0 and 1 (list repeat) and the prefs storage of playmode not read.
- Vote endpoints (parameters, responses, key hash algorithm) not found on disk; only the PR description.
- The Share POST code (bsdsocket) lives on branch playlists-submit-stacked (tip 47c1999), not in the working tree; the patches in thoughts/spot/superseded-branches/backup-playlists-submit/ were listed, not read. Line refs for it come from playlist-post.md.
- Admin PATCH/PUT request body shapes and POST /api/check-urls not read.
- Whether the API v1 is deployed in production: spec says "not yet deployed" (2026-10-18), later rig note shows v1 list and /playlist/test6.prg answering live.
- DeliTracker playlist format and foobar `.fpl` details: knowledge only.
