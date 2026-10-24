# I218c: Amiga console size query and resize events

Problem: amiga/term.c asked only xterm `ESC[18t`; the native console never answers, so the size stayed 80x24 and never changed.

Changes
- amiga/term.c: open sends `ESC[18t` and `ESC[0 q`; query_size parses `CSI 8;r;c t` and `CSI 1;1;r;c SPACE r` (CSI = ESC[ or 0x9B). A native reply sets amiga_state.native and sends `ESC[12{`; close sends `ESC[12}` before the leave sequence. Xterm-only reply sends nothing extra.
- keys.c/keys.h: params[8]; with amiga_console, `CSI 12;...|` gives TXT_KEY_RESIZE, `CSI 1;1;rows;cols SPACE r` gives TXT_KEY_WINDOW (x=cols, y=rows). Space is accepted before the final byte in 7-bit form too when amiga_console is set.
- term.h: txt_term_ask_size, txt_term_set_size; posix no-ops; amiga implementations.
- text_ui.c: regrow extracted to tui_resize; TXT_KEY_RESIZE -> ask_size, TXT_KEY_WINDOW -> set_size; neither reaches bindings. Without a real terminal (headless or keys-file, which skip the size check in the step loop) TXT_KEY_WINDOW also calls tui_resize directly so the flow is testable.
- apps/selecta/main.c: prints `view_window=` (engine list height) in the final stats.

Tests
- grid-unit: 8-bit and 7-bit forms with amiga_console on; same bytes with it off give only the following 'q' keys.
- amiga-console-keys-vamos (tests/m2/amiga_keys.cmake): keys file with resize event then bounds 50x120 under vamos; asserts view_window==46 (80x25 would give 21).
- Revert check: restoring the old keys.c made grid-unit and amiga-console-keys-vamos fail; restored, all pass.
- Passing: grid-unit, amiga-console-keys-vamos, keybind-unit, amiga-text-ui-vamos, text-ui tests (8 of 8 selected). Builds: 0 warnings outside third_party; `commands --unimplemented | wc -l` = 8.

Unverified: behaviour on a real Amiga (reply format taken from the design; not tested on the console).
