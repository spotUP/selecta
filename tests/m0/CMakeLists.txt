# M0 tests that need their own fixtures (shims, scripts). SEL_CLI points at
# the selecta-cli binary of this build.

# no-alloc: render path allocates nothing after the freeze marker. The
# shim uses dyld interposing, so the test exists on macOS hosts only.
if(APPLE)
  add_library(count_malloc SHARED count_malloc.c)
  target_compile_options(count_malloc PRIVATE -Wno-pedantic)
  add_test(NAME no-alloc
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/no_alloc.sh"
      $<TARGET_FILE:selecta-cli> $<TARGET_FILE:count_malloc>)
  # T17: the same window around the render loop of the real engine and wav output.
  add_test(NAME no-alloc-render
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/no_alloc.sh"
      $<TARGET_FILE:selecta-cli> $<TARGET_FILE:count_malloc>
      "${SEL_TESTMOD}" "${SEL_RENDER_OUT}/no-alloc.wav")
  # T26: the same window around the openmpt RENDER replayer, and around the
  # engine render calls of a queue auto-advance script.
  if(NOT SEL_AMIGA)
    add_test(NAME no-alloc-render-openmpt
      COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/no_alloc.sh"
        $<TARGET_FILE:selecta-cli> $<TARGET_FILE:count_malloc>
        "${SEL_MKMOD}" "${SEL_RENDER_OUT}/no-alloc-openmpt.wav")
  endif()
  add_test(NAME no-alloc-render-queue
    COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/no_alloc.sh"
      $<TARGET_FILE:selecta-cli> $<TARGET_FILE:count_malloc>
      --script "${CMAKE_CURRENT_SOURCE_DIR}/../m1/noalloc_queue.sel")
  set_tests_properties(no-alloc-render-queue PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests/songs")
endif()

sel_isolate_config() # I989b

# I1169: the real selecta text UI on a pseudo terminal; stderr written while the UI owns the screen scrolled the I355
# start screen art under the player UI. Kept here (not in tests/CMakeLists.txt) as a fixture-driven shell test.
if(NOT SEL_AMIGA)
  add_executable(text_ui_stderr_tty_test ../ui/text_ui_stderr_tty_test.c)
  if(NOT APPLE)
    target_link_libraries(text_ui_stderr_tty_test PRIVATE util)
  endif()
  add_test(NAME text-ui-stderr-tty
    COMMAND text_ui_stderr_tty_test $<TARGET_FILE:selecta> "${CMAKE_CURRENT_BINARY_DIR}/stderr_tty")
  set_tests_properties(text-ui-stderr-tty PROPERTIES TIMEOUT 60)
endif()
