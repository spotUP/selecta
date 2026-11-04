# I427c (design 2026-10-30-plugin-loader-sdk D7): install the plugin SDK and export the CMake package SelectaSDK.
#   install(DIRECTORY include/selecta)   the MIT headers, the only thing an out-of-tree plugin may include
#   Selecta::sdk                         INTERFACE target: include directory plus the module link rules of
#                                        cmake/selecta_plugin.cmake (only sel_plugin_entry is exported)
#   selecta_add_module()                 helper in SelectaSDKConfig.cmake: the sel_<name>.dylib / .so naming of
#                                        sel_add_plugin(... MODULE)
# A plugin built through the package never links selecta_core (one core per process, design D6).
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_library(selecta_sdk INTERFACE)
add_library(Selecta::sdk ALIAS selecta_sdk)
set_target_properties(selecta_sdk PROPERTIES EXPORT_NAME sdk)
target_include_directories(selecta_sdk INTERFACE
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(selecta_sdk INTERFACE c_std_99)
if(APPLE)
  target_link_options(selecta_sdk INTERFACE "LINKER:-exported_symbol,_sel_plugin_entry")
elseif(UNIX)
  target_link_options(selecta_sdk INTERFACE
    "$<BUILD_INTERFACE:LINKER:--version-script=${PROJECT_SOURCE_DIR}/cmake/sel_module.map>"
    "$<INSTALL_INTERFACE:LINKER:--version-script=$<INSTALL_PREFIX>/${CMAKE_INSTALL_DATADIR}/selecta/sel_module.map>")
endif()

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/selecta" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING PATTERN "*.h" PATTERN "*.def")
install(FILES "${PROJECT_SOURCE_DIR}/cmake/sel_module.map" DESTINATION "${CMAKE_INSTALL_DATADIR}/selecta")
install(TARGETS selecta_sdk EXPORT SelectaSDKTargets)
install(EXPORT SelectaSDKTargets NAMESPACE Selecta:: FILE SelectaSDKTargets.cmake
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SelectaSDK")
write_basic_package_version_file("${PROJECT_BINARY_DIR}/SelectaSDKConfigVersion.cmake"
  VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(FILES "${PROJECT_SOURCE_DIR}/cmake/SelectaSDKConfig.cmake" "${PROJECT_BINARY_DIR}/SelectaSDKConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/SelectaSDK")
