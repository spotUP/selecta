# libxmp replayer (I697): libxmp (pinned in third_party/libxmp/REVISION), the engine bbmp uses for its xmp backend.
# libxmp is MIT with public-domain, BSD and ISC parts and no GPL file in the built set (see LICENSE), so the module is
# MIT and builds with the normal plugin set. A loadable module only; the core binaries never link it. Host only.
set(LIBXMP_DIR "${PROJECT_SOURCE_DIR}/third_party/libxmp")
include("${LIBXMP_DIR}/cmake/libxmp-sources.cmake")

set(XMP_SOURCES)
foreach(src ${LIBXMP_SRC_LIST} ${LIBXMP_SRC_LIST_PROWIZARD})
  list(APPEND XMP_SOURCES "${LIBXMP_DIR}/${src}")
endforeach()

add_library(xmp_engine STATIC ${XMP_SOURCES})
target_include_directories(xmp_engine PUBLIC "${LIBXMP_DIR}/include" PRIVATE "${LIBXMP_DIR}/src")
target_compile_definitions(xmp_engine PUBLIC LIBXMP_STATIC PRIVATE LIBXMP_NO_DEPACKERS _REENTRANT)
set_target_properties(xmp_engine PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
# Upstream code is not clean under the project's -Wall -Wextra -Werror -pedantic; third-party, so warnings are off.
target_compile_options(xmp_engine PRIVATE -w)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(xmp 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${LIBXMP_DIR}/include")
target_link_libraries(sel_module_xmp PRIVATE xmp_engine)
