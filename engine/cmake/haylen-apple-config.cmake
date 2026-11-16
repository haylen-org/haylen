# The CMake package of the engine SDK of Apple platforms. Xcode builds the apps of these platforms with `Haylen.xcframework` and `HaylenPlayer.xcframework` of the SDK, so the package defines only `haylen::native`, the headers of the native API and of nlohmann/json that the CMake projects of native libraries compile against.

get_filename_component(_haylen_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET haylen::native)
  add_library(haylen::native INTERFACE IMPORTED)
  set_target_properties(haylen::native PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_haylen_prefix}/include")
endif()

unset(_haylen_prefix)
