# Row I1003a: the online cover scraper, driven through selecta-cli library scan --scrape against local MusicBrainz, Cover Art
# Archive and iTunes fixtures (design 2026-11-09-artwork-scraper-design.md).
#  1. scan three tagged mp3 files with no cover: "Scrape Hit" (MusicBrainz release + Cover Art Archive front image),
#     "Scrape Itunes" (MusicBrainz empty, iTunes album hit, 600x600 image) and "Scrape Miss" (nothing anywhere);
#  2. artwork_scraped=2, an art=...|cover| row for the first two, an art=...|scrape_miss| row for the third;
#  3. sentinel: scrape_requests > 0 and the fixture log holds the MusicBrainz search for each album, the front-500 request and the
#     iTunes search; the User-Agent is Selecta/... with no email address;
#  4. a second scan of the same database sends no request at all (covers kept, the miss is not retried);
#  5. revert proof: SELECTA_ART_SCRAPE=off on a fresh database scrapes nothing, sends nothing and stores no artwork row.
# -DCLI=<selecta-cli> -DWORK=<fixture work dir> -DFIXLIB=<tests/library/fixtures>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(fx "http://127.0.0.1:${port}")
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "musicbrainz.test=${fx},coverartarchive.test=${fx},itunes.test=${fx},fixture.test=${fx}")
set(ENV{SELECTA_ART_MB_URL} "http://musicbrainz.test")
set(ENV{SELECTA_ART_CAA_URL} "http://coverartarchive.test")
set(ENV{SELECTA_ART_ITUNES_URL} "http://itunes.test")
unset(ENV{SELECTA_ART_SCRAPE})

set(lib "${WORK}/scrape-lib")
file(REMOVE_RECURSE "${lib}")
file(MAKE_DIRECTORY "${lib}")
foreach(f nocover_hit nocover_itunes nocover_miss)
  file(COPY "${FIXLIB}/${f}.mp3" DESTINATION "${lib}")
endforeach()

function(scan db out_var)
  execute_process(COMMAND "${CLI}" library scan --db "${db}" --scrape --dump "${lib}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 60)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "I1003a: library scan --scrape failed rc=${rc}\n${o}${e}")
  endif()
  set(${out_var} "${o}" PARENT_SCOPE)
endfunction()

function(want_match what text pattern)
  if(NOT "${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1003a ${what}: no match for '${pattern}' in\n${text}")
  endif()
endfunction()

function(want_no_match what text pattern)
  if("${text}" MATCHES "${pattern}")
    message(FATAL_ERROR "I1003a ${what}: unexpected match for '${pattern}' in\n${text}")
  endif()
endfunction()

function(fixture_log out_var)
  execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/scrape-log.json" OUTPUT_VARIABLE lo)
  file(READ "${WORK}/scrape-log.json" log)
  set(${out_var} "${log}" PARENT_SCOPE)
endfunction()

function(count_host log host out_var)
  string(REGEX MATCHALL "\"host\": \"${host}\"" hits "${log}")
  list(LENGTH hits n)
  set(${out_var} ${n} PARENT_SCOPE)
endfunction()

# 1-3. First scan.
set(db "${WORK}/scrape.db")
file(REMOVE "${db}")
scan("${db}" first)
want_match("stat artwork_scraped" "${first}" "artwork_scraped=2 ")
want_match("sentinel scrape_requests" "${first}" "scrape_requests=([1-9][0-9]*)")
want_match("hit cover row" "${first}" "art=Scrape Hit\\|Scrape Artist\\|cover\\|[1-9][0-9]*\\|")
want_match("itunes cover row" "${first}" "art=Scrape Itunes\\|Scrape Artist\\|cover\\|[1-9][0-9]*\\|")
want_match("miss recorded" "${first}" "art=Scrape Miss\\|Scrape Artist\\|scrape_miss\\|")
want_no_match("miss has no cover" "${first}" "art=Scrape Miss\\|Scrape Artist\\|cover\\|")
want_match("one miss counted" "${first}" "scrape_misses=1 ")

fixture_log(log1)
count_host("${log1}" "musicbrainz.test" mb1)
count_host("${log1}" "coverartarchive.test" caa1)
count_host("${log1}" "itunes.test" it1)
if(NOT mb1 EQUAL 3)
  message(FATAL_ERROR "I1003a: want 3 MusicBrainz searches (one per album), got ${mb1}\n${log1}")
endif()
if(NOT caa1 EQUAL 1)
  message(FATAL_ERROR "I1003a: want 1 Cover Art Archive request (the hit), got ${caa1}\n${log1}")
endif()
if(NOT it1 EQUAL 3)
  message(FATAL_ERROR "I1003a: want 3 iTunes requests (search + image for the iTunes album, search for the miss album), got ${it1}\n${log1}")
endif()
want_match("front-500 path" "${log1}" "/release/0a1b2c3d-1111-4222-8333-444455556666/front-500")
want_match("user agent" "${log1}" "Selecta/")
want_no_match("no email in user agent" "${log1}" "\"user-agent\": \"[^\"]*@")

# 4. Second scan: nothing to ask, the miss is remembered.
scan("${db}" second)
want_match("second scan scrape_requests" "${second}" "scrape_requests=0[^0-9]")
want_match("second scan artwork_scraped" "${second}" "artwork_scraped=0 ")
want_match("covers kept" "${second}" "art=Scrape Hit\\|Scrape Artist\\|cover\\|")
want_match("miss kept" "${second}" "art=Scrape Miss\\|Scrape Artist\\|scrape_miss\\|")
fixture_log(log2)
count_host("${log2}" "musicbrainz.test" mb2)
if(NOT mb2 EQUAL mb1)
  message(FATAL_ERROR "I1003a: the second scan sent MusicBrainz requests (${mb1} -> ${mb2}); the miss must not be retried")
endif()

# 5. Revert proof: scraping off on a fresh database.
set(db2 "${WORK}/scrape-off.db")
file(REMOVE "${db2}")
set(ENV{SELECTA_ART_SCRAPE} "off")
scan("${db2}" off)
unset(ENV{SELECTA_ART_SCRAPE})
want_match("off: artwork_scraped" "${off}" "artwork_scraped=0 ")
want_match("off: scrape_requests" "${off}" "scrape_requests=0[^0-9]")
want_no_match("off: no artwork row" "${off}" "art=")
fixture_log(log3)
count_host("${log3}" "musicbrainz.test" mb3)
if(NOT mb3 EQUAL mb1)
  message(FATAL_ERROR "I1003a: SELECTA_ART_SCRAPE=off still sent requests (${mb1} -> ${mb3})")
endif()

message("art_scrape_ok")
