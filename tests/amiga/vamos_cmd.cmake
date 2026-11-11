# sel_vamos_cmd(out_var exe [cpu]): the vamos command prefix every vamos test driver runs a 68k exe with
# (I1305i1). Gives `${VAMOS} -C <cpu> -m 8192 -V selbin:<dir of exe> -a PROGDIR:selbin:`; the caller appends
# its own volumes/assigns, then `-- <exe> <args>`.
#
# Why the PROGDIR: assign: on a real Amiga PROGDIR: is the directory the exe was started from, and player
# modules load with LoadSeg("PROGDIR:Players/sel_<name>") (I427e, cmake/selecta_registry.cmake
# sel_amiga_players_beside copies them there). vamos defines no PROGDIR:, so without this every on-demand
# player load under vamos fails "not found" and the song reports no_replayer.
# Inputs: VAMOS (path of the vamos binary).
function(sel_vamos_cmd out_var exe)
  set(cpu 68020)
  if(ARGC GREATER 2)
    set(cpu "${ARGV2}")
  endif()
  get_filename_component(exe_dir "${exe}" DIRECTORY)
  set(${out_var} "${VAMOS}" -C ${cpu} -m 8192 -V "selbin:${exe_dir}" -a "PROGDIR:selbin:" PARENT_SCOPE)
endfunction()
