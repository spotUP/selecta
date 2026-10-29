# I691c2 layering check: selecta_library must not link selecta_gui (the library is below the GUI).
# Run: cmake -DLIBRARY_CMAKE=<src/library/CMakeLists.txt> -P no_gui_link.cmake
file(READ "${LIBRARY_CMAKE}" text)
string(REGEX REPLACE "#[^\n]*" "" code "${text}") # comments may name the GUI
string(REGEX MATCHALL "target_link_libraries\\(selecta_library[^)]*\\)" links "${code}")
if(NOT links)
  message(FATAL_ERROR "no target_link_libraries(selecta_library ...) found in ${LIBRARY_CMAKE}")
endif()
foreach(l ${links})
  if(l MATCHES "selecta_gui")
    message(FATAL_ERROR "selecta_library links selecta_gui: ${l}")
  endif()
endforeach()
message(STATUS "no_gui_link_ok")
