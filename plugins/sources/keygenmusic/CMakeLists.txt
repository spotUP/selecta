# Keygen Music source plugin (row I416; I517 phase 4). Uses only the host's net and catalogue services.
sel_add_plugin(keygenmusic 7 keygenmusic.c keygenmusic_sync.c keygenmusic_net.c)
target_link_libraries(sel_plugin_keygenmusic PUBLIC selecta_shared_source)
