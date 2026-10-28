# dui-home-tile-tint (I600): an idle Home cover has no dark card frame around it. -DSELECTA -DWORK
# Johan, Amiga AGA rig: "the album covers have a phat black outline". Not a shadow (INDEX8 draws none): every shelf tile
# painted an opaque card fill (PANEL, 141418) over the Home header tint, and the first shelf sits inside the tint, so each
# cover sat in a dark box on the lighter gradient. Rendered at 640x256 ydiv 2 INDEX8 (what the aga backend opens):
#  1. the padding band above a cover (the 6 frame rows over its top edge) is mostly NOT the panel colour: the tint shows
#     through (INDEX8 dithers the gradient between palette entries; a band that is 100% panel is the dark box);
#  2. a control strip of the same size 30 rows higher (plain tint, no card ever drew there) passes the same check, so the
#     test cannot pass or fail just because the area is panel coloured.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
include("${HERE}/ppm.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-home-tile-tint needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
set(flat "89504E470D0A1A0A0000000D49484452000000080000000808020000004B6D29DC0000001149444154789C63F870C2062B62185A120004A77D01143E47B20000000049454E44AE426082")
execute_process(COMMAND "${SQLITE3}" "${db}" "PRAGMA trusted_schema = 1; UPDATE artwork SET data = X'${flat}' WHERE kind = 'cover'; INSERT INTO artwork(album_id, kind, data) VALUES(4, 'cover', X'${flat}')"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "flat covers failed: ${err}")
endif()
set(execs "")
foreach(q 1.mod 2.mod 3.xm 4.it)
  list(APPEND execs --exec "QUEUE_ADD \"fixture/${q}\"")
endforeach()
dui_run(shot --db "${db}" --ui desktop --backend mem --view home --canvas 640x256 --ydiv 2 --format index8 --zoom fit --clock fixed --stats ${execs}
  --frame-dump "${WORK}/home.ppm")
if(NOT "${shot}" MATCHES "\ntile_art corner=[0-9a-f]+ centre=[0-9a-f]+ fill=[0-9a-f]+\ntile_art_rect (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+)\n")
  message(FATAL_ERROR "no tile_art_rect line (a Home cover was drawn) in:\n${shot}")
endif()
set(ax ${CMAKE_MATCH_1})
set(ay ${CMAKE_MATCH_2})
ppm_load("${WORK}/home.ppm" hex w)
math(EXPR top "${ay} / 2") # the frame is half the layout height (--ydiv 2)

# Number of 141418 pixels in the 6 rows ending at `last` over 80 columns starting 8 columns inside the cover's left edge.
function(panel_count outvar last)
  set(n 0)
  math(EXPR y0 "${last} - 5")
  math(EXPR x0 "${ax} + 8")
  math(EXPR x1 "${x0} + 79")
  foreach(y RANGE ${y0} ${last})
    foreach(x RANGE ${x0} ${x1})
      math(EXPR off "(${y} * ${w} + ${x}) * 6")
      string(SUBSTRING "${hex}" ${off} 6 h)
      if(h STREQUAL "141418")
        math(EXPR n "${n} + 1")
      endif()
    endforeach()
  endforeach()
  set(${outvar} ${n} PARENT_SCOPE)
endfunction()

math(EXPR band_last "${top} - 1")
math(EXPR ctrl_last "${top} - 31")
panel_count(band ${band_last})
panel_count(ctrl ${ctrl_last})
# 6 x 80 = 480 pixels. The dithered tint is about half panel colour; a solid card is all of it.
if(ctrl GREATER 400)
  message(FATAL_ERROR "control strip rows ${ctrl_last}-5..${ctrl_last} is ${ctrl}/480 panel coloured: the tint does not reach this high, the test setup is wrong")
endif()
if(band GREATER 400)
  message(FATAL_ERROR "the band over the cover top (frame rows ${band_last}-5..${band_last}, from x ${ax}+8) is ${band}/480 panel coloured: the opaque card fill hides the Home tint (dark frame around the cover)")
endif()
