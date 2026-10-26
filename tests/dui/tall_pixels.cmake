# dui-tall-pixels (I468, I457; design 2026-10-24-desktop-ui.md Q14: 640x256 is the AGA design target, PAL hires pixels are 1:2).
# -DSELECTA -DWORK -DGOLDEN
# The desktop shell on the mem backend at --canvas 640x256 --ydiv 2 --zoom fit, which is what the aga backend picks for a
# canvas of up to 300 rows. The layout runs in 640x512 square pixels (1024x819 logical at the fit floor 0.625).
#   1. Layout (`--resize 1024x819` dump): the sidebar region exists (I457: the left rail shows) and the play button is
#      square in layout space.
#   2. On screen (--frame-dump PPM, read column by column): the play button's centre column spans columns = 2 * rows (extents walked from its centre to the flat background),
#      so a round button measures width = 2 * rows on a 1:2 display.
#   3. I457: the Statistics strip (library view, fixture library) lies inside the main region and its heading glyphs are
#      all present (missing_glyphs=0).
#   4. The frame hash matches the golden (update only with SELECTA_GOLDEN_UPDATE=1).
# Revert check: with the canvas ydiv forced to 1 the play button covers its full layout width in rows and check 2 fails.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-tall-pixels needs the sqlite3 command line tool to load the fixture library")
endif()
set(db "${WORK}/library.db")
dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "loading the fixture failed: ${err}")
endif()
dui_run(o --db "${db}" --ui desktop --backend mem --view library --canvas 640x256 --ydiv 2 --zoom fit --clock fixed --stats
  --resize 1024x819 --frame-dump "${WORK}/tall.ppm")

# 1. layout
if(NOT o MATCHES "layout width=1024 height=819 id=[a-z-]+ regions=[0-9]+ check=ok")
  message(FATAL_ERROR "no valid 1024x819 layout line:\n${o}")
endif()
if(NOT o MATCHES "region name=sidebar x=0 y=0 w=([0-9]+) h=([0-9]+)" OR NOT CMAKE_MATCH_1 GREATER 0)
  message(FATAL_ERROR "I457: the left rail (sidebar region) must show at 640x256:\n${o}")
endif()
if(NOT o MATCHES "button id=play x=(-?[0-9]+) y=(-?[0-9]+) w=([0-9]+) h=([0-9]+)")
  message(FATAL_ERROR "no play button line:\n${o}")
endif()
set(bx ${CMAKE_MATCH_1})
set(by ${CMAKE_MATCH_2})
set(bw ${CMAKE_MATCH_3})
set(bh ${CMAKE_MATCH_4})
if(NOT bw EQUAL bh)
  message(FATAL_ERROR "the play button must be square in layout space: ${bw}x${bh}")
endif()

# 2. screen rows
file(SIZE "${WORK}/tall.ppm" ppm_size)
math(EXPR ppm_want "15 + 640 * 256 * 3")
if(NOT ppm_size EQUAL ppm_want)
  message(FATAL_ERROR "frame dump size ${ppm_size}, expected ${ppm_want} (P6 640 256 255)")
endif()
function(pixel outvar x y)
  math(EXPR off "15 + (${y} * 640 + ${x}) * 3")
  file(READ "${WORK}/tall.ppm" hex OFFSET ${off} LIMIT 3 HEX)
  set(${outvar} "${hex}" PARENT_SCOPE)
endfunction()
# The layout is 640x512 square pixels at scale 1 (the dump line above carries the same button geometry): x is a column,
# y/2 a screen row.
math(EXPR cx "${bx} + ${bw} / 2")
math(EXPR cy "(${by} + ${bh} / 2) / 2")
math(EXPR ex "${bx} - 3")
math(EXPR ey "${cy}")
pixel(bg ${ex} ${ey})
function(extent outvar dx dy)
  set(n 0)
  set(x ${cx})
  set(y ${cy})
  while(n LESS 64)
    math(EXPR x "${x} + ${dx}")
    math(EXPR y "${y} + ${dy}")
    pixel(c ${x} ${y})
    if(c STREQUAL bg)
      break()
    endif()
    math(EXPR n "${n} + 1")
  endwhile()
  set(${outvar} ${n} PARENT_SCOPE)
endfunction()
extent(el -1 0)
extent(er 1 0)
extent(eu 0 -1)
extent(ed 0 1)
math(EXPR hw "${el} + ${er} + 1")
math(EXPR vh "${eu} + ${ed} + 1")
math(EXPR skew "${hw} - 2 * ${vh}")
if(hw LESS 6 OR skew LESS -2 OR skew GREATER 2)
  message(FATAL_ERROR "the play button must be ${hw} columns wide and about ${hw}/2 rows tall on 1:2 pixels (round when shown), it is ${hw}x${vh}")
endif()

# 3. I457 statistics strip
dui_stat(missing "${o}" desktop missing_glyphs)
if(NOT missing EQUAL 0)
  message(FATAL_ERROR "missing glyphs at 640x256: ${missing}")
endif()
if(NOT o MATCHES "stats rect (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no stats rect in the library dump:\n${o}")
endif()
math(EXPR strip_bottom "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4}")
if(strip_bottom GREATER 512)
  message(FATAL_ERROR "I457: the Statistics strip ends at ${strip_bottom}, below the 512 row layout")
endif()

# 4. golden
dui_stat(hash "${o}" desktop canvas_hash)
set(gfile "${GOLDEN}/desktop/library-rgba32-640x256-ydiv2@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${hash}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${hash}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
if(NOT hash STREQUAL want)
  message(FATAL_ERROR "tall-pixel hash ${hash} differs from golden ${want} (${gfile})")
endif()
