# Gaps in 2026-10-24-desktop-ui.md (ledger I342)

Filled in place unless noted. Count: 17 gaps; 9 filled by edit (checklist items or fact fixes), 8 left as open questions in section 7 (Q9 to Q17; some are paired with an item).

| # | Gap | Section | How filled |
|---|---|---|---|
| 1 | Hover in the live loop: clock current at input time, end state drawn (4940399, I341) | 5 Phase 3 | edited: DUI-P3-7 |
| 2 | hover_repaint repaints only sidebar and main (desktop_ui.c:4436-4446); player footer, detail panel, top bar not covered | 5 Phase 3 | edited: DUI-P3-8 |
| 3 | Wheel frame rate and dirty area on all scrolling surfaces (I310, I314) | 5 Phase 3 | edited: DUI-P3-9; speed and momentum in Q10 |
| 4 | Widget states (disabled, focus, trap, Escape) unspecified | 2.3 / 2.8 | edited: DUI-P3-10; disabled tooltip in Q11 |
| 5 | Song context menu only on some pages (I305) | 5 Phase 4 | edited: DUI-P4-16 |
| 6 | Text overflow and truncation rules, list rows sized to room (I307) | 2.6 / 5 | edited: DUI-P4-17 |
| 7 | Remove from Library and Clear Library (I313) | 5 Phase 4 | edited: DUI-P4-18; scope in Q16 |
| 8 | Liked-state heart and rating (I306) had no item | 5 Phase 4 | edited: DUI-P4-19; row heart in Q13 |
| 9 | Layouts under 700 px drop to the 40 px mini strip, no compact layout (I343) | 3 / 5 | edited: DUI-P4-20; size and navigation in Q14, Q15 |
| 10 | Empty states, loading states, error toasts absent | 5 | edited: DUI-P4-21; toast time Q12, copy Q17 |
| 11 | Plan contradicted by code: breakpoints 1100/900 (D-DUI-2) vs layout.c:21-33 using 1200/900/700 | 3 / 5 | edited: DUI-P1-11 (code changes to the plan) |
| 12 | Missing numbers: start time, resize time, artwork cache cap, thumb sizes, memory, delays | 2 / 4 | needs measurement: DUI-P6-7 |
| 13 | Hover and press fade durations unnamed ("fast", "normal") | 2.9 | question Q9 |
| 14 | Golden set did not cover the compact layout | 4.2 | edited: +8 hashes (65 total) |
| 15 | Counts: 61 items became 73; count check and finish line updated | 1 / 5 | edited |
| 16 | Reachability: DUI-P3-7 and P3-9 must run through the live loop (sdl offscreen), not the mem step helper `desk_advance` | 5 | edited into the tests |
| 17 | Existing ticked items P3-5, P3-6, P4-1 to P4-5 have tests but no hover/wheel live-loop coverage | 5 | covered by P3-7, P3-9 |

Not checked (budget): per-view keyboard behaviour of each page, section 8 coverage tables.
