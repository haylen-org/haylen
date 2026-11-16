# The native libraries of the test project, built from the C++ sources of the test library that the engine tests call, whose C entry points keep the C interface of native libraries tested. The tooling builds each target for every platform the `native` section of `haylen.json` lists, as a shared or a static library by `BUILD_SHARED_LIBS`, against the installed SDK of the engine, whose target `haylen::native` gives it `haylen/platform/native/HaylenNative.h` and nlohmann/json.
cmake_minimum_required(VERSION 3.28...4.4)
project(native-test LANGUAGES CXX)

find_package(haylen CONFIG REQUIRED)
find_package(Threads REQUIRED)

set(folder "${CMAKE_CURRENT_LIST_DIR}/../../../../../engine/tests/native")
set(sources "${folder}/NativeTest.cpp" "${folder}/NativeTestLibrary.cpp" "${folder}/NativeTestValues.cpp")
add_library(native-test ${sources})
add_library(native-test-static ${sources})
target_compile_definitions(native-test PRIVATE NATIVE_TEST_ORIGIN="dynamic")
target_compile_definitions(native-test-static PRIVATE NATIVE_TEST_ORIGIN="static")

foreach(target native-test native-test-static)
  target_link_libraries(${target} PRIVATE haylen::native Threads::Threads)
  target_compile_features(${target} PRIVATE cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
endforeach()
