# T80 vote-devilbox (row I174): VOTE 5 sends GET /api/hip/vote?k=<token>&m=<uade key>&r=5 byte-exact (I139), the answer
# "<count> <yours>" is parsed, a repeat is rate limited ("ERR rate", 429), VOTE reads /api/hip/votes?m=&k=, VOTE 0
# withdraws, a 404 prints devilbox_missing=hip_vote and keeps the vote local. Own fixture server and request log.
# -DCLI=<selecta-cli> -DWORK=<dir> -DM3=<tests/m3> -DSONGS=<tests/m3/genre>
set(ctl "${CMAKE_CURRENT_LIST_DIR}/fixture_ctl.cmake")
execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=stop -DWORK=${WORK} -P "${ctl}")
file(REMOVE_RECURSE "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=start -DWORK=${WORK} -P "${ctl}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "T80 fixture start failed")
endif()

function(finish)
  execute_process(COMMAND "${CMAKE_COMMAND}" -DACTION=stop -DWORK=${WORK} -P "${ctl}")
endfunction()

function(fail what)
  finish()
  message(FATAL_ERROR "T80 ${what}\n${last_raw}")
endfunction()

file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_DEVILBOX_URL} "http://devilbox.uprough.net")
set(ENV{SELECTA_NET_MAP} "devilbox.uprough.net=http://127.0.0.1:${port},fixture.test=http://127.0.0.1:${port}")
set(last_raw "")

function(run_script tmpl cfg db)
  file(READ "${M3}/${tmpl}" t)
  string(REPLACE "@SONGS@" "${SONGS}" t "${t}")
  file(WRITE "${WORK}/${tmpl}" "${t}")
  execute_process(COMMAND "${CLI}" script "${WORK}/${tmpl}" --db "${db}" --config-dir "${cfg}"
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc)
  set(last_raw "${o}${e}" PARENT_SCOPE)
  set(last_out "${o}" PARENT_SCOPE)
  if(NOT rc EQUAL 0)
    set(last_raw "${o}${e}")
    fail("script ${tmpl} failed rc=${rc}")
  endif()
endfunction()

# The uade key of the song, computed here independently of the C code (xxh32 of the first 262144 bytes, seed 0, then
# the file length mod 65536 as 4 hex digits).
find_program(PY python3 REQUIRED)
execute_process(COMMAND "${PY}" -I -c "
import sys
data = open(sys.argv[1], 'rb').read()
P1, P2, P3, P4, P5 = 2654435761, 2246822519, 3266489917, 668265263, 374761393
M = 0xFFFFFFFF
rotl = lambda x, r: ((x << r) | (x >> (32 - r))) & M
d = data[:262144]
n = len(d)
i = 0
if n >= 16:
    v = [(P1 + P2) & M, P2, 0, (-P1) & M]
    while i + 16 <= n:
        for j in range(4):
            w = int.from_bytes(d[i + 4 * j:i + 4 * j + 4], 'little')
            v[j] = (rotl((v[j] + w * P2) & M, 13) * P1) & M
        i += 16
    h = (rotl(v[0], 1) + rotl(v[1], 7) + rotl(v[2], 12) + rotl(v[3], 18)) & M
else:
    h = P5
h = (h + n) & M
while i + 4 <= n:
    h = (rotl((h + int.from_bytes(d[i:i + 4], 'little') * P3) & M, 17) * P4) & M
    i += 4
while i < n:
    h = (rotl((h + d[i] * P5) & M, 11) * P1) & M
    i += 1
h ^= h >> 15
h = (h * P2) & M
h ^= h >> 13
h = (h * P3) & M
h ^= h >> 16
print('%08x%04x' % (h, len(data) & 0xFFFF))
" "${SONGS}/ambient.mod" OUTPUT_VARIABLE key OUTPUT_STRIP_TRAILING_WHITESPACE)

set(cfg "${WORK}/cfg")
set(db "${WORK}/vote.db")
run_script(vote.sel "${cfg}" "${db}")

# The token: 128 random bits as 32 hex digits, made once, kept in the prefs file.
file(GLOB conf "${cfg}/*")
set(token "")
foreach(f ${conf})
  file(READ "${f}" c)
  if(c MATCHES "vote_token[ \t]*=[ \t]*\"?([0-9a-f]+)\"?")
    set(token "${CMAKE_MATCH_1}")
  endif()
endforeach()
string(LENGTH "${token}" tl)
if(NOT tl EQUAL 32)
  fail("vote token not stored as 32 hex digits in ${cfg}: '${token}'")
endif()

string(REGEX MATCHALL "VOTES [^\n]*" lines "${last_out}")
list(LENGTH lines n)
if(NOT n EQUAL 4)
  fail("want 4 VOTES lines, got ${n}")
endif()
set(want
  "VOTES vote_posts=1 status=ok count=1 yours=5 m=${key}"
  "VOTES vote_posts=1 status=rate count=1 yours=5 m=${key}"
  "VOTES vote_posts=1 status=ok count=1 yours=5 m=${key}"
  "VOTES vote_posts=2 status=ok count=0 yours=0 m=${key}")
set(i 0)
foreach(w ${want})
  list(GET lines ${i} got)
  if(NOT got STREQUAL "${w}")
    fail("VOTES line ${i}: '${got}', want '${w}'")
  endif()
  math(EXPR i "${i} + 1")
endforeach()

# I174a: the playlist row carries the count the service answered ("Votes N"), none before the first answer; a rate
# limited request keeps the stored count; the withdraw stores the new count 0.
string(REGEX MATCHALL "ROW [^\n]*" rows "${last_out}")
list(LENGTH rows nrows)
if(NOT nrows EQUAL 5)
  fail("want 5 ROW lines, got ${nrows}")
endif()
set(want_rows
  "ROW ambient.mod"
  "ROW ambient.mod  Votes 1"
  "ROW ambient.mod  Votes 1"
  "ROW ambient.mod  Votes 1"
  "ROW ambient.mod  Votes 0")
set(i 0)
foreach(w ${want_rows})
  list(GET rows ${i} got)
  if(NOT got STREQUAL "${w}")
    fail("playlist row ${i}: '${got}', want '${w}'")
  endif()
  math(EXPR i "${i} + 1")
endforeach()

# The requests, byte-exact: three votes (5, repeat 5, 0) and one read.
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/log --out "${WORK}/log.json" OUTPUT_VARIABLE o)
file(READ "${WORK}/log.json" logjson)
set(paths "")
string(JSON len LENGTH "${logjson}")
math(EXPR last "${len} - 1")
foreach(j RANGE ${last})
  string(JSON h GET "${logjson}" ${j} host)
  string(JSON p GET "${logjson}" ${j} path)
  if(h STREQUAL "devilbox.uprough.net" AND p MATCHES "^/api/hip/")
    list(APPEND paths "${p}")
  endif()
endforeach()
set(want_paths
  "/api/hip/vote?k=${token}&m=${key}&r=5"
  "/api/hip/vote?k=${token}&m=${key}&r=5"
  "/api/hip/votes?m=${key}&k=${token}"
  "/api/hip/vote?k=${token}&m=${key}&r=0")
if(NOT "${paths}" STREQUAL "${want_paths}")
  fail("request paths\n${paths}\nwant\n${want_paths}")
endif()

# 404: the vote stays local.
execute_process(COMMAND "${CLI}" fetch http://fixture.test/__fixture/votes_missing OUTPUT_VARIABLE o)
run_script(vote_missing.sel "${WORK}/cfg2" "${WORK}/vote2.db")
if(NOT last_out MATCHES "devilbox_missing=hip_vote" OR NOT last_out MATCHES "VOTES vote_posts=0 status=missing")
  fail("404 case did not report devilbox_missing=hip_vote")
endif()
if(NOT last_out MATCHES "SRC_NAME=vote,SRC_PATH=5,SRC_COUNT=1")
  fail("404 case lost the local vote")
endif()

finish()
message(STATUS "vote_devilbox_ok")
