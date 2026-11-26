# Fails when the Ninja build of the tree in `TREE` runs a link of a program or a shared library outside the pool of links, or when the pool does not hold the `DEPTH` links that the memory of the machine gives it.
file(READ "${TREE}/CMakeFiles/rules.ninja" rules)
string(FIND "${rules}" "pool haylen-link\n  depth = ${DEPTH}\n" pool)
if(pool EQUAL -1)
  message(FATAL_ERROR "The build of \"${TREE}\" defines no pool \"haylen-link\" of ${DEPTH} links.")
endif()

# Commands of the build hold semicolons, which would split the statements of a CMake list.
file(READ "${TREE}/build.ninja" build)
string(REPLACE ";" "," build "${build}")
string(REGEX MATCHALL "build [^\n]*_(EXECUTABLE|SHARED_LIBRARY|SHARED_MODULE)_LINKER__[^\n]*\n(  [^\n]*\n)*" links "${build}")
list(LENGTH links count)
if(count EQUAL 0)
  message(FATAL_ERROR "The build of \"${TREE}\" links no program.")
endif()

foreach(link IN LISTS links)
  string(FIND "${link}" "\n  pool = haylen-link\n" pooled)
  if(pooled EQUAL -1)
    string(REGEX MATCH "^build ([^:]*)" statement "${link}")
    message(FATAL_ERROR "The link of \"${CMAKE_MATCH_1}\" in \"${TREE}\" runs outside the pool \"haylen-link\".")
  endif()
endforeach()
