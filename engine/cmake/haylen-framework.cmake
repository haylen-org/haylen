# Merges the SDK libraries and the Lua player into the static library of one `Haylen.xcframework` slice, which apps made from the Apple template link instead of building the engine.
# The command `haylen.py engine --platform apple` builds it for every slice, joins the architectures of each platform with `lipo` and creates the framework with `xcodebuild -create-xcframework`.
# It runs after `haylen-install.cmake`, whose merged engine and runtime libraries it takes as inputs.

if(NOT APPLE OR NOT HAYLEN_BUILD_SDK)
  message(FATAL_ERROR "The option \"HAYLEN_BUILD_FRAMEWORK\" needs an Apple platform and \"HAYLEN_BUILD_SDK\".")
endif()

add_library(haylen_framework_player OBJECT "${HAYLEN_ENGINE_DIR}/src/platform/sokol/LuaPlayer.cpp")
target_link_libraries(haylen_framework_player PRIVATE haylen_engine)
haylen_enable_warnings(haylen_framework_player)

set(framework_libraries "${CMAKE_BINARY_DIR}/haylen-framework/$<CONFIG>")
set(framework_library "${framework_libraries}/libhaylen.a")
file(GENERATE OUTPUT "${framework_libraries}/inputs.txt" CONTENT "${engine_library}\n${runtime_library}\n$<JOIN:$<TARGET_OBJECTS:haylen_framework_player>,\n>\n")

add_custom_command(OUTPUT "${framework_library}"
  COMMAND "${CMAKE_COMMAND}" -DKIND=libtool "-DTOOL=${HAYLEN_LIBTOOL}" "-DOUTPUT=${framework_library}" "-DINPUTS=${framework_libraries}/inputs.txt" -P "${HAYLEN_CMAKE_DIR}/haylen-merge-archives.cmake"
  DEPENDS "${framework_libraries}/inputs.txt" "${engine_library}" "${runtime_library}" "$<TARGET_OBJECTS:haylen_framework_player>"
  COMMENT "Merging the Haylen framework library"
  VERBATIM
)
add_custom_target(haylen_framework ALL DEPENDS "${framework_library}")

# The system frameworks of every Apple platform, by the names of the SDK, which haylen.py links into the projects of Lua apps.
set(frameworks_json "{}")
foreach(platform IN ITEMS iOS macCatalyst tvOS macOS)
  string(TOUPPER "${platform}" key)
  set(names ${HAYLEN_APPLE_FRAMEWORKS_${key}})
  list(TRANSFORM names REPLACE "^(.+)$" "\"\\1.framework\"")
  list(JOIN names ", " names)
  string(JSON frameworks_json SET "${frameworks_json}" "${platform}" "[${names}]")
endforeach()
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/haylen-framework/haylen-frameworks.json" CONTENT "${frameworks_json}\n" @ONLY)

install(FILES "${framework_library}" DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT haylen_framework)
install(FILES "${CMAKE_BINARY_DIR}/haylen-framework/haylen-frameworks.json" DESTINATION "${CMAKE_INSTALL_DATADIR}/haylen" COMPONENT haylen_framework)
