# Vendored mbedTLS 3.6 LTS (D-M3-4), desktop only: the Amiga build is plain HTTP (the include
# is guarded by if(NOT SEL_AMIGA) in src/library/CMakeLists.txt). Built from the unmodified
# upstream library sources with selecta_mbedtls_config.h as the configuration.
file(GLOB SEL_MBEDTLS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/library/*.c)
add_library(selecta_mbedtls STATIC ${SEL_MBEDTLS_SOURCES})
target_include_directories(selecta_mbedtls PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(selecta_mbedtls PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/library)
target_compile_definitions(selecta_mbedtls PUBLIC MBEDTLS_CONFIG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/selecta_mbedtls_config.h")
# Third-party source: no project warning flags (the -Werror build must not trip on it).
target_compile_options(selecta_mbedtls PRIVATE -w)
