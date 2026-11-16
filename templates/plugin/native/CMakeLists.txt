# The desktop part of the {{TITLE}} plugin, a C++ library that the desktop player and the Windows and Linux apps load. The tooling builds the target named after the plugin, `{{ID}}`, against the installed SDK of the engine, whose target `haylen::native` gives it `haylen/platform/native/HaylenNative.h` and nlohmann/json.
cmake_minimum_required(VERSION 3.28...4.4)
project({{ID}} LANGUAGES CXX)

find_package(haylen CONFIG REQUIRED)

add_library({{ID}} {{NAME}}.cpp {{NAME}}Plugin.cpp)
target_link_libraries({{ID}} PRIVATE haylen::native)
target_compile_features({{ID}} PRIVATE cxx_std_20)
set_target_properties({{ID}} PROPERTIES CXX_EXTENSIONS OFF CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
