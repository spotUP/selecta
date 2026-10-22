add_executable(selecta-cli main.c abi.c plugins.c play_null.c machine.c render.c session.c play.c)
target_link_libraries(selecta-cli PRIVATE selecta_registry selecta_core)
