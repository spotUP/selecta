# dui-search-catalogue (I1053a, I1053b): the All sources search draws a section for each archive (catalogue) source from the synced
# rows of the library DB, and a never-synced catalogue source shows "Not synced yet" with a Sync button that starts its sync.
# One reachability test through the real shell: the scripted --input feeder types the query into the Search field (the debounce
# sends SOURCE_SEARCH *), the desktop dump is the oracle. -DSELECTA -DSQLITE3 -DWORK
#  1. seeded DB: modland 3 rows, hippo 2, asma 2, hvsc 1, local 1 carry the word "zorkmid"; sndh and keygenmusic have no rows.
#     `search_section` lines carry the exact hit counts (the sentinel that every catalogue hook ran through sel_catalogue_search
#     and the library FTS scoped to its source id): From Modland (3), From Hippo (2), From ASMA (2), From HVSC (1), From Local (1).
#  2. the two unsynced sources draw "Not synced yet" (no search_failed) and one `search_sync` button each, no auto-download:
#     sources_synced stays 0 and the dump shows sync_ui active=0 before any click.
#  3. clicking the sndh Sync button sends LIBRARY_SYNC for sndh only: `search_sync ... clicks=1` and the sync UI names sndh.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_CACHE_DIR} "${WORK}/cache")
set(db "${WORK}/library.db")
set(common --db "${db}" --ui desktop --backend mem --view search --canvas 1280x2600 --clock fixed --stats)
dui_run(init ${common})
string(CONCAT seed
  "INSERT INTO songs(key, source, path, name, artist, album, format, added) VALUES "
  "('modland:a1.mod','modland','a1.mod','Zorkmid one','Ann','','Protracker',500),"
  "('modland:a2.mod','modland','a2.mod','Zorkmid two','Ann','','Protracker',501),"
  "('modland:a3.xm','modland','a3.xm','Zorkmid three','Bob','','Fasttracker 2',502),"
  "('hippo:h1.mod','hippo','h1.mod','Zorkmid hippo one','Cy','','Protracker',503),"
  "('hippo:h2.mod','hippo','h2.mod','Zorkmid hippo two','Cy','','Protracker',504),"
  "('asma:s1.sap','asma','s1.sap','Zorkmid asma one','Di','','SAP',505),"
  "('asma:s2.sap','asma','s2.sap','Zorkmid asma two','Di','','SAP',506),"
  "('hvsc:v1.sid','hvsc','v1.sid','Zorkmid hvsc one','Ed','','PSID',507),"
  "('local:l1.mod','local','l1.mod','Zorkmid local one','Fay','','Protracker',508)")
execute_process(COMMAND "${SQLITE3}" -cmd "PRAGMA trusted_schema=ON" "${db}" "${seed}" RESULT_VARIABLE rc ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seeding the catalogue rows failed: ${err}")
endif()

file(WRITE "${WORK}/in.txt" "wait 300\nkey z\nkey o\nkey r\nkey k\nkey m\nkey i\nkey d\nkey enter\n")
foreach(i RANGE 1 40)
  file(APPEND "${WORK}/in.txt" "wait 300\n")
endforeach()
dui_run(o ${common} --input "${WORK}/in.txt")
string(REGEX MATCHALL "search_section [0-9]+ rows=[0-9]+ [^\n]*" sections "${o}")
message(STATUS "sections: ${sections}")
foreach(spec "Modland:3" "HippoPlayer:2" "ASMA:2" "HVSC:1" "Local files:1")
  string(REPLACE ":" ";" pair "${spec}")
  list(GET pair 0 nm)
  list(GET pair 1 cnt)
  if(NOT "${o}" MATCHES "search_section [0-9]+ rows=${cnt} From ${nm} \\(${cnt}\\)\n")
    message(FATAL_ERROR "I1053a: expected 'From ${nm} (${cnt})' from the seeded synced rows:\n${sections}")
  endif()
endforeach()

# 2. the unsynced catalogue sources: a Not synced section with a Sync button each, nothing downloaded
foreach(nm "SNDH" "Keygen Music")
  if(NOT "${o}" MATCHES "search_section [0-9]+ rows=2 From ${nm} \\(0\\)\n")
    message(FATAL_ERROR "I1053b: 'From ${nm} (0)' must hold the Not synced row and the Sync row:\n${sections}")
  endif()
endforeach()
foreach(id sndh keygenmusic)
  if(NOT "${o}" MATCHES "search_sync [0-9]+ ${id} (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) clicks=0\n")
    message(FATAL_ERROR "I1053b: no Sync button for the unsynced source ${id}:\n${o}")
  endif()
endforeach()
if("${o}" MATCHES "sync_ui active=1")
  message(FATAL_ERROR "I1053b: a sync started without a click (no auto-download):\n${o}")
endif()
if("${o}" MATCHES "search_failed")
  message(FATAL_ERROR "I1053b: an unsynced source must not raise search_failed:\n${o}")
endif()

# 3. click the sndh Sync button; step 4 presses Enter on the same row
string(REGEX MATCH "search_sync [0-9]+ sndh (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) (-?[0-9]+) clicks=0\n" _b "${o}")
math(EXPR bx "${CMAKE_MATCH_1} + ${CMAKE_MATCH_3} / 2")
math(EXPR by "${CMAKE_MATCH_2} + ${CMAKE_MATCH_4} / 2")
file(READ "${WORK}/in.txt" typed)
file(WRITE "${WORK}/click.txt" "${typed}move ${bx} ${by}\nclick ${bx} ${by}\nwait 600\n")
dui_run(c ${common} --input "${WORK}/click.txt")
if(NOT "${c}" MATCHES "search_sync [0-9]+ sndh [^\n]* clicks=1\n")
  message(FATAL_ERROR "I1053b: the Sync click did not reach the sync branch (clicks=1 expected):\n${c}")
endif()
if(NOT "${c}" MATCHES "sync_ui [^\n]*source=sndh")
  message(FATAL_ERROR "I1053b: the Sync click must start the sndh sync (sync_ui source=sndh):\n${c}")
endif()

# The script's `exec VIEW_CURSOR` waits for the engine's answer (a bare run of `key down` lines races it), so the cursor is on the row
# when Enter arrives. Row 40 is the Keygen Music Sync row of this seed (clicks counts every Sync action, one per run).
file(WRITE "${WORK}/enter.txt" "${typed}key down\nexec VIEW_CURSOR 40\nkey enter\nwait 600\n")
dui_run(en ${common} --input "${WORK}/enter.txt")
if(NOT "${en}" MATCHES "search_sync [0-9]+ keygenmusic [^\n]* clicks=1\n" OR NOT "${en}" MATCHES "sync_ui [^\n]*source=keygenmusic" OR "${en}" MATCHES "row_plays=[1-9]")
  message(FATAL_ERROR "I1053b: Enter on the Keygen Music Sync row must start that sync like the click (clicks=1, no song played):\n${en}")
endif()
