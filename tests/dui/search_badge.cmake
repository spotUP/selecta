# dui-search-badge (I1020): every Search result row draws a small source badge naming where it came from. One reachability test
# through the real shell: the fixture library (local songs) plus the recorded www.modules.pl fan-out, query "chip" typed through
# the field. -DSELECTA -DSONGS -DWORK -DNETWORK=<net fixture work dir>
#  1. All sources: the `search_badges` sentinel equals the rows drawn (library song rows + the From Modules.pl rows) and the
#     `search_badge` lines name Library for the library rows and Modules.pl for the three fixture hits;
#  2. one source picked (Modules.pl chip): the single-source rows each carry a Modules.pl badge, none says Library.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-search-badge needs the sqlite3 command line tool to load the fixture library")
endif()
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx}")
# I1024a: archive.org is a searchable source now; point it at the fixture host (no advancedsearch route there,
# so 404 = search_failed, and no real network is touched)
set(ENV{SELECTA_ARCHIVEORG_BASE} "http://fixture.test")
# I1024d: radiobrowser searches too; same 404 = search_failed
set(ENV{SELECTA_RADIO_BROWSER} "http://fixture.test")
# I1024e: ccmixter searches too; same 404 = search_failed
set(ENV{SELECTA_CCMIXTER_BASE} "http://fixture.test")
# I1024b: jamendo searches too; same 404 = search_failed (no client id set: no request at all)
set(ENV{SELECTA_JAMENDO_API} "http://fixture.test")
unset(ENV{SELECTA_JAMENDO_CLIENT_ID})
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")

set(db "${WORK}/library.db")
function(sb_run outvar input)
  file(REMOVE "${db}")
  dui_run(init --db "${db}" --ui desktop --backend mem --view home --canvas 1280x800 --clock fixed --stats)
  execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" INPUT_FILE "${HERE}/fixture_library.sql"
    RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "loading the fixture failed: ${err}")
  endif()
  # two library songs the query "chip" finds (title), next to the three recorded Modules.pl hits
  execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}"
    "INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(91, 'fixture:91', 'local', 'fixture/91.mod', 'Chip Anthem', 'Fixture', 'Fixture', 'mod', 1990, 'Demo', 1000, 1700000091), (92, 'fixture:92', 'local', 'fixture/92.mod', 'Chip Dream', 'Fixture', 'Fixture', 'mod', 1990, 'Demo', 1000, 1700000092);"
    RESULT_VARIABLE rc ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "adding the library songs failed: ${err}")
  endif()
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${db}" --ui desktop --backend mem --view search --canvas 1280x1000 --clock fixed --stats --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
endfunction()

set(typed "key c\nkey h\nkey i\nkey p\nkey enter\nwait 300\nwait 300\nwait 300\nwait 300\n")

# 1. All sources: Library and Modules.pl rows in one list, one badge each.
sb_run(a "wait 300\n${typed}")
if(NOT "${a}" MATCHES "search_section [0-9]+ rows=3 From Modules.pl \\(3\\)\n")
  message(FATAL_ERROR "no From Modules.pl section with 3 rows:\n${a}")
endif()
if(NOT "${a}" MATCHES "search_badges ([0-9]+)\n")
  message(FATAL_ERROR "no search_badges sentinel in the dump:\n${a}")
endif()
set(n "${CMAKE_MATCH_1}")
string(REGEX MATCHALL "search_badge [0-9]+ [^\n]*" lines "${a}")
list(LENGTH lines nlines)
string(REGEX MATCHALL "search_badge [0-9]+ Modules.pl\n" mp "${a}\n")
list(LENGTH mp nmp)
string(REGEX MATCHALL "search_badge [0-9]+ Library\n" lib "${a}\n")
list(LENGTH lib nlib)
if(NOT n EQUAL nlines OR n LESS 4)
  message(FATAL_ERROR "sentinel ${n} vs ${nlines} badge lines (want >= 4: library rows + 3 hits):\n${a}")
endif()
math(EXPR want "${nlib} + 3")
if(NOT nmp EQUAL 3 OR NOT nlib GREATER 0 OR NOT n EQUAL want)
  message(FATAL_ERROR "badge names: Modules.pl=${nmp} (want 3) Library=${nlib} (want > 0) total=${n}:\n${a}")
endif()

# 2. Modules.pl picked: three rows, three Modules.pl badges, no Library badge.
if(NOT "${a}" MATCHES "online_chip [0-9]+ (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+) scope=modulespl\n")
  message(FATAL_ERROR "no Modules.pl chip (online_chip scope=modulespl):\n${a}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
sb_run(b "wait 300\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 50\n${typed}")
if(NOT "${b}" MATCHES "online scope=modulespl searches=[1-9][0-9]* rows=3 ")
  message(FATAL_ERROR "picked: expected scope=modulespl rows=3:\n${b}")
endif()
if(NOT "${b}" MATCHES "search_badges 3\n" OR "${b}" MATCHES "search_badge [0-9]+ Library")
  message(FATAL_ERROR "picked: want exactly 3 badges, all Modules.pl:\n${b}")
endif()
string(REGEX MATCHALL "search_badge [0-9]+ Modules.pl\n" mp2 "${b}")
list(LENGTH mp2 nmp2)
if(NOT nmp2 EQUAL 3)
  message(FATAL_ERROR "picked: ${nmp2} Modules.pl badge lines, want 3:\n${b}")
endif()
