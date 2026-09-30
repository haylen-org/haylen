# Varn uses nlohmann/json too, so it is resolved here first and Varn reuses it.
CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.12.0
  URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
  SYSTEM YES
)

if(HAYLEN_PLATFORM STREQUAL "android")
  set(HAYLEN_VARN_TARGET "android")
elseif(HAYLEN_PLATFORM STREQUAL "web")
  set(HAYLEN_VARN_TARGET "wasm")
else()
  set(HAYLEN_VARN_TARGET "cli")
endif()

# The engine links the static core of Varn, which the `cli` target builds, and Varn picks the HTTP driver of every platform itself: the URL Loading System on iOS, tvOS and Mac Catalyst, the Android stack on Android and the fetch of the browser on the web.
# Varn hands the error that a to-be-closed variable of a cancelled task raises on as a bare value, which the handler of `async.onFailure` never receives, so the patch wraps it in the table every other failure comes in.
CPMAddPackage(
  NAME varn
  URL "https://github.com/varn-org/varn/archive/04b710da801ed284e48f934bc03bab0d64356aa8.tar.gz"
  URL_HASH SHA256=7532f516adf9063856fbe66f362f1a05742fee4f8f4df47a5a599cb554bf6954
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/patches/varn-close-failure.patch"
  OPTIONS "VARN_TARGET ${HAYLEN_VARN_TARGET}" "VARN_BUILD_TESTS OFF"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

# The headers of Poco ask MSVC to link every Poco library by its file name, which no longer exists once the SDK merges the libraries into one, while CMake links them by their targets anyway.
if(MSVC AND TARGET Poco::Foundation)
  get_target_property(poco_foundation Poco::Foundation ALIASED_TARGET)
  target_compile_definitions(${poco_foundation} PUBLIC POCO_NO_AUTOMATIC_LIBS)
endif()

# The `sokol_app` module sizes the iOS framebuffer by the screen, which crops apps whose window is smaller than the screen, on Mac Catalyst and in iPad windows, so the first patch sizes it by the view of the app.
# The dummy backend of the headless host caps textures at 1024 pixels, below every real GPU, so the second patch gives it the limits of desktop GPUs and headless runs load full-size art.
# Desktop apps open borderless, topmost, unfocusable and taskbar-less windows at a given position, so the third patch creates the window with those options before it first shows, lets the focus behavior change at run time, and makes transparency work on D3D11 through DirectComposition and on X11 through ARGB visuals.
# The `sokol_app` module ends a destroyed Android activity with `exit()`, which aborts the process in the rendering threads of Android, so the fourth patch stops the app through its cleanup callback and lets the activity finish normally, and makes `sapp_quit()` finish the activity.
# Native plugins on Apple platforms receive the events of the application and its scenes, which only the application delegate of `sokol_app` sees, so the fifth patch lets the runtime name a subclass of that delegate.
# The `sokol_app` module hosts Android apps in `NativeActivity`, which can never be the `ComponentActivity` that current SDKs and the Activity Result API need and keeps views from drawing over the app, so the last patch hosts them in `GameActivity`, with input through a queue from the UI thread, a key table, the native saved state and the `Choreographer` frame loop chosen at run time.
# The `sokol_app` module also runs Android frames only while the window of the activity has the focus, which stops timers, cancels and timeouts under every dialog and leaves the surface black when the app comes back under one, so the last patch runs them while the activity is resumed and has a surface, reports the focus as focus events and swaps only the frames that drew, so a covered app keeps its last picture.
CPMAddPackage(
  NAME sokol
  URL "https://github.com/floooh/sokol/archive/2e75443dbd4940b5aa8d76a8e479f8e4b270b9a3.tar.gz"
  URL_HASH SHA256=d8560ddd11fb3223f3aaf6513aaa2d9be66ee7896fa1263ad51c5bd60ec52cf3
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-ios-view-size.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-dummy-limits.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-desktop-window.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-android-quit.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-apple-delegate.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-android-gameactivity.patch"
  DOWNLOAD_ONLY YES
)

# GameActivity of the AndroidX games libraries hosts Android apps. The script `make.py` builds the player outside Gradle, so the engine links the static library of its native side from the prefab folder of the AAR, whose Java classes the Android library and template use at the same version.
if(ANDROID)
  CPMAddPackage(
    NAME games_activity
    VERSION 4.4.2
    URL "https://dl.google.com/android/maven2/androidx/games/games-activity/4.4.2/games-activity-4.4.2.aar"
    URL_HASH SHA256=16069bb61a34e9cc7a4f6b4cef0ddb9e2f3f61fe781b282787f574f8880ba38e
    DOWNLOAD_ONLY YES
  )
endif()

CPMAddPackage(
  NAME stb
  URL "https://github.com/nothings/stb/archive/2c980bb59875b0d32144a71867fbdebb2f77cd20.tar.gz"
  URL_HASH SHA256=9a955b1b49a4410088a2e0ee2a9c057c3c907d0c1d75454144cb980aca0ba515
  DOWNLOAD_ONLY YES
)

# The core of msdfgen builds the signed distance fields of font glyphs from their whole outlines, the cubic curves of OpenType CFF fonts included.
CPMAddPackage(
  NAME msdfgen
  URL "https://github.com/Chlumsky/msdfgen/archive/refs/tags/v1.13.tar.gz"
  URL_HASH SHA256=93cd1ad8918c1a78c5c96e82d4f4c77f0eb86c2e7e8579a0967e54196c4b7167
  OPTIONS "MSDFGEN_CORE_ONLY ON" "MSDFGEN_BUILD_STANDALONE OFF" "MSDFGEN_USE_VCPKG OFF" "MSDFGEN_USE_SKIA OFF" "MSDFGEN_INSTALL OFF" "MSDFGEN_DYNAMIC_RUNTIME ON"
  SYSTEM YES
)

# HarfBuzz shapes text with the OpenType tables of its font: ligatures, contextual forms, mark positioning and kerning.
CPMAddPackage(
  NAME harfbuzz
  URL "https://github.com/harfbuzz/harfbuzz/releases/download/14.5.0/harfbuzz-14.5.0.tar.xz"
  URL_HASH SHA256=b7132e148358a45185c9feafd049dbaf243649d3c44414b3534d9c95d18592b9
  DOWNLOAD_ONLY YES
)

# SheenBidi resolves the bidirectional levels and the visual order of paragraphs and finds their script runs.
CPMAddPackage(
  NAME sheenbidi
  URL "https://github.com/Tehreer/SheenBidi/archive/refs/tags/v3.0.0.tar.gz"
  URL_HASH SHA256=86c56014034739ba39a24c23eb00323b0bf6f737354f665786015fca842af786
  DOWNLOAD_ONLY YES
)

# The libunibreak library finds the line break opportunities and the grapheme clusters of Unicode text.
CPMAddPackage(
  NAME libunibreak
  URL "https://github.com/adah1972/libunibreak/releases/download/libunibreak_8_0/libunibreak-8.0.tar.gz"
  URL_HASH SHA256=9c4fad6e517338a098373acc9f35579ae2c325e6446666fb9ac2666ba15ceba4
  DOWNLOAD_ONLY YES
)

# The Thai model of BudouX finds where Thai text, which has no spaces between words, may break into lines.
CPMAddPackage(
  NAME budoux
  URL "https://github.com/google/budoux/archive/refs/tags/v0.9.3.tar.gz"
  URL_HASH SHA256=55211d599d35c9dcfbb237b5f2050f382daa8e8fe8895986809ee5084e6da893
  DOWNLOAD_ONLY YES
)

# Parses floating point numbers the way `std::from_chars` does, which the C++ library of Apple platforms offers only from iOS and tvOS 26 and macOS 26.
CPMAddPackage(
  NAME fast_float
  URL "https://github.com/fastfloat/fast_float/archive/refs/tags/v8.3.0.tar.gz"
  URL_HASH SHA256=90485994d0fed61d0693e8dba32c464b0e8adf7f2e7c2efbf5bff0df5ef5b13f
  DOWNLOAD_ONLY YES
)

CPMAddPackage(
  NAME imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz"
  URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
  DOWNLOAD_ONLY YES
)

CPMAddPackage(
  NAME miniaudio
  URL "https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz"
  URL_HASH SHA256=b900edcffe979816e2560a0580b9b1216d674b4f17fbadeca8f777a7f8ab0274
  DOWNLOAD_ONLY YES
)

CPMAddPackage(
  NAME box2d
  URL "https://github.com/erincatto/box2d/archive/refs/tags/v3.1.1.tar.gz"
  URL_HASH SHA256=fb6ef914b50f4312d7d921a600eabc12318bb3c55a0b8c0b90608fa4488ef2e4
  OPTIONS "BOX2D_SAMPLES OFF" "BOX2D_BENCHMARKS OFF" "BOX2D_DOCS OFF" "BOX2D_UNIT_TESTS OFF" "BOX2D_VALIDATE OFF"
  SYSTEM YES
)

# Clipper2 computes the polygon booleans, offsets and constrained triangulations of `math::Polygon`.
CPMAddPackage(
  NAME clipper2
  URL "https://github.com/AngusJohnson/Clipper2/archive/refs/tags/Clipper2_2.0.1.tar.gz"
  URL_HASH SHA256=2a3693aceab4aed3e39b743e038d87701acc53cf05ed7b2013aab3e0aec5287e
  SOURCE_SUBDIR CPP
  OPTIONS "CLIPPER2_UTILS OFF" "CLIPPER2_EXAMPLES OFF" "CLIPPER2_TESTS OFF" "CLIPPER2_USINGZ OFF"
  SYSTEM YES
)
# Clipper2 links the math library by name, which Apple platforms keep in libSystem, and libuv links it too, so the Apple linker warns about the duplicate.
if(APPLE)
  set_property(TARGET Clipper2 PROPERTY INTERFACE_LINK_LIBRARIES "")
endif()

CPMAddPackage(
  NAME zstd
  URL "https://github.com/facebook/zstd/archive/refs/tags/v1.5.7.tar.gz"
  URL_HASH SHA256=37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3
  SOURCE_SUBDIR build/cmake
  OPTIONS
    "ZSTD_BUILD_PROGRAMS OFF"
    "ZSTD_BUILD_TESTS OFF"
    "ZSTD_BUILD_SHARED OFF"
    "ZSTD_BUILD_STATIC ON"
    "ZSTD_BUILD_COMPRESSION ON"
    "ZSTD_BUILD_DICTBUILDER OFF"
    "ZSTD_LEGACY_SUPPORT OFF"
    "ZSTD_MULTITHREAD_SUPPORT OFF"
  SYSTEM YES
)

if(HAYLEN_BUILD_TESTS AND HAYLEN_DESKTOP)
  CPMAddPackage(
    NAME googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz"
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    OPTIONS "BUILD_GMOCK OFF" "INSTALL_GTEST OFF" "gtest_force_shared_crt ON"
    SYSTEM YES
  )
endif()

add_library(haylen_sokol_headers INTERFACE)
target_include_directories(haylen_sokol_headers SYSTEM INTERFACE "${sokol_SOURCE_DIR}" "${sokol_SOURCE_DIR}/util")

if(ANDROID)
  set(game_activity_dir "${games_activity_SOURCE_DIR}/prefab/modules/game-activity_static")
  add_library(haylen_game_activity STATIC IMPORTED)
  set_target_properties(haylen_game_activity PROPERTIES IMPORTED_LOCATION "${game_activity_dir}/libs/android.${ANDROID_ABI}/libgame-activity_static.a")
  target_include_directories(haylen_game_activity SYSTEM INTERFACE "${game_activity_dir}/include")
endif()

add_library(haylen_stb INTERFACE)
target_include_directories(haylen_stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")

add_library(haylen_fast_float INTERFACE)
target_include_directories(haylen_fast_float SYSTEM INTERFACE "${fast_float_SOURCE_DIR}/include")

# The amalgamated source of HarfBuzz builds its OpenType shaper alone, without the shapers of legacy and AAT fonts.
add_library(haylen_harfbuzz STATIC "${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc")
target_include_directories(haylen_harfbuzz SYSTEM PUBLIC "${harfbuzz_SOURCE_DIR}/src")
target_compile_definitions(haylen_harfbuzz PRIVATE HB_MINI)

add_library(haylen_sheenbidi STATIC "${sheenbidi_SOURCE_DIR}/Source/SheenBidi.c")
target_include_directories(haylen_sheenbidi SYSTEM PUBLIC "${sheenbidi_SOURCE_DIR}/Headers" PRIVATE "${sheenbidi_SOURCE_DIR}/Source")
target_compile_definitions(haylen_sheenbidi PRIVATE SB_CONFIG_UNITY)

set(unibreak_dir "${libunibreak_SOURCE_DIR}/src")
add_library(haylen_unibreak STATIC "${unibreak_dir}/unibreakbase.c" "${unibreak_dir}/unibreakdef.c" "${unibreak_dir}/linebreak.c" "${unibreak_dir}/linebreakdata.c" "${unibreak_dir}/linebreakdef.c" "${unibreak_dir}/eastasianwidthdef.c" "${unibreak_dir}/emojidef.c" "${unibreak_dir}/graphemebreak.c")
target_include_directories(haylen_unibreak SYSTEM PUBLIC "${unibreak_dir}")

add_library(haylen_imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/ui/ImGuiAssert.cpp"
)
target_include_directories(haylen_imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/misc/cpp")
target_include_directories(haylen_imgui PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")
target_compile_definitions(haylen_imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DEFINE_MATH_OPERATORS IMGUI_USE_WCHAR32 "IMGUI_USER_CONFIG=\"${CMAKE_CURRENT_LIST_DIR}/../src/ui/ImGuiConfig.hpp\"")

add_library(haylen_miniaudio STATIC "${CMAKE_CURRENT_LIST_DIR}/../src/audio/MiniaudioImpl.c")
target_include_directories(haylen_miniaudio SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
target_compile_definitions(haylen_miniaudio PUBLIC MA_NO_GENERATION MA_NO_ENCODING MA_NO_RESOURCE_MANAGER)
if(ANDROID)
  target_compile_definitions(haylen_miniaudio PUBLIC MA_NO_OPENSL)
  target_link_libraries(haylen_miniaudio PUBLIC log)
elseif(APPLE)
  target_link_libraries(haylen_miniaudio PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
  if(HAYLEN_PLATFORM STREQUAL "ios" OR HAYLEN_PLATFORM STREQUAL "tvos")
    # The miniaudio library manages the `AVAudioSession` on iOS and tvOS, so its implementation compiles as Objective-C there.
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../src/audio/MiniaudioImpl.c" PROPERTIES LANGUAGE OBJC)
    target_link_libraries(haylen_miniaudio PUBLIC "-framework AVFoundation")
  endif()
elseif(HAYLEN_PLATFORM STREQUAL "web")
  # Browsers play through the AudioWorklet output of the engine, `src/platform/web/BrowserAudioOutput`, so miniaudio keeps only its custom backend there.
  target_compile_definitions(haylen_miniaudio PUBLIC MA_ENABLE_ONLY_SPECIFIC_BACKENDS MA_ENABLE_CUSTOM)
elseif(HAYLEN_PLATFORM STREQUAL "linux")
  target_link_libraries(haylen_miniaudio PUBLIC ${CMAKE_DL_LIBS} m pthread)
endif()
