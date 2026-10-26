# dui-font-baked (DUI-P4-25, I463): the desktop UI takes its text atlases from the tables baked at build time.
# -DSELECTA -DSONGS -DWORK -DBAKED_UNIT. A full desktop frame through the real entry point (selecta --ui desktop):
# 1. sentinel: atlas_baked_hits > 0 and atlas_builds = 0 and atlas_ms = 0: no atlas was built at run time (the 68020
#    tier has no floating point; building one cost 43.8 s on the first frame).
# 2. a scale that was not baked (3) still renders, by the runtime builder (atlas_builds > 0): the fallback stays.
# 3. the unit compares every baked atlas with the runtime-built atlas of the same font and size, field by field.
get_filename_component(HERE "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
include("${HERE}/dui_run.cmake")
file(REMOVE_RECURSE "${WORK}")
set(common --ui desktop --backend mem --view home --clock fixed --stats --canvas 1280x800 --scale 1)

dui_run(baked ${common})
dui_stat(builds "${baked}" desktop atlas_builds)
dui_stat(hits "${baked}" desktop atlas_baked_hits)
dui_stat(ams "${baked}" desktop atlas_ms)
dui_stat(missing "${baked}" desktop missing_glyphs)
if(NOT builds EQUAL 0 OR hits LESS 1 OR NOT ams EQUAL 0 OR NOT missing EQUAL 0)
  message(FATAL_ERROR "scale 1 frame: atlas_builds=${builds} atlas_baked_hits=${hits} atlas_ms=${ams} missing=${missing}; want 0, >0, 0, 0")
endif()

dui_run(unbaked --ui desktop --backend mem --view home --clock fixed --stats --canvas 3840x2400 --scale 3)
dui_stat(builds3 "${unbaked}" desktop atlas_builds)
if(builds3 LESS 1)
  message(FATAL_ERROR "scale 3 is not baked, so the runtime builder must run: atlas_builds=${builds3}")
endif()

execute_process(COMMAND "${BAKED_UNIT}" OUTPUT_VARIABLE uo ERROR_VARIABLE ue RESULT_VARIABLE urc)
if(NOT urc EQUAL 0)
  message(FATAL_ERROR "baked atlases differ from the runtime atlases (${urc}):\n${uo}${ue}")
endif()
message(STATUS "dui-font-baked ok: baked_hits=${hits} ${uo}")
