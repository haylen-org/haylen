cmake_minimum_required(VERSION 3.28...4.4)

project(haylen_workspace LANGUAGES C CXX)

# Lua samples run from their package folders through `haylen.py run`, and the C++ sample is a CMake project of its own, so the workspace builds only the engine.
set(HAYLEN_BUILD_PLAYER ON CACHE BOOL "Build the \"haylen\" player.")
set(HAYLEN_BUILD_TESTS ON CACHE BOOL "Build the engine tests.")
set(HAYLEN_BUILD_BENCHMARKS ON CACHE BOOL "Build the engine benchmarks.")

# Tests are registered from the engine folder, so ctest needs testing enabled at the workspace root to find them.
enable_testing()
add_subdirectory(engine)
