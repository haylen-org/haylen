# The desktop part of the Native Demo plugin, a C library that the desktop player and the Windows and Linux apps load. make.py builds the target named after the plugin, native_demo, as a shared library and passes HAYLEN_INCLUDE_DIR with haylen/platform/native/HaylenNative.h.
cmake_minimum_required(VERSION 3.28...4.4)
project(native_demo LANGUAGES C)

find_package(Threads REQUIRED)

add_library(native_demo NativeDemo.c)
target_include_directories(native_demo PRIVATE "${HAYLEN_INCLUDE_DIR}")
target_link_libraries(native_demo PRIVATE Threads::Threads)
if(NOT WIN32 AND NOT APPLE)
  target_link_libraries(native_demo PRIVATE m)
endif()
set_target_properties(native_demo PROPERTIES C_STANDARD 11 C_VISIBILITY_PRESET hidden)
