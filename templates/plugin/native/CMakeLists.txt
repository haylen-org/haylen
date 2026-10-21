# The desktop part of the {{TITLE}} plugin, a C library that the desktop player and the Windows and Linux apps load. The tool `haylen.py` builds the target named after the plugin, `{{ID}}`, as a shared library and passes `HAYLEN_INCLUDE_DIR` with `haylen/platform/native/HaylenNative.h`.
cmake_minimum_required(VERSION 3.28...4.4)
project({{ID}} LANGUAGES C)

add_library({{ID}} {{NAME}}.c)
target_include_directories({{ID}} PRIVATE "${HAYLEN_INCLUDE_DIR}")
set_target_properties({{ID}} PROPERTIES C_STANDARD 11 C_VISIBILITY_PRESET hidden)
