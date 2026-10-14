# The C++ app. It adds the engine from the engine folder of a Haylen checkout, which "HAYLEN_ENGINE_SOURCE" names and "haylen run-cpp" passes, or else from an installed SDK, and builds the app of this folder, which is also its package, for the platform it configures for.
cmake_minimum_required(VERSION 3.28...4.4)
project(App LANGUAGES C CXX)

# The Objective-C++ bridge handler of Apple platforms needs the compiler of Objective-C++, and the Apple parts of plugins may hold Swift.
if(APPLE)
  enable_language(OBJCXX Swift)
endif()

set(HAYLEN_ENGINE_SOURCE "" CACHE PATH "The engine folder of a Haylen checkout. Empty finds an installed SDK with find_package.")
if(HAYLEN_ENGINE_SOURCE)
  add_subdirectory("${HAYLEN_ENGINE_SOURCE}" haylen)
else()
  find_package(haylen REQUIRED)
endif()

# The native platform bridge handlers of the app: Objective-C on Apple platforms, the JavaScript of platform/web/app.js on the web and the Kotlin of platform/android on Android, while src/App.cpp answers in C++ on Windows and Linux.
set(sources src/App.cpp)
if(APPLE)
  list(APPEND sources src/AppBridge.mm)
  set_source_files_properties(src/AppBridge.mm PROPERTIES COMPILE_OPTIONS -fobjc-arc)
endif()

haylen_add_app(main CPP
  SOURCES ${sources}
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(EMSCRIPTEN)
  target_link_options(main PRIVATE "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/platform/web/app.js")
  set_property(TARGET main APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/platform/web/app.js")
endif()
