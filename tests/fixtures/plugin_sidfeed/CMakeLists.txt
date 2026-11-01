# Fixture module for the desktop viz test (I434j3): a HW-kind replayer that answers the SID register feed
# (SEL_RP_VIZ_SID) like the playsid adapter does, with a stub in place of playsid.library. dui-viz loads it through
# $SELECTA_PLUGIN_PATH and plays a *.selsid file, so the engine's HW viz path fills sid_chips on the host.
set(SEL_SIDFEED_DIR "${CMAKE_CURRENT_BINARY_DIR}/mod")
set(SEL_MODULE_OUTPUT_DIR "${SEL_SIDFEED_DIR}")
sel_add_plugin(sidfeed 1 MODULE module.c)
