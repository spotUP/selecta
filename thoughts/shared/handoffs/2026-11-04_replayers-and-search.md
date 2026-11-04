---
date: 2026-11-04
author: spot
topic: bbmp replayer ports, I906h search freeze, unpushed main
status: handoff (weekly token limit near)
---

# Handoff 2026-11-04: replayer track + Mac/general track

Trust thoughts/spot/todos.md over this note. Rows named here: I698, I906j, I906k, I832b, I429d, I700-I710, I860, I864-I866, I433, I1065, I1009, I906h.

## State at hand-off

- main is 16 commits ahead of origin/main (remote at 58c71a14). NOT pushed. All authored spotUP.
  Before push: the queued main test runs must be green (see "Unverified").
- Two background agents were still running when this session ended. Their reports land only in the old session, so the next session recovers from their worktrees:
  1. I906j timing metric fix: worktree .claude/worktrees/agent-a0ab1e32285e9d698 (branch worktree-agent-a0ab1e32285e9d698, base 58c71a14).
     Scope: turn_ms_max stops counting the desk_sdl_poll AppKit pump at the top of desk_wait_turn (desktop_ui.c ~15873, subtraction ~16049); paint figure stops spanning SEARCH_DEBOUNCE_MS (frame_t0 :671 opened at input :14396, closed at desk_flush ~714); a dui test with real clock so a typed search fires (nav_turn_cost uses --clock fixed, searches=0).
  2. I698 libkss finish: worktree .claude/worktrees/agent-add0094be2e665ef3 (base commit 4dbc87ba).
     Scope: `--subsong <n>` option on selecta-cli render (apps/selecta-cli/render.c, Johan's choice 2026-11-04, no product change), kss-decode tests .mgs default + .kss song 1, red proof, then deletes scratchpad kssbuild (122 MB) and kssw.
     REPORTED NOT GREEN: work is UNCOMMITTED in the worktree (do not remove it). Ledger row I698 lists what is done and the 5 open steps. Scratchpad kssbuild lives in /private/tmp and may be gone; then rebuild (~20 min).
  For each: `git -C <worktree> log --oneline -3` and `git -C <worktree> status --short`. Committed and green: review, cherry-pick to main, build + affected tests behind gate, `git worktree unlock`, `git worktree remove --force`, `git branch -D`, tick ledger. Dirty or no commit: brief a NEW agent from the ledger row (never resume the old one).
- Other worktrees not from this session's open work: agent-a2141694eab04fe20, agent-a773155596f908acb, agent-ad309b9c77bae5eb3. Owner unknown; do not delete without asking.

## PUSH BLOCKED

Main build passed but 4 search tests fail (dui-search, dui-search-badge, dui-search-online-live, dui-search-online-quit). Ledger row I1009r has the failure lines and the plan; first suspect efecabdb (I1009). Fix before push.

## Unverified

- Queued main test runs (background shells in old session; may die with it). Re-run on main if logs incomplete:
  build `nice -n 19 cmake --build build/app -j2`, then
  `nice -n 19 ctest --test-dir build/app -j1 -R '^(chipnsfx-decode|builtin-modules|bundle-plugins|sdk-isolation.*|mdxmini-decode|tiatracker-decode|dui-nav-turn-cost|dui-kind-pages|dui-collection-page|dui-list-keys|dui-playlist-page|dui-golden-home|dui-search.*)$'`
  Old logs: scratchpad bm.log tm.log bm2.log tm2.log bm3.log tm3.log of session d530c48f (may be gone).
- Gate bug found and fixed in session scratchpad only: old busy.sh always exited 0, so `while sh busy.sh; do sleep 30; done` never ended. New sessions need a busy.sh that exits 0 when busy, 1 when free:
  `if LC_ALL=C ps -Ao command | grep -E "^(cmake --build|ctest|.*/clang |.*/clang\+\+ |.*vitest|.*jest)" | grep -v grep | grep -q .; then echo busy; exit 0; else echo free; exit 1; fi`
  Better: commit it as scripts/machine-busy.sh (RULES 6a-1 gate) so it stops living in scratchpads.

## Decisions made (do not re-ask)

- I698: libkss with drivers; header-less .kss test uses --subsong 1 (render flag, no plugin skip rule).
- I429 TIATracker (GPL-2-only): run as helper program, out of process like bbmp. Row I429d needs an Opus design for a helper-process plugin type.
- I433 Arkos: port later.
- I906h: no search freeze on current main with real db + real clock (all search turns < 100 ms). The 300-1656 ms turns are the AppKit pump at startup (row I906k), and the metric is wrong (row I906j).

## Next, in order

1. Recover the two agents (above), merge, run gated tests, push (check `ssh -G github.com | grep -i serveralive` first; push in background; verify `git ls-remote origin refs/heads/main`).
2. Replayer slot: next open row after I698: I700-I708, I860, I864-I866, I709, I710; I429d design (Opus); I433 last.
3. Mac/general slot: I906k (startup AppKit pump stall), I832b (desk_close pthread join blocks 1.4-5 s on quit), then todos.md "## Now".
4. Johan Mac checks (LOCAL, newest Selecta.app, kill old Selectas first): I1065 artist/composer scroll clamp; I1009 Enter in Search keeps focus; search "chip" from Home and from a source page shows no beachball; I1015 hover feedback; no HippoPlayer row in SOURCES; ZX-Art PT3/SID/Furnace play.

## Gotchas

- build/app/apps/selecta/selecta binary is stale for Johan's db (user_version 17); use Selecta.app/Contents/MacOS/Selecta.
- Sampling the real app: `pgrep -x Selecta` (not pgrep -f wrapper); start sampling after log line "UI open returned" (~12 s).
- Never stage thoughts/shared/research/2026-11-01-ui-thread-library-waits.md or thoughts/spot/agent-reports/I972.md.
- Agent briefs: no subagents, ~30-call budget, cleanup duty, gate loop with fixed busy.sh.
