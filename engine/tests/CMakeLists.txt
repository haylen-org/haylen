set(HAYLEN_TEST_SOURCES
  support/AllocationTrackerTests.cpp
  2d/animation/AnimatedBatchTests.cpp
  2d/animation/AtlasBuilderTests.cpp
  2d/animation/Animation2DTests.cpp
  2d/animation/SkeletonTests.cpp
  2d/animation/SpriteAtlasTests.cpp
  2d/graphics/CameraRigTests.cpp
  2d/graphics/CameraTests.cpp
  2d/graphics/GlassTests.cpp
  2d/graphics/GpuInstanceTests.cpp
  2d/graphics/GroupTests.cpp
  2d/graphics/MeshFringeTests.cpp
  2d/graphics/NineSliceTests.cpp
  2d/graphics/PathTests.cpp
  2d/graphics/ShapeTests.cpp
  2d/graphics/StrokeBuilderTests.cpp
  2d/graphics/SceneTransitionTests.cpp
  2d/graphics/RendererStateTests.cpp
  2d/graphics/RendererTests.cpp
  2d/graphics/TextPainterTests.cpp
  2d/graphics/VectorAtlasTests.cpp
  2d/lighting/Lighting2DTests.cpp
  2d/navigation/CrowdTests.cpp
  2d/navigation/Navigation2DTests.cpp
  2d/navigation/NavMeshTests.cpp
  2d/navigation/PathfindingTests.cpp
  2d/nodes/NodeTreeTests.cpp
  2d/particles/Particles2DTests.cpp
  2d/physics/AssemblyTests.cpp
  2d/physics/BodyMotionTests.cpp
  2d/physics/BodyTransferTests.cpp
  2d/physics/ContactTests.cpp
  2d/physics/DestructionTests.cpp
  2d/physics/FluidTests.cpp
  2d/physics/ForceFieldTests.cpp
  2d/physics/JointRuntimeTests.cpp
  2d/physics/MoverTests.cpp
  2d/physics/RaycasterTests.cpp
  2d/physics/VehicleTests.cpp
  2d/physics/WorldTests.cpp
  2d/physics/WorldTuningTests.cpp
  2d/procedural/PlacementTests.cpp
  2d/procedural/Procedural2DTests.cpp
  2d/procedural/TilingTests.cpp
  2d/spatial/GridAlgorithmTests.cpp
  2d/spatial/OverlapWorldTests.cpp
  2d/spatial/Spatial2DTests.cpp
  2d/tiled/AutoMapperTests.cpp
  2d/tiled/MapQueryTests.cpp
  2d/tiled/TileCollisionTests.cpp
  2d/tiled/TiledTests.cpp
  3d/gltf/AccessorReaderTests.cpp
  3d/gltf/ReaderRulesTests.cpp
  3d/gltf/ReaderTests.cpp
  3d/physics/BackendRegistryTests.cpp
  3d/physics/BodyTests.cpp
  3d/physics/CollisionFilterTests.cpp
  3d/physics/ShapeTests.cpp
  3d/physics/WorldTests.cpp
  3d/physics/jolt/SimulationTests.cpp
  3d/spatial/GridRayTests.cpp
  3d/spatial/KdTreeTests.cpp
  3d/spatial/MeshBvhTests.cpp
  3d/spatial/Spatial3DTests.cpp
  ai/DecisionTests.cpp
  ai/StateMachineTests.cpp
  audio/AudioAssetTests.cpp
  audio/AudioDeviceTests.cpp
  audio/AudioEffectTests.cpp
  audio/AudioInterruptionTests.cpp
  audio/AudioPauseTests.cpp
  audio/AudioStreamTests.cpp
  audio/AudioTapTests.cpp
  audio/FormattedOutputTests.cpp
  audio/FreeverbTests.cpp
  audio/MixerTests.cpp
  audio/OggPageIndexTests.cpp
  audio/PulledOutputTests.cpp
  audio/SoundTests.cpp
  audio/SpatializationTests.cpp
  audio/StreamedSoundTests.cpp
  content/BinaryTests.cpp
  content/ChunkerTests.cpp
  content/CompressionTests.cpp
  content/ContentBuilderTests.cpp
  content/CryptoTests.cpp
  content/HttpTransportTests.cpp
  content/LocalStoreTests.cpp
  content/RecordCacheTests.cpp
  content/ShardTests.cpp
  conversation/ConversationTests.cpp
  core/AppConfigTests.cpp
  core/AppIdentifierTests.cpp
  core/Base64Tests.cpp
  core/ContainerTests.cpp
  core/CoreTests.cpp
  core/FloatBufferTests.cpp
  core/JobSystemTests.cpp
  core/JsonValidatorTests.cpp
  core/LogTests.cpp
  core/MessagePackTests.cpp
  core/SceneManagerTests.cpp
  core/SignalTests.cpp
  core/TweenTests.cpp
  debug/ConsoleTests.cpp
  debug/ProfilerTests.cpp
  debug/TraceRecorderTests.cpp
  gpu/DeviceFactoryTests.cpp
  gpu/direct3d/CompileQueueTests.cpp
  gpu/direct3d/WindowSamplesTests.cpp
  gpu/GpuPreferenceTests.cpp
  gpu/HandleTableTests.cpp
  gpu/NullDeviceTests.cpp
  gpu/SurfaceTurnTests.cpp
  graphics/GradientTests.cpp
  graphics/GraphicsTests.cpp
  graphics/ImageFiltersTests.cpp
  graphics/ImagePaletteTests.cpp
  graphics/MaterialTests.cpp
  graphics/MemoryTrimTests.cpp
  graphics/ShaderTests.cpp
  graphics/WebpImageTests.cpp
  input/ActionMapTests.cpp
  input/CommandBufferTests.cpp
  input/GestureTests.cpp
  input/InputRecordingTests.cpp
  input/InputTests.cpp
  input/PointerEmulationTests.cpp
  io/IoTests.cpp
  localization/CatalogTests.cpp
  localization/FormatTests.cpp
  math/BatchMathTests.cpp
  math/ColorSpaceTests.cpp
  math/ColorTransformTests.cpp
  math/Geometry3DTests.cpp
  math/MatrixTests.cpp
  math/Noise3DTests.cpp
  math/PolygonTests.cpp
  math/ProceduralTests.cpp
  math/QuatTests.cpp
  math/RaycastTests.cpp
  math/ParametricCurve3DTests.cpp
  math/Spline3DTests.cpp
  math/SignalFilterTests.cpp
  math/SphericalCoordinatesTests.cpp
  math/SphericalHarmonicsTests.cpp
  math/Transform3DTests.cpp
  math/TriangulatorTests.cpp
  math/UtilityTests.cpp
  math/Vec3Tests.cpp
  math/VectorTests.cpp
  net/NetRedrawTests.cpp
  net/WebSocketTests.cpp
  platform/AccessibilityTests.cpp
  platform/AndroidBatteryStatusTests.cpp
  platform/AndroidDeviceInfoTests.cpp
  platform/AndroidFramePacerTests.cpp
  platform/AndroidThermalStatusTests.cpp
  platform/CrashGuardTests.cpp
  platform/FoldTests.cpp
  platform/NativeViewsTests.cpp
  platform/StreamTests.cpp
  platform/SystemFontReaderTests.cpp
  platform/FrameCallbackGateTests.cpp
  platform/FrameRateGateTests.cpp
  platform/ProcessorCheckTests.cpp
  platform/PixelDensityCapTests.cpp
  platform/AndroidGamepadStatesTests.cpp
  platform/AndroidGlesVersionTests.cpp
  platform/AndroidGraphicsApisTests.cpp
  platform/AndroidKeyClaimsTests.cpp
  platform/AndroidYuvImageTests.cpp
  platform/DeviceRecoveryTests.cpp
  platform/FrameTimerTests.cpp
  platform/GraphicsApiFallbackTests.cpp
  platform/GraphicsApiOrderTests.cpp
  platform/KeptEventsTests.cpp
  platform/LaunchOptionsTests.cpp
  platform/LinuxSystemReaderTests.cpp
  platform/LinuxVisualColorTests.cpp
  platform/NativeLibrariesTests.cpp
  platform/NativeSignatureTests.cpp
  platform/PlatformTests.cpp
  platform/PluginLoadOrderTests.cpp
  platform/PluginManifestTests.cpp
  platform/PlayerTests.cpp
  platform/ReturnHoldTests.cpp
  platform/ServicesTests.cpp
  platform/SharedTextInputTests.cpp
  platform/sdl/EventsTests.cpp
  platform/sdl/GamepadsTests.cpp
  platform/sdl/HintsTests.cpp
  platform/sdl/PostedEventsTests.cpp
  platform/sdl/MouseMotionTests.cpp
  platform/sdl/OutputPlacementTests.cpp
  platform/sdl/FullscreenRequestTests.cpp
  platform/sdl/TextInputTests.cpp
  platform/sdl/WaylandDisplaysTests.cpp
  platform/sdl/WindowTests.cpp
  platform/SupportedOrientationsTests.cpp
  platform/SurfaceCheckTests.cpp
  platform/SystemTests.cpp
  platform/TakingFramesTests.cpp
  platform/TextHelperTests.cpp
  platform/WebEntryTests.cpp
  platform/MouseModeTests.cpp
  platform/WindowConfigTests.cpp
  platform/WindowPlacementTests.cpp
  platform/DisplayTests.cpp
  platform/WindowsPowerStatusTests.cpp
  platform/worker/EventsTests.cpp
  platform/worker/GamepadsTests.cpp
  platform/worker/KeysTests.cpp
  storage/StorageTests.cpp
  text/BitmapFontTests.cpp
  text/ColorGlyphTests.cpp
  text/FontTests.cpp
  text/GlyphTablesTests.cpp
  text/LineHeightTests.cpp
  text/RichTextLayoutTests.cpp
  text/RichTextMarkupTests.cpp
  text/ShapingTests.cpp
  text/TextPluginTests.cpp
  ui/BackendTests.cpp
  ui/CollectionDiffTests.cpp
  ui/CollectionTests.cpp
  ui/ComponentAssetTests.cpp
  ui/FocusRingTests.cpp
  ui/GuiTests.cpp
  ui/LabelTests.cpp
  ui/LayoutTests.cpp
  ui/MessageStackTests.cpp
  ui/FocusTests.cpp
  ui/GameComponentTests.cpp
  ui/GridLayoutTests.cpp
  ui/MountLinkTests.cpp
  ui/OverlayTests.cpp
  ui/RichTextComponentTests.cpp
  ui/RightToLeftTests.cpp
  ui/ScalingTests.cpp
  ui/ScrollArbiterTests.cpp
  ui/ScrollerTests.cpp
  ui/SizeCacheTests.cpp
  ui/SmoothEdgeTests.cpp
  ui/TextFieldLayoutTests.cpp
  ui/ThemeTests.cpp
  ui/UiRedrawTests.cpp
  ui/WheelTests.cpp
)

# The helpers that the tests of the core and the tests of the Lua layer share, one object library that both programs link, so each program replaces the global allocation functions for `test::AllocationTracker`.
set(HAYLEN_TEST_SUPPORT_SOURCES
  support/ActionBindings.cpp
  support/AllocationTracker.cpp
  support/AudioFiles.cpp
  support/ContentParsers.cpp
  support/ContentServer.cpp
  support/CountingPackage.cpp
  support/DamagedFonts.cpp
  support/FontCollection.cpp
  support/EngineFixture.cpp
  support/EventLoop.cpp
  support/FakeVideoDecoder.cpp
  support/GltfFiles.cpp
  support/MaterialShaders.cpp
  support/ReleaseFixture.cpp
  support/TemporaryDirectory.cpp
  support/TestFiles.cpp
  support/UiFixture.cpp
  support/ZipArchives.cpp
)

# The tests of the platform layer of SDL compile the translation of its events, the controllers, which virtual controllers of SDL stand in for, and the requests of fullscreen, which need no window.
add_library(haylen-test-support OBJECT ${HAYLEN_TEST_SUPPORT_SOURCES})
target_link_libraries(haylen-test-support PUBLIC haylen::engine haylen-content-writer haylen::headless haylen::gpu SDL3::SDL3-static haylen-harfbuzz haylen-miniaudio haylen-stb haylen-monocypher libzip::zip ZLIB::ZLIB libzstd_static Poco::Net GTest::gtest_main)
# The tests reach into the engine's private headers on purpose, to check internals the public API hides.
target_include_directories(haylen-test-support PUBLIC "${CMAKE_CURRENT_LIST_DIR}" "${HAYLEN_ENGINE_DIR}/src")
haylen_enable_warnings(haylen-test-support)
haylen_enable_coverage(haylen-test-support)

# A link of a test program takes gigabytes of memory, and most under AddressSanitizer, so the two test programs link one at a time and a machine that builds both never runs out of memory. The pool binds the generators that run jobs side by side, such as Ninja.
set_property(GLOBAL APPEND PROPERTY JOB_POOLS haylen-test-link=1)

add_executable(haylen-tests ${HAYLEN_TEST_SOURCES} "${HAYLEN_ENGINE_DIR}/src/platform/NativeHost.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/Player.cpp" "${HAYLEN_ENGINE_DIR}/src/port/PortHost.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Events.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/FullscreenRequest.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Gamepads.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/GlContext.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Hints.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Keys.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/OutputPlacement.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/PostedEvents.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/TextInput.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/WaylandDisplays.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Window.cpp")
set_target_properties(haylen-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>" JOB_POOL_LINK haylen-test-link)
# The tests export the variables that the drivers of a laptop with two GPUs read, like the player and the apps.
if(WIN32)
  target_sources(haylen-tests PRIVATE "${HAYLEN_ENGINE_DIR}/src/gpu/windows/GpuPreferenceExports.cpp")
endif()
# The objects of the headless host reach only the programs that link it themselves.
target_link_libraries(haylen-tests PRIVATE haylen-test-support haylen::headless)
# The tests of the backend of Jolt build bodies with the API of Jolt itself.
target_link_libraries(haylen-tests PRIVATE Jolt)
# The queue of the compilations of Direct3D holds plain threads, so its tests run on every desktop, with its source from the GPU library on Windows and compiled into the tests elsewhere.
if(NOT WIN32)
  target_sources(haylen-tests PRIVATE "${HAYLEN_ENGINE_DIR}/src/gpu/direct3d/CompileQueue.cpp")
endif()
# The shader tests compile a material with `haylen app shaders`, the way apps compile theirs, running the tooling of the checkout through uv, the font tests read subsets of open fonts in `data/fonts` that keep the characters the tests use, the audio tests read short tones made for them in `data/audio`, in every compressed format the engine decodes, and the video tests read a clip made for them in `data/video`, the glTF tests read the files of `data/gltf`, which `tools/gltf-fixtures.py` writes, and the WebP tests read the images of `data/images`.
find_program(HAYLEN_UV uv REQUIRED)
target_compile_definitions(haylen-test-support PUBLIC HAYLEN_UV="${HAYLEN_UV}" HAYLEN_TOOLING_ROOT="${HAYLEN_ENGINE_DIR}/.." HAYLEN_SHADER_COMPILER="${HAYLEN_SHADERC}" HAYLEN_TEST_APP_IDENTIFIERS="${CMAKE_CURRENT_LIST_DIR}/data/app-identifiers.json" HAYLEN_TEST_FONTS="${CMAKE_CURRENT_LIST_DIR}/data/fonts" HAYLEN_TEST_AUDIO="${CMAKE_CURRENT_LIST_DIR}/data/audio" HAYLEN_TEST_VIDEO="${CMAKE_CURRENT_LIST_DIR}/data/video" HAYLEN_TEST_GLTF="${CMAKE_CURRENT_LIST_DIR}/data/gltf" HAYLEN_TEST_IMAGES="${CMAKE_CURRENT_LIST_DIR}/data/images" HAYLEN_DOCS_DIR="${HAYLEN_ENGINE_DIR}/../docs")
# On Windows the tests also run the decoder of Media Foundation, the font fallback of DirectWrite and the reader of the accessibility settings, which need no window, on the clip made for the tests in `data/video`.
if(WIN32)
  target_sources(haylen-tests PRIVATE platform/WindowsAccessibilityTests.cpp platform/WindowsFontsTests.cpp platform/WindowsSettingsTests.cpp platform/WindowsVideoDecoderTests.cpp "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsAccessibilityElement.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsAccessibilityRoot.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsAccessibilityTree.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsFonts.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsFontText.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsSettings.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsText.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsVideoDecoder.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsPackageStream.cpp")
  target_link_libraries(haylen-tests PRIVATE advapi32 dwrite ole32 oleaut32 mfplat mfreadwrite mfuuid uiautomationcore)
endif()
# On Linux the tests also find the fonts of the system through fontconfig, where the system has it.
if(HAYLEN_PLATFORM STREQUAL "linux")
  target_sources(haylen-tests PRIVATE platform/LinuxFontsTests.cpp "${HAYLEN_ENGINE_DIR}/src/platform/linux/LinuxFonts.cpp")
  target_link_libraries(haylen-tests PRIVATE ${CMAKE_DL_LIBS})
endif()
# On Apple platforms the tests also find the fonts of the system through CoreText and turn the text of apps into strings of Foundation.
if(APPLE)
  target_sources(haylen-tests PRIVATE platform/AppleFontsTests.mm "${HAYLEN_ENGINE_DIR}/src/platform/apple/AppleFonts.mm")
  set_source_files_properties(platform/AppleFontsTests.mm "${HAYLEN_ENGINE_DIR}/src/platform/apple/AppleFonts.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_sources(haylen-tests PRIVATE platform/AppleStringsTests.mm "${HAYLEN_ENGINE_DIR}/src/platform/apple/AppleStrings.mm")
  set_source_files_properties(platform/AppleStringsTests.mm "${HAYLEN_ENGINE_DIR}/src/platform/apple/AppleStrings.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(haylen-tests PRIVATE "-framework CoreText" "-framework Foundation")
endif()
# The graveyard of the Metal device keeps the objects that frames still use, which its tests check without a GPU.
if("metal" IN_LIST HAYLEN_GRAPHICS_APIS)
  target_sources(haylen-tests PRIVATE gpu/metal/GraveyardTests.mm)
  set_source_files_properties(gpu/metal/GraveyardTests.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()
haylen_enable_warnings(haylen-tests)
haylen_enable_coverage(haylen-tests)

include("${CMAKE_CURRENT_LIST_DIR}/gpu/gpu-tests.cmake")

# The native library that the native interop tests call, a C++ library behind the C entry points that the tests declare. It sits next to the test executable and the headless player, where `native.load` finds it by its exact name.
find_package(Threads REQUIRED)
add_library(haylen-native-test SHARED native/NativeTest.cpp native/NativeTestLibrary.cpp native/NativeTestValues.cpp)
set_target_properties(haylen-native-test PROPERTIES OUTPUT_NAME native-test CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_compile_definitions(haylen-native-test PRIVATE NATIVE_TEST_ORIGIN="dynamic")
target_include_directories(haylen-native-test PRIVATE "${HAYLEN_ENGINE_DIR}/include")
target_link_libraries(haylen-native-test PRIVATE nlohmann_json::nlohmann_json Threads::Threads)
haylen_enable_warnings(haylen-native-test)
add_dependencies(haylen-tests haylen-native-test)
target_compile_definitions(haylen-test-support PUBLIC HAYLEN_NATIVE_TEST_LIBRARY="$<TARGET_FILE:haylen-native-test>")

# The desktop player keeps a folder of its own, so the library goes next to it too, where the tests of native libraries in the test project load it by name.
if(TARGET haylen AND TARGET haylen-headless)
  get_target_property(player_folder haylen RUNTIME_OUTPUT_DIRECTORY)
  add_custom_command(TARGET haylen-native-test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${player_folder}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:haylen-native-test>" "${player_folder}"
    VERBATIM
  )
  target_compile_definitions(haylen-test-support PUBLIC HAYLEN_PLAYER_FOLDER="${player_folder}" HAYLEN_HEADLESS_PLAYER_FOLDER="$<TARGET_FILE_DIR:haylen-headless>")
endif()

# Every test runs with a time limit far above the slowest one, so a test that hangs fails alone and the rest of the suite still runs. The tests find their data through absolute paths, so discovery and every test run in the build tree and write nothing into the sources.
include(GoogleTest)
gtest_discover_tests(haylen-tests
  DISCOVERY_TIMEOUT 60
  PROPERTIES TIMEOUT 120
)

# The fuzzers of the content formats, of input recordings, of MessagePack, of image placeholders, of image files, of SVG documents, of Ogg pages, of TrueType fonts, of glTF files, of BMFont files and of the data of 3D physics shapes run their parsers on the inputs libFuzzer generates.
if(HAYLEN_BUILD_FUZZERS)
  add_executable(haylen-content-fuzzer fuzz/ContentFuzzer.cpp support/ContentParsers.cpp support/ReleaseFixture.cpp support/TemporaryDirectory.cpp support/TestFiles.cpp)
  set_target_properties(haylen-content-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_include_directories(haylen-content-fuzzer PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${HAYLEN_ENGINE_DIR}/src")
  target_link_libraries(haylen-content-fuzzer PRIVATE haylen-content-writer haylen::headless haylen-stb haylen-monocypher)
  target_link_options(haylen-content-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-content-fuzzer)

  add_executable(haylen-input-fuzzer fuzz/InputRecordingFuzzer.cpp)
  set_target_properties(haylen-input-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_link_libraries(haylen-input-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-input-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-input-fuzzer)

  add_executable(haylen-messagepack-fuzzer fuzz/MessagePackFuzzer.cpp)
  set_target_properties(haylen-messagepack-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_link_libraries(haylen-messagepack-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-messagepack-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-messagepack-fuzzer)

  add_executable(haylen-placeholder-fuzzer fuzz/ImagePlaceholderFuzzer.cpp)
  set_target_properties(haylen-placeholder-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_link_libraries(haylen-placeholder-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-placeholder-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-placeholder-fuzzer)

  add_executable(haylen-image-fuzzer fuzz/ImageFuzzer.cpp)
  set_target_properties(haylen-image-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_link_libraries(haylen-image-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-image-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-image-fuzzer)

  add_executable(haylen-vector-image-fuzzer fuzz/VectorImageFuzzer.cpp)
  set_target_properties(haylen-vector-image-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_link_libraries(haylen-vector-image-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-vector-image-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-vector-image-fuzzer)

  add_executable(haylen-ogg-fuzzer fuzz/OggPageFuzzer.cpp)
  set_target_properties(haylen-ogg-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_include_directories(haylen-ogg-fuzzer PRIVATE "${HAYLEN_ENGINE_DIR}/src")
  target_link_libraries(haylen-ogg-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-ogg-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-ogg-fuzzer)

  add_executable(haylen-font-fuzzer fuzz/FontFuzzer.cpp)
  set_target_properties(haylen-font-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_include_directories(haylen-font-fuzzer PRIVATE "${HAYLEN_ENGINE_DIR}/src")
  target_link_libraries(haylen-font-fuzzer PRIVATE haylen::engine haylen::headless haylen-harfbuzz)
  target_link_options(haylen-font-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-font-fuzzer)

  add_executable(haylen-gltf-fuzzer fuzz/GltfFuzzer.cpp)
  set_target_properties(haylen-gltf-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_link_libraries(haylen-gltf-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-gltf-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-gltf-fuzzer)

  add_executable(haylen-bitmap-font-fuzzer fuzz/BitmapFontFuzzer.cpp)
  set_target_properties(haylen-bitmap-font-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_include_directories(haylen-bitmap-font-fuzzer PRIVATE "${HAYLEN_ENGINE_DIR}/src")
  target_link_libraries(haylen-bitmap-font-fuzzer PRIVATE haylen::engine haylen::headless)
  target_link_options(haylen-bitmap-font-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-bitmap-font-fuzzer)

  add_executable(haylen-physics-shape-fuzzer fuzz/PhysicsShapeFuzzer.cpp)
  set_target_properties(haylen-physics-shape-fuzzer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  target_include_directories(haylen-physics-shape-fuzzer PRIVATE "${HAYLEN_ENGINE_DIR}/src")
  target_link_libraries(haylen-physics-shape-fuzzer PRIVATE haylen::engine)
  target_link_options(haylen-physics-shape-fuzzer PRIVATE -fsanitize=fuzzer)
  haylen_enable_warnings(haylen-physics-shape-fuzzer)
endif()
