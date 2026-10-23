# M3 I174: VOTE command (T80)

Design doc: `thoughts/shared/plans/2026-10-22-architecture-design.md`, feature table row "Remote" (line 58, DEViLBOX votes inline), commands list (line 176, `VOTE`), server-gaps note (line 395, vote endpoints unverified).

## Changes
- `VOTE [0|5]` implemented in the command table (`src/engine/engine.c`, `cmd_VOTE`, added to `SEL_CMD_IMPLEMENTED`). 5 votes, 0 withdraws, no argument reads. Song key = UADE key (xxh32 of file head + length) of the open file. Own choice stored locally as sticker `vote` first, so a missing service loses nothing.
- Token: 128 random bits (32 hex), made once, pref `library.vote_token` (`prefs.def`), saved to the config dir.
- Request runs on the library job thread (`db/vote_push.c`, `db/job.c`); inline on the no-thread (Amiga) build. Single slot: a newer request replaces a pending one. Engine command thread never blocks.
- HTTP: `sel_devilbox_vote` (`net/devilbox.c`): `GET /api/hip/vote?k=&m=&r=` and `GET /api/hip/votes?m=&k=`. 400 -> bad, 429 -> rate, 404 -> missing (`devilbox_missing=hip_vote`). A failed request keeps the last count for the same song.
- `sel_uade_key_file` / `sel_file_head` moved into `xxh32.c`; genre sync uses them (static `read_head` removed).
- Script directive `@votes` (waits, prints `VOTES vote_posts= status= count= yours= m=`); stat `vote_posts`.
- Test `vote-devilbox` (`tests/net/vote_devilbox.cmake`, `tests/m3/vote.sel`, `vote_missing.sel`): own fixture server and log, independent Python xxh32 computes expected `m`, asserts exact request paths (k, m, r), parsed count/yours, the ERR rate case, withdraw, and the 404 case with the local sticker.

## Unimplemented count
Before 9 (ratchet file), after 8 (`selecta-cli commands --unimplemented | wc -l`). `tests/ratchet/unimplemented.txt` lowered to 8.

## Tests (nice 19, host build/host)
Pass: vote-devilbox, rate-devilbox, stage-commands, net-guard, cache-fetch, cache-fetch-shell, ratchet. Amiga build (build/amiga) links.

## Open items
- Plan T80 says the count is "read back into the playlist row". Not done: the count lives in the library handle and is printed by `@votes`; no playlist-row column exists yet.
- Server vote endpoints are unverified against the real service (design doc line 395); fixture follows the I139 protocol.
- Regression check by reverting the fix was not run.
