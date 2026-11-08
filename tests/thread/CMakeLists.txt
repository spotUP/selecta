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

# I1082d: the published snapshot. A UI-side reader walks queue, view and the list fields while the engine thread edits the queue
# and rebuilds views 10 000 times. Plain runs check the strings are intact; a build with -fsanitize=thread (the row's proof:
# cmake -DCMAKE_C_FLAGS=-fsanitize=thread in its own build dir) also reports any read of live engine memory as a race.
if(UNIX)
  add_executable(snapshot_tsan_test snapshot_tsan_test.c)
  target_link_libraries(snapshot_tsan_test PRIVATE selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME thread-snapshot COMMAND snapshot_tsan_test)
  set_tests_properties(thread-snapshot PROPERTIES LABELS m3 TIMEOUT 300 PASS_REGULAR_EXPRESSION "snapshot_tsan_ok")
endif()

sel_isolate_config() # I989b
