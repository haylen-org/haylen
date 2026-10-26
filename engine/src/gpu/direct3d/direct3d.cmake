# The code that the devices of Direct3D 11 and 12 share, which the folder of either device adds once to "haylen-gpu": the factory, adapters and swapchains of DXGI, the shader compiler of Direct3D, which loads at run time, and the formats, readbacks and debug messages of both.
include_guard(GLOBAL)

target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Adapter.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DebugMessages.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Formats.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/HResult.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/MipLevels.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/PixelConverter.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ShaderCompiler.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Swapchain.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/WideText.cpp"
)
target_compile_definitions(haylen-gpu PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(haylen-gpu PRIVATE dxgi dxguid)
