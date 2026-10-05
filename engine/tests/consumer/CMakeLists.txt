cmake_minimum_required(VERSION 3.28...4.4)

project(haylen_consumer LANGUAGES C CXX)

# A project of its own that adds the engine the way another repository does, through `add_subdirectory`, CPM or the installed SDK, and builds a C++ app against its public headers. The command `haylen.py sdk --check-consumers` builds it in each way.
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

haylen_add_app(haylen_consumer CPP
  SOURCES ConsumerApp.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
