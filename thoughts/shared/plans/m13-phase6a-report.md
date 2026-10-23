# M1.3 Phase 6a report (I101): text UI viz and pattern panels

Commits: 422f1de (red fixtures), then engine pattern request path, then text UI panels and bindings with real goldens.

Tests: ui-panel-viz and ui-panel-pattern seen red, now green. Neighbours (selecta-*, text-ui, grid, palette, m12-reach, no-alloc*) pass. build/amiga compiles.

Bindings: V cycles viz (off, Levels, Scope, Spectrum); P toggles the pattern panel; 1..9 toggle mute of voice N and select it; 0 clears solo; S solos the last selected voice; > and < tempo +5/-5; } and { pitch +100/-100 cents.

Deviations:
- + and - are already VOLUME, so pitch uses { and }.
- New engine API sel_engine_pattern_request and sel_state.pattern; the audio thread answers, also while paused. sel_engine_tracker_* stays script-thread only.
- The viz fixture uses a libopenmpt (RENDER kind) song, so it shows "No per voice data for this player"; level bars and the scope are not covered by a golden. Spectrum, note dots and phase are not golden-tested; note dots and phase are not implemented.

Open questions (default chosen): solo target is the last digit-chosen voice (default voice 1); a VOICE-kind fixture for Levels and Scope is wanted (I102 or later).
