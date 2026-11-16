# The native libraries of the test project, built from the C source that the engine tests call, which keeps the C interface of `HaylenNativeApi` tested from C. The tooling builds each target for every platform the `native` section of `haylen.json` lists, as a shared or a static library by `BUILD_SHARED_LIBS`, against the installed SDK of the engine, whose target `haylen::native` gives it `haylen/platform/native/HaylenNative.h`.
cmake_minimum_required(VERSION 3.28...4.4)
project(native-test LANGUAGES C)

find_package(haylen CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(source "${CMAKE_CURRENT_LIST_DIR}/../../../../../engine/tests/native/NativeTest.c")
add_library(native-test "${source}")
add_library(native-test-static "${source}")
target_compile_definitions(native-test-static PRIVATE NATIVE_TEST_ORIGIN="static")

foreach(target native-test native-test-static)
  target_link_libraries(${target} PRIVATE haylen::native Threads::Threads)
  set_target_properties(${target} PROPERTIES C_VISIBILITY_PRESET hidden)
endforeach()
