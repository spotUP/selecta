# Selecta

Multi-format music player: headless portable C engine, plugin SDK for replayers, outputs, visualizers and UIs. Targets AmigaOS 3.x (68020+), Linux, macOS, Windows.

Design: `thoughts/shared/plans/2026-10-22-architecture-design.md`. Rules and commands: `RULES.md`.

## Build and test

Fresh clone prerequisites (any build): `git submodule update --init` (third_party/nuade and third_party/libopenmpt; configure stops with the exact command otherwise).

```sh
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host
nice -n 19 ctest --test-dir build/host -j2 --output-on-failure
```

Amiga 68020 cross-build (bebbo amiga-gcc in `~/opt/amiga`):

```sh
cmake -S . -B build/amiga -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/amiga-68020.cmake
cmake --build build/amiga
```

Amiga build prerequisites (outside the repo, checked at configure time by `cmake/amiga-68020.cmake`): bebbo amiga-gcc, built with the MUI 3.8 SDK so that `ndk-include/libraries/mui.h`, `proto/muimaster.h`, `clib/muimaster_protos.h` and `lib/libmui.a` exist under `<AMIGA_GCC_ROOT>/m68k-amigaos` (rebuild amiga-gcc with `SDK=mui`, or copy the SDK files there; the MUI headers carry no redistribution grant, so they are not vendored). Set `AMIGA_GCC_ROOT` if the toolchain is not in `~/opt/amiga` or `/opt/amiga`. Build the packaged targets with `cmake --build build/amiga --target selecta up_mcc_dist`.

With `vamos` installed, reconfigure `build/host` after the Amiga build and the `amiga-roundtrip` test runs the 68k binary. Render a song to a WAV and play it on the Mac:

```sh
build/host/apps/selecta-cli/selecta-cli render build/host/tests/songs/selecta-test.mod --output out.wav --seconds 10 --rate 44100 --resample linear
build/host/apps/selecta-cli/selecta-cli play build/host/tests/songs/selecta-test.mod --output coreaudio --seconds 10
```

## Text UI

```sh
build/host/apps/selecta/selecta --start-dir ~/Music/mods
```

The browser lists the folder; Up/Down move, Enter opens a directory or plays a song, Left/Right seek, `n` plays the next queued song, `i` shows the module info, `q` quits and restores the terminal. `--output null` plays silently (used by the tests). Needs an xterm-256color terminal with UTF-8.

Full command list: `RULES.md`.
