set(HAYLEN_SHADER_LIBRARY_DIR "${CMAKE_CURRENT_LIST_DIR}/../shaders/include" CACHE INTERNAL "Folder of the Haylen shader library that shaders include as \"haylen/<file>\".")

# Compiles a source of the Haylen shader language into a shader package with the code of the shader languages that the build draws with, and embeds the package in the target as the static method `SYMBOL` of `haylen::core::EmbeddedFiles`. Each variant names a program and its defines, as `name=DEFINE,DEFINE`.
function(haylen_compile_shader target)
  cmake_parse_arguments(PARSE_ARGV 1 SHADER "" "SOURCE;SYMBOL" "VARIANTS")
  if(NOT EXISTS "${HAYLEN_SHADERC}")
    message(FATAL_ERROR "The Haylen shader compiler was not found at \"${HAYLEN_SHADERC}\". Run \"haylen tools\" or pass \"-DHAYLEN_SHADERC\".")
  endif()

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

  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${HAYLEN_SHADERC}" "${SHADER_SOURCE}" --output "${output}" --include "${HAYLEN_SHADER_LIBRARY_DIR}" ${arguments}
    DEPENDS "${SHADER_SOURCE}" "${HAYLEN_SHADERC}" ${library}
    COMMENT "Compiling the shader \"${name}\""
    VERBATIM
  )
  haylen_embed_file(${target} "${output}" ${SHADER_SYMBOL})
endfunction()
