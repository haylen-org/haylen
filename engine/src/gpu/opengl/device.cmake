# The OpenGL device, which draws through OpenGL 4.3 core, OpenGL ES 3.0 or WebGL2, the APIs "openGl", "openGlEs" and "webGl2" of the build. It loads the entry points it calls through the context of the platform, so it links no OpenGL library.
include_guard(GLOBAL)

target_sources(haylen-gpu PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Conversions.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/DebugOutput.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Device.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FramebufferCache.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Gl.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/GpuTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/PixelReadback.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/ShaderProgram.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/StateCache.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/UploadRing.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/VertexArray.cpp"
)

foreach(api IN ITEMS openGl openGlEs webGl2)
  if(api IN_LIST HAYLEN_GRAPHICS_APIS)
    string(TOUPPER "${api}" definition)
    target_compile_definitions(haylen-gpu PUBLIC "HAYLEN_GPU_${definition}")
  endif()
endforeach()

# The tests of the device draw through an offscreen context of EGL on Linux and Android and of WGL on Windows, and the web runs them in a browser.
if(NOT EMSCRIPTEN)
  list(APPEND HAYLEN_GPU_TEST_SOURCES
    "${HAYLEN_ENGINE_DIR}/tests/gpu/opengl/DeviceTests.cpp"
    "${HAYLEN_ENGINE_DIR}/tests/gpu/opengl/ShaderProgramTests.cpp"
    "${HAYLEN_ENGINE_DIR}/tests/gpu/opengl/StateCacheTests.cpp"
    "${HAYLEN_ENGINE_DIR}/tests/gpu/opengl/UploadRingTests.cpp"
  )
  if(WIN32)
    list(APPEND HAYLEN_GPU_TEST_SOURCES "${HAYLEN_ENGINE_DIR}/tests/support/windows/OffscreenGlContext.cpp")
    list(APPEND HAYLEN_GPU_TEST_LIBRARIES opengl32)
  else()
    list(APPEND HAYLEN_GPU_TEST_SOURCES "${HAYLEN_ENGINE_DIR}/tests/support/OffscreenGlContext.cpp")
    list(APPEND HAYLEN_GPU_TEST_LIBRARIES EGL)
  endif()
endif()

# The web players reach the functions of WebGL2 through the address of each function, which the context of the page hands over.
if(EMSCRIPTEN)
  target_link_options(haylen-gpu INTERFACE "SHELL:-s MIN_WEBGL_VERSION=2" "SHELL:-s MAX_WEBGL_VERSION=2" "SHELL:-s GL_ENABLE_GET_PROC_ADDRESS=1")
endif()
