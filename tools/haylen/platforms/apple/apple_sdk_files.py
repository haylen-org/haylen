class AppleSdkFiles:
    """The names under which the Apple engine port lists the shared files of its SDK in `haylen-sdk.json`, which the Apple backend reads, and the names of the libraries of each slice.

    - `framework`: `Haylen.xcframework`, the engine and the runtime of every slice with the public headers.
    - `lua`: `HaylenLua.xcframework`, the Lua layer and the Lua player of every slice, which only the apps that run Lua link. The link takes the player only when nothing else defines `haylen::core::Application::create`, so every app defines it once.
    - `settings`: `haylen-apple.json`, the system frameworks that the engine links on each platform and the preprocessor definitions that C++ sources compile with against the headers.
    - `privacy`: the privacy manifest of the engine.
    - `swiftHelpers`: the Swift helpers of the bridge, which every app compiles.
    - `include`: the public headers, and `cmake`: the CMake package whose `haylen::native` native libraries compile against.
    """

    FRAMEWORK = "framework"
    LUA = "lua"
    SETTINGS = "settings"
    PRIVACY = "privacy"
    SWIFT_HELPERS = "swiftHelpers"
    INCLUDE = "include"
    CMAKE = "cmake"

    FRAMEWORK_NAME = "Haylen.xcframework"
    LUA_NAME = "HaylenLua.xcframework"
    SETTINGS_NAME = "haylen-apple.json"
    PRIVACY_NAME = "PrivacyInfo.xcprivacy"
    SWIFT_HELPERS_NAME = "HaylenBridgeAsync.swift"
    # The library of a slice in each framework.
    LIBRARY_NAME = "libhaylen.a"
    LUA_LIBRARY_NAME = "libhaylen-lua.a"
