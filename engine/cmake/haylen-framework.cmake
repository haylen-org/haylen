# Builds the libraries of one slice of the Apple SDK: the engine and the runtime merged into the `libhaylen.a` of `Haylen.xcframework`, which every app links, and the Lua layer with the Lua player in the `libhaylen-lua.a` of `HaylenLua.xcframework`, which only the apps that run Lua link. The player is a member of its own in that library, which the link of an app takes only when nothing else defines `haylen::core::Application::create`, so a C++ app that runs Lua with an application of its own holds no second one.
# The Apple engine port of the tooling builds it for every architecture of a slice, joins the architectures with `lipo` and creates the frameworks with `xcodebuild -create-xcframework`.
# It runs after `haylen-install.cmake`, whose merged engine, runtime and Lua libraries it takes as inputs.

if(NOT APPLE OR NOT HAYLEN_BUILD_SDK)
  message(FATAL_ERROR "The option \"HAYLEN_BUILD_FRAMEWORK\" needs an Apple platform and \"HAYLEN_BUILD_SDK\".")
endif()

set(framework_libraries "${CMAKE_BINARY_DIR}/haylen-framework/$<CONFIG>")
set(framework_library "${framework_libraries}/libhaylen.a")
set(lua_framework_library "${framework_libraries}/libhaylen-lua.a")
file(GENERATE OUTPUT "${framework_libraries}/inputs.txt" CONTENT "${engine_library}\n${runtime_library}\n")
file(GENERATE OUTPUT "${framework_libraries}/lua-inputs.txt" CONTENT "${lua_library}\n$<JOIN:$<TARGET_OBJECTS:haylen-player>,\n>\n")

add_custom_command(OUTPUT "${framework_library}"
  COMMAND "${CMAKE_COMMAND}" -DKIND=libtool "-DTOOL=${HAYLEN_LIBTOOL}" "-DOUTPUT=${framework_library}" "-DINPUTS=${framework_libraries}/inputs.txt" -P "${HAYLEN_CMAKE_DIR}/haylen-merge-archives.cmake"
  DEPENDS "${framework_libraries}/inputs.txt" "${engine_library}" "${runtime_library}"
  COMMENT "Merging the Haylen framework library"
  VERBATIM
)
add_custom_command(OUTPUT "${lua_framework_library}"
  COMMAND "${CMAKE_COMMAND}" -DKIND=libtool "-DTOOL=${HAYLEN_LIBTOOL}" "-DOUTPUT=${lua_framework_library}" "-DINPUTS=${framework_libraries}/lua-inputs.txt" -P "${HAYLEN_CMAKE_DIR}/haylen-merge-archives.cmake"
  DEPENDS "${framework_libraries}/lua-inputs.txt" "${lua_library}" "$<TARGET_OBJECTS:haylen-player>"
  COMMENT "Merging the Lua layer and the Lua player of the Haylen framework"
  VERBATIM
)
add_custom_target(haylen-framework ALL DEPENDS "${framework_library}" "${lua_framework_library}")

# The settings that the Apple projects of apps take from the SDK: the system frameworks of every Apple platform by the names of the SDK, and the preprocessor definitions that sources compile with against the headers of the engine, as `haylen::engine` gives them to CMake projects.
set(settings_json "{}")
set(frameworks_json "{}")
foreach(platform IN ITEMS iOS macCatalyst tvOS visionOS macOS)
  string(TOUPPER "${platform}" key)
  set(frameworks ${HAYLEN_APPLE_FRAMEWORKS_${key}})
  list(TRANSFORM frameworks APPEND ".framework")
  haylen_json_texts(names ${frameworks})
  string(JSON frameworks_json SET "${frameworks_json}" "${platform}" "${names}")
endforeach()
string(JSON settings_json SET "${settings_json}" frameworks "${frameworks_json}")
set(definitions_json "[]")
foreach(definition IN LISTS HAYLEN_SDK_DEFINITIONS ITEMS "IMGUI_USER_CONFIG=\\\"haylen/ui/ImGuiConfig.hpp\\\"")
  string(JSON length LENGTH "${definitions_json}")
  string(JSON definitions_json SET "${definitions_json}" ${length} "\"${definition}\"")
endforeach()
string(JSON settings_json SET "${settings_json}" definitions "${definitions_json}")
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/haylen-framework/haylen-apple.json" CONTENT "${settings_json}\n" @ONLY)

install(FILES "${framework_library}" "${lua_framework_library}" DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT haylen-framework)
install(FILES "${CMAKE_BINARY_DIR}/haylen-framework/haylen-apple.json" DESTINATION "${CMAKE_INSTALL_DATADIR}/haylen" COMPONENT haylen-framework)
# The CMake package of the Apple SDK gives native libraries the headers of the native API, since Xcode builds the apps themselves.
install(FILES "${HAYLEN_CMAKE_DIR}/haylen-apple-config.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/haylen" RENAME haylen-config.cmake COMPONENT haylen-framework)
install(FILES "${sdk}/haylen-config-version.cmake" DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/haylen" COMPONENT haylen-framework)
