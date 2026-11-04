# SelectaSDK package (design 2026-10-30-plugin-loader-sdk D7). find_package(SelectaSDK) provides:
#   Selecta::sdk                       headers (include/selecta, MIT) and the module link rules
#   selecta_add_module(<name> <src>..) builds sel_<name>.dylib / .so, exporting only sel_plugin_entry
include("${CMAKE_CURRENT_LIST_DIR}/SelectaSDKTargets.cmake")

# Same naming and export rules as sel_add_plugin(<name> <kind> MODULE ...) in cmake/selecta_plugin.cmake. The plugin kind
# is the SEL_PLUGIN_KIND value the source declares in its descriptor (include/selecta/plugin.h), not a build argument.
function(selecta_add_module name)
  add_library(${name} MODULE ${ARGN})
  target_link_libraries(${name} PRIVATE Selecta::sdk)
  set_target_properties(${name} PROPERTIES PREFIX "" OUTPUT_NAME "sel_${name}")
  if(APPLE)
    set_target_properties(${name} PROPERTIES SUFFIX ".dylib")
  else()
    set_target_properties(${name} PROPERTIES SUFFIX ".so")
  endif()
endfunction()
