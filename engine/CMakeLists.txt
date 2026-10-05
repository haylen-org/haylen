cmake_minimum_required(VERSION 3.28...4.4)

file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" HAYLEN_VERSION)
string(STRIP "${HAYLEN_VERSION}" HAYLEN_VERSION)

project(haylen VERSION ${HAYLEN_VERSION} LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HAYLEN_ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Haylen engine source directory.")
set(HAYLEN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE INTERNAL "Haylen CMake module directory.")

option(HAYLEN_BUILD_PLAYER "Build the haylen player: the executable on desktop and web, and the library of the Android activity on Android." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_TESTS "Build the engine tests." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_BENCHMARKS "Build the engine benchmarks." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_TOOLS "Build the content tool haylen-content, which builds and publishes the protected releases of apps on desktop hosts." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_SDK "Merge the engine into the SDK that cmake --install --component haylen_sdk installs for find_package(haylen)." OFF)
option(HAYLEN_BUILD_FRAMEWORK "Merge the SDK and the player into the static library of one Haylen.xcframework slice. Apple platforms only." OFF)
option(HAYLEN_ENABLE_COVERAGE "Instrument the engine and its tests for code coverage." OFF)
option(HAYLEN_BUILD_FUZZERS "Build the fuzzer of the content formats with the tests, which needs a Clang that links libFuzzer." OFF)
set(HAYLEN_RENDER_BACKEND "AUTO" CACHE STRING "Rendering backend: AUTO, METAL, D3D11, GLCORE, GLES3 or WGPU.")
set_property(CACHE HAYLEN_RENDER_BACKEND PROPERTY STRINGS AUTO METAL D3D11 GLCORE GLES3 WGPU)
set(HAYLEN_SANITIZERS "OFF" CACHE STRING "Sanitizers of desktop builds without MSVC: OFF, ADDRESS for AddressSanitizer and UndefinedBehaviorSanitizer, or THREAD for ThreadSanitizer.")
set_property(CACHE HAYLEN_SANITIZERS PROPERTY STRINGS OFF ADDRESS THREAD)
set(HAYLEN_SOKOL_SHDC "" CACHE FILEPATH "Path to the sokol-shdc executable.")

list(APPEND CMAKE_MODULE_PATH "${HAYLEN_CMAKE_DIR}")

include(cpm)
include(haylen-platform)
include(haylen-warnings)
include(haylen-coverage)
include(haylen-shaders)
include(haylen-app)
include(haylen-dependencies)

include(haylen-embed)

set(HAYLEN_ENGINE_SOURCES
  src/2d/animation/Animation.cpp
  src/2d/animation/Animation2DLua.cpp
  src/2d/animation/Animator.cpp
  src/2d/animation/SpriteAtlas.cpp
  src/2d/graphics/Camera.cpp
  src/2d/graphics/CameraLua.cpp
  src/2d/graphics/DrawItem.cpp
  src/2d/graphics/FrameSubmitter.cpp
  src/2d/graphics/GpuInstance.cpp
  src/2d/graphics/Graphics2DLua.cpp
  src/2d/graphics/LightInstance.cpp
  src/2d/graphics/Material.cpp
  src/2d/graphics/MaterialLua.cpp
  src/2d/graphics/MaterialResource.cpp
  src/2d/graphics/NineSlice.cpp
  src/2d/graphics/NineSliceLua.cpp
  src/2d/graphics/Parallax.cpp
  src/2d/graphics/ParallaxLua.cpp
  src/2d/graphics/Renderer.cpp
  src/2d/graphics/RendererState.cpp
  src/2d/graphics/RichTextLua.cpp
  src/2d/graphics/SceneTransition.cpp
  src/2d/graphics/SpriteBatch.cpp
  src/2d/graphics/SpriteBatchLua.cpp
  src/2d/graphics/SpriteLayout.cpp
  src/2d/graphics/SpriteLua.cpp
  src/2d/graphics/StaticBatchResource.cpp
  src/2d/graphics/StaticSpriteBatch.cpp
  src/2d/graphics/TextPainter.cpp
  src/2d/graphics/VectorAtlas.cpp
  src/2d/lighting/LightFlicker.cpp
  src/2d/lighting/Light.cpp
  src/2d/lighting/LightLua.cpp
  src/2d/lighting/Lighting2DLua.cpp
  src/2d/lighting/Occluder.cpp
  src/2d/lighting/OccluderLua.cpp
  src/2d/lighting/ShadowMap.cpp
  src/2d/navigation/AgentLua.cpp
  src/2d/navigation/ConstrainedTriangulation.cpp
  src/2d/navigation/Crowd.cpp
  src/2d/navigation/DijkstraMap.cpp
  src/2d/navigation/FlowField.cpp
  src/2d/navigation/Graph.cpp
  src/2d/navigation/GraphSearch.cpp
  src/2d/navigation/Grid.cpp
  src/2d/navigation/GridSearch.cpp
  src/2d/navigation/HierarchicalPathfinder.cpp
  src/2d/navigation/NavGraphLua.cpp
  src/2d/navigation/NavGridLua.cpp
  src/2d/navigation/NavMesh.cpp
  src/2d/navigation/NavMeshLua.cpp
  src/2d/navigation/Navigation2DLua.cpp
  src/2d/navigation/SteeringAgent.cpp
  src/2d/navigation/Wanderer.cpp
  src/2d/particles/Effect.cpp
  src/2d/particles/Emitter.cpp
  src/2d/particles/EmitterConfig.cpp
  src/2d/particles/Particles2DLua.cpp
  src/2d/physics/AssemblyLua.cpp
  src/2d/physics/Body.cpp
  src/2d/physics/BodyLua.cpp
  src/2d/physics/Box2DConverter.cpp
  src/2d/physics/DebugDraw.cpp
  src/2d/physics/DestructionLua.cpp
  src/2d/physics/Explosion.cpp
  src/2d/physics/Fluid.cpp
  src/2d/physics/FluidLua.cpp
  src/2d/physics/ForceField.cpp
  src/2d/physics/ForceFieldLua.cpp
  src/2d/physics/Fracture.cpp
  src/2d/physics/Grabber.cpp
  src/2d/physics/GrabberLua.cpp
  src/2d/physics/Joint.cpp
  src/2d/physics/JointLua.cpp
  src/2d/physics/Mover.cpp
  src/2d/physics/MoverLua.cpp
  src/2d/physics/PathPredictor.cpp
  src/2d/physics/Physics2DLua.cpp
  src/2d/physics/Ragdoll.cpp
  src/2d/physics/RayBatch.cpp
  src/2d/physics/RayDebugDraw.cpp
  src/2d/physics/Raycaster.cpp
  src/2d/physics/Rope.cpp
  src/2d/physics/Shape.cpp
  src/2d/physics/ShapeLua.cpp
  src/2d/physics/StepTasks.cpp
  src/2d/physics/Terrain.cpp
  src/2d/physics/TopDownVehicle.cpp
  src/2d/physics/Vehicle.cpp
  src/2d/physics/VehicleLua.cpp
  src/2d/physics/World.cpp
  src/2d/physics/WorldLua.cpp
  src/2d/physics/WorldRaycastLua.cpp
  src/2d/procedural/Autotile.cpp
  src/2d/procedural/CellularAutomaton.cpp
  src/2d/procedural/Delaunay.cpp
  src/2d/procedural/DrunkardWalk.cpp
  src/2d/procedural/Dungeon.cpp
  src/2d/procedural/MapGeneratorsLua.cpp
  src/2d/procedural/Maze.cpp
  src/2d/procedural/Procedural2DLua.cpp
  src/2d/procedural/Region.cpp
  src/2d/procedural/Scatter.cpp
  src/2d/procedural/Voronoi.cpp
  src/2d/procedural/WaveFunctionCollapse.cpp
  src/2d/spatial/AabbTree.cpp
  src/2d/spatial/Bresenham.cpp
  src/2d/spatial/CellGrid.cpp
  src/2d/spatial/ConnectedComponents.cpp
  src/2d/spatial/EntryBounds.cpp
  src/2d/spatial/FloodFill.cpp
  src/2d/spatial/GridLua.cpp
  src/2d/spatial/GridRay.cpp
  src/2d/spatial/HashGrid.cpp
  src/2d/spatial/KdTree.cpp
  src/2d/spatial/QuadTree.cpp
  src/2d/spatial/Spatial2DLua.cpp
  src/2d/spatial/SpatialIndexLua.cpp
  src/2d/spatial/UnionFind.cpp
  src/2d/spatial/VisibilityPolygon.cpp
  src/2d/tiled/Layer.cpp
  src/2d/tiled/Map.cpp
  src/2d/tiled/MapParser.cpp
  src/2d/tiled/MapQuery.cpp
  src/2d/tiled/MapQueryLua.cpp
  src/2d/tiled/MapRenderer.cpp
  src/2d/tiled/Object.cpp
  src/2d/tiled/ObjectFactories.cpp
  src/2d/tiled/Properties.cpp
  src/2d/tiled/TileCollision.cpp
  src/2d/tiled/TiledLua.cpp
  src/2d/tiled/Tileset.cpp
  src/2d/tiled/World.cpp
  src/ai/AiLua.cpp
  src/ai/BehaviorTree.cpp
  src/ai/BehaviorTreeLua.cpp
  src/ai/InfluenceMap.cpp
  src/ai/InfluenceMapLua.cpp
  src/ai/ResponseCurve.cpp
  src/ai/StateMachine.cpp
  src/ai/UtilityLua.cpp
  src/ai/UtilitySelector.cpp
  src/assets/AssetsLua.cpp
  src/assets/Manager.cpp
  src/audio/AudioLua.cpp
  src/audio/Delay.cpp
  src/audio/Device.cpp
  src/audio/Effect.cpp
  src/audio/EffectChain.cpp
  src/audio/EffectLua.cpp
  src/audio/EffectNode.cpp
  src/audio/Filter.cpp
  src/audio/Freeverb.cpp
  src/audio/MemoryDecoder.cpp
  src/audio/Mixer.cpp
  src/audio/MixerState.cpp
  src/audio/Reverb.cpp
  src/audio/Session.cpp
  src/audio/Sound.cpp
  src/audio/StreamSource.cpp
  src/content/Bootstrap.cpp
  src/content/Compatibility.cpp
  src/content/ContentBuilder.cpp
  src/content/crypto/Aead.cpp
  src/content/crypto/ContentKey.cpp
  src/content/crypto/Digest.cpp
  src/content/crypto/Hasher.cpp
  src/content/crypto/KeyRing.cpp
  src/content/crypto/SigningKey.cpp
  src/content/crypto/VerifyingKey.cpp
  src/content/EmbeddedKeyProvider.cpp
  src/content/Error.cpp
  src/content/format/BinaryReader.cpp
  src/content/format/BinaryWriter.cpp
  src/content/format/Catalog.cpp
  src/content/format/CatalogWriter.cpp
  src/content/format/ChannelDescriptor.cpp
  src/content/format/Chunker.cpp
  src/content/format/ChunkRecord.cpp
  src/content/format/Compression.cpp
  src/content/format/Manifest.cpp
  src/content/format/ShardHeader.cpp
  src/content/format/ShardIndex.cpp
  src/content/format/ShardReference.cpp
  src/content/HpakFileReader.cpp
  src/content/HpakPackage.cpp
  src/content/LuaCompiler.cpp
  src/content/RecordCache.cpp
  src/content/ReleaseBuilder.cpp
  src/content/ReleasePackage.cpp
  src/content/ShardReader.cpp
  src/content/ShardSet.cpp
  src/content/ShardWriter.cpp
  src/core/AppConfig.cpp
  src/core/Application.cpp
  src/core/ClassLua.cpp
  src/core/CollectionsLua.cpp
  src/core/ConnectionScope.cpp
  src/core/CoreLua.cpp
  src/core/Engine.cpp
  src/core/ErrorScreen.cpp
  src/core/EventBus.cpp
  src/core/EventsLua.cpp
  src/core/FloatBuffer.cpp
  src/core/FloatBufferLua.cpp
  src/core/FrameClock.cpp
  src/core/FrameQueue.cpp
  src/core/JobSystem.cpp
  src/core/JsonBytes.cpp
  src/core/JsonNumber.cpp
  src/core/JsonValidator.cpp
  src/core/LoadingView.cpp
  src/core/Log.cpp
  src/core/PropertyTrack.cpp
  src/core/PropertyTween.cpp
  src/core/Scene.cpp
  src/core/SceneLoad.cpp
  src/core/SceneLua.cpp
  src/core/SceneManager.cpp
  src/core/ScriptedTrack.cpp
  src/core/SignalLua.cpp
  src/core/Timeline.cpp
  src/core/TimerLua.cpp
  src/core/TimerScheduler.cpp
  src/core/Tween.cpp
  src/core/TweenLua.cpp
  src/core/TweenManager.cpp
  src/core/TweenMotion.cpp
  src/core/TweenProperty.cpp
  src/core/TweenValue.cpp
  src/core/Utf8.cpp
  src/debug/DebugLua.cpp
  src/debug/Monitor.cpp
  src/debug/ObjectCounter.cpp
  src/debug/OverlayWindow.cpp
  src/debug/Profiler.cpp
  src/debug/Stats.cpp
  src/debug/StatsDisplay.cpp
  src/graphics/BlendMode.cpp
  src/graphics/Device.cpp
  src/graphics/DeviceState.cpp
  src/graphics/FontLua.cpp
  src/graphics/Gpu.cpp
  src/graphics/GraphicsLua.cpp
  src/graphics/Image.cpp
  src/graphics/RenderTarget.cpp
  src/graphics/RenderTargetResource.cpp
  src/graphics/ResourceGraveyard.cpp
  src/graphics/Shader.cpp
  src/graphics/ShaderResource.cpp
  src/graphics/Texture.cpp
  src/graphics/TextureResource.cpp
  src/graphics/VectorImage.cpp
  src/graphics/VectorImageResource.cpp
  src/graphics/Viewport.cpp
  src/input/ActionMap.cpp
  src/input/Controls.cpp
  src/input/Gesture.cpp
  src/input/GestureRecognizer.cpp
  src/input/Input.cpp
  src/input/InputLua.cpp
  src/input/PointerEmulation.cpp
  src/input/VirtualInput.cpp
  src/io/CompositePackage.cpp
  src/io/DirectoryPackage.cpp
  src/io/FileReader.cpp
  src/io/MemoryPackage.cpp
  src/io/MemoryReader.cpp
  src/io/Package.cpp
  src/io/PackageReader.cpp
  src/io/PackageWatcher.cpp
  src/io/Path.cpp
  src/io/ZipArchive.cpp
  src/io/ZipPackage.cpp
  src/io/ZipReader.cpp
  src/localization/Catalog.cpp
  src/localization/LocalizationLua.cpp
  src/lua/Application.cpp
  src/lua/Autoloads.cpp
  src/lua/Binding.cpp
  src/lua/Environment.cpp
  src/lua/Error.cpp
  src/lua/JobsLua.cpp
  src/lua/JsonConverter.cpp
  src/lua/NativeProperty.cpp
  src/lua/Owners.cpp
  src/lua/Promise.cpp
  src/lua/Reference.cpp
  src/lua/Runtime.cpp
  src/lua/ScriptedLoadingView.cpp
  src/lua/ScriptedScene.cpp
  src/lua/ScriptedTransition.cpp
  src/lua/Table.cpp
  src/lua/Task.cpp
  src/lua/TypeConverter.cpp
  src/lua/Userdata.cpp
  src/lua/WeakReference.cpp
  src/math/ChoiceLua.cpp
  src/math/Color.cpp
  src/math/Easing.cpp
  src/math/EasingCurve.cpp
  src/math/Geometry.cpp
  src/math/MarchingSquares.cpp
  src/math/MathLua.cpp
  src/math/Noise2D.cpp
  src/math/PoissonDisk.cpp
  src/math/Polygon.cpp
  src/math/PolygonLua.cpp
  src/math/Random.cpp
  src/math/Raycast.cpp
  src/math/RaycastLua.cpp
  src/math/ShuffleBag.cpp
  src/math/Spline.cpp
  src/math/SplineLua.cpp
  src/math/Spring.cpp
  src/math/SpringLua.cpp
  src/math/Transform2D.cpp
  src/math/WeightedChoice.cpp
  src/net/NetLua.cpp
  src/net/WebSocket.cpp
  src/platform/AppPlugin.cpp
  src/platform/AudioStream.cpp
  src/platform/Battery.cpp
  src/platform/Bridge.cpp
  src/platform/BridgeRelay.cpp
  src/platform/DialogRelay.cpp
  src/platform/DialogRequest.cpp
  src/platform/DialogResult.cpp
  src/platform/Dialogs.cpp
  src/platform/DialogsLua.cpp
  src/platform/native/NativeApi.cpp
  src/platform/native/NativeCallback.cpp
  src/platform/native/NativeCallbacks.cpp
  src/platform/native/NativeLibraries.cpp
  src/platform/native/NativeLua.cpp
  src/platform/native/NativeSignature.cpp
  src/platform/NativeViews.cpp
  src/platform/PlatformLua.cpp
  src/platform/PluginStreams.cpp
  src/platform/SafeAreaSimulation.cpp
  src/platform/ScreenRelay.cpp
  src/platform/Screens.cpp
  src/platform/StreamsLua.cpp
  src/platform/System.cpp
  src/platform/SystemInfo.cpp
  src/platform/SystemLua.cpp
  src/platform/SystemState.cpp
  src/platform/VideoStream.cpp
  src/platform/Window.cpp
  src/platform/WindowLua.cpp
  src/platform/WindowPlacement.cpp
  src/plugins/Animation2DPlugin.cpp
  src/plugins/AssetsPlugin.cpp
  src/plugins/AudioPlugin.cpp
  src/plugins/BuiltInPlugins.cpp
  src/plugins/CorePlugin.cpp
  src/plugins/DebugPlugin.cpp
  src/plugins/Graphics2DPlugin.cpp
  src/plugins/HotReloadPlugin.cpp
  src/plugins/InputPlugin.cpp
  src/plugins/JobsPlugin.cpp
  src/plugins/LocalizationPlugin.cpp
  src/plugins/NativePlugin.cpp
  src/plugins/Navigation2DPlugin.cpp
  src/plugins/NetPlugin.cpp
  src/plugins/Particles2DPlugin.cpp
  src/plugins/Physics2DPlugin.cpp
  src/plugins/PlatformPlugin.cpp
  src/plugins/Procedural2DPlugin.cpp
  src/plugins/Plugin.cpp
  src/plugins/PluginRegistry.cpp
  src/plugins/Spatial2DPlugin.cpp
  src/plugins/StoragePlugin.cpp
  src/plugins/TextPlugin.cpp
  src/plugins/TiledPlugin.cpp
  src/plugins/UiPlugin.cpp
  src/storage/Preferences.cpp
  src/storage/PreferencesLua.cpp
  src/storage/SaveSlots.cpp
  src/storage/StorageLua.cpp
  src/storage/UserStorage.cpp
  src/text/BidiParagraph.cpp
  src/text/BitmapFont.cpp
  src/text/DistanceField.cpp
  src/text/Effect.cpp
  src/text/Font.cpp
  src/text/FontFamily.cpp
  src/text/LayoutBuilder.cpp
  src/text/LayoutCache.cpp
  src/text/MarkupParser.cpp
  src/text/PhraseBreaker.cpp
  src/text/RichText.cpp
  src/text/RichTextRegistry.cpp
  src/text/Segmenter.cpp
  src/text/Style.cpp
  src/text/TrueTypeFont.cpp
  src/ui/Anchor.cpp
  src/ui/Backend.cpp
  src/ui/Component.cpp
  src/ui/ComponentRegistry.cpp
  src/ui/components/BuiltInComponents.cpp
  src/ui/components/buttons/Button.cpp
  src/ui/components/buttons/ButtonBase.cpp
  src/ui/components/buttons/Chip.cpp
  src/ui/components/buttons/ImageButton.cpp
  src/ui/components/buttons/MenuButton.cpp
  src/ui/components/buttons/Popover.cpp
  src/ui/components/ChoiceItem.cpp
  src/ui/components/DragAndDrop.cpp
  src/ui/components/choices/Checkbox.cpp
  src/ui/components/choices/Combo.cpp
  src/ui/components/choices/RadioGroup.cpp
  src/ui/components/choices/SegmentedControl.cpp
  src/ui/components/choices/Toggle.cpp
  src/ui/components/collections/List.cpp
  src/ui/components/collections/ListRow.cpp
  src/ui/components/collections/SlotGrid.cpp
  src/ui/components/collections/Table.cpp
  src/ui/components/collections/Tree.cpp
  src/ui/components/containers/Accordion.cpp
  src/ui/components/containers/Carousel.cpp
  src/ui/components/containers/Divider.cpp
  src/ui/components/containers/FormField.cpp
  src/ui/components/containers/Grid.cpp
  src/ui/components/containers/Linear.cpp
  src/ui/components/containers/SafeArea.cpp
  src/ui/components/containers/Scroll.cpp
  src/ui/components/containers/Spacer.cpp
  src/ui/components/containers/Splitter.cpp
  src/ui/components/containers/Stack.cpp
  src/ui/components/containers/Surfaced.cpp
  src/ui/components/containers/Tabs.cpp
  src/ui/components/indicators/Avatar.cpp
  src/ui/components/indicators/Badge.cpp
  src/ui/components/indicators/BusyIndicator.cpp
  src/ui/components/indicators/CircularProgress.cpp
  src/ui/components/indicators/Icon.cpp
  src/ui/components/indicators/Image.cpp
  src/ui/components/indicators/Progress.cpp
  src/ui/components/indicators/StatusIndicator.cpp
  src/ui/components/inputs/ColorField.cpp
  src/ui/components/inputs/FilterField.cpp
  src/ui/components/inputs/KeyCapture.cpp
  src/ui/components/inputs/NumberField.cpp
  src/ui/components/inputs/PlayArea.cpp
  src/ui/components/inputs/RangeSlider.cpp
  src/ui/components/inputs/SecretField.cpp
  src/ui/components/inputs/Slider.cpp
  src/ui/components/inputs/Stepper.cpp
  src/ui/components/inputs/TextArea.cpp
  src/ui/components/inputs/TextEntry.cpp
  src/ui/components/inputs/TextField.cpp
  src/ui/components/overlays/ContextMenu.cpp
  src/ui/components/overlays/Dialog.cpp
  src/ui/components/overlays/Toast.cpp
  src/ui/components/overlays/Window.cpp
  src/ui/components/PopupList.cpp
  src/ui/components/settings/SettingsActions.cpp
  src/ui/components/settings/SettingsForm.cpp
  src/ui/components/settings/SettingsRow.cpp
  src/ui/components/text/Alert.cpp
  src/ui/components/text/EmptyState.cpp
  src/ui/components/text/Label.cpp
  src/ui/components/text/PageHeader.cpp
  src/ui/components/text/RichText.cpp
  src/ui/components/text/SectionTitle.cpp
  src/ui/components/touch/PointerTracker.cpp
  src/ui/components/touch/TouchButton.cpp
  src/ui/components/touch/TouchStick.cpp
  src/ui/Context.cpp
  src/ui/FocusNavigator.cpp
  src/ui/FocusSearch.cpp
  src/ui/Gui.cpp
  src/ui/ImGuiConverter.cpp
  src/ui/ImGuiLua.cpp
  src/ui/MountLink.cpp
  src/ui/NavigationInput.cpp
  src/ui/PropertyReader.cpp
  src/ui/Style.cpp
  src/ui/Surfaces.cpp
  src/ui/TextEditor.cpp
  src/ui/TextFieldLayout.cpp
  src/ui/TextSession.cpp
  src/ui/TextValue.cpp
  src/ui/Theme.cpp
  src/ui/ToastStack.cpp
  src/ui/TransformLua.cpp
  src/ui/Typography.cpp
  src/ui/UiLua.cpp
  src/ui/Widgets.cpp
)

# The graphics device names its GPU and compiles shader sources ahead of their programs through Metal on Apple platforms, and names its GPU through the backend the build chose elsewhere.
if(APPLE)
  list(APPEND HAYLEN_ENGINE_SOURCES src/graphics/GpuAdapter.mm src/graphics/ShaderPrecompiler.mm)
  set_source_files_properties(src/graphics/GpuAdapter.mm src/graphics/ShaderPrecompiler.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
else()
  list(APPEND HAYLEN_ENGINE_SOURCES src/graphics/GpuAdapter.cpp src/graphics/ShaderPrecompiler.cpp)
  set_source_files_properties(src/graphics/GpuAdapter.cpp PROPERTIES COMPILE_DEFINITIONS SOKOL_${HAYLEN_BACKEND})
endif()

# The browser build talks WebSocket through the page and plays audio through it, while native builds use Poco with the TLS stack Varn already links.
if(EMSCRIPTEN)
  list(APPEND HAYLEN_ENGINE_SOURCES src/platform/web/BrowserAudioOutput.cpp src/platform/web/BrowserWebSocket.cpp)
else()
  list(APPEND HAYLEN_ENGINE_SOURCES src/net/PocoWebSocket.cpp)
endif()

set(HAYLEN_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file("${CMAKE_CURRENT_LIST_DIR}/include/haylen/core/Version.hpp.in" "${HAYLEN_GENERATED_INCLUDE_DIR}/haylen/core/Version.hpp" @ONLY)

add_library(haylen_engine STATIC ${HAYLEN_ENGINE_SOURCES})
add_library(haylen::engine ALIAS haylen_engine)

# Public headers use C++20, so projects that link the engine compile with it too.
target_compile_features(haylen_engine PUBLIC cxx_std_20)
# Apps live in folders of their own, so the exception support of the browser build travels with the engine target instead of the engine folder.
if(EMSCRIPTEN)
  target_compile_options(haylen_engine INTERFACE -fexceptions)
  target_link_options(haylen_engine INTERFACE -fexceptions)
endif()
target_include_directories(haylen_engine
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include" "${HAYLEN_GENERATED_INCLUDE_DIR}"
  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src"
)

target_link_libraries(haylen_engine
  PUBLIC nlohmann_json::nlohmann_json varn_core haylen_imgui
  PRIVATE haylen_sokol_headers box2d::box2d Clipper2 msdfgen::msdfgen-core haylen_harfbuzz haylen_sheenbidi haylen_unibreak haylen_miniaudio haylen_stb haylen_nanosvg haylen_fast_float haylen_monocypher libzip::zip ZLIB::ZLIB libzstd_static
)

if(WIN32)
  target_link_libraries(haylen_engine PRIVATE Poco::NetSSLWin)
elseif(NOT EMSCRIPTEN)
  target_link_libraries(haylen_engine PRIVATE Poco::NetSSL)
endif()

# Native libraries load through the dynamic linker, which Linux keeps in its own library.
target_link_libraries(haylen_engine PRIVATE ${CMAKE_DL_LIBS})

# The name of the GPU and the shaders compiled ahead come from Metal through Objective-C on Apple platforms, and the name comes from the OpenGL library where the build renders with OpenGL.
if(APPLE)
  target_link_libraries(haylen_engine PRIVATE "-framework Foundation")
elseif(HAYLEN_BACKEND STREQUAL "GLES3" AND ANDROID)
  target_link_libraries(haylen_engine PRIVATE GLESv3)
elseif(HAYLEN_BACKEND STREQUAL "GLCORE" AND WIN32)
  target_link_libraries(haylen_engine PRIVATE opengl32)
elseif(HAYLEN_BACKEND STREQUAL "GLCORE")
  find_package(OpenGL REQUIRED)
  target_link_libraries(haylen_engine PRIVATE OpenGL::GL)
endif()

# Programs that draw into lit canvases also compile with HAYLEN_LIT, which writes the images the light pass reads.
set(shader_dir "${CMAKE_CURRENT_LIST_DIR}/shaders")
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/sprite.glsl" MODULE sprite)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/sprite.glsl" MODULE sprite_lit DEFINES HAYLEN_LIT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/text.glsl" MODULE text DEFINES HAYLEN_TEXT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/text.glsl" MODULE text_lit DEFINES HAYLEN_TEXT HAYLEN_LIT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/mesh.glsl" MODULE mesh DEFINES HAYLEN_MESH)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/mesh.glsl" MODULE mesh_lit DEFINES HAYLEN_MESH HAYLEN_LIT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/blend.glsl" MODULE blend)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/blend.glsl" MODULE blend_lit DEFINES HAYLEN_LIT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/metaball.glsl" MODULE metaball)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/metaball.glsl" MODULE metaball_lit DEFINES HAYLEN_LIT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/recolor.glsl" MODULE recolor)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/recolor.glsl" MODULE recolor_lit DEFINES HAYLEN_LIT)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/composite.glsl" MODULE composite)
haylen_compile_shader(haylen_engine SOURCE "${shader_dir}/light.glsl" MODULE light)
haylen_embed_file(haylen_engine "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" getDefaultFont)
haylen_embed_file(haylen_engine "${budoux_SOURCE_DIR}/budoux/models/th.json" getThaiPhraseModel)

haylen_enable_warnings(haylen_engine)
haylen_enable_coverage(haylen_engine)

# Portable pieces of the runtime, which the tests link without an app entry point. The readers of the platform folders compile everywhere, so every host tests them.
add_library(haylen_platform STATIC
  src/platform/GamepadSlots.cpp
  src/platform/KeyboardTranslator.cpp
  src/platform/PluginLoadOrder.cpp
  src/platform/android/AndroidBatteryStatus.cpp
  src/platform/android/AndroidDeviceInfo.cpp
  src/platform/android/AndroidDialogJson.cpp
  src/platform/android/AndroidGamepadStates.cpp
  src/platform/android/AndroidGlesVersion.cpp
  src/platform/linux/LinuxFilePattern.cpp
  src/platform/linux/LinuxSystemReader.cpp
  src/platform/sokol/MemoryWarning.cpp
  src/platform/sokol/SokolEvents.cpp
  src/platform/web/WebDialogJson.cpp
  src/platform/windows/WindowsPowerStatus.cpp
)
add_library(haylen::platform ALIAS haylen_platform)
# Apps see only the public API, so the private headers and Sokol stay private to the engine libraries.
target_include_directories(haylen_platform PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(haylen_platform PUBLIC haylen_engine PRIVATE haylen_sokol_headers)
haylen_enable_warnings(haylen_platform)
haylen_enable_coverage(haylen_platform)

# The runtime turns the engine into an app with sokol_app as the entry point. It is an object library so entry points always reach the final binary.
set(HAYLEN_RUNTIME_SOURCES
  src/platform/sokol/SokolHost.cpp
  src/platform/sokol/SokolRuntime.cpp
)
if(APPLE)
  set(HAYLEN_APPLE_SOURCES
    src/platform/apple/AppleBattery.mm
    src/platform/apple/AppleBridge.mm
    src/platform/apple/AppleDesktop.mm
    src/platform/apple/AppleDialogs.mm
    src/platform/apple/AppleGamepads.mm
    src/platform/apple/AppleNetwork.mm
    src/platform/apple/AppleNotifications.mm
    src/platform/apple/AppleOrientation.mm
    src/platform/apple/ApplePlugins.mm
    src/platform/apple/ApplePresenter.mm
    src/platform/apple/AppleRemote.mm
    src/platform/apple/AppleRuntime.mm
    src/platform/apple/AppleScreens.mm
    src/platform/apple/AppleServices.mm
    src/platform/apple/AppleSystem.mm
    src/platform/apple/AppleTextInput.mm
    src/platform/apple/AppleTheme.mm
    src/platform/apple/CatalystInput.mm
    src/platform/apple/CatalystWindow.mm
    src/platform/apple/HaylenAppDelegate.mm
    src/platform/apple/HaylenAudioStream.mm
    src/platform/apple/HaylenBridge.mm
    src/platform/apple/HaylenDialog.mm
    src/platform/apple/HaylenFieldEditor.mm
    src/platform/apple/HaylenOverlay.mm
    src/platform/apple/HaylenOverlayItem.mm
    src/platform/apple/HaylenOverlayLayer.mm
    src/platform/apple/HaylenPlacement.mm
    src/platform/apple/HaylenPluginContext.mm
    src/platform/apple/HaylenPointer.mm
    src/platform/apple/HaylenRequirement.mm
    src/platform/apple/HaylenRequirements.mm
    src/platform/apple/HaylenSceneDelegate.mm
    src/platform/apple/HaylenScreen.mm
    src/platform/apple/HaylenScreenController.mm
    src/platform/apple/HaylenScreenSceneDelegate.mm
    src/platform/apple/HaylenTextEditor.mm
    src/platform/apple/HaylenTextField.mm
    src/platform/apple/HaylenTextView.mm
    src/platform/apple/HaylenThemeView.mm
    src/platform/apple/HaylenVideoStream.mm
  )
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sokol/SokolImpl.m ${HAYLEN_APPLE_SOURCES})
  set_source_files_properties(${HAYLEN_APPLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(WIN32)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sokol/SokolImpl.c src/platform/windows/WindowsDesktop.cpp src/platform/windows/WindowsDialogs.cpp src/platform/windows/WindowsServices.cpp src/platform/windows/WindowsSystem.cpp src/platform/windows/WindowsText.cpp src/platform/windows/WindowsTextInput.cpp)
elseif(ANDROID)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sokol/SokolImpl.c src/platform/android/AndroidActivity.cpp src/platform/android/AndroidAssetPackage.cpp src/platform/android/AndroidAssetReader.cpp src/platform/android/AndroidGamepads.cpp src/platform/android/AndroidKeys.cpp src/platform/android/AndroidServices.cpp src/platform/android/AndroidStreams.cpp src/platform/android/AndroidTextInput.cpp src/platform/android/JavaBridge.cpp)
elseif(EMSCRIPTEN)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sokol/SokolImpl.c src/platform/web/WebPage.cpp src/platform/web/WebServices.cpp src/platform/web/WebTextInput.cpp)
else()
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sokol/SokolImpl.c src/platform/linux/LinuxDesktop.cpp src/platform/linux/LinuxDialogs.cpp src/platform/linux/LinuxGamepads.cpp src/platform/linux/LinuxGlib.cpp src/platform/linux/LinuxServices.cpp src/platform/linux/LinuxSystem.cpp src/platform/linux/LinuxTheme.cpp)
endif()

add_library(haylen_runtime OBJECT ${HAYLEN_RUNTIME_SOURCES})
add_library(haylen::runtime ALIAS haylen_runtime)
target_compile_definitions(haylen_runtime PRIVATE SOKOL_${HAYLEN_BACKEND})
target_include_directories(haylen_runtime PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(haylen_runtime PUBLIC haylen_platform PRIVATE haylen_sokol_headers)
# The web host gives the mixer the audio output of the page, a backend of miniaudio.
if(EMSCRIPTEN)
  target_link_libraries(haylen_runtime PRIVATE haylen_miniaudio)
endif()
# The Android host is GameActivity, whose static library holds the native side of the activity.
if(ANDROID)
  target_link_libraries(haylen_runtime PRIVATE haylen_game_activity)
endif()
haylen_enable_warnings(haylen_runtime)
haylen_link_runtime_platform(haylen_runtime PUBLIC)

if(HAYLEN_BUILD_SDK)
  include(haylen-install)
endif()

if(HAYLEN_DESKTOP)
  add_library(haylen_headless STATIC
    src/platform/headless/HeadlessHost.cpp
    src/platform/headless/SokolDummy.c
  )
  add_library(haylen::headless ALIAS haylen_headless)
  target_include_directories(haylen_headless PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen_headless PUBLIC haylen_platform PRIVATE haylen_sokol_headers)
  haylen_enable_warnings(haylen_headless)
endif()

# The player runs any package: a folder or zip from the command line on desktop, or what the page loads through Module.haylen on the web.
# On Android it is the library that HaylenActivity of the haylen Android library loads, and it plays the package in the APK assets.
if(HAYLEN_BUILD_PLAYER AND (HAYLEN_DESKTOP OR EMSCRIPTEN))
  add_executable(haylen src/platform/sokol/LuaPlayer.cpp)
  target_link_libraries(haylen PRIVATE haylen::runtime)
  # The generator expression keeps multi-config generators such as Xcode and Visual Studio from adding a folder per configuration.
  set_target_properties(haylen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/haylen/$<0:>")
  if(APPLE)
    target_sources(haylen PRIVATE src/platform/apple/AppleMain.cpp)
  elseif(WIN32)
    set_target_properties(haylen PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(haylen PRIVATE platform/windows/haylen.manifest)
  elseif(EMSCRIPTEN)
    haylen_setup_web_page(haylen "")
  else()
    # Linux apps keep their native libraries in lib next to the player, where the libraries also find the ones they depend on.
    set_target_properties(haylen PROPERTIES BUILD_RPATH "$ORIGIN;$ORIGIN/lib")
    target_link_options(haylen PRIVATE "LINKER:--enable-new-dtags")
  endif()
elseif(HAYLEN_BUILD_PLAYER AND ANDROID)
  add_library(haylen SHARED src/platform/sokol/LuaPlayer.cpp)
  target_link_libraries(haylen PRIVATE haylen::runtime)
  # The activity loads the library by its exact name, while Poco forces a Debug postfix into the cache for every library after it.
  set_target_properties(haylen PROPERTIES DEBUG_POSTFIX "")
endif()

# The headless player runs any package without a window, GPU or audio device on the host of the engine tests, which suits automatic runs of apps such as the test project in continuous integration.
if(HAYLEN_BUILD_PLAYER AND HAYLEN_DESKTOP)
  add_executable(haylen-headless src/platform/headless/HeadlessPlayer.cpp)
  target_include_directories(haylen-headless PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-headless PRIVATE haylen::engine haylen::headless haylen_sokol_headers)
  set_target_properties(haylen-headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-headless)
endif()

# Haylen.xcframework holds one static library per Apple platform with the engine, its dependencies, the runtime and the player merged.
if(HAYLEN_BUILD_FRAMEWORK)
  include(haylen-framework)
endif()

# The sprite benchmark runs on the real GPU of the desktop, so it is an app like any other.
if(HAYLEN_BUILD_BENCHMARKS AND HAYLEN_DESKTOP)
  haylen_add_app(haylen-sprite-benchmark CPP
    SOURCES bench/SpriteBenchmark.cpp
    PACKAGE "${CMAKE_CURRENT_LIST_DIR}/bench/sprite-benchmark"
  )

  # The procedural benchmark times the heavy generation, geometry and destruction algorithms on the CPU, so it is a plain executable on the headless host.
  add_executable(haylen-procedural-benchmark bench/ProceduralBenchmark.cpp)
  target_link_libraries(haylen-procedural-benchmark PRIVATE haylen::headless)
  set_target_properties(haylen-procedural-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-procedural-benchmark)

  # The algorithm benchmark times path finding, crowds, spatial queries and ray casts on the CPU, so it is a plain executable too. The physics world links the debug drawing of the renderer, whose GPU calls the dummy backend of the headless host satisfies.
  add_executable(haylen-algorithm-benchmark bench/AlgorithmBenchmark.cpp)
  target_link_libraries(haylen-algorithm-benchmark PRIVATE haylen::engine haylen::headless)
  set_target_properties(haylen-algorithm-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-algorithm-benchmark)

  # The physics benchmark times the steps, fluids, queries, transform sync, debug drawing and Lua events of physics worlds on the CPU, inside an engine on the headless host.
  add_executable(haylen-physics-benchmark bench/PhysicsBenchmark.cpp)
  target_include_directories(haylen-physics-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-physics-benchmark PRIVATE haylen::engine haylen::headless haylen_sokol_headers)
  set_target_properties(haylen-physics-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-physics-benchmark)

  # The Lua benchmark runs the bunnymark package in bench/lua-benchmark on the headless host, which times what Lua costs to update and draw many sprites through tables and through float buffers.
  add_executable(haylen-lua-benchmark bench/LuaBenchmark.cpp)
  target_include_directories(haylen-lua-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-lua-benchmark PRIVATE haylen::engine haylen::headless haylen_sokol_headers)
  set_target_properties(haylen-lua-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-lua-benchmark)
endif()

if((HAYLEN_BUILD_TOOLS OR HAYLEN_BUILD_TESTS) AND HAYLEN_DESKTOP)
  add_subdirectory(tools/content)
endif()

if(HAYLEN_BUILD_TESTS AND HAYLEN_DESKTOP)
  enable_testing()
  add_subdirectory(tests)
endif()
