# dui-search-spinner (I1231, Johan 2026-11-09: "when i search in the main search i expect the current list to be cleared instantly
# and some search spinner showing"). One reachability test through the real shell: the scripted --input feeder types a query into
# the top-bar Search field and presses Enter (desk_search_fire); the desktop dump is the oracle. -DSELECTA -DSQLITE3 -DWORK
#  1. the frame after the submit drew the searching spinner in place of the old list: `search_spinner drawn>=1` with
#     first_frame_after_fire 0 or 1, and cleared>=1 (frames that drew no stale result rows while the query was out);
#  2. the hits land after it: the seeded synced Modland rows show as From Modland (2), and the spinner left (wait=0).
# With the spinner branch in draw_search removed the dump shows drawn=0 and the test fails.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
set(db "${WORK}/library.db")
set(common --db "${db}" --ui desktop --backend mem --view search --canvas 1280x1600 --clock fixed --stats)
dui_run(init ${common})
string(CONCAT seed
  "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES "
  "('modland:a1.mod','modland','a1.mod','Quaxel one','Ann','','Protracker',500),"
  "('modland:a2.mod','modland','a2.mod','Quaxel two','Bob','','Protracker',501)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the catalogue rows failed: ${err}")
endif()

file(WRITE "${WORK}/in.txt" "wait 300\nkey q\nkey u\nkey a\nkey x\nkey e\nkey l\nkey enter\n")
foreach(i RANGE 1 30)
  file(APPEND "${WORK}/in.txt" "wait 300\n")
endforeach()
dui_run(o ${common} --input "${WORK}/in.txt")
if(NOT "${o}" MATCHES "search_spinner drawn=([0-9]+) first_frame_after_fire=(-?[0-9]+) cleared=([0-9]+) wait=([0-9]+)")
  message(FATAL_ERROR "I1231: no search_spinner dump line:\n${o}")
endif()
set(drawn "${CMAKE_MATCH_1}")
set(first "${CMAKE_MATCH_2}")
set(cleared "${CMAKE_MATCH_3}")
set(wait "${CMAKE_MATCH_4}")
message(STATUS "spinner drawn=${drawn} first=${first} cleared=${cleared} wait=${wait}")
if(drawn LESS 1 OR first LESS 0 OR first GREATER 1 OR cleared LESS 1)
  message(FATAL_ERROR "I1231: the list was not cleared with a spinner within one frame of the submit "
    "(drawn=${drawn} first_frame_after_fire=${first} cleared=${cleared})")
endif()
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=2 From Modland \\(2\\)\n")
  string(REGEX MATCHALL "search_section [^\n]*" sections "${o}")
  message(FATAL_ERROR "I1231: the hits did not land after the spinner:\n${sections}")
endif()
if(NOT wait EQUAL 0)
  message(FATAL_ERROR "I1231: the spinner still waits after the hits landed (wait=${wait})")
endif()
message(STATUS "dui_search_spinner_ok")
