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
# null, wav, nuade, openmpt, the crossfeed DSP, the text UI, the nowplaying-test and lastfm integrations and the voices-test, paula-shadow and paula14-shadow
# output (both host only), plus coreaudio and the
# macOS Now Playing plugin on macOS, or MPRIS on Linux when libsystemd was found; the negative tree adds one bad plugin.
set(SEL_EXPECT_PLUGINS 11)
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
set(SEL_LOOPMOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-loop.mod")
add_custom_command(OUTPUT "${SEL_LOOPMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --loop "${SEL_LOOPMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-loop.mod")
set(SEL_JUMP0MOD "${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-jump0.mod")
add_custom_command(OUTPUT "${SEL_JUMP0MOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs"
  COMMAND gen_testmod --jump0 "${SEL_JUMP0MOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-jump0.mod")
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
add_custom_target(testmod ALL DEPENDS "${SEL_TESTMOD}" "${SEL_SHORTMOD}" "${SEL_LOOPMOD}" "${SEL_JUMP0MOD}" "${SEL_MKMOD}" "${SEL_BREAKMOD}" "${SEL_ZERO1MOD}" "${SEL_SILENTMOD}" "${SEL_ALBUM_STAMP}")
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

# I190b: no libnix startup constructor (___initclock) in the linked Amiga selecta-cli; vamos cannot run it.
if(SEL_AMIGA_NM AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME no-startup-ctor
    COMMAND "${CMAKE_COMMAND}" "-DNM=${SEL_AMIGA_NM}" "-DAMIGA_CLI=${SEL_AMIGA_CLI}"
            -P "${PROJECT_SOURCE_DIR}/cmake/no_startup_ctor.cmake")
  set_tests_properties(no-startup-ctor PROPERTIES FIXTURES_REQUIRED amiga_cli)
endif()

# T12: mixer unit test (period step, resampler vectors, clipping, loop and
# queue semantics). Groups are listed so a resampler lands in the gate with
# the row that implements it.
add_executable(mixer_test mixer/mixer_test.c)
target_link_libraries(mixer_test PRIVATE selecta_core)
add_test(NAME mixer-unit COMMAND mixer_test period none linear cubic sinc default clip loop)

# T53 unit part: LED -3 dB point on a swept sine, width identities, Paula bypass.
add_executable(stage_test mixer/stage_test.c)
target_link_libraries(stage_test PRIVATE selecta_core)
add_test(NAME mixer-stage-unit COMMAND stage_test)
add_executable(loudness_gain_test mixer/loudness_gain_test.c)
target_link_libraries(loudness_gain_test PRIVATE selecta_core m)
add_test(NAME mixer-loudness-gain-unit COMMAND loudness_gain_test)
set_tests_properties(mixer-loudness-gain-unit PROPERTIES PASS_REGULAR_EXPRESSION "loudness_gain_unit_ok")
# T50: Paula voice sink conformance (chip copy, AUDxPER/AUDxVOL, loop registers).
add_executable(paula_voices_test paula/paula_voices_test.c "${PROJECT_SOURCE_DIR}/plugins/outputs/paula/paula_voices.c")
target_include_directories(paula_voices_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME paula-voices-unit COMMAND paula_voices_test)
# I118/I121: hardware and shadow outputs share one skeleton; hardware register bus; routing table.
set(SEL_PAULA_DIR "${PROJECT_SOURCE_DIR}/plugins/outputs/paula")
add_executable(paula_out_test paula/paula_out_test.c
  "${SEL_PAULA_DIR}/paula.c" "${SEL_PAULA_DIR}/shadow.c" "${SEL_PAULA_DIR}/paula_hw.c"
  "${SEL_PAULA_DIR}/paula_out.c" "${SEL_PAULA_DIR}/paula_voices.c")
set_source_files_properties("${SEL_PAULA_DIR}/paula.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=paula_entry)
set_source_files_properties("${SEL_PAULA_DIR}/shadow.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=shadow_entry)
target_include_directories(paula_out_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(paula_out_test PRIVATE selecta_core)
add_test(NAME paula-out-unit COMMAND paula_out_test)
# I118: SEL_MEM_SAMPLE maps to the chip RAM flags (sentinel: chip allocation counter).
add_executable(mem_class_test host/mem_class_test.c)
target_link_libraries(mem_class_test PRIVATE selecta_core)
add_test(NAME mem-class-unit COMMAND mem_class_test)

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

# Context-aware keys: Right pages the list, and seeks with the pattern panel up
# (Down lowers the volume there, Escape returns to the list).
if(NOT SEL_AMIGA)
  add_test(NAME selecta-context-keys
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
      "-DLIST_KEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/context_list.keys"
      "-DPLAYER_KEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/context_player.keys"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/context_keys"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/context_keys.cmake")
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

# Start-dir predicate: AmigaDOS volume paths count as absolute (M2 Q48).
if(NOT SEL_AMIGA)
  add_executable(library_path_test library_path_test.c)
  target_include_directories(library_path_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(library_path_test PRIVATE selecta_core)
  add_test(NAME library-path-unit COMMAND library_path_test)
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
# I101: text UI viz panel and pattern panel screen fixtures (shell, scripted keys, last screen dump).
if(NOT SEL_AMIGA)
  foreach(panel viz pattern)
    add_test(NAME ui-panel-${panel}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
        "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m13/panel_${panel}.keys"
        "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m13/panel_${panel}.screen"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/ui_panel_${panel}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/panel.cmake")
  endforeach()
  # Voice-kind player (nuade plays selecta-test.mod, openmpt plays selecta-test-mk.mod): levels and
  # scopes are real data, not the "No per voice data" notice.
  foreach(panel levels_voices viz_voices)
    add_test(NAME ui-panel-${panel}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_TESTMOD}"
        "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m13/panel_${panel}.keys"
        "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m13/panel_${panel}.screen"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/ui_panel_${panel}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/panel.cmake")
  endforeach()
  # T39 (M1.3 Phase 6): the M1.3 reachability test.
  add_test(NAME selecta-m13-reach
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_TESTMOD}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m13/reach.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m13/reach.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m13_reach"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/reach.cmake")
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
# T45 unit side: ramp shapes, midnight wrap and daily re-arm of src/engine/timer.c.
if(NOT SEL_AMIGA)
  add_executable(timer_test m13/timer_test.c)
  target_link_libraries(timer_test PRIVATE selecta_core)
  add_test(NAME sleep-ramp COMMAND timer_test)
endif()

# M1.3 Phase 7 gap rows I104a-d (T26 with DSP on, T46 nuade scan, key-level engine state, note dots and phase view).
if(APPLE)
  add_test(NAME no-alloc-render-dsp
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m0/no_alloc.sh"
      $<TARGET_FILE:selecta-cli> $<TARGET_FILE:count_malloc>
      "${SEL_TESTMOD}" "${SEL_RENDER_OUT}/no-alloc-dsp.wav" --dsp crossfeed)
endif()
if(NOT SEL_AMIGA)
  add_test(NAME sample-export-nuade
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
      "-DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/m13/nuade_samples.sel"
      "-DMOD=${SEL_TESTMOD}"
      "-DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/m13_samples"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/nuade_samples_check.cmake")
endif()
if(NOT SEL_AMIGA)
  foreach(case pitch_tempo_solo unsolo)
    if(case STREQUAL "pitch_tempo_solo")
      set(want "pitch_cents=100 tempo_percent=10 mute_mask=14 mutes=1")
    else()
      set(want "pitch_cents=0 tempo_percent=0 mute_mask=0 mutes=2")
    endif()
    add_test(NAME selecta-m13-keys-${case}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_TESTMOD}"
        "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m13/keys_${case}.keys" "-DWANT=${want}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m13_keys_${case}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/keys_engine_check.cmake")
  endforeach()
  # I104d: note dots (V x4) and phase view (V x5); the counter proves the draw path ran.
  foreach(view dots phase)
    set(counter dot_rows)
    if(view STREQUAL "phase")
      set(counter phase_points)
    endif()
    add_test(NAME ui-viz-${view}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_TESTMOD}"
        "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m13/viz_${view}.keys"
        "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m13/viz_${view}.screen"
        "-DCOUNTER=${counter}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/ui_viz_${view}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/viz_marks.cmake")
  endforeach()
endif()

# T46 (rows I97, I98): SAMPLE_LIST and SAMPLE_EXPORT (WAV, 8SVX) through the engine, libopenmpt adapter.
if(NOT SEL_AMIGA)
  add_test(NAME sample-export
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}"
      "-DSCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/m13/samples.sel"
      "-DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/m13/samples.expected"
      "-DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/m13_samples"
      "-DGOLDEN_DIR=${CMAKE_CURRENT_SOURCE_DIR}/golden/samples"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/samples_check.cmake")
endif()

# M2 Phase 0 (rows I105, I106): T49 to T61, red until the M2 rows land (plan 2026-10-24-m2-amiga.md).
# Every script fails with "RED[Tnn]: ..." naming the missing feature. Desktop tests run through
# selecta-cli; the "-vamos" twins run the 68k binary and compare against the desktop run.
if(NOT SEL_AMIGA)
  set(SEL_M2 "${CMAKE_CURRENT_SOURCE_DIR}/m2")
  set(SEL_M2_GOLDEN "${CMAKE_CURRENT_SOURCE_DIR}/golden/m2")
  set(SEL_M2_VAMOS "")
  if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
    set(SEL_M2_VAMOS "-DVAMOS=${SEL_VAMOS}" "-DAMIGA_CLI=${SEL_AMIGA_CLI}")
  endif()
  # sel_m2_test(name script mode [-Dvar=value ...]): mode is desktop or vamos.
  function(sel_m2_test name script mode)
    if(mode STREQUAL "vamos" AND NOT SEL_M2_VAMOS)
      return()
    endif()
    set(tname ${name})
    if(mode STREQUAL "vamos")
      set(tname ${name}-vamos)
    endif()
    add_test(NAME ${tname}
      COMMAND "${CMAKE_COMMAND}" "-DMODE=${mode}" "-DCLI=${SEL_CLI}" ${SEL_M2_VAMOS} "-DMOD=${SEL_TESTMOD}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/${tname}" "-DGOLDEN_DIR=${SEL_M2_GOLDEN}" ${ARGN}
        -P "${SEL_M2}/${script}.cmake")
    if(mode STREQUAL "vamos")
      set_tests_properties(${tname} PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
    endif()
  endfunction()

  # T49 registry machine filter, desktop and vamos.
  sel_m2_test(registry-machine-filter registry desktop)
  sel_m2_test(registry-machine-filter registry vamos)
  # T50 machine probe under vamos.
  sel_m2_test(amiga-machine-probe run vamos -DTAG=T50 "-DARGS1=machine"
    "-DCHECKS1=GFX~^(OCS|ECS|AGA)(\\+RTG)?$,AUDIO~paula" "-DARGS2=machine --tier planar" "-DCHECKS2=TIER==planar")
  # I110 clock source: desktop monotonic, vamos has no timer.device so DateStamp (rig: eclock, open).
  sel_m2_test(clock-source run desktop -DTAG=I110 "-DARGS1=machine" "-DCHECKS1=clock.source==monotonic")
  sel_m2_test(clock-source run vamos -DTAG=I110 "-DARGS1=machine" "-DCHECKS1=clock.source==datestamp")
  # T51 Paula voice registers: AUDxPER once per note, register log golden.
  foreach(mode desktop vamos)
    sel_m2_test(paula-voice-registers run ${mode} -DTAG=T51
      "-DARGS1=render --output paula-shadow @MOD@ --seconds 2" "-DCHECKS1=aud_writes>0,reg_log_sha256~^[0-9a-f]+$"
      -DGOLDEN_KEY=reg_log_sha256 "-DGOLDEN_FILE=${SEL_M2_GOLDEN}/paula-voice-registers.sha256")
  endforeach()
  # T52 paula14 split tables, desktop and vamos hashes equal.
  foreach(mode desktop vamos)
    sel_m2_test(paula14-tables run ${mode} -DTAG=T52
      "-DARGS1=render --output paula14-shadow @MOD@ --seconds 2" "-DCHECKS1=p14_frames>0,p14_max_err_lsb<=1" -DHASH_KEY=p14_hash)
  endforeach()
  # I126: a VOICE-kind song on a PCM output (no voice backend) goes through the mixer into the paula14 chip buffers.
  foreach(mode desktop vamos)
    sel_m2_test(paula14-routing run ${mode} -DTAG=T52
      "-DARGS1=render --output paula14-shadow @MOD@ --seconds 2" "-DCHECKS1=mixed_frames>0,p14_bufs>0")
  endforeach()
  # T53 LED filter and stereo width.
  # selecta-test.mod is mono (the protracker4 replayer does not pan), so a width knob cannot
  # move its hash; the desktop run also renders the stereo mk module (libopenmpt pans it).
  sel_m2_test(mixer-led-stereo mixer desktop "-DMKMOD=${SEL_MKMOD}")
  sel_m2_test(mixer-led-stereo mixer vamos "-DMKMOD=${SEL_MKMOD}")
  # I129 the LED filter and stereo width commands reach the stages (desktop; the 68k render is T53).
  sel_m2_test(stage-commands stage_commands desktop "-DMKMOD=${SEL_MKMOD}"
    "-DSCRIPT=${SEL_M2}/stage_commands.sel")
  # T55 Amiga text UI under vamos with scripted stdin.
  if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
    add_test(NAME amiga-text-ui-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DDESKTOP_SELECTA=$<TARGET_FILE:selecta>" "-DKEYS=${SEL_M2}/quit.keys"
        -P "${SEL_M2}/ui.cmake")
    set_tests_properties(amiga-text-ui-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
    # I138b AmigaDOS browse join and parent through the Amiga text UI.
    add_test(NAME amiga-browse-nav-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-browse-nav-vamos"
        -P "${SEL_M2}/browse_nav.cmake")
    set_tests_properties(amiga-browse-nav-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
  endif()
  # T57 library browse under vamos, T58 ToolTypes = CLI.
  sel_m2_test(amiga-library-browse browse vamos)
  sel_m2_test(amiga-tooltypes-cli tooltypes vamos)
  # I114 SPSC queue: 1000 messages through a wrapping queue, head/tail published inside the pal guard.
  foreach(mode desktop vamos)
    sel_m2_test(spsc-queue run ${mode} -DTAG=I114 "-DARGS1=spsc-selftest"
      "-DCHECKS1=spsc_pushed==1000,spsc_popped==1000,spsc_publishes==2000")
  endforeach()
  # T61 per-replayer config.
  sel_m2_test(rp-config-taglist rpconfig desktop)
  sel_m2_test(rp-config-taglist rpconfig vamos)
  # T54 no float in the interrupt path (needs the cross nm and a built build/amiga).
  if(SEL_AMIGA_NM AND EXISTS "${SEL_AMIGA_CLI}")
    add_test(NAME no-float-interrupt
      COMMAND "${CMAKE_COMMAND}" "-DNM=${SEL_AMIGA_NM}" "-DBUILD_DIR=${PROJECT_SOURCE_DIR}/build/amiga"
        -P "${SEL_M2}/nofloat.cmake")
    set_tests_properties(no-float-interrupt PROPERTIES FIXTURES_REQUIRED amiga_cli)
  endif()

  # Rig tests (T56, T58 priority read-back, T59, T60), label rig: skipped (never passed) while the rig is down.
  set(SEL_RIG_DIR "$ENV{HOME}/Code/UP-Term/vtcon/tools/rig")
  foreach(rt T56:rig-selecta-upterm T58:rig-task-priority T59:rig-ahi-render T60:rig-nofpu-voice-budget)
    string(REPLACE ":" ";" parts "${rt}")
    list(GET parts 0 tag)
    list(GET parts 1 name)
    add_test(NAME ${name}
      COMMAND "${CMAKE_COMMAND}" -DTAG=${tag} "-DRIG_DIR=${SEL_RIG_DIR}"
        "-DSELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta" "-DMOD=${SEL_TESTMOD}"
        "-DSELECTA_CLI=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta-cli/selecta-cli"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/${name}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
    set_tests_properties(${name} PROPERTIES LABELS rig RESOURCE_LOCK rig
      SKIP_REGULAR_EXPRESSION "SKIPPED rig not running")
  endforeach()
endif()
# I122/I123/I126: paula14 split tables, chip double buffer through a recording bus, routing of PCM outputs.
set(SEL_P14_DIR "${PROJECT_SOURCE_DIR}/plugins/outputs/paula14")
add_executable(p14_test paula/p14_test.c "${SEL_P14_DIR}/shadow.c" "${SEL_P14_DIR}/common/p14_split.c"
  "${SEL_P14_DIR}/common/p14_sink.c" "${SEL_P14_DIR}/common/p14_out.c")
set_source_files_properties("${SEL_P14_DIR}/shadow.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=shadow_entry)
target_include_directories(p14_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(p14_test PRIVATE selecta_core)
add_test(NAME paula14-unit COMMAND p14_test)

# I141 (D-M3-6): net fixture server. Setup/cleanup tests start and stop it for
# every test that REQUIRES the net-fixture fixture; all share RESOURCE_LOCK
# net-fixture. The self-test starts its own server and needs no fixture.
if(NOT SEL_AMIGA)
  set(SEL_NET_DIR "${CMAKE_CURRENT_SOURCE_DIR}/net")
  set(SEL_NET_WORK "${CMAKE_CURRENT_BINARY_DIR}/net")
  set(SEL_NET_CA "${SEL_NET_WORK}/ca")
  cmake_path(SET SEL_NET_CA NORMALIZE "${SEL_NET_CA}")
  execute_process(COMMAND "${CMAKE_COMMAND}" "-DCA_DIR=${SEL_NET_CA}" -P "${SEL_NET_DIR}/gen_test_ca.cmake")
  add_test(NAME net-fixture-start COMMAND "${CMAKE_COMMAND}" -DACTION=start "-DWORK=${SEL_NET_WORK}"
    "-DCA_DIR=${SEL_NET_CA}" -P "${SEL_NET_DIR}/fixture_ctl.cmake")
  add_test(NAME net-fixture-stop COMMAND "${CMAKE_COMMAND}" -DACTION=stop "-DWORK=${SEL_NET_WORK}"
    -P "${SEL_NET_DIR}/fixture_ctl.cmake")
  set_tests_properties(net-fixture-start PROPERTIES FIXTURES_SETUP net-fixture RESOURCE_LOCK net-fixture)
  set_tests_properties(net-fixture-stop PROPERTIES FIXTURES_CLEANUP net-fixture RESOURCE_LOCK net-fixture)
  find_program(SEL_PYTHON python3)
  if(SEL_PYTHON)
    add_test(NAME net-fixture-selftest COMMAND "${SEL_PYTHON}" -I "${SEL_NET_DIR}/fixture_server.py" selftest)
    set_tests_properties(net-fixture-selftest PROPERTIES LABELS m3)
  endif()

  # T67 (red until I152): the net guard. The mapped host must reach the fixture
  # (request log gets 1 line) and a non-loopback connect is refused with
  # net_blocked=1 (the guard checks the target after the map, so the mapped
  # fixture address 127.0.0.1 is what SELECTA_NET_ALLOW must list).
  add_test(NAME net-guard
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/net-guard"
      -P "${SEL_NET_DIR}/net_guard.cmake")
  set_tests_properties(net-guard PROPERTIES LABELS m3)

  # T68 (row I153): the HTTP/1.1 client, plain HTTP parts, against the fixture server.
  # Includes the real 30 s total timeout, so the test gets a 120 s budget. It runs after
  # net-guard because that test counts the whole request log.
  add_test(NAME http-client
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DCA_FILE=${SEL_NET_CA}/ca.pem" -P "${SEL_NET_DIR}/http_client.cmake")
  set_tests_properties(http-client PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS net-guard TIMEOUT 120)

  # T70 (row I155): passive FTP through selecta-cli fetch ftp://. After http-client because
  # that test reads no log but the FTP check reads the request log.
  add_test(NAME ftp-transport
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/ftp_transport.cmake")
  set_tests_properties(ftp-transport PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS http-client TIMEOUT 60)
endif()

# M3 Phase 1 (plan 2026-10-25-m3-library.md): T62 library-db-open, I144 to I146.
if(NOT SEL_AMIGA)
  if(APPLE)
    set(SEL_T62_SHIM $<TARGET_FILE:count_malloc>)
  else()
    set(SEL_T62_SHIM "")
  endif()
  add_test(NAME library-db-open
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m3/library_db_open.sh" $<TARGET_FILE:selecta-cli> ${SEL_T62_SHIM})
  set_tests_properties(library-db-open PROPERTIES LABELS m3)
endif()

# M3 Phase 2 (plan 2026-10-25-m3-library.md): T63 query-compile, I147 and I148.
if(NOT SEL_AMIGA)
  add_test(NAME query-compile
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m3/query_compile.sh" $<TARGET_FILE:selecta-cli>
      "${CMAKE_CURRENT_SOURCE_DIR}/m3/query.golden")
  set_tests_properties(query-compile PROPERTIES LABELS m3)
endif()

# M3 Phase 2b: T64 library-search, T65 play-query, T66 view-library-source (I149-I151).
if(NOT SEL_AMIGA)
  add_test(NAME library-search
    COMMAND sh m3/library_search.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/search.golden
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(library-search PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_searches=9")

  add_test(NAME play-query
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.expected
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(play-query PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "play_query_ok")

  add_test(NAME user-data
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/userdata.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/userdata.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/userdata.expected
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/userflags.sel ${CMAKE_CURRENT_SOURCE_DIR}/m3/userflags.expected
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/sounds-wrong.golden
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(user-data PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "userdata_ok")

  add_test(NAME cues
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/cues.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/cues.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/cues2.sel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(cues PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "cues_ok")

  add_test(NAME loudness-analysis
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/loudness_analysis.sh $<TARGET_FILE:selecta-cli> ${SEL_TESTMOD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(loudness-analysis PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "loudness_analysis_ok")

  # T90 gain and limiter part (I189): render applies the stored 16.16 gain and the -1 dBFS limiter.
  add_test(NAME loudness-playback
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/loudness_playback.sh $<TARGET_FILE:selecta-cli> ${SEL_TESTMOD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(loudness-playback PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "loudness_playback_ok")

  add_test(NAME share-links
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/share_links.sh $<TARGET_FILE:selecta-cli> $<TARGET_FILE:selecta>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/share_links.keys
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(share-links PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "share_ok links_parsed=1 songs_opened=1")

  add_test(NAME stickers
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/stickers.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/stickers.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/stickers2.sel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(stickers PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stickers_ok")

  add_test(NAME folders-tree
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/folders.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/folders.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/folders2.sel ${CMAKE_CURRENT_SOURCE_DIR}/m3/folders_tree.golden
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/folders_export.golden
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(folders-tree PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "folders_ok")

  add_test(NAME text-ui-folders-tree
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/folders.sel"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/folders_screen.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m3/folders_screen.screen"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/folders_screen"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/folders_screen.cmake")
  set_tests_properties(text-ui-folders-tree PROPERTIES LABELS m3)

  find_program(SEL_SQLITE3_META sqlite3)
  add_test(NAME metadata-panel
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_empty.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_screen.screen"
      "-DGOLDEN_LOCAL=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_local.screen"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/meta_screen"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_screen.cmake")
  set_tests_properties(metadata-panel PROPERTIES LABELS m3)

  add_test(NAME stats-screen
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_empty.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/stats.keys"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/stats_screen"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/stats_screen.cmake")
  set_tests_properties(stats-screen PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stats_screen ok")

  add_test(NAME view-library-source
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/view_query.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m3/view_query.screen"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/view_query"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/view_query.cmake")
  set_tests_properties(view-library-source PROPERTIES LABELS m3)
endif()

# M3 Phase 4 (plan 2026-10-25-m3-library.md): T71 source-local (scan part), T72 source-modland-key (I157, I158).
if(NOT SEL_AMIGA)
  add_test(NAME source-local
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_local.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/local.manifest ${SEL_TESTMOD} ${SEL_TESTMOD})
  set_tests_properties(source-local PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_local_ok")

  add_test(NAME source-modland-key
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_key.sh $<TARGET_FILE:selecta-cli>)
  set_tests_properties(source-modland-key PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_key_ok")
endif()

# T69 mirrors-ranking (row I156): the Mirrors transport through `selecta-cli library fetch`.
# Runs after ftp-transport because both read the shared request log. Includes one real 3 s
# first-byte timeout (the 4 s mirror) per ranking round, so the test gets a 60 s budget.
if(NOT SEL_AMIGA)
  add_test(NAME mirrors-ranking
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/mirrors_ranking.cmake")
  set_tests_properties(mirrors-ranking PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS ftp-transport TIMEOUT 60)
endif()

# M3 Phase 4b (I159): T71 archive part, zip and lha members scan as songs at <archive>/<member>.
if(NOT SEL_AMIGA)
  add_test(NAME source-archive
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_archive.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/make_archives.py ${SEL_TESTMOD})
  set_tests_properties(source-archive PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_archive_ok")
endif()

# M3 Phase 4b (I158a, I158b): T71 scan under vamos agrees with the desktop scan. It crashed (InvalidMemoryAccess
# in the SQLite open) until the Amiga executables requested their own stack (apps/common/amiga_stack.c).
if(NOT SEL_AMIGA AND SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME source-local-scan-vamos
    COMMAND "${CMAKE_COMMAND}" "-DMODE=vamos" "-DCLI=${SEL_CLI}" "-DVAMOS=${SEL_VAMOS}"
      "-DAMIGA_CLI=${SEL_AMIGA_CLI}" "-DMOD=${SEL_TESTMOD}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m3/source-local-scan-vamos"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/scan_vamos.cmake")
  set_tests_properties(source-local-scan-vamos PROPERTIES LABELS m3 FIXTURES_REQUIRED amiga_cli
    RESOURCE_LOCK vamos PASS_REGULAR_EXPRESSION "scan_vamos_ok")
endif()

# T75 cache-fetch (rows I165, I166): LIBRARY_FETCH through the file cache and its companions.
# Runs after mirrors-ranking because both read the shared request log.
if(NOT SEL_AMIGA)
  add_test(NAME cache-fetch
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      -P "${SEL_NET_DIR}/cache_fetch.cmake")
  set_tests_properties(cache-fetch PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS mirrors-ranking TIMEOUT 60)
endif()

# T75a cache-fetch-shell (row I166a): LIBRARY_FETCH through the selecta shell on the same library handle.
if(NOT SEL_AMIGA)
  add_test(NAME cache-fetch-shell
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv" -P "${SEL_NET_DIR}/cache_fetch_shell.cmake")
  set_tests_properties(cache-fetch-shell PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 60)
endif()

# I94a (row I94 refinement, user decision Q24): the first loop of a non-looping song counts as the song's end,
# so SONG_ENDED reports `end`; a song that loops by design (selecta-loop.mod, B01 jump) keeps `loop`.
# Reachability: the script drives the engine dispatch; the nuade adapter turns the wrap to order 0 into ENDED.
if(NOT SEL_AMIGA)
  add_test(NAME loop-first-end
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DSCRIPTS=${CMAKE_CURRENT_SOURCE_DIR}/m13"
      "-DSONGS=${CMAKE_CURRENT_BINARY_DIR}/songs"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m13/first_check.cmake")
endif()

# M3 Phase 5 (I205, I206): T98 text-ui-library-screen. The shell drives L, /, a query and Enter over
# a catalogue, then opens a --db filled by LIBRARY_SCAN.
if(NOT SEL_AMIGA)
  add_test(NAME text-ui-library-screen
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/library_screen.keys"
      "-DDBKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/library_db.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m3/library_screen.screen"
      "-DSONGS=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/library_screen"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/library_screen.cmake")
  set_tests_properties(text-ui-library-screen PROPERTIES LABELS m3)
endif()

# M3 Phase 4 (I160): T71 rescan and watcher part. A file added to one subdirectory of a fixture copy after
# the first scan rescans that directory alone (folder watcher, mtime poll, manual rescan).
if(NOT SEL_AMIGA)
  add_test(NAME source-watch
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_watch.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/local.manifest ${SEL_TESTMOD} ${SEL_TESTMOD})
  set_tests_properties(source-watch PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "source_watch_ok")
endif()

# M3 Phase 5 (I167): T76 cache-budget. LRU trim of the download cache to a budget scaled by sel_machine;
# kept and liked songs are exempt; trim_runs proves the real entry point (library trim) ran it.
if(NOT SEL_AMIGA)
  add_test(NAME cache-budget
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/cache_budget.sh $<TARGET_FILE:selecta-cli>)
  set_tests_properties(cache-budget PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "cache_budget_ok")
endif()

# M3 Phase 4 (I161): T72 source-modland. library sync modland over a 100-row allmods_md5 fixture plus changelogs, offline.
if(NOT SEL_AMIGA)
  add_test(NAME source-modland
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_modland.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3)
  set_tests_properties(source-modland PROPERTIES LABELS m3 TIMEOUT 120 PASS_REGULAR_EXPRESSION "source_modland_ok")
endif()

# M3 Phase 5 (I168): T77 prefetch. Playing a remote song prefetches the next queue entry into the cache;
# prefetch_fetches=1 is the sentinel, and playing the next song then makes no request.
if(NOT SEL_AMIGA)
  add_test(NAME prefetch
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      -P "${SEL_NET_DIR}/prefetch.cmake")
  set_tests_properties(prefetch PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 60 PASS_REGULAR_EXPRESSION "prefetch_ok")
endif()

# M3 Phase 5 (I169): T77 smart downloads. History and smart-playlist songs are fetched behind the foreground;
# smart_fetches=2 is the sentinel, and the off, metered, top and budget prefs each move it.
if(NOT SEL_AMIGA)
  add_test(NAME smart-downloads
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/smart.tsv"
      -P "${SEL_NET_DIR}/smart.cmake")
  set_tests_properties(smart-downloads PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 90 PASS_REGULAR_EXPRESSION "smart_ok")
endif()

# M3 Phase 5 (I167a): T76 part 2. The trim driven through LIBRARY_FETCH keeps a kept song and its companion.
if(NOT SEL_AMIGA)
  add_test(NAME cache-budget-fetch
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      -P "${SEL_NET_DIR}/cache_budget_fetch.cmake")
  set_tests_properties(cache-budget-fetch PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 60 PASS_REGULAR_EXPRESSION "cache_budget_fetch_ok")
endif()

if(NOT SEL_AMIGA)
  add_test(NAME source-hvsc
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_hvsc.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3)
  set_tests_properties(source-hvsc PROPERTIES LABELS m3 TIMEOUT 120 PASS_REGULAR_EXPRESSION "source_hvsc_ok")
endif()

if(NOT SEL_AMIGA)
  add_test(NAME source-hippo
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_hippo.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3)
  set_tests_properties(source-hippo PROPERTIES LABELS m3 TIMEOUT 120 PASS_REGULAR_EXPRESSION "source_hippo_ok")
endif()

# M3 Phase 5 (I168a): T77 part 2. A foreground fetch during a slow prefetch returns within a stated bound.
if(NOT SEL_AMIGA)
  add_test(NAME prefetch-preempt
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv"
      -P "${SEL_NET_DIR}/prefetch_preempt.cmake")
  set_tests_properties(prefetch-preempt PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 60 PASS_REGULAR_EXPRESSION "prefetch_preempt_ok")
endif()

# M3 Phase 4 (I161a): T72 network part. LIBRARY_SYNC modland <url> over the sync.modland.test fixture.
if(NOT SEL_AMIGA)
  add_test(NAME source-modland-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" -P "${SEL_NET_DIR}/modland_net.cmake")
  set_tests_properties(source-modland-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "modland_net_ok")
endif()

# M3 Phase 4 (I164): T74 publish part. PLAYLIST_PUBLISH through selecta-cli script against the fixture.
if(NOT SEL_AMIGA)
  add_test(NAME hippo-publish
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/hippo_publish.cmake")
  set_tests_properties(hippo-publish PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "hippo_publish_ok")
endif()

# M3 (I181): xxh32 published vectors and the UADE key; T85 genre part against the devilbox fixture, two runs.
if(NOT SEL_AMIGA)
  add_executable(xxh32_test xxh32_test.c)
  target_include_directories(xxh32_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(xxh32_test PRIVATE selecta_library)
  add_test(NAME xxh32-unit COMMAND xxh32_test)
  set_tests_properties(xxh32-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "xxh32_ok")
  add_test(NAME devilbox-genre
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre"
            -P "${SEL_NET_DIR}/devilbox_genre.cmake")
  set_tests_properties(devilbox-genre PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "devilbox_genre_ok")

  # T90 fetch part (row I188): the gain from the DEViLBOX loudness endpoint, one PUT after a desktop analysis.
  add_test(NAME loudness-fetch
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre"
            -P "${SEL_NET_DIR}/loudness_fetch.cmake")
  set_tests_properties(loudness-fetch PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "loudness_fetch_ok")

  # T86 similar part (row I183): also-by and similar through selecta-cli; the duplicates part is row I184.
  find_program(SEL_SQLITE3 sqlite3)
  add_test(NAME similar-duplicates
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre"
            "-DGOLDEN=${PROJECT_SOURCE_DIR}/tests/m3/similar.golden" "-DSQLITE3=${SEL_SQLITE3}"
            -P "${SEL_NET_DIR}/similar.cmake")
  set_tests_properties(similar-duplicates PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "similar_ok")

  # T88 (row I186): RADIO from a seed song, steering, skip exclusion, determinism.
  add_test(NAME radio
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/m3/radio.sel" "-DSQLITE3=${SEL_SQLITE3}"
            -P "${SEL_NET_DIR}/radio.cmake")
  set_tests_properties(radio PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "radio_ok")

  # T79 (row I173): RATE pushes PUT/DELETE /api/ratings with the Bearer token, batch reads, pending flush.
  add_test(NAME rate-devilbox
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3"
            "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre" -P "${SEL_NET_DIR}/rate_devilbox.cmake")
  set_tests_properties(rate-devilbox PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "rate_devilbox_ok")

  # T80 (row I174): VOTE posts /api/hip/vote with the I139 parameters, reads /api/hip/votes. Own fixture server and log.
  add_test(NAME vote-devilbox
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/vote-net" "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3"
            "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre" -P "${SEL_NET_DIR}/vote_devilbox.cmake")
  set_tests_properties(vote-devilbox PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "vote_devilbox_ok")

  # I173f: library close returns promptly while a rating push is stuck on a server that never answers; the rating
  # stays pending and is pushed on the next engine create. Own fixture server.
  add_test(NAME library-close-cancel
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/close-cancel-net"
            -P "${SEL_NET_DIR}/library_close_cancel.cmake")
  set_tests_properties(library-close-cancel PROPERTIES LABELS m3 TIMEOUT 90 PASS_REGULAR_EXPRESSION "library_close_cancel_ok")
endif()

# T95 (row I177): SCROBBLE_DUE from the engine, the last.fm scrobbler plugin, its signed POST and the offline queue.
if(NOT SEL_AMIGA)
  add_test(NAME scrobble
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3"
            "-DSONGS=${CMAKE_CURRENT_BINARY_DIR}/songs" -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/scrobble.cmake")
  set_tests_properties(scrobble PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 180 PASS_REGULAR_EXPRESSION "scrobble_ok")
endif()
