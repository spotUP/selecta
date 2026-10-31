# Podcast streaming source plugin (row I725f). Uses only the host net and catalogue services.
sel_add_plugin(podcast 7 podcast.c)
target_link_libraries(sel_plugin_podcast PUBLIC selecta_shared_source)
