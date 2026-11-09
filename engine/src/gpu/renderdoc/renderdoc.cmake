# The capture of frames through the in-application API of RenderDoc, which the devices of Vulkan, Direct3D and OpenGL add once to "haylen-gpu" on Windows, Linux and Android, where RenderDoc runs.
include_guard(GLOBAL)

target_sources(haylen-gpu PRIVATE "${CMAKE_CURRENT_LIST_DIR}/Capturer.cpp")
target_link_libraries(haylen-gpu PRIVATE ${CMAKE_DL_LIBS})
