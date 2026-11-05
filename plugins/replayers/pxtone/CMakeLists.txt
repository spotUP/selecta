# pxtone replayer (I702): Pixel's PiSTOL Collage songs (.ptcop, .pttune) through the pxtone play sample sources
# (third_party/pxtone). The licence is permissive (free use and modification, no permission or attribution needed), so this
# is MIT-compatible; it is a loadable module on the host. C++ engine: no Amiga module path (exemption in ledger row I702).
enable_language(CXX)
set(PX_DIR "${PROJECT_SOURCE_DIR}/third_party/pxtone")
file(GLOB PX_SOURCES "${PX_DIR}/*.cpp")
add_library(pxtone_engine STATIC EXCLUDE_FROM_ALL ${PX_SOURCES})
target_include_directories(pxtone_engine PUBLIC "${PX_DIR}/..")
target_compile_options(pxtone_engine PRIVATE -w -fvisibility=hidden)
set_target_properties(pxtone_engine PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 14)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(pxtone 1 MODULE adapter.c shim.cpp THIRD_PARTY_INCLUDES "${PX_DIR}/..")
target_link_libraries(sel_module_pxtone PRIVATE pxtone_engine ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
set_target_properties(sel_module_pxtone PROPERTIES CXX_STANDARD 14)
