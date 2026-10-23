add_executable(selecta main.c ../common/ctlsock.c ../common/integration_host.c)
target_include_directories(selecta PRIVATE ../common)
target_link_libraries(selecta PRIVATE selecta_registry selecta_core)
