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
| asma.atari.org/asmadb/asmadb.js | https://asma.atari.org/asmadb/asmadb.js (the list endpoint bbmp's asma.py reads), fetched 2026-10-26, Last-Modified Sun, 11 Oct 2026 10:50:45 GMT | 2.2 MB JavaScript object trimmed to 2 of the composerInfos entries and 10 of the 6,400+ fileInfos entries (6 Aki SAPs, 2 TIATracker .ttt with escaped newlines in the comment, one three-author entry (Black_Flock), one with an empty author (Games/221B_Baker_Street)); groupInfos and demozoo sections dropped; each kept line is unmodified |
| sndh.atari.org/download.php | https://sndh.atari.org/download.php (the page bbmp's sndh.py reads for the newest archive), fetched 2026-10-26 | Verbatim, not trimmed (12,652 bytes; links sndh2024_lf.zip, sndh2025_lf.zip, sndh2026_lf.zip) |
| sndh.atari.org/files/sndh2026_lf.zip | https://sndh.atari.org/files/sndh2026_lf.zip, fetched 2026-10-26 by Range requests only: 105,124,396 bytes, Last-Modified Thu, 01 Jan 2026 09:37:39 GMT, 6,657 central-directory records (directory 696,049 bytes at offset 104,428,325) | NOT a complete archive: 14 real central-directory records copied unmodified (the sndh_lf/ and sndh_lf/Micro_Mr/ folder entries and 12 .sndh members, nested ones included), followed by a hand-built end-of-directory record (14 entries, directory offset 0, no comment). The file therefore starts with the directory; the sync reads only the directory, never the member data |

Synthetic (T69, I156): fast.mirror.test, slow.mirror.test (4 s first byte via routes.json), home.mirror.test and devilbox.uprough.net/api/mirror/modland carry copies of fixture.test/data.bin as Protracker/test/song.mod; they stand in for mirrors, not for real Modland content.

Generated (I168): fast.mirror.test/mods/Protracker/test/play1.mod and play2.mod are copies of the gen_testmod song (tests/songs/selecta-test.mod), so the engine can decode them; T77 prefetch plays one and prefetches the other.

Synthetic (I161a): sync.modland.test/allmods.zip holds allmods.txt in the real "<size><TAB><path>" layout (the 100 paths of tests/m3/modland_listing, sizes NR*1000); sync.modland.test/pub/changelogs/ has copies of tests/m3/modland_changelogs 2026-10-20 and 2026-10-22; every other date answers 404 like upstream.

Synthetic (I181): devilbox.uprough.net/api/genre/<key> answers for the four songs of tests/m3/genre (key = UADE key, hex8 xxh32 of the first 256 KB + hex4 of length): ambient and techno carry source modarchive, nogenre source untagged with genre null, down answers 503 via routes.json (genre stays unknown, uncached). The ambient/techno genre strings are invented, not Mod Archive data.

Synthetic (I177): ws.audioscrobbler.com/2.0/ answers every request with the last.fm success JSON `{"scrobbles":{"@attr":{"accepted":1,"ignored":0}}}`; the fixture key and secret in tests/m3/scrobble.cmake are invented, the real service is never contacted.
| zxart.ee/api/language:eng/export:zxMusic/start:0/limit:1000 and export:author/start:0/limit:1000 | https://zxart.ee/api/language:eng/export:zxMusic/start:0/limit:6 (6 real tunes: ids 19636, 19717, 19718, 19719, 19721, 19722) and export:author/start:N/limit:1000 (all 21 pages, 19,658 people), fetched 2026-10-26 with User-Agent bbmp, one request per second. The author file keeps only the 6 real author objects those tunes name (ids 2333, 3586, 7744, 13510, 19720, 205310); every kept object is unchanged. Edited fields: totalAmount (29731 and 19658 became 6 each) and limit (6 and 1000 became 1000) so the sync reads exactly one page of each; the real site holds 29,731 tunes in 30 pages. Note: robots.txt of zxart.ee disallows /api; bbmp reads the same API for the same purpose. |
