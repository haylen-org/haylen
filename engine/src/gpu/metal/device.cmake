# The Metal device of macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS, in Objective-C++ with automatic reference counting.
set(haylen_gpu_metal_sources
  "${CMAKE_CURRENT_LIST_DIR}/Device.mm"
  "${CMAKE_CURRENT_LIST_DIR}/EncoderState.mm"
  "${CMAKE_CURRENT_LIST_DIR}/Formats.mm"
  "${CMAKE_CURRENT_LIST_DIR}/Graveyard.mm"
  "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.mm"
  "${CMAKE_CURRENT_LIST_DIR}/LibraryCache.mm"
  "${CMAKE_CURRENT_LIST_DIR}/Swapchain.mm"
  "${CMAKE_CURRENT_LIST_DIR}/UploadRing.mm"
)
set_source_files_properties(${haylen_gpu_metal_sources} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
target_sources(haylen-gpu PRIVATE ${haylen_gpu_metal_sources})
target_link_libraries(haylen-gpu PRIVATE "-framework Foundation" "-framework Metal" "-framework QuartzCore")
target_compile_definitions(haylen-gpu PUBLIC HAYLEN_GPU_METAL)
