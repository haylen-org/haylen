# The native libraries of the test project, built from the source that the engine tests call. The script `haylen.py` builds each target for every platform the `native` section of `app.json` lists, as a shared or a static library by `BUILD_SHARED_LIBS`, and passes `HAYLEN_INCLUDE_DIR` with `haylen/platform/native/HaylenNative.h`.
cmake_minimum_required(VERSION 3.28...4.4)
project(native_test LANGUAGES C)

set(source "${CMAKE_CURRENT_LIST_DIR}/../../../../../engine/tests/native/NativeTest.c")
find_package(Threads REQUIRED)

add_library(native_test "${source}")
add_library(native_test_static "${source}")
target_compile_definitions(native_test_static PRIVATE NATIVE_TEST_ORIGIN="static")

foreach(target native_test native_test_static)
  target_include_directories(${target} PRIVATE "${HAYLEN_INCLUDE_DIR}")
  target_link_libraries(${target} PRIVATE Threads::Threads)
  set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden)
endforeach()
