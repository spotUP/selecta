# Building and installing a module

## Build against the installed SDK

The Selecta build installs headers and the CMake package `SelectaSDK` (cmake/selecta_sdk.cmake):

```
cmake --install <selecta-build> --prefix <prefix>
```

An out-of-tree plugin then needs only this (examples/plugin-sine/CMakeLists.txt):

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_plugin C)
find_package(SelectaSDK REQUIRED)
selecta_add_module(myplugin myplugin.c)
```

Configure with `-DCMAKE_PREFIX_PATH=<prefix>`. `Selecta::sdk` is an INTERFACE target carrying the include path, C99, and the
link flags that export only `sel_plugin_entry`. `selecta_add_module(name sources...)` (cmake/SelectaSDKConfig.cmake) builds
a MODULE library named `sel_<name>.dylib` on macOS and `sel_<name>.so` elsewhere. These are the same rules the built-in
modules use in cmake/selecta_plugin.cmake (macOS: `-exported_symbol _sel_plugin_entry`; elsewhere a version script,
cmake/sel_module.map).

Do not link `selecta_core`. One shared core exists per process; a second copy inside a module would split its state.

## Search path (src/engine/loader_dl.c)

Files named `sel_*.dylib` or `sel_*.so` are loaded from, in order:

1. each folder of `$SELECTA_PLUGIN_PATH` (colon separated),
2. the app plugin folder (`Contents/PlugIns` in a macOS bundle, else `plugins` next to the executable),
3. the user plugin folder, `plugins` inside the Selecta config folder.

`selecta-cli plugins` lists each plugin with its `origin=` path; use it to check that a module loaded.

## Licensing

Headers in include/selecta are MIT. A module of any licence may include them. A GPL module includes only include/selecta and
its own vendored code, and ships as a separate file with its own COPYING; the core binaries never contain GPL code. The in-tree
GPL modules build by default (`SELECTA_GPL_PLUGINS`, default ON, I1075). A desktop dev configure with it OFF stops with an
error; `-DSELECTA_RELEASE_MIT_ONLY=ON` is the only way to build a desktop app without them (I1218b).
