# SunVox library replayer (I706b): .sunvox songs of every version through the official precompiled SunVox Library
# (warmplace.ru, MIT licence + "Powered by SunVox" credit). Desktop only: a closed binary build has no Amiga or web
# variant, those keep plugins/replayers/sunvox (I706a, BSD 2009 engine). The module dlopen()s the library from
# <module folder>/sunvox_lib/, so nothing links against it and a missing library only makes the probe say NO.
#
# The binary is not committed: configure downloads sunvox_lib-<ver>.zip (sha256 pinned, same idea as
# tests/m3/fetch_fixture.sh) into the build tree and stages the library for this machine's CPU plus the licence texts
# (the licence demands every TXT of the zip's docs/license folder travels with the product) into
# ${CMAKE_BINARY_DIR}/modules/sunvox_lib/. A failed download warns and builds the module without the library.
set(SVL_VERSION "2.1.4d")
set(SVL_URL "https://warmplace.ru/soft/sunvox/sunvox_lib-${SVL_VERSION}.zip")
set(SVL_SHA256 "abe851de9d65a10e06673bf33257154591a2ff63ad2bb298488a5941cc5f4057")
set(SVL_CACHE "${CMAKE_BINARY_DIR}/sunvox_lib_cache")
set(SVL_STAGE "${CMAKE_BINARY_DIR}/modules/sunvox_lib")
set(SVL_ZIP "${SVL_CACHE}/sunvox_lib-${SVL_VERSION}.zip")

if(APPLE)
  set(SVL_OS macos)
  set(SVL_FILE sunvox.dylib)
else()
  set(SVL_OS linux)
  set(SVL_FILE sunvox.so)
endif()
if(CMAKE_OSX_ARCHITECTURES)
  list(GET CMAKE_OSX_ARCHITECTURES 0 SVL_ARCH)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
  set(SVL_ARCH arm64)
else()
  set(SVL_ARCH x86_64)
endif()

if(NOT EXISTS "${SVL_STAGE}/${SVL_FILE}")
  file(MAKE_DIRECTORY "${SVL_CACHE}")
  if(NOT EXISTS "${SVL_ZIP}")
    file(DOWNLOAD "${SVL_URL}" "${SVL_ZIP}" EXPECTED_HASH SHA256=${SVL_SHA256} TIMEOUT 120 STATUS svl_dl)
    list(GET svl_dl 0 svl_dl_rc)
    if(NOT svl_dl_rc EQUAL 0)
      file(REMOVE "${SVL_ZIP}")
      message(WARNING "sunvox_lib: download failed (${svl_dl}); module built without the library, probe will say NO")
    endif()
  endif()
  if(EXISTS "${SVL_ZIP}")
    file(ARCHIVE_EXTRACT INPUT "${SVL_ZIP}" DESTINATION "${SVL_CACHE}/x")
    set(svl_root "${SVL_CACHE}/x/sunvox_lib")
    file(MAKE_DIRECTORY "${SVL_STAGE}")
    file(COPY "${svl_root}/sunvox_lib/${SVL_OS}/lib_${SVL_ARCH}/${SVL_FILE}" DESTINATION "${SVL_STAGE}")
    file(GLOB svl_txt "${svl_root}/sunvox_lib/docs/license/*.txt")
    file(COPY ${svl_txt} DESTINATION "${SVL_STAGE}")
    file(COPY "${svl_root}/lib_sunvox/LICENSE" DESTINATION "${SVL_STAGE}")
    file(RENAME "${SVL_STAGE}/LICENSE" "${SVL_STAGE}/LICENSE-sunvox_lib-MIT")
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/CREDITS.txt" DESTINATION "${SVL_STAGE}")
    file(REMOVE_RECURSE "${SVL_CACHE}/x")
  endif()
endif()

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(sunvox_lib 1 MODULE adapter.c)
target_link_libraries(sel_module_sunvox_lib PRIVATE ${CMAKE_DL_LIBS})
