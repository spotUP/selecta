# dui-text (DUI-P2-3): glyph-atlas text through the real shell. -DSELECTA -DSONGS -DWORK -DGOLDEN
# The text sheet (`--ui desktop --view text`) draws the Inter specimen lines at every size token and weight, then each
# queue name ellipsized and wrapped. The names are the fixture library's album titles (tests/dui/fixture_library.sql:
# a very long title and "Ünïcødé Álbum åäö"), so the run covers ellipsis, wrap and non-ASCII.
#  1. missing_glyphs=0 (every code point was in the atlas), glyphs > 0 (the blit ran), canvas hash = golden (hash
#     golden, plan 4.2; update only with SELECTA_GOLDEN_UPDATE=1).
#  2. A name with CJK, which Inter lacks, moves the missing-glyph sentinel: the sentinel can fail.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")

file(STRINGS "${HERE}/fixture_library.sql" album_lines REGEX "^INSERT INTO albums")
set(execs "")
foreach(line IN LISTS album_lines)
  # No regex here: CMake regex does not match non-ASCII bytes inside a bracket class.
  string(FIND "${line}" "VALUES(" at)
  if(NOT at EQUAL -1)
    string(SUBSTRING "${line}" ${at} -1 rest)
    string(FIND "${rest}" "'" q1)
    math(EXPR from "${q1} + 1")
    string(SUBSTRING "${rest}" ${from} -1 rest)
    string(FIND "${rest}" "'" q2)
    string(SUBSTRING "${rest}" 0 ${q2} title)
    list(APPEND execs --exec "QUEUE_ADD \"fixture/${title}.mod\"")
  endif()
endforeach()
list(LENGTH execs exec_n)
if(exec_n LESS 8)
  message(FATAL_ERROR "expected at least 4 fixture albums, found ${exec_n} exec arguments")
endif()

dui_run(out --ui desktop --backend mem --view text --canvas 1280x800 --stats ${execs})
dui_stat(missing "${out}" desktop missing_glyphs)
dui_stat(glyphs "${out}" desktop glyphs)
dui_stat(hash "${out}" desktop canvas_hash)
if(NOT missing EQUAL 0)
  message(FATAL_ERROR "the fixture names drew ${missing} missing glyphs; Inter must cover them")
endif()
if(glyphs LESS 300)
  message(FATAL_ERROR "only ${glyphs} glyphs were blitted; the text path did not run")
endif()
set(gfile "${GOLDEN}/desktop/text-rgba32-1280x800@1.hash")
if("$ENV{SELECTA_GOLDEN_UPDATE}" STREQUAL "1")
  file(WRITE "${gfile}" "${hash}\n")
endif()
if(NOT EXISTS "${gfile}")
  message(FATAL_ERROR "missing golden ${gfile} (hash ${hash}); create it with SELECTA_GOLDEN_UPDATE=1")
endif()
file(STRINGS "${gfile}" want LIMIT_COUNT 1)
if(NOT hash STREQUAL want)
  message(FATAL_ERROR "text sheet hash ${hash} differs from golden ${want} (${gfile})")
endif()

dui_run(cjk --ui desktop --backend mem --view text --canvas 1280x800 --stats --exec "QUEUE_ADD \"fixture/日本語.mod\"")
dui_stat(cjk_missing "${cjk}" desktop missing_glyphs)
if(cjk_missing LESS 1)
  message(FATAL_ERROR "a CJK name must hit the missing-glyph box (missing_glyphs=${cjk_missing})")
endif()
