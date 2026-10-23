# Fixture provenance

All files fetched 2026-10-23 by HTTP GET. None are synthetic. JSON and .prg cannot carry comments, so notes live here.

| File | Source | Note |
|---|---|---|
| ftp.modland.com/changelog_2026-10-22.txt | https://ftp.modland.com/changelog_2026-10-22.txt | full |
| ftp.modland.com/allmods.txt | https://ftp.modland.com/allmods.zip, member allmods.txt | first 40 lines |
| hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5 | https://hvsc.c64.org/download/C64Music/DOCUMENTS/Songlengths.md5 | first 41 lines |
| hvsc.c64.org/download/C64Music/DOCUMENTS/STIL.txt | same dir, STIL.txt | first 60 lines |
| hippoplayer.se/api/v1/playlists/index.json | https://hippoplayer.se/api/v1/playlists | 12 of 77 entries |
| hippoplayer.se/playlist/LMan_SID_Chip_Club.prg | https://hippoplayer.se/playlist/LMan_SID_Chip_Club.prg | full, 1017 bytes |
| devilbox.uprough.net/api/health/index.json | https://devilbox.uprough.net/api/health | full |
| devilbox.uprough.net/api/modland/search_q_high.json | /api/modland/search?q=high&limit=2 | full |
| devilbox.uprough.net/api/modland/list_virgill.json | /api/modland/list?dir=pub/modules/Protracker/Virgill | first 30 files |
| devilbox.uprough.net/api/songdb/lookup_notfound.json | /api/songdb/lookup?hash=000000000000 | full |
| devilbox.uprough.net/api/hip/votes_400.txt | /api/hip/votes | body `ERR bad`, HTTP 400 |

Synthetic (T69, I156): fast.mirror.test, slow.mirror.test (4 s first byte via routes.json), home.mirror.test and devilbox.uprough.net/api/mirror/modland carry copies of fixture.test/data.bin as Protracker/test/song.mod; they stand in for mirrors, not for real Modland content.
