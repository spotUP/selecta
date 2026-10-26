# ASMA source plugin (row I413; I517 phase 4). Uses only the host's net and catalogue services.
sel_add_plugin(asma 7 asma.c asma_sync.c asma_net.c)
target_link_libraries(sel_plugin_asma PUBLIC selecta_shared_source)
