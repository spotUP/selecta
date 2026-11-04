# dui-output-device (I1039): the now-playing bar's devices button and the picker list, through the real shell and --input feeder.
#   -DSELECTA -DSONGS -DWORK
# The null output lists two devices (Built-in Speakers, Living Room HomePod with the AirPlay mark). The page dumps
# "devices btn x y w h n=<devices> clicks=<button clicks> picks=<list picks> pick=<uid>".
# 1. Without a click: the button exists (n=2) and nothing is counted.
# 2. A click on the button opens the list: clicks=1, picks=0.
# 3. A second click on the AirPlay row (second device row, under the System default row) picks it:
#    picks=1 and pick=null-homepod, the uid sent as PREF_SET output.device (I1042: the engine pref holds it, prefs.conf
#    saves it, and a fresh run on the same config dir starts with that pref).
# 4. A click outside the button counts nothing.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${SONGS}/selecta-test-mk.mod" "${WORK}/song.mod" COPYONLY)

function(dev_run outvar input)
  if(NOT KEEP_CFG)
    file(REMOVE_RECURSE "${WORK}/cfg")
  endif()
  file(WRITE "${WORK}/in.txt" "wait 2\n${input}wait 1\n")
  dui_run(o --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats
    --exec "QUEUE_ADD ${WORK}/song.mod" --exec "PLAY" --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# I1042: the engine-side pref (the "devices pref=" line) after a run.
function(dev_pref_expect out want)
  if(NOT "${out}" MATCHES "devices pref=([^\n]*)")
    message(FATAL_ERROR "no 'devices pref=' line in:\n${out}")
  endif()
  if(NOT "${CMAKE_MATCH_1}" STREQUAL "${want}")
    message(FATAL_ERROR "engine pref output.device is '${CMAKE_MATCH_1}', expected '${want}' (the pick did not reach PREF_SET, or did not survive the restart) in:\n${out}")
  endif()
endfunction()

function(dev_expect out n clicks picks pick)
  if(NOT "${out}" MATCHES "devices btn [0-9]+ [0-9]+ [0-9]+ [0-9]+ n=([0-9]+) clicks=([0-9]+) picks=([0-9]+) pick=([^\n]*)")
    message(FATAL_ERROR "no devices line in:\n${out}")
  endif()
  if(NOT CMAKE_MATCH_1 EQUAL n OR NOT CMAKE_MATCH_2 EQUAL clicks OR NOT CMAKE_MATCH_3 EQUAL picks OR NOT "${CMAKE_MATCH_4}" STREQUAL "${pick}")
    message(FATAL_ERROR "devices n=${CMAKE_MATCH_1} clicks=${CMAKE_MATCH_2} picks=${CMAKE_MATCH_3} pick=${CMAKE_MATCH_4}, expected ${n} ${clicks} ${picks} ${pick} in:\n${out}")
  endif()
endfunction()

dev_run(base "")
if(NOT "${base}" MATCHES "devices btn ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) n=")
  message(FATAL_ERROR "no devices button rectangle in the dump:\n${base}")
endif()
set(bx ${CMAKE_MATCH_1})
set(by ${CMAKE_MATCH_2})
set(bw ${CMAKE_MATCH_3})
set(bh ${CMAKE_MATCH_4})
math(EXPR cx "${bx} + ${bw} / 2")
math(EXPR cy "${by} + ${bh} / 2")
dev_expect("${base}" 2 0 0 "")

# 2. Open the list.
dev_run(opened "click ${cx} ${cy}\n")
dev_expect("${opened}" 2 1 0 "")

# 3. The list opens above the button with System default first, then Built-in Speakers, then the HomePod: three rows
#    of the shared menu row height (24 at scale 1). Click the last row's middle.
math(EXPR ry "${by} - 12") # menu_y = by - 3 rows of 24; the third row spans by-24 .. by, its middle is by-12
dev_run(picked "click ${cx} ${cy}\nclick ${cx} ${ry}\n")
dev_expect("${picked}" 2 1 1 "null-homepod")
# I1042: the pick went through PREF_SET to the engine, and the engine wrote it to prefs.conf.
dev_pref_expect("${picked}" "null-homepod")
file(READ "${WORK}/cfg/prefs.conf" saved)
if(NOT saved MATCHES "output\\.device *= *null-homepod")
  message(FATAL_ERROR "prefs.conf does not hold output.device=null-homepod after the pick:\n${saved}")
endif()

# 3b. A fresh run on the same config dir starts on the picked device: no click in this run, the engine pref is the pick.
set(KEEP_CFG ON)
dev_run(restarted "")
dev_expect("${restarted}" 2 0 0 "")
dev_pref_expect("${restarted}" "null-homepod")
set(KEEP_CFG OFF)

# 4. A click far from the button opens nothing.
math(EXPR fx "${bx} - 300")
dev_run(miss "click ${fx} ${cy}\n")
dev_expect("${miss}" 2 0 0 "")
