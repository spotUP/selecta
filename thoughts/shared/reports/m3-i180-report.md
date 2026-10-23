# I180 report: SHARE verb, link parse, DEViLBOX URL form (D-M3-11)

## Blocked on me
- DEViLBOX URL layout is my assumption. I139 found no song or playlist URL on devilbox.uprough.net, only `?view=`. I used `https://devilbox.uprough.net/?song=<enc source/path>[&sub=<n>][&t=<ms>]`, `?playlist=<enc source/id>`, `?view=<enc query>`. Confirm or give the real form; the change is confined to `parse_devilbox` and `sel_share_format_devilbox` in `src/library/share.c`.

## Changed
- New verb `SHARE song|position|playlist|view` (commands.def, engine.c). It answers two LIBRARY_RESULT lines: `share` (selecta:// form) and `share_devilbox`.
- `OPEN` accepts a share link. Song links open at `sub` (0-based) and seek to `t` (SEEKED reports frames, 5000 ms = 220500). View links run PLAY_QUERY. Playlist links load local playlist files. Malformed links answer `ERROR bad_link`.
- `selecta <link>` opens each link argument at start; `selecta-cli open <link>`, `share make`, `share parse`, `share position` added.
- Sentinels: engine stats `links_parsed`, `links_made`; `selecta` exit line prints `links_parsed= links_made=`; the cli prints `SHARE links_parsed= links_made= songs_opened=`.
- Codec in `src/library/share.c`; test T84 `share-links` (`tests/m3/share_links.sh`) green. It drives `selecta-cli` and the real `selecta` binary. Affected set (protocol, ratchet, stage, cues, folders, browse, user-data, stickers, scrobble, play-query; 17 tests) green. Host and Amiga 68020 builds clean.

## Found
- Fixture `selecta-test-mk.mod` has one subsong, so `sub=2` is verified only as a codec round trip, not by playing it.
- Playlist links for non-local sources answer `no_such_playlist` (no remote playlist store yet). Remote song links with `sub`/`t` are not tested offline.
- `source.h` cannot be included next to `playlist/playlist.h` (typedef conflict); share.h exposes `sel_share_key` and `sel_share_key_split` wrappers instead.
