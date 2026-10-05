set(HAYLEN_TEST_SOURCES
  2d/animation/Animation2DTests.cpp
  2d/graphics/CameraTests.cpp
  2d/graphics/Graphics2DLuaTests.cpp
  2d/graphics/RendererStateTests.cpp
  2d/graphics/RendererTests.cpp
  2d/graphics/TextPainterTests.cpp
  2d/lighting/Lighting2DTests.cpp
  2d/navigation/CrowdTests.cpp
  2d/navigation/NavMeshTests.cpp
  2d/navigation/Navigation2DTests.cpp
  2d/navigation/NavigationLuaTests.cpp
  2d/navigation/PathfindingTests.cpp
  2d/particles/Particles2DTests.cpp
  2d/physics/AssemblyTests.cpp
  2d/physics/DestructionTests.cpp
  2d/physics/Physics2DTests.cpp
  2d/physics/PhysicsFeaturesLuaTests.cpp
  2d/physics/RaycasterTests.cpp
  2d/procedural/Procedural2DLuaTests.cpp
  2d/procedural/Procedural2DTests.cpp
  2d/procedural/TilingTests.cpp
  2d/spatial/GridAlgorithmTests.cpp
  2d/spatial/Spatial2DTests.cpp
  2d/tiled/MapQueryTests.cpp
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
  core/ContainerTests.cpp
  core/CoreTests.cpp
  core/EngineTests.cpp
  core/ErrorScreenTests.cpp
  core/RuntimeTests.cpp
  core/SceneManagerTests.cpp
  core/SignalTests.cpp
  core/TweenTests.cpp
  debug/DebugTests.cpp
  debug/HotReloadTests.cpp
  debug/StatsTests.cpp
  graphics/GraphicsLuaTests.cpp
  graphics/GraphicsTests.cpp
  graphics/ShaderTests.cpp
  input/GestureTests.cpp
  input/InputTests.cpp
  io/IoTests.cpp
  localization/LocalizationTests.cpp
  lua/AssetsLuaTests.cpp
  lua/BufferLuaTests.cpp
  lua/CollectionsLuaTests.cpp
  lua/CoreLuaTests.cpp
  lua/EventsLuaTests.cpp
  lua/GeometryLuaTests.cpp
  lua/InputLuaTests.cpp
  lua/MathLuaTests.cpp
  lua/SceneLuaTests.cpp
  lua/ToolkitTests.cpp
  lua/TweenLuaTests.cpp
  math/ColorTransformTests.cpp
  math/PolygonTests.cpp
  math/ProceduralTests.cpp
  math/RaycastTests.cpp
  math/UtilityTests.cpp
  math/VectorTests.cpp
  net/WebSocketTests.cpp
  platform/AndroidBatteryStatusTests.cpp
  platform/AndroidDeviceInfoTests.cpp
  platform/AndroidDialogJsonTests.cpp
  platform/AndroidGamepadStatesTests.cpp
  platform/AndroidGlesVersionTests.cpp
  platform/DialogsTests.cpp
  platform/LinuxFilePatternTests.cpp
  platform/LinuxSystemReaderTests.cpp
  platform/NativeLuaTests.cpp
  platform/NativeTests.cpp
  platform/NativeViewsTests.cpp
  platform/PlatformLuaTests.cpp
  platform/PlatformTests.cpp
  platform/PluginLoadOrderTests.cpp
  platform/ScreensTests.cpp
  platform/StreamTests.cpp
  platform/SystemTests.cpp
  platform/WebDialogJsonTests.cpp
  platform/WindowLuaTests.cpp
  platform/WindowTests.cpp
  platform/WindowsPowerStatusTests.cpp
  storage/StorageTests.cpp
  support/EngineFixture.cpp
  support/TemporaryDirectory.cpp
  support/TestFiles.cpp
  support/UiFixture.cpp
  text/BitmapFontTests.cpp
  text/FontTests.cpp
  text/RichTextLayoutTests.cpp
  text/RichTextMarkupTests.cpp
  text/ShapingTests.cpp
  text/TextLuaTests.cpp
  ui/AnchorTests.cpp
  ui/BackendTests.cpp
  ui/ComponentTests.cpp
  ui/DocumentTests.cpp
  ui/FocusTests.cpp
  ui/GameComponentTests.cpp
  ui/MountLinkTests.cpp
  ui/RichTextComponentTests.cpp
  ui/RightToLeftTests.cpp
  ui/TextInputTests.cpp
  ui/ThemeTests.cpp
  ui/TransformTests.cpp
  ui/UiLuaTests.cpp
)

add_executable(haylen_tests ${HAYLEN_TEST_SOURCES})
set_target_properties(haylen_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_link_libraries(haylen_tests PRIVATE haylen::engine haylen::headless haylen_sokol_headers haylen_miniaudio haylen_stb libzip::zip ZLIB::ZLIB libzstd_static Poco::Net GTest::gtest_main)
# The tests reach into the engine's private headers on purpose, to check internals the public API hides.
target_include_directories(haylen_tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${HAYLEN_ENGINE_DIR}/src")
# The shader tests compile a material with `haylen.py shaders`, the way apps compile theirs, and the font tests read subsets of open fonts in `data/fonts` that keep the characters the tests use.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
target_compile_definitions(haylen_tests PRIVATE HAYLEN_PYTHON="${Python3_EXECUTABLE}" HAYLEN_TOOL_SCRIPT="${HAYLEN_ENGINE_DIR}/../haylen.py" HAYLEN_TEST_FONTS="${CMAKE_CURRENT_LIST_DIR}/data/fonts")
haylen_enable_warnings(haylen_tests)
haylen_enable_coverage(haylen_tests)

# The native library that the native interop tests call. It sits next to the test executable, where `native.load` finds it by its exact name, which the Debug postfix that Poco forces would change.
add_library(haylen_native_test SHARED native/NativeTest.c)
set_target_properties(haylen_native_test PROPERTIES OUTPUT_NAME native_test DEBUG_POSTFIX "" C_VISIBILITY_PRESET hidden LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
target_include_directories(haylen_native_test PRIVATE "${HAYLEN_ENGINE_DIR}/include")
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(haylen_native_test PRIVATE Threads::Threads)
endif()
haylen_enable_warnings(haylen_native_test)
add_dependencies(haylen_tests haylen_native_test)
target_compile_definitions(haylen_tests PRIVATE HAYLEN_NATIVE_TEST_LIBRARY="$<TARGET_FILE:haylen_native_test>")

# Every test runs with a time limit far above the slowest one, so a test that hangs fails alone and the rest of the suite still runs.
include(GoogleTest)
gtest_discover_tests(haylen_tests
  WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
  DISCOVERY_TIMEOUT 60
  PROPERTIES TIMEOUT 120
)
