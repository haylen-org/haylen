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

# Windows machines often put the patch of Strawberry Perl first on the path, an old version that crashes on the patches of SDL, so the patches of dependencies take the patch that Git for Windows installs next to it, which CPM finds only when the path holds no other.
if(CMAKE_HOST_WIN32 AND NOT PATCH_EXECUTABLE)
  find_package(Git QUIET)
  if(GIT_EXECUTABLE)
    get_filename_component(haylen_git_folder "${GIT_EXECUTABLE}" DIRECTORY)
    find_program(PATCH_EXECUTABLE patch HINTS "${haylen_git_folder}/../usr/bin" "${haylen_git_folder}/../../usr/bin" NO_DEFAULT_PATH)
  endif()
endif()

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
if(HAYLEN_SANITIZERS STREQUAL "THREAD")
  set(HAYLEN_VARN_SANITIZE "thread")
else()
  set(HAYLEN_VARN_SANITIZE "")
endif()

# A port builds the target `port` of Varn with the drivers that its `haylen_port` names, every other driver being the one that needs nothing of the system, such as the event loop that the frames pump, the sources of its external drivers and worker pools on threads of their own wherever the platform has threads.
if(HAYLEN_PORT_PLATFORM)
  set(HAYLEN_VARN_TARGET "port")
  set(VARN_THREADS ${HAYLEN_THREADS} CACHE BOOL "The worker pools of Varn on threads of their own, which follow the threads of the port." FORCE)
  # Each configuration takes the drivers from the `haylen_port` of that moment, so a driver that the port no longer names goes back to the default of the target `port` of Varn rather than keeping a value of an earlier configuration.
  foreach(driver IN ITEMS SOCKET HTTP_CLIENT HTTP_SERVER CRYPTO PROCESS FFI FS LOG JSON XML)
    unset(VARN_${driver}_DRIVER CACHE)
    unset(VARN_${driver}_DRIVER_SOURCES CACHE)
  endforeach()
  unset(VARN_ENABLE_TLS CACHE)
  set(VARN_EVENT_LOOP_DRIVER "PUMP" CACHE STRING "The event loop of Varn, which the port may name." FORCE)
  # The callbacks of the module `ffi` are closures that write code into memory, so a port that does not allow `executableMemory` builds Varn without them, as it builds the native callbacks of the engine.
  if("executableMemory" IN_LIST HAYLEN_CAPABILITIES)
    set(VARN_FFI_CLOSURES ON CACHE BOOL "The callbacks of the ffi module of Varn, which follow the executable memory of the port." FORCE)
  else()
    set(VARN_FFI_CLOSURES OFF CACHE BOOL "The callbacks of the ffi module of Varn, which follow the executable memory of the port." FORCE)
  endif()
  foreach(driver IN LISTS HAYLEN_PORT_VARN_DRIVERS)
    string(REGEX MATCH "^([A-Z_]+)=(.+)$" matched "${driver}")
    if(CMAKE_MATCH_1 STREQUAL "TLS")
      set(VARN_ENABLE_TLS ${CMAKE_MATCH_2} CACHE BOOL "TLS of Varn, which the port names." FORCE)
    else()
      set(VARN_${CMAKE_MATCH_1}_DRIVER "${CMAKE_MATCH_2}" CACHE STRING "A driver of Varn, which the port names." FORCE)
    endif()
  endforeach()
  set(haylen_varn_external_drivers "")
  foreach(source IN LISTS HAYLEN_PORT_VARN_DRIVER_SOURCES)
    string(REGEX MATCH "^([A-Z_]+)=(.+)$" matched "${source}")
    list(APPEND haylen_varn_sources_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
    list(APPEND haylen_varn_external_drivers ${CMAKE_MATCH_1})
  endforeach()
  list(REMOVE_DUPLICATES haylen_varn_external_drivers)
  foreach(driver IN LISTS haylen_varn_external_drivers)
    set(VARN_${driver}_DRIVER_SOURCES "${haylen_varn_sources_${driver}}" CACHE STRING "The sources of an external driver of Varn, which the port names." FORCE)
  endforeach()
endif()

# The engine links the static core of Varn, which the `cli` target builds, and Varn picks the HTTP driver of every platform itself: the URL Loading System on iOS, tvOS and Mac Catalyst, the Android stack on Android and the fetch of the browser on the web.
CPMAddPackage(
  NAME varn
  URL "https://github.com/varn-org/varn/archive/02d9e45944e17a6efba495d5c427c235fafc8d0f.tar.gz"
  URL_HASH SHA256=40c28ed15f0862b5f62f4910a41d743e5323a795b014ff910245fa42b55dea94
  OPTIONS "VARN_TARGET ${HAYLEN_VARN_TARGET}" "VARN_BUILD_TESTS OFF" "VARN_SANITIZE ${HAYLEN_VARN_SANITIZE}"
  EXCLUDE_FROM_ALL YES
  SYSTEM YES
)

# The page shares data views with the engine through the class `VarnDataView` of Varn, which every web build links before the runtime of the page.
set(HAYLEN_VARN_DATAVIEW_SCRIPT "${varn_SOURCE_DIR}/src/wasm/varn-dataview.js" CACHE INTERNAL "The JavaScript class of Varn data views.")

# Varn builds its own code, the Lua it vendors and libuv with the warning flags of their projects, which MSVC answers with hundreds of warnings in code the engine does not own.
set(varn_compiled_targets varn-runtime varn-core varn-vendor-lua)
if(TARGET uv_a)
  list(APPEND varn_compiled_targets uv_a)
endif()
haylen_disable_warnings(${varn_compiled_targets})
# The containers of the standard library mark the room they leave unused for AddressSanitizer only in code that it instruments, and a container that the code of Varn and of the engine share runs the code of either, so the C++ of Varn builds with AddressSanitizer too.
haylen_check_dependency_memory(varn-runtime varn-core)

# The headers of Poco ask MSVC to link every Poco library by its file name, which no longer exists once the SDK merges the libraries into one, while CMake links them by their targets anyway.
if(MSVC AND TARGET Poco::Foundation)
  get_target_property(poco_foundation Poco::Foundation ALIASED_TARGET)
  target_compile_definitions(${poco_foundation} PUBLIC POCO_NO_AUTOMATIC_LIBS)
endif()

# SDL opens the window, runs the frames and reads the input and the controllers of every platform but Android and the platforms of ports, which bring their own platform layer. It builds as a static library with its video, event and gamepad subsystems alone, whose gamepads read their own motion sensors, since the engine plays sound through miniaudio and draws through its own devices, and it rumbles controllers through their gamepads without its haptic subsystem. Linux builds keep X11, Wayland and udev, whose libraries SDL loads when the app starts, and Apple builds keep Metal, which the window hands to the Metal device.
set(haylen_sdl_options
  "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF" "SDL_TESTS OFF" "SDL_EXAMPLES OFF" "SDL_INSTALL OFF" "SDL_UNINSTALL OFF"
  "SDL_AUDIO OFF" "SDL_GPU OFF" "SDL_RENDER OFF" "SDL_CAMERA OFF" "SDL_SENSOR OFF" "SDL_HAPTIC OFF" "SDL_HIDAPI_LIBUSB OFF" "SDL_POWER OFF" "SDL_DIALOG OFF" "SDL_TRAY OFF"
  "SDL_OFFSCREEN OFF" "SDL_KMSDRM OFF" "SDL_RPI OFF" "SDL_ROCKCHIP OFF" "SDL_VIVANTE OFF" "SDL_LIBURING OFF" "SDL_PIPEWIRE OFF" "SDL_OPENVR OFF"
)
# The tests open windows of the dummy video driver of SDL, which draws nothing and needs no display, so only a tree with the tests builds it, and no SDK or app carries it.
if(HAYLEN_BUILD_TESTS)
  list(APPEND haylen_sdl_options "SDL_DUMMYVIDEO ON")
else()
  list(APPEND haylen_sdl_options "SDL_DUMMYVIDEO OFF")
endif()
# Apple platforms draw through Metal alone, so their SDL carries neither OpenGL nor Vulkan, and UIKit apps read their controllers through GameController, so their SDL reads none.
if(APPLE)
  list(APPEND haylen_sdl_options "SDL_OPENGL OFF" "SDL_OPENGLES OFF" "SDL_VULKAN OFF")
endif()
if(HAYLEN_PLATFORM MATCHES "^(ios|tvos)$")
  list(APPEND haylen_sdl_options "SDL_JOYSTICK OFF" "SDL_HIDAPI OFF")
endif()
# SDL turns the keys of hardware keyboards on iOS, iPadOS, Mac Catalyst and tvOS into keys only while GameController reads no keyboard, and the arrows and escape of a keyboard into key commands too, while the engine reads every key of a keyboard through GameController, so the patch leaves keyboards to GameController and turns the presses of remotes and game controllers into keys whether a keyboard is connected or not. It also drops the swipes of the Apple TV remote that SDL turns into arrows, since the engine reads the touch surface of the remote as its stick.
# SDL hides, shows and styles the status bar through UIApplication, which does nothing while view controllers decide the status bar and which iOS 27 reports as a misuse at every call, so the next patch leaves the status bar to the view controllers.
# SDL covers a UIKit scene with a launch window of its own and opens the window of the app only after the scene connected, which shows a black frame as that window takes over, so the next patch starts the app while the scene connects, without the launch window.
# SDL reads mice on tvOS only when the Info.plist has the key UIApplicationSupportsIndirectInputEvents, which only iOS and iPadOS define, and logs at every launch that it is missing, so the next patch reads them on tvOS without it.
# SDL paces the main callbacks of UIKit by a display link at the highest rate of the display, whatever rate the app asks for, so the next patch makes that display link follow the hint of the callback rate.
# SDL reads the keys of the whole page on the web, so a page that hosts the app next to elements of its own, such as an editor, could keep their keys from the app only by stopping them before they reach those elements, so the next patch skips the keys that the runtime marks.
# The Direct3D devices compose transparent windows of Windows through DirectComposition, whose visual the redirection surface of the window covers, so the next patch creates such windows without one.
# SDL drops the copy of a click that X11 reports from both a device and the virtual pointer by the state of the buttons, which a quick click has already left, so the last patch drops the copy with the same source, button and time.
if(NOT ANDROID AND NOT HAYLEN_PORT_PLATFORM)
  set(sdl_patches "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-uikit-presses.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-uikit-status-bar.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-uikit-scene-launch.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-uikit-tvos-mouse.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-uikit-frame-rate.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-web-page-keys.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-windows-composition.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/sdl-x11-quick-clicks.patch")
  set(sdl_hash f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f)
  haylen_patched_package_key(sdl_cache_key ${sdl_hash} ${sdl_patches})
  # SDL picks its UIKit backend by the system name, which Mac Catalyst builds name Darwin for the macOS SDK they compile against, so SDL configures with the name of iOS there.
  if(HAYLEN_CATALYST)
    set(CMAKE_SYSTEM_NAME iOS)
  endif()
  CPMAddPackage(
    NAME SDL3
    VERSION 3.4.20
    URL "https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz"
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

  # SDL compiles every source with a precompiled header of its own, and a compiler cache refuses every compile of MSVC that uses or makes one, so SDL builds without it there and each of its sources comes from the cache.
  if(MSVC)
    set_property(TARGET SDL3-static PROPERTY DISABLE_PRECOMPILE_HEADERS ON)
  endif()

  # SDL links its frameworks through the framework feature of `LINK_LIBRARY`, whose name CMake drops from the link line while it keeps the flag when another library links the same framework with a plain flag first, such as Foundation through Varn, so SDL links them with plain flags as the other libraries do.
  if(APPLE)
    foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
      get_target_property(sdl_links SDL3-static ${property})
      string(REGEX REPLACE "\\$<LINK_LIBRARY:FRAMEWORK,([A-Za-z]+)>" "-framework \\1" sdl_links "${sdl_links}")
      string(REGEX REPLACE "\\$<LINK_LIBRARY:WEAK_FRAMEWORK,([A-Za-z]+)>" "-weak_framework \\1" sdl_links "${sdl_links}")
      set_property(TARGET SDL3-static PROPERTY ${property} "${sdl_links}")
    endforeach()
    # Apple platforms keep the threads of POSIX in the system library that every program links, and libuv names it again for Varn, so the link option of SDL that names it a third time goes, since the linker warns about the duplicate.
    get_target_property(sdl_options SDL3-static INTERFACE_LINK_OPTIONS)
    list(REMOVE_ITEM sdl_options "-lpthread")
    set_property(TARGET SDL3-static PROPERTY INTERFACE_LINK_OPTIONS "${sdl_options}")
  endif()
endif()

# GameActivity of the AndroidX games libraries hosts Android apps. The engine port `android` of the tooling builds the player outside Gradle, so the engine links the static library of its native side from the prefab folder of the AAR, whose Java classes the Android library and template use at the same version.
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
  URL "https://github.com/nothings/stb/archive/d14d46f6b44456eb88beac58cba833a4135d4c89.tar.gz"
  URL_HASH SHA256=1b59a668a789a28a8fd4e01ff1413eec7eaeb28688bafdc66ec7068f4e7095e1
  DOWNLOAD_ONLY YES
)

# NanoSVG reads SVG documents into cubic curves and rasterizes them with anti-aliasing, which vector images rasterize their sizes with. It publishes no releases, so it is pinned to a commit of its default branch.
# NanoSVG hides the shapes of `display` none but draws those whose `visibility` is hidden, such as the background a drawing program leaves in a logo, so a patch hides those too.
# NanoSVG reads the unitless coordinates of a gradient in the bounding box of its shape, the default units of SVG, as lengths of the document, which collapses the gradient, so a patch reads them as fractions of the box, which it stretches over the shape after the gradient transform.
set(nanosvg_patches "${CMAKE_CURRENT_LIST_DIR}/patches/nanosvg-visibility.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/nanosvg-bounding-box-gradients.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/nanosvg-style-bounds.patch")
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
  URL "https://github.com/harfbuzz/harfbuzz/releases/download/15.0.0/harfbuzz-15.0.0.tar.xz"
  URL_HASH SHA256=968536e9abd782f07f7954044c0c4020199fcdd39b41d221ec42d2a3be4a44df
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
  URL "https://github.com/google/budoux/archive/refs/tags/v0.10.2.tar.gz"
  URL_HASH SHA256=4f652e7f1ea1d8b3850743ad1a8aa4866fcbb6ce7c9b7c34a86cc8d548b34a86
  DOWNLOAD_ONLY YES
)

# Parses floating point numbers the way `std::from_chars` does, which the C++ library of Apple platforms offers only from iOS and tvOS 26 and macOS 26.
CPMAddPackage(
  NAME fast_float
  URL "https://github.com/fastfloat/fast_float/archive/refs/tags/v8.3.3.tar.gz"
  URL_HASH SHA256=4dd695f051893c8218faf2ed9871fa4a4990aa6aee2d3190ed7bfa2e83092245
  DOWNLOAD_ONLY YES
)

CPMAddPackage(
  NAME imgui
  URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.93.0.tar.gz"
  URL_HASH SHA256=f7bf6bc76e5d79d40d751096663e6c5d18daf08129dd52e7f7b09e4251c8eeee
  DOWNLOAD_ONLY YES
)

CPMAddPackage(
  NAME miniaudio
  URL "https://github.com/mackron/miniaudio/archive/refs/tags/0.11.26.tar.gz"
  URL_HASH SHA256=838ba55a21c9142adf7b76bdc4351e5cd8b7ae128f46b25ec25f94ae9969c600
  DOWNLOAD_ONLY YES
)

# Box2D recycles the contacts of shapes that barely moved with the material they had, so a patch makes a new material of a shape reach its contacts in the next step, and another one finds the points that lie on a diagonal of a polygon inside it.
set(box2d_patches "${CMAKE_CURRENT_LIST_DIR}/patches/box2d-material-contacts.patch" "${CMAKE_CURRENT_LIST_DIR}/patches/box2d-point-in-polygon.patch")
set(box2d_hash ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d)
haylen_patched_package_key(box2d_cache_key ${box2d_hash} ${box2d_patches})
CPMAddPackage(
  NAME box2d
  URL "https://github.com/erincatto/box2d/archive/refs/tags/v3.2.0.tar.gz"
  URL_HASH SHA256=${box2d_hash}
  PATCHES ${box2d_patches}
  CUSTOM_CACHE_KEY ${box2d_cache_key}
  SYSTEM YES
)

# Clipper2 computes the polygon booleans, offsets and constrained triangulations of `math::Polygon`. Its core header uses `std::back_inserter` without the header `iterator` that declares it, which the standard library of Emscripten does not bring in through another header, so a patch includes it.
set(clipper2_patches "${CMAKE_CURRENT_LIST_DIR}/patches/clipper2-iterator.patch")
set(clipper2_hash 2a3693aceab4aed3e39b743e038d87701acc53cf05ed7b2013aab3e0aec5287e)
haylen_patched_package_key(clipper2_cache_key ${clipper2_hash} ${clipper2_patches})
CPMAddPackage(
  NAME clipper2
  URL "https://github.com/AngusJohnson/Clipper2/archive/refs/tags/Clipper2_2.0.1.tar.gz"
  URL_HASH SHA256=${clipper2_hash}
  PATCHES ${clipper2_patches}
  CUSTOM_CACHE_KEY ${clipper2_cache_key}
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
    URL "https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.367.tar.gz"
    URL_HASH SHA256=0df7d58b5df617e0a6a17dec521af459206719f9c7cb12d05d09275597c16f70
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

if(HAYLEN_BUILD_TESTS AND (HAYLEN_DESKTOP OR HAYLEN_GPU_TESTS_ALONE))
  CPMAddPackage(
    NAME googletest
    URL "https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz"
    URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
    OPTIONS "BUILD_GMOCK OFF" "INSTALL_GTEST OFF" "gtest_force_shared_crt ON"
    SYSTEM YES
  )
endif()

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

# The amalgamated source of HarfBuzz builds its OpenType shaper alone, without the shapers of legacy and AAT fonts. It reads every font file of the engine, and its rasterizers draw the glyphs of Dear ImGui and paint the color glyphs of COLR fonts. The readers of bitmap and SVG glyphs stay out, since the engine draws neither.
add_library(haylen-harfbuzz STATIC "${harfbuzz_SOURCE_DIR}/src/harfbuzz.cc" "${harfbuzz_SOURCE_DIR}/src/hb-raster-draw.cc" "${harfbuzz_SOURCE_DIR}/src/hb-raster-image.cc" "${harfbuzz_SOURCE_DIR}/src/hb-raster-paint.cc")
target_include_directories(haylen-harfbuzz SYSTEM PUBLIC "${harfbuzz_SOURCE_DIR}/src")
target_compile_definitions(haylen-harfbuzz PRIVATE HB_MINI HB_NO_BITMAP HB_NO_SVG)

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
  # The miniaudio library links the audio frameworks of the system instead of opening them while it runs, which it does on macOS otherwise, so the engine references the frameworks that it lists.
  target_compile_definitions(haylen-miniaudio PUBLIC MA_NO_RUNTIME_LINKING)
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

haylen_check_dependency_memory(haylen-nanosvg haylen-harfbuzz haylen-sheenbidi haylen-unibreak haylen-monocypher haylen-miniaudio)

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
