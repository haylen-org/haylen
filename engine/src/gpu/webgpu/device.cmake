# The WebGPU device draws through the WebGPU C API of the port of Dawn for Emscripten, which the web builds compile against and link.
target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/BindGroupCache.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Connection.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Conversion.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Device.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FrameTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/PassTimeline.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Swapchain.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/UniformRing.cpp"
)
target_compile_definitions(haylen-gpu PUBLIC HAYLEN_GPU_WEBGPU)
target_compile_options(haylen-gpu PUBLIC --use-port=emdawnwebgpu)
target_link_options(haylen-gpu INTERFACE --use-port=emdawnwebgpu)

# The headers of the port come with the compile option, where the module scan of CMake does not look for them.
set_target_properties(haylen-gpu PROPERTIES CXX_SCAN_FOR_MODULES OFF)

# The GPU tests of the web check how the timeline of the device turns timestamps into the time of a frame.
list(APPEND HAYLEN_GPU_TEST_SOURCES "${HAYLEN_ENGINE_DIR}/tests/gpu/webgpu/PassTimelineTests.cpp")
