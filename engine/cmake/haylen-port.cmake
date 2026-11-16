# A port of the engine to a platform that the repository does not hold, such as a game console whose SDK and graphics API come under a license that keeps their code private. The cache path `HAYLEN_PORT_DIR` names the folder of the port, outside the repository, whose `port.cmake` describes the port with `haylen_port`, and the engine builds for the platform of the port instead of the platform of the toolchain.
set(HAYLEN_PORT_DIR "" CACHE PATH "Folder of a port of the engine to another platform, whose port.cmake describes it with haylen_port, or empty to build for the platform of the toolchain.")

# The version of the contract between the engine and its ports, which grows whenever `haylen_port`, `platform::NativeHost`, `platform::Services` or `gpu::PortDevice` change in a way that a port must follow.
set(HAYLEN_PORT_CONTRACT_VERSION 1)

# What the platform of a port allows apps to do, which the SDK of the port records and plugins and the tooling read: loading libraries at run time, writing code into memory and starting other processes.
set(HAYLEN_PORT_CAPABILITY_NAMES dynamicLibraries executableMemory processes)

# Describes the port:
# - `CONTRACT`, the version of the contract of ports that the port follows, which must be `HAYLEN_PORT_CONTRACT_VERSION`.
# - `PLATFORM`, the name of the platform, which `HAYLEN_PLATFORM` takes and the services of the port report.
# - `CAPABILITIES`, what the platform allows apps to do, any of `dynamicLibraries`, `executableMemory` and `processes`.
# - `RUNTIME_SOURCES` and `RUNTIME_LIBRARIES`, the platform layer and the services of the port, which `haylen-runtime` compiles and links instead of those of the engine.
# - `GRAPHICS_APIS`, the graphics APIs of the engine whose devices the port compiles, such as `vulkan`.
# - `DEVICE_SOURCES` and `DEVICE_LIBRARIES`, the device of the API `port` that `haylen-gpu` compiles and links, which defines `gpu::PortDevice` for a graphics API the engine has no device of.
# - `SHADER_LANGUAGES`, the shader languages of `haylen-shaderc` that the shader tool of the port reads, and `SHADER_COMMAND`, the command that runs on each shader package with the path of the package after its last argument and writes the code of the device into its `port` target.
# - `HOST_TOOLS`, the programs of the port that run on the build machine, such as its shader tool, which the SDK of the port installs.
# - `APP_FUNCTION`, the CMake function that `haylen_add_app` calls to build the app of the port, which `APP_MODULE` defines, a CMake file that the engine includes and the SDK of the port installs.
# - `RELEASE_PROFILE`, the content profile of the protected releases of the platform.
# - `PLUGIN_PART`, the name of the part of plugins that the backend of the port reads.
# - `SDK`, which is `ON` when the port builds and installs the SDK that `HAYLEN_BUILD_SDK` makes.
function(haylen_port)
  cmake_parse_arguments(PARSE_ARGV 0 port "" "CONTRACT;PLATFORM;APP_FUNCTION;APP_MODULE;RELEASE_PROFILE;PLUGIN_PART;SDK" "CAPABILITIES;RUNTIME_SOURCES;RUNTIME_LIBRARIES;GRAPHICS_APIS;DEVICE_SOURCES;DEVICE_LIBRARIES;SHADER_LANGUAGES;SHADER_COMMAND;HOST_TOOLS")
  if(port_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" passes \"${port_UNPARSED_ARGUMENTS}\" to \"haylen_port\", which takes \"CONTRACT\", \"PLATFORM\", \"CAPABILITIES\", \"RUNTIME_SOURCES\", \"RUNTIME_LIBRARIES\", \"GRAPHICS_APIS\", \"DEVICE_SOURCES\", \"DEVICE_LIBRARIES\", \"SHADER_LANGUAGES\", \"SHADER_COMMAND\", \"HOST_TOOLS\", \"APP_FUNCTION\", \"APP_MODULE\", \"RELEASE_PROFILE\", \"PLUGIN_PART\" and \"SDK\".")
  endif()
  if(NOT port_CONTRACT STREQUAL HAYLEN_PORT_CONTRACT_VERSION)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" follows the contract \"${port_CONTRACT}\" of ports, while this engine has the contract ${HAYLEN_PORT_CONTRACT_VERSION}. Update the port to the contract of the engine, as the guide of ports describes, and pass \"CONTRACT ${HAYLEN_PORT_CONTRACT_VERSION}\".")
  endif()
  if(NOT port_PLATFORM MATCHES "^[a-z][a-z0-9-]*$" OR port_PLATFORM MATCHES "^(macos|windows|linux|ios|tvos|android|web)$")
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" names its platform \"${port_PLATFORM}\". Name it in lowercase dash-case with a name that no platform of the engine has, such as \"console\".")
  endif()
  foreach(capability IN LISTS port_CAPABILITIES)
    if(NOT capability IN_LIST HAYLEN_PORT_CAPABILITY_NAMES)
      message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" names the capability \"${capability}\", which is \"dynamicLibraries\", \"executableMemory\" or \"processes\".")
    endif()
  endforeach()
  if(NOT port_RUNTIME_SOURCES)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" has no \"RUNTIME_SOURCES\". List the host, the window, the runtime and the services of its platform.")
  endif()
  if(NOT port_GRAPHICS_APIS AND NOT port_DEVICE_SOURCES)
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" draws through no graphics API. Name the APIs of the engine it compiles in \"GRAPHICS_APIS\", or list its own device in \"DEVICE_SOURCES\".")
  endif()
  if(port_DEVICE_SOURCES AND (NOT port_SHADER_LANGUAGES OR NOT port_SHADER_COMMAND))
    message(FATAL_ERROR "The device of the port of \"${HAYLEN_PORT_DIR}\" needs \"SHADER_LANGUAGES\" and \"SHADER_COMMAND\", which write the code of its shaders into the shader packages.")
  endif()
  foreach(tool IN LISTS port_HOST_TOOLS)
    if(NOT EXISTS "${tool}" OR IS_DIRECTORY "${tool}")
      message(FATAL_ERROR "The host tool \"${tool}\" of the port of \"${HAYLEN_PORT_DIR}\" is no file. Name the programs of the port that run on the build machine by their paths.")
    endif()
  endforeach()
  if(NOT port_APP_FUNCTION STREQUAL "" AND NOT EXISTS "${port_APP_MODULE}")
    message(FATAL_ERROR "The \"APP_FUNCTION\" \"${port_APP_FUNCTION}\" of the port of \"${HAYLEN_PORT_DIR}\" needs \"APP_MODULE\", the CMake file that defines it, which the SDK of the port installs.")
  endif()
  if(port_APP_MODULE AND port_APP_FUNCTION STREQUAL "")
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" names the \"APP_MODULE\" \"${port_APP_MODULE}\" without the \"APP_FUNCTION\" that it defines.")
  endif()
  if(NOT port_RELEASE_PROFILE STREQUAL "" AND NOT port_RELEASE_PROFILE MATCHES "^[a-z][a-z0-9-]*$")
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" names the release profile \"${port_RELEASE_PROFILE}\". Name it in lowercase dash-case, such as \"console\".")
  endif()
  if(NOT port_PLUGIN_PART STREQUAL "" AND NOT port_PLUGIN_PART MATCHES "^[a-z][a-z0-9]*(-[a-z0-9]+)*$")
    message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" names the part of plugins \"${port_PLUGIN_PART}\". Name it in lowercase dash-case, such as \"console\".")
  endif()
  if(NOT port_SDK STREQUAL "" AND NOT port_SDK MATCHES "^(ON|OFF)$")
    message(FATAL_ERROR "The \"SDK\" of the port of \"${HAYLEN_PORT_DIR}\" is \"ON\" or \"OFF\", not \"${port_SDK}\".")
  endif()

  if(port_APP_MODULE)
    include("${port_APP_MODULE}")
    if(NOT COMMAND "${port_APP_FUNCTION}")
      message(FATAL_ERROR "The \"APP_MODULE\" \"${port_APP_MODULE}\" of the port of \"${HAYLEN_PORT_DIR}\" does not define the \"APP_FUNCTION\" \"${port_APP_FUNCTION}\".")
    endif()
  endif()

  set(HAYLEN_PORT_PLATFORM "${port_PLATFORM}" PARENT_SCOPE)
  set(HAYLEN_PORT_CAPABILITIES "${port_CAPABILITIES}" PARENT_SCOPE)
  set(HAYLEN_PORT_RUNTIME_SOURCES "${port_RUNTIME_SOURCES}" PARENT_SCOPE)
  set(HAYLEN_PORT_RUNTIME_LIBRARIES "${port_RUNTIME_LIBRARIES}" PARENT_SCOPE)
  set(HAYLEN_PORT_GRAPHICS_APIS "${port_GRAPHICS_APIS}" PARENT_SCOPE)
  set(HAYLEN_PORT_DEVICE_SOURCES "${port_DEVICE_SOURCES}" PARENT_SCOPE)
  set(HAYLEN_PORT_DEVICE_LIBRARIES "${port_DEVICE_LIBRARIES}" PARENT_SCOPE)
  set(HAYLEN_PORT_SHADER_LANGUAGES "${port_SHADER_LANGUAGES}" PARENT_SCOPE)
  set(HAYLEN_PORT_SHADER_COMMAND "${port_SHADER_COMMAND}" PARENT_SCOPE)
  set(HAYLEN_PORT_HOST_TOOLS "${port_HOST_TOOLS}" PARENT_SCOPE)
  set(HAYLEN_PORT_APP_FUNCTION "${port_APP_FUNCTION}" PARENT_SCOPE)
  set(HAYLEN_PORT_APP_MODULE "${port_APP_MODULE}" PARENT_SCOPE)
  set(HAYLEN_PORT_RELEASE_PROFILE "${port_RELEASE_PROFILE}" PARENT_SCOPE)
  set(HAYLEN_PORT_PLUGIN_PART "${port_PLUGIN_PART}" PARENT_SCOPE)
  set(HAYLEN_PORT_SDK "${port_SDK}" PARENT_SCOPE)
endfunction()

set(HAYLEN_PORT_PLATFORM "")
set(HAYLEN_PORT_APP_FUNCTION "")
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

# The function `haylen_add_app` runs in the folders of the apps, which do not see the variables of the engine folder.
set(HAYLEN_PORT_PLATFORM "${HAYLEN_PORT_PLATFORM}" CACHE INTERNAL "Platform of the port the engine is built for.")
set(HAYLEN_PORT_APP_FUNCTION "${HAYLEN_PORT_APP_FUNCTION}" CACHE INTERNAL "Function that builds the apps of the port.")
