# M3 external endpoints: facts (rows I139 to I141)

Date: 2026-10-23. Sources: DEViLBOX server source (read-only) `/Users/spot/Code/DEViLBOX/server/src`, hippoplayer.se source (read-only), and live GET of public listings. No POST, no login.

## DEViLBOX server (https://devilbox.uprough.net)

Routes mounted in `server/src/index.ts` lines 81-99. Auth: `Authorization: Bearer <jwt>` (`middleware/auth.ts:18-26`, JWT secret from env, 30 day expiry at line 38).

- GET /api/health (index.ts:102): `{status:'ok',timestamp}`. Live OK.
- POST /api/auth/login (routes/auth.ts:83): body `{username,password}`; returns `{token,user:{id,username,createdAt}}`; 400 `{error:'Username and password required'}`, 401 `{error:'Invalid username or password'}`. POST /api/auth/signup (auth.ts:18), username 3-20 chars.
- PUT /api/ratings (routes/ratings.ts:49), auth: `{source:'modland'|'hvsc',itemKey,rating:1-5}` returns `{ok,avg,count,userRating}`.
- DELETE /api/ratings (ratings.ts:83), auth: `{source,itemKey}` returns `{ok,avg,count,userRating:null}`.
- GET /api/ratings/batch?source=&keys=a,b (ratings.ts:115), optional auth, max 200 keys: `{ratings:{key:{avg,count,userRating?}}}`. Live unknown keys: `{"ratings":{}}`.
- GET /api/genre/:key?name=&size=&md5= (routes/genre.ts:19): key 12 hex, name required (max 255). Returns `{key,genre|null,source:'modarchive'|'untagged'|'miss'|'ambiguous'|'unsupported',modarchiveId}`.
- GET /api/songdb/lookup?hash=<12hex> (routes/songdb.ts:17): `{found:true,format,channels,authors[],publishers[],album,year,subsongs[]}` or `{found:false}`. GET /api/songdb/status.
- Modland (routes/modland.ts): GET /status, /formats (live: `{"formats":[{"format":"Protracker","count":80767},...]}`), /search?q=&format=&author=&limit<=100&offset= (line 70) returns `{results:[{id,format,author,filename,full_path,extension,avg_rating,vote_count}],limit,offset,query}`; /list?dir= (line 155) returns `{files:[{full_path,filename,extension}]}` immediate children only; /download?path=pub/modules/... (line 175) proxies ftp.modland.com (nearest thing to a mirror route); POST /lookup-hash `{hash: sha256 64 hex}`.
- HVSC (routes/hvsc.ts): /stats (live `{"totalFiles":80000,"totalSize":53687091200}`, hardcoded), /featured, /browse?path= (line 79), /search, /download?path=.
- Playlists (routes/playlists.ts): POST /api/playlists (line 18, auth) `{playlistId,name,description,visibility,tracks,environment,totalDuration}`; GET /api/playlists?mine=&limit<=200&offset= (line 78; live `{"playlists":[],"total":0}`); GET /api/playlists/:id (line 131); PUT /api/playlists/:id/visibility (line 178); DELETE /api/playlists/:id (line 206).

### Missing from server source (D-M3-13: degrade to local-only, record `devilbox_missing=<name>`)

- `hip_vote`: /api/hip/vote and /api/hip/votes are NOT in the local source. Live GET /api/hip/votes answers 400 text/plain `ERR bad` for every parameter name tried (hash, m, md5, h, id, mod, key). Parameters unknown. /api/hip/vote not called (may mutate).
- `loudness`: /api/loudness/<hash> live 404, absent from source.
- `lyrics`: absent.
- `comments`: absent.
- `mirror`: /api/mirror/<source>/<path> absent; use /api/modland/download and /api/hvsc/download.
- Share URLs: `src/App.tsx` handles only `?view=`, `#/_view=`, `?embed=`, `?_isolate=`. No song or playlist URL form exists.

## hippoplayer.se

Source: `cygnus-server/src/server/PlaylistRoutes.ts`.
- GET /api/v1/playlists (line 144): public JSON array `{id,title,author,genre,filename,uploaddate,url}`, no auth. Live: 77 entries.
- GET /api/v1/playlists/:filename (line 154) and GET /playlist/:filename: raw `.prg`, begins `HiPPrg`.
- POST /api/v1/playlists?title&author&genre&filename (line 159): raw `.prg` body, must start `HiPPrg`, max 524288 bytes; reply `OK <id> <filename> <url>`. See `thoughts/shared/research/2026-10-22_playlist-formats-and-hippoplayer-server.md` section 2.
- Legacy: /api/uhcplaylists (line 116), /api/playlists (line 199).
- /api/hip/vote is not in hippoplayer.se either.

## Upstream file indexes (live, 2026-10-23)

- https://ftp.modland.com/ root: `allmods.zip` (5.76 MB), `changelog_2026-10-22.txt`, `mirrors.txt`, `readme_updates.txt`, `readme_welcome.txt`. `allmods_md5.zip` does NOT exist (404 at root and pub/modules). `allmods.txt` inside the zip has lines `<size>\t<path>`, no md5.
- Changelog lines: `moved: <path>` followed by `     to: <dir>`; `updated: <path>`; `deleted: <path>`.
- HVSC `https://hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5` (5.2 MB): `[Database]` header, then `; /PATH/file.sid` comment line followed by `<md5>=<m:ss ...>`. `STIL.txt` is v85, 3.75 MB.

## Test net guard contract (I141, T67)

Fixture server `tests/net/fixture_server.py` serves `tests/net/fixtures/<host>/`. Engine network code must honour:
- `SELECTA_NET_ALLOW=127.0.0.1`: only listed hosts reachable; others fail with `net_blocked=1`.
- `SELECTA_NET_MAP=<host>=http://127.0.0.1:<port>`: host remapped to the fixture server; reported as `net_mapped=1`.
- `SELECTA_CA_FILE`: CA bundle for the TLS listener.
Provenance of each fixture: `tests/net/fixtures/PROVENANCE.md`.
