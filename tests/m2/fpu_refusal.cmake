# I1316: `selecta-cli render x.ogg` on a 68020 without an FPU (vamos -C 68020: AttnFlags carry no 68881) refuses the
# song cleanly: stb_vorbis is built with -m68881 (SEL_RP_NEEDS_FPU), so the registry must hide it on the probed machine
# and the engine reports no_replayer. Before I1316 the CLI passed no machine to the registry, stb_vorbis was picked,
# and the 68k CLI died on the first FPU opcode (a CPU exception in vamos, no "error no_replayer" line).
# Inputs: MODE (vamos) VAMOS AMIGA_CLI MOD (an .ogg) WORK
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
m2_path(vmod "${MOD}")
m2_path(vwav "${WORK}/out.wav")
m2_exec(out rc render "${vmod}" --output "${vwav}" --seconds 1)
if(out MATCHES "[Ee]xception|Unhandled|[Ll]ine.?[Ff]|TRAP|Guru|Traceback")
  message(FATAL_ERROR "RED[I1316]: render of an .ogg crashed (CPU exception) on a machine without an FPU.\n--- output ---\n${out}")
endif()
if(rc EQUAL 0 OR NOT out MATCHES "error no_replayer")
  message(FATAL_ERROR "RED[I1316]: wanted a clean no_replayer refusal (non-zero exit), got exit ${rc}.\n--- output ---\n${out}")
endif()
message(STATUS "[OK] I1316 ${MODE}: .ogg refused without an FPU, exit ${rc}")
