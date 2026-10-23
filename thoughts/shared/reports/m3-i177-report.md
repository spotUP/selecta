# I177 report: SCROBBLE_DUE and the last.fm scrobbler (T95)

## What was built
- Engine (`src/engine/engine.c`): `scrobble_check()` emits `SEL_EVT_SCROBBLE_DUE` once per song after the position event. Rule: songs of 30 s or less never scrobble; otherwise due at min(duration/2, 240 s). A song with unknown duration (0) is due at 240 s. The `scrobble_due` counter is exposed in `sel_engine_stats`.
- Host (`apps/common/integration_host.c`, `include/selecta/integration.h`): `http_post` and `md5_hex` added to `sel_int_host`.
- Plugin (`plugins/integrations/lastfm/lastfm.c`): signed `track.scrobble` POST, offline queue file (tab separated, 500 entries max, batches of 50, atomic rewrite), flush on open, after each due event, and on poll with back-off. Prints `scrobble_due= scrobble_posts= scrobble_pending=` at close. `scrobble_posts` counts accepted posts only.
- Config: prefs `scrobble.lastfm_key`, `scrobble.lastfm_secret`, session key; env fallbacks `SELECTA_LASTFM_KEY/SECRET/SESSION/URL`, `SELECTA_SCROBBLE_QUEUE`, `SELECTA_SCROBBLE_ARTIST`, `SELECTA_SCROBBLE_NOW`. Off until key is set. No key is hardcoded.
- Artist is META_AUTHOR with a fallback pref; a song with no artist or title is skipped.

## Test (label m3, `scrobble`)
Driver `tests/m3/scrobble.cmake`, scripts `scrobble.sel`, `scrobble_flush.sel`. Cases: online post with an independently computed api_sig (string(MD5)) and sentinel counters; offline queues one entry; a second process drains it with the original timestamp; key unset means nothing queued or sent. Fixture host `ws.audioscrobbler.com` added (synthetic, noted in PROVENANCE.md); the fixture server now logs POST bodies.

## Reused and searched
Reused: `sel_http_request` (via the host), the net fixture server and routes mechanism, `md5.c`, the integration host/session, `engine_stats`. Searched for existing scrobble, md5, http-post and queue code before writing; none existed besides these.

## Limits
- The 240 s cap is not driven end to end: no test song lasts 240 s (mods end by themselves at about 26-30 s). It is covered by engine logic only.
- `selecta-cli lastfm login` is not built; key/secret/session come from prefs or env.
- `SEL_EXPECT_PLUGINS` raised 10 to 11 in `tests/CMakeLists.txt`; may conflict at merge with other worktrees adding plugins.
- Amiga build not verified. The plugin is desktop only; `integration_host.c` now uses `sel_http_request` and md5, whose Amiga link was not checked.
- `tests/ratchet/unimplemented.txt` unchanged (no T95 entry).
