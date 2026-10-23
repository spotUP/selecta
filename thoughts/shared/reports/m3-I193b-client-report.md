# I193b Selecta client: upload a local .lrc after a definite 404

Design: D-M3-13 / F19. Server contract: DEViLBOX commit 704b98302 (`POST /api/lyrics/<md5>`, text/plain, Bearer, 204).

## Behaviour
- `lyrics_open` (src/engine/engine.c): a song with a non-empty local .lrc and `library.lyrics_online` on now also queues the
  library job with `local_lrc = 1`. The answer is not used (the local file wins); `lyric_fetches` counts it.
- `sel_lyrics_job_run` (src/library/db/lyrics_fetch.c): GET via new `sel_dvb_lyrics_get_ex` (reports the HTTP status). Only
  on HTTP 404 (transport errors give status 0) it calls `lyrics_upload`: needs `sel_devilbox_token`, the .lrc non-empty
  and at most 64 KiB, and the md5 not yet sent this session (64-entry table). Then `sel_dvb_lyrics_put`.
- `sel_dvb_lyrics_put` (src/library/devilbox_lyrics.c): POST, `Content-Type: text/plain; charset=utf-8`,
  `Authorization: Bearer`, built like `sel_dvb_loudness_put`.
- Pref off: engine queues nothing, so no network. No token: no POST, silently.
- API change: `sel_library_lyrics_fetch(lib, path, local_lrc)`.

## Tests
`lyrics` (tests/net/lyrics.cmake, driven by `selecta-cli script` through the engine, fixture log as call count):
- existing cases updated: ambient.lrc now makes 1 GET (3 requests in the first script, fetch counters cumulative).
- 3a token + local .lrc + GET 404: exactly 1 POST; path, content type, Authorization and body (bytes, length) checked;
  reopening the same song later in the session: GET again, no second POST (4 requests total).
- 3b no token: 1 GET, 0 POST. 3c GET 200 (token, local .lrc): 0 POST. 3d pref off with token: 0 requests.
Result: `ctest -R 'lyrics|lrc'`: lrc-unit, lyrics, fixture start/stop all passed (host). selecta-cli `commands --unimplemented | wc -l` = 8.

## Open items
- "Definite 404" is HTTP status 404 whatever the body (the fixture's unrouted 404 has none); a 404 from a proxy
  that is not DEViLBOX would also trigger one upload attempt, harmless (401/400 without effect).
- The dedupe table is process-wide static, keyed by md5, only the one job thread (or inline caller) touches it.
- The worktree needed `git submodule update --init --reference` for third_party (not committed).
