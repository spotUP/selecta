# HVSC source plugin (row I162; I517 phase 4). Uses only the host's net and catalogue services.
sel_add_plugin(hvsc 7 hvsc.c hvsc_sync.c hvsc_net.c)
target_link_libraries(sel_plugin_hvsc PUBLIC selecta_shared_source)
