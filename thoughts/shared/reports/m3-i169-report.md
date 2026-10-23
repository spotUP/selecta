# I169 Smart downloads (F34, T77 part 3)

## Blocked on me
Nothing.

## Changed
- Prefs: `library.prefetch` (on), `library.metered` (off), `library.smart_downloads` (on; off on Amiga), `library.smart_playlist` (saved view name, default "smart"), `library.smart_top` (3, capped at 8).
- `sel_library_smart()` (src/library/db/fetch.c) plans uncached remote songs: first the saved view, then history ordered by play count. They queue behind the prefetch on the same low-priority thread. A prefetch request goes first and cancels a running smart download; a foreground fetch drops the queue. No smart download starts once the cache holds the budget (measured with the non-destructive trim walk).
- Engine `prefetch_sync`: prefetch off or metered on cancels everything and starts nothing (prefetch gets this pref gate too). A plan is made once per (queue version, playing entry, prefs generation).
- Counters `smart_requests smart_fetches smart_skipped` in the cache report.
- Test `smart-downloads` (tests/net/smart.cmake, tests/m3/smart.sel, tests/m3/smart.tsv) drives selecta-cli: defaults give smart_fetches=2 and both files in the cache; smart_downloads off gives 0; metered gives 0; smart_top 1 gives 1 (the view song); `@budget 1000` gives 0 with smart_skipped>=1.
- Results: host build clean, ctest `prefetch|cache|fetch|smart|prefs|engine|library` 15 of 15 pass (prefetch and prefetch-preempt stay green). Amiga build (68020) links.

## Found
- Nothing writes the history table yet (I170). `@mark history:<key>` in scripts seeds it via `sel_library_mark`; replace once SONG_ENDED writes history.
- Amiga: `library.smart_downloads` defaults off (no-thread path, one transfer blocks the single task). The cooperative `sel_library_prefetch_poll` handles smart items but is untested on the rig.
- Smart plans re-run only on queue version, playing entry or prefs generation change. A foreground fetch drops queued smart items without an immediate replan.
- `library.prefetch off` is covered through the shared gate, but no separate test asserts prefetch_fetches=0.
