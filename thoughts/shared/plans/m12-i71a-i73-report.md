# M1.2 rows I71a and I73 report

## I71a random play order (test T30a queue-random-order)
- Red: before the engine change `ctest -R queue-random-order` failed (`Test #37 ... Failed`, 0% passed). Green after: passed.
- Expected file is the real output; its playing= sequence equals an independent model (xorshift32 Fisher-Yates, seed 7): 1,3,2,5,4,0,4,5,4,5,3,2,6,1,0,1,2.
- Semantics chosen: the order is rebuilt lazily when the queue version changes; the played prefix is kept, the unplayed tail is reshuffled. Scope album skips entries outside the album and leaves them unplayed. PLAY after a finished round starts a new round. PREV walks the played prefix. RESHUFFLE with random on reshuffles the unplayed tail and does not move rows.
- `SHUFFLE on [seed]` seeds the engine generator (no separate seed option exists); without a seed it uses clock_ms ^ rng, like QUEUE_SHUFFLE.
- SHUFFLE signature is now `ss?` (mode, optional seed).
- sel_queue_entry gained an appended `flags` field (SEL_QUEUE_FLAG_DIVIDER, SEL_QUEUE_FLAG_HIDDEN).
- Root-cause fix in src/core/codec.c: a trailing `?` made the argument before it required (`required = siglen - 1`); it is now `siglen - 2`, so `?` means the last argument is optional as documented in commands.def. Affects every verb with `?`; all codec/script tests still pass.

## I73 PLAYLIST_LOAD path [mode], PLAYLIST_SAVE path
- Test: `playlist-verbs` (tests/m12/playlist.sel, fixtures list.prg and rel.m3u copied into build/host/tests/songs/b). The ratchet already listed both verbs as unimplemented (ERROR not_implemented) before; no separate failing run was recorded for the script.
- Load appends (one undo record), format by extension, relative keys resolved against the playlist directory (URLs, Unix absolute and Amiga volume paths kept). Divider rows get SEL_QUEUE_FLAG_DIVIDER and are skipped by NEXT/PREV/auto-advance; stealth rows get HIDDEN; subsong n plays replayer subsong n-1.
- Save writes dividers/stealth/subsong; M3U/PLS/XSPF log `playlist lossy divider`.
- Deviation: the plan does not define `mode`. Implemented `append` (default) and `replace` (clears first, same undo record); anything else is `ERROR range`.
- Errors: open_failed (file), bad_format (unknown extension on save / unreadable content), queue_full (does not fit).
- Scratch buffers (entries, text, file) are allocated once in sel_engine_create. No new .c files, object count unchanged.
- Ratchet lowered 33 -> 31.

## Checks
Whole host suite (-E coreaudio): 45 of 45 passed. build/amiga green.
