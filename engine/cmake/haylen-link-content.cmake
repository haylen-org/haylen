# Links `app.json`, `source` and `content` of the `SOURCE` package folder into the `DESTINATION` folder, so edited files are visible without rebuilding and nothing else in the package folder is deployed.

file(MAKE_DIRECTORY "${DESTINATION}")

foreach(entry app.json source content)
  set(original "${SOURCE}/${entry}")
  set(link "${DESTINATION}/${entry}")

  if(IS_SYMLINK "${link}")
    file(READ_SYMLINK "${link}" current)
    if(current STREQUAL original)
      continue()
    endif()
  endif()

  if(EXISTS "${link}" OR IS_SYMLINK "${link}")
    file(REMOVE_RECURSE "${link}")
  endif()
  if(NOT EXISTS "${original}")
    continue()
  endif()

  # Windows creates symbolic links only with extra privileges, so folders become junctions and `app.json` a hard link.
  if(CMAKE_HOST_WIN32 AND IS_DIRECTORY "${original}")
    file(TO_NATIVE_PATH "${original}" native_original)
    file(TO_NATIVE_PATH "${link}" native_link)
    execute_process(COMMAND cmd.exe /c mklink /J "${native_link}" "${native_original}" COMMAND_ERROR_IS_FATAL ANY OUTPUT_QUIET)
  elseif(CMAKE_HOST_WIN32)
    file(CREATE_LINK "${original}" "${link}")
  else()
    file(CREATE_LINK "${original}" "${link}" SYMBOLIC)
  endif()
endforeach()
