# Out-of-tree Selecta replayer (I427c, design 2026-10-30-plugin-loader-sdk D7). Built only through the installed SDK:
#   cmake -S examples/plugin-sine -B build -DCMAKE_PREFIX_PATH=<sdk prefix> && cmake --build build
# The module lands in build/modules; put that folder on $SELECTA_PLUGIN_PATH.
cmake_minimum_required(VERSION 3.20)
project(selecta_plugin_sine LANGUAGES C)
find_package(SelectaSDK REQUIRED)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")
selecta_add_module(sine sine.c)
