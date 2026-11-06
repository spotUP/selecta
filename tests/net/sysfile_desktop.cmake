# I710ga6: the desktop UI's system-file flow over the net fixture, with the loadable test-only replayer sysfile_probe
# (tests/fixtures/plugin_sysfile; one synthetic file "Test system file", identity passed in by environment).
# (a) both mirrors 404: the note line reads "Test system file needed", the fallback dialog opens once by itself, Enter
#     (Pick File...) answers through --dialog-answer with the synthetic file: SYSFILE_INSTALL, the song plays, and the
#     "downloaded" toast count stays 0 (nothing was downloaded).
# (b) the mirror serves the file: the toast "Test system file downloaded" fires once, no dialog, no note, the song plays.
# Sentinels come from the desktop dump line `sysfile state= toasts= buttons= picks= retries= dlg= dlg_opens=` and the
# engine `sysfile` stats line. -DSELECTA -DWORK -DFIXDIR -DPLUGINS=<dir holding sel_sysfile_probe>
file(READ "${WORK}/port" port)
string(STRIP "${port}" port)
set(ENV{SELECTA_NET_ALLOW} "127.0.0.1")
set(ENV{SELECTA_NET_MAP} "mirror.sysfile.test=http://127.0.0.1:${port}")
set(ENV{SELECTA_PLUGIN_PATH} "${PLUGINS}")
unset(ENV{SELECTA_MIRROR})
set(good "${FIXDIR}/mirror.sysfile.test/sysfiles/good.bin")
file(SHA256 "${good}" sha)
set(ENV{SELECTA_PROBE_SYSFILE_SHA} "${sha}")
set(root "${WORK}/sysfile-desktop")
file(REMOVE_RECURSE "${root}")
file(MAKE_DIRECTORY "${root}")
file(WRITE "${root}/probe.sfp" "SFP\n")
# The synthetic file the user picks: a copy of the fixture bytes made in the work dir at test time.
file(COPY "${good}" DESTINATION "${root}/picked")

function(run_leg tag urls answer out)
  set(ENV{SELECTA_PROBE_SYSFILE_URLS} "${urls}")
  set(cfg "${root}/${tag}-cfg")
  file(MAKE_DIRECTORY "${cfg}")
  file(WRITE "${root}/${tag}.txt" "wait 20\nsleep 500\nwait 20\nkey escape\nsleep 3000\nwait 50\nkey enter\nsleep 1500\nwait 50\n")
  set(extra "")
  if(NOT "${answer}" STREQUAL "")
    set(extra --dialog-answer "${answer}")
  endif()
  execute_process(COMMAND "${SELECTA}" --output null --config-dir "${cfg}" --db "${root}/${tag}.db" --ui desktop
    --backend mem --canvas 1280x800 --clock fixed --input "${root}/${tag}.txt" ${extra} --stats
    --exec "OPEN ${root}/probe.sfp" --exec PLAY
    OUTPUT_VARIABLE o ERROR_VARIABLE e RESULT_VARIABLE rc TIMEOUT 90)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "sysfile-desktop ${tag}: selecta exited ${rc}\n${o}${e}")
  endif()
  set(${out} "${o}" PARENT_SCOPE)
endfunction()

function(need text pattern what)
  if(NOT text MATCHES "${pattern}")
    message(FATAL_ERROR "sysfile-desktop: ${what}: no match for ${pattern}\n${text}")
  endif()
endfunction()

# (a) both URLs 404, then the user picks the file.
run_leg(a "http://mirror.sysfile.test/sysfiles/nothere1.bin;http://mirror.sysfile.test/sysfiles/nothere2.bin" "${root}/picked/good.bin" oa)
need("${oa}" "sysfile state=[0-9]+ toasts=0 buttons=[1-9][0-9]* picks=1 retries=0 dlg=0 dlg_opens=2\n" "(a) dump line: note button drawn, one pick, no toast, dialog closed (dlg_opens counts the launch sync offer, escaped first, plus the system file dialog)")
need("${oa}" "nowplaying note=\"\"" "(a) the note is gone once the file is installed")
need("${oa}" "sysfile missing_reports=[0-9]+ fetches=[0-9]+ installed=1 " "(a) the engine installed the picked file")
need("${oa}" "sysfile missing_reports=[0-9]+ fetches=1 " "(a) the silent download was tried once")
need("${oa}" "sysfile missing_reports=[0-9]+ fetches=[0-9]+ installed=[0-9]+ reopens=1 " "(a) the song re-opened after the install")

# (b) the mirror serves the file.
run_leg(b "http://mirror.sysfile.test/sysfiles/nothere1.bin;http://mirror.sysfile.test/sysfiles/good.bin" "" ob)
need("${ob}" "sysfile state=[0-9]+ toasts=1 buttons=0 picks=0 retries=0 dlg=0 dlg_opens=1\n" "(b) dump line: one downloaded toast, no button, no dialog")
need("${ob}" "sysfile missing_reports=[0-9]+ fetches=[0-9]+ installed=1 " "(b) the engine installed the download")
need("${ob}" "sysfile missing_reports=[0-9]+ fetches=[0-9]+ installed=[0-9]+ reopens=1 " "(b) the song re-opened after the download")
message(STATUS "sysfile_desktop_ok")
