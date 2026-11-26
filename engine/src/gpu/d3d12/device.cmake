# The device of Direct3D 12, which "haylen-gpu" compiles when "HAYLEN_GRAPHICS_APIS" names "direct3D12".
include("${CMAKE_CURRENT_LIST_DIR}/../direct3d/direct3d.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../renderdoc/renderdoc.cmake")

target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/BufferArena.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/BufferReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DescriptorHeap.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DescriptorRing.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Device.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FrameTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/PixCapturer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ReadbackCopy.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/RootSignatures.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/StateDescriptions.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/UploadAllocator.cpp"
)
target_compile_definitions(haylen-gpu PUBLIC HAYLEN_GPU_DIRECT3D12)
target_link_libraries(haylen-gpu PRIVATE d3d12)

# The tests of the upload memory of the device on WARP, which every Windows machine has.
list(APPEND HAYLEN_GPU_TEST_SOURCES "${HAYLEN_ENGINE_DIR}/tests/gpu/d3d12/UploadAllocatorTests.cpp")
list(APPEND HAYLEN_GPU_TEST_LIBRARIES d3d12)
