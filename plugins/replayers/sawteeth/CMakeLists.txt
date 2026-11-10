# Sawteeth replayer (I1242e): Sawteeth songs (.st, SWTD binary and SWTT text) through Jonas Norberg's original player library
# (third_party/sawteeth, MIT, unchanged). Float throughout (libm only), so SEL_RP_NEEDS_FLOAT is set: an Amiga without an FPU hides it.
# Host only for now: the Amiga plugin build has no C++ plugin yet (exemption in row I1242e).
enable_language(CXX)
set(SWT_DIR "${PROJECT_SOURCE_DIR}/third_party/sawteeth")
file(GLOB SWT_SOURCES CONFIGURE_DEPENDS "${SWT_DIR}/source/*.cpp")
add_library(sawteeth_engine STATIC EXCLUDE_FROM_ALL ${SWT_SOURCES})
target_include_directories(sawteeth_engine PUBLIC "${SWT_DIR}/headers")
# Upstream code is not clean under -Wall -Wextra -Werror -pedantic (multi-character constants, old C++); third-party, so warnings are off.
target_compile_options(sawteeth_engine PRIVATE -w -fvisibility=hidden)
set_target_properties(sawteeth_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 14)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(sawteeth 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${SWT_DIR}/headers")
target_link_libraries(sel_module_sawteeth PRIVATE sawteeth_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_sawteeth PROPERTIES CXX_STANDARD 14)
set_property(TARGET sel_module_sawteeth PROPERTY SEL_MODULE_MIT ON)
