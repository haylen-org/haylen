# The content tool `haylen-content` builds, verifies, inspects, compares and publishes the protected releases of apps on the machine that builds them, with the format library of the engine, and the tests link its library to check it.
add_library(haylen_content_tool STATIC
  ChannelPublisher.cpp
  ContentTool.cpp
  KeyStore.cpp
  ReleaseInspector.cpp
  SecureRandom.cpp
)
target_include_directories(haylen_content_tool PUBLIC "${HAYLEN_ENGINE_DIR}/tools" "${HAYLEN_ENGINE_DIR}/src")
# The tool reads the configuration of apps with the engine, whose graphics code the headless host completes without a device.
target_link_libraries(haylen_content_tool PUBLIC haylen_engine haylen_headless haylen_monocypher)
if(WIN32)
  target_link_libraries(haylen_content_tool PRIVATE bcrypt)
endif()
haylen_enable_warnings(haylen_content_tool)
haylen_enable_coverage(haylen_content_tool)

add_executable(haylen-content ContentToolMain.cpp)
target_link_libraries(haylen-content PRIVATE haylen_content_tool)
set_target_properties(haylen-content PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
haylen_enable_warnings(haylen-content)
