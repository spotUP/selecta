# Serial merge + build, 2026-11-20 (all PASS, not pushed)
1. I1474 (on main): build OK. nppanel, mui-nowplaying, integration-nowplaying, dui-nowplaying-panel, dui-bar-nowplaying, text-ui-* PASS.
   dui-nowplaying: golden nowplaying-rgba32-1280x800@1.hash c687f0ed522cbbfe -> ff696f6a6aa52733 (2a73c108).
   dui-hover-nowplaying failed, NOT caused by I1474 (also fails with user.c reverted): pointer moved at 300 ms, before the panel heart hover box existed on a fresh scan. Fix 016e60d0: times 2000/2400 ms in tests/dui/hover_nowplaying.cmake. PASS.
2. I1465d: merge b0599076. source-modland-net (+ modland tests) PASS.
3. I1465c: merge 2a462e34. v27 was free (main was 26). library-schema-tx, -final, -dup-rebuild-empty, -db-open, amiga-new-library PASS.
4. I1465b: merge 0563c4f3. library-deepsid-rows/-list/-key/-csdb-thumb, uim-deepsid-tiles, deepsid-fetch/-cover PASS.
5. I1473m: merge 63d2cd6d. mui-canvas-geom PASS; mui-grid hash 336dd8f26e917634 -> 125de398c54038aa RE-BLESSED (define lives in tests/CMakeLists.txt, folded into the merge commit). PASS.
dui-ratchet PASS. dui-ratchet-zero fails by design (label dui-red, DUI-P7-2 not implemented), unchanged.
Scratch: ~small only in scratchpad/h (<10 MB); no large files. todos.md has uncommitted main-session edits, untouched.
