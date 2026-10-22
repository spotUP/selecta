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
endif()
