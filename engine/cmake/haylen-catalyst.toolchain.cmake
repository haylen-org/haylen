# Builds for Mac Catalyst, the UIKit variant of iOS that runs on macOS, which CMake has no system name for.
# Sources compile for the macabi flavor of iOS against the macOS SDK and its iOSSupport folder. Pass one architecture in CMAKE_OSX_ARCHITECTURES.

set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_OSX_SYSROOT macosx)

# The minimum is Mac Catalyst 16.4, which is macOS 13.3, the first macOS release whose C++ library formats floating point with std::format.
# It lives in the target triple, so the macOS deployment target stays empty and dependencies that default it add no conflicting flag.
set(HAYLEN_CATALYST_MINIMUM "16.4")
set(CMAKE_OSX_DEPLOYMENT_TARGET "" CACHE STRING "Mac Catalyst takes its minimum from the target triple.")

execute_process(COMMAND xcrun --sdk macosx --show-sdk-path OUTPUT_VARIABLE sdk OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
set(ios_support "${sdk}/System/iOSSupport")

foreach(language C CXX OBJC OBJCXX ASM)
  set(CMAKE_${language}_COMPILER_TARGET "${CMAKE_OSX_ARCHITECTURES}-apple-ios${HAYLEN_CATALYST_MINIMUM}-macabi")
  set(CMAKE_${language}_FLAGS_INIT "-iframework ${ios_support}/System/Library/Frameworks -isystem ${ios_support}/usr/include")
endforeach()
set(CMAKE_EXE_LINKER_FLAGS_INIT "-iframework ${ios_support}/System/Library/Frameworks -L${ios_support}/usr/lib")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")
