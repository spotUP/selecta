---
date: 2026-10-25
topic: Selecta autonomous finish run, session handover before account switch
tags: [handoff, desktop-ui, m3d, i266, i270]
status: draft
---

# Selecta finish run: session handover (2026-10-25)

## Task(s)

- Goal from Johan: finish all of Selecta.
- Finish line: I202 (finish-line run) is green, and every ledger row is closed except the Johan-only rows I138, I203 and I248.
- Ledger: `thoughts/spot/todos.md`. Count at handover: 331/391. I266 is not ticked yet.
  - Recount: `grep -E '^\| I[0-9]+[a-z]? \| \[' thoughts/spot/todos.md | awk -F'|' '{t++; if($3~/x/)d++} END{print d"/"t}'`
- Current phase: M3d desktop UI, Phase 4 (plan `thoughts/shared/plans/2026-10-24-desktop-ui.md`, lines ~227-241).
  - Ticked: P4-4, P4-5.
  - Partial, with remainders in the ledger: P4-6 to P4-11 and P4-13 (I256-I261, I264, I269).
  - Open: P4-12 tracker tabs (test dui-tracker), P4-14 settings from prefs.def (test dui-settings), P4-15 mini mode (test dui-mini).
- M4 Amiga UI plan: 0 of 37 done; it starts after M3d.

## State at handover

- Branch main. Origin (spotUP/selecta, private) is at 5eb4dea.
- Local main is ahead of origin and NOT pushed:
  - 1dbe525 ledger: Johan's visualizer look check in I269
  - 240963d ledger: search field ring cause in I266
  - e35b76b ledger: new row I270
  - bd68454 I266: Now Playing page, page history, Song Information door, one search-field rect
- The "Push now" standing approval from Johan covers main pushes. Push after the I266 follow-up merges.
- Running at handover: one sonnet worktree agent (I266 follow-up), worktree `.claude/worktrees/agent-a61fc50669663529a`, branch `worktree-agent-a61fc50669663529a`.
  - If its report is lost in the account switch, check `git log main..worktree-agent-a61fc50669663529a`. Merge any finished commit; respawn the brief below for anything missing.
- Worktree `.claude/worktrees/agent-aa732a611176357ff` (b725b89) is of unknown origin. Do not remove it.
- Live window: I launched selecta pid 15740 myself with the binary from before I266.
  - Started with `SELECTA_SOCKET=<scratchpad>/live/ctl.sock`, `--db <scratchpad>/live/live.db`, `--view viz`.
  - The old session scratchpad is `/private/tmp/claude-501/-Users-spot-Code-MusicPlayer/d21f2148-ebf4-461d-8876-9851ed290fe3/scratchpad`. A new account gets a new scratchpad; that is fine.
  - Kill only that pid when relaunching. Never kill Johan's own selecta windows.

## I266 follow-up (in flight)

bd68454 landed with these gaps:
- No revert-checks for the tests/dui/nav.cmake assertions.
- The Search door highlight bug was never reproduced on the old code.
- No assertion that clicking the Home search field keeps the query.
- No unit test for the new `sel_gui_round_frame` (include/selecta/gui/gui_draw.h, src/gui/draw/gui_draw.c).
- apps/selecta/main.c ~507 has an empty `if (own) {}` branch (skips shell VIEW sends for the desktop UI with --view home/library/playlists/songs). Tidy it.

Four tests fail on main after bd68454. Logs are in the old scratchpad (m266-host-test.log, m266-sdl-test.log, m266-3.log). It is unknown whether they fail before bd68454.
- dui-viz: Timeout 120 s (SDL build). It passed at f5b3050 (P4-11 merge). Suspect: `--view viz` now goes through desk_page_apply in plugins/ui/desktop/desktop_ui.c.
- source-archive: "FAIL browse pack.zip: expected songs/ and readme.txt" (tests/m3/source_archive.sh).
- hash-idle-vamos: "Internal Vamos Error: [alloc: NO MEMORY for 04d000 bytes]" (tests/m3/hash_idle_vamos.cmake:33).
- rp-config-taglist-vamos: "Internal Vamos Error: FreeVec: Unknown memory to free" (tests/m2/lib.cmake:115). The ledger note at line ~618 says it passed at 6b9d32a.
- build/amiga/apps/selecta/selecta had a 13:57 timestamp I did not build. Another session may have rebuilt it; check before blaming bd68454.

The agent brief covers: run the 4 tests at bd68454 and bd68454~1; fix any regression at the root with a test; revert-check each nav fix; add the query-kept assertion; add a round_frame unit test (not -L dui). After merge: tick I266 (python3 -I edit of the ledger), commit, push.

## Next steps (ordered)

1. Merge the I266 follow-up (merge pattern below). Tick I266. Push.
2. Relaunch the live window on the new binary (own socket and db in the scratchpad, in the background). Ask Johan to check: search field ring and caret, back/forward arrows (top right of main pane), Now Playing page, right-click then Add to Queue, Now Playing strip.
3. Spawn I270: button and chip labels are left-flush (Playlists page buttons at desktop_ui.c ~1619, Search facet chips in draw_search). Fix: one centred-label helper (sel_gui_text_width, ellipsize) used by every button and chip. Needs a stats-based centring test, a revert-check, and moved goldens.
4. P4-12, P4-14, P4-15, I265(a) Sources tab. Only one desktop_ui.c agent at a time. Fold in I244, I253-I255, I256-I261, I264, I267, I269.
5. I251 FS-UAE probe (own rig, only when other sessions' rigs are idle), I230 slices 2-3.
6. I202 finish-line run.

## Merge pattern

1. `git cherry-pick <agent commit>` onto main.
2. Load gate must be 0: `LC_ALL=C ps -Ao command | grep -E '^(ctest|cmake --build|/opt/homebrew/bin/ninja|.*vamos)' | grep -v grep | wc -l`
3. Build: `nice -n 19 cmake --build <dir> -j2` (build/host, and an SDL build configured with -DSELECTA_UI_DESKTOP=ON).
4. Test the affected set only: `nice -n 19 ctest -j2 -R <affected> -LE dui-red < /dev/null`, log to the scratchpad.
5. Worktree cleanup: make sure third_party/nuade and third_party/libopenmpt are empty dirs (not symlinks), then `git worktree unlock`, `git worktree remove`, `git branch -D`.
6. Ledger edit with python3 -I, commit (no attribution lines), push. Check `ssh -G github.com | grep -i serveraliveinterval` shows 60 first.
- The Bash PreToolUse hook refuses pushes or tests while an emulator or test is busy. Never bypass it.

## Agent brief template

Name the plan section and line. "Reference design: Spotify desktop". Read RULES.md and CLAUDE.md. Reuse first. Rebase onto main. Symlink rule for third_party (never commit symlinks, restore empty dirs). Configure its own build dirs. Load gate, nice, -j2, `< /dev/null`, logs to the scratchpad with a prefix. Affected tests only. Revert-check every fix. Commit as spotUP, add files by name, no Co-Authored-By and no Claude-Session lines. No push. No thoughts/ edits. Never set SELECTA_SOCKET except to a scratch path. No subagents. Never read key files or images. Do not touch ~/Code/UP-Term. Budget ~70-100 tool calls; report in 15-20 lines. Use model sonnet; at most 2 agents at once.

## Learnings and gotchas

- Configure: `cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug`. SDL: add `-DSELECTA_UI_DESKTOP=ON`. `SELECTA_GOLDEN_UPDATE=1` updates goldens.
- Dui ratchet: the `-L dui` test count must equal the plan id count. Extra tests get other labels (for example keys-live).
- Never write the word "double" in a gui header comment (a check rejects it).
- Never commit the third_party symlinks.
- AmigaDOS has no `2>`. CMake CMAKE_MATCH stops at 9 groups. zsh `--include=*.c` globs fail unquoted.
- Foreground `sleep` is blocked; use `perl -e 'select(undef,undef,undef,N)'` or an osascript delay.
- Manual selecta runs need their own socket (`env TMPDIR=<scratchpad>` or a scratch SELECTA_SOCKET) and their own --db.
- New test input verb `exec <engine line>` exists (added in I266).
- k_actions in src/uimodel/keybind.c is looked up by name only, so inserting entries is safe.

## Johan feedback this session

- Launch the app yourself for live checks. Never paste launch commands to him (memory: launch-app-dont-paste-commands).
- "i see the 3 visualizers": recorded in I269 as a partial look confirmation.
- Search input "broken and looks broken": the focus ring was drawn from a hard-coded rect, and the caret was a literal "|". Fixed in bd68454, needs his live recheck.
- "texts in buttons are misaligned": becomes I270.

## Blocked on Johan

- "up" for the Replay (real A1200, 192.168.0.74). It crashes idle (AN_MemCorrupt 8100 0005). Do not touch it before he says "up". Then: a ping watch, then a sonnet Replay agent for I124 (`render --output ahi --seconds 10`, underruns=0), I252, and the I219i/I231/I251 probes. Access: source ~/.config/selecta/replay.env per command only, never globally; VPN off.
- Live checks: search ring/caret, button centring (after I270), Now Playing strip, right-click Add to Queue, P4-6 to P4-9 looks, I219h seek pengotitle.mod, I218d Latin-1, I218e first-frame lines, I242 click/wheel/pinch.
- I248, R10 retest, PR #81 in ~/Code/devilbox-selecta, mpris error location, detail-view title shift, I138, I203, Monday BMS pass.
- I267 (open_failed at launch from stale library.db rows): Johan keeps his stale row as a live repro. Never edit his ~/.config/selecta/library.db. The fix must handle missing files quietly, with a regression test.

## Constraints

- Never read key files (~/.config/wasabi/config, ~/.config/selecta/replay.env, ENVARC:wasabi.key). Scripts read keys; never print them.
- Destructive git operations need explicit approval. Never a bare `git stash` or `pop`.
- No AI attribution in commits or PRs.
- Images never enter the main context; a haiku or sonnet subagent reads them (screenshot inbox ~/Pictures/claude-inbox).
- ~/Code/UP-Term is read only. Rigs rig3-rig7 belong to other sessions.
- One test run on the machine at a time, all sessions counted.

## Artifacts

- Plan: thoughts/shared/plans/2026-10-24-desktop-ui.md
- Ledger: thoughts/spot/todos.md
- Start prompt for the next session: thoughts/spot/next-session.md
