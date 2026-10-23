# I164 report: PLAYLIST_PUBLISH (T74 publish part)

## What ran
- `PLAYLIST_PUBLISH <title> <author> <genre> <filename>` (signature changed from `s` to `ssss`) exports the queue as a Hippo `.prg`, refuses over 512 KB (`ERROR too_large`, no request), POSTs, parses the reply and emits LOG `publish posts=<n> id=<id>` and `publish_url <filename> <url>`.
- Code: `src/library/sources/hippo_publish.c/.h`, `cmd_PLAYLIST_PUBLISH` in `src/engine/engine.c`.
- Test `hippo-publish` (`tests/net/hippo_publish.cmake`) drives `selecta-cli script` over the fixture (`tests/net/fixtures/hippoplayer.se/routes.json`). Small list: sentinel `posts=1 id=181`, exactly 1 POST in the fixture log, query holds title/author/genre/filename, posted body sha256 equals the `.prg` PLAYLIST_SAVE wrote (starts HiPPrg). Oversized list (5600 entries, over 512 KB): `ERROR too_large PLAYLIST_PUBLISH`, 0 POSTs.
- Numbers: unimplemented commands 17 -> 16 (ratchet file set to 16, PLAYLIST_PUBLISH off the list). Full host ctest without vamos/rig: 126 of 126 pass.

## Root-cause fix found on the way
`sel_engine_create` cast the FAST_RAM tag (uintptr_t) to uint32_t: a host with 4 GB, 8 GB or 16 GB wrapped to 0 and got the 256-entry / 32 KB queue instead of 8192 / 2 MB. Now saturates. On this 16 GB host script sessions now have the large queue (the full suite still passes). No separate regression test: the oversized-list case in `hippo-publish` fails without it (queue_full at 256 entries).

## Assumed about the real service (UNPROVEN, all constants in hippo_publish.h)
- URL `https://hippoplayer.se/api/v1/playlists/publish`, method POST, override env `SELECTA_HIPPO_PUBLISH_URL`.
- Query keys `title`, `author`, `genre`, `filename`, percent-encoded; body = raw `.prg`; Content-Type application/octet-stream.
- Reply one line `OK <id> <filename> <url>`; any non-2xx or other reply is a rejection.
- 512 KB limit and that it counts the body bytes.
- The fixture route is synthetic and mirrors these assumptions; it proves the client, not the service.
- Real service is HTTPS; the Amiga build has no TLS, so publish cannot reach it there.

## Leftovers
- The parsed reply is only emitted as LOG lines; it is not stored into a playlist/catalogue row.
- Publish uses the queue as the playlist.
- No live test against hippoplayer.se.
- Amiga build not run: no m68k toolchain on this host (`m68k-amigaos-gcc` missing). The new code is portable C.
