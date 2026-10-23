# I188a report: DEViLBOX loudness endpoint

Branch `selecta-loudness`, commit c4abd1293, worktree ~/Code/DEViLBOX-loudness (from main ccd8e94f9). Not pushed, not deployed.

## Endpoints (mounted at /api/loudness, server/src/index.ts)

GET /api/loudness/:hash (public)
- :hash = 32 lowercase hex MD5 (uppercase is rejected as 400).
- 200 {"hash": string, "lufs": number, "peakDb": number, "method": "r128"|"rms", "count": number}
- 404 {"error":"unknown hash"}; 400 {"error": ...} on bad hash.
- Value = median (even count: mean of the two middle values) of lufs and of peakDb separately, over r128 rows; rms rows only if there is no r128 row. count = rows behind the reported method (not total submissions).

PUT /api/loudness/:hash (Authorization: Bearer <JWT>, authenticateToken)
- Body {lufs, peakDb, method, durationMs?, analyser?}
- lufs number in [-70, 0]; peakDb number in [-120, 6]; method "r128"|"rms"; analyser string <= 64; durationMs (extra check) non-negative integer.
- 200 same shape as GET (after the upsert). 400 {"error"} on validation, 401 without/invalid token.
- Upsert key (user_id, hash).

## Files
- server/src/db/database.ts: createLoudnessTables() (table song_loudness, PK user_id+hash, index on hash), called from initDatabase beside createHipVoteTables.
- server/src/routes/loudness.ts: createLoudnessRouter(db) factory (hip pattern) plus default export.
- server/src/routes/loudness.test.ts: 12 tests (404, 400, 401, PUT then GET, median of two users, r128 over rms and rms fallback, 7 validation cases).
- server/src/index.ts: mount, plus loudnessLimiter (300 requests per 10 min per IP, same style as genreLimiter; applies to GET and PUT).

## Verification
- loudness.test.ts: 12 of 12 pass. Pre-commit hook ran 6 affected server suites, 68 tests pass.
- Server tsc --noEmit: no errors in touched non-test files. Test files show pre-existing "Cannot find module vitest" errors because vitest lives in the root package. Root `npm run type-check` not run (root node_modules is a symlink to the main checkout's).

## Notes for the Selecta client
- A rate-limit hit returns 429 with a plain text body.
- No foreign key to users: rows survive user deletion.
- Needs a DEViLBOX JWT (30 day) from the existing auth routes.
- Worktree has an untracked symlink node_modules -> ~/Code/DEViLBOX/node_modules (gitignored) so the pre-commit hook runs; remove with the worktree.
- Merge to main deploys live; whoever merges must do so deliberately.
