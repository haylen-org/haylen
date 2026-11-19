# The CMake API of apps: `haylen_add_app` builds the binary of an app against the engine, from the SDK that `find_package(haylen)` imports or from the engine that `add_subdirectory` adds.
# The tooling prepares the content that an app ships, the staged package of a Debug build or the protected release of a Release build with the bootstrap that the content tool writes for it, and the function places that content where the runtime of each platform reads it.
# Xcode builds the apps of every Apple platform from the projects of the tooling, so the function builds only plain macOS executables there, such as those of CMake projects that consume the engine.

# The system frameworks that the engine needs on each Apple platform. The runtime links them, and the Apple SDK publishes them in haylen-apple.json, from which the Xcode projects of apps link them.
# Every Apple platform finds the fonts of the system through CoreText, Mac Catalyst reads the battery of the Mac from the power sources of IOKit, every Apple platform decodes videos with AVFoundation and CoreMedia, and SDL reads the keyboard layouts of macOS through Carbon and its controllers through IOKit, GameController, CoreHaptics and ForceFeedback.
set(HAYLEN_APPLE_FRAMEWORKS_MACOS Cocoa Carbon CoreFoundation CoreGraphics CoreText QuartzCore Metal MetalKit GameController CoreHaptics ForceFeedback AVFoundation AudioToolbox CoreAudio CoreMedia Network CoreVideo IOKit UniformTypeIdentifiers)
set(HAYLEN_APPLE_FRAMEWORKS_IOS Foundation UIKit CoreGraphics CoreText QuartzCore Metal MetalKit GameController AVFoundation AudioToolbox CoreAudio CoreMedia Network CoreVideo UniformTypeIdentifiers)
set(HAYLEN_APPLE_FRAMEWORKS_MACCATALYST ${HAYLEN_APPLE_FRAMEWORKS_IOS} IOKit)
set(HAYLEN_APPLE_FRAMEWORKS_TVOS ${HAYLEN_APPLE_FRAMEWORKS_IOS})
set(HAYLEN_APPLE_FRAMEWORKS_VISIONOS ${HAYLEN_APPLE_FRAMEWORKS_IOS})

# Adds the system libraries and link options every runtime needs, with PUBLIC for the runtime the engine builds and INTERFACE for the one an installed SDK imports.
function(haylen_link_runtime_platform target scope)
  if(APPLE)
    if(HAYLEN_PLATFORM STREQUAL "macos")
      set(frameworks ${HAYLEN_APPLE_FRAMEWORKS_MACOS})
    elseif(CMAKE_CXX_COMPILER_TARGET MATCHES "-macabi$")
      set(frameworks ${HAYLEN_APPLE_FRAMEWORKS_MACCATALYST})
    elseif(HAYLEN_PLATFORM STREQUAL "ios")
      set(frameworks ${HAYLEN_APPLE_FRAMEWORKS_IOS})
    else()
      set(frameworks ${HAYLEN_APPLE_FRAMEWORKS_TVOS})
    endif()
    list(TRANSFORM frameworks PREPEND "-framework ")
    target_link_libraries(${target} ${scope} ${frameworks})
  elseif(HAYLEN_PLATFORM STREQUAL "windows")
    target_link_libraries(${target} ${scope} shell32 shcore shlwapi ole32 oleaut32 advapi32 uuid dwrite mfplat mfreadwrite mfuuid runtimeobject uiautomationcore)
  elseif(HAYLEN_PLATFORM STREQUAL "android")
    # The context of OpenGL ES comes from EGL, through which the device of OpenGL ES loads its functions, the video streams of plugins read the pixels of bitmaps through `jnigraphics`, and videos decode through `mediandk`.
    target_link_libraries(${target} ${scope} android log EGL jnigraphics mediandk)
    # Java finds the native sides of GameActivity and of the runtime through JNI entries that nothing in the library references, so every library that links the runtime takes the entry of GameActivity and `JNI_OnLoad`, whose file holds every JNI entry of the runtime.
    target_link_options(${target} INTERFACE "LINKER:-u,Java_com_google_androidgamesdk_GameActivity_initializeNativeCode" "LINKER:-u,JNI_OnLoad")
  elseif(HAYLEN_PLATFORM STREQUAL "web")
    target_link_options(${target} INTERFACE
      -sALLOW_MEMORY_GROWTH=1
      -sSTACK_SIZE=1048576
      -lidbfs.js
      "-sEXPORTED_RUNTIME_METHODS=[ccall,UTF8ToString,stringToNewUTF8,lengthBytesUTF8,stringToUTF8,HEAPU8,HEAPU32,HEAPF32,FS,IDBFS,addRunDependency,removeRunDependency]"
      "-sEXPORTED_FUNCTIONS=[_main,_malloc,_free,_haylen_web_resolve,_haylen_web_emit,_haylen_web_open_link,_haylen_web_finish_screen,_haylen_web_restore_screen,_haylen_web_load_zip,_haylen_web_clear_files,_haylen_web_edit,_haylen_web_run_files,_haylen_web_console,_haylen_web_inspect,_haylen_web_dispose,_haylen_web_stop,_haylen_web_set_paused,_haylen_web_paused,_haylen_web_apply_changes,_haylen_web_last_error,_haylen_web_visibility,_haylen_web_page_hidden,_haylen_web_network,_haylen_web_displays_changed,_haylen_web_theme,_haylen_web_languages,_haylen_web_accessibility,_haylen_web_accessibility_action,_haylen_web_battery,_haylen_web_time_zone,_haylen_web_text_edited,_haylen_web_text_action,_haylen_web_keyboard,_haylen_web_scroll,_haylen_web_socket_opened,_haylen_web_socket_received,_haylen_web_socket_closed,_haylen_web_socket_failed,_haylen_web_audio_render,_haylen_web_open_video_stream,_haylen_web_push_video_frame,_haylen_web_open_audio_stream,_haylen_web_push_audio_frames,_haylen_web_media_command,_varn_dataview_new,_varn_dataview_acquire,_varn_dataview_retain,_varn_dataview_release,_varn_dataview_data,_varn_dataview_size,_varn_dataview_id]"
      "--pre-js=${HAYLEN_VARN_DATAVIEW_SCRIPT}"
      "--pre-js=${HAYLEN_ENGINE_DIR}/platform/web/haylen-runtime.js"
      "--pre-js=${HAYLEN_ENGINE_DIR}/platform/web/haylen-page.js"
    )
    # The scripts go into the script of every app that links the runtime, so a change to them links the app again even when no library changed. The engine side of the runtime comes before its page side, which reaches it as it loads.
    set_property(TARGET ${target} APPEND PROPERTY INTERFACE_LINK_DEPENDS "${HAYLEN_VARN_DATAVIEW_SCRIPT}" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-runtime.js" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-page.js")
  else()
    # The window services of X11 call Xlib, its shape extension and RandR, while SDL loads the libraries of X11 and Wayland when the app starts.
    find_package(X11 REQUIRED)
    target_link_libraries(${target} ${scope} X11::X11 X11::Xext X11::Xrandr ${CMAKE_DL_LIBS} m pthread)
  endif()
endfunction()

function(haylen_setup_web_page target shell)
  if(NOT shell)
    set(shell "${HAYLEN_ENGINE_DIR}/platform/web/shell.html")
  endif()
  set_target_properties(${target} PROPERTIES SUFFIX ".html")
  target_link_options(${target} PRIVATE "--shell-file=${shell}")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${shell}" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-audio-worklet.js")

  # The shell travels with the build output, next to the engine logo that the default shell shows as its icon.
  # The runtime loads the processor of its audio output and the worker of its content store from next to its script, and a page of the Worker mode loads the page side of the runtime and the worker that runs it from there, so they travel with the build output too.
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${shell}" "$<TARGET_FILE_DIR:${target}>/${target}.shell.html"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-logo.svg" "$<TARGET_FILE_DIR:${target}>/haylen-logo.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-audio-worklet.js" "$<TARGET_FILE_DIR:${target}>/haylen-audio-worklet.js"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-content-store.js" "$<TARGET_FILE_DIR:${target}>/haylen-content-store.js"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-page.js" "$<TARGET_FILE_DIR:${target}>/haylen-page.js"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-worker.js" "$<TARGET_FILE_DIR:${target}>/haylen-worker.js"
    VERBATIM
  )
endfunction()

# Places the content of an app next to its binary after every build, where the runtime opens it as the folder "app": a link to the staged package of a Debug build, so the files that the tooling stages again show up without a link of the app, and a copy of a protected release, so a file that the release no longer holds never stays behind.
function(haylen_place_content target content destination release)
  if(release)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E rm -rf "${destination}"
      COMMAND "${CMAKE_COMMAND}" -E copy_directory "${content}" "${destination}"
      VERBATIM
    )
    return()
  endif()
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE=${content}" "-DDESTINATION=${destination}" -P "${HAYLEN_CMAKE_DIR}/haylen-link-content.cmake"
    VERBATIM
  )
endfunction()

# Windows apps embed the application manifest of the engine, which selects the Common Controls of version 6 for the controls that native libraries show and UTF-8 as the code page of the process.
function(haylen_setup_windows_app target)
  if(WIN32)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")
    target_sources(${target} PRIVATE "${HAYLEN_ENGINE_DIR}/platform/windows/haylen.manifest")
  endif()
endfunction()

# Builds a release without the symbols of the app, which stay in the folder `symbols/<target>` of the build tree for crash reports: a dSYM on macOS, the symbol map of Emscripten, the PDB of the public symbols on Windows and a debug file that the stripped binary names elsewhere.
function(haylen_harden_release target)
  set(folder "${CMAKE_BINARY_DIR}/symbols/${target}")
  file(MAKE_DIRECTORY "${folder}")
  if(APPLE)
    target_link_options(${target} PRIVATE "LINKER:-dead_strip")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND xcrun dsymutil "$<TARGET_FILE:${target}>" -o "${folder}/$<TARGET_FILE_NAME:${target}>.dSYM"
      COMMAND xcrun strip "$<TARGET_FILE:${target}>"
      VERBATIM
    )
  elseif(EMSCRIPTEN)
    # Emscripten minifies the runtime it links, and the workers next to the page go through the terser it ships.
    target_link_options(${target} PRIVATE --emit-symbol-map)
    set(terser "${EMSCRIPTEN_ROOT_PATH}/node_modules/terser/bin/terser")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E rename "$<TARGET_FILE:${target}>.symbols" "${folder}/$<TARGET_FILE_NAME:${target}>.symbols"
      COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} "${terser}" "$<TARGET_FILE_DIR:${target}>/haylen-audio-worklet.js" --compress --mangle --output "$<TARGET_FILE_DIR:${target}>/haylen-audio-worklet.js"
      COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} "${terser}" "$<TARGET_FILE_DIR:${target}>/haylen-content-store.js" --compress --mangle --output "$<TARGET_FILE_DIR:${target}>/haylen-content-store.js"
      VERBATIM
    )
  elseif(MSVC)
    target_link_options(${target} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    set_target_properties(${target} PROPERTIES PDB_OUTPUT_DIRECTORY "${folder}")
  else()
    target_link_options(${target} PRIVATE "LINKER:--gc-sections")
    if(ANDROID)
      # The library exports only the entry points that Java and GameActivity look up.
      file(WRITE "${folder}/exports.map" "{\n  global:\n    JNI_OnLoad;\n    Java_*;\n    GameActivity_onCreate;\n  local:\n    *;\n};\n")
      target_link_options(${target} PRIVATE "LINKER:--version-script=${folder}/exports.map")
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND "${CMAKE_OBJCOPY}" --only-keep-debug "$<TARGET_FILE:${target}>" "${folder}/$<TARGET_FILE_NAME:${target}>.debug"
      COMMAND "${CMAKE_OBJCOPY}" --strip-all "--add-gnu-debuglink=${folder}/$<TARGET_FILE_NAME:${target}>.debug" "$<TARGET_FILE:${target}>"
      VERBATIM
    )
  endif()
endfunction()

# Builds the binary of an app:
#   haylen_add_app(<target> LUA|CPP [SOURCES <files>...] CONTENT <folder> [BOOTSTRAP <file>] [WEB_SHELL <file>])
# - `LUA` links the Lua player of `haylen::player`, which defines the app, and `CPP` compiles the `SOURCES`, which define `haylen::core::Application::create`.
# - `CONTENT` is the folder that the app ships, the staged package of a Debug build or the protected release of a Release build.
# - `BOOTSTRAP` is the bootstrap source that the content tool writes for a protected release, which the binary compiles in and which makes the build a release build without symbols.
# - `WEB_SHELL` is the page of a web app, by default the shell of the engine.
# On desktops the app is an executable with its content in the folder "app" next to it, on Android the shared library that the activity loads while Gradle takes the content into the assets, and on the web a page that preloads its package or, for a release, streams it from the folder "app" next to it. A port builds the app with the function that `APP_FUNCTION` of its `haylen_port` names.
function(haylen_add_app target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "LUA;CPP" "CONTENT;BOOTSTRAP;WEB_SHELL" "SOURCES")
  if(arg_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "The app \"${target}\" passes \"${arg_UNPARSED_ARGUMENTS}\" to \"haylen_add_app\", which takes \"LUA\" or \"CPP\", \"SOURCES\", \"CONTENT\", \"BOOTSTRAP\" and \"WEB_SHELL\".")
  endif()
  if(arg_LUA STREQUAL arg_CPP)
    message(FATAL_ERROR "The app \"${target}\" names its kind with exactly one of \"LUA\", for the Lua player of the engine, and \"CPP\", for sources that define \"haylen::core::Application::create\".")
  endif()
  if(arg_LUA AND arg_SOURCES)
    message(FATAL_ERROR "The Lua app \"${target}\" runs the Lua player of the engine, which defines the app, so it takes no \"SOURCES\". Make it a \"CPP\" app to compile sources of its own.")
  endif()
  if(arg_CPP AND NOT arg_SOURCES)
    message(FATAL_ERROR "The C++ app \"${target}\" needs \"SOURCES\" that define \"haylen::core::Application::create\".")
  endif()
  if(NOT arg_CONTENT)
    message(FATAL_ERROR "The app \"${target}\" needs \"CONTENT\", the folder of the staged package or of the protected release that it ships.")
  endif()
  get_filename_component(content "${arg_CONTENT}" ABSOLUTE)
  if(NOT IS_DIRECTORY "${content}")
    message(FATAL_ERROR "The \"CONTENT\" of the app \"${target}\" names \"${content}\", which is no folder. Prepare the app first, which stages its content.")
  endif()
  if(arg_BOOTSTRAP AND NOT EXISTS "${arg_BOOTSTRAP}")
    message(FATAL_ERROR "The \"BOOTSTRAP\" of the app \"${target}\" names \"${arg_BOOTSTRAP}\", which does not exist. The content tool writes it with the protected release of the app.")
  endif()
  set(release OFF)
  if(arg_BOOTSTRAP)
    set(release ON)
  endif()

  set(sources ${arg_SOURCES} ${arg_BOOTSTRAP})
  set(libraries haylen::runtime)
  if(arg_LUA)
    list(APPEND libraries haylen::player)
  endif()

  if(HAYLEN_PORT_PLATFORM)
    if(NOT HAYLEN_PORT_APP_FUNCTION)
      message(FATAL_ERROR "The port of the platform \"${HAYLEN_PORT_PLATFORM}\" builds no apps, since its \"haylen_port\" names no \"APP_FUNCTION\".")
    endif()
    set(release_flag "")
    if(release)
      set(release_flag RELEASE)
    endif()
    cmake_language(CALL "${HAYLEN_PORT_APP_FUNCTION}" ${target} SOURCES ${sources} LIBRARIES ${libraries} CONTENT "${content}" ${release_flag})
    return()
  endif()
  if(APPLE AND NOT HAYLEN_PLATFORM STREQUAL "macos")
    message(FATAL_ERROR "The app \"${target}\" builds for \"${HAYLEN_PLATFORM}\", whose apps Xcode builds from the Apple project that the tooling generates, so \"haylen_add_app\" builds only plain macOS executables on Apple platforms.")
  endif()

  # The drivers of a laptop with two GPUs read which one the app wants from variables of the executable itself, and the macOS runtime leaves main to the executable, which enters it through haylen_main.
  if(WIN32)
    list(APPEND sources "${HAYLEN_ENGINE_DIR}/src/gpu/windows/GpuPreferenceExports.cpp")
  elseif(APPLE)
    list(APPEND sources "${HAYLEN_ENGINE_DIR}/src/platform/apple/AppleMain.cpp")
  endif()

  # The generator expression keeps multi-config generators such as Xcode and Visual Studio from adding a folder per configuration.
  if(ANDROID)
    add_library(${target} SHARED ${sources})
    set_target_properties(${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${target}/$<0:>")
  else()
    add_executable(${target} ${sources})
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${target}/$<0:>")
  endif()
  target_link_libraries(${target} PRIVATE ${libraries})
  # A Lua app may compile no source, so the language of the engine links it.
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)

  if(EMSCRIPTEN)
    haylen_setup_web_page(${target} "${arg_WEB_SHELL}")
  endif()
  if(EMSCRIPTEN AND release)
    # The page of a release downloads its manifests and its app domain before the app starts and streams the rest from the folder "app" next to it.
    haylen_place_content(${target} "${content}" "$<TARGET_FILE_DIR:${target}>/app" ON)
  elseif(EMSCRIPTEN)
    # The link preloads the package into the page, so a change to a file of the package links the page again.
    target_link_options(${target} PRIVATE "--preload-file=${content}@/app")
    file(GLOB_RECURSE package_files CONFIGURE_DEPENDS "${content}/*")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${package_files})
  elseif(NOT ANDROID)
    haylen_place_content(${target} "${content}" "$<TARGET_FILE_DIR:${target}>/app" ${release})
    haylen_setup_windows_app(${target})
  endif()

  if(release)
    haylen_harden_release(${target})
  endif()
endfunction()
