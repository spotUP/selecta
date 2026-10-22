# selecta_core: the engine and everything under it (design doc section 2).
# Apps link this; plugins never do (they see include/selecta only).
add_library(selecta_core STATIC
  core/version.c
  core/tag.c
  core/codec.c)
target_include_directories(selecta_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
