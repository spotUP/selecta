# mui-build-split (MUI-P1-1): -DHAS_MAP=<0|1> -DHAS_UI=<0|1> -DAMIGA=<0|1> are the target facts at configure time.
if(NOT HAS_MAP)
  message(FATAL_ERROR "RED[mui-build-split]: target mui_map is missing")
endif()
# The descriptor is registered on every target (Johan 2026-10-27, MUI-P1-4): the MUI calls compile only on the Amiga.
if(NOT HAS_UI)
  message(FATAL_ERROR "RED[mui-build-split]: the target list lacks mui_ui")
endif()
