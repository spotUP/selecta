# Selecta

Multi-format music player: headless portable C engine, plugin SDK for replayers, outputs, visualizers and UIs. Targets AmigaOS 3.x (68020+), Linux, macOS, Windows.

Design: `thoughts/shared/plans/2026-10-22-architecture-design.md`. Rules and commands: `RULES.md`.

## Build and test

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

With `vamos` installed, reconfigure `build/host` after the Amiga build and the `amiga-roundtrip` test runs the 68k binary. Render a song to a WAV and play it on the Mac:

```sh
build/host/apps/selecta-cli/selecta-cli render build/host/tests/songs/selecta-test.mod --output out.wav --seconds 10 --rate 44100 --resample linear
build/host/apps/selecta-cli/selecta-cli play build/host/tests/songs/selecta-test.mod --output coreaudio --seconds 10
```

Full command list: `RULES.md`.
