# Audio-thread assertion (src/engine/thread.h): Debug builds on Apple and
# Linux only; Release and the Amiga build carry no checks, so nothing to test.
if(UNIX AND NOT CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel)$")
  add_executable(thread_assert_test thread_assert_test.c)
  target_link_libraries(thread_assert_test PRIVATE selecta_core ${CMAKE_THREAD_LIBS_INIT})
  foreach(mode render-off-thread send-on-audio render-on-audio)
    if(mode STREQUAL "render-on-audio")
      set(abort OFF)
    else()
      set(abort ON)
    endif()
    add_test(NAME thread-assert-${mode}
      COMMAND "${CMAKE_COMMAND}" "-DTEST=$<TARGET_FILE:thread_assert_test>" -DMODE=${mode}
              -DEXPECT_ABORT=${abort} -P "${CMAKE_CURRENT_SOURCE_DIR}/thread_assert.cmake")
  endforeach()
endif()
