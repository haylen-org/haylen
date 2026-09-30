set(HAYLEN_SHADER_LIBRARY_DIR "${CMAKE_CURRENT_LIST_DIR}/../shaders/include" CACHE INTERNAL "Folder of the Haylen shader library that shaders include as \"haylen/<file>\".")

# Compiles one variant of a sokol-shdc shader into `generated/shaders/<module>.glsl.h`. The module prefixes every generated name, so variants of one source live side by side, and the defines select the variant.
function(haylen_compile_shader target)
  cmake_parse_arguments(PARSE_ARGV 1 SHADER "" "SOURCE;MODULE" "DEFINES")
  if(NOT EXISTS "${HAYLEN_SOKOL_SHDC}")
    message(FATAL_ERROR "The sokol-shdc tool was not found at \"${HAYLEN_SOKOL_SHDC}\". Run \"python3 make.py tools\" or pass \"-DHAYLEN_SOKOL_SHDC\".")
  endif()

  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
  file(MAKE_DIRECTORY "${output_dir}")
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

  set(header "${output_dir}/${SHADER_MODULE}.glsl.h")
  set(defines "")
  if(SHADER_DEFINES)
    list(JOIN SHADER_DEFINES ":" joined)
    set(defines "--defines=${joined}")
  endif()
  file(GLOB library "${HAYLEN_SHADER_LIBRARY_DIR}/haylen/*.glsl")

  # The library folder is the working directory, which is where sokol-shdc resolves includes such as `haylen/material.glsl`.
  add_custom_command(
    OUTPUT "${header}"
    COMMAND "${HAYLEN_SOKOL_SHDC}" --input "${SHADER_SOURCE}" --output "${header}" --slang "glsl430:glsl300es:hlsl5:metal_macos:metal_ios:metal_sim:wgsl" --module "${SHADER_MODULE}" ${defines}
    DEPENDS "${SHADER_SOURCE}" ${library}
    WORKING_DIRECTORY "${HAYLEN_SHADER_LIBRARY_DIR}"
    COMMENT "Compiling shader \"${SHADER_MODULE}\""
    VERBATIM
  )
  target_sources(${target} PRIVATE "${header}")
endfunction()
