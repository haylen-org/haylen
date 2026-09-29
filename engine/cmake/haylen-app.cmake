# Apps deploy their package into the app of each platform. A package is app.json, the source folder and the content folder, so nothing else that shares the package folder ships.
# Desktop builds link those entries into an "app" folder next to the executable, so edited files show up without a rebuild.
# Apple bundles carry them under Resources/app.
# Web builds preload them at /app in the virtual file system.
# Android apps take them into the APK assets of their Gradle project, which make.py assembles from the Android template, so the build records the package folder next to the library.

# Adds the system libraries and link options every runtime needs, with PUBLIC for the runtime the engine builds and INTERFACE for the one an installed SDK imports.
function(haylen_link_runtime_platform target scope)
  if(HAYLEN_PLATFORM STREQUAL "macos")
    target_link_libraries(${target} ${scope} "-framework Cocoa" "-framework QuartzCore" "-framework Metal" "-framework MetalKit" "-framework GameController" "-framework AudioToolbox" "-framework CoreAudio" "-framework Network" "-framework UserNotifications")
  elseif(HAYLEN_PLATFORM MATCHES "^(ios|tvos)$")
    target_link_libraries(${target} ${scope} "-framework Foundation" "-framework UIKit" "-framework CoreGraphics" "-framework QuartzCore" "-framework Metal" "-framework MetalKit" "-framework GameController" "-framework AVFoundation" "-framework AudioToolbox" "-framework Network" "-framework UserNotifications")
  elseif(HAYLEN_PLATFORM STREQUAL "windows")
    target_link_libraries(${target} ${scope} xinput shell32 shcore ole32 imm32)
    if(HAYLEN_BACKEND STREQUAL "D3D11")
      target_link_libraries(${target} ${scope} d3d11 dxgi)
    else()
      target_link_libraries(${target} ${scope} opengl32)
    endif()
  elseif(HAYLEN_PLATFORM STREQUAL "android")
    target_link_libraries(${target} ${scope} android log EGL GLESv3)
  elseif(HAYLEN_PLATFORM STREQUAL "web")
    target_link_options(${target} INTERFACE
      -sALLOW_MEMORY_GROWTH=1
      -sSTACK_SIZE=1048576
      -lidbfs.js
      "-sEXPORTED_RUNTIME_METHODS=[ccall,UTF8ToString,stringToNewUTF8,HEAPU8,HEAPF32,FS,IDBFS,addRunDependency,removeRunDependency]"
      "-sEXPORTED_FUNCTIONS=[_main,_malloc,_free,_haylen_web_resolve,_haylen_web_emit,_haylen_web_load_zip,_haylen_web_clear_files,_haylen_web_set_file,_haylen_web_remove_file,_haylen_web_run_files,_haylen_web_restart,_haylen_web_stop,_haylen_web_set_paused,_haylen_web_paused,_haylen_web_reload_asset,_haylen_web_last_error,_haylen_web_visibility,_haylen_web_page_hidden,_haylen_web_network,_haylen_web_text_edited,_haylen_web_text_action,_haylen_web_keyboard,_haylen_web_socket_opened,_haylen_web_socket_received,_haylen_web_socket_closed,_haylen_web_socket_failed,_haylen_web_audio_render]"
      "--pre-js=${HAYLEN_ENGINE_DIR}/platform/web/haylen-runtime.js"
    )
    if(HAYLEN_BACKEND STREQUAL "WGPU")
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

function(haylen_link_package target folder)
  set(sync_target "SYNC_PACKAGE-${target}")
  add_custom_target(${sync_target} ALL
    COMMAND "${CMAKE_COMMAND}" -D "SOURCE=${folder}" -D "DESTINATION=$<TARGET_FILE_DIR:${target}>/app"
            -P "${HAYLEN_CMAKE_DIR}/haylen-link-content.cmake"
    COMMENT "Linking the app package of ${target}"
    VERBATIM
  )
  add_dependencies(${sync_target} ${target})
  set_target_properties(${sync_target} PROPERTIES FOLDER Utils)
endfunction()

function(haylen_setup_web_page target shell)
  if(NOT shell)
    set(shell "${HAYLEN_ENGINE_DIR}/platform/web/shell.html")
  endif()
  set_target_properties(${target} PROPERTIES SUFFIX ".html")
  target_link_options(${target} PRIVATE "--shell-file=${shell}")
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${shell}" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-runtime.js" "${HAYLEN_ENGINE_DIR}/platform/web/haylen-audio-worklet.js")

  # The WebGPU and WebGL2 bundle of make.py run-cpp builds its page from the same shell, so the shell travels with the build output, next to the engine logo that the default shell shows as its icon.
  # The runtime loads the processor of its audio output from next to its script, so the processor travels with the build output too.
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${shell}" "$<TARGET_FILE_DIR:${target}>/${target}.shell.html"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-logo.svg" "$<TARGET_FILE_DIR:${target}>/haylen-logo.svg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${HAYLEN_ENGINE_DIR}/platform/web/haylen-audio-worklet.js" "$<TARGET_FILE_DIR:${target}>/haylen-audio-worklet.js"
    VERBATIM
  )
endfunction()

function(haylen_setup_apple_bundle target project_dir display_name identifier version orientation show_in_taskbar)
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
    set(plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist.in")
    configure_file("${project_dir}/ios/Info.plist.in" "${plist}" @ONLY)
    set(launch_screen "${project_dir}/ios/LaunchScreen.storyboard")
  else()
    set(plist "${project_dir}/tvos/Info.plist.in")
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

  if(DEFINED launch_screen)
    target_sources(${target} PRIVATE "${launch_screen}")
    set_source_files_properties("${launch_screen}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
  endif()

  # Xcode signs the bundles it builds, while other generators leave only the signature the linker gives the executable, which binds neither the bundle identifier nor the Info.plist. macOS places the window of an unsigned Mac Catalyst app at the top left, whatever frame the app asks for, so the bundle is signed ad hoc.
  if(CMAKE_CXX_COMPILER_TARGET MATCHES "-macabi$" AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND codesign --force --sign - --timestamp=none "$<TARGET_BUNDLE_DIR:${target}>"
      VERBATIM
    )
  endif()
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
    list(APPEND sources "${HAYLEN_ENGINE_DIR}/src/platform/sokol/LuaPlayer.cpp")
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

  if(APPLE)
    set(project_dir "${arg_APPLE_PROJECT}")
    if(NOT project_dir)
      set(project_dir "${HAYLEN_ENGINE_DIR}/platform/apple")
    endif()
    file(GLOB_RECURSE package_files CONFIGURE_DEPENDS "${arg_PACKAGE}/source/*" "${arg_PACKAGE}/content/*")
    list(FILTER package_files EXCLUDE REGEX "/\\.DS_Store$")
    list(APPEND package_files "${arg_PACKAGE}/app.json")
    haylen_mark_resources(FILES ${package_files} BASEDIR "${arg_PACKAGE}" RESOURCEBASE "Resources/app")
    target_sources(${target} PRIVATE ${package_files})
    haylen_setup_apple_bundle(${target} "${project_dir}" "${app_name}" "${app_identifier}" "${app_version}" "${app_orientation}" "${app_show_in_taskbar}")
  elseif(EMSCRIPTEN)
    haylen_setup_web_page(${target} "${arg_WEB_SHELL}")
    target_link_options(${target} PRIVATE "--preload-file=${arg_PACKAGE}/app.json@/app/app.json")
    foreach(folder source content)
      if(IS_DIRECTORY "${arg_PACKAGE}/${folder}")
        target_link_options(${target} PRIVATE "--preload-file=${arg_PACKAGE}/${folder}@/app/${folder}")
      endif()
    endforeach()
  elseif(ANDROID)
    get_filename_component(package "${arg_PACKAGE}" ABSOLUTE)
    file(WRITE "${CMAKE_BINARY_DIR}/bin/${target}/package.txt" "${package}")
  else()
    haylen_link_package(${target} "${arg_PACKAGE}")
    if(WIN32)
      set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>")
    endif()
  endif()
endfunction()
