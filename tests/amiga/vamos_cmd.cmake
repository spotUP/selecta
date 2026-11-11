# sel_vamos_cmd(out_var exe [cpu]): the vamos command prefix every vamos test driver runs a 68k exe with
# (I1305i1). Gives `${VAMOS} -C <cpu> -m ${SEL_VAMOS_RAM_KB} -H disable -V selbin:<dir of exe> -a PROGDIR:selbin:`;
# the caller appends its own volumes/assigns, then `-- <exe> <args>`.
#
# Why 16 MB (I1305i4, Johan 2026-11-11: "this is a highend amiga music player we can expect people to have
# 16-256mb ram", "we should optimize ram usage as much as possible but not limit us to 8mb"): the test machine
# is the smallest supported one. Peak use of hash-idle-vamos was 9,485,860 bytes, over the old 8 MB. RAM use
# is still kept down; a player that needs more RAM than the machine has is disabled, not the whole app.
# Why -H disable: vamos refuses more than ~10 MB with custom-chip access mapped ("Too much RAM allocated
# with hw access enabled!"). No test reaches the custom chips: output is the null sink or a WAV file.
#
# Why the PROGDIR: assign: on a real Amiga PROGDIR: is the directory the exe was started from, and player
# modules load with LoadSeg("PROGDIR:Players/sel_<name>") (I427e, cmake/selecta_registry.cmake
# sel_amiga_players_beside copies them there). vamos defines no PROGDIR:, so without this every on-demand
# player load under vamos fails "not found" and the song reports no_replayer.
#
# Why the private RAM: volume (I1305l): vamos auto-adds RAM: as the temp volume ~/.vamos/volumes/ram, one
# fixed path per user. A temp volume refuses to start when its dir exists ("temp volume volume path already
# exists") and is rmtree'd at shutdown even when its own setup failed. So a second vamos started while one
# runs fails, and its shutdown deletes the first one's RAM: (T: lives there). ctest's RESOURCE_LOCK vamos
# only serialises tests inside one ctest process; other ctest runs, agents and hand runs on the machine
# collide. A user-given ram: volume replaces the auto one, so each run gets its own dir under TMPDIR.
# Inputs: VAMOS (path of the vamos binary).
set(SEL_VAMOS_RAM_KB 16384)
function(sel_vamos_cmd out_var exe)
  set(cpu 68020)
  if(ARGC GREATER 2)
    set(cpu "${ARGV2}")
  endif()
  get_filename_component(exe_dir "${exe}" DIRECTORY)
  set(tmp "$ENV{TMPDIR}")
  if(NOT tmp)
    set(tmp "/tmp")
  endif()
  string(REGEX REPLACE "/+$" "" tmp "${tmp}")
  string(RANDOM LENGTH 16 tag)
  set(${out_var} "${VAMOS}" -C ${cpu} -m ${SEL_VAMOS_RAM_KB} -H disable -V "selbin:${exe_dir}" -a "PROGDIR:selbin:"
      -V "ram:${tmp}/sel-vamos-ram-${tag}?temp" PARENT_SCOPE)
endfunction()
