# The native code of the app, which the external native build of Gradle compiles for each ABI of the app against the engine SDK of that ABI: the library of the app that the activity loads, with the C++ sources of a C++ app or the Lua player with the bootstrap of a release, and the native libraries of the app and its plugins that Lua loads.
cmake_minimum_required(VERSION 3.28...4.4)
project(app LANGUAGES C CXX)

{{SDKS}}
set(haylen_DIR "${HAYLEN_SDK_${ANDROID_ABI}}")
find_package(haylen CONFIG REQUIRED)
{{APP}}{{LIBRARIES}}