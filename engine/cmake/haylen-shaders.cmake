set(HAYLEN_SHADER_LIBRARY_DIR "${CMAKE_CURRENT_LIST_DIR}/../shaders/include" CACHE INTERNAL "Folder of the Haylen shader library that shaders include as \"haylen/<file>\".")

include(haylen-shader-compiler)

# The build provides the compiler that `haylen-shader-compiler.json` pins when `HAYLEN_SHADERC` names none, and configures again when the pins change.
if(CPM_SOURCE_CACHE)
  set(HAYLEN_SHADERC_CACHE "${CPM_SOURCE_CACHE}" CACHE PATH "Folder that keeps the shader compilers that the build provides.")
else()
  set(HAYLEN_SHADERC_CACHE "${CMAKE_BINARY_DIR}/tools" CACHE PATH "Folder that keeps the shader compilers that the build provides.")
endif()
if(HAYLEN_SHADERC)
  if(NOT EXISTS "${HAYLEN_SHADERC}")
    message(FATAL_ERROR "The Haylen shader compiler was not found at \"${HAYLEN_SHADERC}\". Name a build of \"haylen-shaderc\" with \"HAYLEN_SHADERC\", or leave it empty so the build provides the pinned compiler.")
  endif()
else()
  haylen_shader_compiler(HAYLEN_SHADERC "${CMAKE_CURRENT_LIST_DIR}/haylen-shader-compiler.json" "${HAYLEN_SHADERC_CACHE}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/haylen-shader-compiler.json")
endif()

# Compiles a source of the Haylen shader language into a shader package with the code of the shader languages that the build draws with, and embeds the package in the target as the static method `SYMBOL` of `haylen::core::EmbeddedFiles`. Each variant names a program and its defines, as `name=DEFINE,DEFINE`. The shader command of a port with a device of its own then writes the code of that device into the package.
function(haylen_compile_shader target)
  cmake_parse_arguments(PARSE_ARGV 1 SHADER "" "SOURCE;SYMBOL" "VARIANTS")
  get_filename_component(name "${SHADER_SOURCE}" NAME_WE)
  set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders/${name}.shader")
  set(arguments "")
  foreach(variant IN LISTS SHADER_VARIANTS)
    list(APPEND arguments --variant "${variant}")
  endforeach()
  foreach(language IN LISTS HAYLEN_SHADER_LANGUAGES)
    list(APPEND arguments --target "${language}")
  endforeach()
  file(GLOB library "${HAYLEN_SHADER_LIBRARY_DIR}/haylen/*.glsl")
  set(port_command "")
  if("port" IN_LIST HAYLEN_GRAPHICS_APIS)
    set(port_command COMMAND ${HAYLEN_PORT_SHADER_COMMAND} "${output}")
  endif()

  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${HAYLEN_SHADERC}" "${SHADER_SOURCE}" --output "${output}" --include "${HAYLEN_SHADER_LIBRARY_DIR}" ${arguments}
    ${port_command}
    DEPENDS "${SHADER_SOURCE}" "${HAYLEN_SHADERC}" ${library}
    COMMENT "Compiling the shader \"${name}\""
    VERBATIM
  )
  haylen_embed_file(${target} "${output}" ${SHADER_SYMBOL})
endfunction()
