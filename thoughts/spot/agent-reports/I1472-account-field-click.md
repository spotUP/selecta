# I1472 account dialog: cannot click or type in the Username field

Status: fix written and committed, NOT built or tested (buildq heavy lane had 3 jobs ahead of mine for 20+ min; build/sdl is configured, build queued/running).

Root cause (static read, unverified by run): the account dialog changes state with `d->repaint = 1` only, never `d->uim.dirty |= SEL_UIM_DIRTY_ALL`.
A live (SDL) turn presents only a dirty model (desk_present returns 0 when dirty==0, desktop_ui.c ~17552; same lesson in nagdlg_open "I789").
Sites: acct_open, acct_submit, acct_pointer (field focus, toggle), acct_key (every typed char) in desktop_ui.c ~3797-3945.
Result on a real Mac: the dialog only repaints when something else (a mouse move hover tween) dirties the model, so a field click shows no focus
change and typed letters stay invisible until the mouse moves. Hit rects (acct_field_rect), PRESS/CLICK routing and key routing (desk_key -> dlg_key -> acct_key,
text via SEL_GUI_RAW_TEXT -> IN_KEY) read correct. Headless mem runs present on every input, so the old test could not see it.
Fix: dirty ALL beside every repaint in the account code.
Test: tests/dui/account_dialog.cmake extended (click Create Account Instead, click Username, type "ab", click Password, type "c", Log In form too) asserting
the new dump line `acctform signup= field= namelen= passlen= toggle= f0= f1=`. It checks state, not presentation, so it does not prove this fix; the live-only
presentation needs Johan on Mac (avatar, click Username, type, no mouse move: letters must appear).
Caret: not added (not trivial; field draws no caret for any state).
