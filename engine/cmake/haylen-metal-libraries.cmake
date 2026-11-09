# Precompiles the Metal code of the shader packages that `haylen_compile_shader` adds to the target `SHADERS` with the Metal toolchain of Xcode, so the Metal device loads the programs of the renderer from libraries instead of compiling their source when an app starts. Each library is compiled for the kind of device of the build at its minimum system version, and on iOS, tvOS and visionOS also for the simulator, whose slices pick their own at compile time. The libraries go into the target as the source of `haylen::gpu::metal::PrecompiledLibraries::getLibraries`, keyed by the SHA-256 of the Metal source they were compiled from.
function(haylen_precompile_metal_libraries target)
  cmake_parse_arguments(PARSE_ARGV 1 METAL "" "SHADERS" "")
  get_target_property(packages ${METAL_SHADERS} HAYLEN_SHADER_PACKAGES)

  # Each variant reads `condition,sdk,target,language`: the preprocessor test that picks its libraries, or `none` for the one variant of a platform, the SDK and the AIR target triple that the toolchain compiles for, and the Metal language of the packages it compiles.
  if(HAYLEN_CATALYST)
    string(REGEX REPLACE "^[^-]+-" "air64-" catalyst "${CMAKE_CXX_COMPILER_TARGET}")
    set(variants "none,macosx,${catalyst},msl-ios")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    haylen_metal_minimum(minimum macosx)
    set(variants "none,macosx,air64-apple-macos${minimum},msl-macos")
  else()
    set(systems_iOS "iphoneos;iphonesimulator;ios")
    set(systems_tvOS "appletvos;appletvsimulator;tvos")
    set(systems_visionOS "xros;xrsimulator;xros")
    if(NOT DEFINED systems_${CMAKE_SYSTEM_NAME})
      message(FATAL_ERROR "The Metal libraries of the engine have no target for the system \"${CMAKE_SYSTEM_NAME}\".")
    endif()
    list(GET systems_${CMAKE_SYSTEM_NAME} 0 device_sdk)
    list(GET systems_${CMAKE_SYSTEM_NAME} 1 simulator_sdk)
    list(GET systems_${CMAKE_SYSTEM_NAME} 2 system)
    haylen_metal_minimum(minimum ${device_sdk})
    set(variants "!TARGET_OS_SIMULATOR,${device_sdk},air64-apple-${system}${minimum},msl-ios" "TARGET_OS_SIMULATOR,${simulator_sdk},air64-apple-${system}${minimum}-simulator,msl-ios-simulator")
  endif()

  foreach(variant IN LISTS variants)
    string(REPLACE "," ";" fields "${variant}")
    list(GET fields 1 sdk)
    execute_process(COMMAND xcrun -sdk ${sdk} metal --version RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "The Metal toolchain of Xcode for the SDK \"${sdk}\" is missing, and Apple builds of the engine precompile the shaders of the renderer with it. Install it with \"xcodebuild -downloadComponent MetalToolchain\" and configure again.")
    endif()
  endforeach()

  set(output "${CMAKE_CURRENT_BINARY_DIR}/generated/metal/PrecompiledLibraries.cpp")
  string(REPLACE ";" "|" package_list "${packages}")
  string(REPLACE ";" "|" variant_list "${variants}")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -D "PACKAGES=${package_list}" -D "VARIANTS=${variant_list}" -D "WORK=${CMAKE_CURRENT_BINARY_DIR}/generated/metal" -D "OUTPUT=${output}" -P "${HAYLEN_CMAKE_DIR}/haylen-metal-libraries-generate.cmake"
    DEPENDS ${packages} "${HAYLEN_CMAKE_DIR}/haylen-metal-libraries-generate.cmake"
    COMMENT "Precompiling the Metal libraries of the renderer"
    VERBATIM
  )
  target_sources(${target} PRIVATE "${output}")
endfunction()

# The minimum system version that the libraries load on, which is the deployment target of the build, or the version of the SDK when the build names none, as the compilers of Xcode take it then.
function(haylen_metal_minimum variable sdk)
  if(CMAKE_OSX_DEPLOYMENT_TARGET)
    set(${variable} "${CMAKE_OSX_DEPLOYMENT_TARGET}" PARENT_SCOPE)
    return()
  endif()
  execute_process(COMMAND xcrun -sdk ${sdk} --show-sdk-version OUTPUT_VARIABLE version OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
  set(${variable} "${version}" PARENT_SCOPE)
endfunction()
