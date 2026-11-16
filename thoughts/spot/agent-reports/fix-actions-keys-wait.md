# Fix dui-actions and text-ui-keys-wait (2026-11-16)

1. dui-actions: invariant "every COMMANDS verb has a row or an exemption" violated for CANCEL_OPEN (src/uimodel/actions.c k_exempt). Verb is sent only by the Cancel button on the loading now-playing bar (desktop_ui.c, mui_ui.c) and Escape in text_ui.c, like the other CTX verbs. Added an exemption with a reason.
2. text-ui-keys-wait: the text UI is right (Backspace is bound to @back, which never reaches u->quit). The test was wrong: tests/ui/text_ui_test.c "backspace root" ended its keys file after 420 ms, and the end of a keys file makes wait() return -1 (tui_file_step), read as a quit. Final wait raised to 5000 ms so it outlasts the 40 x 20 ms loop. No UI change needed.
