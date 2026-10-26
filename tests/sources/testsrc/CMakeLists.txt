# Test-only source plugin (I517 phase 3): the reachability probe. Added from the root CMakeLists only when
# SEL_BUILD_TESTS is on, before the registry list is generated.
sel_add_plugin(testsrc 7 testsrc.c)
