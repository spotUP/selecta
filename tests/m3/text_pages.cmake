# I1189 text-ui-pages: the text UI Shelf page (Enter on a Home shelf heading = Show all) and Collection page (Enter on a Home tile, key e =
# artist page of the row under the cursor) over the shared uimodel/pages.h routes. The exit report prints text_pages shelf_opens /
# collection_opens / page_rows / page (the sentinel only the page code moves). Real content: --db filled by --exec LIBRARY_SCAN.
#   -DSELECTA=<exe> -DSQLITE3=<sqlite3> -DSONGS=<dir with .mod files> -DKEYS_<A..D>=<keys files> -DWORK=<dir>
# Exemption: r (@album_page) works only on rows with an art_id; song rows show "No album under the cursor" (case d).
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# the seed library: a scan, then one artist and album on every song so the Home quick tiles and the artist page have content
execute_process(COMMAND "${SELECTA}" --output null --size 100x30 --start-dir "${WORK}" --db "${WORK}/seed.db"
                        --exec "LIBRARY_SCAN ${SONGS}" --keys "${KEYS_A}"
                OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "seed scan exited ${rc}\n${o}\n${e}")
endif()
execute_process(COMMAND "${SQLITE3}" "${WORK}/seed.db" "PRAGMA trusted_schema = 1; UPDATE songs SET artist = 'Zed Artist', album = 'Alpha Album';"
                RESULT_VARIABLE rc ERROR_VARIABLE e)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sqlite3 failed: ${e}")
endif()

function(run tag keys)
  file(COPY_FILE "${WORK}/seed.db" "${WORK}/${tag}.db")
  execute_process(
    COMMAND "${SELECTA}" --output null --size 100x30 --start-dir "${WORK}" --db "${WORK}/${tag}.db"
            --keys "${keys}" --screen-dump "${WORK}/${tag}.txt"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "run ${tag} exited ${rc}\n${out}\n${err}")
  endif()
  set(out_${tag} "${out}" PARENT_SCOPE)
  file(READ "${WORK}/${tag}.txt" scr)
  set(scr_${tag} "${scr}" PARENT_SCOPE)
endfunction()

function(expect tag text what)
  string(FIND "${${text}}" "${what}" at)
  if(at LESS 0)
    message(FATAL_ERROR "${tag}: missing '${what}' in:\n${${text}}")
  endif()
endfunction()

# a: Enter on the New This Week heading opens its Show all page: the 4 scanned songs.
run(a "${KEYS_A}")
expect(a out_a "text_pages shelf_opens=1 collection_opens=0 page_rows=4 page=1")
expect(a scr_a "New This Week  Show all  4 rows")
expect(a scr_a "Ambient Drift  - Zed Artist")
expect(a scr_a "Techno Pulse  - Zed Artist")

# b: Enter on the Liked Songs tile opens a Collection page.
run(b "${KEYS_B}")
expect(b out_b "shelf_opens=0 collection_opens=1")
expect(b out_b "page=2")

# c: e on a song tile opens the artist page; Enter on its Albums line opens the album; Escape walks back to the artist page.
run(c "${KEYS_C}")
expect(c out_c "shelf_opens=0 collection_opens=3 page_rows=5")
expect(c out_c "page=2")
expect(c scr_c "artist  Zed Artist")
expect(c scr_c "Albums: Alpha Album")

# d: r on a song tile: no album id, no page.
run(d "${KEYS_D}")
expect(d out_d "collection_opens=0")
expect(d scr_d "No album under the cursor")
