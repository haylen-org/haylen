# The platform layer of SDL: SDL opens the window, runs the frames and reads the input, and the GPU devices of `haylen-gpu` draw the frames through the graphics API that starts first among the ones the app prefers.
target_sources(haylen-runtime PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/Events.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FrameTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/GlContext.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Host.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Keys.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Runtime.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Window.cpp"
)

# The application delegate of macOS is the runtime's own, so SDL leaves the menus, the quit requests and the events of plugins to it, and the application and scene delegate of UIKit derives from the one of SDL, which runs the app in its scene. The view controller of the window answers UIKit with the status bar of the app.
if(APPLE)
  set(haylen_sdl_apple_sources
    "${CMAKE_CURRENT_LIST_DIR}/apple/AppleApplication.mm"
    "${CMAKE_CURRENT_LIST_DIR}/apple/HaylenAppDelegate.mm"
    "${CMAKE_CURRENT_LIST_DIR}/apple/HaylenRootViewController.mm"
    "${CMAKE_CURRENT_LIST_DIR}/apple/HaylenSceneDelegate.mm"
  )
  set_source_files_properties(${haylen_sdl_apple_sources} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_sources(haylen-runtime PRIVATE ${haylen_sdl_apple_sources})
endif()

target_link_libraries(haylen-runtime PRIVATE SDL3::SDL3-static haylen-gpu)

# The tests of the platform layer, which the test suite of the engine runs: the translation of the events of SDL and the timing of the frames, which read only their own data.
set(HAYLEN_BACKEND_TEST_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/Events.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/FrameTimer.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/Keys.cpp"
  "${HAYLEN_ENGINE_DIR}/tests/platform/sdl/EventsTests.cpp"
  "${HAYLEN_ENGINE_DIR}/tests/platform/sdl/FrameTimerTests.cpp"
  CACHE INTERNAL "Sources of the tests of the backend.")
set(HAYLEN_BACKEND_TEST_LIBRARIES SDL3::SDL3-static CACHE INTERNAL "Libraries the tests of the backend link.")
