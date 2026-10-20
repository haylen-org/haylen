# Settings that every target of the engine and its dependencies compiles with, so release binaries hold nothing of the machine that built them and their linkers drop the code nothing calls.

# Binaries name the files of the engine and of its dependencies by their paths inside their source trees, never by the paths of the machine that built them, such as the file names that assertions keep. MSVC has no such option, so a Windows build keeps the paths that its assertions name.
if(NOT MSVC)
  set(haylen_cpm_cache "${CPM_SOURCE_CACHE}")
  if(NOT haylen_cpm_cache)
    set(haylen_cpm_cache "$ENV{CPM_SOURCE_CACHE}")
  endif()
  add_compile_options("-fmacro-prefix-map=${HAYLEN_ENGINE_DIR}/=engine/" "-fmacro-prefix-map=${CMAKE_BINARY_DIR}/=build/")
  if(haylen_cpm_cache)
    file(REAL_PATH "${haylen_cpm_cache}" haylen_cpm_cache)
    add_compile_options("-fmacro-prefix-map=${haylen_cpm_cache}/=deps/")
  endif()
endif()

# A Release build compiles the engine and its dependencies to the intermediate code of the compiler, so the link of each program optimizes the engine, its dependencies and the program as one and drops what the program never reaches. The prebuilt SDK keeps that code, so an app that links it is optimized the same way. The tools of Apple build an xcframework only from machine code, and the intermediate code of MSVC makes the one library of the SDK larger than the 4 GB that a library of Windows holds, so those compile without it, while a C++ app that compiles the engine with MSVC keeps it.
include(CheckIPOSupported)
check_ipo_supported(RESULT HAYLEN_LINK_TIME_OPTIMIZATION LANGUAGES C CXX)
if(HAYLEN_BUILD_FRAMEWORK OR (MSVC AND HAYLEN_BUILD_SDK))
  set(HAYLEN_LINK_TIME_OPTIMIZATION OFF)
endif()
if(HAYLEN_LINK_TIME_OPTIMIZATION)
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

# Every function and object lands in a section of its own, so linking with "--gc-sections" drops what a release never calls. Apple linkers strip dead code by atom without them.
if(NOT MSVC AND NOT APPLE)
  add_compile_options(-ffunction-sections -fdata-sections)
endif()

# Apple executables export every global symbol unless told otherwise, so the engine and its dependencies keep theirs hidden there, and an app exports only the symbols of its own code, which `native.findSymbol` finds. Other platforms export nothing from an executable by default, and the library of an Android app names its exports in a version script.
if(APPLE)
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
  set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
  set(CMAKE_OBJCXX_VISIBILITY_PRESET hidden)
  set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
endif()
