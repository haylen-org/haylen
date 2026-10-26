# The Vulkan device of Windows, Linux and Android. It opens the Vulkan loader of the system at run time through volk, so no Vulkan SDK is needed to build or run the engine, and allocates its memory through the Vulkan Memory Allocator.
target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Context.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Conversions.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DescriptorCache.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Device.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ImageLayouts.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Instance.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/LayoutCache.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/PreparedModules.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ReadbackQueue.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ReleaseQueue.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/RenderPassCache.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Swapchain.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/UploadRing.cpp"
)
target_link_libraries(haylen-gpu PRIVATE haylen-vulkan-memory-allocator)
target_compile_definitions(haylen-gpu PUBLIC HAYLEN_GPU_VULKAN)

# The GPU tests run the render passes of GPUs without dynamic rendering on every machine with Vulkan.
list(APPEND HAYLEN_GPU_TEST_SOURCES "${HAYLEN_ENGINE_DIR}/tests/gpu/vulkan/VulkanDeviceTests.cpp")
list(APPEND HAYLEN_GPU_TEST_LIBRARIES haylen-vulkan-memory-allocator)
