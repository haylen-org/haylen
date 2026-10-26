# The code that the devices of Direct3D 11 and 12 share, which the folder of either device adds once to "haylen-gpu": the factory, adapters and swapchains of DXGI, the shader compiler of Direct3D, which loads at run time, with the threads of its compilations, the formats, readbacks and debug messages of both, and their tests.
include_guard(GLOBAL)

target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Adapter.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/CompileQueue.cpp"
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

# The tests of both devices on WARP and in windows, which run on every Windows machine.
list(APPEND HAYLEN_GPU_TEST_SOURCES "${HAYLEN_ENGINE_DIR}/tests/gpu/direct3d/Direct3DDeviceTests.cpp")
list(APPEND HAYLEN_GPU_TEST_LIBRARIES dxgi dxguid)
