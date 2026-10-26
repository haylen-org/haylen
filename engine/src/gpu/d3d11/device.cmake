# The device of Direct3D 11, which "haylen-gpu" compiles when "HAYLEN_GRAPHICS_APIS" names "direct3D11".
include("${CMAKE_CURRENT_LIST_DIR}/../direct3d/direct3d.cmake")

target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Device.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DynamicRing.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FrameTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/StateObjects.cpp"
)
target_compile_definitions(haylen-gpu PUBLIC HAYLEN_GPU_DIRECT3D11)
target_link_libraries(haylen-gpu PRIVATE d3d11)
