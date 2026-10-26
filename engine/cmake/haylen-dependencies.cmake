# The source cache keeps a patched package under a key made of the hash of its archive and the contents of its patches, so an edited patch reaches every checkout that already holds the package, and checkouts in other folders share it.
# The patches are configure dependencies, so a build tree reconfigures and fetches the package under its new key when a patch changes.
function(haylen_patched_package_key variable archive_hash)
  set(contents "${archive_hash}")
  foreach(patch IN LISTS ARGN)
    file(SHA256 "${patch}" patch_hash)
    string(APPEND contents ";${patch_hash}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${patch}")
  endforeach()
  string(SHA256 key "${contents}")
  string(SUBSTRING "${key}" 0 16 key)
  set(${variable} "${key}" PARENT_SCOPE)
endfunction()

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

# The thread sanitizer cannot follow the `_longjmp` that a yield of Lua takes, so its builds tell Varn to keep yields as exceptions.
if(HAYLEN_SANITIZERS STREQUAL "THREAD" AND HAYLEN_DESKTOP AND NOT MSVC)
  set(HAYLEN_VARN_SANITIZE "thread")
else()
  set(HAYLEN_VARN_SANITIZE "")
endif()

# The engine links the static core of Varn, which the `cli` target builds, and Varn picks the HTTP driver of every platform itself: the URL Loading System on iOS, tvOS and Mac Catalyst, the Android stack on Android and the fetch of the browser on the web.
CPMAddPackage(
  NAME varn
  URL "https://github.com/varn-org/varn/archive/0adbdababe23ac98346f694e87228eadb2af2d61.tar.gz"
  URL_HASH SHA256=00aa7eaca39dcb1facbc6672af56bd1346d9a2f58e30678795d17375b0bbb414
  OPTIONS "VARN_TARGET ${HAYLEN_VARN_TARGET}" "VARN_BUILD_TESTS OFF" "VARN_SANITIZE ${HAYLEN_VARN_SANITIZE}"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

# Varn builds its own code, the Lua it vendors and libuv with the warning flags of their projects, which MSVC answers with hundreds of warnings in code the engine does not own.
set(varn_compiled_targets varn_core varn_vendor_lua)
if(TARGET uv_a)
  list(APPEND varn_compiled_targets uv_a)
endif()
haylen_disable_warnings(${varn_compiled_targets})

# The headers of Poco ask MSVC to link every Poco library by its file name, which no longer exists once the SDK merges the libraries into one, while CMake links them by their targets anyway.
if(MSVC AND TARGET Poco::Foundation)
  get_target_property(poco_foundation Poco::Foundation ALIASED_TARGET)
  target_compile_definitions(${poco_foundation} PUBLIC POCO_NO_AUTOMATIC_LIBS)
endif()

# The `sokol_app` module sizes the iOS framebuffer by the screen, which crops apps whose window is smaller than the screen, on Mac Catalyst and in iPad windows, so the first patch sizes it by the view of the app.
# Desktop apps open borderless, topmost, unfocusable and taskbar-less windows at a given position, so the second patch creates the window with those options before it first shows, lets the focus behavior change at run time, and makes transparency work on D3D11 through DirectComposition and on X11 through ARGB visuals. It also gives X11 windows the class by which desktops find the desktop entry of the app, and Windows windows the icon of the executable, since `sokol_app` gives them the logo of the system.
# The `sokol_app` module ends a destroyed Android activity with `exit()`, which aborts the process in the rendering threads of Android, so the third patch stops the app through its cleanup callback and lets the activity finish normally, and makes `sapp_quit()` finish the activity.
# Native plugins on Apple platforms receive the events of the application and its scenes, which only the application delegate of `sokol_app` sees, and the app shows and styles the status bar, which UIKit asks the view controller of the window about, so the fourth patch lets the runtime name a subclass of that delegate and of that view controller.
# The `sokol_app` module hosts Android apps in `NativeActivity`, which can never be the `ComponentActivity` that current SDKs and the Activity Result API need and keeps views from drawing over the app, so the last patch hosts them in `GameActivity`, with input through a queue from the UI thread, a key table and the `Choreographer` frame loop chosen at run time.
# The `sokol_app` module also runs Android frames only while the window of the activity has the focus, which stops timers, cancels and timeouts under every dialog and leaves the surface black when the app comes back under one, so the last patch runs them while the activity is resumed and has a surface, reports the focus as focus events and swaps only the frames that drew, so a covered app keeps its last picture.
# The `sokol_app` module suspends iOS, tvOS and Mac Catalyst apps whenever their scene resigns active, such as under the app switcher, Control Center, a phone call or another app in front of a Mac Catalyst window, so the last patch reports that as the loss of the focus, with the frames going on, and suspends the app only when its scene enters the background.
# The `sokol_app` module runs no Android frames at all while the activity is paused, which stops timers, network callbacks and native answers under every other activity and in the background, so the last patch also ticks a paused app ten times a second with frames that present nothing, and keeps the GL context current without a surface for them.
# The `sokol_app` module scales the points that trackpads scroll down to a tenth on macOS and to a hundredth on the web, without telling them from the steps of a wheel, and counts the three lines that browsers report for a step of a wheel as four steps, so the last patch reports precise scrolls in points with a flag and the scroll of wheels in steps on every platform.
# The `sokol_app` module reads the keys of the whole page and takes the focus moving between its elements for the focus of the app, so a page that hosts the app next to elements of its own, such as an editor, could only keep their keys from the app by stopping them before they reach those elements, so the next patch skips the keys that the runtime marks and every focus change but that of the window.
# The `sokol_app` module turns only the arrows, Select, Menu and Play/Pause presses of UIKit into keys, so a hardware keyboard of an iPad or an Apple TV reaches the app with those keys alone, while the engine reads every key of it through GameController, so the last patch leaves the presses of keyboards to GameController and keeps the presses of remotes and game controllers.
# The `sokol_gfx` module declares the `copy_src` usage of images without using it yet, so WebGPU textures never allow the copies out of them that the readback of render targets needs, and the next patch gives that usage to the textures that ask for it.
# The `sokol_gfx` module measures no time on the GPU, which the traces of the profiler show, so the next patch reads it from the completed command buffer of every Metal frame.
# The validation of `sokol_gfx` sets the bit of a view slot by shifting a signed 1, which overflows for the last of its 32 slots, so the last patch shifts an unsigned 1.
set(sokol_patches "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-ios-view-size.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-desktop-window.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-android-quit.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-apple-classes.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-android-gameactivity.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-ios-scene-states.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-scroll-units.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-web-page-events.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-ios-keyboard-presses.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-webgpu-copy-source.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-metal-gpu-time.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sokol-validation-shifts.patch")
set(sokol_hash 8ddf2d37017e9029caca91bf6417d2c7fe0d4f52f35793864fbff70e52d23a97)
haylen_patched_package_key(sokol_cache_key ${sokol_hash} ${sokol_patches})
CPMAddPackage(
  NAME sokol
  URL "https://github.com/floooh/sokol/archive/5e94d1291fea504ed911090338cf35bd9ef49a06.tar.gz"
  URL_HASH SHA256=${sokol_hash}
  PATCHES ${sokol_patches}
  CUSTOM_CACHE_KEY ${sokol_cache_key}
  DOWNLOAD_ONLY YES
)

# SDL opens the window, runs the frames and reads the input of every platform but Android. It builds as a static library with its video and event subsystems alone, since the engine plays sound through miniaudio, reads gamepads through the services of each platform and draws through its own devices. Linux builds keep X11 and Wayland, whose libraries SDL loads when the app starts, and Apple builds keep Metal, which the window hands to the Metal device.
set(haylen_sdl_options
  "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_TESTS OFF" "SDL_EXAMPLES OFF" "SDL_INSTALL OFF" "SDL_UNINSTALL OFF"
  "SDL_AUDIO OFF" "SDL_GPU OFF" "SDL_RENDER OFF" "SDL_CAMERA OFF" "SDL_JOYSTICK OFF" "SDL_HAPTIC OFF" "SDL_HIDAPI OFF" "SDL_POWER OFF" "SDL_SENSOR OFF" "SDL_DIALOG OFF" "SDL_TRAY OFF"
  "SDL_OFFSCREEN OFF" "SDL_DUMMYVIDEO OFF" "SDL_KMSDRM OFF" "SDL_RPI OFF" "SDL_ROCKCHIP OFF" "SDL_VIVANTE OFF" "SDL_LIBUDEV OFF" "SDL_LIBURING OFF" "SDL_PIPEWIRE OFF" "SDL_OPENVR OFF"
)
# Apple platforms draw through Metal alone, so their SDL carries neither OpenGL nor Vulkan.
if(APPLE)
  list(APPEND haylen_sdl_options "SDL_OPENGL OFF" "SDL_OPENGLES OFF" "SDL_VULKAN OFF")
endif()
# SDL turns the keys of hardware keyboards on iOS, iPadOS, Mac Catalyst and tvOS into keys only while GameController reads no keyboard, and the arrows and escape of a keyboard into key commands too, while the engine reads every key of a keyboard through GameController, so the patch leaves keyboards to GameController and turns the presses of remotes and game controllers into keys whether a keyboard is connected or not. It also drops the swipes of the Apple TV remote that SDL turns into arrows, since the engine reads the touch surface of the remote as its stick.
# SDL reads the keys of the whole page on the web, so a page that hosts the app next to elements of its own, such as an editor, could keep their keys from the app only by stopping them before they reach those elements, so the next patch skips the keys that the runtime marks.
# The Direct3D devices compose transparent windows of Windows through DirectComposition, whose visual the redirection surface of the window covers, so the last patch creates such windows without one.
if(NOT ANDROID)
  set(sdl_patches "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-uikit-presses.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-web-page-keys.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-windows-composition.patch")
  set(sdl_hash 9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3)
  haylen_patched_package_key(sdl_cache_key ${sdl_hash} ${sdl_patches})
  # SDL picks its UIKit backend by the system name, which Mac Catalyst builds name Darwin for the macOS SDK they compile against, so SDL configures with the name of iOS there.
  if(HAYLEN_CATALYST)
    set(CMAKE_SYSTEM_NAME iOS)
  endif()
  CPMAddPackage(
    NAME SDL3
    VERSION 3.4.18
    URL "https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz"
    URL_HASH SHA256=${sdl_hash}
    PATCHES ${sdl_patches}
    CUSTOM_CACHE_KEY ${sdl_cache_key}
    OPTIONS ${haylen_sdl_options}
    EXCLUDE_FROM_ALL YES
    SYSTEM YES
  )
  if(HAYLEN_CATALYST)
    set(CMAKE_SYSTEM_NAME Darwin)
  endif()
  haylen_disable_warnings(SDL3-static)

  # SDL links its frameworks through the framework feature of `LINK_LIBRARY`, whose name CMake drops from the link line while it keeps the flag when another library links the same framework with a plain flag first, such as Foundation through Varn, so SDL links them with plain flags as the other libraries do.
  if(APPLE)
    get_target_property(sdl_links SDL3-static INTERFACE_LINK_LIBRARIES)
    string(REGEX REPLACE "\\$<LINK_LIBRARY:FRAMEWORK,([A-Za-z]+)>" "-framework \\1" sdl_links "${sdl_links}")
    string(REGEX REPLACE "\\$<LINK_LIBRARY:WEAK_FRAMEWORK,([A-Za-z]+)>" "-weak_framework \\1" sdl_links "${sdl_links}")
    set_property(TARGET SDL3-static PROPERTY INTERFACE_LINK_LIBRARIES "${sdl_links}")
  endif()
endif()

# GameActivity of the AndroidX games libraries hosts Android apps. The script `haylen.py` builds the player outside Gradle, so the engine links the static library of its native side from the prefab folder of the AAR, whose Java classes the Android library and template use at the same version.
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

# NanoSVG reads SVG documents into cubic curves and rasterizes them with anti-aliasing, which vector images rasterize their sizes with. It publishes no releases, so it is pinned to a commit of its default branch.
# NanoSVG hides the shapes of `display` none but draws those whose `visibility` is hidden, such as the background a drawing program leaves in a logo, so a patch hides those too.
# NanoSVG reads the unitless coordinates of a gradient in the bounding box of its shape, the default units of SVG, as lengths of the document, which collapses the gradient, so a patch reads them as fractions of the box, which it stretches over the shape after the gradient transform.
set(nanosvg_patches "${CMAKE_CURRENT_LIST_DIR}/patches/nanosvg-visibility.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/nanosvg-bounding-box-gradients.patch")
set(nanosvg_hash 2bc68bdb518d7800252042e5cad50a0ab321596f0cbf49ef2a752926329063d2)
haylen_patched_package_key(nanosvg_cache_key ${nanosvg_hash} ${nanosvg_patches})
CPMAddPackage(
  NAME nanosvg
  URL "https://github.com/memononen/nanosvg/archive/239e102ec2c691f2902e20ace2ed36ee4a35cfe6.tar.gz"
  URL_HASH SHA256=${nanosvg_hash}
  PATCHES ${nanosvg_patches}
  CUSTOM_CACHE_KEY ${nanosvg_cache_key}
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
  URL "https://github.com/harfbuzz/harfbuzz/releases/download/14.6.0/harfbuzz-14.6.0.tar.xz"
  URL_HASH SHA256=d07a007327277708a2a73ae437887cdbaf282937f6d03ca5467723e9099af586
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
  URL "https://github.com/fastfloat/fast_float/archive/refs/tags/v8.3.2.tar.gz"
  URL_HASH SHA256=71bb9e865b888df01351558db5a39586d13ea97ada162156b3a07784d8c1b26b
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

# Zstandard decompresses the layers of Tiled maps, and it compresses every chunk of protected content on its own, which keeps random access and decodes faster than the other codecs at a similar ratio.
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
# Zstandard passes an option to the assembler, which the link of GCC drops from code compiled for link-time optimization with a warning, so it builds as machine code.
set_property(TARGET libzstd_static PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE OFF)

# Monocypher gives protected content its authenticated encryption (XChaCha20-Poly1305), its hashes and key derivation (BLAKE2b) and its manifest signatures (Ed25519). It is an audited C library of two files without dependencies, so it builds the same on every target, the web included, and the release pinned here fixes a timing leak of its signatures.
CPMAddPackage(
  NAME monocypher
  VERSION 4.0.3
  URL "https://github.com/LoupVaillant/Monocypher/releases/download/4.0.3/monocypher-4.0.3.tar.gz"
  URL_HASH SHA256=8cc9bc341a66249016db9bd70e9142d8d0aef9945973744b1ac05dbc55d8ee66
  DOWNLOAD_ONLY YES
)

# The Vulkan device draws through the headers of Vulkan, opens the loader of the system at run time through volk and allocates its memory through the Vulkan Memory Allocator, so building it needs no Vulkan SDK.
if("vulkan" IN_LIST HAYLEN_GRAPHICS_APIS)
  CPMAddPackage(
    NAME vulkan_headers
    URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.366.tar.gz"
    URL_HASH SHA256=577b90c208e6fd39ec9a1460e48772f63c76f6f5afb441df2a977a0fdb3f618c
    DOWNLOAD_ONLY YES
  )
  CPMAddPackage(
    NAME volk
    URL "https://github.com/zeux/volk/archive/refs/tags/1.4.350.tar.gz"
    URL_HASH SHA256=e47c6efe5294bb03729a976b385864bba5daf46ec60f0bdea11e1d1446345f9a
    DOWNLOAD_ONLY YES
  )
  CPMAddPackage(
    NAME vulkan_memory_allocator
    URL "https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.5.0.tar.gz"
    URL_HASH SHA256=c5f3db848fca9cb9726b41f256cc3007773321fa92a86d1e0902e66474e8a9e3
    DOWNLOAD_ONLY YES
  )
endif()

if(HAYLEN_BUILD_TESTS AND (HAYLEN_DESKTOP OR EMSCRIPTEN OR ANDROID))
  CPMAddPackage(
    NAME googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz"
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    OPTIONS "BUILD_GMOCK OFF" "INSTALL_GTEST OFF" "gtest_force_shared_crt ON"
    SYSTEM YES
  )
endif()

add_library(haylen-sokol-headers INTERFACE)
target_include_directories(haylen-sokol-headers SYSTEM INTERFACE "${sokol_SOURCE_DIR}" "${sokol_SOURCE_DIR}/util")

if(ANDROID)
  set(game_activity_dir "${games_activity_SOURCE_DIR}/prefab/modules/game-activity_static")
  add_library(haylen-game-activity STATIC IMPORTED)
  set_target_properties(haylen-game-activity PROPERTIES IMPORTED_LOCATION "${game_activity_dir}/libs/android.${ANDROID_ABI}/libgame-activity_static.a")
  target_include_directories(haylen-game-activity SYSTEM INTERFACE "${game_activity_dir}/include")
endif()

add_library(haylen-stb INTERFACE)
target_include_directories(haylen-stb SYSTEM INTERFACE "${stb_SOURCE_DIR}")

# The SVG reader and rasterizer compile in a library of their own, which keeps the warnings of the engine off their code.
add_library(haylen-nanosvg STATIC "${CMAKE_CURRENT_LIST_DIR}/../src/graphics/NanoSvgImpl.c")
target_include_directories(haylen-nanosvg SYSTEM PUBLIC "${nanosvg_SOURCE_DIR}/src")

add_library(haylen-fast-float INTERFACE)
target_include_directories(haylen-fast-float SYSTEM INTERFACE "${fast_float_SOURCE_DIR}/include")

# The amalgamated source of HarfBuzz builds its OpenType shaper alone, without the shapers of legacy and AAT fonts.
add_library(haylen-harfbuzz STATIC "${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc")
target_include_directories(haylen-harfbuzz SYSTEM PUBLIC "${harfbuzz_SOURCE_DIR}/src")
target_compile_definitions(haylen-harfbuzz PRIVATE HB_MINI)

add_library(haylen-sheenbidi STATIC "${sheenbidi_SOURCE_DIR}/Source/SheenBidi.c")
target_include_directories(haylen-sheenbidi SYSTEM PUBLIC "${sheenbidi_SOURCE_DIR}/Headers" PRIVATE "${sheenbidi_SOURCE_DIR}/Source")
target_compile_definitions(haylen-sheenbidi PRIVATE SB_CONFIG_UNITY)

set(unibreak_dir "${libunibreak_SOURCE_DIR}/src")
add_library(haylen-unibreak STATIC "${unibreak_dir}/unibreakbase.c" "${unibreak_dir}/unibreakdef.c" "${unibreak_dir}/linebreak.c" "${unibreak_dir}/linebreakdata.c" "${unibreak_dir}/linebreakdef.c" "${unibreak_dir}/eastasianwidthdef.c" "${unibreak_dir}/emojidef.c" "${unibreak_dir}/graphemebreak.c")
target_include_directories(haylen-unibreak SYSTEM PUBLIC "${unibreak_dir}")

# The optional file of Monocypher holds Ed25519 with SHA-512, the signature scheme of manifests.
add_library(haylen-monocypher STATIC "${monocypher_SOURCE_DIR}/src/monocypher.c" "${monocypher_SOURCE_DIR}/src/optional/monocypher-ed25519.c")
target_include_directories(haylen-monocypher SYSTEM PUBLIC "${monocypher_SOURCE_DIR}/src" "${monocypher_SOURCE_DIR}/src/optional")

add_library(haylen-imgui STATIC
  "${imgui_SOURCE_DIR}/imgui.cpp"
  "${imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
  "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/../src/ui/ImGuiAssert.cpp"
)
target_include_directories(haylen-imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/misc/cpp")
target_include_directories(haylen-imgui PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")
target_compile_definitions(haylen-imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DEFINE_MATH_OPERATORS IMGUI_USE_WCHAR32 "IMGUI_USER_CONFIG=\"${CMAKE_CURRENT_LIST_DIR}/../src/ui/ImGuiConfig.hpp\"")

add_library(haylen-miniaudio STATIC "${CMAKE_CURRENT_LIST_DIR}/../src/audio/MiniaudioImpl.c")
target_include_directories(haylen-miniaudio SYSTEM PUBLIC "${miniaudio_SOURCE_DIR}")
target_compile_definitions(haylen-miniaudio PUBLIC MA_NO_GENERATION MA_NO_ENCODING MA_NO_RESOURCE_MANAGER)
if(ANDROID)
  target_compile_definitions(haylen-miniaudio PUBLIC MA_NO_OPENSL)
  target_link_libraries(haylen-miniaudio PUBLIC log)
elseif(APPLE)
  target_link_libraries(haylen-miniaudio PUBLIC "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
  if(HAYLEN_PLATFORM STREQUAL "ios" OR HAYLEN_PLATFORM STREQUAL "tvos")
    # The miniaudio library manages the `AVAudioSession` on iOS and tvOS, so its implementation compiles as Objective-C there.
    set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/../src/audio/MiniaudioImpl.c" PROPERTIES LANGUAGE OBJC)
    target_link_libraries(haylen-miniaudio PUBLIC "-framework AVFoundation")
  endif()
elseif(HAYLEN_PLATFORM STREQUAL "web")
  # Browsers play through the AudioWorklet output of the engine, `src/platform/web/BrowserAudioOutput`, so miniaudio keeps only its custom backend there.
  target_compile_definitions(haylen-miniaudio PUBLIC MA_ENABLE_ONLY_SPECIFIC_BACKENDS MA_ENABLE_CUSTOM)
elseif(HAYLEN_PLATFORM STREQUAL "linux")
  target_link_libraries(haylen-miniaudio PUBLIC ${CMAKE_DL_LIBS} m pthread)
endif()

# The loader of volk declares the functions of the surfaces of the windows of each platform, which the Vulkan device makes its swapchains on.
if("vulkan" IN_LIST HAYLEN_GRAPHICS_APIS)
  add_library(haylen-volk STATIC "${volk_SOURCE_DIR}/volk.c")
  target_include_directories(haylen-volk SYSTEM PUBLIC "${volk_SOURCE_DIR}" "${vulkan_headers_SOURCE_DIR}/include")
  target_link_libraries(haylen-volk PUBLIC ${CMAKE_DL_LIBS})
  if(WIN32)
    target_compile_definitions(haylen-volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
  elseif(ANDROID)
    target_compile_definitions(haylen-volk PUBLIC VK_USE_PLATFORM_ANDROID_KHR)
  elseif(HAYLEN_PLATFORM STREQUAL "linux")
    target_compile_definitions(haylen-volk PUBLIC VK_USE_PLATFORM_XLIB_KHR VK_USE_PLATFORM_WAYLAND_KHR)
  endif()

  # The allocator finds the functions of Vulkan through the two entry points of the loader that volk opens, and takes the headers of Vulkan that volk includes without the headers of the window systems.
  add_library(haylen-vulkan-memory-allocator STATIC "${CMAKE_CURRENT_LIST_DIR}/../src/gpu/vulkan/VulkanMemoryAllocatorImpl.cpp")
  target_include_directories(haylen-vulkan-memory-allocator SYSTEM PUBLIC "${vulkan_memory_allocator_SOURCE_DIR}/include")
  target_compile_definitions(haylen-vulkan-memory-allocator PUBLIC VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=1 VMA_VULKAN_HEADERS_ALREADY_INCLUDED)
  target_link_libraries(haylen-vulkan-memory-allocator PUBLIC haylen-volk)
endif()
