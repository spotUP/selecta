# T49 registry-machine-filter: `plugins --machine <tier> --stats` for four fake sel_machine tiers.
# Sentinel `filtered=N` (plugins the registry machine filter removed). More capable tier, fewer filtered:
# a500 > a1200 >= a1200-fpu >= a060; the listed set per tier equals the golden list (pinned from the first
# green run; an empty or absent golden is red). Inputs: MODE CLI [VAMOS AMIGA_CLI] GOLDEN_DIR
include("${CMAKE_CURRENT_LIST_DIR}/lib.cmake")
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
# 68000 hides SEL_RP_MIN_CPU=68020 plugins, no-FPU hides SEL_RP_NEEDS_FLOAT plugins.
m2_run_ok(T49 out plugins --machine a500 --stats)
m2_check(T49 "${out}" "hidden_min_cpu>=1")
m2_run_ok(T49 out plugins --machine a1200 --stats)
m2_check(T49 "${out}" "hidden_needs_float>=1,hidden_min_cpu==0")
m2_run_ok(T49 out plugins --machine a500 --stats)
m2_check(T49 "${out}" "hidden_memory>=1")
m2_run_ok(T49 out plugins --machine a1200-fpu --stats)
m2_check(T49 "${out}" "filtered==0,hidden_needs_float==0")
# Priority owner: one owner per format, from the registry the filter feeds.
m2_run_ok(T49 out plugins --owner actionamics)
if(NOT out MATCHES "owner=nuade")
  message(FATAL_ERROR "RED[T49]: plugins --owner actionamics did not pick nuade:\n${out}")
endif()
message(STATUS "[OK] T49 ${MODE}")
