set(HAYLEN_TEST_SOURCES
  2d/animation/AnimatedBatchTests.cpp
  2d/animation/AtlasBuilderTests.cpp
  2d/animation/Animation2DTests.cpp
  2d/animation/ChainTests.cpp
  2d/animation/FrameDataTests.cpp
  2d/animation/SkeletonTests.cpp
  2d/animation/StateGraphTests.cpp
  2d/graphics/CameraRigTests.cpp
  2d/graphics/CameraTests.cpp
  2d/graphics/Graphics2DLuaTests.cpp
  2d/graphics/GpuInstanceTests.cpp
  2d/graphics/GroupTests.cpp
  2d/graphics/NineSliceTests.cpp
  2d/graphics/StrokeBuilderTests.cpp
  2d/graphics/SceneTransitionTests.cpp
  2d/graphics/TileLayerTests.cpp
  2d/graphics/PartMaskTests.cpp
  2d/graphics/PostProcessTests.cpp
  2d/graphics/RendererStateTests.cpp
  2d/graphics/RendererTests.cpp
  2d/graphics/ScreenEffectsTests.cpp
  2d/graphics/ShapeTests.cpp
  2d/graphics/SpriteDataTests.cpp
  2d/graphics/TextPainterTests.cpp
  2d/graphics/VectorAtlasTests.cpp
  2d/lighting/Lighting2DTests.cpp
  2d/navigation/CrowdTests.cpp
  2d/nodes/NodeTreeTests.cpp
  2d/navigation/NavMeshTests.cpp
  2d/navigation/Navigation2DTests.cpp
  2d/navigation/NavigationLuaTests.cpp
  2d/navigation/PathfindingTests.cpp
  2d/particles/Particles2DLuaTests.cpp
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
  2d/physics/Physics2DTests.cpp
  2d/physics/PhysicsFeaturesLuaTests.cpp
  2d/physics/RaycasterTests.cpp
  2d/physics/VehicleTests.cpp
  2d/physics/WorldTuningTests.cpp
  2d/procedural/Procedural2DLuaTests.cpp
  2d/procedural/Procedural2DTests.cpp
  2d/procedural/TilingTests.cpp
  2d/spatial/GridAlgorithmTests.cpp
  2d/spatial/OverlapWorldTests.cpp
  2d/spatial/Spatial2DTests.cpp
  2d/tiled/AutoMapperTests.cpp
  2d/tiled/MapQueryTests.cpp
  2d/tiled/TileCollisionTests.cpp
  2d/tiled/TiledTests.cpp
  ai/DecisionLuaTests.cpp
  ai/DecisionTests.cpp
  ai/StateMachineTests.cpp
  assets/AssetTests.cpp
  audio/AudioStreamTests.cpp
  audio/AudioTests.cpp
  audio/EffectTests.cpp
  audio/MixerLuaTests.cpp
  audio/OutputTests.cpp
  audio/PauseTests.cpp
  audio/SpatializationTests.cpp
  audio/StreamedSoundTests.cpp
  content/BootstrapTests.cpp
  content/BootstrapWriterTests.cpp
  content/CatalogTests.cpp
  content/ChannelPublisherTests.cpp
  content/ChunkerTests.cpp
  content/CompressionTests.cpp
  content/ContentBuilderTests.cpp
  content/ContentLuaTests.cpp
  content/ContentManagerTests.cpp
  content/ContentToolTests.cpp
  content/CryptoTests.cpp
  content/HostileInputTests.cpp
  content/HpakPackageTests.cpp
  content/HttpTransportTests.cpp
  content/KeyStoreTests.cpp
  content/LocalStoreTests.cpp
  content/LuaCompilerTests.cpp
  content/ManifestTests.cpp
  content/RecordCacheTests.cpp
  content/ReleaseAcceptanceTests.cpp
  content/ReleaseBuilderTests.cpp
  content/ReleaseInspectorTests.cpp
  content/ReleaseLuaTests.cpp
  content/ReleaseStagerTests.cpp
  content/ReleaseStreamingTests.cpp
  content/ShardTests.cpp
  content/UpdaterTests.cpp
  core/ContainerTests.cpp
  conversation/ConversationTests.cpp
  core/CoreTests.cpp
  core/MessagePackTests.cpp
  core/EngineTests.cpp
  core/ErrorReportTests.cpp
  core/ErrorScreenTests.cpp
  core/RuntimeTests.cpp
  core/SceneManagerTests.cpp
  core/SignalTests.cpp
  core/TweenTests.cpp
  debug/ConsoleTests.cpp
  debug/DebugTests.cpp
  debug/DrawingTests.cpp
  debug/HotReloadTests.cpp
  debug/InspectorTests.cpp
  debug/StatsTests.cpp
  debug/TraceRecorderTests.cpp
  gpu/DeviceFactoryTests.cpp
  gpu/direct3d/CompileQueueTests.cpp
  gpu/NullDeviceTests.cpp
  graphics/GradientTests.cpp
  graphics/GraphicsLuaTests.cpp
  graphics/GraphicsTests.cpp
  graphics/ImageFiltersTests.cpp
  graphics/ImagePaletteTests.cpp
  graphics/ShaderTests.cpp
  graphics/VectorImageTests.cpp
  graphics/VideoTests.cpp
  input/CommandBufferTests.cpp
  input/GestureTests.cpp
  input/InputRecordingTests.cpp
  input/InputTests.cpp
  input/PointerEmulationTests.cpp
  io/IoTests.cpp
  localization/FormatTests.cpp
  localization/LocalizationTests.cpp
  lua/AssetsLuaTests.cpp
  lua/BufferLuaTests.cpp
  lua/CollectionsLuaTests.cpp
  lua/CoreLuaTests.cpp
  lua/EventsLuaTests.cpp
  lua/GeometryLuaTests.cpp
  lua/InputLuaTests.cpp
  lua/MathLuaTests.cpp
  lua/LuaReferenceTests.cpp
  lua/ModuleReloadTests.cpp
  lua/SceneLuaTests.cpp
  lua/ToolkitTests.cpp
  lua/TweenLuaTests.cpp
  lua/YieldTests.cpp
  math/ColorTransformTests.cpp
  math/SignalFilterTests.cpp
  math/PolygonTests.cpp
  math/ProceduralTests.cpp
  math/RaycastTests.cpp
  math/TriangulatorTests.cpp
  math/UtilityTests.cpp
  math/VectorTests.cpp
  net/WebSocketTests.cpp
  platform/AccessibilityTests.cpp
  platform/AndroidBatteryStatusTests.cpp
  platform/AndroidDeviceInfoTests.cpp
  platform/AndroidDialogJsonTests.cpp
  platform/AndroidFramePacerTests.cpp
  platform/AndroidGamepadStatesTests.cpp
  platform/AndroidGlesVersionTests.cpp
  platform/AndroidGraphicsApisTests.cpp
  platform/AndroidKeyClaimsTests.cpp
  platform/AndroidYuvImageTests.cpp
  platform/CrashGuardTests.cpp
  platform/DevelopmentConnectionTests.cpp
  platform/DialogsTests.cpp
  platform/FoldTests.cpp
  platform/GraphicsApiOrderTests.cpp
  platform/HeadlessPlayerTests.cpp
  platform/LinuxFilePatternTests.cpp
  platform/LinuxSystemReaderTests.cpp
  platform/MediaSessionTests.cpp
  platform/NativeLuaTests.cpp
  platform/NativeTests.cpp
  platform/NativeViewsTests.cpp
  platform/PlatformLuaTests.cpp
  platform/PlatformTests.cpp
  platform/PluginLoadOrderTests.cpp
  platform/ScreensTests.cpp
  platform/sdl/EventsTests.cpp
  platform/sdl/GamepadsTests.cpp
  platform/sdl/FrameTimerTests.cpp
  platform/StreamTests.cpp
  platform/SystemTests.cpp
  platform/WebDialogJsonTests.cpp
  platform/WebEntryTests.cpp
  platform/WindowLuaTests.cpp
  platform/WindowTests.cpp
  platform/WindowsPowerStatusTests.cpp
  storage/StorageTests.cpp
  support/AllocationTracker.cpp
  support/AllocationTrackerTests.cpp
  support/ContentParsers.cpp
  support/ContentServer.cpp
  support/CountingPackage.cpp
  support/EngineFixture.cpp
  support/EventLoop.cpp
  support/FakeVideoDecoder.cpp
  support/ReleaseFixture.cpp
  support/TemporaryDirectory.cpp
  support/TestFiles.cpp
  support/UiFixture.cpp
  text/BitmapFontTests.cpp
  text/FontTests.cpp
  text/RichTextLayoutTests.cpp
  text/RichTextMarkupTests.cpp
  text/ShapingTests.cpp
  text/TextLuaTests.cpp
  ui/AccessibilityTreeTests.cpp
  ui/AnchorTests.cpp
  ui/BackendTests.cpp
  ui/CollectionDiffTests.cpp
  ui/CollectionLuaTests.cpp
  ui/CollectionTests.cpp
  ui/ComponentTests.cpp
  ui/FocusRingTests.cpp
  ui/GuiTests.cpp
  ui/ImageLibraryTests.cpp
  ui/LabelTests.cpp
  ui/LayoutTests.cpp
  ui/FocusTests.cpp
  ui/GameComponentTests.cpp
  ui/GridLayoutTests.cpp
  ui/MountLinkTests.cpp
  ui/NodeEventTests.cpp
  ui/OverlayTests.cpp
  ui/PlayAreaTests.cpp
  ui/RichTextComponentTests.cpp
  ui/RightToLeftTests.cpp
  ui/ScalingTests.cpp
  ui/SceneGuiTests.cpp
  ui/ScrollArbiterTests.cpp
  ui/ScrollbarTests.cpp
  ui/ScrollerTests.cpp
  ui/SizeCacheTests.cpp
  ui/SmoothEdgeTests.cpp
  ui/TextInputTests.cpp
  ui/StyleTests.cpp
  ui/ThemeTests.cpp
  ui/ToastStackTests.cpp
  ui/TransformTests.cpp
  ui/UiLuaTests.cpp
  ui/WheelTests.cpp
)

# The tests of the platform layer of SDL compile the translation of its events, the controllers, which virtual controllers of SDL stand in for, and the timing of the frames, which need no window.
add_executable(haylen-tests ${HAYLEN_TEST_SOURCES} "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Events.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/FrameTimer.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Gamepads.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/sdl/Keys.cpp")
set_target_properties(haylen-tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_link_libraries(haylen-tests PRIVATE haylen::engine haylen::headless haylen::gpu haylen-content-tool SDL3::SDL3-static haylen-miniaudio haylen-stb haylen-monocypher libzip::zip ZLIB::ZLIB libzstd_static Poco::Net GTest::gtest_main)
# The tests reach into the engine's private headers on purpose, to check internals the public API hides.
target_include_directories(haylen-tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${HAYLEN_ENGINE_DIR}/src")
# The queue of the compilations of Direct3D holds plain threads, so its tests run on every desktop, with its source from the GPU library on Windows and compiled into the tests elsewhere.
if(NOT WIN32)
  target_sources(haylen-tests PRIVATE "${HAYLEN_ENGINE_DIR}/src/gpu/direct3d/CompileQueue.cpp")
endif()
# The shader tests compile a material with `haylen.py shaders`, the way apps compile theirs, the font tests read subsets of open fonts in `data/fonts` that keep the characters the tests use, the audio tests read short tones made for them in `data/audio`, in every compressed format the engine decodes, and the video tests read a clip made for them in `data/video`.
find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
target_compile_definitions(haylen-tests PRIVATE HAYLEN_PYTHON="${Python3_EXECUTABLE}" HAYLEN_TOOL_SCRIPT="${HAYLEN_ENGINE_DIR}/../haylen.py" HAYLEN_SHADER_COMPILER="${HAYLEN_SHADERC}" HAYLEN_TEST_FONTS="${CMAKE_CURRENT_LIST_DIR}/data/fonts" HAYLEN_TEST_AUDIO="${CMAKE_CURRENT_LIST_DIR}/data/audio" HAYLEN_TEST_VIDEO="${CMAKE_CURRENT_LIST_DIR}/data/video" HAYLEN_DOCS_DIR="${HAYLEN_ENGINE_DIR}/../docs")
# On Windows the tests also run the decoder of Media Foundation and the reader of the accessibility settings, which need no window, on the clip made for the tests in `data/video`.
if(WIN32)
  target_sources(haylen-tests PRIVATE platform/WindowsAccessibilityTests.cpp platform/WindowsSettingsTests.cpp platform/WindowsVideoDecoderTests.cpp "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsAccessibilityElement.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsAccessibilityRoot.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsAccessibilityTree.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsSettings.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsVideoDecoder.cpp" "${HAYLEN_ENGINE_DIR}/src/platform/windows/WindowsPackageStream.cpp")
  target_link_libraries(haylen-tests PRIVATE advapi32 ole32 oleaut32 mfplat mfreadwrite mfuuid uiautomationcore)
endif()
haylen_enable_warnings(haylen-tests)
haylen_enable_coverage(haylen-tests)

include("${CMAKE_CURRENT_LIST_DIR}/gpu/gpu-tests.cmake")

# The native library that the native interop tests call. It sits next to the test executable and the headless player, where `native.load` finds it by its exact name, which the Debug postfix that Poco forces would change.
add_library(haylen-native-test SHARED native/NativeTest.c)
set_target_properties(haylen-native-test PROPERTIES OUTPUT_NAME native-test DEBUG_POSTFIX "" C_VISIBILITY_PRESET hidden LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_include_directories(haylen-native-test PRIVATE "${HAYLEN_ENGINE_DIR}/include")
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(haylen-native-test PRIVATE Threads::Threads)
endif()
haylen_enable_warnings(haylen-native-test)
add_dependencies(haylen-tests haylen-native-test)
target_compile_definitions(haylen-tests PRIVATE HAYLEN_NATIVE_TEST_LIBRARY="$<TARGET_FILE:haylen-native-test>")

# The desktop player keeps a folder of its own, so the library goes next to it too, where the tests of native libraries in the test project load it by name.
if(TARGET haylen AND TARGET haylen-headless)
  get_target_property(player_folder haylen RUNTIME_OUTPUT_DIRECTORY)
  add_custom_command(TARGET haylen-native-test POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${player_folder}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:haylen-native-test>" "${player_folder}"
    VERBATIM
  )
  target_compile_definitions(haylen-tests PRIVATE HAYLEN_PLAYER_FOLDER="${player_folder}" HAYLEN_HEADLESS_PLAYER_FOLDER="$<TARGET_FILE_DIR:haylen-headless>")
endif()

# Every test runs with a time limit far above the slowest one, so a test that hangs fails alone and the rest of the suite still runs.
include(GoogleTest)
gtest_discover_tests(haylen-tests
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  DISCOVERY_TIMEOUT 60
  PROPERTIES TIMEOUT 120
)

# The fuzzers of the content formats, of input recordings, of MessagePack and of image placeholders run their parsers on the inputs libFuzzer generates.
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
endif()
