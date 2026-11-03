# dui-search-all (I1019e): the desktop Search page with NO source chip picked sends the fan-out line (SOURCE_SEARCH *) and draws one
# "From <name> (n)" section per searchable source after the library groups. One reachability test through the real shell, the
# scripted --input feeder (the typed query goes through the field and the debounce), against the recorded www.modules.pl fixture.
# -DSELECTA -DSONGS -DWORK -DNETWORK=<net fixture work dir>
#  1. no chip click: `search_section` lines for From Modules.pl with rows == the fixture's hits (3, the sentinel that the fan-out ran
#     and its answer reached the shared row loop), and the query reached the library too (`search groups` line present);
#  1b. (I1024a) the same fan-out draws `From Internet Archive (8)`: archiveorg answers from the local advancedsearch fixture (11 hits,
#     8 get a metadata request = the plugin's cap), the sentinel that its search hook is registered and ran;
#  1c. (I1024d) the same fan-out draws `From Radio Browser (2)`: radiobrowser answers from the local stations fixture (3 stations, the AAC one dropped).
#  1d. (I1024e) and `From ccMixter (2)`: ccmixter answers from the local /api/query fixture (4 uploads, closed licence and WAV-only dropped).
#  1e. (I1041) a 24 px gap above the second From heading (search_section_y lines).
#  2. the From rows are song rows of the shared loop: `searchrow` lines carry their modulespl: keys.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(READ "${NETWORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "www.modules.pl=${fx},radio.modules.pl=${fx},fixture.test=${fx},ia.search.test=${fx},rb.search.test=${fx},cc.search.test=${fx},jam.search.test=${fx}")
set(ENV{SELECTA_ARCHIVEORG_BASE} "http://ia.search.test")
set(ENV{SELECTA_RADIO_BROWSER} "http://rb.search.test") # I1024d
set(ENV{SELECTA_CCMIXTER_BASE} "http://cc.search.test") # I1024e
set(ENV{SELECTA_JAMENDO_API} "http://jam.search.test") # I1024b
set(ENV{SELECTA_JAMENDO_CLIENT_ID} "searchid1")
set(ENV{SELECTA_MODULESPL_SITE} "http://www.modules.pl")
unset(ENV{SELECTA_MIRROR})
set(ENV{SELECTA_MIRRORS_MODULESPL} "http://www.modules.pl/modules/")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")

file(REMOVE_RECURSE "${WORK}/cfg")
file(REMOVE "${WORK}/library.db")
file(WRITE "${WORK}/in.txt" "wait 300\nkey c\nkey h\nkey i\nkey p\nkey enter\n")
foreach(i RANGE 1 40)
  file(APPEND "${WORK}/in.txt" "wait 300\n")
endforeach()
# I1024e: four From sections (8 + 2 + 3 + 2 rows and headings) do not fit 800 px; the frame is tall enough to draw them all.
dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view search --canvas 1280x1400 --clock fixed --stats
  --input "${WORK}/in.txt")
string(REGEX MATCHALL "search_section [0-9]+ rows=[0-9]+ [^\n]*" sections "${o}")
message(STATUS "sections: ${sections}")
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=([0-9]+) From Modules.pl \\(([0-9]+)\\)\n")
  message(FATAL_ERROR "no 'From Modules.pl' section in the dump (no chip picked):\n${o}")
endif()
set(rows "${CMAKE_MATCH_1}")
set(count "${CMAKE_MATCH_2}")
if(NOT rows EQUAL 3 OR NOT count EQUAL 3)
  message(FATAL_ERROR "From Modules.pl: expected 3 rows and (3), got rows=${rows} (${count}):\n${o}")
endif()
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=8 From Internet Archive \\(8\\)\n")
  message(FATAL_ERROR "I1024a: no 'From Internet Archive' section with 8 rows:\n${o}")
endif()
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=2 From Radio Browser \\(2\\)\n")
  message(FATAL_ERROR "I1024d: no 'From Radio Browser' section with 2 rows:\n${o}")
endif()
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=2 From ccMixter \\(2\\)\n")
  message(FATAL_ERROR "I1024e: no 'From ccMixter' section with 2 rows:\n${o}")
endif()
if(NOT "${o}" MATCHES "search_section [0-9]+ rows=2 From Jamendo \\(2\\)\n")
  message(FATAL_ERROR "I1024b: no 'From Jamendo' section with 2 rows:\n${o}")
endif()
# I1041: a gap above every section heading after the first. From section 0 to 1 the heading top moves by its heading (32),
# its rows (32 each) and the 24 px gap; before the fix the next heading sat right under the last row (gap 0).
if(NOT "${o}" MATCHES "search_section 0 rows=([0-9]+) [^\n]*\nsearch_section_y 0 (-?[0-9]+)\n")
  message(FATAL_ERROR "I1041: no search_section_y 0 line:\n${o}")
endif()
set(r0 "${CMAKE_MATCH_1}")
set(y0 "${CMAKE_MATCH_2}")
if(NOT "${o}" MATCHES "search_section_y 1 (-?[0-9]+)\n")
  message(FATAL_ERROR "I1041: no search_section_y 1 line:\n${o}")
endif()
math(EXPR gap "${CMAKE_MATCH_1} - ${y0} - 32 * (1 + ${r0})")
if(NOT gap EQUAL 24)
  message(FATAL_ERROR "I1041: the gap above the second From heading is ${gap} px, want 24:\n${o}")
endif()
if(NOT "${o}" MATCHES "search groups songs=")
  message(FATAL_ERROR "no library groups line in:\n${o}")
endif()
if(NOT "${o}" MATCHES "searchrow [0-9]+ [^\n]*key=modulespl:")
  message(FATAL_ERROR "the From rows are not song rows of the shared loop (no searchrow with a modulespl: key):\n${o}")
endif()

# ---- I1030: a library that also matches the query (Johan's real library) fills the Songs, Albums and Artists groups (16 rows each),
# so the From sections start past row 51 and Radio Browser, the last bucket, past row 64. The page draws the whole grouped result,
# scrolled in pixels; a view window shorter than the search text hid the last source's section (VIEW_WINDOW 64). The sentinel: the
# radiobrowser bucket ran and its 2 rows reached the From Radio Browser section with a full library above it.
find_program(SQLITE3 sqlite3)
if(NOT SQLITE3)
  message(FATAL_ERROR "dui-search-all needs the sqlite3 command line tool to load the matching library")
endif()
set(full_db "${WORK}/full.db")
file(REMOVE "${full_db}")
dui_run(init --db "${full_db}" --ui desktop --backend mem --view home --canvas 900x600 --clock fixed --stats)
set(sql "BEGIN;\n")
foreach(i RANGE 1 40)
  string(APPEND sql "INSERT INTO songs(id, key, source, path, name, artist, album, format, year, genre, duration_ms, added) VALUES(${i}, 'chipfx:${i}', 'local', 'chipfx/${i}.mod', 'Chip Tune ${i}', 'Chip Artist ${i}', 'Chip Album ${i}', 'mod', 1992, 'Chiptune', 120000, ${i});\n")
endforeach()
string(APPEND sql "COMMIT;\n")
file(WRITE "${WORK}/full.sql" "${sql}")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${full_db}" INPUT_FILE "${WORK}/full.sql"
  RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "I1030: loading the matching library failed: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}/cfg")
file(WRITE "${WORK}/in.txt" "wait 300\nkey c\nkey h\nkey i\nkey p\nkey enter\n")
foreach(i RANGE 1 40)
  file(APPEND "${WORK}/in.txt" "wait 300\n")
endforeach()
dui_run(f --db "${full_db}" --ui desktop --backend mem --view search --canvas 1280x800 --clock fixed --stats
  --input "${WORK}/in.txt")
if(NOT "${f}" MATCHES "search groups songs=40 ")
  message(FATAL_ERROR "I1030: the library must answer 40 songs for chip:\n${f}")
endif()
if(NOT "${f}" MATCHES "search_section [0-9]+ rows=2 From Radio Browser \\(2\\)\n")
  string(REGEX MATCHALL "search_section [0-9]+ rows=[0-9]+ [^\n]*" fsec "${f}")
  message(FATAL_ERROR "I1030: with a full library above, no 'From Radio Browser' section with 2 rows (sections: ${fsec}):\n${f}")
endif()

# ---- I1019f: the source chip row. Chips: All sources (0), Library (1), then each enabled searchable source (2 = Internet Archive, 3 = Modules.pl, I1024a).
# The source row and the facet row are drawn always; a source pick never hides the facet row; the filter is not persisted.
function(sa_run outvar input)
  file(REMOVE "${WORK}/library.db")
  file(WRITE "${WORK}/in.txt" "${input}")
  dui_run(o --db "${WORK}/library.db" --ui desktop --backend mem --view search --canvas 1280x800 --clock fixed --stats
    --input "${WORK}/in.txt")
  set(${outvar} "${o}" PARENT_SCOPE)
  set(${outvar}_err "${o_err}" PARENT_SCOPE)
endfunction()

# Both rows' rects are present in the dump, the source row above the facet row (4 facets, >= 3 source chips).
function(sa_rows label dump)
  foreach(k RANGE 0 2)
    if(NOT "${dump}" MATCHES "online_chip ${k} (-?[0-9]+) (-?[0-9]+) ([1-9][0-9]*) ([1-9][0-9]*)\n")
      message(FATAL_ERROR "${label}: no source chip ${k} rect:\n${dump}")
    endif()
    set(sy "${CMAKE_MATCH_2}")
  endforeach()
  foreach(k RANGE 0 3)
    if(NOT "${dump}" MATCHES "search_facet_chip ${k} (-?[0-9]+) (-?[0-9]+) ([1-9][0-9]*) ([1-9][0-9]*)\n")
      message(FATAL_ERROR "${label}: no facet chip ${k} rect (the facet row must always be drawn):\n${dump}")
    endif()
    set(fy "${CMAKE_MATCH_2}")
  endforeach()
  if(NOT "${dump}" MATCHES "search_facet_drawn 4\n")
    message(FATAL_ERROR "${label}: the last frame must paint all 4 facet chips (search_facet_drawn 4):\n${dump}")
  endif()
  if(NOT sy LESS fy)
    message(FATAL_ERROR "${label}: the source row (y=${sy}) must sit above the facet row (y=${fy})")
  endif()
endfunction()

sa_run(s0 "wait 300\n")
sa_rows("start" "${s0}")
if(NOT "${s0}" MATCHES "online scope=- ")
  message(FATAL_ERROR "start: the page must open on All sources (scope=-):\n${s0}")
endif()
if(NOT "${s0}" MATCHES "online_chip 5 (-?[0-9]+) (-?[0-9]+) ([0-9]+) ([0-9]+)\n")
  message(FATAL_ERROR "no Modules.pl chip (online_chip 5):\n${s0}")
endif()
math(EXPR cx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR cy "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
set(click "wait 300\nmove ${cx} ${cy}\nclick ${cx} ${cy}\nwait 50\n")
set(typed "key c\nkey h\nkey i\nkey p\nkey enter\nwait 300\nwait 300\nwait 300\n")

# Picked: the scope is Modules.pl, only its rows, and both rows stay.
sa_run(s1 "${click}${typed}")
sa_rows("picked Modules.pl" "${s1}")
if(NOT "${s1}" MATCHES "online scope=modulespl searches=[1-9][0-9]* rows=3 ")
  message(FATAL_ERROR "picked: expected scope=modulespl rows=3:\n${s1}")
endif()
if("${s1}" MATCHES "search_section [0-9]+ rows=[0-9]+ From ")
  message(FATAL_ERROR "picked: one source must not draw From sections:\n${s1}")
endif()

# Picked twice: back on All sources, the rows stay, the fan-out answers again.
sa_run(s2 "${click}${click}${typed}")
sa_rows("unpicked Modules.pl" "${s2}")
if(NOT "${s2}" MATCHES "online scope=- ")
  message(FATAL_ERROR "a second click on the selected chip must return to All sources (scope=-):\n${s2}")
endif()
if(NOT "${s2}" MATCHES "search_section [0-9]+ rows=3 From Modules.pl \\(3\\)\n")
  message(FATAL_ERROR "unpicked: the From Modules.pl section must be back:\n${s2}")
endif()

# A stale pref from older builds is ignored: the page opens on All sources.
file(MAKE_DIRECTORY "${WORK}/cfg")
file(WRITE "${WORK}/cfg/prefs.conf" "library.sync_stale_days=0\nsearch.last_source=modulespl\n")
sa_run(s3 "wait 300\n${typed}")
file(REMOVE_RECURSE "${WORK}/cfg")
if(NOT "${s3}" MATCHES "online scope=- " OR NOT "${s3}" MATCHES "search_section [0-9]+ rows=3 From Modules.pl")
  message(FATAL_ERROR "a stale search.last_source must open on All sources with the From section:\n${s3}")
endif()

# ---- I1037: no Jamendo client id (the default). The fan-out leaves Jamendo out: no From Jamendo section (it was a failed one), no
# search request to it, no "no client id" line on stderr; the configured sources still draw their sections. The sentinel is the
# other sections: the fan-out ran, only the unconfigured source was skipped. The earlier runs (client id set) keep From Jamendo (2).
unset(ENV{SELECTA_JAMENDO_CLIENT_ID})
sa_run(nj "wait 300\n${typed}")
if(NOT "${nj}" MATCHES "search_section [0-9]+ rows=3 From Modules.pl \\(3\\)\n" OR
   NOT "${nj}" MATCHES "search_section [0-9]+ rows=8 From Internet Archive \\(8\\)\n")
  message(FATAL_ERROR "I1037: without a Jamendo client id the other From sections must stay:\n${nj}")
endif()
if("${nj}" MATCHES "From Jamendo")
  message(FATAL_ERROR "I1037: no client id: a From Jamendo section is drawn (the source must be left out of the fan-out):\n${nj}")
endif()
if("${nj_err}" MATCHES "jamendo: no client id")
  message(FATAL_ERROR "I1037: no client id: the fan-out printed the no-client-id line on stderr:\n${nj}")
endif()
