# T49 registry-machine-filter: `plugins --machine <tier> --stats` for four fake sel_machine tiers.
# Sentinel `filtered=N` (plugins the registry machine filter removed). More capable tier, fewer filtered:
# a500 > a1200 >= a1200-fpu >= a060; the listed set per tier equals the golden list (pinned from the first
# green run; an empty or absent golden is red). Inputs: MODE CLI [VAMOS AMIGA_CLI] GOLDEN_DIR
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
set(ENV{SELECTA_NO_APP_PLUGINS} 1) # I1305d: the golden counts the built-in set only, not the CLI plugins folder (I1242b)
foreach(tier a500 a1200 a1200-fpu a060)
  m2_run_ok(T49 out plugins --machine ${tier} --stats)
  m2_need_key(T49 "${out}" filtered "registry machine filter (--machine ${tier})")
  m2_get("${out}" filtered f_${tier})
  m2_golden(T49 "${f_${tier}}" "${GOLDEN_DIR}/registry-${MODE}-${tier}.filtered" "filtered count for ${tier}")
endforeach()
# Deviation from the Phase 0 text (a500 > a1200): the 68k build carries nuade only and desktop adds
# openmpt; both need the 68020 and float, so a500 and a1200 both hide them. Goldens are per mode.
if(f_a500 LESS f_a1200 OR f_a1200 LESS f_a1200-fpu OR f_a1200-fpu LESS f_a060)
  message(FATAL_ERROR "RED[T49]: filtered counts not ordered a500>=a1200>=a1200-fpu>=a060: ${f_a500} ${f_a1200} ${f_a1200-fpu} ${f_a060}")
endif()
# 68000 hides SEL_RP_MIN_CPU=68020 plugins; no-FPU hides SEL_RP_NEEDS_FPU (68881 code) plugins and keeps the
# soft-float SEL_RP_NEEDS_FLOAT ones, counted in slow_needs_float (I1316).
m2_run_ok(T49 out plugins --machine a500 --stats)
m2_check(T49 "${out}" "hidden_min_cpu>=1")
m2_run_ok(T49 out plugins --machine a1200 --stats)
m2_check(T49 "${out}" "slow_needs_float>=1,hidden_min_cpu==0,hidden_needs_fpu>=1") # I1316: stb_vorbis, minimp3
m2_run_ok(T49 out plugins --machine a500 --stats)
m2_check(T49 "${out}" "hidden_memory>=1")
m2_run_ok(T49 out plugins --machine a1200-fpu --stats)
m2_check(T49 "${out}" "filtered==0,slow_needs_float==0,hidden_needs_fpu==0")
# I1313: the real machine probe (--machine probe) under vamos. SEL_MACHINE_FREE_RAM is AvailMem(MEMF_ANY) after
# Selecta has started; a player whose measured SEL_RP_MEMORY peak is above it is hidden (hidden_memory). At the
# 16 MB baseline nuade (the largest Amiga player) is listed; at SMALL_RAM_KB it is not.
if(MODE STREQUAL "vamos")
  m2_run_ok(T49 out plugins --machine probe --stats)
  m2_check(T49 "${out}" "hidden_memory==0")
  if(NOT out MATCHES "(^|\n)nuade[ \t]")
    message(FATAL_ERROR "RED[T49]: nuade not listed at -m ${SEL_VAMOS_RAM_KB}:\n${out}")
  endif()
  set(ram_16mb ${SEL_VAMOS_RAM_KB})
  # 4 MB: free RAM after start falls below nuade's SEL_RP_MEMORY (2621440, plugins/replayers/nuade/manifest.c).
  # The run prints the free RAM the gate used, so a smaller Selecta (more free RAM) fails the precondition
  # below by name instead of turning the gate check red.
  set(SEL_VAMOS_RAM_KB 4096)
  m2_run_ok(T49 out plugins --machine probe --stats)
  set(SEL_VAMOS_RAM_KB ${ram_16mb})
  if(NOT out MATCHES "free_ram=([0-9]+)")
    message(FATAL_ERROR "RED[T49]: --stats printed no free_ram:\n${out}")
  endif()
  set(free_ram ${CMAKE_MATCH_1})
  if(NOT free_ram LESS 2621440)
    message(FATAL_ERROR "RED[T49]: precondition: free_ram=${free_ram} at vamos -m 4096 is not below nuade's 2621440; lower the RAM size")
  endif()
  m2_check(T49 "${out}" "hidden_memory>=1")
  if(out MATCHES "(^|\n)nuade[ \t]")
    message(FATAL_ERROR "RED[T49]: nuade listed with free_ram=${free_ram} below its 2621440 (vamos -m 4096):\n${out}")
  endif()
  # I1313c: the registry keeps why nuade is off (the filter's own numbers), and the line a UI shows.
  if(NOT out MATCHES "(^|\n)hidden=nuade reason=memory needed=2621440 available=${free_ram}\n")
    message(FATAL_ERROR "RED[I1313c]: no hidden record for nuade (reason memory, 2621440 vs ${free_ram}):\n${out}")
  endif()
  if(NOT out MATCHES "(^|\n)hidden_line=nuade off: needs 2560 KB of free memory, [0-9]+ KB free\n")
    message(FATAL_ERROR "RED[I1313c]: no hidden_line for nuade:\n${out}")
  endif()
  # The text UI's Now Playing page shows the same line, read from the engine state (never derived in the UI).
  if(AMIGA_SELECTA)
    set(WORK "${WORK}-i1313c")
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${WORK}")
    file(WRITE "${WORK}/np.keys" "wait 500\nN\nwait 300\nq\n")
    # Selecta is larger than selecta-cli and does not load at -m 4096 (vamos "alloc: NO MEMORY"); at -m 6144 its free RAM
    # after start (about 2150 KB) is still below nuade's 2560 KB. Below 16 MB Selecta exits 1 with a CPU error on a custom
    # chip register after the keys ran (with or without the Now Playing page), so only the screen dump is checked here.
    set(SEL_VAMOS_RAM_KB 6144)
    sel_vamos_cmd(SEL_VAMOS_CMD "${AMIGA_SELECTA}")
    set(SEL_VAMOS_RAM_KB ${ram_16mb})
    execute_process(COMMAND ${SEL_VAMOS_CMD} -V work:${WORK} -- "${AMIGA_SELECTA}" --output null --size 80x25 --start-dir work:
                            --keys work:np.keys --screen-dump work:np.txt
                    RESULT_VARIABLE rc OUTPUT_VARIABLE uout ERROR_VARIABLE uerr TIMEOUT 120)
    if(NOT EXISTS "${WORK}/np.txt")
      message(FATAL_ERROR "RED[I1313c]: Selecta at vamos -m 6144 wrote no screen dump (rc=${rc}):\n${uout}\n${uerr}")
    endif()
    file(READ "${WORK}/np.txt" dump)
    if(NOT dump MATCHES "Players Turned Off" OR NOT dump MATCHES "nuade off: needs 2560 KB of free memory, [0-9]+ KB free")
      message(FATAL_ERROR "RED[I1313c]: text UI Now Playing shows no nuade line (rc=${rc}):\n${dump}\n${uerr}")
    endif()
    string(REGEX MATCH "nuade off: needs 2560 KB of free memory, [0-9]+ KB free" ui_line "${dump}")
    message(STATUS "[OK] I1313c text UI: ${ui_line}")
  endif()
endif()
# Priority owner: one owner per format, from the registry the filter feeds.
m2_run_ok(T49 out plugins --owner actionamics)
if(NOT out MATCHES "owner=nuade")
  message(FATAL_ERROR "RED[T49]: plugins --owner actionamics did not pick nuade:\n${out}")
endif()
message(STATUS "[OK] T49 ${MODE}")
