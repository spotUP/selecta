# I710ga7: reachability of the system-file feature (design doc section 13), through the selecta top-level entry point.
# A real CyberTracker .ct song needs the user's tracker disk image (a declared system file, RP_SYSFILE, bbmp_cybertracker).
# The config dir is empty; the first URL of the list is the fixture server (csdb.test), the second one is a 404.
# (1) first run: ERROR system_file_missing line, `sysfile missing_reports=1 fetches=1 installed=1 reopens=1` (the sentinel:
#     the download job ran and the engine re-opened the song), exactly one fixture GET of the zip, the stored .d64 has the
#     declared sha256, the replayer is bbmp_cybertracker and rendered frames > 0 with a non-zero output peak.
# (2) second run, same config dir: missing_reports=0 fetches=0 (the store is reused by later sessions).
# (3) store-call leg: fresh config dir, both URLs 404, SYSFILE_INSTALL with the cached zip: playback after, installed=1.
# The zip is linked into the served tree at run time and removed again; it is never committed (see sysfile_cybertracker.sh).
# -DSELECTA -DWORK -DFIXDIR -DPLUGINS -DZIP -DSONG
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "csdb.test=http://127.0.0.1:${port},mirror.sysfile.test=http://127.0.0.1:${port}")
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
unset(ENV{SELECTA_MIRROR})
set(want_sha 000ccb714183281bcdf7ac59fef062e46b02e475511990f4c3e8067ea8f8c148)
set(served "${FIXDIR}/csdb.test/getinternalfile.php/63969")
set(link "${served}/cybertracker1_01-d64.zip")
set(root "${WORK}/sysfile-cybertracker")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}" "${served}")

function(cleanup)
  file(REMOVE "${link}")
  file(REMOVE_RECURSE "${FIXDIR}/csdb.test/getinternalfile.php")
endfunction()
function(die msg)
  cleanup()
  message(FATAL_ERROR "sysfile-cybertracker: ${msg}")
endfunction()
function(need text pattern what)
  if(NOT text MATCHES "${pattern}")
    die("${what}: no match for ${pattern}\n${text}")
  endif()
endfunction()

function(play tag cfg urls exec_extra out)
  set(ENV{SELECTA_SYSFILE_URLS_CYBERTRACKER101_D64} "${urls}")
  file(WRITE "${root}/${tag}.txt" "wait 20\nsleep 6000\nwait 20\n")
  execute_process(COMMAND "${SELECTA}" --output null --config-dir "${cfg}" --db "${root}/${tag}.db"
    --ui desktop --backend mem --canvas 1280x800 --clock fixed --input "${root}/${tag}.txt" --stats --exec "OPEN ${SONG}" --exec PLAY ${exec_extra}
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 120)
  if(NOT rc EQUAL 0)
    die("${tag}: selecta exited ${rc}\n${o}${e}")
  endif()
  set(${out} "${o}${e}" PARENT_SCOPE)
endfunction()

function(playing text tag)
  need("${text}" "replayer=bbmp_cybertracker" "${tag}: the CyberTracker replayer played")
  string(REGEX MATCH "rendered_frames=([0-9]+)" m "${text}")
  if(NOT m OR CMAKE_MATCH_1 LESS 1)
    die("${tag}: no frames rendered (${m})\n${text}")
  endif()
  string(REGEX MATCH "out_peak=([0-9]+)" m "${text}")
  if(NOT m OR CMAKE_MATCH_1 LESS 1)
    die("${tag}: silent output (${m})\n${text}")
  endif()
endfunction()

file(CREATE_LINK "${ZIP}" "${link}" SYMBOLIC)
set(urls "http://csdb.test/getinternalfile.php/63969/cybertracker1_01-d64.zip;http://mirror.sysfile.test/sysfiles/nothere.bin")

# (1) first run, empty config dir: missing -> download -> install -> re-open -> plays.
set(cfg1 "${root}/cfg1")
file(MAKE_DIRECTORY "${cfg1}")
find_program(CURL curl)
execute_process(COMMAND "${CURL}" -s -X POST "http://127.0.0.1:${port}/__fixture/reset" -H "Host: csdb.test")
play(run1 "${cfg1}" "${urls}" "" o1)
need("${o1}" "error system_file_missing bbmp_cybertracker:cybertracker101.d64" "(1) the missing-file error line")
need("${o1}" "sysfile missing_reports=1 fetches=1 installed=1 reopens=1 " "(1) sentinel: the download job ran and the song re-opened")
playing("${o1}" "(1)")
file(SHA256 "${cfg1}/cybertracker101.d64" got)
if(NOT got STREQUAL want_sha)
  die("(1) stored sha256 ${got}, want ${want_sha}")
endif()
execute_process(COMMAND "${CURL}" -s "http://127.0.0.1:${port}/__fixture/log" -H "Host: csdb.test" OUTPUT_VARIABLE log)
string(REGEX MATCHALL "cybertracker1_01-d64.zip" gets "${log}")
list(LENGTH gets n)
if(NOT n EQUAL 1)
  die("(1) ${n} fixture requests for the zip, want exactly 1\n${log}")
endif()

# (2) second run, same config dir: the store answers, nothing is fetched.
play(run2 "${cfg1}" "${urls}" "" o2)
need("${o2}" "sysfile missing_reports=0 fetches=0 " "(2) later sessions reuse the stored file")
playing("${o2}" "(2)")

# (3) store-call leg: both URLs 404, the user supplies the zip through SYSFILE_INSTALL.
set(cfg3 "${root}/cfg3")
file(MAKE_DIRECTORY "${cfg3}")
set(dead "http://mirror.sysfile.test/sysfiles/nothere1.bin;http://mirror.sysfile.test/sysfiles/nothere2.bin")
play(run3a "${cfg3}" "${dead}" "" o3a)
need("${o3a}" "sysfile missing_reports=1 fetches=1 installed=0 reopens=0 state=2\n" "(3) without the file the song does not play")
if("${o3a}" MATCHES "replayer=bbmp_cybertracker")
  die("(3) played with no system file\n${o3a}")
endif()
play(run3b "${cfg3}" "${dead}" "--exec;SYSFILE_INSTALL cybertracker101.d64 ${ZIP}" o3b)
need("${o3b}" "sysfile missing_reports=1 [^\n]*installed=1 reopens=1 " "(3) SYSFILE_INSTALL supplied the image and the song re-opened")
playing("${o3b}" "(3)")
file(SHA256 "${cfg3}/cybertracker101.d64" got3)
if(NOT got3 STREQUAL want_sha)
  die("(3) stored sha256 ${got3}, want ${want_sha}")
endif()
cleanup()
message(STATUS "sysfile_cybertracker_ok")
