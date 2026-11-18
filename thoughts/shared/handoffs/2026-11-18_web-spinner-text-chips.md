---
date: 2026-11-18
topic: Web hang fix, cover download spinner, text search chips, red tests on main
status: in-progress
---

# Handoff 2026-11-18

## Merged to main today (unbuilt as a whole)
- 5ef262d5 shadow fix in desktop_ui.c (I1458 build break).
- aa93dd74 dui-search-all expects folded Internet Archive section.
- f2cd2718 web UI branch (I694a): stack overflow hang fixed (-sSTACK_SIZE=8MB, 293a8bdf). Page still blank, canvas 300x150. Next: -sOFFSCREENCANVASES_TO_PTHREAD=#canvas + log SDL renderer; check desk_wait_turn yields. Report agent-reports/I694a-web-build-4.md.
- 66a9e200 cover spinner during download (I1459). Test dui-cover-download-spinner green in agent worktree. Risk: fetch flag is global, any background scrape spins now-playing cover. Report agent-reports/I1459-download-spinner.md.
- 42993399 text UI search chips (I1456 text twin), shared sel_uim_chip_toggle / sel_uim_search_group_shown in src/uimodel/features.c; desktop_ui.c calls them (logic equivalent, reviewed). Test text-ui-search-chips green in agent worktree. I1454/I1455/I1459 text twins exempt (rule 9, text UI draws no cover). Report agent-reports/I1454-6-text-twins.md.

## Red on main (as of 5ef262d5 run)
- dui-search: golden hash stale (432ba11de0469e75 vs aa4d662031a94d75), last blessed ca36d016 before I1456/I1458. Rebless: in build/sdl `SELECTA_GOLDEN_UPDATE=1 nice -n 19 ctest -j1 -R '^dui-search$'`, rerun without, commit .hash.
- dui-cover-slideshow (I1460): mp3 plays; slideshow never switches (cover_slide of=3 switches=0). Suspect deferred song-open series 5e7b1595 05b0e0e8 96e73adc b9120fcb ec27597e 765b27ab 14c9baad. Fix idea: scripted `wait` settles pending deferred open (desk_side_settle I787 pattern). Report agent-reports/I1460-mp3-play.md.
- dui-search-all: test fixed in aa93dd74, not rerun.
- dui-search-online-quit: 3054 ms vs 3000 ms limit, rerun on idle machine.

## Next, in order
1. Ask Johan for build go. Build main through buildq, run affected tests (above + dui-cover-download-spinner, text-ui-search-chips, dui-search-badge).
2. Rebless dui-search, fix I1460, tick I1454-I1458.
3. Relaunch newest Selecta (kill old Selecta GUIs first) and give Johan Mac steps: search "hyperbased" with Turran FTP chip (song names show); hover a search row (no "..." over source badge); toggle Modland + AMP together; play song with several stills (cover rotates, spinner while downloading/decoding).
4. MUI 3.8 twins of I1454/I1455/I1456/I1459 not started.
5. Older open items: R14 unmerged (agent-a409871ebc216f5d8, 31307463), I774r question, Amiga rebuild from main, confirmed Modland+AMP example for Johan. Side finding: song.mod/wav give no_replayer in build/sdl (unverified).

## Notes
- Main is far ahead of origin; no push without Johan's go.
- No agents running at handoff.
