# I989b: no test reads the user's real ~/.config/selecta (prefs.conf such as mix.stereo_width changes the frame hashes).
# sel_isolate_config() gives every test of the CURRENT directory its own empty XDG_CONFIG_HOME under the build tree
# (sel_config_dir honours it before HOME). A test that sets XDG_CONFIG_HOME itself keeps its own value.
# Call it last in every tests/**/CMakeLists.txt that defines tests (set_property(TEST) only sees the current directory on CMake 3.20).
function(sel_isolate_config)
  get_directory_property(dir_tests TESTS)
  foreach(t IN LISTS dir_tests)
    get_test_property(${t} ENVIRONMENT env)
    if(NOT env MATCHES "(^|;)XDG_CONFIG_HOME=")
      set(xdg "${PROJECT_BINARY_DIR}/tests/xdg/${t}")
      file(MAKE_DIRECTORY "${xdg}")
      set_property(TEST ${t} APPEND PROPERTY ENVIRONMENT "XDG_CONFIG_HOME=${xdg}")
    endif()
  endforeach()
endfunction()
