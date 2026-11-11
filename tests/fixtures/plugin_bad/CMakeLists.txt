# T5 negative path (I1317): the broken descriptor is a test-only loadable module in this tree, found by the real
# selecta-cli through $SELECTA_PLUGIN_PATH. Before I1317 the test configured and built a second full tree
# (build/host/host-bad) with the plugin compiled in; serially under nice 19 that took over 25 min and the lint never ran.
set(SEL_BAD_PLUGIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/module")
set(SEL_MODULE_OUTPUT_DIR "${SEL_BAD_PLUGIN_DIR}")
sel_add_plugin(bad 2 MODULE module.c)

# EXPECT_PLUGINS: the registry-tags count plus this module (SEL_EXPECT_BAD_PLUGINS, tests/CMakeLists.txt).
add_test(NAME registry-tags-negative
  COMMAND "${CMAKE_COMMAND}" "-DCLI=$<TARGET_FILE:selecta-cli>" "-DDIR=${SEL_BAD_PLUGIN_DIR}"
    "-DEXPECT_PLUGINS=${SEL_EXPECT_BAD_PLUGINS}" -P "${CMAKE_CURRENT_SOURCE_DIR}/lint.cmake")

sel_isolate_config() # I989b
