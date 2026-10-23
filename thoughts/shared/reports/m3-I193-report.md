# M3 I193: lyrics (T93)

Row I193, "Lyrics: LRC, song message, DEViLBOX lyrics, current line in the snapshot". Done when T93 is green.

## What changed

- `src/library/lyrics.c`: pure LRC parser (`sel_lrc_parse`, multi-stamp lines, `[offset:]`, BOM, sorted), `sel_lrc_index_at`, `sel_lrc_path`. Integer only.
- `src/library/devilbox_lyrics.c`: `GET <devilbox>/api/lyrics/<md5>`; HIT, UNKNOWN (404 "unknown hash"), MISSING (any other failure).
- `src/library/db/lyrics_fetch.c` plus a `job_lyric` branch in `job.c`: the request runs on the library job thread, never on the pump or audio thread. Without threads (Amiga) it runs inline.
- `src/engine/engine.c`: `lyrics_open` picks the source once per song path (a seek reopens the song, so the key is the path, not the replayer state). Precedence: `.lrc` next to the song, then the tracker song message, then the endpoint. Snapshot fields: `lyric_source`, `lyric_text`, `lyric_lines`, `lyric_line`, `lyric_current`. Local files are read on the pump thread.
- `include/selecta/prefs.def`: `library.lyrics_online`, default `off`.
- `apps/selecta-cli`: `@lyrics` script directive with the sentinels `lyric_source= lyric_line=` and `devilbox_missing=lyrics`.
- Tests: `lyrics` (tests/net/lyrics.cmake driving `selecta-cli script tests/m3/lyrics.sel` and `lyrics_off.sel` over the fixture, with request counts read from the fixture log) and `lrc-unit`.

## T93 results

- At 12.5 s the LRC line index is 3 (golden), `lyric_fetches=0`.
- Song message from `message.it` shows as plain lyrics, `lyric_fetches=0`.
- Song without a local file takes the fixture endpoint lyrics, synced (index 1 at 2.6 s).
- Song with no route: `devilbox_missing=lyrics`, `lyric_source=none`.
- Pref off: no request reaches the fixture.
- Fail before: with the baseline binary the script printed `unknown directive @lyrics` five times and `ERROR unknown_pref library.lyrics_online`.
- `ctest -L m3`: 49 of 49 pass. Amiga build compiles.

## Open points

- The endpoint `GET /api/lyrics/<md5>` is invented: the route is absent upstream (thoughts/shared/research/2026-10-25-m3-external-endpoints.md line 24). The shape is 200 text/plain (LRC or plain text), 404 "unknown hash" for an unknown song. It needs agreeing with DEViLBOX.
- The spec says the endpoint is used when there is no local file. It is opt-in here (`library.lyrics_online`, default off) because the song hash leaves the machine. Flip the default if that is not wanted.
- Plain song messages are not split into synced lines; `lyric_line` is -1 for them.
