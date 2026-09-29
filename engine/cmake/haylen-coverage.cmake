function(haylen_enable_coverage target)
  if(NOT HAYLEN_ENABLE_COVERAGE)
    return()
  endif()

  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Coverage uses LLVM source-based coverage and requires Clang.")
  endif()

  # Every instrumented program writes its profile into the build tree, including the test discovery that runs after linking.
  set(profile "-fprofile-instr-generate=${CMAKE_BINARY_DIR}/coverage/haylen-%p.profraw")
  target_compile_options(${target} PRIVATE "${profile}" -fcoverage-mapping)
  target_link_options(${target} PRIVATE "${profile}")
endfunction()
