# I211c: text-ui-folders-tree private song dir

Change: tests/m3/folders_screen.cmake copies selecta-test.mod and selecta-test-mk.mod into ${WORK}/songs and uses that as CWD / --start-dir (same pattern as tests/m3/meta_screen.cmake).

Fail-before: none. With the old script and a dirty shared songs dir (a/, b/, c/, album.stamp present), `ctest -R text-ui-folders-tree` passed alone. The golden shows only the folders view (opened with key f) and playback, not the directory listing, so it never depended on dir contents today. The private dir is still the fix (removes the shared-state dependency; start-dir no longer holds sibling-test output).

After: `ctest -R text-ui-folders-tree` passed alone; `ctest -L m3 -j1` (reason: order dependence) 60 of 60 passed, text-ui-folders-tree #39 of 60.
