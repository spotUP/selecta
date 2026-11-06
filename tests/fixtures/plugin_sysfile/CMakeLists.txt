# I710ga6: the loadable test-only replayer sysfile_probe (one synthetic system file). Builds with the tests only; the
# directory is one $SELECTA_PLUGIN_PATH entry in tests/net/sysfile_desktop.cmake.
set(SEL_SYSFILE_PROBE_DIR "${CMAKE_CURRENT_BINARY_DIR}/module")
set(SEL_SYSFILE_PROBE_DIR "${SEL_SYSFILE_PROBE_DIR}" PARENT_SCOPE)
set(SEL_MODULE_OUTPUT_DIR "${SEL_SYSFILE_PROBE_DIR}")
sel_add_plugin(sysfile_probe 1 MODULE module.c)
