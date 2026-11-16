# Builds an app of the test port for "haylen_add_app": an executable of its sources and libraries with its content in the folder "app" next to it, where the runtime of the port opens it. The content is copied again on every build, so a change of the package alone reaches the app without linking it again.
function(haylen_test_port_add_app target)
  cmake_parse_arguments(PARSE_ARGV 1 app "RELEASE" "CONTENT" "SOURCES;LIBRARIES")
  add_executable(${target} ${app_SOURCES})
  target_link_libraries(${target} PRIVATE ${app_LIBRARIES})
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${target}/$<0:>")
  add_custom_target(${target}-content ALL
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "$<TARGET_FILE_DIR:${target}>/app"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${app_CONTENT}" "$<TARGET_FILE_DIR:${target}>/app"
    VERBATIM
  )
  add_dependencies(${target}-content ${target})
endfunction()
