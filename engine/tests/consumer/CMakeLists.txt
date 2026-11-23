cmake_minimum_required(VERSION 3.28...4.4)

project(haylen-consumer LANGUAGES C CXX)

# A project of its own that adds the engine the way another repository does, through `add_subdirectory`, CPM or the installed SDK, and builds a C++ app against its public headers. The command `haylen engine consumers` builds it in each way.
set(HAYLEN_CONSUMER_MODE "subdirectory" CACHE STRING "How the project adds the engine: \"subdirectory\", \"cpm\" or \"package\".")
set_property(CACHE HAYLEN_CONSUMER_MODE PROPERTY STRINGS subdirectory cpm package)
set(engine "${CMAKE_CURRENT_LIST_DIR}/../..")

if(HAYLEN_CONSUMER_MODE STREQUAL "subdirectory")
  add_subdirectory("${engine}" haylen)
elseif(HAYLEN_CONSUMER_MODE STREQUAL "cpm")
  include("${engine}/cmake/cpm.cmake")
  CPMAddPackage(NAME haylen SOURCE_DIR "${engine}")
elseif(HAYLEN_CONSUMER_MODE STREQUAL "package")
  find_package(haylen REQUIRED)
else()
  message(FATAL_ERROR "The variable \"HAYLEN_CONSUMER_MODE\" is \"subdirectory\", \"cpm\" or \"package\", not \"${HAYLEN_CONSUMER_MODE}\".")
endif()

# The project stages the package of the app next to its build, as the tooling stages the package of every app, so nothing else of the project folder ships.
set(package "${CMAKE_CURRENT_BINARY_DIR}/package")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/haylen.json" DESTINATION "${package}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/haylen.json")

haylen_add_app(haylen-consumer CPP
  SOURCES ConsumerApp.cpp
  CONTENT "${package}"
)

# A project that adds the engine from its sources also builds the app as a program of the headless player, which `haylen engine consumers` runs on the headless host, while the installed SDK has no headless host.
set(programs haylen-consumer)
if(TARGET haylen::headless-player)
  add_executable(haylen-consumer-headless ConsumerApp.cpp)
  target_link_libraries(haylen-consumer-headless PRIVATE haylen::headless-player)
  set_target_properties(haylen-consumer-headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  list(APPEND programs haylen-consumer-headless)
endif()

# A C++ app without Lua links nothing of the Lua layer, so the build fails when a program holds a symbol of Lua. The linker of MSVC keeps no symbols in the program and lists them in its map instead.
foreach(program IN LISTS programs)
  if(MSVC)
    set(symbols "${CMAKE_CURRENT_BINARY_DIR}/${program}.map")
    target_link_options(${program} PRIVATE "/MAP:${symbols}")
    set(lister "")
  else()
    set(symbols "$<TARGET_FILE:${program}>")
    set(lister "${CMAKE_NM}")
  endif()
  add_custom_command(TARGET ${program} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=${program}" "-DSYMBOLS=${symbols}" "-DLISTER=${lister}" -P "${CMAKE_CURRENT_SOURCE_DIR}/check-lua-symbols.cmake"
    VERBATIM
  )
endforeach()
