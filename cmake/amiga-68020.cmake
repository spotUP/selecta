# Cross toolchain: bebbo amiga-gcc, 68020 baseline, libnix (Kickstart 2.0+).
# Usage: cmake -S . -B build/amiga -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/amiga-68020.cmake
# Root search order: $AMIGA_GCC_ROOT, ~/opt/amiga, /opt/amiga.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)
set(SEL_AMIGA 1)

set(AMIGA_GCC_ROOT "$ENV{AMIGA_GCC_ROOT}" CACHE PATH "bebbo amiga-gcc install root")
if(NOT AMIGA_GCC_ROOT)
  foreach(cand "$ENV{HOME}/opt/amiga" "/opt/amiga")
    if(EXISTS "${cand}/bin/m68k-amigaos-gcc")
      set(AMIGA_GCC_ROOT "${cand}" CACHE PATH "bebbo amiga-gcc install root" FORCE)
      break()
    endif()
  endforeach()
endif()
if(NOT EXISTS "${AMIGA_GCC_ROOT}/bin/m68k-amigaos-gcc")
  message(FATAL_ERROR "m68k-amigaos-gcc not found under $AMIGA_GCC_ROOT, ~/opt/amiga or /opt/amiga; set AMIGA_GCC_ROOT")
endif()

set(CMAKE_C_COMPILER "${AMIGA_GCC_ROOT}/bin/m68k-amigaos-gcc")
set(CMAKE_C_FLAGS_INIT "-m68020 -mcrt=nix20 -fomit-frame-pointer")
# build/amiga is an optimised Release build (-O2, no -g); build/amiga-dbg passes -DCMAKE_BUILD_TYPE=Debug.
set(CMAKE_C_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Release default, Debug for build/amiga-dbg)" FORCE)
endif()
set(CMAKE_EXE_LINKER_FLAGS_INIT "-m68020 -mcrt=nix20")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH "${AMIGA_GCC_ROOT}/m68k-amigaos")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_EXECUTABLE_SUFFIX "")
