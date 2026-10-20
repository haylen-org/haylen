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
