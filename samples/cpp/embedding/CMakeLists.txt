cmake_minimum_required(VERSION 3.28...4.4)

project(cpp-embedding LANGUAGES C CXX)

# An app project of its own that reaches the engine the way another repository would, through `add_subdirectory` by default, through CPM or through an installed SDK.
set(HAYLEN_ENGINE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../../engine" CACHE PATH "The engine folder of a Haylen checkout.")
set(CPP_EMBEDDING_MODE "subdirectory" CACHE STRING "How the engine is added: \"subdirectory\", \"cpm\" or \"package\".")
set_property(CACHE CPP_EMBEDDING_MODE PROPERTY STRINGS subdirectory cpm package)

if(CPP_EMBEDDING_MODE STREQUAL "cpm")
  include("${HAYLEN_ENGINE_SOURCE}/cmake/cpm.cmake")
  CPMAddPackage(NAME haylen SOURCE_DIR "${HAYLEN_ENGINE_SOURCE}")
elseif(CPP_EMBEDDING_MODE STREQUAL "subdirectory")
  add_subdirectory("${HAYLEN_ENGINE_SOURCE}" haylen)
elseif(CPP_EMBEDDING_MODE STREQUAL "package")
  find_package(haylen REQUIRED)
else()
  message(FATAL_ERROR "The variable \"CPP_EMBEDDING_MODE\" is \"subdirectory\", \"cpm\" or \"package\", not \"${CPP_EMBEDDING_MODE}\".")
endif()

# The project stages the package of the app next to its build, as the tooling stages the package of every app, so its sources and its CMake files never ship.
set(package "${CMAKE_CURRENT_BINARY_DIR}/package")
file(REMOVE_RECURSE "${package}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/haylen.json" "${CMAKE_CURRENT_SOURCE_DIR}/content" DESTINATION "${package}")
file(GLOB_RECURSE package_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/content/*")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/haylen.json" ${package_files})

haylen_add_app(main CPP
  SOURCES src/EmbeddingApp.cpp
  CONTENT "${package}"
)
