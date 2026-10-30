# radio-browser.info directory source plugin (row I725c). Uses only the host's net and catalogue services.
sel_add_plugin(radiobrowser 7 radiobrowser.c)
target_link_libraries(sel_plugin_radiobrowser PUBLIC selecta_shared_source)
