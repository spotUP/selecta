# ctest entries T1 to T9 (plan 2026-10-22-m0-skeleton.md). Expected counts
# come from cmake/selecta_defs.cmake, never retyped here.
set(SEL_CLI $<TARGET_FILE:selecta-cli>)
set(SEL_EXPECT "${CMAKE_CURRENT_SOURCE_DIR}/expect.cmake")
math(EXPR SEL_COUNT_MSG "${SEL_COUNT_CMD} + ${SEL_COUNT_EVT}")

function(sel_cli_test name args)
  # Remaining arguments are -D settings for expect.cmake.
  add_test(NAME ${name}
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DARGS=${args}" ${ARGN} -P "${SEL_EXPECT}")
endfunction()

# T1: every row of both tables survives encode -> decode.
sel_cli_test(protocol-roundtrip "roundtrip"
  "-DEXPECT=commands=${SEL_COUNT_CMD} events=${SEL_COUNT_EVT} ok=${SEL_COUNT_MSG}")

# T2: reflection lists one line per table row.
sel_cli_test(protocol-reflection-commands "commands" "-DLINES=${SEL_COUNT_CMD}")
sel_cli_test(protocol-reflection-events   "events"   "-DLINES=${SEL_COUNT_EVT}")
sel_cli_test(protocol-reflection-tagtypes "tagtypes" "-DLINES=${SEL_COUNT_META}")

# T5: the registry walked every descriptor and found only known tags.
# null, wav, nuade, openmpt, the crossfeed DSP, the text UI, the nowplaying-test integration and the voices-test
# output (both host only), plus coreaudio and the
# macOS Now Playing plugin on macOS, or MPRIS on Linux when libsystemd was found; the negative tree adds one bad plugin.
set(SEL_EXPECT_PLUGINS 8)
if(APPLE)
  math(EXPR SEL_EXPECT_PLUGINS "${SEL_EXPECT_PLUGINS} + 2")
elseif(SEL_HAVE_MPRIS)
  math(EXPR SEL_EXPECT_PLUGINS "${SEL_EXPECT_PLUGINS} + 1")
endif()
math(EXPR SEL_EXPECT_BAD_PLUGINS "${SEL_EXPECT_PLUGINS} + 1")
sel_cli_test(registry-tags "plugins --lint" "-DEXPECT=plugins=${SEL_EXPECT_PLUGINS} unknown_tags=0 duplicate_tags=0")

# T6: sel_host layout golden per architecture.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SEL_ARCH)
sel_cli_test(abi-host-layout "abi" "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/abi/sel_host.${SEL_ARCH}.txt")
sel_cli_test(abi-voices-layout "abi voices" "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/abi/sel_voices.${SEL_ARCH}.txt")

# T3: the engine runs a script through the real dispatch and emits the
# expected events, including batch deferral, unknown and unimplemented verbs.
sel_cli_test(m0-script "script ${CMAKE_CURRENT_SOURCE_DIR}/m0/basic.sel"
  "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m0/basic.expected")

# T4: the null output pulls the requested frames through the engine render.
sel_cli_test(m0-null-output "play-null --rate 44100 --frames 4410"
  "-DEXPECT_REGEX=^pulled=4410 render_calls=[1-9][0-9]* rate=44100$")

# T8: the ratchet of unimplemented verbs.
add_test(NAME ratchet
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DRATCHET=${CMAKE_CURRENT_SOURCE_DIR}/ratchet/unimplemented.txt"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/ratchet.cmake")

# T27b: a forced relink of selecta-cli must not warn about duplicate libraries.
# Serial: it deletes the binary the other CLI tests run.
add_test(NAME link-warnings
  COMMAND "${CMAKE_COMMAND}" "-DBUILD_DIR=${PROJECT_BINARY_DIR}" "-DCLI=${SEL_CLI}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/link_warnings.cmake")
set_tests_properties(link-warnings PROPERTIES RUN_SERIAL TRUE)

# T13: render the generated test module, four resamplers. The module comes
# from tools/gen_testmod.c (host build tool); hashes pinned per resampler in
# tests/golden/nuade/ once the linear render has been listened to. A missing
# hash file skips the comparison and prints the computed value.
set(SEL_TESTMOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-test.mod")
set(SEL_RENDER_OUT "${CMAKE_CURRENT_BINARY_DIR}/out")
add_executable(gen_testmod "${PROJECT_SOURCE_DIR}/tools/gen_testmod.c")
add_custom_command(OUTPUT "${SEL_TESTMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod "${SEL_TESTMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-test.mod")
set(SEL_SHORTMOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-short.mod")
add_custom_command(OUTPUT "${SEL_SHORTMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --short "${SEL_SHORTMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-short.mod")
set(SEL_MKMOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-test-mk.mod")
add_custom_command(OUTPUT "${SEL_MKMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --format mk "${SEL_MKMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-test-mk.mod")
set(SEL_BREAKMOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-break-mk.mod")
add_custom_command(OUTPUT "${SEL_BREAKMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --break --format mk "${SEL_BREAKMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-break-mk.mod")
# T42: selecta-test.mod with sample 1 (channel 0 only) zeroed; the golden for MUTE 1.
set(SEL_ZERO1MOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-test-zero1.mod")
add_custom_command(OUTPUT "${SEL_ZERO1MOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --zero-sample 1 "${SEL_ZERO1MOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-test-zero1.mod")
# T43: selecta-test.mod with every sample zeroed (digital silence).
set(SEL_SILENTMOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-silent.mod")
add_custom_command(OUTPUT "${SEL_SILENTMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --silent "${SEL_SILENTMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-silent.mod")
# T31: copies of the test module in album directories a, b and c.
set(SEL_ALBUM_STAMP "${CMAKE_CURRENT_BINARY_DIR}/songs/album.stamp")
add_custom_command(OUTPUT "${SEL_ALBUM_STAMP}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs/a"
    "${CMAKE_CURRENT_BINARY_DIR}/songs/b" "${CMAKE_CURRENT_BINARY_DIR}/songs/c"
  COMMAND "${CMAKE_COMMAND}" -E copy "${SEL_TESTMOD}" "${CMAKE_CURRENT_BINARY_DIR}/songs/a/one.mod"
  COMMAND "${CMAKE_COMMAND}" -E copy "${SEL_TESTMOD}" "${CMAKE_CURRENT_BINARY_DIR}/songs/a/two.mod"
  COMMAND "${CMAKE_COMMAND}" -E copy "${SEL_TESTMOD}" "${CMAKE_CURRENT_BINARY_DIR}/songs/b/three.mod"
  COMMAND "${CMAKE_COMMAND}" -E copy "${SEL_TESTMOD}" "${CMAKE_CURRENT_BINARY_DIR}/songs/b/four.mod"
  COMMAND "${CMAKE_COMMAND}" -E copy "${SEL_TESTMOD}" "${CMAKE_CURRENT_BINARY_DIR}/songs/c/five.mod"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/m12/list.prg" "${CMAKE_CURRENT_BINARY_DIR}/songs/b/list.prg"
  COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/m12/rel.m3u" "${CMAKE_CURRENT_BINARY_DIR}/songs/b/rel.m3u"
  COMMAND "${CMAKE_COMMAND}" -E touch "${SEL_ALBUM_STAMP}"
  DEPENDS "${SEL_TESTMOD}" "${CMAKE_CURRENT_SOURCE_DIR}/m12/list.prg" "${CMAKE_CURRENT_SOURCE_DIR}/m12/rel.m3u"
  COMMENT "Copying album test songs")
add_custom_target(testmod ALL DEPENDS "${SEL_TESTMOD}" "${SEL_SHORTMOD}" "${SEL_MKMOD}" "${SEL_BREAKMOD}" "${SEL_ZERO1MOD}" "${SEL_SILENTMOD}" "${SEL_ALBUM_STAMP}")
add_test(NAME testmod-sha
  COMMAND "${CMAKE_COMMAND}" "-DMOD=${SEL_TESTMOD}"
    "-DPINNED=${CMAKE_CURRENT_SOURCE_DIR}/songs/selecta-test.mod.sha256"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/testmod_sha.cmake")
foreach(res none linear cubic sinc)
  if(res STREQUAL "linear")
    set(test_name render-golden-host)
  else()
    set(test_name render-golden-host-${res})
  endif()
  add_test(NAME ${test_name}
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_TESTMOD}"
      "-DOUT=${SEL_RENDER_OUT}/selecta-test.${res}.wav" "-DRESAMPLE=${res}"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/nuade/selecta-test.${res}.sha256"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/render_check.cmake")
  set_tests_properties(${test_name} PROPERTIES SKIP_REGULAR_EXPRESSION "golden hash absent")
endforeach()

# T22: the plain M.K. module goes through libopenmpt (not nuade, whose
# protracker4 needs the IFF wrapper). Counters and size now; the SHA-256 is
# compared once tests/golden/openmpt/selecta-test-mk.sha256 exists (committed
# only after the listening check, row I55).
if(NOT SEL_AMIGA)
  add_test(NAME render-golden-openmpt
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_MKMOD}"
      "-DOUT=${SEL_RENDER_OUT}/selecta-test-mk.openmpt.wav"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/openmpt/selecta-test-mk.sha256"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/render_openmpt.cmake")
  set_tests_properties(render-golden-openmpt PROPERTIES SKIP_REGULAR_EXPRESSION "golden hash absent")
endif()

# T16: real playback through the default output device (macOS only). Exit
# code 77 from the CLI (no output device) skips the test.
if(APPLE)
  add_test(NAME coreaudio-play
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_TESTMOD}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/coreaudio_play.cmake")
  set_tests_properties(coreaudio-play PROPERTIES SKIP_REGULAR_EXPRESSION "no output device available")
endif()

add_subdirectory(m0)
add_subdirectory(nuade_ab)
add_subdirectory(thread)

# T5 negative path: a second tree with a broken descriptor must fail lint.
add_test(NAME registry-tags-negative
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}"
    "-DBIN=${PROJECT_BINARY_DIR}/host-bad" -DEXPECT_PLUGINS=${SEL_EXPECT_BAD_PLUGINS} "-DGEN=${CMAKE_GENERATOR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m0/bad_plugin.cmake")

# T7: every plugin compiles with include/ as its only project include path.
add_test(NAME sdk-isolation
  COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target sdk-isolation)

# T9: the 68k build runs under vamos and agrees with the host. Registered
# only when vamos is installed and build/amiga holds a selecta-cli.
find_program(SEL_VAMOS vamos)
set(SEL_AMIGA_CLI "${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta-cli/selecta-cli")
# Every vamos test runs the binary in build/amiga, which the host build does
# not produce. The amiga-build fixture rebuilds it first so a host ctest never
# runs a stale 68k binary. vamos keeps one temp RAM: volume per user
# (~/.vamos/volumes/ram) and fails "temp volume volume path already exists"
# when two instances start at once, so all vamos tests share the vamos lock.
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME amiga-build
    COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_SOURCE_DIR}/build/amiga" --parallel 2)
  set_tests_properties(amiga-build PROPERTIES FIXTURES_SETUP amiga_cli)
  add_test(NAME amiga-roundtrip
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DHOST_CLI=${SEL_CLI}"
            "-DAMIGA_CLI=${SEL_AMIGA_CLI}" "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/abi/sel_host.m68k.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_vamos.cmake")
  set_tests_properties(amiga-roundtrip PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
endif()

# T10: the machine probe through `selecta-cli machine`. FAST_RAM is the
# machine's physical memory, read at test time by machine.cmake.
add_test(NAME machine-probe
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" -DPLATFORM=host
          "-DTAGS_DEF=${PROJECT_SOURCE_DIR}/include/selecta/tags.def"
          -DEXPECT_CPU=desktop -DEXPECT_FPU=1 -DEXPECT_TIER=gpu -DEXPECT_FAST_RAM=sysctl
          -P "${CMAKE_CURRENT_SOURCE_DIR}/machine.cmake")

# T11: the same probe on the 68k binary under vamos -C 68020. vamos's
# AvailMem with -m 8192 (measured 2026-10-23 by running `selecta-cli machine`; vamos 68020
# profile; the 121-player image needs more than the 1 MiB default): CHIP_RAM and FAST_RAM, bytes.
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  set(SEL_VAMOS_CHIP_RAM 0) # vamos has no chip/fast split: AvailMem(MEMF_CHIP) is 0
  set(SEL_VAMOS_FAST_RAM 8384512) # 8188 KiB: vamos -m 8192 RAM less its own use
  add_test(NAME amiga-machine
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DAMIGA_CLI=${SEL_AMIGA_CLI}" -DPLATFORM=amiga
            "-DTAGS_DEF=${PROJECT_SOURCE_DIR}/include/selecta/tags.def"
            -DEXPECT_CPU=68020 -DEXPECT_FPU=0 -DEXPECT_TIER=text
            -DEXPECT_CHIP_RAM=${SEL_VAMOS_CHIP_RAM} -DEXPECT_FAST_RAM=${SEL_VAMOS_FAST_RAM}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/machine.cmake")
  set_tests_properties(amiga-machine PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
endif()

# T14: the T13 render on the 68k binary under vamos must be byte-identical to
# the host render (the mixer is integer-only). The linear resampler is the
# named test; the other three run too because the render takes under a
# second each. vamos holds ~1 MiB of RAM by default; the render streams the
# WAV from small buffers and fits, and every vamos call passes -m 8192 because the 121-player image is 1.85 MB.
# Also the m68k sel_voices layout golden (32-bit pointers, big-endian).
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  foreach(res none linear cubic sinc)
    if(res STREQUAL "linear")
      set(test_name render-golden-amiga)
    else()
      set(test_name render-golden-amiga-${res})
    endif()
    add_test(NAME ${test_name}
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DHOST_CLI=${SEL_CLI}"
        "-DAMIGA_CLI=${SEL_AMIGA_CLI}" "-DMOD=${SEL_TESTMOD}" "-DROOT=${PROJECT_SOURCE_DIR}"
        "-DOUT_DIR=${SEL_RENDER_OUT}" -DRESAMPLE=${res}
        "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/nuade/selecta-test.${res}.sha256"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_render.cmake")
    set_tests_properties(${test_name} PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
  endforeach()
  add_test(NAME amiga-voices-layout
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DAMIGA_CLI=${SEL_AMIGA_CLI}"
            "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/abi/sel_voices.m68k.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_voices.cmake")
  set_tests_properties(amiga-voices-layout PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
endif()

# T18: rule 4 from the 68k object files. Needs the cross toolchain's nm and a
# built build/amiga (the amiga-build fixture rebuilds it first).
find_program(SEL_AMIGA_NM m68k-amigaos-nm HINTS "$ENV{HOME}/opt/amiga/bin" /opt/amiga/bin)
if(SEL_AMIGA_NM AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME no-float-engine
    COMMAND "${CMAKE_COMMAND}" "-DNM=${SEL_AMIGA_NM}" "-DBUILD_DIR=${PROJECT_SOURCE_DIR}/build/amiga"
            -P "${PROJECT_SOURCE_DIR}/cmake/no_float.cmake")
  set_tests_properties(no-float-engine PROPERTIES FIXTURES_REQUIRED amiga_cli)
endif()

# T12: mixer unit test (period step, resampler vectors, clipping, loop and
# queue semantics). Groups are listed so a resampler lands in the gate with
# the row that implements it.
add_executable(mixer_test mixer/mixer_test.c)
target_link_libraries(mixer_test PRIVATE selecta_core)
add_test(NAME mixer-unit COMMAND mixer_test period none linear cubic sinc default clip loop)

# T24: browse listing and view model through the engine command queue and
# snapshot. The temp tree is created inside the generated songs directory.
add_executable(view_test view/view_test.c)
target_link_libraries(view_test PRIVATE selecta_core selecta_registry)
add_test(NAME view-unit COMMAND view_test "${CMAKE_CURRENT_BINARY_DIR}/songs" browse sort cursor activate window style)
# T32: grouping, node ids, expand/collapse, drilldown, cursor across styles.
add_test(NAME view-tree COMMAND view_test "${CMAKE_CURRENT_BINARY_DIR}/songs" tree)
# Both rebuild the same song directory.
set_tests_properties(view-unit view-tree PROPERTIES RESOURCE_LOCK view_songs_dir)

# T33: title-format resolver and PREF_GET / PREF_SET (src/engine/format.c, prefs.c).
add_executable(format_test view/format_test.c)
target_link_libraries(format_test PRIVATE selecta_core selecta_registry)
target_include_directories(format_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
add_test(NAME title-format COMMAND format_test)

# T25: text UI cell grid, key parser, theme tokens and POSIX terminal device
# (plugins/ui/text). Desktop only: the Amiga text UI runs in UP-Term in M2.
if(NOT SEL_AMIGA)
  add_executable(grid_test grid/grid_test.c)
  target_link_libraries(grid_test PRIVATE selecta_textui)
  add_test(NAME grid-unit COMMAND grid_test)
  # I60 fix: a keys-file `wait` ends under a clockless and a device-clocked
  # shell (it hung when the shell passed timeout 0).
  add_executable(text_ui_test ui/text_ui_test.c)
  target_link_libraries(text_ui_test PRIVATE selecta_core selecta_registry)
  add_test(NAME text-ui-keys-wait COMMAND text_ui_test "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(text-ui-keys-wait PROPERTIES TIMEOUT 30)
endif()

# T19: the M1.1 reachability test. The real shell and text UI, driven by a key
# file through the same key decoder as a terminal: browse, play, info panel.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-browse-play
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/browse_play"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.cmake")
endif()

# T34 reachability (I76): the same run with x and z bound to info and quit by
# --bindings. The screen must equal the browse_play golden and the shell must
# have decoded the keys through the table.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-bindings
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/bindings.keys"
      "-DBINDINGS=${CMAKE_CURRENT_SOURCE_DIR}/m1/bindings.conf"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/bindings"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.cmake")
endif()

# T24: the queue as a folder tree, scripted through the shell's key decoder.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-tree-view
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/tree_view.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m1/tree_view.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/tree_view"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/tree_view.cmake")
endif()

# T24: the same queue as a drilldown: pick a folder, the breadcrumb and its songs show.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-drill-view
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/drill_view.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m1/drill_view.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/drill_view"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/tree_view.cmake")
  set_tests_properties(selecta-tree-view selecta-drill-view PROPERTIES RESOURCE_LOCK tree_view_root)
endif()

# T20: queue edits, ids, NEXT/PREV and auto-advance through the engine
# dispatch. The script names songs relative to the generated songs directory.
add_test(NAME queue-engine
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m1/queue.sel"
    "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m1/queue.expected"
    -P "${SEL_EXPECT}")
set_tests_properties(queue-engine PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")

# T30: queue history ring, MOVE, SWAP, SHUFFLE, RESHUFFLE, UNDO, REDO.
add_test(NAME queue-history
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m12/history.sel"
    "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m12/history.expected"
    -P "${SEL_EXPECT}")
set_tests_properties(queue-history PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")

# T31: SCOPE, NEXT_ALBUM, PREV_ALBUM (album = parent directory).
add_test(NAME queue-scope-album
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m12/album.sel"
    "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m12/album.expected"
    -P "${SEL_EXPECT}")
set_tests_properties(queue-scope-album PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")

# T21: a 5000 ms seek renders the second half of the unseeked 10 s render,
# for the nuade song (restart-and-skip) and the openmpt song (native RP_SEEK).
add_test(NAME seek-render
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_TESTMOD}"
    "-DOUT_DIR=${SEL_RENDER_OUT}" -DRESAMPLE=linear -DTAG=nuade
    "-DPATH_RE=seek_path=skip skipped_frames=220500"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/seek_render.cmake")
if(NOT SEL_AMIGA)
  add_test(NAME seek-render-openmpt
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_MKMOD}"
      "-DOUT_DIR=${SEL_RENDER_OUT}" -DRESAMPLE=linear -DTAG=openmpt -DEXACT=0
      "-DPATH_RE=seek_path=native"
      # Row 41 comes from a straight-through render of selecta-test-mk.mod at frame 220500
      # (tests/m1 script OPEN/PLAY/@render 220500/@position); the seek must land on it.
      "-DPOS_LINE=seek_position order=0 row=41 pattern=0"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/seek_render.cmake")
  # I56: seek into the last row before a Dxx break; the row before order 1 row 0 is row 15.
  add_test(NAME seek-render-openmpt-break
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_BREAKMOD}"
      "-DOUT_DIR=${SEL_RENDER_OUT}" -DRESAMPLE=linear -DTAG=openmpt-break -DEXACT=0
      "-DPATH_RE=seek_path=native" -DSEEK_MS=1850 -DSEEKED_FRAMES=81585
      # Row 15 of order 0 comes from a straight-through render of selecta-break-mk.mod at frame
      # 81585 (tests/m1/position-break.sel: OPEN, @render 81585, @position); the seek must land on it.
      "-DPOS_LINE=seek_position order=0 row=15 pattern=0"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/seek_render.cmake")
  # I56: RP_POSITION in POSITION events and the snapshot.
  add_test(NAME position-openmpt
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
      "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m1/position.sel"
      "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m1/position.expected"
      -P "${SEL_EXPECT}")
  set_tests_properties(position-openmpt PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")
endif()

# T23: the nuade adapter lists every player in third_party/nuade/players.
add_test(NAME nuade-players
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DNUADE_DIR=${PROJECT_SOURCE_DIR}/third_party/nuade"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/nuade_players.cmake")

# T29: playlist codecs (Hippo .prg, M3U, PLS, XSPF), src/library/playlist.
add_executable(playlist_test playlist/playlist_test.c)
target_link_libraries(playlist_test PRIVATE selecta_library)
target_include_directories(playlist_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/playlist)
add_test(NAME playlist-codecs COMMAND playlist_test)

# T30a: random play order (seeded permutation, each entry once before a repeat).
add_test(NAME queue-random-order
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m12/random.sel"
    "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m12/random.expected"
    -P "${SEL_EXPECT}")
set_tests_properties(queue-random-order PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")

# T29 script half (row I73): PLAYLIST_LOAD and PLAYLIST_SAVE through the real dispatch.
add_test(NAME playlist-verbs
  COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
    "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m12/playlist.sel"
    "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m12/playlist.expected"
    -P "${SEL_EXPECT}")
set_tests_properties(playlist-verbs PROPERTIES
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")

# codec-optional-arg: a trailing `?` makes only the last argument optional.
add_executable(codec_test protocol/codec_test.c)
target_link_libraries(codec_test PRIVATE selecta_core)
add_test(NAME codec-optional-arg COMMAND codec_test)
# T34: key binding table, keys.conf lines, key names (plugins/ui/text/keybind.c).
if(NOT SEL_AMIGA)
  add_executable(keybind_test ui/keybind_test.c)
  target_link_libraries(keybind_test PRIVATE selecta_textui selecta_core)
  add_test(NAME keybind-unit COMMAND keybind_test)
endif()

# T37: the command palette and help overlay model.
if(NOT SEL_AMIGA)
  add_executable(palette_test ui/palette_test.c)
  target_link_libraries(palette_test PRIVATE selecta_textui selecta_core)
  add_test(NAME palette-unit COMMAND palette_test)
endif()

# T36 reachability (I77): the real shell, keys typed into the command line.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-cmdline
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m12/cmdline.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m12/cmdline.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/cmdline"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.cmake")
endif()

# T37 reachability (I78): Ctrl-P palette runs a verb picked by fuzzy query.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-palette
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m12/palette.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m12/palette.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/palette"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/browse_play.cmake")
endif()

# T36: the command line. argdump.sh stands in for `:!program`.
if(NOT SEL_AMIGA)
  add_executable(cmdline_test ui/cmdline_test.c)
  target_link_libraries(cmdline_test PRIVATE selecta_textui selecta_core)
  add_test(NAME cmdline-unit
    COMMAND cmdline_test "${CMAKE_CURRENT_SOURCE_DIR}/m12/argdump.sh" "${CMAKE_CURRENT_BINARY_DIR}/cmdline_argdump.txt")
endif()

if(NOT SEL_AMIGA)
  # T35 (row I79): the shell's control socket, driven by `selecta-cli send`.
  add_test(NAME control-socket
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m12/control_socket.sh" "$<TARGET_FILE:selecta>"
      "${SEL_CLI}" "${CMAKE_CURRENT_SOURCE_DIR}/m12/wait.keys" "${CMAKE_CURRENT_BINARY_DIR}/control_socket")

  # T38 (row I80): the integration host and the fake Now Playing backend.
  add_test(NAME integration-nowplaying
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
      "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m12/nowplaying.sel --integration nowplaying-test"
      "-DEXPECT_REGEX=np_record title=selecta test duration_ms=0 state=playing.*np_publish=3 np_command=1"
      -P "${SEL_EXPECT}")
  set_tests_properties(integration-nowplaying PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")
endif()

# T28 reachability (I82): the whole of M1.2 through the real shell, one run.
if(NOT SEL_AMIGA)
  add_test(NAME selecta-m12-reach
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m12/reach.keys"
      "-DBINDINGS=${CMAKE_CURRENT_SOURCE_DIR}/m12/reach.bindings"
      "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/m12/three.m3u"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m12/reach.screen"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m12/reach.cmake")
endif()
# T40, T48 (M1.3 Phase 1): viz taps, VU ballistics, spectrum bands, phase ring.
if(NOT SEL_AMIGA)
  add_executable(phase_test viz/phase_test.c)
  target_link_libraries(phase_test PRIVATE selecta_core m)
  add_test(NAME phase-tap COMMAND phase_test phase)
  add_test(NAME viz-vu COMMAND phase_test vu)
  add_test(NAME viz-spectrum COMMAND phase_test spectrum)
endif()
if(NOT SEL_AMIGA)
  add_test(NAME viz-taps
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_TESTMOD}"
      "-DOUT=${SEL_RENDER_OUT}/viz.wav" "-DDUMP=${CMAKE_CURRENT_BINARY_DIR}/viz/viz.txt"
      "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/m13/viz.expected"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/viz_check.cmake")
endif()

# T41 (rows I88, I89): RP_TRACKER_INFO and RP_TRACKER_CELLS, nuade and openmpt.
if(NOT SEL_AMIGA)
  add_test(NAME pattern-cells
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
      "-DARGS=script ${CMAKE_CURRENT_SOURCE_DIR}/m13/pattern.sel"
      "-DEXPECT_FILE=${CMAKE_CURRENT_SOURCE_DIR}/m13/pattern.expected"
      -P "${SEL_EXPECT}")
  set_tests_properties(pattern-cells PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/songs")
endif()

# T42 (rows I90, I91, I92): mute mask, SOLO, PITCH, TEMPO through render --script, nuade and openmpt.
if(NOT SEL_AMIGA)
  add_test(NAME voice-control
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DNUADE_MOD=${SEL_TESTMOD}" "-DMK_MOD=${SEL_MKMOD}"
      "-DZERO1_MOD=${SEL_ZERO1MOD}" "-DOUT_DIR=${SEL_RENDER_OUT}/voice"
      "-DSCRIPTS=${CMAKE_CURRENT_SOURCE_DIR}/m13"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/nuade/selecta-test-mute1.sha256"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/voice_check.cmake")
endif()
# T47 (rows I99, I100): crossfeed DSP. Unit half through the plugin ABI, render
# half through selecta-cli --dsp.
if(NOT SEL_AMIGA)
  add_executable(crossfeed_test dsp/crossfeed_test.c)
  target_link_libraries(crossfeed_test PRIVATE selecta_core sel_plugin_crossfeed)
  add_test(NAME crossfeed-unit COMMAND crossfeed_test)
  add_test(NAME crossfeed-render
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DMOD=${SEL_TESTMOD}"
      "-DOUT=${SEL_RENDER_OUT}/crossfeed.wav" "-DPLAIN=${SEL_RENDER_OUT}/crossfeed-plain.wav"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/golden/dsp/crossfeed.sha256"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/dsp/render_check.cmake")
endif()

# T43 (rows I93, I94): loop detect, REPEAT, fade, max_song_ms, silence_ms, stop-after options.
# T44 (rows I93, I95): crossfade and gapless. T45 (rows I93, I96): sleep timer and alarm.
if(NOT SEL_AMIGA)
  foreach(t loop:loop-fade crossfade:crossfade sleep:sleep-alarm)
    string(REPLACE ":" ";" parts "${t}")
    list(GET parts 0 file)
    list(GET parts 1 name)
    add_test(NAME ${name}
      COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DSCRIPTS=${CMAKE_CURRENT_SOURCE_DIR}/m13"
        "-DSONGS=${CMAKE_CURRENT_BINARY_DIR}/songs"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/${file}_check.cmake")
  endforeach()
endif()
# T46 (rows I97, I98): SAMPLE_LIST and SAMPLE_EXPORT (WAV, 8SVX) through the engine, libopenmpt adapter.
if(NOT SEL_AMIGA)
  add_test(NAME sample-export
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
      "-DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/m13/samples.sel"
      "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/m13/samples.expected"
      "-DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DGOLDEN_DIR=${CMAKE_CURRENT_SOURCE_DIR}/golden/samples"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/samples_check.cmake")
endif()
