# A port of the engine to a platform that the repository does not hold, such as a game console whose SDK and graphics API come under a license that keeps their code private. The cache path `HAYLEN_PORT_DIR` names the folder of the port, outside the repository, whose `port.cmake` describes the port with `haylen_port`, and the engine builds for the platform of the port instead of the platform of the toolchain.
set(HAYLEN_PORT_DIR "" CACHE PATH "Folder of a port of the engine to another platform, whose port.cmake describes it with haylen_port, or empty to build for the platform of the toolchain.")

# Describes the port:
# - `PLATFORM`, the name of the platform, which `HAYLEN_PLATFORM` takes and the services of the port report.
# - `RUNTIME_SOURCES` and `RUNTIME_LIBRARIES`, the platform layer and the services of the port, which `haylen-runtime` compiles and links instead of those of the engine.
# - `GRAPHICS_APIS`, the graphics APIs of the engine whose devices the port compiles, such as `vulkan`.
# - `DEVICE_SOURCES` and `DEVICE_LIBRARIES`, the device of the API `port` that `haylen-gpu` compiles and links, which defines `gpu::PortDevice` for a graphics API the engine has no device of.
# - `SHADER_LANGUAGES`, the shader languages of `haylen-shaderc` that the shader tool of the port reads, and `SHADER_COMMAND`, the command that runs on each shader package with the path of the package after its last argument and writes the code of the device into its `port` target.
function(haylen_port)
  cmake_parse_arguments(PARSE_ARGV 0 port "" "PLATFORM" "RUNTIME_SOURCES;RUNTIME_LIBRARIES;GRAPHICS_APIS;DEVICE_SOURCES;DEVICE_LIBRARIES;SHADER_LANGUAGES;SHADER_COMMAND")
  if(port_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" passes \"${port_UNPARSED_ARGUMENTS}\" to \"haylen_port\", which takes \"PLATFORM\", \"RUNTIME_SOURCES\", \"RUNTIME_LIBRARIES\", \"GRAPHICS_APIS\", \"DEVICE_SOURCES\", \"DEVICE_LIBRARIES\", \"SHADER_LANGUAGES\" and \"SHADER_COMMAND\".")
  endif()
  if(NOT port_PLATFORM MATCHES "^[a-z][a-z0-9-]*$" OR port_PLATFORM MATCHES "^(macos|windows|linux|ios|tvos|android|web)$")
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" names its platform \"${port_PLATFORM}\". Name it in lowercase dash-case with a name that no platform of the engine has, such as \"console\".")
  endif()
  if(NOT port_RUNTIME_SOURCES)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" has no \"RUNTIME_SOURCES\". List the host, the window, the runtime and the services of its platform.")
  endif()
  if(NOT port_GRAPHICS_APIS AND NOT port_DEVICE_SOURCES)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" draws through no graphics API. Name the APIs of the engine it compiles in \"GRAPHICS_APIS\", or list its own device in \"DEVICE_SOURCES\".")
  endif()
  if(port_DEVICE_SOURCES AND (NOT port_SHADER_LANGUAGES OR NOT port_SHADER_COMMAND))
    message(FATAL_ERROR "The device of the port of \"${HAYLEN_PORT_DIR}\" needs \"SHADER_LANGUAGES\" and \"SHADER_COMMAND\", which write the code of its shaders into the shader packages.")
  endif()

  set(HAYLEN_PORT_PLATFORM "${port_PLATFORM}" PARENT_SCOPE)
  set(HAYLEN_PORT_RUNTIME_SOURCES "${port_RUNTIME_SOURCES}" PARENT_SCOPE)
  set(HAYLEN_PORT_RUNTIME_LIBRARIES "${port_RUNTIME_LIBRARIES}" PARENT_SCOPE)
  set(HAYLEN_PORT_GRAPHICS_APIS "${port_GRAPHICS_APIS}" PARENT_SCOPE)
  set(HAYLEN_PORT_DEVICE_SOURCES "${port_DEVICE_SOURCES}" PARENT_SCOPE)
  set(HAYLEN_PORT_DEVICE_LIBRARIES "${port_DEVICE_LIBRARIES}" PARENT_SCOPE)
  set(HAYLEN_PORT_SHADER_LANGUAGES "${port_SHADER_LANGUAGES}" PARENT_SCOPE)
  set(HAYLEN_PORT_SHADER_COMMAND "${port_SHADER_COMMAND}" PARENT_SCOPE)
endfunction()

set(HAYLEN_PORT_PLATFORM "")
if(HAYLEN_PORT_DIR)
  if(NOT EXISTS "${HAYLEN_PORT_DIR}/port.cmake")
    message(FATAL_ERROR "The folder \"${HAYLEN_PORT_DIR}\" of \"HAYLEN_PORT_DIR\" holds no \"port.cmake\". Name the folder of a port, whose \"port.cmake\" describes it with \"haylen_port\".")
  endif()
  include("${HAYLEN_PORT_DIR}/port.cmake")
  if(NOT HAYLEN_PORT_PLATFORM)
    message(FATAL_ERROR "The file \"${HAYLEN_PORT_DIR}/port.cmake\" does not describe its port. Call \"haylen_port\" in it.")
  endif()
  message(STATUS "Haylen port: \"${HAYLEN_PORT_DIR}\"")
endif()
