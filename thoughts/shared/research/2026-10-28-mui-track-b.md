# MUI track B progress (ledger I638, plan 2026-10-25-amiga-mui.md section P8)

Branch: worktree-agent-a48b66d9ed5a16d37. Host tests only; no rig touched.

| Item | Status | Commit | Test |
|---|---|---|---|
| MUI-P8-1 | [~] host done, native button row unbuilt, rig check pending (track A) | 04cd8417 | mui-p8-transport-extras (revert-checked) |
| MUI-P8-2 | [~] host done, native F-key route into mui_ui.c not wired, rig check pending (track A) | see git log "MUI-P8-2" | mui-p8-voices-programs (revert-checked) |
| MUI-P8-3..15 | not started | | |

Notes
- MUI-P8-2 created a new shared model `sel_uim_fkey_*` (include/selecta/uimodel/fkeys.h, src/uimodel/fkeys.c): F<n> = PLAYLIST_LOAD F<n> then PLAY, Shift+F<n> = PLAYLIST_SAVE F<n>. DUI-P5-8 was still open; the desktop can reuse it. Program naming is Johan's call.
- MUI-P8-15 (alarm) needs new engine scheduler work (ALARM verb is in the k_exempt list).
- Waiting for rig check: P8-1 and P8-2 native halves.
