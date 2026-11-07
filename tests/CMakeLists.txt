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

# T5: the registry walked every descriptor and found only known tags. The expected count is the plugin list the registry
# is generated from (cmake/selecta_registry.cmake, every sel_add_plugin call), not a hand count: I512, the literal said 20
# after dr_flac, gme and ym joined and the registry held 23. The negative tree adds one bad plugin.
# I1130: with SELECTA_PLUGIN_LINK=MODULE (the macOS/Linux default) the built-in plugins are loadable modules listed in
# SEL_BUILTIN_MODULES, not in SEL_PLUGINS; the registry finds them at run time, so both lists count.
get_property(_sel_expect_plugins GLOBAL PROPERTY SEL_PLUGINS)
get_property(_sel_expect_modules GLOBAL PROPERTY SEL_BUILTIN_MODULES)
list(APPEND _sel_expect_plugins ${_sel_expect_modules})
list(LENGTH _sel_expect_plugins SEL_EXPECT_PLUGINS)
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

# I216: `play --output null` pulls on the command thread, which also destroys the engine; the debug-build thread
# assertion in sel_engine_destroy must not fire (it aborted with exit 134).
sel_cli_test(play-null-output "play ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream/sine.mp3 --output null --seconds 1"
  "-DEXPECT_REGEX=^callbacks=[0-9]+ frames=44100 out_latency_ms=[0-9]+ viz_delay_blocks=[0-9]+$")

# I1042 reachability: `play` is the product's output entry. The null output, given SELECTA_NULL_LATENCY_MS=340, queries a distinct 340 ms (the coreaudio
# plugin's nominal tag is 20), the engine must hold the visual frames back by that many 20 ms blocks (17), not by the
# nominal value (1 block) or by nothing.
sel_cli_test(play-output-latency "play ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream/sine.mp3 --output null --seconds 1"
  "-DEXPECT_REGEX=^callbacks=[0-9]+ frames=44100 out_latency_ms=340 viz_delay_blocks=17$")
# The null sink reports 0 ms by default (a headless sink has no device buffer); this test asks for the distinct value.
set_tests_properties(play-output-latency PROPERTIES ENVIRONMENT "SELECTA_NULL_LATENCY_MS=340")

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

# I494: the Amiga desktop plugin gate (cmake/desktop_bake_check.cmake) stops configure for SEL_DESKTOP_AGA without
# the host bake tools instead of silently building selecta without the desktop plugin.
add_test(NAME desktop-bake-check
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" "-DWORK=${PROJECT_BINARY_DIR}/tests/desktop-bake-check.tmp"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/dui/desktop_bake_check.cmake")

# I565: every "ui.*" pref a UI plugin reads is declared in prefs.def (ui.desktop.zoom was not, so prefs.conf could not set it).
add_test(NAME ui-prefs-declared
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/dui/ui_prefs_declared.cmake")
set_tests_properties(ui-prefs-declared PROPERTIES PASS_REGULAR_EXPRESSION "ui_prefs_declared_ok")

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
# I177a: a 16 minute module for the 4 minute scrobble cap; its own directory, so scans of songs/ do not see it.
set(SEL_LONGMOD "${CMAKE_CURRENT_BINARY_DIR}/songs-long/selecta-long-mk.mod")
add_custom_command(OUTPUT "${SEL_LONGMOD}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/songs-long"
  COMMAND gen_testmod --long --format mk "${SEL_LONGMOD}"
  DEPENDS gen_testmod
  COMMENT "Generating selecta-long-mk.mod")
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
add_custom_target(testmod ALL DEPENDS "${SEL_TESTMOD}" "${SEL_SHORTMOD}" "${SEL_LOOPMOD}" "${SEL_JUMP0MOD}" "${SEL_MKMOD}" "${SEL_BREAKMOD}" "${SEL_ZERO1MOD}" "${SEL_SILENTMOD}" "${SEL_LONGMOD}" "${SEL_ALBUM_STAMP}")
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

# I1039: the CoreAudio output's device list, pick, default follow and latency through its plugin entry, on a fake device layer.
if(APPLE)
  enable_language(OBJC)
  add_executable(coreaudio_device_test coreaudio_device_test.c
    "${PROJECT_SOURCE_DIR}/plugins/outputs/coreaudio/macos/coreaudio_av.m")
  target_include_directories(coreaudio_device_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_compile_options(coreaudio_device_test PRIVATE $<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>)
  target_link_libraries(coreaudio_device_test PRIVATE "-framework AudioToolbox" "-framework CoreAudio" "-framework CoreFoundation"
    "-framework AVFoundation" "-framework CoreMedia" "-framework Foundation")
  add_test(NAME coreaudio-device COMMAND coreaudio_device_test)
  set_tests_properties(coreaudio-device PROPERTIES PASS_REGULAR_EXPRESSION "coreaudio_device_ok")
  # I1039 reachability: headless selecta with the AirPlay route entry picked plays through the AVSampleBufferAudioRenderer
  # path; out_av_buffers in the --stats dump is the sentinel. Volume 0, so nothing is heard.
  add_test(NAME coreaudio-airplay-route
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_TESTMOD}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/coreaudio_airplay_route"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/coreaudio_airplay_route.cmake")
  set_tests_properties(coreaudio-airplay-route PROPERTIES SKIP_REGULAR_EXPRESSION "no output device available")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/isolate_config.cmake")
add_subdirectory(m0)
add_subdirectory(nuade_ab)
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_subdirectory(fixtures/plugin_dyn)
  add_subdirectory(fixtures/plugin_sidfeed)
  add_subdirectory(fixtures/plugin_sysfile) # I710ga6
  if(SELECTA_GPL_PLUGINS)
    add_subdirectory(fixtures/sndh)
  endif()
endif()
add_subdirectory(thread)

# T5 negative path: a second tree with a broken descriptor must fail lint.
add_test(NAME registry-tags-negative
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}"
    "-DBIN=${PROJECT_BINARY_DIR}/host-bad" -DEXPECT_PLUGINS=${SEL_EXPECT_BAD_PLUGINS} "-DGEN=${CMAKE_GENERATOR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m0/bad_plugin.cmake")

# T7: every plugin compiles with include/ as its only project include path.
add_test(NAME sdk-isolation
  COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target sdk-isolation)
# I228: the include check rejects "../other/x.h" (fixture plugin) and accepts a clean plugin.
add_test(NAME sdk-isolation-fixture
  COMMAND "${CMAKE_COMMAND}" "-DSRC=${PROJECT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/sdk_isolation/check_fixture.cmake")
set_tests_properties(sdk-isolation-fixture PROPERTIES PASS_REGULAR_EXPRESSION "sdk_isolation_fixture_ok")

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

# I214: config directory falls back to PROGDIR:Selecta when ENVARC: is unassigned (requesters off while probing).
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME amiga-config-dir
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DAMIGA_CLI=${SEL_AMIGA_CLI}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_config_dir.cmake")
  set_tests_properties(amiga-config-dir PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 120)

  # I218d: Latin-1 file names listed as UTF-8 and opened by that name, through library browse --names.
  add_test(NAME amiga-latin1-name
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DAMIGA_CLI=${SEL_AMIGA_CLI}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_latin1_name.cmake"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(amiga-latin1-name PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 120)
endif()

# Library open on a low file descriptor (libnix hands SQLite fd 0 after close(0); SQLite must not reopen /dev/null).
set(SEL_AMIGA_LOWFD "${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta-cli/selecta-lowfd-test")
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME amiga-library-opens-on-low-file-descriptor
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DLOWFD_TEST=${SEL_AMIGA_LOWFD}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_lowfd.cmake"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(amiga-library-opens-on-low-file-descriptor PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 120)
endif()

# I376: the bottom-of-stack canary on the 68k binary: a normal run is clean, an overflow prints the error line.
set(SEL_AMIGA_STACK_CANARY "${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta-cli/selecta-stack-canary-test")
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME amiga-stack-canary
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DCANARY_TEST=${SEL_AMIGA_STACK_CANARY}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_stack_canary.cmake"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(amiga-stack-canary PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 120)
endif()

# I287: selecta (68k) overrides libnix's Ctrl-C exit so the desktop UI closes its RTG screen.
set(SEL_AMIGA_SELECTA "${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/CMakeFiles/selecta.dir/main.c.obj")
find_program(SEL_AMIGA_NM m68k-amigaos-nm HINTS "$ENV{HOME}/opt/amiga/bin" /opt/amiga/bin)
if(SEL_AMIGA_NM AND EXISTS "${SEL_AMIGA_SELECTA}")
  add_test(NAME amiga-selecta-overrides-libnix-ctrl-c-exit
    COMMAND "${CMAKE_COMMAND}" "-DNM=${SEL_AMIGA_NM}" "-DMAIN_OBJ=${SEL_AMIGA_SELECTA}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/check_no_libnix_abort.cmake")
endif()

# I225: a new database on libnix (lseek past the end grows the file with junk; ftruncate returns the size).
set(SEL_AMIGA_DBCREATE "${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta-cli/selecta-dbcreate-test")
if(SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME amiga-new-library-database-is-not-junk
    COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}" "-DDBCREATE_TEST=${SEL_AMIGA_DBCREATE}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_dbcreate.cmake"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  set_tests_properties(amiga-new-library-database-is-not-junk PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 150)
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
  set(SEL_VAMOS_FAST_RAM vamos-image) # free RAM = 8 MiB less vamos use less the loaded hunks: derived from the binary
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

# I213a: the stb_vorbis FPU objects call no libm and no soft-float routine (nm -u on the 68k objects).
if(SEL_AMIGA_NM AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME stb-vorbis-fpu-symbols
    COMMAND "${CMAKE_COMMAND}" "-DNM=${SEL_AMIGA_NM}" "-DBUILD_DIR=${PROJECT_SOURCE_DIR}/build/amiga"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/stb_vorbis_fpu.cmake")
  set_tests_properties(stb-vorbis-fpu-symbols PROPERTIES FIXTURES_REQUIRED amiga_cli LABELS m3)
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
add_executable(paula_voices_test paula/paula_voices_test.c)
target_link_libraries(paula_voices_test PRIVATE selecta_plugin_shared)
target_include_directories(paula_voices_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME paula-voices-unit COMMAND paula_voices_test)
# I118/I121: hardware and shadow outputs share one skeleton; hardware register bus; routing table.
set(SEL_PAULA_DIR "${PROJECT_SOURCE_DIR}/plugins/outputs/paula")
add_executable(paula_out_test paula/paula_out_test.c
  "${SEL_PAULA_DIR}/paula.c" "${SEL_PAULA_DIR}/shadow.c" "${SEL_PAULA_DIR}/paula_out.c")
set_source_files_properties("${SEL_PAULA_DIR}/paula.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=paula_entry)
set_source_files_properties("${SEL_PAULA_DIR}/shadow.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=shadow_entry)
target_include_directories(paula_out_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(paula_out_test PRIVATE selecta_core selecta_plugin_shared)
add_test(NAME paula-out-unit COMMAND paula_out_test)
# I118: SEL_MEM_SAMPLE maps to the chip RAM flags (sentinel: chip allocation counter).
add_executable(mem_class_test host/mem_class_test.c)
target_link_libraries(mem_class_test PRIVATE selecta_core)
add_test(NAME mem-class-unit COMMAND mem_class_test)
# I222: the default output follows the machine's SEL_MACHINE_AUDIO list and the registry.
add_executable(default_output_test host/default_output_test.c)
target_link_libraries(default_output_test PRIVATE selecta_registry)
add_test(NAME default-output-unit COMMAND default_output_test)
# I710ga1: RP_SYSFILE declarations parse out of a plugin's hook list (sel_registry_sysfiles).
add_executable(sysfile_tags_test host/sysfile_tags_test.c)
target_link_libraries(sysfile_tags_test PRIVATE selecta_registry)
add_test(NAME sysfile-tags-unit COMMAND sysfile_tags_test)
set_tests_properties(sysfile-tags-unit PROPERTIES PASS_REGULAR_EXPRESSION "sysfile_tags_ok")
# The default integration follows the OS preference list and the registry ("none" when not built).
add_executable(default_integration_test host/default_integration_test.c)
target_link_libraries(default_integration_test PRIVATE selecta_registry)
if(SEL_HAVE_MPRIS)
  target_compile_definitions(default_integration_test PRIVATE SEL_HAVE_MPRIS=1)
endif()
add_test(NAME default-integration-unit COMMAND default_integration_test)
# I729: a prefs.conf key (wav.path) reaches the output plugin through the shell host (host->config_get).
add_test(NAME shell-config-get COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
  "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/shell-config-get" -P "${CMAKE_CURRENT_SOURCE_DIR}/host/shell_config_get.cmake")
set_tests_properties(shell-config-get PROPERTIES TIMEOUT 120)

# T24: browse listing and view model through the engine command queue and
# snapshot. The temp tree is created inside the generated songs directory.
add_executable(view_test view/view_test.c)
target_link_libraries(view_test PRIVATE selecta_registry)
add_test(NAME view-unit COMMAND view_test "${CMAKE_CURRENT_BINARY_DIR}/songs" browse sort cursor activate window style)
# T32: grouping, node ids, expand/collapse, drilldown, cursor across styles.
add_test(NAME view-tree COMMAND view_test "${CMAKE_CURRENT_BINARY_DIR}/songs" tree)
# Both rebuild the same song directory.
if(NOT SEL_AMIGA)
  # MUI-P3-5, MUI-P3-6 (engine half): the engine against the MUI plugin's planar and RTG style masks.
  target_link_libraries(view_test PRIVATE mui_map)
  target_compile_definitions(view_test PRIVATE SEL_VIEW_TEST_MUI=1)
  add_test(NAME mui-style-refusal COMMAND view_test "${CMAKE_CURRENT_BINARY_DIR}/songs" mui-refusal)
  add_test(NAME mui-style-keep COMMAND view_test "${CMAKE_CURRENT_BINARY_DIR}/songs" mui-keep)
  set_tests_properties(mui-style-refusal mui-style-keep PROPERTIES TIMEOUT 30 LABELS mui RESOURCE_LOCK view_songs_dir)
endif()
set_tests_properties(view-unit view-tree PROPERTIES RESOURCE_LOCK view_songs_dir)

# T33: title-format resolver and PREF_GET / PREF_SET (src/engine/format.c, prefs.c).
add_executable(format_test view/format_test.c)
target_link_libraries(format_test PRIVATE selecta_registry)
target_include_directories(format_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
add_test(NAME title-format COMMAND format_test)

# I786a: library.sync_snoozed with 11 sources (> 128 chars) survives set, save and load; SEL_PREF_VALUE_MAX is the exact limit.
add_executable(prefs_value_max_test engine/prefs_value_max_test.c)
target_link_libraries(prefs_value_max_test PRIVATE selecta_registry)
target_include_directories(prefs_value_max_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
add_test(NAME prefs-value-max COMMAND prefs_value_max_test)
set_tests_properties(prefs-value-max PROPERTIES PASS_REGULAR_EXPRESSION "prefs_value_max_ok")

# T25: text UI cell grid, key parser, theme tokens and POSIX terminal device
# (plugins/ui/text). Desktop only: the Amiga text UI runs in UP-Term in M2.
if(NOT SEL_AMIGA)
  add_executable(grid_test grid/grid_test.c)
  target_link_libraries(grid_test PRIVATE selecta_textui)
  add_test(NAME grid-unit COMMAND grid_test)
  # I60 fix: a keys-file `wait` ends under a clockless and a device-clocked
  # shell (it hung when the shell passed timeout 0).
  add_executable(text_ui_test ui/text_ui_test.c)
  target_link_libraries(text_ui_test PRIVATE selecta_registry)
  add_test(NAME text-ui-keys-wait COMMAND text_ui_test "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(text-ui-keys-wait PROPERTIES TIMEOUT 30)
endif()

# Amiga MUI UI (plan 2026-10-25-amiga-mui.md): MUI-P0-4 sentinel tag, MUI-P1-2 descriptor, MUI-P1-1 build split,
# MUI-P0-6 SDK probe. Host only; the plugin entry is compiled straight into the test (it is not registered on the host).
# I685c: the MUI class package source tree; every test that reads a shipped class source starts from this one path.
set(MCC_DIR "${PROJECT_SOURCE_DIR}/plugins/shared/mcc")
add_test(NAME mcc-include-check COMMAND "${CMAKE_COMMAND}" "-DMCC_DIR=${MCC_DIR}" "-DUI_DIR=${PROJECT_SOURCE_DIR}/plugins/ui/mui"
  "-DSDK_DIR=${PROJECT_SOURCE_DIR}/include/selecta" "-DROOTS=${MCC_CHECK_ROOTS}" -P "${PROJECT_SOURCE_DIR}/cmake/mcc_check_includes.cmake")
set_tests_properties(mcc-include-check PROPERTIES TIMEOUT 30)
if(NOT SEL_AMIGA)
  add_executable(mui_test ui/mui_test.c "${PROJECT_SOURCE_DIR}/plugins/ui/mui/mui_ui.c")
  target_include_directories(mui_test PRIVATE "${PROJECT_SOURCE_DIR}/plugins/ui/mui/map")
  target_compile_definitions(mui_test PRIVATE SEL_PLUGIN_ENTRY=sel_plugin_entry_mui_test SEL_PLUGIN_KIND_BUILD=5)
  target_link_libraries(mui_test PRIVATE selecta_registry mui_map)
  add_executable(mui_fontprobe_test ui/mui_fontprobe_test.c "${MCC_DIR}/src/map/mui_fontprobe.c")
  target_include_directories(mui_fontprobe_test PRIVATE "${MCC_DIR}/src")
  add_test(NAME mui-fontprobe COMMAND mui_fontprobe_test)
  set_tests_properties(mui-fontprobe PROPERTIES TIMEOUT 30 LABELS mui)
  add_test(NAME mui-descriptor COMMAND mui_test "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME mui-stat-tag COMMAND mui_test "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(mui-descriptor mui-stat-tag PROPERTIES TIMEOUT 30 LABELS mui)
  add_executable(mui_contract_test ui/mui_contract_test.c "${PROJECT_SOURCE_DIR}/plugins/ui/mui/mui_ui.c" "${PROJECT_SOURCE_DIR}/plugins/scripting/arexx/arexx_core.c")
  target_include_directories(mui_contract_test PRIVATE "${MCC_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/ui/mui/map" "${PROJECT_SOURCE_DIR}/plugins/scripting/arexx")
  target_compile_definitions(mui_contract_test PRIVATE SEL_PLUGIN_ENTRY=sel_plugin_entry_mui_test SEL_PLUGIN_KIND_BUILD=5 "MUI_UI_SOURCE=\"${PROJECT_SOURCE_DIR}/plugins/ui/mui/mui_ui.c\"" "MUI_DIR=\"${PROJECT_SOURCE_DIR}/plugins/ui/mui\"" "MCC_DIR=\"${MCC_DIR}\"")
  target_link_libraries(mui_contract_test PRIVATE selecta_registry mui_map)
  add_test(NAME mui-wait-contract COMMAND mui_contract_test wait)
  add_test(NAME mui-registry COMMAND mui_contract_test registry)
  add_test(NAME mui-drop COMMAND mui_contract_test drop)
  add_test(NAME mui-open-failure COMMAND mui_contract_test failure)
  add_test(NAME mui-open-nomui COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
    "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m2/quit.keys" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mui-open-nomui"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/ui/mui_nomui.cmake")
  add_test(NAME mui-song-arg COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
    "-DSONG=${SEL_TESTMOD}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mui-song-arg"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/ui/mui_song_arg.cmake")
  set_tests_properties(mui-song-arg PROPERTIES TIMEOUT 120 LABELS mui)
  add_test(NAME mui-exec-queue COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
    "-DSONG=${SEL_TESTMOD}" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mui-exec-queue"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/ui/mui_exec_queue.cmake")
  set_tests_properties(mui-exec-queue PROPERTIES TIMEOUT 120 LABELS mui)
  add_test(NAME mui-status-face COMMAND mui_contract_test status-face)
  add_test(NAME mui-volume-row COMMAND mui_contract_test volume-row)
  add_test(NAME mui-player-bar COMMAND mui_contract_test player-bar)
  add_test(NAME mui-sidebar-icons COMMAND mui_contract_test sidebar-icons)
  add_test(NAME mui-compact-rail COMMAND mui_contract_test compact-rail)
  add_test(NAME mui-compact-player COMMAND mui_contract_test compact-player)
  add_test(NAME mui-player-bar-reach COMMAND mui_contract_test player-bar-reach)
  add_test(NAME mui-player-band-sizes COMMAND mui_contract_test player-band-sizes) # I973a
  add_test(NAME mui-icon-square COMMAND mui_contract_test icon-square) # I973b
  add_test(NAME mui-subsong-bar COMMAND mui_contract_test subsong-bar) # I973c
  add_test(NAME mui-viz-chooser COMMAND mui_contract_test viz-chooser)
  add_test(NAME mui-overlays COMMAND mui_contract_test overlays)
  add_test(NAME mui-dialog-windows COMMAND mui_contract_test dialog-windows)
  add_test(NAME mui-tooltip-flat COMMAND mui_contract_test tooltip-flat)
  add_test(NAME mui-menu COMMAND mui_contract_test menu)
  add_test(NAME mui-style-menu COMMAND mui_contract_test style-menu)
  add_test(NAME mui-theme COMMAND mui_contract_test theme)
  add_test(NAME mui-pens COMMAND mui_contract_test pens)
  add_test(NAME mui-theme-file COMMAND mui_contract_test theme-file "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME mui-theme-live COMMAND mui_contract_test theme-live "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME mui-theme-live-repaint COMMAND mui_contract_test theme-live-repaint "${CMAKE_CURRENT_BINARY_DIR}")
  add_test(NAME mui-icon-buttons COMMAND mui_contract_test icon-buttons)
  add_test(NAME mui-aabtn-tokens COMMAND mui_contract_test aabtn-tokens)
  add_test(NAME mui-aacheck COMMAND mui_contract_test aacheck)
  add_test(NAME mui-newplaylist COMMAND mui_contract_test newplaylist)
  add_test(NAME mui-libfilter COMMAND mui_contract_test libfilter)
  add_test(NAME mui-icon-rows COMMAND mui_contract_test icon-rows)
  add_test(NAME mui-screen-plan COMMAND mui_contract_test screen-plan)
  add_test(NAME mui-aa-aspect COMMAND mui_contract_test aa-aspect)
  add_test(NAME mui-screen-rtg-flag COMMAND mui_contract_test screen-rtg-flag)
  add_test(NAME mui-window-borderless COMMAND mui_contract_test window-borderless)
  add_test(NAME mui-menu-dispatch COMMAND mui_contract_test menu-dispatch)
  add_test(NAME mui-weights COMMAND mui_contract_test weights)
  add_test(NAME mui-topbar COMMAND mui_contract_test topbar)
  add_test(NAME mui-route COMMAND mui_contract_test route)
  add_test(NAME mui-route-reach COMMAND mui_contract_test route-reach)
  add_test(NAME mui-tiles-reach COMMAND mui_contract_test tiles-reach)
  add_test(NAME mui-relayout COMMAND mui_contract_test relayout)
  add_test(NAME mui-transport-map COMMAND mui_contract_test transport)
  add_test(NAME mui-seek-row COMMAND mui_contract_test seek-row)
  add_test(NAME mui-dirty-map COMMAND mui_contract_test dirty)
  add_test(NAME mui-keys COMMAND mui_contract_test keys)
  add_test(NAME mui-list-regular COMMAND mui_contract_test list-regular)
  add_test(NAME mui-list-tree COMMAND mui_contract_test list-tree)
  add_test(NAME mui-list-drill COMMAND mui_contract_test list-drill)
  add_test(NAME mui-columns COMMAND mui_contract_test columns)
  add_test(NAME mui-style-mask COMMAND mui_contract_test style-mask)
  add_test(NAME mui-list-pending COMMAND mui_contract_test list-pending)
  add_test(NAME mui-list-dbl-row COMMAND mui_contract_test list-dbl-row)
  add_test(NAME mui-list-backends COMMAND mui_contract_test list-backends)
  add_test(NAME mui-home COMMAND mui_contract_test home)
  # MUI-P8 parity rows: one executable, one ctest per item (plan section P8).
  add_executable(mui_p8_test ui/mui_p8_test.c)
  target_include_directories(mui_p8_test PRIVATE "${PROJECT_SOURCE_DIR}/plugins/ui/mui/map")
  target_link_libraries(mui_p8_test PRIVATE mui_map selecta_uimodel)
  add_test(NAME mui-p8-transport-extras COMMAND mui_p8_test transport-extras)
  add_test(NAME mui-p8-voices-programs COMMAND mui_p8_test voices-programs)
  add_test(NAME mui-p8-library-modes COMMAND mui_p8_test library-modes)
  set_tests_properties(mui-p8-library-modes PROPERTIES TIMEOUT 30 LABELS mui)
  add_test(NAME mui-p8-queue-scope COMMAND mui_p8_test queue-scope)
  target_compile_definitions(mui_p8_test PRIVATE MUI_SRC_ROOT="${PROJECT_SOURCE_DIR}")
  foreach(t view-buttons tile-steps amiga-settings doors statistics panels sources infoline tooltips remember pref-keys)
    add_test(NAME mui-p8-${t} COMMAND mui_p8_test ${t})
    set_tests_properties(mui-p8-${t} PROPERTIES TIMEOUT 30 LABELS mui)
  endforeach()
  set_tests_properties(mui-p8-queue-scope PROPERTIES TIMEOUT 30 LABELS mui)
  set_tests_properties(mui-p8-transport-extras mui-p8-voices-programs PROPERTIES TIMEOUT 30 LABELS mui)
  add_test(NAME mui-doors COMMAND mui_contract_test doors)
  add_test(NAME mui-sidebar COMMAND mui_contract_test sidebar)
  add_test(NAME mui-panes COMMAND mui_contract_test panes)
  add_test(NAME mui-library COMMAND mui_contract_test library)
  add_test(NAME mui-toast COMMAND mui_contract_test toast)
  add_test(NAME mui-help COMMAND mui_contract_test help)
  add_executable(mui_canvas_test ui/mui_canvas_test.c)
  target_link_libraries(mui_canvas_test PRIVATE mui_map selecta_gui)
  target_compile_definitions(mui_canvas_test PRIVATE MUI_GRID_GOLDEN="bf70b206eb67b88c")
  add_test(NAME mui-canvas-geom COMMAND mui_canvas_test geom)
  add_test(NAME mui-grid COMMAND mui_canvas_test grid)
  add_test(NAME mui-art-path COMMAND mui_canvas_test art-path)
  set_tests_properties(mui-canvas-geom mui-grid mui-art-path PROPERTIES TIMEOUT 30 LABELS mui)
  add_executable(mui_aa_test ui/mui_aa_test.c)
  target_include_directories(mui_aa_test PRIVATE "${PROJECT_SOURCE_DIR}/plugins/ui/mui/map")
  target_link_libraries(mui_aa_test PRIVATE mui_map selecta_uimodel)
  target_compile_definitions(mui_aa_test PRIVATE "AABTN_SOURCE=\"${MCC_DIR}/src/class/mui_aabtn_class.c\"" "AADRAW_SOURCE=\"${MCC_DIR}/src/mui_aa_draw.c\"" "UI_SOURCE=\"${PROJECT_SOURCE_DIR}/plugins/ui/mui/mui_ui.c\"" "AATEXT_SOURCE=\"${MCC_DIR}/src/class/mui_aatext_class.c\"" "AAPANEL_SOURCE=\"${MCC_DIR}/src/class/mui_aapanel_class.c\"" "AATABS_SOURCE=\"${MCC_DIR}/src/class/mui_aatabs_class.c\"" "AABALANCE_SOURCE=\"${MCC_DIR}/src/class/mui_aabalance_class.c\"" "AASLIDER_SOURCE=\"${MCC_DIR}/src/class/mui_aaslider_class.c\"" "AACYCLE_SOURCE=\"${MCC_DIR}/src/class/mui_aacycle_class.c\"" "AALIST_SOURCE=\"${MCC_DIR}/src/class/mui_aalist_class.c\"" "LISTVIEW_SOURCE=\"${PROJECT_SOURCE_DIR}/plugins/ui/mui/mui_listview_class.c\"" "AASCROLLER_SOURCE=\"${MCC_DIR}/src/class/mui_aascroller_class.c\"" "AAFIELD_SOURCE=\"${MCC_DIR}/src/class/mui_aafield_class.c\"" "MUI_CMAKE=\"${MCC_DIR}/CMakeLists.txt\"")
  foreach(t round ramp state tokens check)
    add_test(NAME mui-aa-${t} COMMAND mui_aa_test ${t})
    set_tests_properties(mui-aa-${t} PROPERTIES TIMEOUT 30 LABELS mui)
  endforeach()
  foreach(t icon-ratio text-fit text-maxw text-tone text-source panel-source tabs tabs-source balance-geom balance-source slider-geom slider-partial slider-source btn-align transport-icons cycle-step cycle-source field-plan field-source field-draw-flags field-placeholder scroller-geom list-rows list-cols list-source i667n i667n-root tight-pad screen-font)
    add_test(NAME mui-${t} COMMAND mui_aa_test ${t})
    set_tests_properties(mui-${t} PROPERTIES TIMEOUT 30 LABELS mui)
  endforeach()
  # I685f: spec structs map to tag lists; no class reads a spec id; UP_TAG( defined only in up.h.
  add_executable(mui_tagmap_test ui/mui_tagmap_test.c)
  target_include_directories(mui_tagmap_test PRIVATE "${PROJECT_SOURCE_DIR}/plugins/ui/mui/map" "${MCC_DIR}/include" "${MCC_DIR}/src/class")
  target_link_libraries(mui_tagmap_test PRIVATE mui_map selecta_uimodel)
  target_compile_definitions(mui_tagmap_test PRIVATE "MCC_DIR=\"${MCC_DIR}\"")
  add_test(NAME mui-tagmap COMMAND mui_tagmap_test tags)
  add_test(NAME mui-tagmap-source COMMAND mui_tagmap_test source)
  set_tests_properties(mui-tagmap mui-tagmap-source PROPERTIES TIMEOUT 30 LABELS mui)
  # I685e2: the package log gate (aa_log quiet unless UP_MCC_DEBUG / ENV:UP-MCC/Debug is set) and the source check that no other stderr writer remains.
  add_executable(mcc_log_test ui/mcc_log_test.c)
  target_link_libraries(mcc_log_test PRIVATE sel_mcc_static)
  target_compile_definitions(mcc_log_test PRIVATE "MCC_DIR=\"${MCC_DIR}\"")
  foreach(t gate-off gate-on source)
    add_test(NAME mcc-log-${t} COMMAND mcc_log_test ${t} "${CMAKE_CURRENT_BINARY_DIR}/mcc_log_${t}.txt")
    set_tests_properties(mcc-log-${t} PROPERTIES TIMEOUT 30 LABELS mui)
  endforeach()
  # I685e2 part 2: the package font search list (setter, theme key font.file, default) and the source check that no Selecta:/RAM: path remains.
  # I685e4: the package links alone (no mui_map on the link line) and serves the baked icon table.
  add_executable(mcc_link_test ui/mcc_link_test.c)
  target_link_libraries(mcc_link_test PRIVATE sel_mcc_static)
  add_test(NAME mcc-link-no-map COMMAND mcc_link_test)
  set_tests_properties(mcc-link-no-map PROPERTIES TIMEOUT 30 LABELS mui)

  add_executable(mcc_font_list_test ui/mcc_font_list_test.c)
  target_link_libraries(mcc_font_list_test PRIVATE sel_mcc_static)
  target_compile_definitions(mcc_font_list_test PRIVATE "MCC_DIR=\"${MCC_DIR}\"")
  add_test(NAME mcc-font-list-order COMMAND mcc_font_list_test order "${CMAKE_CURRENT_BINARY_DIR}/mcc_font_list_theme.txt")
  add_test(NAME mcc-font-list-source COMMAND mcc_font_list_test source)
  set_tests_properties(mcc-font-list-order mcc-font-list-source PROPERTIES TIMEOUT 30 LABELS mui)
  # I970: the baked 16x8 tall-pixel icon bodies (List, Repeat, Stop) keep their detail; the revert proof is the old baker.
  add_executable(mui_icon_fit_test ui/mui_icon_fit_test.c)
  target_include_directories(mui_icon_fit_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(mui_icon_fit_test PRIVATE mui_map)
  add_test(NAME mui-icon-fit COMMAND mui_icon_fit_test)
  set_tests_properties(mui-icon-fit PROPERTIES TIMEOUT 30 LABELS mui)
  add_executable(mui_viz_test ui/mui_viz_test.c)
  target_link_libraries(mui_viz_test PRIVATE mui_map selecta_gui selecta_uimodel)
  add_test(NAME mui-viz-surface COMMAND mui_viz_test surface)
  add_test(NAME mui-budget COMMAND mui_viz_test budget)
  add_test(NAME mui-copper-tint COMMAND mui_viz_test copper)
  add_test(NAME mui-viz-set COMMAND mui_viz_test set)
  set_tests_properties(mui-viz-surface mui-budget mui-copper-tint mui-viz-set PROPERTIES TIMEOUT 30 LABELS mui)
  add_executable(gui_dither_test ui/gui_dither_test.c)
  target_link_libraries(gui_dither_test PRIVATE selecta_gui)
  target_compile_definitions(gui_dither_test PRIVATE GRADIENT_GOLDEN=0x6b25f44c8d897ac0ull COVER_GOLDEN=0x5feee6196fcf1ca2ull)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bitart")
  add_test(NAME gui-dither-atkinson COMMAND gui_dither_test dither)
  add_test(NAME mui-art-planar-cache COMMAND gui_dither_test cache "${CMAKE_CURRENT_BINARY_DIR}/bitart")
  set_tests_properties(gui-dither-atkinson mui-art-planar-cache PROPERTIES TIMEOUT 30 LABELS mui)
  add_test(NAME mui-search COMMAND mui_contract_test search)
  add_test(NAME mui-queue COMMAND mui_contract_test queue)
  add_test(NAME mui-playlists COMMAND mui_contract_test playlists)
  add_test(NAME mui-song-menu COMMAND mui_contract_test song-menu)
  add_test(NAME mui-like-rating COMMAND mui_contract_test like-rating)
  add_test(NAME mui-nowplaying COMMAND mui_contract_test nowplaying)
  add_test(NAME mui-songinfo COMMAND mui_contract_test songinfo)
  add_test(NAME mui-settings COMMAND mui_contract_test settings)
  add_test(NAME mui-dialogs COMMAND mui_contract_test dialogs)
  add_test(NAME mui-cmdline COMMAND mui_contract_test cmdline)
  add_test(NAME mui-states COMMAND mui_contract_test states)
  add_test(NAME mui-tracker COMMAND mui_contract_test tracker)
  add_test(NAME mui-toolrows COMMAND mui_contract_test toolrows)
  add_test(NAME mui-live-guard COMMAND mui_contract_test live-guard)
  add_test(NAME mui-cycle-initial COMMAND mui_contract_test cycle-initial)
  set_tests_properties(mui-wait-contract mui-registry mui-open-failure mui-open-nomui mui-menu mui-style-menu mui-menu-dispatch mui-weights mui-topbar mui-route mui-route-reach mui-tiles-reach mui-relayout mui-theme mui-theme-file mui-theme-live mui-theme-live-repaint mui-pens mui-icon-buttons mui-aabtn-tokens mui-aacheck mui-newplaylist mui-libfilter mui-transport-map mui-dirty-map mui-keys mui-list-regular mui-list-tree mui-list-drill mui-list-pending mui-list-dbl-row mui-columns mui-style-mask mui-list-backends mui-home mui-doors mui-sidebar mui-panes mui-library mui-toast mui-help mui-search mui-queue mui-playlists mui-song-menu mui-like-rating mui-nowplaying mui-songinfo mui-settings mui-dialogs mui-cmdline mui-states mui-tracker mui-toolrows mui-drop mui-screen-plan mui-window-borderless mui-player-bar mui-sidebar-icons mui-compact-rail mui-compact-player mui-player-bar-reach mui-player-band-sizes mui-icon-square mui-subsong-bar mui-viz-chooser mui-overlays mui-dialog-windows mui-tooltip-flat PROPERTIES TIMEOUT 30 LABELS mui)
  # I792: the MUI-vs-Mac parity guard runs in the pre-push gate: nice -n 19 ctest --test-dir build/host -L push-gate
  set_tests_properties(mui-weights mui-doors mui-panes PROPERTIES LABELS "mui;push-gate")
  set(_mui_has_ui 0)
  if(TARGET sel_plugin_mui_ui)
    set(_mui_has_ui 1)
  endif()
  add_test(NAME mui-build-split COMMAND "${CMAKE_COMMAND}" -DHAS_MAP=$<TARGET_EXISTS:mui_map> -DHAS_UI=${_mui_has_ui}
    -DAMIGA=0 -P "${CMAKE_CURRENT_SOURCE_DIR}/ui/mui_build_split.cmake")
  set_tests_properties(mui-build-split PROPERTIES LABELS mui)
  set(SEL_MUI_INCLUDE "$ENV{HOME}/opt/amiga/m68k-amigaos/ndk-include" CACHE PATH "MUI 3.8 developer headers (libraries/mui.h, proto/muimaster.h) for the m68k cross build")
  if(SEL_MUI_INCLUDE STREQUAL "")
    # A build dir configured before this default holds an empty cache entry; empty is never a valid header path.
    set_property(CACHE SEL_MUI_INCLUDE PROPERTY VALUE "$ENV{HOME}/opt/amiga/m68k-amigaos/ndk-include")
  endif()
  set(SEL_M68K_CC "$ENV{HOME}/opt/amiga/bin/m68k-amigaos-gcc" CACHE FILEPATH "m68k cross compiler for mui-sdk-probe")
  add_test(NAME mui-sdk-probe COMMAND "${CMAKE_COMMAND}" "-DCC=${SEL_M68K_CC}" "-DMUI_INCLUDE=${SEL_MUI_INCLUDE}"
    "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/mui-sdk-probe" -P "${CMAKE_CURRENT_SOURCE_DIR}/ui/mui_sdk_probe.cmake")
  set_tests_properties(mui-sdk-probe PROPERTIES LABELS "mui;m68k-gcc")
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

  # I684c: the shell loop paces on the UI wait unless the open output's pump blocks. Reachability through the real shell
  # (paula-shadow: 20 ms every turn, ahi-shadow: 0) and the rule itself, handover included.
  foreach(pace paula-shadow:20 ahi-shadow:0)
    string(REPLACE ":" ";" pace_parts "${pace}")
    list(GET pace_parts 0 pace_out)
    list(GET pace_parts 1 pace_ms)
    add_test(NAME selecta-shell-pace-${pace_out}
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DMOD=${SEL_MKMOD}"
        "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m1/shell_pace.keys" "-DOUTPUT=${pace_out}"
        "-DWANT_MIN=${pace_ms}" "-DWANT_MAX=${pace_ms}" -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/shell_pace.cmake")
  endforeach()
  add_executable(shell_pace_unit m1/shell_pace_unit.c)
  target_include_directories(shell_pace_unit PRIVATE ${PROJECT_SOURCE_DIR}/apps/common)
  add_test(NAME shell-pace-unit COMMAND shell_pace_unit)

  # I563: an unregistered UI is a fatal start-up exit; it must go through the shared report (sel_startup_error),
  # whose count --stats prints, and carry the message. The Amiga requester half is the same call on the Amiga.
  add_test(NAME selecta-startup-error-report
    COMMAND selecta --stats --ui no-such-ui --output null)
  set_tests_properties(selecta-startup-error-report PROPERTIES
    PASS_REGULAR_EXPRESSION "selecta: ui 'no-such-ui' is not registered.*startup_error_reports=1")

  # I625a: the start-up phase trace. --log writes one "trace t_ms=" line per phase (ms since main entry) plus the system
  # time at main entry; without --log none appear. Fail-before: the lines did not exist.
  add_test(NAME selecta-start-phase-trace
    COMMAND ${CMAKE_COMMAND} -DSELECTA=$<TARGET_FILE:selecta> -DWORK=${CMAKE_CURRENT_BINARY_DIR}/start_phase_trace
      -DSRC=${PROJECT_SOURCE_DIR}/apps/selecta/main.c
      -P ${CMAKE_CURRENT_SOURCE_DIR}/m1/start_phase_trace.cmake)

  # I916: a UI that refuses its flags hands the shell the reason (sel_ui_open_args.why); the start-up report names it
  # instead of "the UI could not open (no terminal?)". rc 1 is the WILL_FAIL half.
  if(SELECTA_UI_DESKTOP)
    add_test(NAME selecta-startup-error-ui-reason
      COMMAND ${CMAKE_COMMAND} -DSEL=$<TARGET_FILE:selecta> -P ${CMAKE_CURRENT_SOURCE_DIR}/startup_ui_reason.cmake)
    set_tests_properties(selecta-startup-error-ui-reason PROPERTIES PASS_REGULAR_EXPRESSION "startup_ui_reason_ok")
  endif()
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
    "-DNUADE_DIR=${SELECTA_NUADE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/m1/nuade_players.cmake")

# I710au: the nuade adapter reads companion files (smp.set, smp.*) from the song's own directory only. The unit test drives the
# NuadeIo (reads, and refuses "../x", absolute and backslash paths); the decode test plays real Pierre Adane / Synth Pack songs
# through the render entry point (fetched by checksum, exit 77 = skip).
add_executable(nuade_sibling_io_test m3/nuade_sibling_io_test.c)
target_link_libraries(nuade_sibling_io_test PRIVATE sel_nuade_shim)
target_include_directories(nuade_sibling_io_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${SELECTA_NUADE_DIR}/include")
add_test(NAME nuade-sibling-io COMMAND nuade_sibling_io_test)
set_tests_properties(nuade-sibling-io PROPERTIES LABELS m3 TIMEOUT 30 PASS_REGULAR_EXPRESSION "nuade_sibling_io_ok"
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
if(NOT SEL_AMIGA AND SELECTA_PLUGIN_LINK STREQUAL "MODULE")
  # I1133: 400 modules through the user path, none rejected with "module table full".
  add_test(NAME plugin-table-growth COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/plugin_table_growth.sh $<TARGET_FILE:selecta-cli> ${CMAKE_BINARY_DIR}/modules)
  set_tests_properties(plugin-table-growth PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "plugin_table_growth_ok" SKIP_RETURN_CODE 77)
endif()
if(NOT SEL_AMIGA)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/nuade-companion-decode)
  add_test(NAME nuade-companion-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/nuade_companion_decode.sh $<TARGET_FILE:selecta-cli>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/nuade-companion-decode)
  set_tests_properties(nuade-companion-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "nuade_companion_decode_ok" SKIP_RETURN_CODE 77
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
endif()

# T29: playlist codecs (Hippo .prg, M3U, PLS, XSPF), src/library/playlist.
add_executable(playlist_test playlist/playlist_test.c)
target_link_libraries(playlist_test PRIVATE selecta_library selecta_core)
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
  # I218i: the folder browser hides files with no recognised format.
  # I263: a Home song key longer than 127 bytes crosses library, view parser and row whole.
  add_executable(home_key_test engine/home_key_test.c)
  target_include_directories(home_key_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db ${PROJECT_SOURCE_DIR}/src/engine)
  target_link_libraries(home_key_test PRIVATE selecta_core)
  add_test(NAME home-key-long-path COMMAND home_key_test)
  add_executable(browse_filter_test library/browse_filter_test.c)
  target_include_directories(browse_filter_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(browse_filter_test PRIVATE selecta_core)
  add_test(NAME browse-hides-unrecognised-files COMMAND browse_filter_test)
  add_executable(replayer_priority_test engine/replayer_priority_test.c)
  target_link_libraries(replayer_priority_test PRIVATE selecta_core)
  add_test(NAME engine-replayer-priority-wins-probe COMMAND replayer_priority_test)
  set_tests_properties(engine-replayer-priority-wins-probe PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "replayer_priority: ok")
  # I1078: every bundled replayer probe answers within a bound on a 50 MB buffer (a podcast MP3 froze the UI in the zxtune probe).
  add_executable(replayer_probe_bound_test engine/replayer_probe_bound_test.c)
  target_link_libraries(replayer_probe_bound_test PRIVATE selecta_core ${CMAKE_DL_LIBS})
  add_test(NAME engine-replayer-probe-bounded COMMAND replayer_probe_bound_test ${CMAKE_BINARY_DIR}/modules ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/zxtune_test.pt3)
  set_tests_properties(engine-replayer-probe-bounded PROPERTIES TIMEOUT 300 PASS_REGULAR_EXPRESSION "replayer_probe_bound: ok")
  add_executable(lyric_neighbours_test engine/lyric_neighbours_test.c)
  target_link_libraries(lyric_neighbours_test PRIVATE selecta_core)
  add_test(NAME engine-lyric-prev-next COMMAND lyric_neighbours_test)
  set_tests_properties(engine-lyric-prev-next PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "lyric_neighbours: ok")
  # I605: the song length database. The tool and lookup on a TSV fixture; the engine takes a missing player duration from it.
  add_executable(songlen_unit_test songlen_unit_test.c)
  target_link_libraries(songlen_unit_test PRIVATE selecta_library selecta_songlen_bake)
  add_test(NAME songlen-bake-lookup COMMAND songlen_unit_test $<TARGET_FILE:songlen_bake>)
  set_tests_properties(songlen-bake-lookup PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "songlen_unit_ok")
  add_executable(songlen_engine_test engine/songlen_engine_test.c)
  target_link_libraries(songlen_engine_test PRIVATE selecta_core selecta_library selecta_songlen_bake)
  add_test(NAME engine-song-length-from-database COMMAND songlen_engine_test)
  set_tests_properties(engine-song-length-from-database PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "songlen_engine_ok")

  # I434: a render-kind replayer's length comes from the catalogue lengths_ms sticker (song_length_ms).
  add_executable(catalogue_length_render_test engine/catalogue_length_render_test.c)
  target_include_directories(catalogue_length_render_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(catalogue_length_render_test PRIVATE selecta_core selecta_library)
  add_test(NAME engine-catalogue-length-render COMMAND catalogue_length_render_test)
  set_tests_properties(engine-catalogue-length-render PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "catalogue_length_render_ok")
  add_executable(hw_prepare_test engine/hw_prepare_test.c)
  target_link_libraries(hw_prepare_test PRIVATE selecta_core)
  # I732: the native kplayer adapter's tracker hooks, through the engine's own lookup (host stub for the 68k call).
  add_executable(kplayer_tracker_test engine/kplayer_tracker_test.c ${PROJECT_SOURCE_DIR}/plugins/replayers/kplayer/modwalk.c)
  target_include_directories(kplayer_tracker_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/replayers/kplayer)
  target_link_libraries(kplayer_tracker_test PRIVATE selecta_core)
  add_test(NAME engine-kplayer-tracker-cells COMMAND kplayer_tracker_test "${SEL_MKMOD}")
  set_tests_properties(engine-kplayer-tracker-cells PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "kplayer_tracker: ok")
  # I434b: the native playsid adapter through the engine (stub of the LVO layer logs every library call).
  add_executable(playsid_test engine/playsid_test.c ${PROJECT_SOURCE_DIR}/plugins/replayers/playsid/playsid_map.c)
  target_include_directories(playsid_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/replayers/playsid)
  target_include_directories(playsid_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(playsid_test PRIVATE selecta_library selecta_core selecta_formats)
  target_compile_definitions(playsid_test PRIVATE PLAYSID_FIXTURE_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/playsid")
  add_test(NAME engine-playsid-native-adapter COMMAND playsid_test "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(engine-playsid-native-adapter PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "playsid: ok")
  add_test(NAME engine-hw-song-not-cut-by-early-prepare COMMAND hw_prepare_test)
  set_tests_properties(engine-hw-song-not-cut-by-early-prepare PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "hw_prepare: ok")
  add_test(NAME engine-hw-song-ends-on-loop COMMAND hw_prepare_test loop)
  set_tests_properties(engine-hw-song-ends-on-loop PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "hw_prepare: ok")
  add_test(NAME engine-hw-song-seeks-natively COMMAND hw_prepare_test seek)
  set_tests_properties(engine-hw-song-seeks-natively PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "hw_prepare: ok")
  add_test(NAME engine-hw-song-not-cut-by-silence COMMAND hw_prepare_test silence)
  set_tests_properties(engine-hw-song-not-cut-by-silence PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "hw_prepare: ok")
  # I532: a mixer-routed song on a voice-only output (Paula) swaps to a PCM sink (ahi, else paula14).
  add_executable(pcm_route_test engine/pcm_route_test.c)
  target_link_libraries(pcm_route_test PRIVATE selecta_core)
  add_test(NAME engine-pcm-song-leaves-voice-only-output COMMAND pcm_route_test ahi)
  add_test(NAME engine-pcm-song-falls-back-to-paula14 COMMAND pcm_route_test paula)
  add_test(NAME engine-voice-song-stays-on-paula COMMAND pcm_route_test voice)
  set_tests_properties(engine-pcm-song-leaves-voice-only-output engine-pcm-song-falls-back-to-paula14
                       engine-voice-song-stays-on-paula PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "pcm_route: ok")
  set_tests_properties(browse-hides-unrecognised-files PROPERTIES PASS_REGULAR_EXPRESSION "browse_filter_ok")
  # M4-AX-0-1 / M4-AX-0-2: the shared reply layer (apps/common/ctlexec.c) returns rc and RESULT lines.
  add_executable(ctl_exec_test engine/ctl_exec_test.c ../apps/common/ctlexec.c)
  target_link_libraries(ctl_exec_test PRIVATE selecta_uimodel)
  target_include_directories(ctl_exec_test PRIVATE ${PROJECT_SOURCE_DIR}/apps/common)
  target_link_libraries(ctl_exec_test PRIVATE selecta_core)
  # I632a: the shared multi-selection model.
  # I710ga6: the Now Playing note line and button for a song that waits for a system file.
  add_executable(sysfile_note_test uimodel/sysfile_note_test.c)
  target_link_libraries(sysfile_note_test PRIVATE selecta_uimodel)
  add_test(NAME sysfile-note-unit COMMAND sysfile_note_test)
  set_tests_properties(sysfile-note-unit PROPERTIES PASS_REGULAR_EXPRESSION "sysfile_note_ok")
  add_executable(uim_select_test uimodel/select_test.c)
  target_link_libraries(uim_select_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-select COMMAND uim_select_test)
  set_tests_properties(uim-select PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_select_ok")
  # I678a: the Your Library row subtitle formatter.
  add_executable(uim_library_subtitle_test uimodel/library_subtitle_test.c)
  target_link_libraries(uim_library_subtitle_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-library-subtitle COMMAND uim_library_subtitle_test)
  set_tests_properties(uim-library-subtitle PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_library_subtitle_ok")
  # I1079a: the shared column splitter for row text widths.
  add_executable(uim_cols_test uimodel/cols_test.c)
  target_link_libraries(uim_cols_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-cols COMMAND uim_cols_test)
  set_tests_properties(uim-cols PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_cols_ok")
  # I1019d: the shared SOURCE_SEARCH * line and the search heading kinds.
  add_executable(uim_search_group_test uimodel/search_group_test.c)
  target_link_libraries(uim_search_group_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-search-group COMMAND uim_search_group_test)
  set_tests_properties(uim-search-group PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_search_group_ok")
  # I831: the left button of the label control (Previous Filter or Clear Filter).
  add_executable(uim_libfilter_left_test uimodel/libfilter_left_test.c)
  target_link_libraries(uim_libfilter_left_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-libfilter-left COMMAND uim_libfilter_left_test)
  set_tests_properties(uim-libfilter-left PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_libfilter_left_ok")
  # I611: Your Library sort comparators and the text filter.
  add_executable(uim_lsort_test uimodel/lsort_test.c)
  target_link_libraries(uim_lsort_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-lsort COMMAND uim_lsort_test)
  add_executable(uim_pref_rows_test uimodel/pref_rows_test.c)
  target_link_libraries(uim_pref_rows_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-pref-rows COMMAND uim_pref_rows_test)
  # I706d: the About view model holds the module credits (the SunVox credit text from plugins/replayers/sunvox_lib/CREDITS.txt).
  add_executable(uim_about_test uimodel/about_test.c)
  target_link_libraries(uim_about_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-about COMMAND uim_about_test "${PROJECT_SOURCE_DIR}/plugins/replayers")
  set_tests_properties(uim-about PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_about_ok credits=[1-9]")
  add_executable(uim_rom_test uimodel/rom_test.c)
  target_link_libraries(uim_rom_test PRIVATE selecta_uimodel selecta_core)
  add_test(NAME uim-rom COMMAND uim_rom_test "${PROJECT_BINARY_DIR}/tests/uim-rom")
  set_tests_properties(uim-rom PROPERTIES PASS_REGULAR_EXPRESSION "rom: ok")
  set_tests_properties(uim-lsort PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "uim_lsort_ok")
  add_test(NAME ctl-exec-result COMMAND ctl_exec_test status)
  add_test(NAME commands-reflection-format COMMAND ctl_exec_test commands)
  add_test(NAME result-event-commands COMMAND ctl_exec_test raw)
  add_test(NAME ctl-exec-limits COMMAND ctl_exec_test limits)
  add_test(NAME ctl-exec-last-error COMMAND ctl_exec_test lasterror)
  add_test(NAME ctl-exec-quit COMMAND ctl_exec_test quit)
  add_test(NAME ctl-exec-quit-action COMMAND ctl_exec_test quitaction)
  # M4-AX-3-1 / M4-AX-3-4: the ARexx message mapping through the shared dispatcher (the Amiga half is cross-built).
  add_executable(arexx_test engine/arexx_test.c ../apps/common/ctlexec.c ../plugins/scripting/arexx/arexx_core.c)
  target_include_directories(arexx_test PRIVATE ${PROJECT_SOURCE_DIR}/apps/common ${PROJECT_SOURCE_DIR}/plugins/scripting/arexx)
  target_link_libraries(arexx_test PRIVATE selecta_core)
  add_test(NAME arexx-dispatch-reach COMMAND arexx_test reach)
  add_test(NAME arexx-dispatch-rc COMMAND arexx_test rc)
  add_test(NAME arexx-port-names COMMAND arexx_test names)
  add_test(NAME arexx-batch COMMAND arexx_test batch)
  add_test(NAME arexx-hook COMMAND arexx_test hook)
  add_test(NAME arexx-scripts COMMAND arexx_test scripts ${PROJECT_SOURCE_DIR}/dist/amiga/rexx)
  add_test(NAME arexx-message-line COMMAND arexx_test msgline)
  add_test(NAME arexx-queue-argnames COMMAND arexx_test argnames)
  set_tests_properties(arexx-dispatch-reach arexx-dispatch-rc arexx-port-names arexx-batch arexx-hook arexx-scripts arexx-message-line arexx-queue-argnames PROPERTIES TIMEOUT 60 LABELS engine
                       PASS_REGULAR_EXPRESSION "arexx_test [a-z]+ ok")
  # M4-AX-2-1: the commands.def build check accepts the real table and rejects a row without a label and one whose
  # argnames do not match the signature.
  add_test(NAME commands-def-check COMMAND ${CMAKE_COMMAND} -DSEL_CMD_DEF=${PROJECT_SOURCE_DIR}/include/selecta/commands.def
           -P ${PROJECT_SOURCE_DIR}/cmake/selecta_cmd_check.cmake)
  add_test(NAME commands-def-check-no-label COMMAND ${CMAKE_COMMAND}
           -DSEL_CMD_DEF=${CMAKE_CURRENT_SOURCE_DIR}/cmake_fixtures/commands_no_label.def
           -P ${PROJECT_SOURCE_DIR}/cmake/selecta_cmd_check.cmake)
  add_test(NAME commands-def-check-bad-argnames COMMAND ${CMAKE_COMMAND}
           -DSEL_CMD_DEF=${CMAKE_CURRENT_SOURCE_DIR}/cmake_fixtures/commands_bad_argnames.def
           -P ${PROJECT_SOURCE_DIR}/cmake/selecta_cmd_check.cmake)
  set_tests_properties(commands-def-check PROPERTIES PASS_REGULAR_EXPRESSION "commands.def check passed")
  set_tests_properties(commands-def-check-no-label PROPERTIES PASS_REGULAR_EXPRESSION "without argnames and label")
  set_tests_properties(commands-def-check-bad-argnames PROPERTIES PASS_REGULAR_EXPRESSION "2 argnames for signature")
  set_tests_properties(ctl-exec-result commands-reflection-format result-event-commands ctl-exec-limits ctl-exec-last-error ctl-exec-quit ctl-exec-quit-action PROPERTIES TIMEOUT 60 LABELS engine
                       PASS_REGULAR_EXPRESSION "ctl_exec: ok")
endif()

# I182a: composer canonical key and display name golden cases.
add_executable(composer_test library/composer_test.c "${PROJECT_SOURCE_DIR}/src/library/composer.c")
target_include_directories(composer_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/include)
add_test(NAME composer-unit COMMAND composer_test)

# I406: the Modules.pl search page parser over the recorded response.
add_executable(modulespl_parse_test library/modulespl_parse_test.c
  "${PROJECT_SOURCE_DIR}/plugins/sources/modulespl/modulespl_parse.c")
target_include_directories(modulespl_parse_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/sources/modulespl)
target_compile_definitions(modulespl_parse_test PRIVATE
  SEL_MODULESPL_FIX="${PROJECT_SOURCE_DIR}/tests/net/fixtures/www.modules.pl/search_chip.html")
add_test(NAME modulespl-parse COMMAND modulespl_parse_test)
set_tests_properties(modulespl-parse PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "modulespl_parse_ok")

# I710ga2: system-file store (resolve by sha256, install raw or from a zip, .part then rename).
add_executable(sysfile_store_test library/sysfile_store_test.c)
target_link_libraries(sysfile_store_test PRIVATE selecta_shared_cast selecta_library)
target_include_directories(sysfile_store_test PRIVATE "${PROJECT_SOURCE_DIR}/src/library" "${PROJECT_SOURCE_DIR}/third_party/miniz")
add_test(NAME sysfile-store COMMAND sysfile_store_test "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(sysfile-store PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sysfile_store_ok")

# I437: embedded cover readers (ID3v2 APIC, FLAC PICTURE).
add_executable(cover_tag_test library/cover_tag_test.c)
add_test(NAME cover-tag-unit COMMAND cover_tag_test)
set_tests_properties(cover-tag-unit PROPERTIES PASS_REGULAR_EXPRESSION "cover_tag_ok")

# I892c1: RAOP (AirPlay 1) sender, pure part. Needs mbedtls, which the build tree has off the Amiga only.
if(TARGET selecta_shared_raop)
  add_executable(raop_pure_test library/raop_pure_test.c)
  target_link_libraries(raop_pure_test PRIVATE selecta_shared_raop)
  add_test(NAME raop-pure COMMAND raop_pure_test)
  set_tests_properties(raop-pure PROPERTIES PASS_REGULAR_EXPRESSION "raop_pure_ok")
endif()

# I892c2: raop_server.py (fixture receiver) starts raop_stream_test as the sender and compares the decrypted PCM.
find_program(SEL_PYTHON python3)
if(TARGET selecta_shared_raop_net AND SEL_PYTHON)
  add_executable(raop_stream_test library/raop_stream_test.c)
  target_link_libraries(raop_stream_test PRIVATE selecta_shared_raop_net)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/raop_stream")
  add_test(NAME raop-stream COMMAND "${SEL_PYTHON}" -I "${CMAKE_CURRENT_SOURCE_DIR}/net/raop_server.py"
    $<TARGET_FILE:raop_stream_test> "${CMAKE_CURRENT_BINARY_DIR}/raop_stream")
  set_tests_properties(raop-stream PROPERTIES TIMEOUT 90 PASS_REGULAR_EXPRESSION "raop_stream_ok")
  # I892c3: the fixture drops packet 7, asks for a resend on the control port; sync and timing are checked in both runs.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/raop_resend")
  add_test(NAME raop-resend COMMAND "${SEL_PYTHON}" -I "${CMAKE_CURRENT_SOURCE_DIR}/net/raop_server.py"
    $<TARGET_FILE:raop_stream_test> "${CMAKE_CURRENT_BINARY_DIR}/raop_resend" drop=7)
  set_tests_properties(raop-resend PROPERTIES TIMEOUT 90 PASS_REGULAR_EXPRESSION "raop_stream_ok")
endif()

# I218d: Latin-1 <-> UTF-8 at the OS boundary.
add_executable(latin1_test library/latin1_test.c)
target_link_libraries(latin1_test PRIVATE selecta_shared_source)
add_test(NAME latin1-unit COMMAND latin1_test)
add_executable(meta_utf8_test library/meta_utf8_test.c
               "${PROJECT_SOURCE_DIR}/src/library/meta_utf8.c" "${PROJECT_SOURCE_DIR}/src/core/tag.c")
target_link_libraries(meta_utf8_test PRIVATE selecta_shared_source)
target_include_directories(meta_utf8_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/include)
add_test(NAME meta-utf8-latin1-composer-name-unit COMMAND meta_utf8_test)

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

  # I1013: live UI control over the socket (ui.state, ui.input, ui.frame), SDL dummy video, desktop UI only.
  if(SELECTA_UI_DESKTOP)
    add_test(NAME ui-control-socket
      COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m12/ui_control.sh" "$<TARGET_FILE:selecta>" "${SEL_CLI}"
        "${CMAKE_CURRENT_SOURCE_DIR}/dui/fixture_library.sql" "${CMAKE_CURRENT_BINARY_DIR}/ui_control_socket")
    set_tests_properties(ui-control-socket PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "ui-control-socket ok")
    add_test(NAME ui-control-mcp
      COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m12/ui_mcp_smoke.sh" "$<TARGET_FILE:selecta>" "${SEL_CLI}"
        "${CMAKE_CURRENT_SOURCE_DIR}/dui/fixture_library.sql" "${CMAKE_CURRENT_BINARY_DIR}/ui_control_mcp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../scripts/selecta-ui-mcp.py")
    set_tests_properties(ui-control-mcp PROPERTIES TIMEOUT 120 PASS_REGULAR_EXPRESSION "ui-control-mcp ok")
  endif()

  # I575: scripted runs (--console-frames, --output null) ignore a Selecta that holds the default control socket.
  add_test(NAME scripted-run-no-socket
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m12/scripted_no_socket.sh" "$<TARGET_FILE:selecta>"
      "${CMAKE_CURRENT_BINARY_DIR}/scripted_no_socket")
  set_tests_properties(scripted-run-no-socket PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "scripted-no-socket ok")

  # M4-AX-0-4 ctl-socket-result (and M4-AX-2-4 QUIT): the one reachability test of the result reply and the new verbs.
  add_test(NAME ctl-socket-result
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/m12/ctl_socket_result.sh" "$<TARGET_FILE:selecta>"
      "${SEL_CLI}" "${CMAKE_CURRENT_SOURCE_DIR}/m12/wait.keys" "${CMAKE_CURRENT_BINARY_DIR}/ctl_socket_result" "${SEL_COUNT_CMD}" "${SEL_TESTMOD}")
  set_tests_properties(ctl-socket-result PROPERTIES TIMEOUT 60 LABELS engine PASS_REGULAR_EXPRESSION "ctl-socket-result ok")

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
  add_test(NAME viz-sid-glow COMMAND phase_test sidglow)
  add_test(NAME viz-spectrum COMMAND phase_test spectrum)
  add_test(NAME viz-delay COMMAND phase_test delay) # I1042: the delayed frame carries its analysis
  # I434j2: SID PatternScope surface test (the plugin source is included, its tables are static).
  add_executable(sidregs_test viz/sidregs_test.c)
  target_include_directories(sidregs_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/shared")
  add_test(NAME viz-sidregs COMMAND sidregs_test all)
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
  # I427f: the plugin is a module on desktop; the unit half compiles its source in directly.
  add_executable(crossfeed_test dsp/crossfeed_test.c "${PROJECT_SOURCE_DIR}/plugins/dsp/crossfeed/crossfeed.c")
  target_include_directories(crossfeed_test PRIVATE "${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/plugins/shared")
  target_compile_definitions(crossfeed_test PRIVATE SEL_PLUGIN_ENTRY=sel_plugin_entry_crossfeed SEL_PLUGIN_KIND_BUILD=3)
  target_link_libraries(crossfeed_test PRIVATE selecta_core)
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
  # I222: the real probe plus the registry pick the machine's output (vamos: paula).
  sel_m2_test(default-output run desktop -DTAG=I222 "-DARGS1=machine" "-DCHECKS1=default_output~^(coreaudio|null)$")
  sel_m2_test(default-output run vamos -DTAG=I222 "-DARGS1=machine" "-DCHECKS1=default_output==paula")
  # T50 machine probe under vamos. I217: the AHI probe request is a struct AHIRequest (80 bytes,
  # ahir_Version 4); a plain IOStdReq made ahi.device raise alert #00080000 on the Replay.
  sel_m2_test(amiga-machine-probe run vamos -DTAG=T50 "-DARGS1=machine"
    "-DCHECKS1=GFX~^(OCS|ECS|AGA)(\\+RTG)?$,AUDIO~paula,ahi.request==80/4" "-DARGS2=machine --tier planar" "-DCHECKS2=TIER==planar")
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
  # I124/I125 (T58/T59): OUTPUT=AHI routing, no --rate: the output's SEL_OUT_RATE_FOR picks the rate (D-M2-5). The shadow twin
  # counts buffer swaps: desktop 2 s at 44100 Hz / 1024 frames = 86; 68020 2 s at 28150 Hz / 4096 frames = 13.7.
  sel_m2_test(ahi-routing run desktop -DTAG=T59
    "-DARGS1=render --output ahi-shadow @MOD@ --seconds 2"
    "-DCHECKS1=ahi_bufs>=78,ahi_bufs<=95,underruns==0,ahi_buffer_frames==1024,ahi_rate==44100")
  sel_m2_test(ahi-routing run vamos -DTAG=T59
    "-DARGS1=render --output ahi-shadow @MOD@ --seconds 2"
    "-DCHECKS1=ahi_bufs>=12,ahi_bufs<=15,underruns==0,ahi_buffer_frames==4096,ahi_rate==28150")
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
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-text-ui-vamos"
        -P "${SEL_M2}/ui.cmake")
    set_tests_properties(amiga-text-ui-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
    # I218e the first frame on the Amiga console: same test script, registered under the symptom's name.
    add_test(NAME amiga-console-first-frame-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DDESKTOP_SELECTA=$<TARGET_FILE:selecta>" "-DKEYS=${SEL_M2}/quit.keys"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-console-first-frame-vamos"
        -P "${SEL_M2}/ui.cmake")
    set_tests_properties(amiga-console-first-frame-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
    # I138b AmigaDOS browse join and parent through the Amiga text UI.
    add_test(NAME amiga-browse-nav-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-browse-nav-vamos"
        -P "${SEL_M2}/browse_nav.cmake")
    set_tests_properties(amiga-browse-nav-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
    # I218 Amiga console arrows (8-bit CSI 0x9B) move the browse cursor through the real stdin path.
    add_test(NAME amiga-console-keys-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-console-keys-vamos"
        -P "${SEL_M2}/amiga_keys.cmake")
    set_tests_properties(amiga-console-keys-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
    # I219c KPlayer HW start refused cleanly under vamos (no ciab.resource): handover, no start, no crash.
    add_test(NAME amiga-kplayer-start-refused-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-kplayer-start-refused-vamos"
        -P "${SEL_M2}/kplayer_vamos_start.cmake")
    set_tests_properties(amiga-kplayer-start-refused-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos TIMEOUT 120)
    # I218d2 the Latin-1 name row on the Amiga console: the console is sent 0xF6 / 0xC4, not UTF-8 or U+FFFD.
    add_test(NAME amiga-console-latin1-name-box-glyph-vamos
      COMMAND "${CMAKE_COMMAND}" "-DVAMOS=${SEL_VAMOS}"
        "-DAMIGA_SELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/amiga-console-latin1-name-box-glyph-vamos"
        -P "${SEL_M2}/latin1_row_bytes.cmake")
    set_tests_properties(amiga-console-latin1-name-box-glyph-vamos PROPERTIES FIXTURES_REQUIRED amiga_cli RESOURCE_LOCK vamos)
  endif()
  # T57 library browse under vamos, T58 ToolTypes = CLI.
  sel_m2_test(amiga-library-browse browse vamos)
  sel_m2_test(amiga-tooltypes-cli tooltypes vamos)
  # T96 library search and stats over a prebuilt read-only index plus a user DB, 68020/2 MB tier, I197.
  find_program(SEL_AMIGA_SIZE m68k-amigaos-size HINTS "$ENV{HOME}/opt/amiga/bin" /opt/amiga/bin)
  sel_m2_test(amiga-library library_vamos vamos "-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}/m3"
    "-DAMIGA_SIZE=${SEL_AMIGA_SIZE}" "-DAMIGA_BUILD_DIR=${PROJECT_SOURCE_DIR}/build/amiga")
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
  # One place for the properties of every rig row: label rig, one rig at a time, and Skipped (never Passed, never Failed)
  # while run_rig.cmake reports no rig (UPTERM_RIG unset, or the rig down).
  function(sel_rig_test_props name)
    set_tests_properties(${name} PROPERTIES LABELS rig RESOURCE_LOCK rig
      SKIP_REGULAR_EXPRESSION "SKIPPED rig not running")
  endfunction()
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
    sel_rig_test_props(${name})
  endforeach()
  # I450 / M4-P5-1: RTG first light with a real library, on rig 2 (uaegfx), through Selecta --ui desktop --backend rtg.
  add_test(NAME rig-m4-rtg-first-light
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4RTG "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta/selecta" "-DMOD=${SEL_TESTMOD}"
      "-DSELECTA_CLI=${PROJECT_SOURCE_DIR}/build/amiga/apps/selecta-cli/selecta-cli"
      "-DGENRE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/m3/genre"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-rtg-first-light"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-rtg-first-light)
  # I508 / MUI-P1-3, MUI-P1-5: the MUI window opens on rig 2 through Selecta --ui mui (every Amiga build tree carries the
  # MUI plugin; SEL_MUI_BUILD names the one to run, default build/amiga).
  set(SEL_MUI_BUILD "${PROJECT_SOURCE_DIR}/build/amiga" CACHE PATH "Amiga build tree whose Selecta rig-m4-mui-open runs")
  # MUI-P3-1 / MUI-P2-6: a real module the rig test plays through a list-row double-click (never committed; unset or
  # missing: those steps are skipped with a NOTE). Set -DSEL_MUI_SONG=<path> or the SEL_MUI_SONG environment variable.
  set(SEL_MUI_SONG "$ENV{SEL_MUI_SONG}" CACHE FILEPATH "Real .mod for the MUI play steps of rig-m4-mui-open")
  add_test(NAME rig-m4-mui-open
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUI "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta" "-DMOD=${SEL_TESTMOD}" "-DSONG=${SEL_MUI_SONG}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-open"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-mui-open)
  # I571 / MUI-P3-8: View > Style on the MUI window: the menu lists the planar styles, each pick reaches the engine
  # (view_style in the exit stats). Run as UPTERM_RIG=<n> ctest -R rig-m4-mui-styles.
  add_test(NAME rig-m4-mui-styles
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUISTYLES "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-styles"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-mui-styles)
  # MUI-P3-2 / P3-3 / P3-4 / I558: Tree and Drilldown styles and the columns of the MUI List on a real song (SEL_MUI_SONG).
  add_test(NAME rig-m4-mui-list
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUILIST "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta" "-DSONG=${SEL_MUI_SONG}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-list"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-mui-list)
  # I684 / MUI-P5-3 / MUI-P5-5: the Now Playing visualizer renders into the MUI window (vizframes > 0 in the ui_p4 exit stats).
  # Run as UPTERM_RIG=<n> ctest -R rig-m4-mui-viz; needs SEL_MUI_SONG.
  add_test(NAME rig-m4-mui-viz
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUIVIZ "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta" "-DSONG=${SEL_MUI_SONG}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-viz"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-mui-viz)
  # MUI-P5-6: an icon dragged from a Workbench drawer window onto the MUI window (AppWindow drop) and Edit > Copy Song Title
  # (clipboard.device). Needs a rig started with --no-dopus (plain Workbench) and SEL_MUI_SONG.
  add_test(NAME rig-m4-mui-drop
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUIDROP "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta" "-DMOD=${SEL_TESTMOD}" "-DSONG=${SEL_MUI_SONG}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-drop"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-mui-drop)
  # MUI-P1-6 / M4-P11-2: MUI absent (SYS:MUI/Libs renamed, restored on every path): the Selecta requester with Continue,
  # then the text UI runs and Selecta exits 0. Needs the same Amiga build tree as rig-m4-mui-open.
  add_test(NAME rig-m4-mui-nomui
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUINO "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta" "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m2/quit.keys"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-nomui"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-mui-nomui)
  # muimaster.library keeps one open count of its own once any MUI object has been created in a boot (a standalone
  # OpenLibrary/MUI_NewObject/MUI_DisposeObject/CloseLibrary program leaves lib_OpenCnt=1 too, so Selecta does not leak),
  # and the library can no longer be expunged. The missing-MUI test must therefore run before rig-m4-mui-open in a boot.
  set_tests_properties(rig-m4-mui-open PROPERTIES DEPENDS rig-m4-mui-nomui)
  # MUI-P0-2 / MUI-P0-3: recorded facts (plan section 9). rig-m4-mui-mem needs a fresh boot of the 68020 rig
  # (`rig.py start --a1200`), before any MUI run, so the library is not yet resident when Avail is read.
  foreach(t facts mem)
    string(TOUPPER "${t}" T_UP)
    add_test(NAME rig-m4-mui-${t}
      COMMAND "${CMAKE_COMMAND}" -DTAG=M4MUI${T_UP} "-DRIG_DIR=${SEL_RIG_DIR}"
        "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta" "-DMOD=${SEL_TESTMOD}"
        "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-mui-${t}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
    sel_rig_test_props(rig-m4-mui-${t})
  endforeach()
  # I439 / M4-P6-1: AGA c2p first light on rig 2, backend alone and the full AGA Selecta (SEL_DESKTOP_AGA build tree in
  # SEL_AGA_BUILD, default build/amiga-aga).
  set(SEL_AGA_BUILD "${PROJECT_SOURCE_DIR}/build/amiga-aga" CACHE PATH "Amiga build tree configured with -DSEL_DESKTOP_AGA=ON")
  add_test(NAME rig-m4-aga-first-light
    COMMAND "${CMAKE_COMMAND}" -DTAG=M4AGA "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_AGA_BUILD}/apps/selecta/selecta" "-DMOD=${SEL_TESTMOD}"
      "-DSELECTA_CLI=${SEL_AGA_BUILD}/apps/selecta-cli/selecta-cli"
      "-DAGA_FIRST_LIGHT=${SEL_AGA_BUILD}/plugins/ui/desktop/selecta-aga-first-light"
      "-DGENRE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/m3/genre"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-m4-aga-first-light"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-m4-aga-first-light)
  # I623: a Ctrl-C that a bsdsocket wait swallows must survive the net layer, so Selecta quits on Break.
  add_test(NAME rig-i623-net-break
    COMMAND "${CMAKE_COMMAND}" -DTAG=I623NET "-DRIG_DIR=${SEL_RIG_DIR}"
      "-DSELECTA=${SEL_MUI_BUILD}/apps/selecta/selecta"
      "-DNET_BREAK=${SEL_MUI_BUILD}/src/library/selecta-net-break"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m2/rig-i623-net-break"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/amiga/run_rig.cmake")
  sel_rig_test_props(rig-i623-net-break)
endif()
# I122/I123/I126: paula14 split tables, chip double buffer through a recording bus, routing of PCM outputs.
set(SEL_P14_DIR "${PROJECT_SOURCE_DIR}/plugins/outputs/paula14")
add_executable(p14_test paula/p14_test.c "${SEL_P14_DIR}/shadow.c" "${SEL_P14_DIR}/common/p14_split.c"
  "${SEL_P14_DIR}/common/p14_sink.c" "${SEL_P14_DIR}/common/p14_out.c")
set_source_files_properties("${SEL_P14_DIR}/shadow.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=shadow_entry)
target_include_directories(p14_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(p14_test PRIVATE selecta_core selecta_plugin_shared)
add_test(NAME paula14-unit COMMAND p14_test)

# I124/I125: AHI buffer/rate table per CPU class, output.buffer override, shadow double-buffer bookkeeping.
set(SEL_AHI_DIR "${PROJECT_SOURCE_DIR}/plugins/outputs/ahi")
add_executable(ahi_test paula/ahi_test.c "${SEL_AHI_DIR}/shadow.c" "${SEL_AHI_DIR}/common/ahi_cfg.c"
  "${SEL_AHI_DIR}/common/ahi_out.c")
set_source_files_properties("${SEL_AHI_DIR}/shadow.c" PROPERTIES COMPILE_DEFINITIONS SEL_PLUGIN_ENTRY=shadow_entry)
target_include_directories(ahi_test PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(ahi_test PRIVATE selecta_core selecta_plugin_shared)
add_test(NAME ahi-unit COMMAND ahi_test)

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
    # I736: the rig's row reader finds the untitled full-screen Selecta window through its screen title
    add_test(NAME mui-list-rows-selftest COMMAND "${SEL_PYTHON}" -I "${PROJECT_SOURCE_DIR}/tests/amiga/mui_list_rows.py" --selftest)
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
  # Includes the real 30 s total timeout, so the test gets a 120 s budget. net-guard runs its own
  # fixture server and log (I173e), so the two share no state and need no order.
  add_test(NAME http-client
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DCA_FILE=${SEL_NET_CA}/ca.pem" -P "${SEL_NET_DIR}/http_client.cmake")
  set_tests_properties(http-client PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 120)

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

  # I198b: a word of 4+ letters searches the 3-letter prefix index and sel_fts_prefix keeps whole-token prefixes.
  add_test(NAME library-search-long-prefix
    COMMAND sh m3/library_search.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/search_prefix.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/search_prefix.golden
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(library-search-long-prefix PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_searches=16")

  # I198e: a one-letter per-row scan stops at the queue capacity and reports truncated=1.
  add_test(NAME library-search-scan-cap
    COMMAND sh m3/library_scan_cap.sh $<TARGET_FILE:selecta-cli> ${CMAKE_CURRENT_BINARY_DIR}/scan_cap
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(library-search-scan-cap PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "scan_cap count=256 keys=256 truncated=1")

  # T96 desktop half: export-index plus read-only ATTACH next to a user DB (I197, D-M3-16).
  add_test(NAME library-index-attach
    COMMAND sh m3/index_attach.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/search.golden
            ${CMAKE_CURRENT_BINARY_DIR}/m3-index-attach
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(library-index-attach PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "index_checks=13")

  add_test(NAME play-query
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.expected
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(play-query PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "play_query_ok")

  # DUI-P4-3 / I220f: SOURCES lists every registered source with its songs count, through `selecta-cli script`.
  add_test(NAME sources
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/sources.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/sources.expected
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(sources PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "play_query_ok"
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${SEL_TESTSRC_PLUGIN_DIR}")

  # I265: library.sources_off switches a source off: SOURCES reports SRC_ENABLED=0 and the library search skips its songs.
  add_test(NAME sources-off
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/play_query.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/sources_off.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/sources_off.expected
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(sources-off PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "play_query_ok"
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${SEL_TESTSRC_PLUGIN_DIR}")

  add_test(NAME user-data
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/userdata.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue_keys.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/userdata.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/userdata.expected
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/userflags.sel ${CMAKE_CURRENT_SOURCE_DIR}/m3/userflags.expected
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/sounds-wrong.golden
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/usernote.sel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(user-data PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "userdata_ok")

  # I306: LIKE toggles, LIKE <key> and RATE <n> <key> address any song, RATE 0 <key> clears; read back through LIBRARY_SEARCH.
  add_test(NAME likes
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/likes.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue_keys.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/likes.sel
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/likes.expected
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(likes PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "likes_ok")

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

  # T94 decode part (I194, D-M3-8): MP3 and Ogg Vorbis play through render as RENDER-kind replayer plugins.
  # Own working directory: the script's output files must not land in the shared songs dir (I184c).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/stream-decode)
  add_test(NAME stream-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream_decode.sh $<TARGET_FILE:selecta-cli> ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/stream-decode)
  set_tests_properties(stream-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stream_decode_ok")

  # I194b: ID3v2/ID3v1/Vorbis comment tags reach the library through rp_metadata.
  # Own working directory: the script's output files must not land in the shared songs dir (I184c).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/stream-tags)
  add_test(NAME stream-tags
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream_tags.sh $<TARGET_FILE:selecta-cli> ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/stream-tags)
  set_tests_properties(stream-tags PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stream_tags_ok")

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

  add_test(NAME playlist-create
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/playlist_create.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_SOURCE_DIR}/m3/playlist_create.sel
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(playlist-create PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "plc_ok")

  add_test(NAME library-remove
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/library_remove.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv ${CMAKE_CURRENT_BINARY_DIR}/songs/selecta-test.mod
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/songs)
  set_tests_properties(library-remove PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "lrm_ok")

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

  set(SEL_SQLITE3_META "$<TARGET_FILE:sel_sqlite3_shell>")
  add_test(NAME metadata-panel
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue_keys.tsv"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_empty.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_screen.screen"
      "-DGOLDEN_LOCAL=${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_local.screen"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/meta_screen"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/meta_screen.cmake")
  set_tests_properties(metadata-panel PROPERTIES LABELS m3)

  add_test(NAME song-key
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/song_key.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/song_key"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/song_key.cmake")
  set_tests_properties(song-key PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "song_key_ok")

  # I535: per-source song counts kept by schema v8 triggers, read by SOURCES without a scan of songs.
  add_test(NAME source-songs
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/sources.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/source_songs"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/source_songs.cmake")
  set_tests_properties(source-songs PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_songs_ok"
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${SEL_TESTSRC_PLUGIN_DIR}")

  add_executable(key_remote_test m3/key_remote_test.c)
  target_link_libraries(key_remote_test PRIVATE selecta_core)
  add_test(NAME key-remote COMMAND key_remote_test)
  set_tests_properties(key-remote PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "key_remote_ok")

  add_executable(modland_parse_test m3/modland_parse_test.c)
  target_link_libraries(modland_parse_test PRIVATE selecta_core)
  add_test(NAME modland-parse COMMAND modland_parse_test)
  set_tests_properties(modland-parse PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "modland_parse_ok")

  add_test(NAME home-view
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue_keys.tsv"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/history.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/home.keys"
      "-DKEYS_SONG=${CMAKE_CURRENT_SOURCE_DIR}/m3/home_song.keys"
      "-DKEYS_ALBUM=${CMAKE_CURRENT_SOURCE_DIR}/m3/home_album.keys"
      "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/m3/home_view.screen"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/home_view"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/home_view.cmake")
  set_tests_properties(home-view PROPERTIES LABELS m3)
  # I555: history rows of songs that left the library are not listed on Home (empty library shows the hint).
  add_test(NAME home-stale-history
    COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue_keys.tsv"
      "-DSEL=${CMAKE_CURRENT_SOURCE_DIR}/m3/history.sel"
      "-DSQLITE3=${SEL_SQLITE3_META}"
      "-DKEYS=${CMAKE_CURRENT_SOURCE_DIR}/m3/home.keys"
      "-DCWD=${CMAKE_CURRENT_BINARY_DIR}/songs"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/home_stale_history"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/home_stale_history.cmake")
  set_tests_properties(home-stale-history PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "home_stale_history_ok")
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

  # I807: the Songs group of the search view lists every source with hits (a later source in key order was cut by the 16 row cap).
  add_test(NAME search-songs-every-source
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/search_sources.tsv"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/search_sources"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/search_sources.cmake")
  set_tests_properties(search-songs-every-source PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "search_sources ok")
endif()

# M3 Phase 4 (plan 2026-10-25-m3-library.md): T71 source-local (scan part), T72 source-modland-key (I157, I158).
if(NOT SEL_AMIGA)
  add_test(NAME source-local
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_local.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/local.manifest ${SEL_TESTMOD} ${SEL_TESTMOD})
  set_tests_properties(source-local PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_local_ok")

  # I688: partial downloads (.part, .crdownload, .download, .partial) are not scanned.
  add_test(NAME source-local-partial
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/source_local_partial.sh $<TARGET_FILE:selecta-cli> ${SEL_TESTMOD})
  set_tests_properties(source-local-partial PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_local_partial_ok")

  # I276: library scan without --db fills the app's library.db in the config directory.
  add_test(NAME library-scan-default-db
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/scan_default_db.sh $<TARGET_FILE:selecta-cli> ${SEL_TESTMOD})
  set_tests_properties(library-scan-default-db PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "scan_default_db_ok")

  # I577: the scan fits the 512 KB SQLite heap of the 2 MB tier, into :memory: and into a file.
  add_test(NAME library-scan-small-heap
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/scan_small_heap.sh $<TARGET_FILE:selecta-cli> ${SEL_TESTMOD})
  set_tests_properties(library-scan-small-heap PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "scan_small_heap_ok")

  # I184d: a local MP3 / Ogg is scanned and played through a file-backed byte stream, not read whole.
  add_test(NAME file-stream-local
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/file_stream.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/stream)
  set_tests_properties(file-stream-local PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "file_stream_ok")

  # I436: FLAC plays through the render entry point (dr_flac replayer).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/flac-decode)
  add_test(NAME flac-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/flac_decode.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/library/fixtures/cover_tagged.flac
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/flac-decode)
  set_tests_properties(flac-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "flac_decode_ok")

  # I426: a NES NSF plays through the render entry point (gme replayer).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/gme-decode)
  add_test(NAME gme-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/gme_decode.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/gme_test.nsf
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/gme-decode)
  set_tests_properties(gme-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "gme_decode_ok")

  # I862: a .vgz / .vgm file is never open_failed (the gme probe claimed the gzip and Vgm magics without a VGM core).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/vgz-no-open-failed)
  add_test(NAME vgz-no-open-failed
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/vgz_no_open_failed.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/vgm_test.vgz ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/vgm_test.vgm
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/vgz-no-open-failed)
  set_tests_properties(vgz-no-open-failed PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "vgz_no_open_failed_ok")

  # I428: a YM (Atari ST chip music) file plays through the render entry point (ym replayer, StSound).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/ym-decode)
  add_test(NAME ym-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/ym_decode.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/ym_test.ym
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/ym-decode)
  set_tests_properties(ym-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "ym_decode_ok")

  # I434: without the GPL plugins a SID file has no replayer and OPEN answers the exact no_replayer error.
  if(NOT SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-no-replayer)
    add_test(NAME sid-no-replayer
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sid_no_replayer.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_test.sid
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-no-replayer)
    set_tests_properties(sid-no-replayer PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sid_no_replayer_ok")
  endif()

  # I432: a CHIPNSFX song plays through the render entry point (chipnsfx module). GPL module, opt-in.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/chipnsfx-decode)
    add_test(NAME chipnsfx-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/chipnsfx_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/chp_test.chp
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/chipnsfx-decode)
    set_tests_properties(chipnsfx-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "chipnsfx_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I699: an MDX song plays through the render entry point (mdxmini module). GPL module, opt-in.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/mdxmini-decode)
    add_test(NAME mdxmini-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/mdxmini_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/mdx_test.mdx
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/mdxmini-decode)
    set_tests_properties(mdxmini-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "mdxmini_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I698: MSX songs (plain .kss, MGSDRV .mgs) play through the render entry point; libkss must win over gme for .kss.
  if(SELECTA_KSS_DRIVERS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/kss-decode)
    add_test(NAME kss-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/kss_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/kss_test.kss ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/kss_test.mgs
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/kss-decode)
    set_tests_properties(kss-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "kss_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I700: PC-98 PMD and FMP songs play through the render entry point (98fmplayer module). Same gate as the other GPL modules.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/fmp98-decode)
    add_test(NAME fmp98-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/fmp98_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/pmd_test.m ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/fmp_test.ovi
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/fmp98-decode)
    set_tests_properties(fmp98-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "fmp98_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I707: an SNDH tune plays on the psgplay module (second SNDH backend); sc68 stays the default. Same gate as the other GPL modules.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/psgplay-decode)
    add_test(NAME psgplay-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/psgplay_decode.sh $<TARGET_FILE:selecta-cli> ${CMAKE_BINARY_DIR}/modules
              ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sndh/oculus_ice.sndh
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/psgplay-decode)
    set_tests_properties(psgplay-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "psgplay_decode_ok")
  endif()

  # I701: an FM Towns Euphony song plays through the render entry point (eupmini module, GPL-2.0). Same gate as the other GPL modules.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/eup-decode)
    add_test(NAME eup-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/eup_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/eup_test.eup
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/eup-decode)
    set_tests_properties(eup-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "eup_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I709a: ZX Spectrum AY songs (Flash Tracker, Fuxoft AY Language, AY Amadeus, Sound Tracker) play through the render entry point
  # (bbmp_ay module, GPL-3.0). Songs are fetched by checksum, exit 77 = skip. Same gate as the other GPL modules.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/bbmp-ay-decode)
    add_test(NAME bbmp-ay-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/bbmp_ay_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/bbmp-ay-decode)
    set_tests_properties(bbmp-ay-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "bbmp_ay_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ac: Sound Club (.sn) and Sound Club 2 (.sn2) songs play through the render entry point (bbmp_sclub module, GPL-3.0). Songs
  # are fetched by checksum, exit 77 = skip. A .sn that is not Sound Club must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sclub-decode)
    add_test(NAME sclub-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sclub_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sclub-decode)
    set_tests_properties(sclub-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sclub_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710d: Buzz (.bmx) songs on the BmxPlay machines play through the render entry point (bbmp_buzz module, GPL-3.0). Songs are
  # fetched by checksum, exit 77 = skip. A song that needs a Windows machine must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/buzz-decode)
    add_test(NAME buzz-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/buzz_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/buzz-decode)
    set_tests_properties(buzz-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "buzz_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ak: FAC SoundTracker (MSX .mus) songs play through the render entry point (bbmp_fac module, GPL-3.0). Songs and
  # drum kits are fetched by checksum, exit 77 = skip. A non-FAC .mus must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/fac-decode)
    add_test(NAME fac-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/fac_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/fac-decode)
    set_tests_properties(fac-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "fac_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ad: SoundSmith (Apple IIGS) songs play through the render entry point (bbmp_iigs module, GPL-3.0). Songs and wave
  # banks are fetched by checksum, exit 77 = skip. A song without its bank and a non-SoundSmith file must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/iigs-decode)
    add_test(NAME iigs-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/iigs_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/iigs-decode)
    set_tests_properties(iigs-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "iigs_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710r: Onyx Music Files (.omf) play through the render entry point (bbmp_onyx module, GPL-3.0 engine over libopenmpt). Songs are
  # fetched by checksum, exit 77 = skip. A non-Onyx file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/onyx-decode)
    add_test(NAME onyx-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/onyx_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/onyx-decode)
    set_tests_properties(onyx-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "onyx_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710t: Picatune 1 songs (.smufi) play through the render entry point (bbmp_smufi module, GPL-3.0 engine). Songs are fetched by
  # checksum, exit 77 = skip. A non-Picatune file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/smufi-decode)
    add_test(NAME smufi-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/smufi_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/smufi-decode)
    set_tests_properties(smufi-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "smufi_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ah: VGM Music Maker songs (.vge) play through the render entry point (bbmp_vgemm module, GPL-3.0 engine over selecta_gme). Songs are fetched by
  # checksum, exit 77 = skip. A non-VGM-Music-Maker file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/vgemm-decode)
    add_test(NAME vgemm-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/vgemm_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/vgemm-decode)
    set_tests_properties(vgemm-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "vgemm_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ai: VIC-TRACKER songs (.vt) play through the render entry point (bbmp_victracker module, GPL-3.0 engine running the tracker's 6502 player). Songs are fetched by
  # checksum, exit 77 = skip. A non-VIC-TRACKER file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/victracker-decode)
    add_test(NAME victracker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/victracker_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/victracker-decode)
    set_tests_properties(victracker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "victracker_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710aj: Westwood SND songs (.snd) play through the render entry point (bbmp_wwsnd module, GPL-3.0 engine). Songs are fetched by checksum, exit 77 = skip.
  # A non-Westwood file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/wwsnd-decode)
    add_test(NAME wwsnd-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/wwsnd_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/wwsnd-decode)
    set_tests_properties(wwsnd-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "wwsnd_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710am: SCC-Musixx songs (.sng) play through the render entry point (bbmp_smx module, GPL-3.0 engine on emu2212). Songs are fetched by checksum, exit 77 = skip.
  # Other .sng formats (GoatTracker-style, zero file, cut-off song) must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/smx-decode)
    add_test(NAME smx-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/smx_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/smx-decode)
    set_tests_properties(smx-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "smx_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710al: GoatTracker 1 and 2 songs (.sng) play through the render entry point (goattracker module: GoatTracker play routine and reSID, GPL-2.0-or-later glue). Songs are fetched by checksum, exit 77 = skip.
  # Other .sng formats (SCC-Musixx-shaped, zero file, junk, cut-off songs) must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/goattracker-decode)
    add_test(NAME goattracker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/goattracker_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/goattracker-decode)
    set_tests_properties(goattracker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "goattracker_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710an: Synder SNG-Player songs (.sng) play through the render entry point (bbmp_synder module, GPL-3.0 bbmp-synder). Songs are fetched by checksum, exit 77 = skip.
  # Other .sng formats (GoatTracker-style, zero file, cut-off song) must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/synder-decode)
    add_test(NAME synder-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/synder_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/synder-decode)
    set_tests_properties(synder-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "synder_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ao: MONOTONE songs (.mon) play through the render entry point (bbmp_monotone module, GPL-3.0 bbmp monotone.cpp). Songs are fetched by checksum, exit 77 = skip.
  # A .mon file with another magic (nuade maniacsofnoise shape), a zero file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/monotone-decode)
    add_test(NAME monotone-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/monotone_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/monotone-decode)
    set_tests_properties(monotone-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "monotone_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ap: DigiTrekker songs (.dtm) play through the render entry point (bbmp_digitrekker module, GPL-3.0 bbmp digitrekker.cpp). Songs are fetched by checksum, exit 77 = skip.
  # A Digital Tracker header, a DeFy-like file, a zero file and a cut-off song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/digitrekker-decode)
    add_test(NAME digitrekker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/digitrekker_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/digitrekker-decode)
    set_tests_properties(digitrekker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "digitrekker_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ab: Skale Tracker songs (.skm) play through the render entry point (bbmp_skale module over the patched libopenmpt). Songs are fetched by
  # checksum, exit 77 = skip. A file that is not an .skm is not claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/skale-decode)
    add_test(NAME skale-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/skale_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/skale-decode)
    set_tests_properties(skale-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "skale_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710af: T'SoundSystem songs (.tss, .tsd) play through the render entry point (bbmp_tss module, BSD 3-clause). Songs are fetched by checksum,
  # exit 77 = skip. A file that is not a TSS song is not claimed.
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/tss-decode)
  add_test(NAME tss-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/tss_decode.sh $<TARGET_FILE:selecta-cli>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/tss-decode)
  set_tests_properties(tss-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "tss_decode_ok" SKIP_RETURN_CODE 77
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")

  # I710s: P/ECE music driver songs (.pmd) play through the render entry point (bbmp_piece module, GPL-3.0). Songs are fetched by
  # checksum, exit 77 = skip. A non-song .pmd, a cut-off song and a song without the extension must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/piece-decode)
    add_test(NAME piece-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/piece_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/piece-decode)
    set_tests_properties(piece-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "piece_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710p: Noise Tracker GS 1 modules (Apple IIGS) play through the render entry point (bbmp_ntgs module, GPL-3.0). Modules are
  # fetched by checksum, exit 77 = skip. A non-module file and a cut-off module must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/ntgs-decode)
    add_test(NAME ntgs-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/ntgs_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/ntgs-decode)
    set_tests_properties(ntgs-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "ntgs_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710y: PlayStation SPU logs (.spu) play through the render entry point (bbmp_spu module, GPL-2.0-or-later). Two logs fetched by
  # checksum (tests/m3/fixtures/bbmp_spu.SOURCE), exit 77 = skip. A file that is not an SPU log must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/spu-decode)
    add_test(NAME spu-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/spu_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/spu-decode)
    set_tests_properties(spu-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "spu_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710i: JayTrax songs (.jxs) play through the render entry point (bbmp_jxs module, GPL-3.0-or-later). Two songs fetched by
  # checksum (tests/m3/fixtures/bbmp_jxs.SOURCE), exit 77 = skip. A file that is not a JayTrax song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/jxs-decode)
    add_test(NAME jxs-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/jxs_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/jxs-decode)
    set_tests_properties(jxs-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "jxs_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710j: Ken's Digital Music (.kdm) songs play through the render entry point (bbmp_kdm module, GPL-3.0-or-later). Two songs and
  # waves.kwv fetched by checksum (tests/m3/fixtures/bbmp_kdm.SOURCE), exit 77 = skip. A song without its bank, or a file that is
  # not a KDM song, must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/kdm-decode)
    add_test(NAME kdm-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/kdm_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/kdm-decode)
    set_tests_properties(kdm-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "kdm_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ae: StoneTracker (.spm + .sps) through the render entry point; skips (77) without STONEPLAYER_HARD_BIN (the player is a user system file).
  if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/stonetracker-decode)
    add_test(NAME stonetracker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/stonetracker_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/stonetracker-decode)
    set_tests_properties(stonetracker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stonetracker_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710e: Buzzic 1 and 2 (.buz, .buz2) songs play through the render entry point (bbmp_buzzic module, GPL-3.0-or-later). Four songs
  # fetched by checksum (tests/m3/fixtures/bbmp_buzzic.SOURCE), exit 77 = skip. A file that is not a Buzzic song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/buzzic-decode)
    add_test(NAME buzzic-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/buzzic_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/buzzic-decode)
    set_tests_properties(buzzic-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "buzzic_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710k: MaxTrax (.mxtx) scores play through the render entry point (bbmp_maxtrax module, GPL). Songs fetched by checksum
  # (tests/m3/fixtures/bbmp_maxtrax.SOURCE), exit 77 = skip. A file that is not a song of the format must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/maxtrax-decode)
    add_test(NAME maxtrax-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/maxtrax_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/maxtrax-decode)
    set_tests_properties(maxtrax-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "maxtrax_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710a: AXS (.axs) songs play through the render entry point (bbmp_axs module, GPL). Songs fetched by checksum
  # (tests/m3/fixtures/bbmp_axs.SOURCE), exit 77 = skip. A file that is not a song of the format must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/axs-decode)
    add_test(NAME axs-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/axs_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/axs-decode)
    set_tests_properties(axs-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "axs_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710c: BoyScout (.bsf) songs play through the render entry point (bbmp_boyscout module, GPL). Songs fetched by checksum
  # (tests/m3/fixtures/bbmp_boyscout.SOURCE), exit 77 = skip. A file that is not a song of the format must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/boyscout-decode)
    add_test(NAME boyscout-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/boyscout_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/boyscout-decode)
    set_tests_properties(boyscout-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "boyscout_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710g: CyberTracker C64 (.ct) songs play through the render entry point (bbmp_cybertracker module, GPL), on the user's own tracker disk
  # image. Songs and image fetched by checksum (tests/m3/fixtures/bbmp_cybertracker.SOURCE), exit 77 = skip. A file that is not a song of
  # the format must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/cybertracker-decode)
    add_test(NAME cybertracker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/cybertracker_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/cybertracker-decode)
    set_tests_properties(cybertracker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "cybertracker_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710aa: Mario Paint (.sho) songs play through the render entry point (bbmp_shroom module, GPL). The instrument sounds are a user-supplied system
  # file: playback runs only with env SELECTA_SHROOM_SAMPLES (path of Mariopants notes.wav), else exit 77 after the system_file_missing checks.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/shroom-decode)
    add_test(NAME shroom-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/shroom_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/shroom-decode)
    set_tests_properties(shroom-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "shroom_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710ag: T0AST (.t0ast) songs play through the render entry point (bbmp_t0ast module, GPL). Songs fetched by checksum, exit 77 = skip.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/t0ast-decode)
    add_test(NAME t0ast-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/t0ast_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/t0ast-decode)
    set_tests_properties(t0ast-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "t0ast_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710f: Cubic Tiny XM (.mxm) songs play through the render entry point (bbmp_mxm module, GPL-3.0-or-later). Two songs fetched by
  # checksum (tests/m3/fixtures/bbmp_mxm.SOURCE), exit 77 = skip. A file that is not an MXM song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/mxm-decode)
    add_test(NAME mxm-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/mxm_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/mxm-decode)
    set_tests_properties(mxm-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "mxm_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710h: DarkWave Studio projects (.dwp) play through the render entry point (bbmp_darkwave module, GPL-3.0-or-later). Three projects fetched by
  # checksum (tests/m3/fixtures/bbmp_darkwave.SOURCE), exit 77 = skip. A file that is not a DarkWave Studio project must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/darkwave-decode)
    add_test(NAME darkwave-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/darkwave_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/darkwave-decode)
    set_tests_properties(darkwave-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "darkwave_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710m: MikMod UNITRK songs (.uni) play through the render entry point (bbmp_unitrk module, libmikmod LGPL 2.1). Three songs fetched by
  # checksum (tests/m3/fixtures/bbmp_unitrk.SOURCE), exit 77 = skip. A file that is not UNITRK must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/unitrk-decode)
    add_test(NAME unitrk-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/unitrk_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/unitrk-decode)
    set_tests_properties(unitrk-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "unitrk_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710q: NoiseTrekker / ProTrekkr songs (.ntk, .ptk) play through the render entry point (bbmp_ptk module, BSD 2-clause engine, no GPL gate).
  # Four songs fetched by checksum (tests/m3/fixtures/bbmp_ptk.SOURCE), exit 77 = skip. A file that is not a Trekker song must not be claimed.
  if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/ptk-decode)
    add_test(NAME ptk-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/ptk_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/ptk-decode)
    set_tests_properties(ptk-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "ptk_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710v: SBStudio songs (.pac) play through the render entry point (bbmp_sbstudio module, libpac ISC, no GPL gate). Four songs fetched by
  # checksum (tests/m3/fixtures/bbmp_sbstudio.SOURCE), exit 77 = skip. A file that is not an SBStudio song must not be claimed.
  if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sbstudio-decode)
    add_test(NAME sbstudio-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sbstudio_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sbstudio-decode)
    set_tests_properties(sbstudio-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sbstudio_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710b: BeRoTracker songs (.brt) play through the render entry point (bbmp_berotracker module, GPL-3.0-or-later). Two songs fetched by
  # checksum (tests/m3/fixtures/bbmp_berotracker.SOURCE), exit 77 = skip. A file that is not a BeRoTracker song must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/berotracker-decode)
    add_test(NAME berotracker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/berotracker_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/berotracker-decode)
    set_tests_properties(berotracker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "berotracker_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710u: Psycle songs (.psy) play through the render entry point (bbmp_psycle module, helper child process), GPL; two Alk
  # songs fetched by checksum (tests/m3/fixtures/bbmp_psycle.SOURCE), exit 77 = skip.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/psycle-decode)
    add_test(NAME psycle-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/psycle_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/psycle-decode)
    set_tests_properties(psycle-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "psycle_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710n: Musicline Editor (.ml) songs play through the render entry point (bbmp_musicline module, GPL). Songs are fetched by
  # checksum, exit 77 = skip. A .ml that is not Musicline must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/musicline-decode)
    add_test(NAME musicline-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/musicline_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/musicline-decode)
    set_tests_properties(musicline-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "musicline_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I710z: WonderSwan (.wsr) rips play through the render entry point (bbmp_wsr module, GPL). Rips are fetched by checksum, exit 77 =
  # skip. A .wsr without the WSRF footer must not be claimed.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/wsr-decode)
    add_test(NAME wsr-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/wsr_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/wsr-decode)
    set_tests_properties(wsr-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "wsr_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I702: a pxtone song plays through the render entry point (pxtone module).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/pxtone-decode)
  add_test(NAME pxtone-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/pxtone_decode.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/pxtone_test.ptcop
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/pxtone-decode)
  set_tests_properties(pxtone-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "pxtone_decode_ok"
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")

  # I706a: a SunVox song plays through the render entry point (sunvox module); a newer-version song is refused.
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sunvox-decode)
  add_test(NAME sunvox-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sunvox_decode.sh $<TARGET_FILE:selecta-cli>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sunvox-decode)
  set_tests_properties(sunvox-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sunvox_refusal_ok" SKIP_RETURN_CODE 77
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")

  # I706b: modern SunVox songs play through the precompiled SunVox Library module (sunvox_lib); it wins over the BSD plugin and falls back without the library.
  if(TARGET sel_module_sunvox_lib AND EXISTS ${CMAKE_BINARY_DIR}/modules/sunvox_lib)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sunvox-lib-decode)
    add_test(NAME sunvox-lib-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sunvox_lib_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sunvox-lib-decode)
    set_tests_properties(sunvox-lib-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sunvox_lib_fallback_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I703: an Organya song plays through the render entry point (organya module); a wave100.wdb beside it must change the sound.
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/organya-decode)
  add_test(NAME organya-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/organya_decode.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/org_test.org ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/org_bank_test.wdb
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/organya-decode)
  set_tests_properties(organya-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "organya_decode_ok"
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")

  # I708a: a real Klystrack song (fetched by checksum, exit 77 = skip) plays through the render entry point (klystron module).
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/klystron-decode)
  add_test(NAME klystron-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/klystron_decode.sh $<TARGET_FILE:selecta-cli>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/klystron-decode)
  set_tests_properties(klystron-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "klystron_decode_ok" SKIP_RETURN_CODE 77
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")

  # I704: a QSF song plays through the render entry point (qsf module). GPL module, opt-in.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/qsf-decode)
    add_test(NAME qsf-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/qsf_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/qsf_test.qsf
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/qsf-decode)
    set_tests_properties(qsf-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "qsf_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I705a: a generated SNSF (hand-assembled 65816 + SPC700 boot handshake) plays through the render entry point (snsf module).
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/snsf-decode)
    add_test(NAME snsf-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/snsf_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/snsf_test.snsf
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/snsf-decode)
    set_tests_properties(snsf-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "snsf_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    # I705b: real Modland sets (game ROM data, fetched at test time by m3/fetch_fixture.sh, never in git; exit 77 = SKIP).
    # Default length 180 s + 10 s fade = 190 s. wolfenstein = HiROM, blackthorne = LoROM.
    foreach(game wolfenstein blackthorne)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/snsf-real-${game})
      add_test(NAME snsf-real-${game}
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/snsf_real_decode.sh $<TARGET_FILE:selecta-cli> ${game}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/snsf-real-${game})
      set_tests_properties(snsf-real-${game} PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "snsf_decode_ok" SKIP_RETURN_CODE 77
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endforeach()
  endif()

  # I704b: a real Capcom game set (Mega Man: The Power Battle, CPS-2 Kabuki key and ROM banks in the .qsflib) renders
  # through the same entry point. Length tag 0:21, fade 0: exactly 21 s, so 21 s minimum and 22 s maximum.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/qsf-real-decode)
    add_test(NAME qsf-real-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/qsf_real_decode.sh $<TARGET_FILE:selecta-cli>
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/qsf-real-decode)
    # I704c: the game set is fetched at test time (m3/fetch_fixture.sh); exit 77 = offline = SKIP.
    set_tests_properties(qsf-real-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "qsf_decode_ok" SKIP_RETURN_CODE 77
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I429: a TIATracker song plays through the render entry point (tiatracker module). GPL module, opt-in.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/tiatracker-decode)
    add_test(NAME tiatracker-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/tiatracker_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/tia_test.ttt
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/tiatracker-decode)
    set_tests_properties(tiatracker-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "tiatracker_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
  endif()

  # I434: a SID (C64) file plays through the render entry point (sid replayer, libsidplayfp). GPL module, opt-in.
  if(SELECTA_GPL_PLUGINS)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-decode)
    add_test(NAME sid-decode
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sid_decode.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_test.sid
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-decode)
    set_tests_properties(sid-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sid_decode_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-mute)
    add_test(NAME sid-mute
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sid_mute.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_test.sid
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-mute)
    set_tests_properties(sid-mute PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sid_mute_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-config)
    add_test(NAME sid-config
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sid_config.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_test.sid ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_basic.sid
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-config)
    set_tests_properties(sid-config PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sid_config_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    # I421: Pro Tracker 3 and 2 modules play through the render entry point (zxtune module). Registered only when
    # the plugin was configured (SELECTA_ZXTUNE_SRC set); the module is a dependency of selecta-cli's test run.
    if(TARGET sel_module_zxtune)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/zxtune-decode)
      add_test(NAME zxtune-decode
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/zxtune_decode.sh $<TARGET_FILE:selecta-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/zxtune_test.pt3
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/zxtune_test.pt2
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/zxtune-decode)
      set_tests_properties(zxtune-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "zxtune_decode_ok"
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endif()
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-seek)
    add_test(NAME sid-seek
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sid_seek.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_test.sid
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-seek)
    set_tests_properties(sid-seek PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sid_seek_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-subsong)
    add_test(NAME sid-subsong
      COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/sid_subsong.sh $<TARGET_FILE:selecta-cli>
              ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/sid_subsongs.sid
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/sid-subsong)
    set_tests_properties(sid-subsong PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sid_subsong_ok"
      ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")

    # I431: VGM and VGZ play through the render entry point (libvgm module); the format list is reported.
    if(TARGET sel_module_libvgm)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/libvgm-decode)
      add_test(NAME libvgm-decode
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/libvgm_decode.sh $<TARGET_FILE:selecta-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/vgm_test.vgm
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/vgm_test.vgz
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/libvgm-decode)
      set_tests_properties(libvgm-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "libvgm_decode_ok"
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endif()

    # I710l: Megadrive CYM (.cym, Capcom CPS-1 YM2151 log) plays through the render entry point: the libvgm module converts the log to VGM.
    # One song fetched by checksum (tests/m3/fixtures/bbmp_cym.SOURCE), exit 77 = skip. A log without a register write is not claimed.
    if(TARGET sel_module_libvgm)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/cym-decode)
      add_test(NAME cym-decode
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/cym_decode.sh $<TARGET_FILE:selecta-cli>
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/cym-decode)
      set_tests_properties(cym-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "cym_decode_ok" SKIP_RETURN_CODE 77
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endif()

    # I696: OPL/AdLib songs (HSC, RAW) play through the render entry point (adplug module); the format list is reported.
    if(TARGET sel_module_adplug)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/adplug-decode)
      add_test(NAME adplug-decode
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/adplug_decode.sh $<TARGET_FILE:selecta-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/adplug_test.hsc
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/adplug_test.raw
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/adplug-decode)
      set_tests_properties(adplug-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "adplug_decode_ok"
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endif()

    # I697: Megatracker plays through the render entry point (libxmp module); the format list is reported.
    if(TARGET sel_module_xmp)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/xmp-decode)
      add_test(NAME xmp-decode
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/xmp_decode.sh $<TARGET_FILE:selecta-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/xmp_test.mgt
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/xmp-decode)
      set_tests_properties(xmp-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "xmp_decode_ok"
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endif()

    # I430: Furnace/DefleMask/FamiTracker/TFM modules play through the render entry point (furnace module). The fixture is
    # one 32-row pattern at speed 6, 60 Hz: 192 ticks = 3.2 s, the expected length the script checks within 1%.
    if(TARGET sel_module_furnace)
      file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/furnace-decode)
      add_test(NAME furnace-decode
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/furnace_decode.sh $<TARGET_FILE:selecta-cli>
                ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/furnace_test.fur 3.2
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/furnace-decode)
      set_tests_properties(furnace-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "furnace_decode_ok"
        ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
    endif()
  endif()

  # I437: the scan stores the embedded cover of a tagged mp3 as a 200x200 artwork row.
  add_test(NAME library-embedded-cover
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/embedded_cover.sh $<TARGET_FILE:selecta-cli>
            ${CMAKE_CURRENT_SOURCE_DIR}/library/fixtures)
  set_tests_properties(library-embedded-cover PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "embedded_cover_ok")

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

# I184b: the idle md5 pass of the 68k build (no library threads) reads a big file across idle ticks, one chunk
# per tick, and stores md5 values that `library duplicates` groups.
if(NOT SEL_AMIGA AND SEL_VAMOS AND EXISTS "${SEL_AMIGA_CLI}")
  add_test(NAME hash-idle-vamos
    COMMAND "${CMAKE_COMMAND}" "-DMODE=vamos" "-DCLI=${SEL_CLI}" "-DVAMOS=${SEL_VAMOS}"
      "-DAMIGA_CLI=${SEL_AMIGA_CLI}" "-DMOD=${SEL_TESTMOD}"
      "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/m3/hash-idle-vamos"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/hash_idle_vamos.cmake")
  set_tests_properties(hash-idle-vamos PROPERTIES LABELS m3 FIXTURES_REQUIRED amiga_cli
    RESOURCE_LOCK vamos PASS_REGULAR_EXPRESSION "hash_idle_vamos_ok")
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

# I751 support-file downloader: consent, SHA-256 check, cut pieces, offline from the cache (selecta-cli support).
if(NOT SEL_AMIGA)
  add_test(NAME support-dl
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/support_dl.cmake")
  set_tests_properties(support-dl PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    PASS_REGULAR_EXPRESSION "support_dl_ok" TIMEOUT 60)
endif()

# I695b open-fail-toast: a failed play from search raises the toast with the engine's reason (404 from the fixture mirror).
if(NOT SEL_AMIGA AND SELECTA_UI_DESKTOP)
  add_test(NAME open-fail-toast
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${PROJECT_BINARY_DIR}/tests/open-fail-toast"
      "-DNETWORK=${SEL_NET_WORK}" -P "${CMAKE_CURRENT_SOURCE_DIR}/dui/open_fail_toast.cmake")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/open-fail-toast.tmp")
  set_tests_properties(open-fail-toast PROPERTIES LABELS open-fail-toast FIXTURES_REQUIRED net-fixture
    RESOURCE_LOCK net-fixture TIMEOUT 120 ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/open-fail-toast.tmp")
endif()

# T75a cache-fetch-shell (row I166a): LIBRARY_FETCH through the selecta shell on the same library handle.
if(NOT SEL_AMIGA)
  add_test(NAME cache-fetch-shell
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv" -P "${SEL_NET_DIR}/cache_fetch_shell.cmake")
  set_tests_properties(cache-fetch-shell PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 60)
endif()

# I853 modland-builtin-mirror: a root-relative Modland song key fetches from the built-in mirror list (no doubled
# pub/modules/ in the URL).
if(NOT SEL_AMIGA)
  add_test(NAME modland-builtin-mirror
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/catalogue.tsv" -P "${SEL_NET_DIR}/modland_builtin_mirror.cmake")
  set_tests_properties(modland-builtin-mirror PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60)
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
  add_test(NAME keep-command
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" "-DCAT=${CMAKE_CURRENT_SOURCE_DIR}/m3/smart.tsv"
      -P "${SEL_NET_DIR}/keep.cmake")
  set_tests_properties(keep-command PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    DEPENDS cache-fetch TIMEOUT 60 PASS_REGULAR_EXPRESSION "keep_ok")

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

# I1064: HippoPlayer is a Playlists group, never a Sources row (engine SOURCES rows skip SEL_SOURCE_PLAYLISTS_ONLY).
if(NOT SEL_AMIGA)
  add_executable(hippo_not_a_source_test engine/hippo_not_a_source_test.c)
  target_include_directories(hippo_not_a_source_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(hippo_not_a_source_test PRIVATE selecta_library selecta_core)
  add_test(NAME hippo-not-a-source
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/hippo_not_a_source.sh $<TARGET_FILE:selecta-cli> $<TARGET_FILE:hippo_not_a_source_test>
            ${CMAKE_CURRENT_SOURCE_DIR}/m3)
  set_tests_properties(hippo-not-a-source PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "hippo_not_a_source_ok")
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

# I550: Modland index sync on the Amiga route (SELECTA_AMIGA_ROUTE=1) hits <devilbox>/api/mirror/modland/.
if(NOT SEL_AMIGA)
  add_test(NAME source-modland-amiga-route
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" -P "${SEL_NET_DIR}/modland_amiga_route.cmake")
  set_tests_properties(source-modland-amiga-route PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "modland_amiga_route_ok")
endif()

# M3 Phase 4 (I162a): T73 network part. LIBRARY_SYNC hvsc <url> over the hvsc.c64.org fixture.
if(NOT SEL_AMIGA)
  add_test(NAME source-hvsc-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/hvsc_net.cmake")
  set_tests_properties(source-hvsc-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "hvsc_net_ok")
endif()

# I718: LIBRARY_SYNC hippo <url> fetches both lists over the hippoplayer.se fixture; HTML 200 pages are refused.
if(NOT SEL_AMIGA)
  add_test(NAME source-hippo-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/hippo_net.cmake")
  set_tests_properties(source-hippo-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "hippo_net_ok")
endif()

# I1058e: the posted playlist-keys read fills an unfetched Hippo list (1 prg request) and the pump's take reloads the folder rows.
if(NOT SEL_AMIGA)
  add_executable(hippo_lazy_open_test engine/hippo_lazy_open_test.c)
  target_include_directories(hippo_lazy_open_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(hippo_lazy_open_test PRIVATE selecta_library selecta_registry selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME hippo-lazy-open
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DTEST=$<TARGET_FILE:hippo_lazy_open_test>" "-DWORK=${SEL_NET_WORK}"
      -P "${SEL_NET_DIR}/hippo_lazy_open.cmake")
  set_tests_properties(hippo-lazy-open PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "hippo_lazy_open_ok")
endif()

# Row I710ga3: system-file download loop and job kind over the net fixture.
if(NOT SEL_AMIGA)
  add_executable(sysfile_fetch_test net/sysfile_fetch_test.c)
  target_include_directories(sysfile_fetch_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(sysfile_fetch_test PRIVATE selecta_library selecta_registry selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME sysfile-fetch
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:sysfile_fetch_test>" "-DWORK=${SEL_NET_WORK}"
      "-DFIXDIR=${SEL_NET_DIR}/fixtures" -P "${SEL_NET_DIR}/sysfile_fetch.cmake")
  set_tests_properties(sysfile-fetch PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "sysfile_fetch_ok")
endif()

# Row I710ga4: engine system-file contract (missing reason, one request, re-OPEN on land, FAILED, RETRY, INSTALL).
if(NOT SEL_AMIGA)
  add_executable(sysfile_engine_test engine/sysfile_engine_test.c)
  target_include_directories(sysfile_engine_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(sysfile_engine_test PRIVATE selecta_library selecta_registry selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME sysfile-engine
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:sysfile_engine_test>" "-DWORK=${SEL_NET_WORK}"
      "-DFIXDIR=${SEL_NET_DIR}/fixtures" -P "${SEL_NET_DIR}/sysfile_engine.cmake")
  set_tests_properties(sysfile-engine PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "sysfile_engine_ok")
endif()

# Row I710ga6: the desktop UI's system-file flow (toast, note + dialog, pick) with the loadable test-only sysfile_probe.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32 AND SELECTA_UI_DESKTOP)
  add_test(NAME sysfile-desktop
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DPLUGINS=${SEL_SYSFILE_PROBE_DIR}" -P "${SEL_NET_DIR}/sysfile_desktop.cmake")
  set_tests_properties(sysfile-desktop PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 120 PASS_REGULAR_EXPRESSION "sysfile_desktop_ok")
endif()

# Row I405: LIBRARY_SYNC aminet <url> over the aminet.net fixture, then play one archived module (reachability).
if(NOT SEL_AMIGA)
  add_test(NAME source-aminet-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/aminet_net.cmake")
  set_tests_properties(source-aminet-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "aminet_net_ok")
endif()

# Row I414: LIBRARY_SYNC sndh <url> over the sndh.atari.org fixture.
if(NOT SEL_AMIGA)
  add_test(NAME source-sndh-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/sndh_net.cmake")
  set_tests_properties(source-sndh-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "sndh_net_ok")
endif()

# Row I695d: a sndh tune with no loose file on the site comes out of the year zip (Range requests + inflate).
if(NOT SEL_AMIGA)
  add_test(NAME source-sndh-zip-member
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3" -P "${SEL_NET_DIR}/sndh_zip_member.cmake")
  set_tests_properties(source-sndh-zip-member PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "sndh_zip_member_ok")
endif()

# Row I415: LIBRARY_SYNC zxart <url> over the zxart.ee fixture.
if(NOT SEL_AMIGA)
  add_test(NAME source-zxart-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/zxart_net.cmake")
  set_tests_properties(source-zxart-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "zxart_net_ok")
endif()

# Row I1057: a zxart song key renders through selecta-cli; the download URL comes from the source's url hook (GPL zxtune).
if(NOT SEL_AMIGA AND TARGET sel_module_zxtune)
  add_test(NAME zxart-play-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DCA_FILE=${SEL_NET_CA}/ca.pem"
      "-DPLUGINS=${CMAKE_BINARY_DIR}/modules" -P "${SEL_NET_DIR}/zxart_play.cmake")
  set_tests_properties(zxart-play-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "zxart_play_ok")
endif()

# Row I406: SOURCE_SEARCH modulespl chip over the www.modules.pl fixture, then OPEN + PLAY a result (a zip around one mod).
if(NOT SEL_AMIGA)
  add_test(NAME source-modulespl-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/modulespl.cmake")
  set_tests_properties(source-modulespl-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "modulespl_net_ok")
endif()

# Row I416: LIBRARY_SYNC keygenmusic <url> over the keygenmusic.tk fixture.
if(NOT SEL_AMIGA)
  add_test(NAME source-keygenmusic-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/keygenmusic_net.cmake")
  set_tests_properties(source-keygenmusic-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "keygenmusic_net_ok")
endif()

# Row I413: LIBRARY_SYNC asma <url> over the asma.atari.org fixture.
if(NOT SEL_AMIGA)
  add_test(NAME source-asma-net
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
      -P "${SEL_NET_DIR}/asma_net.cmake")
  set_tests_properties(source-asma-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "asma_net_ok")
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
  # I213a: the stb_vorbis FPU math substitutes equal libm on the inputs stb_vorbis uses.
  add_executable(stb_vorbis_fpu_math_test m3/stb_vorbis_fpu_math_test.c)
  add_test(NAME stb-vorbis-fpu-math COMMAND stb_vorbis_fpu_math_test)
  set_tests_properties(stb-vorbis-fpu-math PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stb_vorbis_fpu_math_ok")
  # I161c: sel_archive_extract_file streams a zip member to disk with a bounded heap (generated 8 MB member).
  add_executable(archive_stream_test archive_stream_test.c)
  target_link_libraries(archive_stream_test PRIVATE selecta_core selecta_shared_archive)
  add_test(NAME archive-stream COMMAND archive_stream_test)
  set_tests_properties(archive-stream PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "archive_stream_ok")
  # I433a: the AT3 (.aks) importer parses the Arkos Tracker 3 example song into the model; corrupt input fails cleanly.
  add_executable(at3_song_test m3/at3_song_test.c)
  target_link_libraries(at3_song_test PRIVATE selecta_arkos)
  add_test(NAME at3-song COMMAND at3_song_test ${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/arkos_lopears.aks)
  set_tests_properties(at3-song PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "at3_song_ok")
  # I819: the one format label (sel_format_label) every UI shows.
  add_executable(format_label_test format_label_test.c)
  target_link_libraries(format_label_test PRIVATE selecta_formats)
  add_test(NAME format-label-unit COMMAND format_label_test)
  set_tests_properties(format-label-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "format_label_ok")
  add_executable(xxh32_test xxh32_test.c)
  target_include_directories(xxh32_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(xxh32_test PRIVATE selecta_library selecta_core)
  add_test(NAME xxh32-unit COMMAND xxh32_test)
  set_tests_properties(xxh32-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "xxh32_ok")
  # I227: the library heap falls back to a smaller block when fast RAM is fragmented.
  add_executable(library_heap_fallback_test library_heap_fallback_test.c)
  target_include_directories(library_heap_fallback_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_heap_fallback_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-opens-on-fragmented-ram COMMAND library_heap_fallback_test)
  set_tests_properties(library-opens-on-fragmented-ram PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_heap_fallback_ok")
  # I787a: a v12 file with 40k songs migrates to v13 through sel_library_open on the small and 2 MiB heap tiers.
  add_executable(library_migrate_big_test library_migrate_big_test.c)
  target_include_directories(library_migrate_big_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_migrate_big_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-migrate-big COMMAND library_migrate_big_test ${CMAKE_CURRENT_BINARY_DIR})
  # I1094a: the one duplicate rank (SEL_DUP_RANK_SQL) and the maintained groups (dup_of, dup_members) on a planted 3-group fixture.
  add_executable(dup_rank_test library/dup_rank_test.c)
  target_include_directories(dup_rank_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(dup_rank_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-dup-rank COMMAND dup_rank_test)
  set_tests_properties(library-dup-rank PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "dup_rank_ok")
  # I1094c: library.hide_duplicates hides dup_of copies from the search arms (bare words and queries), copies:/duplicate: show them.
  add_executable(hide_dups_test library/hide_dups_test.c)
  target_include_directories(hide_dups_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(hide_dups_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-hide-dups COMMAND hide_dups_test)
  set_tests_properties(library-hide-dups PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "hide_dups_ok")
  add_executable(dup_writers_test library/dup_writers_test.c)
  target_include_directories(dup_writers_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(dup_writers_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-dup-writers COMMAND dup_writers_test)
  set_tests_properties(library-dup-writers PROPERTIES TIMEOUT 120 LABELS "m3;library" PASS_REGULAR_EXPRESSION "dup_writers_ok")
  # I1094b: catalogue-sync style import (256-row scan batches) leaves dup_of/dup_members equal to a fresh sel_dup_rebuild.
  add_test(NAME library-dup-sync COMMAND dup_writers_test bench 4000 3500 ${CMAKE_CURRENT_BINARY_DIR}/dup_sync.db)
  set_tests_properties(library-dup-sync PROPERTIES TIMEOUT 120 LABELS "m3;library" PASS_REGULAR_EXPRESSION "sync_import songs=4000 rows=3500 ok=1")
  # I1094b: sel_dup_rebuild (bulk writers, v18 migration) stays under 1 MiB of heap on the a060 tier at 20000 songs inside a transaction.
  add_test(NAME library-dup-rebuild-heap COMMAND dup_writers_test probe 20000 ${CMAKE_CURRENT_BINARY_DIR}/dup_heap.db txn)
  set_tests_properties(library-dup-rebuild-heap PROPERTIES TIMEOUT 120 LABELS "m3;library" PASS_REGULAR_EXPRESSION "dup_rebuild_heap_ok")
  set_tests_properties(library-migrate-big PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_migrate_big_ok")
  # I790: every level of the source page drill on 600k songs stays inside its window and under 200 ms.
  add_executable(library_source_drill_test library_source_drill_test.c)
  target_include_directories(library_source_drill_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_source_drill_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-source-drill COMMAND library_source_drill_test ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(library-source-drill PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_source_drill_ok")
  # I848a: the OPEN lookups (best source by md5, key/path) are index searches on a 300k-song catalogue, so a double-click never scans songs.
  add_executable(library_best_source_plan_test library_best_source_plan_test.c)
  target_include_directories(library_best_source_plan_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_best_source_plan_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-best-source-plan COMMAND library_best_source_plan_test ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(library-best-source-plan PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_best_source_plan_ok")
  # I296: a scan writes the author tag into song_composers; the songdb sync overrides it.
  add_executable(library_scan_composers_test library_scan_composers_test.c)
  target_include_directories(library_scan_composers_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_scan_composers_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-scan-composers COMMAND library_scan_composers_test)
  set_tests_properties(library-scan-composers PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_scan_composers_ok")
  # I787: every Your Library category read answers in under 200 ms warm on a 600 000-song library.
  add_executable(library_items_budget_test library_items_budget_test.c)
  target_include_directories(library_items_budget_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_items_budget_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-items-budget COMMAND library_items_budget_test)
  set_tests_properties(library-items-budget PROPERTIES LABELS m3 TIMEOUT 300 PASS_REGULAR_EXPRESSION "library_items_budget_ok")
  # I687: an album without an embedded cover still gets its albums row (trigger + migration backfill) and reaches the Albums list.
  add_executable(library_album_rows_test library_album_rows_test.c)
  target_include_directories(library_album_rows_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_album_rows_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-album-rows COMMAND library_album_rows_test)
  set_tests_properties(library-album-rows PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_album_rows_ok")
  # I336: the library job thread runs the folder scan task off the caller's thread.
  add_executable(library_task_test library_task_test.c)
  target_include_directories(library_task_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_task_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-task-thread COMMAND library_task_test)
  set_tests_properties(library-task-thread PROPERTIES LABELS "m3;library" PASS_REGULAR_EXPRESSION "library_task_ok")
  # I1077b: the store's write generation counts committed write transactions (sqlite commit hook).
  add_executable(library_write_gen_test library/write_gen_test.c)
  target_include_directories(library_write_gen_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_write_gen_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-write-gen COMMAND library_write_gen_test)
  add_executable(library_schema_tx_test library/schema_tx_test.c)
  target_include_directories(library_schema_tx_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_schema_tx_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-schema-tx COMMAND library_schema_tx_test ${CMAKE_CURRENT_BINARY_DIR})
  set_tests_properties(library-schema-tx PROPERTIES PASS_REGULAR_EXPRESSION "schema_tx_ok")
  set_tests_properties(library-write-gen PROPERTIES LABELS "m3;library" PASS_REGULAR_EXPRESSION "write_gen_ok")
  # I219g: the KPlayer timing walker behind the native .mod seek on Amiga (pure C, so the host runs it).
  add_executable(kplayer_modwalk_test kplayer_modwalk_test.c ${PROJECT_SOURCE_DIR}/plugins/replayers/kplayer/modwalk.c)
  target_include_directories(kplayer_modwalk_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/replayers/kplayer ${PROJECT_SOURCE_DIR}/include)
  add_test(NAME kplayer-modwalk COMMAND kplayer_modwalk_test)
  set_tests_properties(kplayer-modwalk PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "kplayer_modwalk_ok")
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

  # T93 (row I193): lyrics from a .lrc, a tracker song message and the DEViLBOX endpoint, current line in the snapshot.
  add_executable(lyrics_unit_test lyrics_unit_test.c)
  target_include_directories(lyrics_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(lyrics_unit_test PRIVATE selecta_library selecta_core)
  add_test(NAME lrc-unit COMMAND lyrics_unit_test)
  # I420: the DEViLBOX default base is https on every build with TLS (this block is the non-Amiga build).
  add_executable(devilbox_base_test net/devilbox_base_test.c)
  target_include_directories(devilbox_base_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(devilbox_base_test PRIVATE selecta_library selecta_core)
  add_test(NAME devilbox-default-https COMMAND devilbox_base_test)
  # I413: the asma list reader, artist rule and source descriptor, on the recorded asmadb.js excerpt.
  add_executable(asma_unit_test asma_unit_test.c)
  target_sources(asma_unit_test PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/sources/asma/asma.c ${PROJECT_SOURCE_DIR}/plugins/sources/asma/asma_sync.c
    ${PROJECT_SOURCE_DIR}/plugins/sources/asma/asma_net.c)
  target_include_directories(asma_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/sources/asma)
  target_compile_definitions(asma_unit_test PRIVATE
    SEL_ASMA_FIXTURE="${PROJECT_SOURCE_DIR}/tests/net/fixtures/asma.atari.org/asmadb/asmadb.js")
  target_link_libraries(asma_unit_test PRIVATE selecta_shared_source selecta_core)
  add_test(NAME asma-unit COMMAND asma_unit_test)
  set_tests_properties(asma-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "asma_unit_ok")

  # I405: the aminet INDEX line parser and source descriptor, on the recorded INDEX fixture.
  add_executable(aminet_unit_test aminet_unit_test.c)
  target_sources(aminet_unit_test PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/sources/aminet/aminet.c ${PROJECT_SOURCE_DIR}/plugins/sources/aminet/aminet_sync.c
    ${PROJECT_SOURCE_DIR}/plugins/sources/aminet/aminet_net.c)
  target_include_directories(aminet_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/sources/aminet)
  target_compile_definitions(aminet_unit_test PRIVATE
    SEL_AMINET_INDEX="${PROJECT_SOURCE_DIR}/tests/net/fixtures/aminet.net/INDEX")
  target_link_libraries(aminet_unit_test PRIVATE selecta_shared_source selecta_shared_archive selecta_core)
  add_test(NAME aminet-unit COMMAND aminet_unit_test)
  set_tests_properties(aminet-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "aminet_unit_ok")
  # I414: the sndh directory reader, download page parser and source descriptor, on the recorded fixtures.
  add_executable(sndh_unit_test sndh_unit_test.c)
  target_sources(sndh_unit_test PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/sources/sndh/sndh.c ${PROJECT_SOURCE_DIR}/plugins/sources/sndh/sndh_sync.c
    ${PROJECT_SOURCE_DIR}/plugins/sources/sndh/sndh_net.c)
  target_include_directories(sndh_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/sources/sndh)
  target_compile_definitions(sndh_unit_test PRIVATE
    SEL_SNDH_ZIP="${PROJECT_SOURCE_DIR}/tests/net/fixtures/sndh.atari.org/files/sndh2026_lf.zip"
    SEL_SNDH_PAGE="${PROJECT_SOURCE_DIR}/tests/net/fixtures/sndh.atari.org/download.php")
  target_link_libraries(sndh_unit_test PRIVATE selecta_shared_source selecta_shared_archive selecta_core)
  add_test(NAME sndh-unit COMMAND sndh_unit_test)
  set_tests_properties(sndh-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "sndh_unit_ok")

  # I415: the zxart page reader, format table, length parser and source descriptor, on the recorded API answers.
  add_executable(zxart_unit_test zxart_unit_test.c)
  target_sources(zxart_unit_test PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/sources/zxart/zxart.c ${PROJECT_SOURCE_DIR}/plugins/sources/zxart/zxart_sync.c
    ${PROJECT_SOURCE_DIR}/plugins/sources/zxart/zxart_net.c)
  target_include_directories(zxart_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/sources/zxart)
  target_compile_definitions(zxart_unit_test PRIVATE
    SEL_ZXART_FIX_TUNES="${PROJECT_SOURCE_DIR}/tests/net/fixtures/zxart.ee/api/language:eng/export:zxMusic/start:0/limit:1000"
    SEL_ZXART_FIX_PEOPLE="${PROJECT_SOURCE_DIR}/tests/net/fixtures/zxart.ee/api/language:eng/export:author/start:0/limit:1000")
  target_link_libraries(zxart_unit_test PRIVATE selecta_shared_source selecta_core)
  add_test(NAME zxart-unit COMMAND zxart_unit_test)
  set_tests_properties(zxart-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "zxart_unit_ok")

  # I416: the keygenmusic list reader, path decoding, format and group rules and source descriptor.
  add_executable(keygenmusic_unit_test keygenmusic_unit_test.c)
  target_sources(keygenmusic_unit_test PRIVATE
    ${PROJECT_SOURCE_DIR}/plugins/sources/keygenmusic/keygenmusic.c ${PROJECT_SOURCE_DIR}/plugins/sources/keygenmusic/keygenmusic_sync.c
    ${PROJECT_SOURCE_DIR}/plugins/sources/keygenmusic/keygenmusic_net.c)
  target_include_directories(keygenmusic_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/plugins/sources/keygenmusic)
  target_compile_definitions(keygenmusic_unit_test PRIVATE
    SEL_KEYGEN_FIX_LIST="${PROJECT_SOURCE_DIR}/tests/net/fixtures/keygenmusic.tk/kgm/lib.txt")
  target_link_libraries(keygenmusic_unit_test PRIVATE selecta_shared_source selecta_core)
  add_test(NAME keygenmusic-unit COMMAND keygenmusic_unit_test)
  set_tests_properties(keygenmusic-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "keygenmusic_unit_ok")
  # I376: the bottom-of-stack canary (apps/common/stack_canary.c, used by amiga_start.c on the Amiga).
  add_executable(stack_canary_test stack_canary_test.c ${PROJECT_SOURCE_DIR}/apps/common/stack_canary.c)
  target_include_directories(stack_canary_test PRIVATE ${PROJECT_SOURCE_DIR}/apps/common)
  add_test(NAME stack-canary COMMAND stack_canary_test)
  set_tests_properties(stack-canary PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stack_canary_unit_ok")
  # I671b: the text UI fallback opens a console when none is attached (apps/common/console_fallback.c).
  add_executable(console_fallback_test console_fallback_test.c ${PROJECT_SOURCE_DIR}/apps/common/console_fallback.c)
  target_include_directories(console_fallback_test PRIVATE ${PROJECT_SOURCE_DIR}/apps/common)
  add_test(NAME console-fallback COMMAND console_fallback_test)
  set_tests_properties(console-fallback PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "console_fallback_unit_ok")
  set_tests_properties(lrc-unit PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "lyrics_unit_ok")
  # I266: a cancelled job thread's send is SEL_NET_E_IO, not a timeout (TLS retried until its deadline).
  add_executable(net_cancel_send_test net_cancel_send_test.c)
  target_include_directories(net_cancel_send_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(net_cancel_send_test PRIVATE selecta_library selecta_core)
  add_test(NAME net-cancel-send COMMAND net_cancel_send_test)
  set_tests_properties(net-cancel-send PROPERTIES LABELS m3 TIMEOUT 20 PASS_REGULAR_EXPRESSION "net_cancel_send_ok")
  # I1044: the connection table holds every searchable source's search thread plus stream, prefetch, sync and DLNA at once.
  add_executable(net_table_fanout_test net_table_fanout_test.c)
  target_include_directories(net_table_fanout_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(net_table_fanout_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME net-table-fanout COMMAND net_table_fanout_test)
  set_tests_properties(net-table-fanout PROPERTIES LABELS m3 TIMEOUT 20 PASS_REGULAR_EXPRESSION "net_table_fanout_ok")
  # I196d: the recording split walker cuts at MP3 frame and Ogg page starts.
  # I168b: the no-thread prefetch downloads in resumable slices; one test per defect, named after the symptom.
  add_executable(cache_slice_test library/cache_slice_test.c)
  target_include_directories(cache_slice_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(cache_slice_test PRIVATE selecta_core)
  add_test(NAME cache-slice COMMAND cache_slice_test)
  set_tests_properties(cache-slice PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "cache_slice_ok")
  # I1127b: content companions. U1/U2 sanitiser + trim exemption (unit); the reachability test fetches a remote .eup
  # through the library cache and asserts the banks arrived (net-fixture).
  add_executable(content_companions_test m3/content_companions_test.c)
  target_include_directories(content_companions_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(content_companions_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME content-companions COMMAND content_companions_test)
  set_tests_properties(content-companions PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "content_companions_ok")
  if(SELECTA_GPL_PLUGINS)
    add_test(NAME remote-eup-banks-net
      COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DWORK=${SEL_NET_WORK}" "-DFIXDIR=${SEL_NET_DIR}/fixtures"
        "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures" "-DPLUGINS=${CMAKE_BINARY_DIR}/modules" -P "${SEL_NET_DIR}/remote_eup_banks.cmake")
    set_tests_properties(remote-eup-banks-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture TIMEOUT 120)
    # I1127c: the same song with its banks on no mirror plays and Now Playing carries the "Sample bank not found" flag.
    if(SELECTA_UI_DESKTOP)
      set(SEL_MISSING_BANK_UI desktop)
    else()
      set(SEL_MISSING_BANK_UI text)
    endif()
    add_test(NAME remote-eup-missing-bank-net
      COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${SEL_NET_WORK}"
        "-DUI=${SEL_MISSING_BANK_UI}" "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures" "-DPLUGINS=${CMAKE_BINARY_DIR}/modules" -P "${SEL_NET_DIR}/remote_eup_missing_bank.cmake")
    set_tests_properties(remote-eup-missing-bank-net PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture TIMEOUT 120)
    # I710ga7: system files end to end (real .ct, empty config dir, fixture CSDb mirror); SKIP 77 without the cached zip.
    add_test(NAME sysfile-cybertracker
      COMMAND sh "${SEL_NET_DIR}/sysfile_cybertracker.sh" "${CMAKE_COMMAND}" "$<TARGET_FILE:selecta>" "${SEL_NET_WORK}"
        "${SEL_NET_DIR}/fixtures" "${CMAKE_BINARY_DIR}/modules" "${SEL_NET_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/m3")
    set_tests_properties(sysfile-cybertracker PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
      TIMEOUT 240 SKIP_RETURN_CODE 77 PASS_REGULAR_EXPRESSION "sysfile_cybertracker_ok")
  endif()
  add_executable(record_cut_unit_test record_cut_unit_test.c)
  target_include_directories(record_cut_unit_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(record_cut_unit_test PRIVATE selecta_library selecta_core)
  add_test(NAME record-cut COMMAND record_cut_unit_test)
  set_tests_properties(record-cut PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "record_cut_unit_ok")
  add_test(NAME lyrics
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3"
            -P "${SEL_NET_DIR}/lyrics.cmake")
  set_tests_properties(lyrics PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "lyrics_ok")

  # I419: the server answers 404 {"error":"unknown md5"} for a hash without lyrics; any 404 is SEL_DVB_LYRICS_UNKNOWN.
  add_executable(lyrics_status_test lyrics_status_test.c)
  target_include_directories(lyrics_status_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(lyrics_status_test PRIVATE selecta_library selecta_core)
  add_test(NAME lyrics-status
    COMMAND "${CMAKE_COMMAND}" "-DEXE=$<TARGET_FILE:lyrics_status_test>" "-DWORK=${SEL_NET_WORK}"
            -P "${SEL_NET_DIR}/lyrics_status.cmake")
  set_tests_properties(lyrics-status PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "lyrics_status_ok")

  # T86 similar part (row I183): also-by and similar through selecta-cli; the duplicates part is row I184.
  set(SEL_SQLITE3 "$<TARGET_FILE:sel_sqlite3_shell>")
  add_test(NAME similar-duplicates
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre"
            "-DGOLDEN=${PROJECT_SOURCE_DIR}/tests/m3/similar.golden" "-DSQLITE3=${SEL_SQLITE3}"
            -P "${SEL_NET_DIR}/similar.cmake")
  set_tests_properties(similar-duplicates PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "similar_ok")

  # T94 URL part (row I195): OPEN of an http URL and a pipe path, M3U with only URL lines.
  add_test(NAME stream-url
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DM3=${PROJECT_SOURCE_DIR}/tests/m3"
            "-DSONG=${PROJECT_SOURCE_DIR}/tests/net/fixtures/fixture.test/stream.mod" -P "${SEL_NET_DIR}/stream_url.cmake")
  set_tests_properties(stream-url PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_url_ok")

  # I195e: a 404 and a 503 whose body is a playable module report open_failed, never play and never no_replayer.
  add_test(NAME stream-status
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/stream_status.cmake")
  set_tests_properties(stream-status PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_status_ok")

  # I195b: OPEN of an http URL plays while it downloads (server sends 60% of the file and stalls).
  add_test(NAME stream-live
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DSLOW=${SEL_NET_DIR}/slow_server.py"
            -P "${SEL_NET_DIR}/stream_live.cmake")
  set_tests_properties(stream-live PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_live_ok")

  # I195d: the same for Ogg Vorbis (stb_vorbis pushdata path); one long file, no repeat (chained Ogg streams).
  # I1080: a podcast MP3 opens with an ID3v2 tag larger than the 256 KB ring (620 KB, like the real Pixelated Audio feed):
  # the live open skips the tag as it streams. The 200 KB case is a tag that fits the ring but not the 16 KB head.
  foreach(_kb 620 200)
    if(_kb EQUAL 620)
      set(_name stream-live-id3)
    else()
      set(_name stream-live-id3-small)
    endif()
    add_test(NAME ${_name}
      COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
              "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DSLOW=${SEL_NET_DIR}/slow_server.py"
              -DREPEAT=400 -DTAG_KB=${_kb} -P "${SEL_NET_DIR}/stream_live.cmake")
    set_tests_properties(${_name} PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
      TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_live_ok")
  endforeach()

  add_test(NAME stream-live-ogg
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/long.ogg" "-DSLOW=${SEL_NET_DIR}/slow_server.py"
            -DREPEAT=1 -DEXT=ogg -P "${SEL_NET_DIR}/stream_live.cmake")
  set_tests_properties(stream-live-ogg PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_live_ok")

  # I195f: a chained Ogg stream (new BOS page, new serial) resets the decoder and changes the title tag.
  add_test(NAME stream-live-ogg-chained
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DOGG=${PROJECT_SOURCE_DIR}/tests/m3/stream/chained.ogg" "-DSLOW=${SEL_NET_DIR}/slow_server.py"
            -P "${SEL_NET_DIR}/stream_chained.cmake")
  set_tests_properties(stream-live-ogg-chained PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_chained_ok")

  # I195h: the stb_vorbis adapter makes no malloc or alloca after (or even during) the render path: every decoder
  # allocation comes from the host-allocated arena. The test compiles the adapter and stb_vorbis.c with counting
  # malloc/alloca (stb_vorbis_count.h) and drives the plugin hooks over chained.ogg; the tiny variant has a 4 KB
  # arena and must fail rp_open cleanly (VORBIS_outofmem).
  foreach(_variant IN ITEMS "" "_tiny")
    add_executable(stb_vorbis_arena${_variant}_test m3/stb_vorbis_arena_test.c
      "${PROJECT_SOURCE_DIR}/plugins/replayers/stb_vorbis/adapter.c" "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c")
    target_include_directories(stb_vorbis_arena${_variant}_test PRIVATE "${PROJECT_SOURCE_DIR}/include"
      "${PROJECT_SOURCE_DIR}/third_party/stb_vorbis")
    target_compile_definitions(stb_vorbis_arena${_variant}_test PRIVATE SEL_PLUGIN_KIND_BUILD=1)
    target_compile_options(stb_vorbis_arena${_variant}_test PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/m3/stb_vorbis_count.h")
    if(_variant STREQUAL "_tiny")
      target_compile_definitions(stb_vorbis_arena${_variant}_test PRIVATE VORBIS_ARENA_BYTES=4096u)
    endif()
    set_source_files_properties("${PROJECT_SOURCE_DIR}/third_party/stb_vorbis/stb_vorbis.c" PROPERTIES COMPILE_OPTIONS "-w" COMPILE_DEFINITIONS STB_VORBIS_NO_STDIO)
    target_link_libraries(stb_vorbis_arena${_variant}_test PRIVATE m selecta_plugin_shared)
    set(_sv_args "${PROJECT_SOURCE_DIR}/tests/m3/stream/chained.ogg")
    if(_variant STREQUAL "_tiny")
      list(APPEND _sv_args tiny)
    endif()
    add_test(NAME stb-vorbis-arena${_variant} COMMAND stb_vorbis_arena${_variant}_test ${_sv_args})
    set_tests_properties(stb-vorbis-arena${_variant} PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "stb_vorbis_arena_ok${_variant}")
  endforeach()

  # I196b: OPEN + OPTION record_to plays and records from one request.
  add_test(NAME stream-record-play
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DSLOW=${SEL_NET_DIR}/slow_server.py"
            -P "${SEL_NET_DIR}/stream_record_play.cmake")
  set_tests_properties(stream-record-play PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "stream_record_play_ok")

  # I196a + I196c: ICY metadata stripped, split per song into a record directory, play --record.
  add_test(NAME stream-icy
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/stream_icy.cmake")
  set_tests_properties(stream-icy PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "stream_icy_ok")

  # I725a: a saved stream list (STREAM_ADD) and a .pls / .m3u URL resolved to the stream, ICY title shown.
  add_test(NAME stream-list
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            "-DSQLITE3=${SEL_SQLITE3}" -P "${SEL_NET_DIR}/stream_list.cmake")
  set_tests_properties(stream-list PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "stream_list_ok")

  # I725c: the radio-browser directory source: search sync, click ping, playback of the resolved stream.
  add_test(NAME radio-browser
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/radio_browser.cmake")
  set_tests_properties(radio-browser PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "radio_browser_ok")

  # I1029: a station whose endless stream no replayer can stream fails the open at once instead of being drained.
  add_test(NAME radio-endless
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/radio_endless.cmake")
  set_tests_properties(radio-endless PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "radio_endless_ok")

  # I725e: the Jamendo source: popular-tracks sync, missing client id, streamed playback, no audio file cached.
  add_test(NAME jamendo
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/jamendo.cmake")
  set_tests_properties(jamendo PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "jamendo_ok")

  # I725d: the Internet Archive source: licence filter, 429 / Retry-After back-off, User-Agent, streamed playback.
  add_test(NAME archiveorg
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/archiveorg.cmake")
  set_tests_properties(archiveorg PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "archiveorg_ok")

  # I1024a: SOURCE_SEARCH archiveorg over a local advancedsearch/metadata fixture: licence gate, 8-hit metadata cap, -1 when unreachable.
  add_test(NAME archiveorg-search
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/archiveorg_search.cmake")
  set_tests_properties(archiveorg-search PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "archiveorg_search_ok")

  # I1024d: SOURCE_SEARCH radiobrowser over a local /json/stations/search fixture: codec filter, sink rows, -1 when unreachable.
  add_test(NAME radiobrowser-search
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/radiobrowser_search.cmake")
  set_tests_properties(radiobrowser-search PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "radiobrowser_search_ok")

  # I1024e: SOURCE_SEARCH ccmixter over a local /api/query fixture: licence and MP3 filter, sink rows, -1 when unreachable.
  add_test(NAME ccmixter-search
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/ccmixter_search.cmake")
  set_tests_properties(ccmixter-search PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "ccmixter_search_ok")

  # I1024b: SOURCE_SEARCH jamendo over a local /tracks fixture: client id, audio filter, sink rows, -1 when unreachable or keyless.
  add_test(NAME jamendo-search
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/jamendo_search.cmake")
  set_tests_properties(jamendo-search PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "jamendo_search_ok")

  # I1024g: SOURCE_SEARCH aminet over the aminet.net fixture: search page request, the sync's row shape, -1 when unreachable.
  add_test(NAME aminet-search
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/aminet_search.cmake")
  set_tests_properties(aminet-search PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "aminet_search_ok")

  # I1024f: SOURCE_SEARCH zxart over the zxart.ee fixture: title filter request, the sync's row shape, -1 when unreachable.
  add_test(NAME zxart-search
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" -P "${SEL_NET_DIR}/zxart_search.cmake")
  set_tests_properties(zxart-search PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "zxart_search_ok")

  # I725f: the podcast source: RSS parse, Podcast Index search auth, Range playback and seek, User-Agent.
  add_test(NAME podcast
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/podcast.cmake")
  set_tests_properties(podcast PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "podcast_ok")

  # I894a3: the dlna source: recursive Browse into library.db (153 songs), SSDP discover, play from the res URL.
  add_test(NAME dlna
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DSRV=${SEL_NET_DIR}/upnp_server.py"
            -P "${SEL_NET_DIR}/dlna.cmake")
  set_tests_properties(dlna PROPERTIES LABELS m3 TIMEOUT 90 PASS_REGULAR_EXPRESSION "dlna_ok")

  # I892e1: CASTV2 session (TLS with a self-signed device certificate, CONNECT, heartbeat, RECEIVER_STATUS).
  add_executable(cast_session_test net/cast_session_test.c)
  target_link_libraries(cast_session_test PRIVATE selecta_shared_cast selecta_library)
  add_test(NAME cast-session
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:cast_session_test>" "-DWORK=${SEL_NET_WORK}"
            "-DSRV=${SEL_NET_DIR}/cast_server.py" -P "${SEL_NET_DIR}/cast_session.cmake")
  set_tests_properties(cast-session PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "cast_session_ok")

  # I892e2: LAUNCH + LOAD of a URL served by the built-in HTTP file server (md5 of the fetched bytes equals the file).
  add_executable(cast_load_test net/cast_load_test.c)
  target_link_libraries(cast_load_test PRIVATE selecta_shared_cast selecta_library)
  add_test(NAME cast-load
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:cast_load_test>" "-DWORK=${SEL_NET_WORK}"
            "-DSRV=${SEL_NET_DIR}/cast_server.py" "-DFILE=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3"
            -P "${SEL_NET_DIR}/cast_load.cmake")
  set_tests_properties(cast-load PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "cast_load_ok")

  # I892e1: Chromecast mDNS discovery (pure parser with name compression + browse against the loopback responder).
  add_executable(cast_discover_test net/cast_discover_test.c)
  target_link_libraries(cast_discover_test PRIVATE selecta_shared_cast selecta_library)
  add_test(NAME cast-discover
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:cast_discover_test>" "-DWORK=${SEL_NET_WORK}"
            "-DSRV=${SEL_NET_DIR}/mdns_server.py" -P "${SEL_NET_DIR}/cast_discover.cmake")
  set_tests_properties(cast-discover PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "cast_discover_ok")

  # I899a: entropy pool (threshold, distinct output, seed file) and the cast HTTP server refusing to start unseeded.
  add_executable(entropy_test net/entropy_test.c)
  target_link_libraries(entropy_test PRIVATE selecta_shared_cast selecta_library)
  target_include_directories(entropy_test PRIVATE "${PROJECT_SOURCE_DIR}/src/library")
  add_test(NAME entropy COMMAND entropy_test "${CMAKE_CURRENT_BINARY_DIR}")
  set_tests_properties(entropy PROPERTIES LABELS m3 TIMEOUT 30 PASS_REGULAR_EXPRESSION "entropy_ok")

  # I899g: TLS backend choice and the missing-AmiSSL fallback. "mirrors" runs on the real host library; "amissl" compiles
  # tls_gate.c with SEL_TLS_AMISSL against a stub opener (the AmiSSL path itself is rig-checked, tests/amiga).
  add_executable(tls_gate_test net/tls_gate_test.c)
  target_link_libraries(tls_gate_test PRIVATE selecta_library)
  target_include_directories(tls_gate_test PRIVATE "${PROJECT_SOURCE_DIR}/src/library/net")
  add_executable(tls_gate_amissl_test net/tls_gate_test.c ${PROJECT_SOURCE_DIR}/src/library/net/tls_gate.c)
  target_compile_definitions(tls_gate_amissl_test PRIVATE GATE_AMISSL_STUB SEL_TLS_AMISSL)
  target_include_directories(tls_gate_amissl_test PRIVATE "${PROJECT_SOURCE_DIR}/src/library/net")
  add_test(NAME tls-gate-mirrors COMMAND tls_gate_test mirrors)
  set_tests_properties(tls-gate-mirrors PROPERTIES LABELS m3 TIMEOUT 30 PASS_REGULAR_EXPRESSION "tls_gate_mirrors_ok")
  foreach(rc 0 1 2)
    add_test(NAME tls-gate-amissl-${rc} COMMAND tls_gate_amissl_test amissl ${rc})
    set_tests_properties(tls-gate-amissl-${rc} PROPERTIES LABELS m3 TIMEOUT 30 PASS_REGULAR_EXPRESSION "tls_gate_amissl_ok rc=${rc} ")
  endforeach()

  # I894a1: UPnP MediaServer discovery (SSDP unicast to the fixture, HTTP description, ContentDirectory controlURL).
  add_executable(upnp_discover_test net/upnp_discover_test.c)
  target_link_libraries(upnp_discover_test PRIVATE selecta_shared_upnp selecta_library)
  add_test(NAME upnp-discover
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:upnp_discover_test>" "-DWORK=${SEL_NET_WORK}"
            "-DSRV=${SEL_NET_DIR}/upnp_server.py" -P "${SEL_NET_DIR}/upnp_discover.cmake")
  set_tests_properties(upnp-discover PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "upnp_discover_ok")
  # I894a2: ContentDirectory Browse (SOAP, paged) + DIDL-Lite parse against the same fixture.
  add_executable(upnp_browse_test net/upnp_browse_test.c)
  target_link_libraries(upnp_browse_test PRIVATE selecta_shared_upnp selecta_library)
  add_test(NAME upnp-browse
    COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:upnp_browse_test>" "-DWORK=${SEL_NET_WORK}"
            "-DSRV=${SEL_NET_DIR}/upnp_server.py" -P "${SEL_NET_DIR}/upnp_browse.cmake")
  set_tests_properties(upnp-browse PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "upnp_browse_ok")

  # I725g: the ccMixter source: one query per sync, per-upload licence shown, User-Agent, streamed playback.
  add_test(NAME ccmixter
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DICY=${SEL_NET_DIR}/icy_server.py"
            -P "${SEL_NET_DIR}/ccmixter.cmake")
  set_tests_properties(ccmixter PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "ccmixter_ok")

  # T94 record part (I196, F36): `selecta-cli record` writes the served bytes to disk; STOP (SIGTERM) and the time
  # limit end a stalled stream with a valid file.
  add_test(NAME stream-record
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}"
            "-DMP3=${PROJECT_SOURCE_DIR}/tests/m3/stream/sine.mp3" "-DSLOW=${SEL_NET_DIR}/slow_server.py"
            -P "${SEL_NET_DIR}/stream_record.cmake")
  set_tests_properties(stream-record PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 90 PASS_REGULAR_EXPRESSION "stream_record_ok")

  # I517 phase 1: LIBRARY_SYNC dispatches through the source's own `sync` (call count, args, stats tags, host services).
  # I652c: PLAYLIST_IMPORT appends the existing files of an .m3u in order and counts the missing ones.
  add_executable(playlist_import_test engine/playlist_import_test.c)
  target_include_directories(playlist_import_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(playlist_import_test PRIVATE selecta_library selecta_core)
  add_test(NAME engine-playlist-import COMMAND playlist_import_test)
  set_tests_properties(engine-playlist-import PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "playlist_import_ok")
  # I652d: PLAYLIST_ADD_FOLDER scans a folder recursively and appends the modules to a playlist in path order (addf_runs sentinel).
  add_executable(playlist_add_folder_test engine/playlist_add_folder_test.c)
  target_include_directories(playlist_add_folder_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(playlist_add_folder_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-playlist-add-folder COMMAND playlist_add_folder_test "$<TARGET_FILE:gen_testmod>")
  set_tests_properties(engine-playlist-add-folder PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "playlist_add_folder_ok")
  # I994a: PLAY_CONTEXT (context_plays sentinel): placement, replace, hand-added first, capacity, name, QUEUE_MOVE flag.
  add_executable(play_context_test engine/play_context_test.c)
  target_include_directories(play_context_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(play_context_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-play-context COMMAND play_context_test "$<TARGET_FILE:gen_testmod>")
  set_tests_properties(engine-play-context PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "play_context_ok")
  # I994d1: the pure collection order (sort keys both ways, three-field filter, ties, title fallback).
  add_executable(collection_order_test engine/collection_order_test.c)
  target_include_directories(collection_order_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
  target_link_libraries(collection_order_test PRIVATE selecta_core)
  add_test(NAME engine-collection-order COMMAND collection_order_test)
  set_tests_properties(engine-collection-order PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "collection_order_ok")
  add_executable(play_context_lists_test engine/play_context_lists_test.c)
  target_include_directories(play_context_lists_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(play_context_lists_test PRIVATE selecta_library selecta_registry selecta_core)
  # I994d1: the collection page order through the engine (collection_orders sentinel, shown index, stale serial).
  add_executable(collection_view_test engine/collection_view_test.c)
  target_include_directories(collection_view_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(collection_view_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-collection-view COMMAND collection_view_test)
  set_tests_properties(engine-collection-view PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "collection_view_ok")
  add_test(NAME engine-play-context-lists COMMAND play_context_lists_test)
  set_tests_properties(engine-play-context-lists PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "play_context_lists_ok")
  # I632b: the SONGS_* bulk verbs (songs_runs sentinel): order, one history step, one QUEUE_CHANGED, stale version, playlist add/remove.
  add_executable(songs_bulk_test engine/songs_bulk_test.c)
  target_include_directories(songs_bulk_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(songs_bulk_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-songs-bulk COMMAND songs_bulk_test "$<TARGET_FILE:gen_testmod>")
  set_tests_properties(engine-songs-bulk PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "songs_bulk_ok")
  # I912: VIEW_SOURCE query with more results than the queue capacity must not overrun qmeta_off.
  add_executable(query_meta_cap_test engine/query_meta_cap_test.c)
  target_include_directories(query_meta_cap_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(query_meta_cap_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-query-meta-cap COMMAND query_meta_cap_test)
  set_tests_properties(engine-query-meta-cap PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "query_meta_cap_ok")
  # I691c2: the library must not link the GUI (layering).
  add_test(NAME library-no-gui-link COMMAND ${CMAKE_COMMAND} -DLIBRARY_CMAKE=${PROJECT_SOURCE_DIR}/src/library/CMakeLists.txt -P ${CMAKE_CURRENT_SOURCE_DIR}/library/no_gui_link.cmake)
  set_tests_properties(library-no-gui-link PROPERTIES PASS_REGULAR_EXPRESSION "no_gui_link_ok")
  # I678a: the Your Library item list through the engine entry (library_items_calls sentinel): kinds, counts, mask, paging.
  add_executable(library_items_test engine/library_items_test.c)
  target_include_directories(library_items_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_items_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-library-items COMMAND library_items_test)
  add_executable(source_drill_engine_test engine/source_drill_engine_test.c)
  target_include_directories(source_drill_engine_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(source_drill_engine_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-source-drill COMMAND source_drill_engine_test)
  set_tests_properties(engine-source-drill PROPERTIES PASS_REGULAR_EXPRESSION "source_drill_engine_ok")
  set_tests_properties(engine-library-items PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_items_ok")
  # I782: a draw-path library request in nowait mode returns at once while the sync owns the library; briefs batch the playlist page.
  add_executable(library_nowait_test engine/library_nowait_test.c)
  target_include_directories(library_nowait_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_nowait_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-library-nowait COMMAND library_nowait_test)
  set_tests_properties(engine-library-nowait PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_nowait_ok")
  # I829: the pump never waits on a busy library (search typing, Home chips, Sources/folders post their reads)
  add_executable(library_ui_nowait_test engine/library_ui_nowait_test.c)
  target_include_directories(library_ui_nowait_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_ui_nowait_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-library-ui-nowait COMMAND library_ui_nowait_test)
  set_tests_properties(engine-library-ui-nowait PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_ui_nowait_ok")
  # I906b: FORMATS_SHOW and COMPOSERS_SHOW post read slots, the pump never waits on a busy library
  add_executable(library_ui_browse_reads_test engine/library_ui_browse_reads_test.c)
  target_include_directories(library_ui_browse_reads_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_ui_browse_reads_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-library-browse-reads COMMAND library_ui_browse_reads_test)
  set_tests_properties(engine-library-browse-reads PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_ui_browse_reads_ok")
  add_executable(library_ui_follow_reads_test engine/library_ui_follow_reads_test.c)
  target_include_directories(library_ui_follow_reads_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_ui_follow_reads_test PRIVATE selecta_library selecta_registry selecta_core)
  target_compile_definitions(library_ui_follow_reads_test PRIVATE SELECTA_TEST_MOD="${PROJECT_SOURCE_DIR}/tests/m3/genre/down.mod")
  add_test(NAME engine-library-follow-reads COMMAND library_ui_follow_reads_test "$<TARGET_FILE:gen_testmod>")
  set_tests_properties(engine-library-follow-reads PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_ui_follow_reads_ok")
  add_executable(library_serial_writes_test engine/library_serial_writes_test.c)
  target_include_directories(library_serial_writes_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_serial_writes_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-library-serial-writes COMMAND library_serial_writes_test)
  set_tests_properties(engine-library-serial-writes PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_serial_writes_ok")
  # I906a: the debug UI-wait guard prints and counts a watched thread's blocking library request.
  add_executable(library_ui_guard_test engine/library_ui_guard_test.c)
  target_include_directories(library_ui_guard_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_ui_guard_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-library-ui-guard COMMAND library_ui_guard_test)
  # I915: the guard's one condition holds for an Amiga Debug view (compile-time check in the test).
  add_executable(library_ui_guard_macro_test engine/library_ui_guard_macro_test.c)
  target_include_directories(library_ui_guard_macro_test PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  add_test(NAME engine-library-ui-guard-macro COMMAND library_ui_guard_macro_test)
  set_tests_properties(engine-library-ui-guard-macro PROPERTIES PASS_REGULAR_EXPRESSION "library_ui_guard_macro_ok")
  set_tests_properties(engine-library-ui-guard PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_ui_guard_ok")
  # I406b: the pin ring (32 slots) drops the 33rd queued pin, rows are written with added 0, close drains the ring.
  add_executable(library_pin_test engine/library_pin_test.c)
  target_include_directories(library_pin_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db)
  target_link_libraries(library_pin_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME library-pin COMMAND library_pin_test)
  set_tests_properties(library-pin PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_pin_ok")
  add_executable(majority_format_test engine/majority_format_test.c)
  target_include_directories(majority_format_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library ${PROJECT_SOURCE_DIR}/src/library/db ${PROJECT_SOURCE_DIR}/src/engine)
  target_link_libraries(majority_format_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME engine-majority-format COMMAND majority_format_test)
  set_tests_properties(engine-majority-format PROPERTIES TIMEOUT 60 PASS_REGULAR_EXPRESSION "majority_format_ok")
  # I738: SEL_SOURCE_MAX entries register and SOURCES lists them all; one more registration is refused with a log line.
  add_executable(source_cap_test engine/source_cap_test.c)
  target_include_directories(source_cap_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(source_cap_test PRIVATE selecta_library selecta_core)
  add_test(NAME source-cap COMMAND source_cap_test)
  set_tests_properties(source-cap PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_cap_ok")
  add_executable(library_sync_dispatch_test engine/library_sync_dispatch_test.c)
  target_include_directories(library_sync_dispatch_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(library_sync_dispatch_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-sync-dispatch COMMAND library_sync_dispatch_test)
  set_tests_properties(library-sync-dispatch PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_sync_dispatch_ok")

  # I1090: link order of a folded library and selecta_core is irrelevant in MODULE mode. The wrong order is deliberate here.
  if(SELECTA_PLUGIN_LINK STREQUAL "MODULE")
    add_executable(core_single_copy_test engine/core_single_copy_test.c)
    target_include_directories(core_single_copy_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
    target_link_libraries(core_single_copy_test PRIVATE selecta_library selecta_core ${CMAKE_DL_LIBS})
    add_test(NAME core-single-copy COMMAND core_single_copy_test)
    set_tests_properties(core-single-copy PROPERTIES LABELS m3 TIMEOUT 30 PASS_REGULAR_EXPRESSION "core_single_copy_ok")
  endif()

  # I1019a: SOURCE_SEARCH * fans out over every enabled searchable source in registry order; a newer query drops the stale task's hits.
  add_executable(source_search_all_test engine/source_search_all_test.c)
  target_include_directories(source_search_all_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(source_search_all_test PRIVATE selecta_core selecta_library)
  add_test(NAME source-search-all COMMAND source_search_all_test)
  set_tests_properties(source-search-all PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "source_search_all_ok")
  add_executable(search_keeps_sync_test engine/search_keeps_sync_test.c)
  target_include_directories(search_keeps_sync_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(search_keeps_sync_test PRIVATE selecta_core selecta_library)
  add_test(NAME search-keeps-sync COMMAND search_keeps_sync_test)
  set_tests_properties(search-keeps-sync PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "search_keeps_sync_ok")
  add_executable(search_follows_library_test engine/search_follows_library_test.c)
  target_include_directories(search_follows_library_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(search_follows_library_test PRIVATE selecta_library selecta_core)
  add_test(NAME search-follows-library COMMAND search_follows_library_test)
  set_tests_properties(search-follows-library PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "search_follows_library_ok")
  add_executable(search_publish_never_empty_test engine/search_publish_never_empty_test.c)
  target_include_directories(search_publish_never_empty_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(search_publish_never_empty_test PRIVATE selecta_library selecta_core)
  add_test(NAME search-publish-never-empty COMMAND search_publish_never_empty_test)
  set_tests_properties(search-publish-never-empty PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "search_publish_never_empty_ok")

  # I1019b: the Search view lists a From section per source after Artists; Enter on a From row plays its source:path key.
  add_executable(source_search_view_test engine/source_search_view_test.c)
  target_include_directories(source_search_view_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(source_search_view_test PRIVATE selecta_core selecta_library)
  add_test(NAME source-search-view COMMAND source_search_view_test)
  set_tests_properties(source-search-view PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "source_search_view_ok")

  # I1043: a search hit keeps its whole format name (the longest table label), not 15 characters.
  add_executable(source_search_format_test engine/source_search_format_test.c)
  target_include_directories(source_search_format_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(source_search_format_test PRIVATE selecta_core selecta_library)
  add_test(NAME source-search-format COMMAND source_search_format_test)
  set_tests_properties(source-search-format PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "source_search_format_ok")

  # I527: LIBRARY_SYNC runs on the library job thread; the pump returns while a blocking sync runs, a second sync and a scan queue.
  add_executable(library_sync_async_test engine/library_sync_async_test.c)
  target_include_directories(library_sync_async_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(library_sync_async_test PRIVATE selecta_library selecta_core)
  add_test(NAME library-sync-async COMMAND library_sync_async_test)
  set_tests_properties(library-sync-async PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "library_sync_async_ok")

  # I517 phase 3: a source that exists only as a plugin (tests/sources/testsrc) reaches SOURCES, LIBRARY_SYNC and the Sources page.
  add_executable(src_plugin_reach_test engine/src_plugin_reach_test.c)
  target_include_directories(src_plugin_reach_test PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
  target_link_libraries(src_plugin_reach_test PRIVATE selecta_library selecta_registry selecta_core)
  add_test(NAME src-plugin-reach
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${PROJECT_BINARY_DIR}/tests/src-plugin-reach"
      "-DGOLDEN=${PROJECT_SOURCE_DIR}/tests/golden" "-DSDL_BUILT=${SELECTA_UI_DESKTOP}" "-DREACH_TEST=$<TARGET_FILE:src_plugin_reach_test>"
      "-DSRC=${PROJECT_SOURCE_DIR}" "-DPLUGINS=${SEL_TESTSRC_PLUGIN_DIR}" -P "${PROJECT_SOURCE_DIR}/tests/dui/src_plugin_reach.cmake")
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tests/src-plugin-reach.tmp")
  set_tests_properties(src-plugin-reach PROPERTIES LABELS m3 TIMEOUT 120 PASS_REGULAR_EXPRESSION "src_plugin_reach_ok"
    ENVIRONMENT "TMPDIR=${PROJECT_BINARY_DIR}/tests/src-plugin-reach.tmp")

  # I527: the stale decision behind the sync-on-launch banner (never synced, fresh, stale, dismissed, off).
  add_executable(source_sync_due_test engine/source_sync_due_test.c)
  target_link_libraries(source_sync_due_test PRIVATE selecta_core)
  add_test(NAME source-sync-due COMMAND source_sync_due_test)
  set_tests_properties(source-sync-due PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "source_sync_due_ok")

  # I1082b: command string arguments are copied into the queue slot on send (64 lines through a 16-entry pool, one pump).
  add_executable(cmd_arena_test engine/cmd_arena_test.c)
  target_link_libraries(cmd_arena_test PRIVATE selecta_core)
  add_test(NAME engine-cmd-arena COMMAND cmd_arena_test)
  set_tests_properties(engine-cmd-arena PROPERTIES LABELS m3 TIMEOUT 60 PASS_REGULAR_EXPRESSION "cmd_arena_ok")

  # I1082c: engine -> UI event ring: 10 000 events from a producer thread, on_event on the UI thread, none lost, strings and tag lists copied.
  add_executable(evt_ring_test engine/evt_ring_test.c)
  target_link_libraries(evt_ring_test PRIVATE selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME engine-evt-ring COMMAND evt_ring_test)
  set_tests_properties(engine-evt-ring PROPERTIES LABELS m3 TIMEOUT 120 PASS_REGULAR_EXPRESSION "evt_ring_ok")

  # I195c: STOP sends racing the pump never lose their net cancel (threads, many iterations).
  add_executable(cancel_race_test engine/cancel_race_test.c)
  target_link_libraries(cancel_race_test PRIVATE selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME net-cancel-race COMMAND cancel_race_test)
  set_tests_properties(net-cancel-race PROPERTIES LABELS m3 TIMEOUT 120 PASS_REGULAR_EXPRESSION "cancel_race_ok")

  # T88 (row I186): RADIO from a seed song, steering, skip exclusion, determinism.
  add_test(NAME radio
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre"
            "-DSCRIPT=${PROJECT_SOURCE_DIR}/tests/m3/radio.sel" "-DSQLITE3=${SEL_SQLITE3}"
            -P "${SEL_NET_DIR}/radio.cmake")
  set_tests_properties(radio PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "radio_ok")

  # T79b (row I306): LIKE and unlike queue pending_push kind like rows, flushed through /api/hip/vote.
  add_test(NAME like-devilbox
    COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DWORK=${SEL_NET_WORK}" "-DM3=${CMAKE_CURRENT_SOURCE_DIR}/m3"
            "-DSONGS=${PROJECT_SOURCE_DIR}/tests/m3/genre" "-DSQLITE3=${SEL_SQLITE3}" -P "${SEL_NET_DIR}/like_devilbox.cmake")
  set_tests_properties(like-devilbox PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 60 PASS_REGULAR_EXPRESSION "like_devilbox_ok")

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
            "-DSONGS=${CMAKE_CURRENT_BINARY_DIR}/songs" "-DSONGS_LONG=${CMAKE_CURRENT_BINARY_DIR}/songs-long"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/m3/scrobble.cmake")
  set_tests_properties(scrobble PROPERTIES LABELS m3 FIXTURES_REQUIRED net-fixture RESOURCE_LOCK net-fixture
    TIMEOUT 180 PASS_REGULAR_EXPRESSION "scrobble_ok")
endif()

# M3d desktop UI: red tests, ratchet and dependency check (DUI-P0-1, DUI-P0-2).
if(NOT SEL_AMIGA)
  # Generated modules the shell-driven dui-* tests run on (custom command outputs are only visible in this directory).
  add_custom_target(dui-songs ALL DEPENDS "${SEL_TESTMOD}" "${SEL_SHORTMOD}" "${SEL_LOOPMOD}")
endif()
add_subdirectory(dui)
# I847a: the perf fixture generator and the bench command, run through selecta-cli at 5000 songs (the 600k run is manual, see the design doc section 4).
add_test(NAME library-gen-bench COMMAND "${CMAKE_COMMAND}" "-DCLI=${SEL_CLI}" "-DDIR=${CMAKE_CURRENT_BINARY_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/library_gen_bench.cmake")
set_tests_properties(library-gen-bench PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "library_gen_bench_ok")

sel_isolate_config()

# I427c: the installed SDK builds examples/plugin-sine out of tree and selecta-cli loads and renders it (sentinel inside).
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  add_test(NAME sdk-example
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/sdk/sdk_example.sh" $<TARGET_FILE:selecta-cli> "${CMAKE_COMMAND}"
            "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/sdk-example")
  set_tests_properties(sdk-example PROPERTIES LABELS sdk TIMEOUT 300 PASS_REGULAR_EXPRESSION "sdk_example_ok")
endif()

# I427f: built-ins are modules (SELECTA_PLUGIN_LINK=MODULE): no sel_plugin_* symbol in the binary, modules render, a
# missing module is reported. Own label so dui-ratchet does not count it.
if(SELECTA_PLUGIN_LINK STREQUAL "MODULE" AND TARGET selecta)
  find_program(SEL_NM nm)
  if(SEL_NM)
    add_test(NAME builtin-modules
      COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DNM=${SEL_NM}" "-DSONG=${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/gme_test.nsf"
        "-DMODDIR=${SEL_BUILTIN_MODULE_DIR}" "-DWORK=${CMAKE_BINARY_DIR}/tests/builtin-modules"
        -P "${PROJECT_SOURCE_DIR}/tests/dui/builtin_modules.cmake")
    set_tests_properties(builtin-modules PROPERTIES LABELS builtin-modules TIMEOUT 180)
  endif()
endif()

# I427d: the macOS bundle starts with its modules from Contents/PlugIns (no env override), GPL modules are not in it,
# and the GPL-3 SNDH archive lists COPYING, the module and the sc68 source. Own label, not counted by dui-ratchet.
if(APPLE AND SELECTA_PLUGIN_LINK STREQUAL "MODULE" AND TARGET selecta)
  set(_bp_gpl "")
  if(SELECTA_GPL_PLUGINS AND TARGET sel_module_sndh)
    set(_bp_gpl "-DGPL_MODULE=$<TARGET_FILE:sel_module_sndh>" "-DGPL_SOURCE=${PROJECT_SOURCE_DIR}/third_party/sc68"
      "-DGPL_ARCHIVE=${CMAKE_BINARY_DIR}/tests/bundle-plugins/selecta-sndh-gpl3.tar.gz")
  endif()
  # The expected bundle is the list the bundle itself is built from (apps/selecta/CMakeLists.txt), never a hand list.
  get_property(_bp_bundled GLOBAL PROPERTY SEL_BUNDLED_MODULES)
  get_property(_bp_left_out GLOBAL PROPERTY SEL_LEFT_OUT_MODULES)
  set(_bp_want "")
  foreach(_m IN LISTS _bp_bundled)
    list(APPEND _bp_want "$<TARGET_FILE_NAME:${_m}>")
  endforeach()
  set(_bp_absent "")
  foreach(_m IN LISTS _bp_left_out)
    list(APPEND _bp_absent "$<TARGET_FILE_NAME:${_m}>")
  endforeach()
  add_test(NAME bundle-plugins
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWANT=${_bp_want}" "-DABSENT=${_bp_absent}"
      "-DRELEASE=${SELECTA_RELEASE_MIT_ONLY}" "-DGPL_BUILT=${SELECTA_GPL_PLUGINS}" "-DSONG=${CMAKE_CURRENT_SOURCE_DIR}/m3/fixtures/gme_test.nsf"
      "-DWORK=${CMAKE_BINARY_DIR}/tests/bundle-plugins" "-DGPL_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/gpl_archive.cmake" ${_bp_gpl}
      -P "${PROJECT_SOURCE_DIR}/tests/dui/bundle_plugins.cmake")
  set_tests_properties(bundle-plugins PROPERTIES LABELS builtin-modules TIMEOUT 180 PASS_REGULAR_EXPRESSION "bundle_plugins_ok")
  # I1129: the bundle copies match the build output (a stale core let red proofs pass).
  set(_bf_mods "")
  foreach(_m IN LISTS _bp_bundled)
    list(APPEND _bf_mods "$<TARGET_FILE:${_m}>")
  endforeach()
  add_test(NAME bundle-fresh
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DCORE=$<TARGET_FILE:selecta_core>" "-DMODULES=${_bf_mods}"
      -P "${PROJECT_SOURCE_DIR}/tests/dui/bundle_fresh.cmake")
  set_tests_properties(bundle-fresh PROPERTIES LABELS builtin-modules TIMEOUT 60 PASS_REGULAR_EXPRESSION "bundle_fresh_ok")
endif()

# I710x: Gameboy Tracker songs (.mgb) play through the render entry point (bbmp_mgb module, GPL-3.0 engine, GPL-plugin gate). Songs are fetched by
# checksum (tests/m3/fixtures/bbmp_mgb.SOURCE), exit 77 = skip. A file that is not a Gameboy Tracker song must not be claimed.
if(SELECTA_GPL_PLUGINS AND NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/mgb-decode)
  add_test(NAME mgb-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/mgb_decode.sh $<TARGET_FILE:selecta-cli>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/mgb-decode)
  set_tests_properties(mgb-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "mgb_decode_ok" SKIP_RETURN_CODE 77
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
endif()

# I710w: SGC and GBR songs play through the render entry point (bbmp_nez module, libnezplug, no GPL gate). Four songs fetched by checksum
# (tests/m3/fixtures/bbmp_nez.SOURCE), exit 77 = skip. A file that is not an SGC/GBR song must not be claimed.
if(NOT SEL_AMIGA AND NOT EMSCRIPTEN AND NOT WIN32)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/nez-decode)
  add_test(NAME nez-decode
    COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/m3/nez_decode.sh $<TARGET_FILE:selecta-cli>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/m3/nez-decode)
  set_tests_properties(nez-decode PROPERTIES LABELS m3 PASS_REGULAR_EXPRESSION "nez_decode_ok" SKIP_RETURN_CODE 77
    ENVIRONMENT "SELECTA_PLUGIN_PATH=${CMAKE_BINARY_DIR}/modules")
endif()
