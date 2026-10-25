# Gaps in 2026-10-24-m4-amiga-ui.md (ledger I342, pass 2026-10-25)

Method: plan headings and sections 1, 3.3-4.5, 6, 8, 8a read; code read: `plugins/ui/desktop/amiga/backend_rtg.c` (pick_mode, default size, open, IDCMP), `src/uimodel/layout.c:29`, ledger rows I124, I286, I287, I340-I343. Nothing built or run. Phases 7 to 12 beyond the new items, and sections 9 coverage, were not re-audited.

| ID | Gap | Where | Filled how |
|----|-----|-------|-----------|
| G1 | Canvas size source (Workbench screen via LockPubScreen, 18acad6/I340) not recorded | 4.2 | edited (4.2 sentence, M4-P5-8) |
| G2 | Plan 4.2 says `p96OpenScreenTags` and "mode id"; code uses `OpenScreenTags` + `SA_DisplayID` found by size, depth minimum 16 | 4.2 | edited (4.2 sentence) |
| G3 | Below 700 px (`layout.c:29`) only the 40 px strip; 640x400, 680x270, AGA 640x256/200 get no full pages (I343) | 3.2, 6 | item M4-P5-6 + question Q24 |
| G4 | `pick_mode` exact-size only; no fallback, no failure path to the user (`backend_rtg.c:88,167`) | 4.2, 6 | item M4-P5-7 + Q25, Q26 |
| G5 | Default "Workbench mode id" contradicted by code (size only) | 4.2 | item M4-P5-8 |
| G6 | Hover/live-loop fix (4940399, I341) has no rtg reachability test | 6 P5 | item M4-P5-9 |
| G7 | Close/Ctrl-C/Quit paths unspecified; I287 shows screen left open, no stats | 4.4, 6 | item M4-P5-10 (cause still unknown: needs measurement) |
| G8 | Keys and mouse with a populated library (I287) untested | 6 P5 | item M4-P5-11 |
| G9 | IDCMP set in code lacks NEWSIZE/ACTIVEWINDOW the plan relies on (`backend_rtg.c:181`); wheel, repeat, qualifiers unspecified | 4.4, M4-P7-4 | item M4-P5-12 |
| G10 | Low memory behaviour and buffer numbers missing | 4.2 | item M4-P5-13 (numbers w x h x 4; limit for fallback to be measured) |
| G11 | AGA has no INDEX8 golden dependency stated (I286, DUI-P6-4); `backend_aga.c` absent | 4.3, P6 | item M4-P6-7 |
| G12 | Real mode lists of the rigs and the Replay not recorded | P0 | item M4-P0-4 (needs measurement) |
| G13 | AGA screen size choice and small-screen layout | 4.3 | item M4-P6-8 |
| G14 | Stutter I124: underrun asserts rely on an AHI sink that does not exist | P5-4, P5-5, P6-5 | item M4-P6-9 (AHI assertions wait for I124) |
| G15 | Workbench start has no stdout; error messages invisible | 4.4 | item M4-P7-6 |
| G16 | "One runtime tier, modern bar" not stated or tested; AGA vs RTG differences not listed | 4.5 | edited (4.5 paragraph) + item M4-P11-7 |
| G17 | Rig P5-2/P5-3/P5-5 name only `rtg-060` and `a1200-aga-020`; Replay and FS-UAE rig 2 (uaegfx, I338) are the real RTG targets today | 1 | not edited: rig config naming is Johan's; recorded here |
| G18 | Plan text still contains the word "double" in 11 places (buffering, click) | various | not edited (rename would touch established terms) |

Open questions added (section 8): Q24 compact layout design (recommended: one column with bottom tab bar), Q25 mode fallback (largest fitting mode), Q26 failure report (requester then text UI), Q27 smallest supported screen (640x200). Section 8a has no answers for them yet.

Counts: 18 gaps; 3 filled by edit alone (G1, G2, G16 partly), 12 by new items (14 items M4-P0-4, P5-6..13, P6-7..9, P7-6, P11-7), 4 questions raised (G3, G4, G15 partly, smallest screen); G7, G10, G12 need measurement; G17, G18 left. Plan total now 84 items, 52 in M4; `tests/ratchet/unimplemented.txt` (23 lines) is not updated and must be regenerated.
