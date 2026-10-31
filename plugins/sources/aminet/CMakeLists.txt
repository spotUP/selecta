# Aminet source plugin (row I405). Uses only the host's net and catalogue services; the songs are archives the
# engine unpacks at playback (archive.c), so the plugin itself needs no archive code.
sel_add_plugin(aminet 7 aminet.c aminet_sync.c aminet_net.c)
target_link_libraries(sel_plugin_aminet PUBLIC selecta_shared_source)
