# Links the `DESTINATION` folder to the staged package of an app in the `SOURCE` folder, so the files that the tooling stages again are visible without a build of the app.

if(IS_SYMLINK "${DESTINATION}")
  file(READ_SYMLINK "${DESTINATION}" current)
  if(current STREQUAL SOURCE)
    return()
  endif()
endif()
if(EXISTS "${DESTINATION}" OR IS_SYMLINK "${DESTINATION}")
  file(REMOVE_RECURSE "${DESTINATION}")
endif()

# Windows creates symbolic links only with extra privileges, so the folder becomes a junction, which reaches every local volume.
if(CMAKE_HOST_WIN32)
  file(TO_NATIVE_PATH "${SOURCE}" native_source)
  file(TO_NATIVE_PATH "${DESTINATION}" native_destination)
  execute_process(COMMAND cmd.exe /c mklink /J "${native_destination}" "${native_source}" COMMAND_ERROR_IS_FATAL ANY OUTPUT_QUIET)
else()
  file(CREATE_LINK "${SOURCE}" "${DESTINATION}" SYMBOLIC)
endif()
