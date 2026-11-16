class AppleSdkFiles:
    """The names under which the Apple engine port lists the shared files of its SDK in `haylen-sdk.json`, which the Apple backend reads, and the names of the files of each slice.

    - `framework`: `Haylen.xcframework`, the engine and the runtime of every slice with the public headers.
    - `player`: `HaylenPlayer.xcframework`, the Lua player of every slice, which only Lua apps link, so every app defines `haylen::core::Application::create` once.
    - `settings`: `haylen-apple.json`, the system frameworks that the engine links on each platform and the preprocessor definitions that C++ sources compile with against the headers.
    - `privacy`: the privacy manifest of the engine.
    - `swiftHelpers`: the Swift helpers of the bridge, which every app compiles.
    - `include`: the public headers, and `cmake`: the CMake package whose `haylen::native` native libraries compile against.
    """

    FRAMEWORK = "framework"
    PLAYER = "player"
    SETTINGS = "settings"
    PRIVACY = "privacy"
    SWIFT_HELPERS = "swiftHelpers"
    INCLUDE = "include"
    CMAKE = "cmake"
    # The files of a slice: its library in each framework.
    LIBRARY = "library"
    PLAYER_LIBRARY = "playerLibrary"

    FRAMEWORK_NAME = "Haylen.xcframework"
    PLAYER_NAME = "HaylenPlayer.xcframework"
    SETTINGS_NAME = "haylen-apple.json"
    PRIVACY_NAME = "PrivacyInfo.xcprivacy"
    SWIFT_HELPERS_NAME = "HaylenBridgeAsync.swift"
    LIBRARY_NAME = "libhaylen.a"
    PLAYER_LIBRARY_NAME = "libhaylen-player.a"
