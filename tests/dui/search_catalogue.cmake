# dui-search-catalogue (I1053a, I1053b): the All sources search draws a section for each archive (catalogue) source from the synced
# rows of the library DB, and a never-synced catalogue source (0 hits) is hidden (I1206; was a Not synced section with a Sync button, I1053b).
# One reachability test through the real shell: the scripted --input feeder types the query into the Search field (the debounce
# sends SOURCE_SEARCH *), the desktop dump is the oracle. -DSELECTA -DSQLITE3 -DWORK
#  1. seeded DB: modland 3 rows, hippo 2, asma 2, hvsc 1, local 1 carry the word "zorkmid"; sndh and keygenmusic have no rows.
#     `search_section` lines carry the exact hit counts (the sentinel that every catalogue hook ran through sel_catalogue_search
#     and the library FTS scoped to its source id): From Modland (3), From Hippo (2), From ASMA (2), From HVSC (1). The local row
#     draws no From Local files section: the Library section above the fan-out already lists it (I1009r, Johan 2026-11-04).
#  2. the two unsynced sources draw no section and no `search_sync` button (I1206), no search_failed, no auto-download.
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
foreach(spec "Modland:3" "HippoPlayer:2" "ASMA:2" "HVSC:1")
  string(REPLACE ":" ";" pair "${spec}")
  list(GET pair 0 nm)
  list(GET pair 1 cnt)
  if(NOT "${o}" MATCHES "search_section [0-9]+ rows=${cnt} From ${nm} \\(${cnt}\\)\n")
    message(FATAL_ERROR "I1053a: expected 'From ${nm} (${cnt})' from the seeded synced rows:\n${sections}")
  endif()
endforeach()

if("${o}" MATCHES "search_section [0-9]+ rows=[0-9]+ From Local files")
  message(FATAL_ERROR "I1009r: All sources must not repeat the Library rows as a From Local files section:\n${sections}")
endif()

# 2. the unsynced catalogue sources have no hits, so I1206 hides their sections and the Sync buttons under them (the Sources page
# still offers the sync): no From SNDH / From Keygen Music section, no search_sync button, nothing downloaded
foreach(nm "SNDH" "Keygen Music")
  if("${o}" MATCHES "search_section [0-9]+ rows=[0-9]+ From ${nm} ")
    message(FATAL_ERROR "I1206: 'From ${nm} (0)' has no hits and must be hidden:\n${sections}")
  endif()
endforeach()
if("${o}" MATCHES "search_sync ")
  message(FATAL_ERROR "I1206: a hidden Not synced section must not draw a Sync button:\n${o}")
endif()
if("${o}" MATCHES "sync_ui active=1")
  message(FATAL_ERROR "I1053b: a sync started without a click (no auto-download):\n${o}")
endif()
if("${o}" MATCHES "search_failed")
  message(FATAL_ERROR "I1053b: an unsynced source must not raise search_failed:\n${o}")
endif()
