if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

# Mac Catalyst builds through `haylen-catalyst.toolchain.cmake` compile for the `macabi` flavor of iOS, so the engine treats them as iOS.
if(CMAKE_CXX_COMPILER_TARGET MATCHES "-macabi$")
  set(HAYLEN_CATALYST ON)
else()
  set(HAYLEN_CATALYST OFF)
endif()

# A port names its own platform, whatever the system of its toolchain is.
if(HAYLEN_PORT_PLATFORM)
  set(HAYLEN_PLATFORM "${HAYLEN_PORT_PLATFORM}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR HAYLEN_CATALYST)
  set(HAYLEN_PLATFORM "ios")
elseif(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
  set(HAYLEN_PLATFORM "tvos")
elseif(ANDROID)
  set(HAYLEN_PLATFORM "android")
elseif(EMSCRIPTEN)
  set(HAYLEN_PLATFORM "web")
elseif(WIN32)
  set(HAYLEN_PLATFORM "windows")
elseif(APPLE)
  set(HAYLEN_PLATFORM "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(HAYLEN_PLATFORM "linux")
else()
  message(FATAL_ERROR "The platform \"${CMAKE_SYSTEM_NAME}\" is not supported.")
endif()

if(HAYLEN_PLATFORM MATCHES "^(macos|windows|linux)$")
  set(HAYLEN_DESKTOP ON)
else()
  set(HAYLEN_DESKTOP OFF)
endif()

# The function `haylen_add_app` runs in the folders of the apps, which do not see the variables of the engine folder.
set(HAYLEN_PLATFORM "${HAYLEN_PLATFORM}" CACHE INTERNAL "Platform the engine is built for.")
set(HAYLEN_DESKTOP "${HAYLEN_DESKTOP}" CACHE INTERNAL "Whether the platform is a desktop.")

# Lua is compiled as C++ by Varn, so a Lua error unwinds as a C++ exception and a yield jumps back to its resume with `_longjmp`, and the browser build uses the native exceptions and the native `longjmp` of WebAssembly that Varn builds with.
if(EMSCRIPTEN)
  add_compile_options(-fwasm-exceptions "SHELL:-s SUPPORT_LONGJMP=wasm")
  add_link_options(-fwasm-exceptions "SHELL:-s SUPPORT_LONGJMP=wasm")
endif()

message(STATUS "Haylen platform: \"${HAYLEN_PLATFORM}\"")
