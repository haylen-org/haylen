# Embeds a binary file into a target as the static method named by the symbol argument of haylen::core::EmbeddedFiles, which the header engine/src/core/EmbeddedFiles.hpp declares.
function(haylen_embed_file target file symbol)
  set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded/${symbol}.cpp")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -D "INPUT=${file}" -D "OUTPUT=${output}" -D "SYMBOL=${symbol}" -P "${HAYLEN_CMAKE_DIR}/haylen-embed-generate.cmake"
    DEPENDS "${file}" "${HAYLEN_CMAKE_DIR}/haylen-embed-generate.cmake"
    COMMENT "Embedding ${file}"
    VERBATIM
  )
  target_sources(${target} PRIVATE "${output}")
endfunction()
