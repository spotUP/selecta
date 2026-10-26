add_executable(selecta-cli main.c abi.c plugins.c play_null.c machine.c render.c session.c play.c library.c analyse.c folders.c fetch.c record.c devilbox.c lastfm.c mirror_fetch.c dryrun.c spsc.c
  ../common/integration_host.c ../common/catalogue.c)
target_include_directories(selecta-cli PRIVATE ../common)
target_link_libraries(selecta-cli PRIVATE selecta_registry)
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
endif()
