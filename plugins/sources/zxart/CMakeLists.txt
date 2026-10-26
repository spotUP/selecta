# ZX-Art source plugin (row I415; I517 phase 3). Uses only the host's net and catalogue services.
sel_add_plugin(zxart 7 zxart.c zxart_sync.c zxart_net.c)
target_link_libraries(sel_plugin_zxart PUBLIC selecta_shared_source)
