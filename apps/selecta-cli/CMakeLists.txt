add_executable(selecta-cli main.c abi.c plugins.c play_null.c machine.c render.c session.c play.c library.c analyse.c folders.c fetch.c record.c devilbox.c mirror_fetch.c dryrun.c spsc.c
  ../common/integration_host.c ../common/catalogue.c)
target_include_directories(selecta-cli PRIVATE ../common)
target_link_libraries(selecta-cli PRIVATE selecta_registry)
if(SEL_AMIGA)
  target_compile_definitions(selecta-cli PRIVATE SEL_NO_ANALYSE)
endif()
# The control socket is POSIX only; the Amiga's equivalent is the ARexx port (M4).
if(SEL_AMIGA)
  target_compile_definitions(selecta-cli PRIVATE SEL_NO_CONTROL_SOCKET)
  target_sources(selecta-cli PRIVATE ../common/amiga_start.c)
else()
  target_sources(selecta-cli PRIVATE ../common/ctlsock.c)
endif()
