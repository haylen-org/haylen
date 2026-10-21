# The Sokol backend: `sokol_app` opens the window, runs the frames and holds the entry point, and `sokol_gfx` draws the frames through the graphics API of the build.
target_sources(haylen_runtime PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Events.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/GpuDevice.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Host.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Runtime.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Window.cpp"
)

# The GPU device names its GPU, reads render targets back and compiles shader sources ahead of their programs through Metal on Apple platforms, and names its GPU and reads render targets back through the graphics API the build chose elsewhere. The application delegates of Apple platforms derive from the ones of `sokol_app`.
if(APPLE)
  set(haylen_sokol_apple_sources
    "${CMAKE_CURRENT_LIST_DIR}/Adapter.mm"
    "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.mm"
    "${CMAKE_CURRENT_LIST_DIR}/Precompiler.mm"
    "${CMAKE_CURRENT_LIST_DIR}/apple/AppleClasses.mm"
    "${CMAKE_CURRENT_LIST_DIR}/apple/HaylenAppDelegate.mm"
    "${CMAKE_CURRENT_LIST_DIR}/apple/HaylenSceneDelegate.mm"
  )
  set_source_files_properties(${haylen_sokol_apple_sources} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_sources(haylen_runtime PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Implementation.m" ${haylen_sokol_apple_sources})
else()
  target_sources(haylen_runtime PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Adapter.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Implementation.c"
    "${CMAKE_CURRENT_LIST_DIR}/Precompiler.cpp"
  )
  # The readback of WebGPU calls the API of the port of WebGPU that the web players link, whose headers the module scan of CMake does not find.
  if(EMSCRIPTEN AND HAYLEN_GRAPHICS STREQUAL "WGPU")
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/ImageReadback.cpp" PROPERTIES COMPILE_OPTIONS "--use-port=emdawnwebgpu" CXX_SCAN_FOR_MODULES OFF)
  endif()
endif()

target_compile_definitions(haylen_runtime PRIVATE SOKOL_${HAYLEN_GRAPHICS})
target_link_libraries(haylen_runtime PRIVATE haylen_sokol_headers)

# The name of the GPU comes from the OpenGL library where the build draws with OpenGL.
if(HAYLEN_GRAPHICS STREQUAL "GLES3" AND ANDROID)
  target_link_libraries(haylen_runtime PRIVATE GLESv3)
elseif(HAYLEN_GRAPHICS STREQUAL "GLCORE" AND WIN32)
  target_link_libraries(haylen_runtime PRIVATE opengl32)
elseif(HAYLEN_GRAPHICS STREQUAL "GLCORE")
  find_package(OpenGL REQUIRED)
  target_link_libraries(haylen_runtime PRIVATE OpenGL::GL)
endif()

# The tests of the backend, which the test suite of the engine runs: the translation of the events of `sokol_app`, which reads only their fields.
set(HAYLEN_BACKEND_TEST_SOURCES "${CMAKE_CURRENT_LIST_DIR}/Events.cpp" "${HAYLEN_ENGINE_DIR}/tests/backends/sokol/EventsTests.cpp" CACHE INTERNAL "Sources of the tests of the backend.")
set(HAYLEN_BACKEND_TEST_LIBRARIES haylen_sokol_headers CACHE INTERNAL "Libraries the tests of the backend link.")
