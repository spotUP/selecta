# Selecta project rules

The global rules (`~/.claude/CLAUDE.md`) apply; this file adds the project's. Design: `thoughts/shared/plans/2026-10-22-architecture-design.md`. Research: `thoughts/shared/research/`.

1. **Engine is headless and is the single source of truth.** UIs render `sel_engine_state()` and send commands; no playback logic, no derived state in a UI, viz or protocol module. New behaviour goes into `src/engine/` or `src/library/`, never a frontend.
2. **One definition per concept.** Commands and events exist only in `include/selecta/commands.def` and `events.def` (X-macros feed the enums, the remote codec and the docs). Voices exist only as `sel_voices`. Metadata only as `SEL_META_*` tag lists reported by the replayer (no fixed sink struct). A second copy of any of these is a defect.
3. **Plugins see `include/selecta/` and nothing else.** Replayers, outputs, viz and UI plugins compile alone against the SDK headers and the `sel_host` table. OS headers only under `plugins/*/<os>/` and `src/core/<os>/`. Plugin descriptors are tag lists (`sel_tag` arrays ending in `SEL_TAG_END`, design doc section 10): a plugin declares only what it implements, the host queries by tag and skips unknown tags. The `sel_host` table is a versioned struct whose fields are appended, never reordered.
4. **Portable C99, scaled at runtime, never capped at build time.** `-Wall -Wextra -Werror`, no VLAs, no `alloca`. Build time decides only the ISA (`-m68020` today; `-m68000`, `-m68060`, x86-64, arm64 later). Everything else is probed at start into `sel_machine` (design doc section 12): CPU class and measured speed, fast and chip RAM, graphics (planar ECS/AGA, RTG, desktop GPU), audio, network, float speed. Features and UI layouts read `sel_machine` and scale or hide themselves; no feature is cut from the design because the 68020 + 2 MB baseline cannot run it. Absolute rules that never scale: no floating point in the engine or any path that runs under interrupt, no allocation after `start` in engine or plugins, sample memory through `host->mem_alloc(size, SEL_MEM_SAMPLE)` (chip RAM on Amiga).
5. **Adapt, never fork.** nuade, libopenmpt, libgme, libsidplayfp, bbmp helpers and HippoPlayer/UADE code are wrapped behind plugin adapters in `third_party/` + `plugins/`; a fix in them goes upstream or into a patch file in `third_party/patches/`.
6. **Golden WAVs are the oracle.** Every replayer plugin ships a song and a hash in `tests/golden/`; `selecta-cli render` must reproduce it on desktop and under the 68k build. A playback change updates the hash and says why in the commit.
7. **Text UI assumes xterm-256color + UTF-8** and nothing Amiga-specific; on the Amiga it runs inside UP-Term (`~/Code/UP-Term/vtcon`). Capabilities it may use: research doc `2026-10-22_upterm-text-ui-capabilities.md`.
8. **Icons and labels:** text UI uses ASCII and Unicode box/block glyphs only, no emoji; GUI UIs use the project's SVG/bitmap icon set under `art/` (to be created with the first GUI).
9. **Ledger discipline.** Every task list lives in `thoughts/spot/todos.md` with IDs (R research, D design, A ABI, S sources, P playlists, M modern, F features, L ledger). Each reply names the IDs ticked since the last reply and the running count `done/total`. Nothing is started that is not in the ledger; nothing is reported done that is not ticked.

## Commands

Toolchains: desktop clang/gcc + CMake + Ninja; Amiga bebbo amiga-gcc at `/opt/amiga` (CMake toolchain file `cmake/amiga-68020.cmake`), vasm for `.s`. Test rig: UP-Term's FS-UAE rig (`~/Code/UP-Term/vtcon/tools/rig/`).

| Task | Command |
|------|---------|
| Configure + build (desktop, debug) | `cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/host` |
| Build (Amiga 68020) | `cmake -S . -B build/amiga -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/amiga-68020.cmake && cmake --build build/amiga` |
| Type/static check | `cmake --build build/host --target lint` (clang-tidy + `-Werror` build; the SDK lint checks plugins compile against `include/selecta/` alone) |
| Format | `cmake --build build/host --target format` (clang-format, `.clang-format` at root) |
| Test, one suite | `ctest --test-dir build/host -R <name> --output-on-failure` |
| Test, CI (all) | `ctest --test-dir build/host --output-on-failure` |
| Golden render for one replayer | `build/host/apps/selecta-cli/selecta-cli render <song> --output <wav>`; compare with `tests/golden/<plugin>/<song>.sha256` |
| Run text UI (local) | `build/host/apps/selecta/selecta` in a terminal (LOCAL; on the Amiga: `Selecta` in an UP-Term window) |
| Stop / cleanup | `rm -rf build/host build/amiga` |

Not yet implemented: every command above is the contract for M0 (see the design doc, section 14). Update this table in the same commit that makes a command real.
