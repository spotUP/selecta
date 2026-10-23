# I161a: Modland network sync

## Design
`LIBRARY_SYNC modland <http(s) URL>` runs `sel_modland_net_sync` (src/library/sources/modland_net.c). It uses the work dir `<cache dir>/sync/modland`, removes stale `changelog_*` files, GETs `<base>/allmods.zip`, extracts the `allmods.txt` member to `allmods_md5.txt`, and GETs `pub/changelogs/changelog_YYYY-MM-DD.txt` for the 14 days before `now`. A 200 counts as fetched, a 404 as missing, and any other status fails the sync. It then calls the existing file-based `sel_modland_sync`.

`new_this_week` is persisted through the new `SEL_LIB_REQ_META` DB request (meta table, INSERT OR REPLACE) under key `modland.new_this_week`. It is written only after a successful modland sync, so a failed sync keeps the stored value. `selecta-cli library meta <key>` reads it back. `library stats` output is unchanged.

## Memory and platform
The zip is held in RAM and inflated from a memory buffer. The path is desktop only; the Amiga has no network sync path here and no Amiga build was attempted.

## Test
ctest `source-modland-net` (tests/net/modland_net.cmake), against the local fixture `sync.modland.test`. Expected values are counted from the fixture sources. Sentinel: `net_changelogs_fetched` and the fixture request log (1 zip request, 14 changelog requests). Expected songs = listing rows + added - deleted. Result: pass. `source-modland`, `cache-fetch` and the other source tests still pass.
