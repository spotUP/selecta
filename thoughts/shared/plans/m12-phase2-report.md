# M1.2 Phase 2 report (I68..I71)

Branch worktree-agent-af221bacbf31cdca2. Commits: 1e84407 (I68 red tests), 40c7e1a (I69), 624e73e (I70), then the I71 commit.

Gate 2 evidence (red before code): `28 - queue-history (Failed)`, `29 - queue-scope-album (Failed)`, 0% passed.
Green now: queue-history (T30), queue-scope-album (T31), ratchet (T8: 45 -> 41 -> 38 -> 35 by my verb counts), full ctest 43 of 43.

## Design
- History: `src/engine/history.c`. Undo ring of 64 id snapshots, redo stack of 64. Removed entries go to a dead table with its own path arena so UNDO can restore them. If the dead table fills, history is dropped (the edit stays, not undoable). All memory is allocated at engine create.
- QUEUE_HISTORY `undo redo` is emitted after every QUEUE_CHANGED; `tests/m1/queue.expected` updated for that.
- MOVE/SWAP take entry ids. MOVE puts the entry at the position the target id holds now.
- Shuffle: `src/engine/shuffle.c`, xorshift32 + Fisher-Yates over rows after the playing one. Seed 0 maps to 0x9E3779B9. RESHUFFLE reseeds with xorshift of the last state.
- SCOPE option (library|album|playlist; playlist behaves as library in M1); album = parent directory of the path. NEXT and auto-advance stop at the album end under scope album.

## Deviations and open points
- PREV_ALBUM follows the T31 numbers: it plays the first song of the previous album from any song of the current album. The plan Decisions text (go to the album's first song when not on it) conflicts; the test wins. User must pick.
- SHUFFLE takes one optional string (on|off), per commands.def signature "s?"; no seed argument. Seeded shuffles use QUEUE_SHUFFLE <seed>.
- SHUFFLE on/off only sets the `random` option; NEXT does not yet consume a random play order (no play-order structure exists). Not done; needs a decision and its own test.
- The history.expected `@position` placeholder was row=50; actual is row=0 after one second (equal before and after UNDO, the property tested). Corrected.
- UNDO/REDO with nothing to do report `ERROR nothing_to_do`.
