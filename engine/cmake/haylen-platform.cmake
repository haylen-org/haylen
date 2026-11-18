if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

# Mac Catalyst builds through `haylen-catalyst.toolchain.cmake` compile for the `macabi` flavor of iOS, so the engine treats them as iOS.
if(CMAKE_CXX_COMPILER_TARGET MATCHES "-macabi$")
  set(HAYLEN_CATALYST ON)
else()
  set(HAYLEN_CATALYST OFF)
endif()

# Apps of visionOS run the iPhone and iPad API of UIKit, so the engine treats them as iOS too, and scripts see them as iOS apps.
if(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
  set(HAYLEN_VISIONOS ON)
else()
  set(HAYLEN_VISIONOS OFF)
endif()

# A port names its own platform, whatever the system of its toolchain is.
if(HAYLEN_PORT_PLATFORM)
  set(HAYLEN_PLATFORM "${HAYLEN_PORT_PLATFORM}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR HAYLEN_CATALYST OR HAYLEN_VISIONOS)
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

# The web, Android, iOS and tvOS build the GPU tests alone, which run in a page of a browser, on a device or an emulator, and on the simulators and Mac Catalyst.
if(HAYLEN_PLATFORM MATCHES "^(web|android|ios|tvos)$")
  set(HAYLEN_GPU_TESTS_ALONE ON)
else()
  set(HAYLEN_GPU_TESTS_ALONE OFF)
endif()

# The interfaces of the system that the engine compiles its system code against, which a port declares in `SYSTEM_API`: POSIX on Apple platforms, Linux and Android, Windows, and the browser, which has the files and the local time of POSIX and no native crashes or native libraries.
if(HAYLEN_PORT_PLATFORM)
  set(HAYLEN_SYSTEM_API "${HAYLEN_PORT_SYSTEM_API}")
elseif(EMSCRIPTEN)
  set(HAYLEN_SYSTEM_API "web")
elseif(WIN32)
  set(HAYLEN_SYSTEM_API "windows")
else()
  set(HAYLEN_SYSTEM_API "posix")
endif()

# What the platform allows apps to do, which a port declares in `CAPABILITIES`: Mac Catalyst starts no process of its own, iPhones, iPads, TVs and Apple Vision Pro neither start processes nor write code into memory, and a page does none of the three.
if(HAYLEN_PORT_PLATFORM)
  set(HAYLEN_CAPABILITIES ${HAYLEN_PORT_CAPABILITIES})
elseif(HAYLEN_PLATFORM STREQUAL "web")
  set(HAYLEN_CAPABILITIES "")
elseif(HAYLEN_CATALYST)
  set(HAYLEN_CAPABILITIES dynamicLibraries processes)
elseif(HAYLEN_PLATFORM MATCHES "^(ios|tvos)$")
  set(HAYLEN_CAPABILITIES dynamicLibraries)
else()
  set(HAYLEN_CAPABILITIES dynamicLibraries executableMemory processes)
endif()

# Whether the platform runs threads besides the one that plays the frames, which a page does not and a port declares in `THREADS`.
if(HAYLEN_PORT_PLATFORM)
  set(HAYLEN_THREADS ${HAYLEN_PORT_THREADS})
elseif(HAYLEN_PLATFORM STREQUAL "web")
  set(HAYLEN_THREADS OFF)
else()
  set(HAYLEN_THREADS ON)
endif()

# The function `haylen_add_app` runs in the folders of the apps, which do not see the variables of the engine folder.
set(HAYLEN_PLATFORM "${HAYLEN_PLATFORM}" CACHE INTERNAL "Platform the engine is built for.")
set(HAYLEN_DESKTOP "${HAYLEN_DESKTOP}" CACHE INTERNAL "Whether the platform is a desktop.")

# Lua is compiled as C++ by Varn, so a Lua error unwinds as a C++ exception and a yield jumps back to its resume with `_longjmp`, and the browser build uses the native exceptions and the native `longjmp` of WebAssembly that Varn builds with.
if(EMSCRIPTEN)
  add_compile_options(-fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
  add_link_options(-fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
endif()

message(STATUS "Haylen platform: \"${HAYLEN_PLATFORM}\"")
