# The GPU tests run the conformance suite of the GPU devices and the reference scenes of the 2D renderer on every device the build compiled, or on the one that `HAYLEN_TEST_GRAPHICS_API` names, with what the devices add to `HAYLEN_GPU_TEST_SOURCES`, `HAYLEN_GPU_TEST_LIBRARIES` and the offscreen contexts of `HAYLEN_GPU_OFFSCREEN_SOURCES` and `HAYLEN_GPU_OFFSCREEN_LIBRARIES`. They need a GPU, so CTest leaves them to `haylen.py test --gpu`.
set(haylen_tests_dir "${HAYLEN_ENGINE_DIR}/tests")
set(haylen_gpu_test_sources
  "${haylen_tests_dir}/gpu/BlendTests.cpp"
  "${haylen_tests_dir}/gpu/DepthStencilTests.cpp"
  "${haylen_tests_dir}/gpu/DrawTests.cpp"
  "${haylen_tests_dir}/gpu/DriverIssuesTests.cpp"
  "${haylen_tests_dir}/gpu/LifetimeTests.cpp"
  "${haylen_tests_dir}/gpu/PassTests.cpp"
  "${haylen_tests_dir}/gpu/RendererImageTests.cpp"
  "${haylen_tests_dir}/gpu/ShaderPackageTests.cpp"
  "${haylen_tests_dir}/gpu/TextureTests.cpp"
  "${haylen_tests_dir}/gpu/support/GpuFixture.cpp"
  "${haylen_tests_dir}/gpu/support/ImageComparison.cpp"
  "${haylen_tests_dir}/gpu/support/LoggedErrors.cpp"
  "${haylen_tests_dir}/gpu/support/OffscreenSurface.cpp"
  "${haylen_tests_dir}/support/AllocationTracker.cpp"
  "${haylen_tests_dir}/support/EngineFixture.cpp"
  "${haylen_tests_dir}/support/Environment.cpp"
  "${haylen_tests_dir}/support/EventLoop.cpp"
  "${haylen_tests_dir}/support/TemporaryDirectory.cpp"
  "${haylen_tests_dir}/support/TestFiles.cpp"
  ${HAYLEN_GPU_TEST_SOURCES}
  ${HAYLEN_GPU_OFFSCREEN_SOURCES}
)
# Android starts the tests as an app, whose NativeActivity loads them as a library, since a program that adb starts reaches the Vulkan driver of some devices only from an app.
if(ANDROID)
  add_library(haylen-gpu-tests SHARED ${haylen_gpu_test_sources})
else()
  add_executable(haylen-gpu-tests ${haylen_gpu_test_sources})
endif()
target_link_libraries(haylen-gpu-tests PRIVATE haylen::engine haylen::headless haylen::gpu ${HAYLEN_GPU_TEST_LIBRARIES} ${HAYLEN_GPU_OFFSCREEN_LIBRARIES})
target_include_directories(haylen-gpu-tests PRIVATE "${haylen_tests_dir}" "${HAYLEN_ENGINE_DIR}/src")
haylen_compile_shader(haylen-gpu-tests SOURCE "${haylen_tests_dir}/gpu/shaders/conformance.glsl" SYMBOL getConformanceShader)
haylen_enable_warnings(haylen-gpu-tests)

# An iOS or tvOS device starts no plain executable, so there the tests are a signed app that carries the reference images and keeps its results in its container, which `haylen.py` installs, launches and copies the results back from.
if(HAYLEN_PLATFORM MATCHES "^(ios|tvos)$" AND NOT HAYLEN_CATALYST AND NOT CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
  file(GLOB haylen_gpu_references CONFIGURE_DEPENDS "${haylen_tests_dir}/data/gpu/*.png")
  target_sources(haylen-gpu-tests PRIVATE
    "${haylen_tests_dir}/gpu/apple/GpuTestApp.mm"
    "${haylen_tests_dir}/gpu/apple/GpuTestFiles.mm"
    "${haylen_tests_dir}/gpu/apple/GpuTestMain.cpp"
    "${haylen_tests_dir}/gpu/apple/HaylenGpuTestSceneDelegate.mm"
    ${haylen_gpu_references}
  )
  set_source_files_properties(${haylen_gpu_references} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/gpu")
  set_target_properties(haylen-gpu-tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>"
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST "${haylen_tests_dir}/gpu/apple/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "Haylen GPU Tests"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.haylen.gputests"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
    MACOSX_BUNDLE_BUNDLE_VERSION "1"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.haylen.gputests"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
  )
  target_link_libraries(haylen-gpu-tests PRIVATE GTest::gtest "-framework UIKit")
elseif(ANDROID)
  # The library goes into an APK that `haylen.py` packages with the reference images as assets, installs and launches, and whose results it copies back from the files of the app.
  set_target_properties(haylen-gpu-tests PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  target_sources(haylen-gpu-tests PRIVATE
    "${haylen_tests_dir}/gpu/android/GpuTestActivity.cpp"
    "${haylen_tests_dir}/gpu/android/GpuTestFiles.cpp"
    "${haylen_tests_dir}/gpu/android/GpuTestMain.cpp"
  )
  target_link_libraries(haylen-gpu-tests PRIVATE GTest::gtest android log)
  # The APK carries the library without its debug information, which would not fit the storage of small devices and emulators, while the tree keeps the whole library for the symbols of crashes.
  add_custom_command(TARGET haylen-gpu-tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin/stripped"
    COMMAND "${CMAKE_STRIP}" --strip-debug "$<TARGET_FILE:haylen-gpu-tests>" -o "${CMAKE_BINARY_DIR}/bin/stripped/libhaylen-gpu-tests.so"
    VERBATIM
  )
else()
  # The simulators of iOS and tvOS run the tests as a plain executable that `simctl spawn` starts, never as an app bundle.
  set_target_properties(haylen-gpu-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>" MACOSX_BUNDLE OFF)
  target_sources(haylen-gpu-tests PRIVATE "${haylen_tests_dir}/gpu/support/GpuTestFiles.cpp")
  target_link_libraries(haylen-gpu-tests PRIVATE GTest::gtest_main)
  target_compile_definitions(haylen-gpu-tests PRIVATE HAYLEN_GPU_REFERENCES_DIR="${haylen_tests_dir}/data/gpu" HAYLEN_GPU_TEST_OUTPUT="${CMAKE_BINARY_DIR}/gpu-test-output")
endif()

# On the web the tests run in the page of `tests/web/gpu-tests.html`, which names the API through the environment and whose files the driver of the page reads the images of the scenes that differ from. They wait for the GPU in turns of the event loop of the browser through the promise integration of WebAssembly, and read the reference images that the page preloads at their paths on this machine.
if(EMSCRIPTEN)
  target_link_options(haylen-gpu-tests PRIVATE
    -sJSPI
    -sEXIT_RUNTIME=1
    -sALLOW_MEMORY_GROWTH=1
    -sSTACK_SIZE=1048576
    "-sEXPORTED_RUNTIME_METHODS=[ENV,FS]"
    "--preload-file=${haylen_tests_dir}/data/gpu@${haylen_tests_dir}/data/gpu"
  )
  set_property(TARGET haylen-gpu-tests APPEND PROPERTY LINK_DEPENDS "${haylen_tests_dir}/web/gpu-tests.html")
  add_custom_command(TARGET haylen-gpu-tests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${haylen_tests_dir}/web/gpu-tests.html" "$<TARGET_FILE_DIR:haylen-gpu-tests>/gpu-tests.html"
    VERBATIM
  )
endif()
