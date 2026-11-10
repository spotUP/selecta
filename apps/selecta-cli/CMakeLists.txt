add_executable(selecta-cli main.c abi.c plugins.c play_null.c machine.c render.c session.c play.c library.c analyse.c folders.c library_bench.c fetch.c support.c record.c devilbox.c lastfm.c mirror_fetch.c dryrun.c spsc.c
  ../common/integration_host.c ../common/catalogue.c)
target_include_directories(selecta-cli PRIVATE ../common)
target_link_libraries(selecta-cli PRIVATE selecta_registry_cli)

# I1242b: the CLI carries its modules in <exe dir>/plugins, the folder sel_app_plugin_dir (src/core/config_dir.c) gives the loader for a
# program that is not a macOS bundle, so a CLI run from the build tree or from wherever it is installed finds every built-in and
# standalone module with no SELECTA_PLUGIN_PATH. Same module list and data folders as the app bundle (apps/selecta/CMakeLists.txt).
if(NOT SEL_AMIGA AND SELECTA_PLUGIN_LINK STREQUAL "MODULE")
  sel_bundled_modules(_sel_cli_mods _sel_cli_left_out)
  # The list the CLI really carries, for tests/CMakeLists.txt (registry-tags counts it; later-added test fixtures are not in it).
  set_property(GLOBAL PROPERTY SEL_CLI_MODULES ${_sel_cli_mods})
  set(_sel_cli_plugins_dir "$<TARGET_FILE_DIR:selecta-cli>/plugins")
  add_custom_target(selecta_cli_modules
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_sel_cli_plugins_dir}"
    VERBATIM)
  foreach(_m IN LISTS _sel_cli_mods)
    add_custom_command(TARGET selecta_cli_modules POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_m}>" "${_sel_cli_plugins_dir}/"
      VERBATIM)
    get_target_property(_sel_data_dir ${_m} SEL_MODULE_DATA_DIR)
    if(_sel_data_dir)
      get_filename_component(_sel_data_name "${_sel_data_dir}" NAME)
      add_custom_command(TARGET selecta_cli_modules POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${_sel_data_dir}" "${_sel_cli_plugins_dir}/${_sel_data_name}"
        VERBATIM)
    endif()
  endforeach()
  add_dependencies(selecta_cli_modules ${_sel_cli_mods})
  add_dependencies(selecta-cli selecta_cli_modules)
endif()
if(SEL_AMIGA)
  target_compile_definitions(selecta-cli PRIVATE SEL_NO_ANALYSE)
endif()
# The control socket is POSIX only; the Amiga's equivalent is the ARexx port (M4).
if(SEL_AMIGA)
  target_compile_definitions(selecta-cli PRIVATE SEL_NO_CONTROL_SOCKET)
  target_sources(selecta-cli PRIVATE ../common/amiga_start.c ../common/stack_canary.c)
else()
  target_sources(selecta-cli PRIVATE ../common/ctlsock.c)
endif()

# Regression test program for amiga-library-opens-on-low-file-descriptor (tests/amiga/run_lowfd.cmake).
if(SEL_AMIGA)
  add_executable(selecta-lowfd-test ../../tests/amiga/lowfd_test.c ../common/amiga_start.c ../common/stack_canary.c)
  target_include_directories(selecta-lowfd-test PRIVATE ../common)
  target_link_libraries(selecta-lowfd-test PRIVATE selecta_library selecta_registry)
  # I225 (tests/amiga/run_dbcreate.cmake): the wraps stand in for libnix behaviour that vamos does not have.
  add_executable(selecta-dbcreate-test ../../tests/amiga/dbcreate_test.c ../common/amiga_start.c ../common/stack_canary.c)
  target_include_directories(selecta-dbcreate-test PRIVATE ../common)
  target_link_libraries(selecta-dbcreate-test PRIVATE selecta_library selecta_registry)
  target_link_options(selecta-dbcreate-test PRIVATE -Wl,--wrap=lseek -Wl,--wrap=ftruncate)
  # I376 (tests/amiga/run_stack_canary.cmake): overflows a small swapped stack on purpose.
  add_executable(selecta-stack-canary-test ../../tests/amiga/stack_canary_test.c ../common/amiga_start.c ../common/stack_canary.c)
  target_include_directories(selecta-stack-canary-test PRIVATE ../common)
endif()
