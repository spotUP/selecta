add_library(selecta_core STATIC
  core/version.c
  core/tag.c
  core/codec.c
  host/host.c
  host/mem.c
  host/log.c
  engine/spsc.c
  engine/engine.c)
target_include_directories(selecta_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

# One clock source per platform: C clock() on desktop, DateStamp on Amiga.
if(SEL_AMIGA)
  target_sources(selecta_core PRIVATE host/clock_amiga.c)
else()
  target_sources(selecta_core PRIVATE host/clock.c)
endif()
