add_executable(selecta-cli main.c abi.c plugins.c play_null.c machine.c render.c session.c play.c
  ../common/integration_host.c)
target_include_directories(selecta-cli PRIVATE ../common)
target_link_libraries(selecta-cli PRIVATE selecta_registry selecta_core)
# The control socket is POSIX only; the Amiga's equivalent is the ARexx port (M4).
if(SEL_AMIGA)
  target_compile_definitions(selecta-cli PRIVATE SEL_NO_CONTROL_SOCKET)
else()
  target_sources(selecta-cli PRIVATE ../common/ctlsock.c)
endif()
