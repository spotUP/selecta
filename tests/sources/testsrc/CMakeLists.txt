# Test-only source plugin (I517 phase 3, I1025): the reachability probe. A loadable module (sel_testsrc.dylib / .so)
# in SEL_TESTSRC_PLUGIN_DIR, never linked or registered statically: a selecta built with tests lists no testsrc unless a
# test puts that directory on $SELECTA_PLUGIN_PATH.
set(SEL_MODULE_OUTPUT_DIR "${SEL_TESTSRC_PLUGIN_DIR}")
sel_add_plugin(testsrc 7 MODULE testsrc.c)
target_include_directories(sel_module_testsrc PRIVATE "${PROJECT_SOURCE_DIR}/plugins/shared")
add_custom_target(sel_testsrc_module ALL DEPENDS sel_module_testsrc)
