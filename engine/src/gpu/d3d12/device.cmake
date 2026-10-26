# The device of Direct3D 12, which "haylen-gpu" compiles when "HAYLEN_GRAPHICS_APIS" names "direct3D12".
include("${CMAKE_CURRENT_LIST_DIR}/../direct3d/direct3d.cmake")

target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/BufferArena.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DescriptorHeap.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DescriptorRing.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Device.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FrameTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/RootSignatures.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/StateDescriptions.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/UploadAllocator.cpp"
)
target_compile_definitions(haylen-gpu PUBLIC HAYLEN_GPU_DIRECT3D12)
target_link_libraries(haylen-gpu PRIVATE d3d12)
