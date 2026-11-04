# SNDH source plugin (row I414; I517 phase 4). Uses only the host's net and catalogue services.
sel_add_plugin(sndh_source 7 sndh.c sndh_sync.c sndh_net.c)
target_link_libraries(sel_plugin_sndh_source PUBLIC selecta_shared_source selecta_shared_archive)
