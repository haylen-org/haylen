cmake_minimum_required(VERSION 3.28...4.4)

project(cpp_embedding LANGUAGES C CXX)

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

haylen_add_app(embedding CPP
  SOURCES src/EmbeddingApp.cpp
  PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}"
)
