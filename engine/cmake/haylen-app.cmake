# Apps deploy their package into the app of each platform. A package is app.json, the source folder and the content folder, so nothing else that shares the package folder ships.
# Desktop builds link those entries into an "app" folder next to the executable, so edited files show up without a rebuild.
# Apple bundles carry them under Resources/app.
# Web builds preload them at /app in the virtual file system.
# Android apps take them into the APK assets of their Gradle project, which haylen.py assembles from the Android template, so the build records the package folder next to the library.
# With HAYLEN_PROTECTED_RELEASE every app ships its protected release instead, built by the content tool with the keys of the app, which the app opens with the bootstrap the build compiles into it, in the same places, and a web page preloads only the manifests and the app domain and streams the rest from the "app" folder next to it.

option(HAYLEN_PROTECTED_RELEASE "Ships the package of every app as its protected release, with the bootstrap of the app compiled in." OFF)
set(HAYLEN_CONTENT_TOOL "" CACHE FILEPATH "The content tool of the host, haylen-content, which builds protected releases. Empty uses the haylen-content target of the build.")
set(HAYLEN_PREVIOUS_RELEASE "" CACHE PATH "A folder that holds the last release of the app that shipped, which the protected release follows instead of the last release of the build tree, so a clean machine keeps the shards of unchanged content. Empty follows the build tree.")

# The system frameworks that the engine needs on each Apple platform. The runtime links them, and the Apple artifacts publish them in haylen-frameworks.json, from which haylen.py links them into the projects of Lua apps.
# Mac Catalyst reads the battery of the Mac from the power sources of IOKit, and every Apple platform decodes videos with AVFoundation and CoreMedia.
set(HAYLEN_APPLE_FRAMEWORKS_MACOS Cocoa CoreGraphics QuartzCore Metal MetalKit GameController AVFoundation AudioToolbox CoreAudio CoreMedia Network CoreVideo IOKit UniformTypeIdentifiers)
set(HAYLEN_APPLE_FRAMEWORKS_IOS Foundation UIKit CoreGraphics QuartzCore Metal MetalKit GameController AVFoundation AudioToolbox CoreAudio CoreMedia Network CoreVideo UniformTypeIdentifiers)
set(HAYLEN_APPLE_FRAMEWORKS_MACCATALYST ${HAYLEN_APPLE_FRAMEWORKS_IOS} IOKit)
set(HAYLEN_APPLE_FRAMEWORKS_TVOS ${HAYLEN_APPLE_FRAMEWORKS_IOS})

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
    target_link_libraries(${target} ${scope} xinput shell32 shcore shlwapi ole32 oleaut32 imm32 advapi32 uuid mfplat mfreadwrite mfuuid runtimeobject uiautomationcore)
    if(HAYLEN_GRAPHICS STREQUAL "D3D11")
      target_link_libraries(${target} ${scope} d3d11 dxgi)
    else()
      target_link_libraries(${target} ${scope} opengl32)
    endif()
  elseif(HAYLEN_PLATFORM STREQUAL "android")
    # The context of OpenGL ES comes from EGL, through which the device of OpenGL ES loads its functions, the video streams of plugins read the pixels of bitmaps through `jnigraphics`, and videos decode through `mediandk`.
    target_link_libraries(${target} ${scope} android log EGL jnigraphics mediandk)
    # GameActivity finds its native side through this JNI entry, which nothing in the library references.
    target_link_options(${target} INTERFACE "LINKER:-u,Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")
  elseif(HAYLEN_PLATFORM STREQUAL "web")
    target_link_options(${target} INTERFACE
      -sALLOW_MEMORY_GROWTH=1
      -sSTACK_SIZE=1048576
      -lidbfs.js
      "-sEXPORTED_RUNTIME_METHODS=[ccall,UTF8ToString,stringToNewUTF8,HEAPU8,HEAPU32,HEAPF32,FS,IDBFS,addRunDependency,removeRunDependency]"
      "-sEXPORTED_FUNCTIONS=[_main,_malloc,_free,_haylen_web_resolve,_haylen_web_emit,_haylen_web_finish_screen,_haylen_web_restore_screen,_haylen_web_load_zip,_haylen_web_clear_files,_haylen_web_set_file,_haylen_web_remove_file,_haylen_web_run_files,_haylen_web_console,_haylen_web_restart,_haylen_web_stop,_haylen_web_set_paused,_haylen_web_paused,_haylen_web_apply_changes,_haylen_web_last_error,_haylen_web_visibility,_haylen_web_page_hidden,_haylen_web_network,_haylen_web_theme,_haylen_web_accessibility,_haylen_web_accessibility_action,_haylen_web_battery,_haylen_web_resolve_dialog,_haylen_web_text_edited,_haylen_web_text_action,_haylen_web_keyboard,_haylen_web_socket_opened,_haylen_web_socket_received,_haylen_web_socket_closed,_haylen_web_socket_failed,_haylen_web_audio_render,_haylen_web_open_video_stream,_haylen_web_push_video_frame,_haylen_web_open_audio_stream,_haylen_web_push_audio_frames,_haylen_web_media_command]"
      "--pre-js=${HAYLEN_ENGINE_DIR}/platform/web/haylen-runtime.js"
    )
    if(HAYLEN_GRAPHICS STREQUAL "WGPU")
      target_link_options(${target} INTERFACE --use-port=emdawnwebgpu)
      target_compile_options(${target} ${scope} --use-port=emdawnwebgpu)
    else()
      target_link_options(${target} INTERFACE -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
    endif()
  else()
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(${target} ${scope} X11::X11 X11::Xi X11::Xcursor X11::Xext X11::Xrandr OpenGL::GL ${CMAKE_DL_LIBS} m pthread)
  endif()
endfunction()

function(haylen_mark_resources)
  cmake_parse_arguments(arg "" "BASEDIR;RESOURCEBASE" "FILES" ${ARGN})

  foreach(file IN LISTS arg_FILES)
    file(RELATIVE_PATH relative "${arg_BASEDIR}" "${file}")
    get_filename_component(location "${relative}" DIRECTORY)
    set_source_files_properties("${file}" PROPERTIES MACOSX_PACKAGE_LOCATION "${arg_RESOURCEBASE}/${location}" HEADER_FILE_ONLY ON)
    string(REPLACE "/" "\\\\" group "${arg_RESOURCEBASE}/${location}")
    source_group("${group}" FILES "${file}")
  endforeach()
endfunction()

function(haylen_link_package target folder plugins)
  set(package_target "${target}-package")
  set(entries app.json source content ${plugins})
  list(JOIN entries "|" entries)
  add_custom_target(${package_target} ALL
    COMMAND "${CMAKE_COMMAND}" -D "SOURCE=${folder}" -D "DESTINATION=$<TARGET_FILE_DIR:${target}>/app" -D "ENTRIES=${entries}"
            -P "${HAYLEN_CMAKE_DIR}/haylen-link-content.cmake"
    COMMENT "Linking the app package of ${target}"
    VERBATIM
  )
  add_dependencies(${package_target} ${target})
  set_target_properties(${package_target} PROPERTIES FOLDER Utils)
endfunction()

function(haylen_setup_web_page target shell)
  if(NOT shell)
    set(shell "${HAYLEN_ENGINE_DIR}/platform/web/shell.html")
  endif()
  set_target_properties(${target} PROPERTIES SUFFIX ".html")
  target_link_options(${target} PRIVATE "--shell-file=${shell}")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${shell}" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-runtime.js" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-audio-worklet.js")

  # The WebGPU and WebGL2 bundle of haylen.py run-cpp builds its page from the same shell, so the shell travels with the build output, next to the engine logo that the default shell shows as its icon.
  # The runtime loads the processor of its audio output and the worker of its content store from next to its script, so both travel with the build output too.
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${shell}" "$<TARGET_FILE_DIR:${target}>/${target}.shell.html"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-logo.svg" "$<TARGET_FILE_DIR:${target}>/haylen-logo.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-audio-worklet.js" "$<TARGET_FILE_DIR:${target}>/haylen-audio-worklet.js"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-content-store.js" "$<TARGET_FILE_DIR:${target}>/haylen-content-store.js"
    VERBATIM
  )
endfunction()

function(haylen_setup_apple_bundle target project_dir display_name identifier version orientation show_in_taskbar status_bar)
  if(HAYLEN_PLATFORM STREQUAL "macos")
    # An app without a taskbar button has no Dock icon, not even while it starts.
    set(HAYLEN_UI_ELEMENT "")
    if(NOT show_in_taskbar)
      set(HAYLEN_UI_ELEMENT "\t<key>LSUIElement</key>\n\t<true/>\n")
    endif()
    set(plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist.in")
    configure_file("${project_dir}/mac/Info.plist.in" "${plist}" @ONLY)
  elseif(HAYLEN_PLATFORM STREQUAL "ios")
    # The supported orientations follow the orientation of app.json, and the iPhone does not turn upside down.
    set(left "\t\t<string>UIInterfaceOrientationLandscapeLeft</string>\n\t\t<string>UIInterfaceOrientationLandscapeRight</string>\n")
    set(up "\t\t<string>UIInterfaceOrientationPortrait</string>\n")
    set(down "\t\t<string>UIInterfaceOrientationPortraitUpsideDown</string>\n")
    if(orientation STREQUAL "landscape")
      set(HAYLEN_ORIENTATIONS "${left}")
      set(HAYLEN_ORIENTATIONS_IPAD "${left}")
    elseif(orientation STREQUAL "portrait")
      set(HAYLEN_ORIENTATIONS "${up}")
      set(HAYLEN_ORIENTATIONS_IPAD "${up}${down}")
    else()
      set(HAYLEN_ORIENTATIONS "${up}${left}")
      set(HAYLEN_ORIENTATIONS_IPAD "${up}${down}${left}")
    endif()
    # The app launches with the status bar of app.json, which the view controller of the runtime keeps until the engine applies the same value.
    set(HAYLEN_STATUS_BAR_HIDDEN "false")
    set(HAYLEN_STATUS_BAR_STYLE "UIStatusBarStyleDefault")
    if(status_bar STREQUAL "hidden")
      set(HAYLEN_STATUS_BAR_HIDDEN "true")
    elseif(status_bar STREQUAL "light")
      set(HAYLEN_STATUS_BAR_STYLE "UIStatusBarStyleLightContent")
    elseif(status_bar STREQUAL "dark")
      set(HAYLEN_STATUS_BAR_STYLE "UIStatusBarStyleDarkContent")
    endif()
    set(plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist.in")
    configure_file("${project_dir}/ios/Info.plist.in" "${plist}" @ONLY)
    set(launch_screen "${project_dir}/ios/LaunchScreen.storyboard")
  else()
    set(plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist.in")
    configure_file("${project_dir}/tvos/Info.plist.in" "${plist}" @ONLY)
    set(launch_screen "${project_dir}/tvos/LaunchScreen.storyboard")
  endif()

  set_target_properties(${target} PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST "${plist}"
    MACOSX_BUNDLE_BUNDLE_NAME "${display_name}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${identifier}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${version}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${version}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${identifier}"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
  )

  if(HAYLEN_PLATFORM STREQUAL "ios")
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2" XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "16.3")
  elseif(HAYLEN_PLATFORM STREQUAL "tvos")
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3" XCODE_ATTRIBUTE_TVOS_DEPLOYMENT_TARGET "16.3")
  endif()

  # The privacy manifest declares the APIs with required reasons that the engine calls.
  set(resources "${project_dir}/PrivacyInfo.xcprivacy" ${launch_screen})
  target_sources(${target} PRIVATE ${resources})
  set_source_files_properties(${resources} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

  # Xcode signs the bundles it builds, while other generators leave only the signature the linker gives the executable, which binds neither the bundle identifier nor the Info.plist. macOS places the window of an unsigned Mac Catalyst app at the top left, whatever frame the app asks for, so the bundle is signed ad hoc.
  if(CMAKE_CXX_COMPILER_TARGET MATCHES "-macabi$" AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND codesign --force --sign - --timestamp=none "$<TARGET_BUNDLE_DIR:${target}>"
      VERBATIM
    )
  endif()
endfunction()

# Lists the entries of the plugins that `app.json` names which belong to the package, `plugins/<id>/plugin.json` and `plugins/<id>/source` of each one, relative to the package.
function(haylen_read_app_plugins package output)
  file(READ "${package}/app.json" document)
  set(entries "")
  string(JSON count ERROR_VARIABLE missing LENGTH "${document}" plugins)
  if(NOT missing AND count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
      string(JSON id MEMBER "${document}" plugins ${index})
      list(APPEND entries "plugins/${id}/plugin.json")
      if(IS_DIRECTORY "${package}/plugins/${id}/source")
        list(APPEND entries "plugins/${id}/source")
      endif()
    endforeach()
  endif()
  set(${output} "${entries}" PARENT_SCOPE)
endfunction()

# Builds the Apple parts of the plugins of an app into it from the file that HAYLEN_PLUGINS_FILE names, which haylen.py run-cpp writes from app.json and the plugins: their Objective-C, C and Swift sources, where the Swift files see the plugin API of the engine through its bridging header and take the Swift helpers of the engine, the system frameworks they link, their resources and the keys they add to the Info.plist.
function(haylen_add_apple_plugins target)
  set(HAYLEN_PLUGIN_KEYS "" PARENT_SCOPE)
  if(NOT HAYLEN_PLUGINS_FILE OR NOT EXISTS "${HAYLEN_PLUGINS_FILE}")
    return()
  endif()
  include("${HAYLEN_PLUGINS_FILE}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HAYLEN_PLUGINS_FILE}")
  target_sources(${target} PRIVATE ${HAYLEN_PLUGIN_SOURCES})
  set(swift ${HAYLEN_PLUGIN_SOURCES})
  list(FILTER swift INCLUDE REGEX "\\.swift$")
  set(objc ${HAYLEN_PLUGIN_SOURCES})
  list(FILTER objc INCLUDE REGEX "\\.mm?$")
  set_source_files_properties(${objc} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  if(swift)
    if(NOT CMAKE_Swift_COMPILER)
      message(FATAL_ERROR "The plugins of ${target} have Swift sources, which need the Swift language. Add \"enable_language(Swift)\" to the CMake project on Apple platforms, as the C++ template does.")
    endif()
    target_sources(${target} PRIVATE "${HAYLEN_ENGINE_DIR}/platform/apple/HaylenBridgeAsync.swift")
    set(bridging "${HAYLEN_ENGINE_DIR}/platform/apple/HaylenPlugins.h")
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${bridging}" XCODE_ATTRIBUTE_SWIFT_VERSION "5.0")
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-import-objc-header ${bridging}>" "$<$<COMPILE_LANGUAGE:Swift>:SHELL:-Xcc -I${HAYLEN_ENGINE_DIR}/include>")
  endif()
  foreach(framework IN LISTS HAYLEN_PLUGIN_FRAMEWORKS)
    target_link_libraries(${target} PRIVATE "-framework ${framework}")
  endforeach()
  if(HAYLEN_PLUGIN_RESOURCES)
    target_sources(${target} PRIVATE ${HAYLEN_PLUGIN_RESOURCES})
    set_source_files_properties(${HAYLEN_PLUGIN_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  endif()
  set(HAYLEN_PLUGIN_KEYS "${HAYLEN_PLUGIN_KEYS}" PARENT_SCOPE)
endfunction()

# Reads the name, identifier and version of an app package, so the app carries the same values the runtime reads.
function(haylen_read_app_info target package)
  if(NOT EXISTS "${package}/app.json")
    message(FATAL_ERROR "The app package of ${target} needs an app.json: ${package}")
  endif()
  file(READ "${package}/app.json" document)
  foreach(key name identifier version)
    string(JSON value ERROR_VARIABLE error GET "${document}" ${key})
    if(error OR value STREQUAL "")
      message(FATAL_ERROR "The app.json of ${target} needs a ${key} to build an app: ${package}/app.json")
    endif()
    set(app_${key} "${value}" PARENT_SCOPE)
  endforeach()

  # The orientation defaults to landscape, like the runtime.
  string(JSON orientation ERROR_VARIABLE error GET "${document}" orientation)
  if(error)
    set(orientation "landscape")
  endif()
  set(app_orientation "${orientation}" PARENT_SCOPE)

  # Windows show in the taskbar unless the window section of app.json says otherwise, like the runtime.
  string(JSON show_in_taskbar ERROR_VARIABLE error GET "${document}" window showInTaskbar)
  if(error)
    set(show_in_taskbar ON)
  endif()
  set(app_show_in_taskbar "${show_in_taskbar}" PARENT_SCOPE)

  # The status bar of phones and tablets stays hidden unless the window section of app.json shows it, like the runtime.
  string(JSON status_bar ERROR_VARIABLE error GET "${document}" window statusBar)
  if(error)
    set(status_bar "hidden")
  endif()
  set(app_status_bar "${status_bar}" PARENT_SCOPE)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${package}/app.json")
endfunction()

# Creates an app from its package folder, named after the name, identifier and version of its app.json.
#   haylen_add_app(<target> PACKAGE <folder> [SOURCES <files>...] [CPP] [APPLE_PROJECT <folder>] [WEB_SHELL <file>])
# Lua apps need no sources. C++ apps pass CPP and define haylen::core::Application::create in their sources.
function(haylen_add_app target)
  cmake_parse_arguments(arg "CPP" "PACKAGE;APPLE_PROJECT;WEB_SHELL" "SOURCES" ${ARGN})
  haylen_read_app_info(${target} "${arg_PACKAGE}")

  set(sources ${arg_SOURCES})
  if(NOT arg_CPP)
    list(APPEND sources "${HAYLEN_ENGINE_DIR}/src/platform/LuaPlayer.cpp")
  endif()
  # The Apple runtime leaves main to the app, which enters it through haylen_main.
  if(APPLE)
    list(APPEND sources "${HAYLEN_ENGINE_DIR}/src/platform/apple/AppleMain.cpp")
  endif()

  if(ANDROID)
    add_library(${target} SHARED ${sources})
    # The activity loads the library by its exact name, while Poco forces a Debug postfix into the cache for every library after it.
    set_target_properties(${target} PROPERTIES DEBUG_POSTFIX "" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${target}/$<0:>")
  else()
    add_executable(${target} ${sources})
    # The generator expression keeps multi-config generators such as Xcode and Visual Studio from adding a folder per configuration.
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${target}/$<0:>")
  endif()
  target_link_libraries(${target} PRIVATE haylen::runtime)
  get_filename_component(package "${arg_PACKAGE}" ABSOLUTE)
  file(WRITE "${CMAKE_BINARY_DIR}/bin/${target}/package.txt" "${package}")
  haylen_read_app_plugins("${package}" plugin_entries)
  if(HAYLEN_PROTECTED_RELEASE)
    haylen_add_release(${target} "${package}" "${arg_APPLE_PROJECT}" "${arg_WEB_SHELL}")
    return()
  endif()

  if(APPLE)
    set(project_dir "${arg_APPLE_PROJECT}")
    if(NOT project_dir)
      set(project_dir "${HAYLEN_ENGINE_DIR}/platform/apple")
    endif()
    set(globs "${arg_PACKAGE}/source/*" "${arg_PACKAGE}/content/*")
    foreach(entry IN LISTS plugin_entries)
      list(APPEND globs "${arg_PACKAGE}/${entry}" "${arg_PACKAGE}/${entry}/*")
    endforeach()
    file(GLOB_RECURSE package_files CONFIGURE_DEPENDS ${globs})
    list(FILTER package_files EXCLUDE REGEX "/\\.DS_Store$")
    list(APPEND package_files "${arg_PACKAGE}/app.json")
    haylen_mark_resources(FILES ${package_files} BASEDIR "${arg_PACKAGE}" RESOURCEBASE "Resources/app")
    target_sources(${target} PRIVATE ${package_files})
    haylen_add_apple_plugins(${target})
    haylen_setup_apple_bundle(${target} "${project_dir}" "${app_name}" "${app_identifier}" "${app_version}" "${app_orientation}" "${app_show_in_taskbar}" "${app_status_bar}")
  elseif(EMSCRIPTEN)
    haylen_setup_web_page(${target} "${arg_WEB_SHELL}")
    target_link_options(${target} PRIVATE "--preload-file=${arg_PACKAGE}/app.json@/app/app.json")
    foreach(folder source content ${plugin_entries})
      if(EXISTS "${arg_PACKAGE}/${folder}")
        target_link_options(${target} PRIVATE "--preload-file=${arg_PACKAGE}/${folder}@/app/${folder}")
      endif()
    endforeach()
  elseif(NOT ANDROID)
    haylen_link_package(${target} "${arg_PACKAGE}" "${plugin_entries}")
    haylen_setup_windows_app(${target})
  endif()
endfunction()

# Windows apps embed the application manifest of the engine, which selects the Common Controls that message dialogs need and UTF-8 as the code page of the process.
function(haylen_setup_windows_app target)
  if(WIN32)
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")
    target_sources(${target} PRIVATE "${HAYLEN_ENGINE_DIR}/platform/windows/haylen.manifest")
  endif()
endfunction()

# The key folder of an app, as haylen.py finds it: under the folder that HAYLEN_KEYS_DIR names in continuous integration, or else under the configuration folder of the user, never inside a project.
function(haylen_keys_folder identifier output)
  if(DEFINED ENV{HAYLEN_KEYS_DIR} AND NOT "$ENV{HAYLEN_KEYS_DIR}" STREQUAL "")
    set(root "$ENV{HAYLEN_KEYS_DIR}")
  elseif(CMAKE_HOST_APPLE)
    set(root "$ENV{HOME}/Library/Application Support/Haylen/keys")
  elseif(CMAKE_HOST_WIN32)
    file(TO_CMAKE_PATH "$ENV{APPDATA}/Haylen/keys" root)
  elseif(DEFINED ENV{XDG_CONFIG_HOME} AND NOT "$ENV{XDG_CONFIG_HOME}" STREQUAL "")
    set(root "$ENV{XDG_CONFIG_HOME}/haylen/keys")
  else()
    set(root "$ENV{HOME}/.config/haylen/keys")
  endif()
  set(${output} "${root}/${identifier}" PARENT_SCOPE)
endfunction()

# Builds the protected release of an app and its bootstrap, and ships the part of the release that the app needs where the platform keeps the package.
function(haylen_add_release target package apple_project web_shell)
  set(tool "${HAYLEN_CONTENT_TOOL}")
  set(tool_target "")
  if(NOT tool AND TARGET haylen-content)
    set(tool "$<TARGET_FILE:haylen-content>")
    set(tool_target haylen-content)
  endif()
  if(NOT tool)
    message(FATAL_ERROR "A protected release needs the content tool of the host. Name it with \"HAYLEN_CONTENT_TOOL\", such as the haylen-content of the desktop artifacts that \"haylen.py engine --platform desktop\" builds.")
  endif()

  # The profile names the platform family of the release, and the build number comes from the version, 1.2.3 being 1002003. Android releases build shards that fill an install-time asset pack of haylen.py several times over, as CONTENT_SHARD_TARGETS of haylen.py says.
  set(shard_target "")
  if(APPLE)
    set(profile apple)
  elseif(ANDROID)
    set(profile android)
    set(shard_target 268435456)
  elseif(EMSCRIPTEN)
    set(profile web)
  elseif(WIN32)
    set(profile windows)
  else()
    set(profile linux)
  endif()
  string(REGEX MATCHALL "[0-9]+" parts "${app_version}")
  list(APPEND parts 0 0 0)
  list(GET parts 0 major)
  list(GET parts 1 minor)
  list(GET parts 2 patch)
  math(EXPR build "${major} * 1000000 + ${minor} * 1000 + ${patch}")
  file(READ "${package}/app.json" document)
  string(JSON server ERROR_VARIABLE missing GET "${document}" content updates baseUrl)
  if(missing)
    set(server "")
  endif()
  haylen_keys_folder("${app_identifier}" keys)

  # The build folder of the release records the package, which haylen.py inspects the built app against.
  set(folder "${CMAKE_BINARY_DIR}/haylen-release/${target}")
  file(WRITE "${folder}/package.txt" "${package}")
  set(stamp "${folder}/release.stamp")
  set(bootstrap "${folder}/HaylenBootstrap.cpp")
  set(boot "")
  if(EMSCRIPTEN)
    set(boot "${folder}/boot")
  endif()
  file(GLOB_RECURSE package_files CONFIGURE_DEPENDS "${package}/source/*" "${package}/content/*" "${package}/plugins/*")
  add_custom_command(OUTPUT "${stamp}"
    COMMAND "${CMAKE_COMMAND}" "-DTOOL=${tool}" "-DPACKAGE=${package}" "-DIDENTIFIER=${app_identifier}" "-DKEYS=${keys}" "-DPROFILE=${profile}" "-DBUILD=${build}" "-DRELEASE=${folder}/release" "-DPREVIOUS=${HAYLEN_PREVIOUS_RELEASE}" "-DCACHE=${folder}/cache" "-DSHIPPED=${folder}/shipped" "-DPACKS=${folder}/packs" "-DSERVER=${server}" "-DSHARD_TARGET=${shard_target}" "-DBOOT=${boot}" "-DSTAMP=${stamp}" -P "${HAYLEN_CMAKE_DIR}/haylen-release-content.cmake"
    DEPENDS "${package}/app.json" ${package_files} "${HAYLEN_CMAKE_DIR}/haylen-release-content.cmake" ${tool_target}
    COMMENT "Building the protected release of ${target}."
    VERBATIM
  )
  add_custom_command(OUTPUT "${bootstrap}"
    COMMAND "${tool}" bootstrap --keys "${keys}" --profile "${profile}" --build "${build}" --output "${bootstrap}"
    DEPENDS "${stamp}"
    COMMENT "Writing the bootstrap of ${target}."
    VERBATIM
  )
  add_custom_target(${target}-release DEPENDS "${stamp}")
  add_dependencies(${target} ${target}-release)
  target_sources(${target} PRIVATE "${bootstrap}")

  if(APPLE)
    # The release goes into the bundle before the bundle is signed, at the resources of the bundle, where macOS and Mac Catalyst keep them in Contents.
    set(destination "$<TARGET_BUNDLE_DIR:${target}>/app")
    if(HAYLEN_PLATFORM STREQUAL "macos" OR CMAKE_CXX_COMPILER_TARGET MATCHES "-macabi$")
      set(destination "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources/app")
    endif()
    haylen_copy_release(${target} "${folder}/shipped" "${destination}")
    set(project_dir "${apple_project}")
    if(NOT project_dir)
      set(project_dir "${HAYLEN_ENGINE_DIR}/platform/apple")
    endif()
    haylen_add_apple_plugins(${target})
    haylen_setup_apple_bundle(${target} "${project_dir}" "${app_name}" "${app_identifier}" "${app_version}" "${app_orientation}" "${app_show_in_taskbar}" "${app_status_bar}")
  elseif(EMSCRIPTEN)
    # The page preloads the manifests and the app domain, and the shards of the content stay in the "app" folder next to it, where downloads read them by range.
    haylen_setup_web_page(${target} "${web_shell}")
    file(WRITE "${folder}/haylen-release.js" "Module.haylen = Module.haylen || {};\nModule.haylen.release = {address: \"app/\", identifier: \"${app_identifier}\", files: []};\n")
    target_link_options(${target} PRIVATE "--preload-file=${boot}@/app" "--pre-js=${folder}/haylen-release.js")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${stamp}")
    haylen_copy_release(${target} "${folder}/release" "$<TARGET_FILE_DIR:${target}>/app")
  elseif(ANDROID)
    file(WRITE "${CMAKE_BINARY_DIR}/bin/${target}/release.txt" "${folder}/shipped")
  else()
    haylen_copy_release(${target} "${folder}/shipped" "$<TARGET_FILE_DIR:${target}>/app")
    haylen_setup_windows_app(${target})
  endif()
  haylen_harden_release(${target} "${folder}")
endfunction()

# Builds a release without the symbols of the app, which stay in the build folder of its release for crash reports: a dSYM on Apple platforms, the symbol map of Emscripten, the PDB of the public symbols on Windows and a debug file that the stripped binary names elsewhere.
function(haylen_harden_release target folder)
  if(APPLE)
    set_target_properties(${target} PROPERTIES XCODE_ATTRIBUTE_DEBUG_INFORMATION_FORMAT dwarf-with-dsym XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING YES XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING YES XCODE_ATTRIBUTE_STRIP_INSTALLED_PRODUCT YES XCODE_ATTRIBUTE_STRIP_STYLE all)
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

# Replaces the release folder of an app after each build, so a file the release no longer names never stays behind.
function(haylen_copy_release target source destination)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${destination}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${source}" "${destination}"
    VERBATIM
  )
endfunction()
