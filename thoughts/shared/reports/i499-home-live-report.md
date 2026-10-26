# I499 Home page live window: report

## Quick picks missing on live window
Cause: not a bug. src/library/db/user.c home_text emits the Quick picks heading and grid only when history joined with songs has a played song with a non-empty album (max 8 albums by latest play). Goldens use fixture names with no history, so they never showed Quick picks either. Johan's live db had 1 history row (a song with no album), so no grid was expected. Reproduced with a copy of the live db after a second history row on an album song: dump shows `quick 0 cols=4 Iron Garden|Iron Garden|Mono Lake`.
Open product decision: fall back to recently added albums when there is no play history. No code change made.

## Stray "4" at top-left
Not reproduced in the mem backend at scale 1 (1280x800) or scale 2, live db. No text drawn at origin in desktop_ui.c. I480 recorded it gone at bc4f444. Live-only cause (SDL backend, stale dirty rect, old build) not ruled out. No fix, so no regression test.

## Live data path test (done)
tests/dui/home.cmake section 12 plus fixture tests/dui/fixtures/art-music (3 albums x 2 mp3, embedded 64x64 jpg APIC). Drops the folder into the real scan entry point, asserts 6 songs, 3 cover artwork rows, then with one history row asserts quick_count 1, the quick dump line and shelf_album_art_draws >= 1.
Revert check: with APIC stripped from the fixture the test fails: "one cover per album: got '0', expected '3'". Fixture restored.
Suite: ctest -L dui -LE dui-red, 56 of 56 pass.
