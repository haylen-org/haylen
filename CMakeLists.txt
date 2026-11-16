cmake_minimum_required(VERSION 3.28...4.4)

project(haylen-workspace LANGUAGES C CXX)

# Samples build against an engine SDK through `haylen app run` and the backend of their target, and the C++ sample is a CMake project of its own, so the workspace builds only the engine and its tools.
set(HAYLEN_BUILD_PLAYER ON CACHE BOOL "Build the \"haylen\" player.")
set(HAYLEN_BUILD_TESTS ON CACHE BOOL "Build the engine tests.")
set(HAYLEN_BUILD_BENCHMARKS ON CACHE BOOL "Build the engine benchmarks.")
set(HAYLEN_BUILD_TOOLS ON CACHE BOOL "Build the content tool \"haylen-content\".")

# Tests are registered from the engine folder, so ctest needs testing enabled at the workspace root to find them.
enable_testing()
add_subdirectory(engine)
