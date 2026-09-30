if(NOT HAYLEN_SANITIZERS MATCHES "^(OFF|ADDRESS|THREAD)$")
  message(FATAL_ERROR "The option \"HAYLEN_SANITIZERS\" is \"OFF\", \"ADDRESS\" or \"THREAD\", not \"${HAYLEN_SANITIZERS}\".")
endif()

# ThreadSanitizer sees only the synchronization of the code it instruments, such as the atomics with which libuv wakes its loop, so it instruments the dependencies too.
if(HAYLEN_SANITIZERS STREQUAL "THREAD" AND HAYLEN_DESKTOP AND NOT MSVC)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

function(haylen_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:preprocessor /utf-8)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow)
    # Designated initializers that leave fields to their default member values are the intended style.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${target} PRIVATE -Wno-missing-designated-field-initializers)
    endif()
  endif()

  if(HAYLEN_SANITIZERS STREQUAL "ADDRESS" AND HAYLEN_DESKTOP AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()
