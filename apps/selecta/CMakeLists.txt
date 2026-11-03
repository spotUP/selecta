add_executable(selecta main.c ../common/integration_host.c ../common/catalogue.c ../common/startup_error.c ../common/console_fallback.c ../common/ctlexec.c)
target_include_directories(selecta PRIVATE ../common)
target_include_directories(selecta PRIVATE ${PROJECT_SOURCE_DIR}/src/library)
target_link_libraries(selecta PRIVATE selecta_registry)
# The control socket is POSIX only; the Amiga's equivalent is the ARexx port (M4).
if(SEL_AMIGA)
  target_compile_definitions(selecta PRIVATE SEL_NO_CONTROL_SOCKET)
  target_include_directories(selecta PRIVATE ${PROJECT_SOURCE_DIR}/plugins/scripting/arexx)
  target_sources(selecta PRIVATE ../common/amiga_start.c ../common/stack_canary.c ../common/amiga_startup_error.c ../common/amiga_console.c)
else()
  target_sources(selecta PRIVATE ../common/ctlsock.c)
endif()

# I1023: on macOS the selecta target is a bundle, so a double-click on Selecta.app opens the desktop UI with no Terminal
# window. $<TARGET_FILE:selecta> then names Selecta.app/Contents/MacOS/Selecta, which is how every test finds the binary.
# The icon (apps/selecta/icon/selecta.svg) is rendered into Selecta.icns at build time by icon/make_icns.sh.
if(APPLE)
  set(SELECTA_ICNS "${CMAKE_CURRENT_BINARY_DIR}/Selecta.icns")
  add_custom_command(OUTPUT "${SELECTA_ICNS}"
    COMMAND /bin/sh "${CMAKE_CURRENT_SOURCE_DIR}/icon/make_icns.sh" "${CMAKE_CURRENT_SOURCE_DIR}/icon/selecta.svg" "${SELECTA_ICNS}"
    DEPENDS icon/selecta.svg icon/make_icns.sh VERBATIM)
  target_sources(selecta PRIVATE "${SELECTA_ICNS}")
  set_source_files_properties("${SELECTA_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(selecta PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME Selecta
    MACOSX_BUNDLE_GUI_IDENTIFIER se.spotup.selecta
    MACOSX_BUNDLE_ICON_FILE Selecta.icns
    OUTPUT_NAME Selecta)
endif()
