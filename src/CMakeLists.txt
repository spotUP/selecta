add_library(selecta_core STATIC
  core/version.c
  core/tag.c
  core/codec.c
  core/machine.c
  host/host.c
  host/mem.c
  host/log.c
  engine/spsc.c
  engine/engine.c)
target_include_directories(selecta_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# One clock source and one machine probe per platform. OS headers live only
# under src/core/<os>/ (RULES.md rule 3): C clock() plus sysctl (macOS) or
# /proc (Linux) on desktop, DateStamp and exec AttnFlags on Amiga.
target_include_directories(selecta_core PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/core ${CMAKE_CURRENT_SOURCE_DIR}/host)
if(SEL_AMIGA)
  target_sources(selecta_core PRIVATE core/amiga/clock.c core/amiga/machine.c)
elseif(APPLE)
  target_sources(selecta_core PRIVATE host/clock.c core/macos/machine.c)
else()
  target_sources(selecta_core PRIVATE host/clock.c core/linux/machine.c)
endif()
