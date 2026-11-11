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
  set(SEL_VAMOS_RAM_KB 6144) # free RAM after start falls below nuade's SEL_RP_MEMORY
  m2_run_ok(T49 out plugins --machine probe --stats)
  set(SEL_VAMOS_RAM_KB ${ram_16mb})
  m2_check(T49 "${out}" "hidden_memory>=1")
  if(out MATCHES "(^|\n)nuade[ \t]")
    message(FATAL_ERROR "RED[T49]: nuade listed with too little free RAM (vamos -m 6144):\n${out}")
  endif()
endif()
# Priority owner: one owner per format, from the registry the filter feeds.
m2_run_ok(T49 out plugins --owner actionamics)
if(NOT out MATCHES "owner=nuade")
  message(FATAL_ERROR "RED[T49]: plugins --owner actionamics did not pick nuade:\n${out}")
endif()
message(STATUS "[OK] T49 ${MODE}")
