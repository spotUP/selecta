# I173f library-close-cancel: closing the library must not wait out a rating push stuck on a server that accepts and
# never answers. The script RATEs a Modland song against such a server and exits (library close); the wall time of
# that process must stay under CLOSE_BOUND_MS. The rating stays pending in the DB and the next run, with the
# fixture server up, pushes it.
# -DCLI=<selecta-cli> -DWORK=<dir> -DM3=<tests/m3>
set(CLOSE_BOUND_MS 500)
set(ctl "${CMAKE_CURRENT_LIST_DIR}/fixture_ctl.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=stop -DWORK=${WORK} -P "${ctl}")
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=start -DWORK=${WORK} -P "${ctl}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I173f fixture start failed")
endif()
set(last_raw "")
function(fail what)
  execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=stop -DWORK=${WORK} -P "${ctl}")
  message(FATAL_ERROR "I173f ${what}\n${last_raw}")
endfunction()

file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_DEVILBOX_URL} "http://devilbox.uprough.net")
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=${fx},fixture.test=${fx},fast.mirror.test=${fx}")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODLAND} "http://fast.mirror.test/mods/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
set(cfg "${WORK}/cfg")
set(db "${WORK}/lcc.db")

# token from the fixture server
file(WRITE "${WORK}/pw" "fixture-password\n")
execute_process(COMMAND "${CLI}" devilbox login rater --password-stdin --config-dir "${cfg}" INPUT_FILE "${WORK}/pw"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
set(last_raw "${o}${e}")
if(NOT rc EQUAL 0)
  fail("login failed rc=${rc}")
endif()

# the silent server: accepts, never answers; it ends itself after 40 s
execute_process(COMMAND sh -c "python3 -I '${CMAKE_CURRENT_LIST_DIR}/silent_server.py' '${WORK}/silent-port' 40 >/dev/null 2>&1 &")
foreach(i RANGE 50)
  if(EXISTS "${WORK}/silent-port")
    file(READ "${WORK}/silent-port" sp)
    if(NOT sp STREQUAL "")
      break()
    endif()
  endif()
  execute_process(COMMAND sleep 0.1)
endforeach()
if(sp STREQUAL "")
  fail("silent server did not start")
endif()
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=http://127.0.0.1:${sp},fixture.test=${fx},fast.mirror.test=${fx}")

# Wall time of a script run. The baseline (same script without RATE) removes process start and the 300 ms sleep, so
# the difference is what library close spent waiting on the stuck push.
function(timed_run var script)
  execute_process(COMMAND python3 -I -c
    "import subprocess,sys,time\nt=time.monotonic()\nr=subprocess.run(sys.argv[1:],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)\nprint(int((time.monotonic()-t)*1000))\nsys.exit(r.returncode)"
    "${CLI}" script "${script}" --db "${db}" --config-dir "${cfg}"
    OUTPUT_VARIABLE ms RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    fail("script ${script} failed rc=${rc}")
  endif()
  string(STRIP "${ms}" ms)
  set(${var} "${ms}" PARENT_SCOPE)
endfunction()

file(WRITE "${WORK}/idle.sel" "OPEN modland:Protracker/test/play1.mod\n@sleep 300\n")
timed_run(base "${WORK}/idle.sel")
# RATE, give the push time to be in flight, exit (library close).
file(WRITE "${WORK}/stuck.sel" "OPEN modland:Protracker/test/play1.mod\n@timed RATE 3\n@sleep 300\n")
timed_run(stuck "${WORK}/stuck.sel")
math(EXPR close_ms "${stuck} - ${base}")
set(last_raw "stuck=${stuck} ms base=${base} ms")
message(STATUS "I173f library close with a stuck push: ${close_ms} ms (bound ${CLOSE_BOUND_MS})")
if(close_ms GREATER CLOSE_BOUND_MS)
  fail("library close waited on the stuck push: ${close_ms} ms, want <= ${CLOSE_BOUND_MS} ms")
endif()

# the rating is still pending and the next run with the real server up pushes it
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=${fx},fixture.test=${fx},fast.mirror.test=${fx}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "@ratings" OUTPUT_FILE "${WORK}/flush.sel")
execute_process(COMMAND "${CLI}" script "${WORK}/flush.sel" --db "${db}" --config-dir "${cfg}"
  OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
set(last_raw "${o}${e}")
if(NOT rc EQUAL 0 OR NOT last_raw MATCHES "rating_pushes=1" OR NOT last_raw MATCHES "rating_pending=0")
  fail("the cancelled rating was not pushed on the next engine create")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=stop -DWORK=${WORK} -P "${ctl}")
message(STATUS "library_close_cancel_ok")
