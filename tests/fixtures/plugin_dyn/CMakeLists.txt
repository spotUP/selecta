# Fixture plugin modules for the runtime loader (I427a). good/ holds the working module, reject/ the three
# modules the loader must skip; each directory is one $SELECTA_PLUGIN_PATH entry in the tests. The modules build
# with the default target, so the ctest entries below find them.
set(SEL_DYN_GOOD "${CMAKE_CURRENT_BINARY_DIR}/good")
set(SEL_DYN_REJECT "${CMAKE_CURRENT_BINARY_DIR}/reject")

set(SEL_MODULE_OUTPUT_DIR "${SEL_DYN_GOOD}")
sel_add_plugin(dyn_tone 1 MODULE module.c)

set(SEL_MODULE_OUTPUT_DIR "${SEL_DYN_REJECT}")
sel_add_plugin(dyn_apimismatch 1 MODULE module.c)
target_compile_definitions(sel_module_dyn_apimismatch PRIVATE DYN_API_MISMATCH)
sel_add_plugin(dyn_dup 1 MODULE module.c)
target_compile_definitions(sel_module_dyn_dup PRIVATE DYN_ID="ym")

# No entry symbol: a plain library, not built through sel_add_plugin.
add_library(sel_module_dyn_nosym MODULE nosym.c)
set_target_properties(sel_module_dyn_nosym PROPERTIES PREFIX "" OUTPUT_NAME "sel_dyn_nosym"
  LIBRARY_OUTPUT_DIRECTORY "${SEL_DYN_REJECT}")
if(APPLE)
  set_target_properties(sel_module_dyn_nosym PROPERTIES SUFFIX ".dylib")
else()
  set_target_properties(sel_module_dyn_nosym PROPERTIES SUFFIX ".so")
endif()

# The three tests run the real selecta-cli: render through the loader, nm of the core binary, the reject counts.
add_test(NAME plugin-dyn-render
  COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DGOOD=${SEL_DYN_GOOD}"
    "-DSONG=${CMAKE_CURRENT_SOURCE_DIR}/song.seldyn" "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/dyn.wav"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/render.cmake")
add_test(NAME plugin-dyn-nm
  COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" -P "${CMAKE_CURRENT_SOURCE_DIR}/nm.cmake")
add_test(NAME plugin-dyn-reject
  COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DREJECT=${SEL_DYN_REJECT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/reject.cmake")
