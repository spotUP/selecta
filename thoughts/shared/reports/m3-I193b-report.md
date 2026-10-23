# I193b report: DEViLBOX lyrics store (server side)

Commit: 704b98302 on feat/selecta-links in /Users/spot/Code/devilbox-selecta (not pushed).

## Routes (server/src/routes/lyrics.ts, mounted in server/src/index.ts at /api/lyrics with a 300 per 10 min rate limiter, same as loudness)
- GET /api/lyrics/:md5: 200 raw LRC, Content-Type `text/plain; charset=utf-8`; 404 unknown; 400 malformed md5. Public.
- POST /api/lyrics/:md5: raw LRC text body (any content type except application/json, which the global json parser consumes first and is then rejected as 400). 204 on success; 401 no auth; 400 malformed md5, empty/blank body, or body over cap.
- Upsert: an existing row IS overwritten (last writer wins); user_id and updated_at record the last writer.
- Body cap: 64 KiB (65536 bytes; exactly 65536 accepted, 65537 gives 400, not 413).

## Table (server/src/db/database.ts, createLyricsTables, called from initDatabase)
song_lyrics(md5 TEXT PK CHECK length=32, lrc TEXT NOT NULL, user_id TEXT NOT NULL FK users(id) ON DELETE CASCADE, created_at INTEGER, updated_at INTEGER). CREATE TABLE IF NOT EXISTS, same style as createLoudnessTables.

## Auth reused
authenticateToken (Bearer JWT) from server/src/middleware/auth.ts:17, used exactly as PUT /api/loudness/:hash (server/src/routes/loudness.ts:64). No new scheme. Note: the "Selecta rating/vote push" routes are loudness (JWT) and /api/hip (device token); chose loudness JWT since it is the Selecta-specific one.

## Tests (server/src/routes/lyrics.test.ts, vitest, nice 19, maxWorkers 2)
- POST then GET round trip (content type, utf-8 body, overwrite)
- GET unknown md5 is 404
- malformed md5 x3 is 400 on GET and POST
- POST without/bad auth is 401 and stores nothing
- empty/blank/oversized body is 400, exactly-cap is 204
Result: lyrics.test.ts 7 passed; with loudness.test.ts 20/20 passed. `tsc --noEmit` in server: clean.

## Open
- Not run against the real server process or deployed; no migration of production DB needed (IF NOT EXISTS at startup).
- Selecta's fixture only shows GET; POST status 204 and JWT auth are my choice, confirm with the client upload code (I193b client side).
- No moderation/profanity or LRC format validation; any text up to the cap is stored.
