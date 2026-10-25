---
date: 2026-10-25
topic: Gaps found in the M4 ARexx plan and how each was filled (ledger I342)
---
# Gaps in 2026-10-25-m4-arexx.md

Filled: edited in the plan. Q: open question for Johan (section 8). M: needs measurement (checklist item).

1. Wrong line numbers (poll at main.c:588 not 576; COMMANDS LOG at engine.c:1103). Section 2. Edited.
2. Fact "hook_command exists": only the key is declared, nothing reads it. Section 2, M4-AX-4-2. Edited; scope question Q9.
3. BEGIN/END is one engine-wide flag (engine.c:302), plan said per-port queue. Sections 2, 5. Edited; Q8.
4. IDLE has no engine handler; refusal location unspecified. Sections 2, 5. Edited.
5. QUIT has no engine path (text UI sets its own flag). Section 4.2, M4-AX-2-4. Edited (sel_engine_take_quit, like take_ui_switch).
6. Reply buffer 1024 bytes vs about 7 KB COMMANDS; emit_log ring of 8 slots; event queue 256; no numbers. Section 3, M4-AX-1-4. Edited; measurement inside the test.
7. Request length limit and over-long ARG0. Section 3, 5. Edited (overflow, RC 10).
8. RC codes and error text table; empty line, non-command message, no RXFF_RESULT. Section 5. Edited.
9. Error text in RESULT on RC=10 is probably not how ARexx works (Result2 is a number on error). Section 5, finish line step 5. M4-AX-0-3 (M); Q6.
10. Error plus results in one line; verb case sensitivity. Section 3, M4-AX-1-4. Edited / M (case from code).
11. Concurrency with GUI, slow command blocks sender, per-turn message cap. Section 5, M4-AX-5-4 (M); Q7 for the number.
12. Port name: upper limit, reuse of freed SELECTA, name in stats. Section 5. Edited (.99, lowest free suffix, arexx_port=).
13. Shutdown routes: only QUIT covered; Ctrl-C, UI failure, message arriving during close. Section 5, M4-AX-5-4. Edited.
14. Quoting: ARG0 is one string, escaping rules, script comment. Section 5. Edited.
15. Startup order and RexxMast absent (hook only). Section 5. Edited.
16. No reachability test for the socket reply (M4-AX-1-3 only "stay green"). M4-AX-0-4 added. Edited.
17. No negative test for the build-time label check. M4-AX-2-1. Edited.
18. QUIT tests incomplete (socket, inside BEGIN). M4-AX-2-4. Edited.
19. Port logic untestable on the host. M4-AX-3-4 pure mapping function. Edited.
20. Hook has no test (async, missing rx, once per song). M4-AX-4-2. Edited.
21. Sentinel `arexx_cmds>=6` ambiguous (socket counts OK lines only). M4-AX-5-1 now exact 6 plus arexx_errors. Edited.
