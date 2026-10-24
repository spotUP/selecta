# dui-grid-coverflow (DUI-P4-5): the Library grid and 2D cover flow views. -DSELECTA -DSONGS -DWORK -DGOLDEN
# 1. Golden hashes for the grid and for the cover flow (update only with SELECTA_GOLDEN_UPDATE=1).
# 2. Sentinels from the library dump: "grid draws N tiles T cols C" and "flow draws N centre I" count the new draw code.
#    The grid run draws the grid (5 tiles) and never the cover flow; the cover flow run the reverse.
# 3. Right in cover flow moves the centred album by exactly 1 (0 -> 1), changes the frame, and clamps at the last album;
#    Left clamps at the first. Right in the grid moves the cursor (the frame changes), Down moves it by one row.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-grid-coverflow needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(queue "")
foreach(p 1.mod 2.mod 3.xm 4.it 5.sid)
  list(APPEND queue --exec "QUEUE_ADD \"fixture/${p}\"")
endforeach()

# Runs the desktop with VIEW_STYLE ${style} and the key script ${script}; sets <tag>_out and <tag>_hash.
function(flow_run tag style script)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/${tag}.txt" "${script}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 1280x800 --clock fixed --stats ${queue}
    --exec "VIEW_SOURCE queue" --exec "VIEW_WINDOW 8" --exec "VIEW_STYLE ${style}" --input "${WORK}/${tag}.txt")
  set(out "${o}\n${o_err}")
  dui_stat(hash "${o}" desktop canvas_hash)
  set(${tag}_out "${out}" PARENT_SCOPE)
  set(${tag}_hash "${hash}" PARENT_SCOPE)
endfunction()

function(golden name hash)
  set(gfile "${GOLDEN}/desktop/${name}-rgba32-1280x800@1.hash")
  if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
    file(WRITE "${gfile}" "${hash}\n")
  endif()
  if(NOT EXISTS "${gfile}")
    message(FATAL_ERROR "missing golden ${gfile} (hash ${hash}); create it with SELECTA_GOLDEN_UPDATE=1")
  endif()
  file(STRINGS "${gfile}" want LIMIT_COUNT 1)
  if(NOT hash STREQUAL want)
    message(FATAL_ERROR "${name} hash ${hash} differs from golden ${want} (${gfile})")
  endif()
endfunction()

function(expect out pattern what)
  if(NOT out MATCHES "${pattern}")
    message(FATAL_ERROR "${what}: no match for '${pattern}' in:\n${out}")
  endif()
endfunction()

# 1 + 2: the grid.
flow_run(grid grid "wait 600\n")
golden(grid-view "${grid_hash}")
expect("${grid_out}" "grid draws [1-9][0-9]* tiles 5 cols [1-9]" "grid sentinel")
expect("${grid_out}" "flow draws 0 centre 0\n" "the grid run must not draw the cover flow")

# 1 + 2: the cover flow, centre on album 0.
flow_run(flow0 coverflow "wait 600\n")
golden(coverflow "${flow0_hash}")
expect("${flow0_out}" "flow draws [1-9][0-9]* centre 0\n" "cover flow sentinel")
expect("${flow0_out}" "grid draws 0 tiles 0 cols 0\n" "the cover flow run must not draw the grid")

# 3: Right moves the centre by exactly 1; the glide has ended after the wait, so the frame is deterministic.
flow_run(flow1 coverflow "key right\nwait 600\n")
expect("${flow1_out}" "flow draws [1-9][0-9]* centre 1\n" "Right in cover flow moves the centre by 1")
if(flow1_hash STREQUAL flow0_hash)
  message(FATAL_ERROR "Right in cover flow did not change the frame")
endif()
flow_run(flow1b coverflow "key right\nwait 600\n")
if(NOT flow1b_hash STREQUAL flow1_hash)
  message(FATAL_ERROR "the cover flow frame after Right is not deterministic")
endif()
flow_run(flow2 coverflow "key right\nkey right\nwait 600\n")
expect("${flow2_out}" "centre 2\n" "two Right keys")
flow_run(flowend coverflow "key right\nkey right\nkey right\nkey right\nkey right\nkey right\nwait 600\n")
expect("${flowend_out}" "centre 4\n" "Right clamps at the last album")
flow_run(flowback coverflow "key right\nkey left\nwait 600\n")
expect("${flowback_out}" "centre 0\n" "Left returns to the first album")
if(NOT flowback_hash STREQUAL flow0_hash)
  message(FATAL_ERROR "Right then Left must restore the first cover flow frame")
endif()
flow_run(flowleft coverflow "key left\nwait 600\n")
expect("${flowleft_out}" "centre 0\n" "Left clamps at the first album")

# The grid: Right and Down move the selection.
flow_run(gridr grid "key right\nwait 600\n")
if(gridr_hash STREQUAL grid_hash)
  message(FATAL_ERROR "Right in the grid did not move the selection")
endif()
flow_run(gridd grid "key down\nwait 600\n")
if(gridd_hash STREQUAL grid_hash OR gridd_hash STREQUAL gridr_hash)
  message(FATAL_ERROR "Down in the grid must move the selection and differ from Right")
endif()
