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

# I1082e: the accessors of the state-read class answer from the snapshot (same harness; run it in the TSan build dir for the proof).
if(UNIX)
  add_executable(accessors_tsan_test accessors_tsan_test.c)
  target_link_libraries(accessors_tsan_test PRIVATE selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME thread-accessors COMMAND accessors_tsan_test)
  set_tests_properties(thread-accessors PROPERTIES LABELS m3 TIMEOUT 300 PASS_REGULAR_EXPRESSION "accessors_tsan_ok")
endif()

# I1162: a second Selecta opens the library database while another process holds its lock (the text UI beside the desktop app).
if(UNIX)
  add_test(NAME library-second-instance
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/library_second_instance"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/library_second_instance.cmake")
  set_tests_properties(library-second-instance PROPERTIES TIMEOUT 60)
endif()

# I1162c: a database newer than the build is refused with a stated reason, not only "cannot open the library database".
if(UNIX)
  add_test(NAME library-newer-schema
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/library_newer_schema"
            "-DLAUNCHER=$<$<PLATFORM_ID:Darwin>:${PROJECT_BINARY_DIR}/apps/selecta/selecta>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/library_newer_schema.cmake")
  set_tests_properties(library-newer-schema PROPERTIES TIMEOUT 60)
endif()

# I1162b: a second Selecta beside one that owns the control socket runs its own UI with no socket and leaves the first one's socket alone.
if(UNIX)
  add_test(NAME second-instance-socket
    COMMAND "${CMAKE_COMMAND}" "-DSELECTA=$<TARGET_FILE:selecta>" "-DWORK=${CMAKE_CURRENT_BINARY_DIR}/second_instance_socket"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/second_instance_socket.cmake")
  set_tests_properties(second-instance-socket PROPERTIES TIMEOUT 60)
endif()

sel_isolate_config() # I989b

# I1082e G4: pattern_request from the UI thread goes through a mailbox the pump drains (TSan build for the race proof).
if(UNIX)
  add_executable(pattern_tsan_test pattern_tsan_test.c)
  target_link_libraries(pattern_tsan_test PRIVATE selecta_core ${CMAKE_THREAD_LIBS_INIT})
  add_test(NAME thread-pattern COMMAND pattern_tsan_test)
  set_tests_properties(thread-pattern PROPERTIES LABELS m3 TIMEOUT 300 PASS_REGULAR_EXPRESSION "pattern_tsan_ok")
endif()
