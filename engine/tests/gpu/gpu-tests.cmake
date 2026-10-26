# The GPU tests run the conformance suite of the GPU devices and the reference scenes of the 2D renderer on every device the build compiled, or on the one that `HAYLEN_TEST_GRAPHICS_API` names, with what the devices add to `HAYLEN_GPU_TEST_SOURCES` and `HAYLEN_GPU_TEST_LIBRARIES`. They need a GPU, so CTest leaves them to `haylen.py test --gpu`.
set(haylen_tests_dir "${HAYLEN_ENGINE_DIR}/tests")
add_executable(haylen-gpu-tests
  "${haylen_tests_dir}/gpu/BlendTests.cpp"
  "${haylen_tests_dir}/gpu/DepthStencilTests.cpp"
  "${haylen_tests_dir}/gpu/DrawTests.cpp"
  "${haylen_tests_dir}/gpu/LifetimeTests.cpp"
  "${haylen_tests_dir}/gpu/PassTests.cpp"
  "${haylen_tests_dir}/gpu/RendererImageTests.cpp"
  "${haylen_tests_dir}/gpu/TextureTests.cpp"
  "${haylen_tests_dir}/gpu/support/GpuFixture.cpp"
  "${haylen_tests_dir}/gpu/support/ImageComparison.cpp"
  "${haylen_tests_dir}/gpu/support/LoggedErrors.cpp"
  "${haylen_tests_dir}/gpu/support/OffscreenSurface.cpp"
  "${haylen_tests_dir}/support/AllocationTracker.cpp"
  "${haylen_tests_dir}/support/EngineFixture.cpp"
  "${haylen_tests_dir}/support/EventLoop.cpp"
  "${haylen_tests_dir}/support/TemporaryDirectory.cpp"
  "${haylen_tests_dir}/support/TestFiles.cpp"
  ${HAYLEN_GPU_TEST_SOURCES}
)
set_target_properties(haylen-gpu-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_link_libraries(haylen-gpu-tests PRIVATE haylen::engine haylen::headless haylen::gpu ${HAYLEN_GPU_TEST_LIBRARIES} GTest::gtest_main)
target_include_directories(haylen-gpu-tests PRIVATE "${haylen_tests_dir}" "${HAYLEN_ENGINE_DIR}/src")
target_compile_definitions(haylen-gpu-tests PRIVATE HAYLEN_GPU_REFERENCES_DIR="${haylen_tests_dir}/data/gpu" HAYLEN_GPU_TEST_OUTPUT="${CMAKE_BINARY_DIR}/gpu-test-output")
haylen_compile_shader(haylen-gpu-tests SOURCE "${haylen_tests_dir}/gpu/shaders/conformance.glsl" SYMBOL getConformanceShader)
haylen_enable_warnings(haylen-gpu-tests)

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
