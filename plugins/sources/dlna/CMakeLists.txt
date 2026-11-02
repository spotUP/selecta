# DLNA / UPnP MediaServer library source plugin (row I894a3). Discovery and ContentDirectory Browse live in
# plugins/shared/upnp; this plugin walks the tree into the catalogue and hands the res URL to the stream path.
# selecta_shared_upnp has no Amiga socket backend yet (POSIX only since 11979d80); the Amiga build of this plugin
# waits on row I1004 (upnp socket shim on src/core/amiga/net.c). RULES 4a exemption until then.
if(SEL_AMIGA)
  return()
endif()
sel_add_plugin(dlna 7 dlna.c)
target_link_libraries(sel_plugin_dlna PUBLIC selecta_shared_source selecta_shared_upnp)
