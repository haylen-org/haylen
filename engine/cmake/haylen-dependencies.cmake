# Varn uses these packages too, so they are resolved here first and Varn reuses them.
CPMAddPackage(
  NAME nlohmann_json
  VERSION 3.12.0
  URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF"
  SYSTEM YES
)

# Varn pins libuv 1.49.2, which misses <limits.h> with NDK r30. Varn waits on libuv everywhere except the web and reuses this release.
if(NOT HAYLEN_PLATFORM STREQUAL "web")
  CPMAddPackage(
    NAME libuv
    VERSION 1.53.0
    URL "https://github.com/libuv/libuv/archive/refs/tags/v1.53.0.tar.gz"
    URL_HASH SHA256=279f3f67a24bb9921fe999ca6cd5e332fade8d515873ef9ba054b70e70a31d9e
    OPTIONS "LIBUV_BUILD_TESTS OFF" "LIBUV_BUILD_BENCH OFF" "LIBUV_BUILD_SHARED OFF"
    SYSTEM YES
  )
  # The tvOS branch of uv_spawn in libuv 1.53.0 still calls QUEUE_INIT, which libuv renamed to uv__queue_init everywhere else.
  if(HAYLEN_PLATFORM STREQUAL "tvos")
    target_compile_options(uv_a PRIVATE "-DQUEUE_INIT(queue)=uv__queue_init(queue)")
  endif()
  # libuv 1.53.0 compiles its posix_spawn path on every Unix, while Android declares posix_spawn only from API 28 on. libuv never takes that path on Android and forks instead, so weak references let it build for older releases.
  if(ANDROID)
    target_compile_definitions(uv_a PRIVATE __ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__)
  endif()
endif()

if(HAYLEN_PLATFORM STREQUAL "android")
  set(HAYLEN_VARN_TARGET "android")
elseif(HAYLEN_PLATFORM STREQUAL "web")
  set(HAYLEN_VARN_TARGET "wasm")
else()
  set(HAYLEN_VARN_TARGET "cli")
endif()

# Varn picks the HTTP driver of a mobile target only once its own cache holds the target, which is not the case on the first configure, so the driver is named here.
set(HAYLEN_VARN_OPTIONS "VARN_TARGET ${HAYLEN_VARN_TARGET}" "VARN_BUILD_TESTS OFF")
if(HAYLEN_PLATFORM STREQUAL "ios" OR HAYLEN_PLATFORM STREQUAL "tvos")
  list(APPEND HAYLEN_VARN_OPTIONS "VARN_HTTP_CLIENT_DRIVER APPLE")
elseif(HAYLEN_PLATFORM STREQUAL "android")
  list(APPEND HAYLEN_VARN_OPTIONS "VARN_HTTP_CLIENT_DRIVER ANDROID")
endif()

# Varn builds OpenSSL with its own Configure script, which takes the compiler flags of Mac Catalyst from the environment, next to the optimization level it would pick itself.
if(HAYLEN_CATALYST)
  set(ENV{CFLAGS} "-O3 --target=${CMAKE_C_COMPILER_TARGET} ${CMAKE_C_FLAGS}")
endif()

CPMAddPackage(
  NAME varn
  URL "https://github.com/varn-org/varn/archive/refs/tags/v0.0.1.tar.gz"
  URL_HASH SHA256=646dd5ee1001508fca55d0768e99f8fcfe8e0329b2d7d6be3a05810dbfcf5588
  OPTIONS ${HAYLEN_VARN_OPTIONS}
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

# Varn configures Lua for macOS when a Mac Catalyst build names Darwin as its system, while Mac Catalyst has no system function, like iOS.
if(HAYLEN_CATALYST)
  unset(ENV{CFLAGS})
  target_compile_definitions(varn_vendor_lua PRIVATE LUA_USE_IOS)
endif()

# sokol_app sizes the iOS framebuffer by the screen, which crops apps whose window is smaller than the screen, on Mac Catalyst and in iPad windows, so the first patch sizes it by the view of the app.
# The dummy backend of the headless host caps textures at 1024 pixels, below every real GPU, so the second patch gives it the limits of desktop GPUs and headless runs load full-size art.
# Desktop apps open borderless, topmost, unfocusable and taskbar-less windows at a given position, so the third patch creates the window with those options before it first shows, lets the focus behavior change at run time, and makes transparency work on D3D11 through DirectComposition and on X11 through ARGB visuals.
# sokol_app ends a destroyed Android activity with exit(), which aborts the process in the rendering threads of Android, so the fourth patch stops the app through its cleanup callback and lets the activity finish normally, and makes sapp_quit() finish the activity.
CPMAddPackage(
  NAME sokol
  URL "https://github.com/floooh/sokol/archive/2e75443dbd4940b5aa8d76a8e479f8e4b270b9a3.tar.gz"
  URL_HASH SHA256=d8560ddd11fb3223f3aaf6513aaa2d9be66ee7896fa1263ad51c5bd60ec52cf3
  PATCHES "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-ios-view-size.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-dummy-limits.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-desktop-window.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-android-quit.patch"
  DOWNLOAD_ONLY YES
)

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

# libunibreak finds the line break opportunities and the grapheme clusters of Unicode text.
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

# Parses floating point numbers the way std::from_chars does, which the C++ library of Apple platforms offers only from iOS and tvOS 26 and macOS 26.
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

# Clipper2 computes the polygon booleans, offsets and constrained triangulations of math::Polygon.
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
    # miniaudio manages the AVAudioSession on iOS and tvOS, so its implementation compiles as Objective-C there.
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../src/audio/MiniaudioImpl.c" PROPERTIES LANGUAGE OBJC)
    target_link_libraries(haylen_miniaudio PUBLIC "-framework AVFoundation")
  endif()
elseif(HAYLEN_PLATFORM STREQUAL "linux")
  target_link_libraries(haylen_miniaudio PUBLIC ${CMAKE_DL_LIBS} m pthread)
endif()
