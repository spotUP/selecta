# SNSF replayer (I705a, bbmp backend bsnes): Super Famicom sound sets (.snsf, .minisnsf with .snsflib) through the vendored
# bsnes core (third_party/bsnes, GPL-3.0, C++17 plus libco) and psflib (third_party/psflib, MIT) behind a RENDER adapter.
# GPL, so it only ever builds as a loadable module, and only when SELECTA_GPL_PLUGINS=ON; the core binaries never link it.
# Host only: the Amiga build has no GPL module path (exemption in ledger row I705, RULES 4a).
# Picture processor: the sfc/ppu unit holds the PPU wrapper plus the accurate PPU in one translation unit and is built
# whole; the wrapper runs the fast PPU (configuration Hacks/PPU/Fast, set by the bridge), whose unit sfc/ppu-fast is built too.
set(BSNES_DIR "${PROJECT_SOURCE_DIR}/third_party/bsnes")
set(PSFLIB_DIR "${PROJECT_SOURCE_DIR}/third_party/psflib")
find_package(ZLIB REQUIRED)
enable_language(CXX)

set(BSNES_SFC_SOURCES
  sfc/interface/interface.cpp sfc/system/system.cpp sfc/controller/controller.cpp sfc/cartridge/cartridge.cpp
  sfc/memory/memory.cpp sfc/cpu/cpu.cpp sfc/smp/smp.cpp sfc/dsp/dsp.cpp sfc/ppu/ppu.cpp sfc/ppu-fast/ppu.cpp
  sfc/expansion/expansion.cpp sfc/coprocessor/coprocessor.cpp sfc/slot/slot.cpp emulator/emulator.cpp
  processor/wdc65816/wdc65816.cpp processor/spc700/spc700.cpp processor/arm7tdmi/arm7tdmi.cpp)
list(TRANSFORM BSNES_SFC_SOURCES PREPEND "${BSNES_DIR}/bsnes/")

add_library(selecta_bsnes_core STATIC ${BSNES_SFC_SOURCES} "${BSNES_DIR}/libco/libco.c" bsnes_bridge.cpp
  "${PSFLIB_DIR}/psflib.c")
target_include_directories(selecta_bsnes_core SYSTEM PUBLIC "${BSNES_DIR}/bsnes" "${BSNES_DIR}" "${PSFLIB_DIR}")
target_include_directories(selecta_bsnes_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(selecta_bsnes_core PRIVATE -w -fvisibility=hidden)
target_link_libraries(selecta_bsnes_core PUBLIC ZLIB::ZLIB)
set_target_properties(selecta_bsnes_core PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS ON
  C_STANDARD 11 C_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)

set(SEL_MODULE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/modules")
sel_add_plugin(snsf 1 MODULE adapter.c THIRD_PARTY_INCLUDES "${PSFLIB_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(sel_module_snsf PRIVATE selecta_bsnes_core)
