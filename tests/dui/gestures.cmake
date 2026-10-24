# dui-gestures (DUI-P3-4): touchpad gestures in the desktop UI, through the real shell and the scripted --input feeder.
#   -DSELECTA -DSONGS -DWORK
# The grid view is the Home view (tiles of the queue entries); there is no separate grid yet. Tile size steps are
# 96, 120, 144 (default), 176, 208.
# 1. `pinch 1.5` in Home moves the tile size one step up (144 -> 176); `pinch 0.5` moves it down; both clamp at the ends.
# 2. open_tile is 0 on Home and 1 in the detail of tile 0. `swipe right` after opening a tile returns to Home; `swipe left` goes forward again. The gestures stat counts
#    pinch and swipe events (2 for one pinch and one swipe).
# 3. A precise wheel in the songs list scrolls by pixels: `wheel 0 -1.5` moves 24 logical pixels, not a row multiple (32).
# Every stat is a sentinel that the handler ran: gestures, tile_size and scroll_px move only in desk_gesture.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
configure_file("${SONGS}/selecta-test.mod" "${WORK}/a.mod" COPYONLY)

function(expect out key want)
  dui_stat(got "${out}" desktop ${key})
  if(NOT got EQUAL want)
    message(FATAL_ERROR "${key}=${got}, expected ${want} in:\n${out}")
  endif()
endfunction()

function(gesture_run outvar view count input)
  file(REMOVE_RECURSE "${WORK}/cfg")
  file(WRITE "${WORK}/in.txt" "${input}")
  set(adds "")
  foreach(i RANGE 1 ${count})
    list(APPEND adds --exec "QUEUE_ADD ${WORK}/a.mod")
  endforeach()
  dui_run(o --ui desktop --backend mem --view ${view} --canvas 1280x800 --clock fixed --stats ${adds}
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

# Baseline.
gesture_run(base home 3 "move 5 5\n")
expect("${base}" tile_size 144)
expect("${base}" gestures 0)

# 1. Pinch steps and clamps.
gesture_run(up home 3 "pinch 1.5\n")
expect("${up}" tile_size 176)
gesture_run(down home 3 "pinch 0.5\n")
expect("${down}" tile_size 120)
gesture_run(top home 3 "pinch 1.5\npinch 1.5\npinch 1.5\npinch 1.5\n")
expect("${top}" tile_size 208)
gesture_run(bottom home 3 "pinch 0.5\npinch 0.5\npinch 0.5\npinch 0.5\n")
expect("${bottom}" tile_size 96)

# 2. Pinch then open tile 0 then swipe right: back on Home (open_tile 0 shows as view_opens=1 and the home hash).
gesture_run(nav home 3 "pinch 1.5\ntile 0\nswipe right\nwait 1000\n")
expect("${nav}" tile_size 176)
expect("${nav}" view_opens 1)
expect("${nav}" gestures 2)
expect("${nav}" open_tile 0)
gesture_run(fwd home 3 "tile 0\nswipe right\nswipe left\nwait 1000\n")
expect("${fwd}" open_tile 1)
expect("${fwd}" gestures 2)

# 3. Precise wheel in the songs list: 40 rows overflow the main region.
gesture_run(wheel songs 40 "wheel 0 -1.5\n")
expect("${wheel}" scroll_px 24)
dui_stat(px "${wheel}" desktop scroll_px)
math(EXPR rem "${px} % 32")
if(rem EQUAL 0)
  message(FATAL_ERROR "scroll ${px} is a row multiple")
endif()
gesture_run(wheel_up songs 40 "wheel 0 -3\nwheel 0 1\n")
expect("${wheel_up}" scroll_px 32)
