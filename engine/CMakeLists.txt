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
# The engine and its dependencies use no C++ modules, so their sources compile without the module scan, which would preprocess each of them once more.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
# MSVC writes the debug information into each object instead of one PDB that every compile of a target shares, so parallel compiles never wait for that file and compiler caches keep the objects.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

set(HAYLEN_ENGINE_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE INTERNAL "Haylen engine source directory.")
set(HAYLEN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE INTERNAL "Haylen CMake module directory.")

option(HAYLEN_BUILD_PLAYER "Build the haylen player: the executable on desktop and web, and the library of the Android activity on Android." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_TESTS "Build the engine tests." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_BENCHMARKS "Build the engine benchmarks." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_TOOLS "Build the content tool haylen-content, which builds and publishes the protected releases of apps on desktop hosts." ${PROJECT_IS_TOP_LEVEL})
option(HAYLEN_BUILD_SDK "Merge the engine into the SDK that cmake --install --component haylen-sdk installs for find_package(haylen)." OFF)
option(HAYLEN_BUILD_FRAMEWORK "Merge the SDK into the static libraries of one slice of Haylen.xcframework and HaylenLua.xcframework. Apple platforms only." OFF)
option(HAYLEN_ENABLE_COVERAGE "Instrument the engine and its tests for code coverage." OFF)
option(HAYLEN_BUILD_FUZZERS "Build the fuzzer of the content formats with the tests, which needs a Clang that links libFuzzer." OFF)
set(HAYLEN_SANITIZERS "OFF" CACHE STRING "Sanitizers of builds for this machine without MSVC, a desktop or a port that builds with the compiler of this machine: OFF, ADDRESS for AddressSanitizer and UndefinedBehaviorSanitizer, or THREAD for ThreadSanitizer.")
set_property(CACHE HAYLEN_SANITIZERS PROPERTY STRINGS OFF ADDRESS THREAD)
set(HAYLEN_SHADERC "" CACHE FILEPATH "Path to a build of the Haylen shader compiler haylen-shaderc, or empty for the pinned compiler that the build downloads or builds.")
if(DEFINED ENV{HAYLEN_DEBUG_INFO})
  set(haylen_debug_info "$ENV{HAYLEN_DEBUG_INFO}")
else()
  set(haylen_debug_info "FULL")
endif()
set(HAYLEN_DEBUG_INFO "${haylen_debug_info}" CACHE STRING "The debug information of the Debug and RelWithDebInfo configurations: FULL, or LINES for the file and line of each instruction alone. The variable HAYLEN_DEBUG_INFO of the environment names it for every build tree.")
set_property(CACHE HAYLEN_DEBUG_INFO PROPERTY STRINGS FULL LINES)
if(DEFINED ENV{HAYLEN_WARNINGS_AS_ERRORS})
  set(haylen_warnings_as_errors "$ENV{HAYLEN_WARNINGS_AS_ERRORS}")
else()
  set(haylen_warnings_as_errors OFF)
endif()
option(HAYLEN_WARNINGS_AS_ERRORS "Fail the build of the targets of the engine on any compiler warning. The variable HAYLEN_WARNINGS_AS_ERRORS of the environment names it for every build tree." ${haylen_warnings_as_errors})

# The file and line of each instruction are what stack traces and sanitizer reports read, in objects about a third of the size of those that describe the code fully for a debugger, as continuous integration builds them for its compiler caches. MSVC has no format with lines alone, so its objects carry no debug information then.
if(HAYLEN_DEBUG_INFO STREQUAL "LINES")
  if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options("$<$<CONFIG:Debug,RelWithDebInfo>:-g1>")
  else()
    add_compile_options("$<$<CONFIG:Debug,RelWithDebInfo>:-gline-tables-only>")
  endif()
elseif(NOT HAYLEN_DEBUG_INFO STREQUAL "FULL")
  message(FATAL_ERROR "The option \"HAYLEN_DEBUG_INFO\" is \"FULL\" or \"LINES\", not \"${HAYLEN_DEBUG_INFO}\".")
endif()

list(APPEND CMAKE_MODULE_PATH "${HAYLEN_CMAKE_DIR}")

include(cpm)
include(haylen-port)
include(haylen-platform)
include(haylen-gpu)
include(haylen-release)
include(haylen-warnings)
include(haylen-coverage)
include(haylen-shaders)
include(haylen-app)
include(haylen-dependencies)

include(haylen-embed)

set(HAYLEN_ENGINE_SOURCES
  src/2d/animation/AnimatedBatch.cpp
  src/2d/animation/Animation.cpp
  src/2d/animation/AtlasBuilder.cpp
  src/2d/animation/Animator.cpp
  src/2d/animation/Chain.cpp
  src/2d/animation/Skeleton.cpp
  src/2d/animation/SkeletonAnimation.cpp
  src/2d/animation/SkeletonData.cpp
  src/2d/animation/SpriteAtlas.cpp
  src/2d/animation/StateGraph.cpp
  src/2d/animation/StateGraphPlayer.cpp
  src/2d/graphics/Camera.cpp
  src/2d/graphics/CameraRig.cpp
  src/2d/graphics/DrawItem.cpp
  src/2d/graphics/FrameSubmitter.cpp
  src/2d/graphics/GpuInstance.cpp
  src/2d/graphics/LightInstance.cpp
  src/2d/graphics/Material.cpp
  src/2d/graphics/MaterialResource.cpp
  src/2d/graphics/Mesh.cpp
  src/2d/graphics/NineSlice.cpp
  src/2d/graphics/Parallax.cpp
  src/2d/graphics/PixelCanvas.cpp
  src/2d/graphics/MeshFringe.cpp
  src/2d/graphics/Path.cpp
  src/2d/graphics/PathTessellation.cpp
  src/2d/graphics/PolygonMesh.cpp
  src/2d/graphics/PostProcess.cpp
  src/2d/graphics/Renderer.cpp
  src/2d/graphics/RendererState.cpp
  src/2d/graphics/SceneTransition.cpp
  src/2d/graphics/Shape.cpp
  src/2d/graphics/SpriteBatch.cpp
  src/2d/graphics/SpriteEffect.cpp
  src/2d/graphics/SpriteLayout.cpp
  src/2d/graphics/StaticBatchResource.cpp
  src/2d/graphics/StaticSpriteBatch.cpp
  src/2d/graphics/Stroke.cpp
  src/2d/graphics/StrokeBuilder.cpp
  src/2d/graphics/TextPainter.cpp
  src/2d/graphics/TileLayer.cpp
  src/2d/graphics/VectorAtlas.cpp
  src/2d/lighting/LightFlicker.cpp
  src/2d/lighting/Light.cpp
  src/2d/lighting/Occluder.cpp
  src/2d/lighting/ShadowMap.cpp
  src/2d/navigation/ConstrainedTriangulation.cpp
  src/2d/navigation/Crowd.cpp
  src/2d/navigation/DijkstraMap.cpp
  src/2d/navigation/FlowField.cpp
  src/2d/navigation/Graph.cpp
  src/2d/navigation/GraphSearch.cpp
  src/2d/navigation/Grid.cpp
  src/2d/navigation/GridSearch.cpp
  src/2d/navigation/HierarchicalPathfinder.cpp
  src/2d/navigation/NavMesh.cpp
  src/2d/navigation/SteeringAgent.cpp
  src/2d/navigation/Wanderer.cpp
  src/2d/nodes/Node.cpp
  src/2d/nodes/NodeSerializer.cpp
  src/2d/nodes/NodeTree.cpp
  src/2d/nodes/Prefab.cpp
  src/2d/particles/Effect.cpp
  src/2d/particles/Emitter.cpp
  src/2d/particles/ImageShape.cpp
  src/2d/particles/Ribbon.cpp
  src/2d/particles/System.cpp
  src/2d/particles/Trail.cpp
  src/2d/physics/Body.cpp
  src/2d/physics/Box2DConverter.cpp
  src/2d/physics/DebugDraw.cpp
  src/2d/physics/Explosion.cpp
  src/2d/physics/Fluid.cpp
  src/2d/physics/ForceField.cpp
  src/2d/physics/Fracture.cpp
  src/2d/physics/Grabber.cpp
  src/2d/physics/Joint.cpp
  src/2d/physics/Mover.cpp
  src/2d/physics/PathPredictor.cpp
  src/2d/physics/Ragdoll.cpp
  src/2d/physics/RayBatch.cpp
  src/2d/physics/RayDebugDraw.cpp
  src/2d/physics/Raycaster.cpp
  src/2d/physics/Rope.cpp
  src/2d/physics/Shape.cpp
  src/2d/physics/StepTasks.cpp
  src/2d/physics/Terrain.cpp
  src/2d/physics/TopDownVehicle.cpp
  src/2d/physics/Vehicle.cpp
  src/2d/physics/World.cpp
  src/2d/procedural/Autotile.cpp
  src/2d/procedural/CellularAutomaton.cpp
  src/2d/procedural/Delaunay.cpp
  src/2d/procedural/DrunkardWalk.cpp
  src/2d/procedural/Dungeon.cpp
  src/2d/procedural/Maze.cpp
  src/2d/procedural/Placement.cpp
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
  src/2d/spatial/GridRay.cpp
  src/2d/spatial/HashGrid.cpp
  src/2d/spatial/KdTree.cpp
  src/2d/spatial/OverlapWorld.cpp
  src/2d/spatial/QuadTree.cpp
  src/2d/spatial/UnionFind.cpp
  src/2d/spatial/VisibilityPolygon.cpp
  src/2d/tiled/AutoMapper.cpp
  src/2d/tiled/Layer.cpp
  src/2d/tiled/Map.cpp
  src/2d/tiled/MapParser.cpp
  src/2d/tiled/MapQuery.cpp
  src/2d/tiled/MapRenderer.cpp
  src/2d/tiled/MapWriter.cpp
  src/2d/tiled/Object.cpp
  src/2d/tiled/ObjectFactories.cpp
  src/2d/tiled/Properties.cpp
  src/2d/tiled/PropertyTypes.cpp
  src/2d/tiled/TileCollision.cpp
  src/2d/tiled/Tileset.cpp
  src/2d/tiled/WangSet.cpp
  src/2d/tiled/World.cpp
  src/ai/BehaviorTree.cpp
  src/ai/InfluenceMap.cpp
  src/ai/ResponseCurve.cpp
  src/ai/StateMachine.cpp
  src/ai/UtilitySelector.cpp
  src/assets/GroupManifest.cpp
  src/assets/Manager.cpp
  src/audio/BufferSource.cpp
  src/audio/Delay.cpp
  src/audio/Device.cpp
  src/audio/Effect.cpp
  src/audio/EffectChain.cpp
  src/audio/EffectNode.cpp
  src/audio/Filter.cpp
  src/audio/Freeverb.cpp
  src/audio/HostedOutput.cpp
  src/audio/Limiter.cpp
  src/audio/MemoryDecoder.cpp
  src/audio/OutputSettings.cpp
  src/audio/Mixer.cpp
  src/audio/MixerState.cpp
  src/audio/OggPageIndex.cpp
  src/audio/ReaderDecoder.cpp
  src/audio/ReaderSource.cpp
  src/audio/Reverb.cpp
  src/audio/Session.cpp
  src/audio/Sound.cpp
  src/audio/SoundData.cpp
  src/audio/StreamSource.cpp
  src/audio/VorbisDecoder.cpp
  src/content/Bootstrap.cpp
  src/content/Compatibility.cpp
  src/content/CompositeKeyProvider.cpp
  src/content/ContentManager.cpp
  src/content/crypto/Aead.cpp
  src/content/crypto/ContentKey.cpp
  src/content/crypto/Digest.cpp
  src/content/crypto/Hasher.cpp
  src/content/crypto/KeyRing.cpp
  src/content/crypto/VerifyingKey.cpp
  src/content/EmbeddedKeyProvider.cpp
  src/content/KeyProvider.cpp
  src/content/Error.cpp
  src/content/FolderStoreFiles.cpp
  src/content/format/BinaryReader.cpp
  src/content/format/BinaryWriter.cpp
  src/content/format/Catalog.cpp
  src/content/format/ChannelDescriptor.cpp
  src/content/format/ChunkRecord.cpp
  src/content/format/Compression.cpp
  src/content/format/Manifest.cpp
  src/content/format/PartialRecord.cpp
  src/content/format/ShardHeader.cpp
  src/content/format/ShardIndex.cpp
  src/content/format/ShardReference.cpp
  src/content/format/StoreEntry.cpp
  src/content/format/StoreState.cpp
  src/content/HpakFileReader.cpp
  src/content/Downloader.cpp
  src/content/HpakPackage.cpp
  src/content/HttpTransport.cpp
  src/content/LocalStore.cpp
  src/content/PublicationLayout.cpp
  src/content/RangePlanner.cpp
  src/content/ReleasePackage.cpp
  src/content/ShardIndexFile.cpp
  src/content/ShardReader.cpp
  src/content/ShardSet.cpp
  src/content/StoreFiles.cpp
  src/content/UpdateScreen.cpp
  src/content/Updater.cpp
  src/conversation/Conversation.cpp
  src/conversation/ConversationState.cpp
  src/conversation/Expression.cpp
  src/core/AppConfig.cpp
  src/core/AppIdentifier.cpp
  src/core/AppError.cpp
  src/core/Base64.cpp
  src/core/Application.cpp
  src/core/ConnectionScope.cpp
  src/core/Engine.cpp
  src/core/ErrorScreen.cpp
  src/core/EventBus.cpp
  src/core/FloatBuffer.cpp
  src/core/FrameClock.cpp
  src/core/FrameQueue.cpp
  src/core/JobSystem.cpp
  src/core/JsonBytes.cpp
  src/core/JsonNumber.cpp
  src/core/JsonValidator.cpp
  src/core/LoadingView.cpp
  src/core/Log.cpp
  src/core/MessagePack.cpp
  src/core/PropertyTrack.cpp
  src/core/PropertyTween.cpp
  src/core/Scene.cpp
  src/core/SceneLoad.cpp
  src/core/SceneManager.cpp
  src/core/Timeline.cpp
  src/core/TimerScheduler.cpp
  src/core/Tween.cpp
  src/core/TweenManager.cpp
  src/core/TweenMotion.cpp
  src/core/TweenProperty.cpp
  src/core/TweenValue.cpp
  src/core/Utf8.cpp
  src/debug/Console.cpp
  src/debug/DebugWindow.cpp
  src/debug/Inspector.cpp
  src/debug/InspectorWindow.cpp
  src/debug/Monitor.cpp
  src/debug/ObjectCounter.cpp
  src/debug/OverlayWindow.cpp
  src/debug/Profiler.cpp
  src/debug/Stats.cpp
  src/debug/StatsDisplay.cpp
  src/debug/TraceRecorder.cpp
  src/debug/TraceSpan.cpp
  src/gpu/SurfaceTurn.cpp
  src/graphics/BlendMode.cpp
  src/graphics/Device.cpp
  src/graphics/DeviceState.cpp
  src/graphics/Gradient.cpp
  src/graphics/ImageFilters.cpp
  src/graphics/ImagePalette.cpp
  src/graphics/ImagePlaceholder.cpp
  src/graphics/Image.cpp
  src/graphics/PixelMemory.cpp
  src/graphics/RenderTarget.cpp
  src/graphics/ResourceGraveyard.cpp
  src/graphics/Shader.cpp
  src/graphics/ShaderPackageError.cpp
  src/graphics/ShaderResource.cpp
  src/graphics/SvgDocument.cpp
  src/graphics/Texture.cpp
  src/graphics/TextureResource.cpp
  src/graphics/VectorImage.cpp
  src/graphics/VectorImageResource.cpp
  src/graphics/Video.cpp
  src/graphics/Viewport.cpp
  src/input/ActionMap.cpp
  src/input/ActionMaps.cpp
  src/input/AppTextField.cpp
  src/input/CommandBuffer.cpp
  src/input/Controls.cpp
  src/input/Gesture.cpp
  src/input/GestureRecognizer.cpp
  src/input/Input.cpp
  src/input/InputPlayback.cpp
  src/input/InputRecording.cpp
  src/input/GamepadMotion.cpp
  src/input/PointerEmulation.cpp
  src/input/VirtualInput.cpp
  src/io/CompositePackage.cpp
  src/io/DirectoryPackage.cpp
  src/io/FileReader.cpp
  src/io/MemoryPackage.cpp
  src/io/MemoryReader.cpp
  src/io/OverlayPackage.cpp
  src/io/Package.cpp
  src/io/PackageReader.cpp
  src/io/PackageWatcher.cpp
  src/io/Path.cpp
  src/io/ZipArchive.cpp
  src/io/ZipPackage.cpp
  src/io/ZipReader.cpp
  src/localization/Catalog.cpp
  src/localization/DateFormat.cpp
  src/localization/LanguageTag.cpp
  src/localization/ListFormat.cpp
  src/localization/LocaleData.cpp
  src/localization/NumberFormat.cpp
  src/localization/PluralRules.cpp
  src/math/Box.cpp
  src/math/Color.cpp
  src/math/CurvePath3D.cpp
  src/math/ColorSpace.cpp
  src/math/DualQuat.cpp
  src/math/Cylindrical.cpp
  src/math/Easing.cpp
  src/math/EasingCurve.cpp
  src/math/Frustum.cpp
  src/math/Geometry2D.cpp
  src/math/Geometry3D.cpp
  src/math/HalfFloat.cpp
  src/math/HampelFilter.cpp
  src/math/KalmanFilter.cpp
  src/math/LowPassFilter.cpp
  src/math/MarchingSquares.cpp
  src/math/Mat3.cpp
  src/math/Mat4.cpp
  src/math/MatrixBatch.cpp
  src/math/MedianFilter.cpp
  src/math/MovingAverage.cpp
  src/math/Noise2D.cpp
  src/math/Noise3D.cpp
  src/math/NurbsBasis.cpp
  src/math/NurbsSurface.cpp
  src/math/NurbsVolume.cpp
  src/math/ParallelTransport.cpp
  src/math/ParametricCurve3D.cpp
  src/math/Obb.cpp
  src/math/Photometry.cpp
  src/math/PoissonDisk2D.cpp
  src/math/PoissonDisk3D.cpp
  src/math/Polygon.cpp
  src/math/Quat.cpp
  src/math/Random.cpp
  src/math/Raycast.cpp
  src/math/ShuffleBag.cpp
  src/math/SignalFilterHelper.cpp
  src/math/Spherical.cpp
  src/math/SphericalHarmonics.cpp
  src/math/Spline2D.cpp
  src/math/Spline3D.cpp
  src/math/Spring.cpp
  src/math/Transform2D.cpp
  src/math/Transform3D.cpp
  src/math/Triangle.cpp
  src/math/Triangulator.cpp
  src/math/WeightedChoice.cpp
  src/net/SocketLink.cpp
  src/net/WebSocket.cpp
  src/platform/Accessibility.cpp
  src/platform/AccessibilityElement.cpp
  src/platform/AccessibilityRelay.cpp
  src/platform/AccessibilitySettings.cpp
  src/platform/AppPlugin.cpp
  src/platform/AudioStream.cpp
  src/platform/Battery.cpp
  src/platform/Bridge.cpp
  src/platform/BridgeRelay.cpp
  src/platform/ConsoleRelay.cpp
  src/platform/DevelopmentConnection.cpp
  src/platform/DevelopmentSession.cpp
  src/platform/KeptEvents.cpp
  src/platform/TextHelper.cpp
  src/platform/native/NativeApi.cpp
  src/platform/native/NativeCallback.cpp
  src/platform/native/NativeLibraries.cpp
  src/platform/native/NativeSignature.cpp
  src/platform/NativeViews.cpp
  src/platform/ReturnHold.cpp
  src/platform/PluginManifest.cpp
  src/platform/PluginStreams.cpp
  src/platform/Fold.cpp
  src/platform/FoldSimulation.cpp
  src/platform/FullscreenTarget.cpp
  src/platform/SafeAreaSimulation.cpp
  src/platform/ScreenRelay.cpp
  src/platform/Screens.cpp
  src/platform/LaunchOptions.cpp
  src/platform/LinkRelay.cpp
  src/platform/System.cpp
  src/platform/SystemInfo.cpp
  src/platform/SystemAnswer.cpp
  src/platform/SystemState.cpp
  src/platform/VideoStream.cpp
  src/platform/Display.cpp
  src/platform/Displays.cpp
  src/platform/Outputs.cpp
  src/platform/Window.cpp
  src/platform/WindowPlacement.cpp
  src/plugins/Animation2DPlugin.cpp
  src/plugins/AudioPlugin.cpp
  src/plugins/BuiltInPlugins.cpp
  src/plugins/ContentPlugin.cpp
  src/plugins/ConversationPlugin.cpp
  src/plugins/DebugPlugin.cpp
  src/plugins/HotReloadPlugin.cpp
  src/plugins/LocalizationPlugin.cpp
  src/plugins/NetPlugin.cpp
  src/plugins/Nodes2DPlugin.cpp
  src/plugins/Particles2DPlugin.cpp
  src/plugins/Physics2DPlugin.cpp
  src/plugins/PlatformPlugin.cpp
  src/plugins/Plugin.cpp
  src/plugins/PluginRegistry.cpp
  src/plugins/ReturnHoldPlugin.cpp
  src/plugins/StoragePlugin.cpp
  src/plugins/TextPlugin.cpp
  src/plugins/TiledPlugin.cpp
  src/plugins/UiPlugin.cpp
  src/plugins/VideoPlugin.cpp
  src/storage/Preferences.cpp
  src/storage/ResumeState.cpp
  src/storage/SaveSlots.cpp
  src/storage/StoredJson.cpp
  src/storage/UserStorage.cpp
  src/text/BidiParagraph.cpp
  src/text/BitmapFont.cpp
  src/text/ColorGlyphImage.cpp
  src/text/DistanceField.cpp
  src/text/Effect.cpp
  src/text/Font.cpp
  src/text/FontFamily.cpp
  src/text/FontFile.cpp
  src/text/Gradient.cpp
  src/text/LayoutBuilder.cpp
  src/text/LayoutCache.cpp
  src/text/MarkupParser.cpp
  src/text/PhraseBreaker.cpp
  src/text/RichText.cpp
  src/text/RichTextRegistry.cpp
  src/text/Segmenter.cpp
  src/text/SystemFonts.cpp
  src/text/Style.cpp
  src/text/TextFit.cpp
  src/text/TrueTypeFont.cpp
  src/ui/AccessibilityTree.cpp
  src/ui/AlertScreen.cpp
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
  src/ui/components/ChoiceItem.cpp
  src/ui/components/DragAndDrop.cpp
  src/ui/components/choices/Checkbox.cpp
  src/ui/components/choices/Combo.cpp
  src/ui/components/choices/RadioGroup.cpp
  src/ui/components/choices/SegmentedControl.cpp
  src/ui/components/choices/Toggle.cpp
  src/ui/components/collections/CellTemplate.cpp
  src/ui/components/collections/CellTypes.cpp
  src/ui/components/collections/CollectionDiff.cpp
  src/ui/components/collections/CollectionIds.cpp
  src/ui/components/collections/GridLayout.cpp
  src/ui/components/collections/LinearLayout.cpp
  src/ui/components/collections/List.cpp
  src/ui/components/collections/ListRow.cpp
  src/ui/components/collections/SizeCache.cpp
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
  src/ui/ClampedScroll.cpp
  src/ui/Collection.cpp
  src/ui/CollectionCell.cpp
  src/ui/CollectionItems.cpp
  src/ui/Context.cpp
  src/ui/FocusNavigator.cpp
  src/ui/FocusSearch.cpp
  src/ui/IconColor.cpp
  src/ui/Gui.cpp
  src/ui/ImGuiConverter.cpp
  src/ui/ImGuiFontLoader.cpp
  src/ui/ImageLibrary.cpp
  src/ui/Message.cpp
  src/ui/MessageStack.cpp
  src/ui/MessageView.cpp
  src/ui/MountLink.cpp
  src/ui/Navigation.cpp
  src/ui/NavigationInput.cpp
  src/ui/NavigationLink.cpp
  src/ui/NavigationSplit.cpp
  src/ui/NavigationStack.cpp
  src/ui/NavigationTabs.cpp
  src/ui/NavigationTransition.cpp
  src/ui/Popup.cpp
  src/ui/Presentation.cpp
  src/ui/PresentationView.cpp
  src/ui/PropertyReader.cpp
  src/ui/Router.cpp
  src/ui/Scaling.cpp
  src/ui/Screen.cpp
  src/ui/ScrollArbiter.cpp
  src/ui/ScrollFade.cpp
  src/ui/ScrollFinger.cpp
  src/ui/Scrollbar.cpp
  src/ui/Scroller.cpp
  src/ui/SoundLibrary.cpp
  src/ui/Style.cpp
  src/ui/Surfaces.cpp
  src/ui/TextEditor.cpp
  src/ui/TextFieldLayout.cpp
  src/ui/TextSession.cpp
  src/ui/TextValue.cpp
  src/ui/Theme.cpp
  src/ui/Typography.cpp
  src/ui/Widgets.cpp
)

# Native libraries load at run time through the dynamic linker of the system where the platform allows `dynamicLibraries`, and only the libraries linked into the app load elsewhere.
if("dynamicLibraries" IN_LIST HAYLEN_CAPABILITIES AND HAYLEN_SYSTEM_API MATCHES "^(posix|windows)$")
  list(APPEND HAYLEN_ENGINE_SOURCES src/platform/${HAYLEN_SYSTEM_API}/LibraryLoader.cpp)
else()
  list(APPEND HAYLEN_ENGINE_SOURCES src/platform/native/unavailable/LibraryLoader.cpp)
endif()

# Native callbacks are the closures of libffi, which Varn builds with its driver `LIBFFI`, and which a port writes into memory only where it allows `executableMemory`, while the platforms of the engine without that capability take the closures that libffi makes from trampolines it ships. A platform without them makes no callback.
if(VARN_FFI_DRIVER STREQUAL "LIBFFI" AND (NOT HAYLEN_PORT_PLATFORM OR "executableMemory" IN_LIST HAYLEN_CAPABILITIES))
  list(APPEND HAYLEN_ENGINE_SOURCES src/platform/native/libffi/NativeCallback.cpp)
  set(HAYLEN_NATIVE_CALLBACKS_LIBFFI ON)
else()
  list(APPEND HAYLEN_ENGINE_SOURCES src/platform/native/unavailable/NativeCallback.cpp)
endif()

# The files of the content store and the local time reach the C library of the system of the build, which the browser gives the interfaces of POSIX for.
if(HAYLEN_SYSTEM_API STREQUAL "windows")
  list(APPEND HAYLEN_ENGINE_SOURCES src/content/windows/StoreFile.cpp src/localization/windows/LocalTime.cpp)
else()
  list(APPEND HAYLEN_ENGINE_SOURCES src/content/posix/StoreFile.cpp src/localization/posix/LocalTime.cpp)
endif()

# The browser build talks WebSocket through the page and plays audio through it, while native builds use the sockets of Poco on threads of their own with the TLS stack that Varn builds, whatever its event loop.
if(EMSCRIPTEN)
  list(APPEND HAYLEN_ENGINE_SOURCES src/platform/web/BrowserAudioOutput.cpp src/platform/web/BrowserWebSocket.cpp)
elseif(VARN_ENABLE_TLS)
  list(APPEND HAYLEN_ENGINE_SOURCES src/net/PocoWebSocket.cpp)
else()
  list(APPEND HAYLEN_ENGINE_SOURCES src/net/unavailable/WebSocketTransport.cpp)
endif()

set(HAYLEN_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file("${CMAKE_CURRENT_LIST_DIR}/include/haylen/core/Version.hpp.in" "${HAYLEN_GENERATED_INCLUDE_DIR}/haylen/core/Version.hpp" @ONLY)

# The contract of ports answers what the platform of the build allows from the capabilities and the threads that `haylen-platform.cmake` decided.
foreach(capability IN ITEMS DYNAMIC_LIBRARIES:dynamicLibraries EXECUTABLE_MEMORY:executableMemory PROCESSES:processes)
  string(REPLACE ":" ";" capability "${capability}")
  list(GET capability 0 capability_variable)
  list(GET capability 1 capability_name)
  if(capability_name IN_LIST HAYLEN_CAPABILITIES)
    set(HAYLEN_ALLOWS_${capability_variable} true)
  else()
    set(HAYLEN_ALLOWS_${capability_variable} false)
  endif()
endforeach()
if(HAYLEN_THREADS)
  set(HAYLEN_HAS_THREADS true)
else()
  set(HAYLEN_HAS_THREADS false)
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/src/port/Contract.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/generated/src/port/Contract.cpp" @ONLY)
list(APPEND HAYLEN_ENGINE_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/generated/src/port/Contract.cpp")

add_library(haylen-engine STATIC ${HAYLEN_ENGINE_SOURCES})
add_library(haylen::engine ALIAS haylen-engine)

# Public headers use C++20, so projects that link the engine compile with it too.
target_compile_features(haylen-engine PUBLIC cxx_std_20)
# Apps live in folders of their own, so the exception and `longjmp` support and the SIMD instructions of the browser build travel with the engine target instead of the engine folder.
if(EMSCRIPTEN)
  target_compile_options(haylen-engine INTERFACE -fwasm-exceptions -sSUPPORT_LONGJMP=wasm -msimd128)
  target_link_options(haylen-engine INTERFACE -fwasm-exceptions -sSUPPORT_LONGJMP=wasm)
endif()
target_include_directories(haylen-engine
  PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include" "${HAYLEN_GENERATED_INCLUDE_DIR}"
  PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src"
)

target_link_libraries(haylen-engine
  PUBLIC nlohmann_json::nlohmann_json varn-runtime haylen-imgui
  PRIVATE box2d::box2d Clipper2 msdfgen::msdfgen-core haylen-harfbuzz haylen-sheenbidi haylen-unibreak haylen-miniaudio haylen-stb haylen-nanosvg haylen-fast-float haylen-monocypher libzip::zip ZLIB::ZLIB libzstd_static
)

if(TARGET Poco::NetSSL)
  target_link_libraries(haylen-engine PRIVATE Poco::NetSSL)
endif()

# The native callbacks of the engine are closures of the libffi that Varn builds.
if(HAYLEN_NATIVE_CALLBACKS_LIBFFI)
  target_link_libraries(haylen-engine PRIVATE varn-ffi-vendor)
endif()

# Native libraries load through the dynamic linker, which Linux keeps in its own library.
target_link_libraries(haylen-engine PRIVATE ${CMAKE_DL_LIBS})

# Apple platforms reach Foundation through Objective-C.
if(APPLE)
  target_link_libraries(haylen-engine PRIVATE "-framework Foundation")
endif()

# Programs that draw into lit canvases also compile with HAYLEN_LIT, which writes the images the light pass reads, the sprite program compiles once more with HAYLEN_BATCH for the sprite batches with several textures or sprite data, and the text program with HAYLEN_TEXT_GRADIENT for text filled with a gradient.
set(shader_dir "${CMAKE_CURRENT_LIST_DIR}/shaders")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/sprite.glsl" SYMBOL getSpriteShader VARIANTS "sprite=" "sprite_lit=HAYLEN_LIT" "batch=HAYLEN_BATCH" "batch_lit=HAYLEN_BATCH,HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/text.glsl" SYMBOL getTextShader VARIANTS "text=HAYLEN_TEXT" "text_lit=HAYLEN_TEXT,HAYLEN_LIT" "text_plain=HAYLEN_TEXT,HAYLEN_TEXT_PLAIN" "text_plain_lit=HAYLEN_TEXT,HAYLEN_TEXT_PLAIN,HAYLEN_LIT" "text_gradient=HAYLEN_TEXT,HAYLEN_TEXT_GRADIENT" "text_gradient_lit=HAYLEN_TEXT,HAYLEN_TEXT_GRADIENT,HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/mesh.glsl" SYMBOL getMeshShader VARIANTS "mesh=HAYLEN_MESH" "mesh_lit=HAYLEN_MESH,HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/shape.glsl" SYMBOL getShapeShader VARIANTS "shape=HAYLEN_SHAPE" "shape_lit=HAYLEN_SHAPE,HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/blend.glsl" SYMBOL getBlendShader VARIANTS "blend=" "blend_lit=HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/backdrop.glsl" SYMBOL getBackdropShader)
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/glass.glsl" SYMBOL getGlassShader VARIANTS "glass=HAYLEN_SHAPE")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/metaball.glsl" SYMBOL getMetaballShader VARIANTS "metaball=" "metaball_lit=HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/recolor.glsl" SYMBOL getRecolorShader VARIANTS "recolor=" "recolor_lit=HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/composite.glsl" SYMBOL getCompositeShader VARIANTS "composite=HAYLEN_COMPOSITE_LIGHTING,HAYLEN_COMPOSITE_EFFECTS" "composite_lit=HAYLEN_COMPOSITE_LIGHTING" "composite_plain=")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/filter.glsl" SYMBOL getFilterShader)
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/effect.glsl" SYMBOL getEffectShader VARIANTS "effect=" "effect_lit=HAYLEN_LIT")
haylen_compile_shader(haylen-engine SOURCE "${shader_dir}/light.glsl" SYMBOL getLightShader VARIANTS "light=" "light_shadowed=HAYLEN_SHADOWS" "light_uniform=HAYLEN_UNIFORM_LIGHTING" "light_uniform_shadowed=HAYLEN_SHADOWS,HAYLEN_UNIFORM_LIGHTING")
haylen_embed_file(haylen-engine "${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf" getDefaultFont)
haylen_embed_file(haylen-engine "${CMAKE_CURRENT_SOURCE_DIR}/fonts/space-mono-regular.ttf" getMonospaceFont)
haylen_embed_file(haylen-engine "${budoux_SOURCE_DIR}/budoux/models/th.json" getThaiPhraseModel)

haylen_enable_warnings(haylen-engine)
haylen_enable_coverage(haylen-engine)

haylen_add_gpu_library()

# GCC and MSVC link the intermediate code of a Release build only when the link asks for it, so every program that links the engine asks, whatever its own configuration, which a Debug app that links the prebuilt SDK has.
if(HAYLEN_LINK_TIME_OPTIMIZATION AND CMAKE_BUILD_TYPE STREQUAL "Release")
  if(MSVC)
    target_link_options(haylen-engine INTERFACE /LTCG /INCREMENTAL:NO)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # The only code that the late string checks of the link of GCC report is `lstrlib.c` of Lua, a dependency, whose `add_s` copies a match whose start GCC cannot prove lies before its end, while every file of the engine compiles with those checks on.
    target_link_options(haylen-engine INTERFACE -flto=auto -Wno-stringop-overflow -Wno-stringop-overread)
  elseif(ANDROID AND ANDROID_PLATFORM_LEVEL LESS 29)
    # Android before API 29 keeps thread-local variables in emulated storage, which the compile uses, so the code that the link generates from the intermediate code uses it too.
    target_link_options(haylen-engine INTERFACE "LINKER:-plugin-opt=-emulated-tls")
  endif()
endif()

# The writing side of the content format, which builds, seals and signs releases on the machines that make them with the modules that a module compiler of the scripts compiles: the content tool, the tests and the benchmarks link it, while apps link only the reading side of the engine, so it builds only for them.
add_library(haylen-content-writer STATIC EXCLUDE_FROM_ALL
  src/content/ContentBuilder.cpp
  src/content/crypto/SigningKey.cpp
  src/content/format/CatalogWriter.cpp
  src/content/format/ChannelWriter.cpp
  src/content/format/Chunker.cpp
  src/content/format/Compressor.cpp
  src/content/format/ManifestWriter.cpp
  src/content/format/RecordSealer.cpp
  src/content/RecordCache.cpp
  src/content/ReleaseBuilder.cpp
  src/content/ShardWriter.cpp
)
target_include_directories(haylen-content-writer PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(haylen-content-writer PUBLIC haylen-engine PRIVATE haylen-monocypher libzstd_static)
haylen_enable_warnings(haylen-content-writer)
haylen_enable_coverage(haylen-content-writer)

# Portable pieces of the runtime, which the tests link without an app entry point. The readers of the platform folders compile everywhere, so every host tests them.
add_library(haylen-platform STATIC
  src/platform/CrashGuard.cpp
  src/platform/SystemFontReader.cpp
  src/platform/DeviceRecovery.cpp
  src/platform/GraphicsApiFallback.cpp
  src/platform/GamepadSlots.cpp
  src/platform/GamepadVendors.cpp
  src/platform/FrameCallbackGate.cpp
  src/platform/FrameRateGate.cpp
  src/platform/FrameTimer.cpp
  src/platform/GraphicsApiOrder.cpp
  src/platform/KeyboardTranslator.cpp
  src/platform/PixelDensityCap.cpp
  src/platform/PluginLoadOrder.cpp
  src/platform/SharedTextInput.cpp
  src/platform/android/AndroidBatteryStatus.cpp
  src/platform/android/AndroidThermalStatus.cpp
  src/platform/android/AndroidDeviceInfo.cpp
  src/platform/android/AndroidFramePacer.cpp
  src/platform/android/AndroidGamepadStates.cpp
  src/platform/android/AndroidGlesVersion.cpp
  src/platform/android/AndroidGraphicsApis.cpp
  src/platform/android/AndroidKeyClaims.cpp
  src/platform/android/AndroidYuvImage.cpp
  src/platform/linux/LinuxSystemReader.cpp
  src/platform/linux/LinuxVisualColor.cpp
  src/platform/MemoryWarning.cpp
  src/platform/ReportedState.cpp
  src/platform/Services.cpp
  src/platform/SupportedOrientations.cpp
  src/platform/SurfaceCheck.cpp
  src/platform/Touches.cpp
  src/platform/windows/WindowsPowerStatus.cpp
  src/platform/worker/Events.cpp
  src/platform/worker/Gamepads.cpp
  src/platform/worker/Keys.cpp
)
add_library(haylen::platform ALIAS haylen-platform)
# Apps see only the public API, so the private headers stay private to the engine libraries.
# The handlers of native crashes are the signals of POSIX, the unhandled exceptions of Windows, or none in the browser.
if(HAYLEN_SYSTEM_API STREQUAL "posix")
  target_sources(haylen-platform PRIVATE src/platform/posix/CrashHandlers.cpp src/platform/posix/PosixCrashHandlers.cpp)
elseif(HAYLEN_SYSTEM_API STREQUAL "windows")
  target_sources(haylen-platform PRIVATE src/platform/windows/CrashHandlers.cpp src/platform/windows/WindowsCrashHandlers.cpp)
else()
  target_sources(haylen-platform PRIVATE src/platform/web/CrashHandlers.cpp)
endif()
target_include_directories(haylen-platform PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(haylen-platform PUBLIC haylen-engine haylen-gpu)
haylen_enable_warnings(haylen-platform)
haylen_enable_coverage(haylen-platform)

# The runtime turns the engine into an app: the player, the native host, the services of the platform and the platform layer that opens the window, runs the frames, reads the input and holds the entry point, while the GPU devices of `haylen-gpu` draw the frames. It is an object library so entry points always reach the final binary.
set(HAYLEN_RUNTIME_SOURCES
  src/platform/NativeHost.cpp
  src/platform/Player.cpp
)
# A port brings the platform layer and the services of its platform, which reach the engine through the public API of `haylen/port`.
if(HAYLEN_PORT_PLATFORM)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/port/BridgeReplies.cpp src/port/GpuDevices.cpp src/port/Player.cpp src/port/PortHost.cpp src/port/SystemReports.cpp ${HAYLEN_PORT_RUNTIME_SOURCES})
elseif(APPLE)
  set(HAYLEN_APPLE_SOURCES
    src/platform/apple/AppleAccessibility.mm
    src/platform/apple/AppleBackgroundAudio.mm
    src/platform/apple/AppleBattery.mm
    src/platform/apple/AppleBridge.mm
    src/platform/apple/AppleDesktop.mm
    src/platform/apple/AppleDisplays.mm
    src/platform/apple/AppleCutouts.mm
    src/platform/apple/AppleFold.mm
    src/platform/apple/AppleFonts.mm
    src/platform/apple/AppleOutputs.mm
    src/platform/apple/AppleLanguages.mm
    src/platform/apple/AppleNetwork.mm
    src/platform/apple/AppleOrientation.mm
    src/platform/apple/ApplePlugins.mm
    src/platform/apple/ApplePointer.mm
    src/platform/apple/ApplePowerSaving.mm
    src/platform/apple/AppleThermalState.mm
    src/platform/apple/ApplePresenter.mm
    src/platform/apple/AppleRuntime.mm
    src/platform/apple/AppleScroll.mm
    src/platform/apple/AppleScreens.mm
    src/platform/apple/AppleServices.mm
    src/platform/apple/AppleSplash.mm
    src/platform/apple/AppleStatusBar.mm
    src/platform/apple/AppleStrings.mm
    src/platform/apple/AppleSystemGestures.mm
    src/platform/apple/AppleSystem.mm
    src/platform/apple/AppleTextInput.mm
    src/platform/apple/AppleTheme.mm
    src/platform/apple/AppleTimeZone.mm
    src/platform/apple/AppleVideoDecoder.mm
    src/platform/apple/CatalystWindow.mm
    src/platform/apple/HaylenAccessibilityElement.mm
    src/platform/apple/HaylenAccessibilityView.mm
    src/platform/apple/HaylenAudioStream.mm
    src/platform/apple/HaylenBridge.mm
    src/platform/apple/HaylenDisplaySceneDelegate.mm
    src/platform/apple/HaylenFieldEditor.mm
    src/platform/apple/HaylenOverlay.mm
    src/platform/apple/HaylenOverlayItem.mm
    src/platform/apple/HaylenOutputView.mm
    src/platform/apple/HaylenOverlayLayer.mm
    src/platform/apple/HaylenPlacement.mm
    src/platform/apple/HaylenPluginContext.mm
    src/platform/apple/HaylenRequirement.mm
    src/platform/apple/HaylenRequirements.mm
    src/platform/apple/HaylenScreen.mm
    src/platform/apple/HaylenScreenController.mm
    src/platform/apple/HaylenScreenSceneDelegate.mm
    src/platform/apple/HaylenScrollGesture.mm
    src/platform/apple/HaylenTextEditor.mm
    src/platform/apple/HaylenTextField.mm
    src/platform/apple/HaylenTextView.mm
    src/platform/apple/HaylenVideoLoader.mm
    src/platform/apple/HaylenThemeView.mm
    src/platform/apple/HaylenVideoStream.mm
  )
  # Only a Mac has a desktop of windows.
  if(HAYLEN_PLATFORM STREQUAL "macos")
    list(APPEND HAYLEN_APPLE_SOURCES src/platform/apple/AppleDesktopService.mm)
  endif()
  # Apple TV has no pasteboard, so its apps keep the clipboard of SDL inside the app.
  if(NOT HAYLEN_PLATFORM STREQUAL "tvos")
    list(APPEND HAYLEN_APPLE_SOURCES src/platform/apple/ApplePasteboard.mm)
  endif()
  # UIKit apps read their controllers through GameController, while SDL reads the controllers of macOS.
  if(NOT HAYLEN_PLATFORM STREQUAL "macos")
    list(APPEND HAYLEN_APPLE_SOURCES src/platform/apple/AppleGamepads.mm src/platform/apple/AppleRemote.mm)
  endif()
  list(APPEND HAYLEN_RUNTIME_SOURCES ${HAYLEN_APPLE_SOURCES})
  set_source_files_properties(${HAYLEN_APPLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
elseif(WIN32)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/windows/WindowsAccessibility.cpp src/platform/windows/WindowsAccessibilityElement.cpp src/platform/windows/WindowsAccessibilityRoot.cpp src/platform/windows/WindowsAccessibilityTree.cpp src/platform/windows/WindowsDesktop.cpp src/platform/windows/WindowsFonts.cpp src/platform/windows/WindowsFontText.cpp src/platform/windows/WindowsLinks.cpp src/platform/windows/WindowsMemory.cpp src/platform/windows/WindowsNetwork.cpp src/platform/windows/WindowsServices.cpp src/platform/windows/WindowsSettings.cpp src/platform/windows/WindowsSystem.cpp src/platform/windows/WindowsPackageStream.cpp src/platform/windows/WindowsText.cpp src/platform/windows/WindowsVideoDecoder.cpp)
elseif(ANDROID)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/android/AndroidActivity.cpp src/platform/android/AndroidAssetPackage.cpp src/platform/android/AndroidAssetReader.cpp src/platform/android/AndroidDisplays.cpp src/platform/android/AndroidEglContext.cpp src/platform/android/AndroidFonts.cpp src/platform/android/AndroidGamepads.cpp src/platform/android/AndroidGraphics.cpp src/platform/android/AndroidHost.cpp src/platform/android/AndroidInputEvents.cpp src/platform/android/AndroidJniExports.cpp src/platform/android/AndroidKeys.cpp src/platform/android/AndroidMessageQueue.cpp src/platform/android/AndroidMouseWheel.cpp src/platform/android/AndroidOutputs.cpp src/platform/android/AndroidRuntime.cpp src/platform/android/AndroidServices.cpp src/platform/android/AndroidStreams.cpp src/platform/android/AndroidTextInput.cpp src/platform/android/AndroidVideoDecoder.cpp src/platform/android/AndroidWindow.cpp src/platform/android/JavaBridge.cpp)
  # The video decoder references the functions of Android 9 that it needs weakly, so the runtime links `mediandk` and runs on older versions, which decode no video.
  set_source_files_properties(src/platform/android/AndroidVideoDecoder.cpp PROPERTIES COMPILE_DEFINITIONS "__ANDROID_UNAVAILABLE_SYMBOLS_ARE_WEAK__")
elseif(EMSCRIPTEN)
  # The web runs on the page through the platform layer of SDL, or in a dedicated worker through the worker layer, which `platform::WebMain` picks as the page asks.
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/web/WebDisplays.cpp src/platform/web/WebMain.cpp src/platform/web/WebOutputs.cpp src/platform/web/WebPage.cpp src/platform/web/WebServices.cpp src/platform/web/WebStoreFiles.cpp src/platform/web/WebTextInput.cpp src/platform/web/WebVideoDecoder.cpp src/platform/worker/GlContext.cpp src/platform/worker/Host.cpp src/platform/worker/Runtime.cpp src/platform/worker/Window.cpp)
else()
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/linux/LinuxAccessibility.cpp src/platform/linux/LinuxDesktop.cpp src/platform/linux/LinuxFonts.cpp src/platform/linux/LinuxGlib.cpp src/platform/linux/LinuxLinks.cpp src/platform/linux/LinuxMonitors.cpp src/platform/linux/LinuxServices.cpp src/platform/linux/LinuxSystem.cpp src/platform/linux/LinuxSettings.cpp)
endif()

# Every platform but Android runs apps through the platform layer of SDL, which the web replaces with the worker layer in a dedicated worker. The application delegate of macOS is the runtime's own, so SDL leaves the menus, the quit requests and the events of plugins to it, the scene delegate of UIKit derives from the one of SDL, which runs the app in its scene, and the view controller of the window answers UIKit with the status bar of the app.
if(NOT ANDROID AND NOT HAYLEN_PORT_PLATFORM)
  list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sdl/Events.cpp src/platform/sdl/FullscreenRequest.cpp src/platform/sdl/GlContext.cpp src/platform/sdl/Host.cpp src/platform/sdl/Keys.cpp src/platform/sdl/OutputPlacement.cpp src/platform/sdl/OutputWindows.cpp src/platform/sdl/Runtime.cpp src/platform/sdl/WaylandDisplays.cpp src/platform/sdl/Window.cpp)
  # Windows and Linux type through the text events of SDL, while the other platforms edit text in a native field of their own.
  if(HAYLEN_PLATFORM MATCHES "^(windows|linux)$")
    list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sdl/TextInput.cpp)
  endif()
  # A Wayland session paces the frames of the window by the frame callbacks of its surface, through the library of Wayland that SDL loads.
  if(HAYLEN_PLATFORM STREQUAL "linux")
    list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sdl/WaylandFramePacer.cpp)
  endif()
  # The host of Apple platforms reaches the native text field of `platform::AppleTextInput` and, in UIKit apps, the controllers of `platform::AppleGamepads`, whose headers speak Objective-C. UIKit apps read their controllers through those, and the other platforms through the gamepads of SDL.
  if(APPLE)
    set_source_files_properties(src/platform/sdl/Host.cpp PROPERTIES LANGUAGE OBJCXX COMPILE_OPTIONS "-fobjc-arc")
  endif()
  if(NOT HAYLEN_PLATFORM MATCHES "^(ios|tvos)$")
    list(APPEND HAYLEN_RUNTIME_SOURCES src/platform/sdl/Gamepads.cpp)
  endif()
  if(APPLE)
    set(HAYLEN_SDL_APPLE_SOURCES src/platform/sdl/apple/AppleApplication.mm src/platform/sdl/apple/AppleFramePacer.mm src/platform/sdl/apple/HaylenAppDelegate.mm src/platform/sdl/apple/HaylenFramePacer.mm src/platform/sdl/apple/HaylenRootViewController.mm src/platform/sdl/apple/HaylenSceneDelegate.mm)
    list(APPEND HAYLEN_RUNTIME_SOURCES ${HAYLEN_SDL_APPLE_SOURCES})
    set_source_files_properties(${HAYLEN_SDL_APPLE_SOURCES} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  endif()
endif()

add_library(haylen-runtime OBJECT ${HAYLEN_RUNTIME_SOURCES})
add_library(haylen::runtime ALIAS haylen-runtime)
target_include_directories(haylen-runtime PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
target_link_libraries(haylen-runtime PUBLIC haylen-platform haylen-gpu)
if(HAYLEN_PORT_PLATFORM)
  target_link_libraries(haylen-runtime PRIVATE ${HAYLEN_PORT_RUNTIME_LIBRARIES})
elseif(NOT ANDROID)
  target_link_libraries(haylen-runtime PRIVATE SDL3::SDL3-static)
endif()
# The web host gives the mixer the audio output of the page, a backend of miniaudio.
if(EMSCRIPTEN)
  target_link_libraries(haylen-runtime PRIVATE haylen-miniaudio)
endif()
# The Android host is GameActivity, whose static library holds the native side of the activity, its context of OpenGL ES comes from EGL, and its video decoder resamples the sound of codecs whose rate differs from their track with miniaudio.
if(ANDROID)
  target_link_libraries(haylen-runtime PRIVATE haylen-game-activity haylen-miniaudio EGL android log)
endif()
haylen_enable_warnings(haylen-runtime)
if(NOT HAYLEN_PORT_PLATFORM)
  haylen_link_runtime_platform(haylen-runtime PUBLIC)
endif()

add_subdirectory(lua)

# A port installs the SDK of its platform only when its `haylen_port` says that it builds one.
if(HAYLEN_BUILD_SDK AND HAYLEN_PORT_PLATFORM AND NOT HAYLEN_PORT_SDK)
  message(FATAL_ERROR "The port of \"${HAYLEN_PORT_DIR}\" builds no SDK, since its \"haylen_port\" does not pass \"SDK ON\", so \"HAYLEN_BUILD_SDK\" stays off.")
elseif(HAYLEN_BUILD_SDK)
  include(haylen-install)
endif()

# The headless host draws through the null GPU device, which checks every call of the engine against the contract of the device and draws nothing, and the GPU tests of the platforms that build them alone run the engine on it with a real device.
if(HAYLEN_DESKTOP OR (HAYLEN_GPU_TESTS_ALONE AND HAYLEN_BUILD_TESTS))
  add_library(haylen-headless-host OBJECT
    src/platform/headless/HeadlessHost.cpp
  )
  add_library(haylen::headless ALIAS haylen-headless-host)
  target_include_directories(haylen-headless-host PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-headless-host PUBLIC haylen-platform)
  haylen_enable_warnings(haylen-headless-host)
endif()

# The player runs any package: a folder or zip from the command line on desktop, or what the page loads through Module.haylen on the web.
# On Android it is the library that HaylenActivity of the haylen Android library loads, and it plays the package in the APK assets.
if(HAYLEN_BUILD_PLAYER AND (HAYLEN_DESKTOP OR EMSCRIPTEN OR HAYLEN_PORT_PLATFORM))
  add_executable(haylen)
  target_link_libraries(haylen PRIVATE haylen::runtime haylen::player)
  # The generator expression keeps multi-config generators such as Xcode and Visual Studio from adding a folder per configuration.
  set_target_properties(haylen PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/haylen/$<0:>")
  # The runtime of a port holds the entry point of its platform, while the Apple player starts from a main function of its own.
  if(APPLE AND NOT HAYLEN_PORT_PLATFORM)
    target_sources(haylen PRIVATE src/platform/apple/AppleMain.cpp)
  elseif(WIN32 AND NOT HAYLEN_PORT_PLATFORM)
    set_target_properties(haylen PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(haylen PRIVATE platform/windows/haylen.manifest src/gpu/windows/GpuPreferenceExports.cpp)
  elseif(EMSCRIPTEN)
    haylen_setup_web_page(haylen "")
  elseif(NOT HAYLEN_PORT_PLATFORM)
    # Linux apps keep their native libraries in lib next to the player, where the libraries also find the ones they depend on.
    set_target_properties(haylen PROPERTIES BUILD_RPATH "$ORIGIN;$ORIGIN/lib")
    target_link_options(haylen PRIVATE "LINKER:--enable-new-dtags")
  endif()
elseif(HAYLEN_BUILD_PLAYER AND ANDROID)
  add_library(haylen SHARED)
  target_link_libraries(haylen PRIVATE haylen::runtime haylen::player)
  # The player library records only the system libraries whose symbols it uses, and the engine port of the tooling checks them against the list of the engine, which this tree writes for it.
  target_link_options(haylen PRIVATE "LINKER:--as-needed")
  set(libraries_json "[]")
  foreach(library IN LISTS HAYLEN_ANDROID_LIBRARIES)
    string(JSON length LENGTH "${libraries_json}")
    string(JSON libraries_json SET "${libraries_json}" ${length} "\"${library}\"")
  endforeach()
  file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/haylen-android.json" CONTENT "{\"libraries\": ${libraries_json}}\n" @ONLY)
endif()

# The headless player runs a package without a window, GPU or audio device on the host of the engine tests, with the application that `core::Application::create` of the program makes, which suits automatic runs of apps in continuous integration. The `haylen-headless` program runs any package with the app of the Lua player, such as the test project, and a C++ app that adds the engine from its sources links the headless player next to its own sources. The player is a static library with the headless host, whose `main` the link of a program takes from it, as the program defines none of its own.
if(HAYLEN_DESKTOP)
  add_library(haylen-headless-player STATIC src/platform/headless/HeadlessMain.cpp src/platform/headless/HeadlessPlayer.cpp)
  add_library(haylen::headless-player ALIAS haylen-headless-player)
  target_include_directories(haylen-headless-player PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-headless-player PUBLIC haylen::headless)
  haylen_enable_warnings(haylen-headless-player)
endif()
if(HAYLEN_BUILD_PLAYER AND HAYLEN_DESKTOP)
  add_executable(haylen-headless)
  target_link_libraries(haylen-headless PRIVATE haylen::headless-player haylen::player)
  set_target_properties(haylen-headless PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-headless)
endif()

# Each slice of the Apple SDK merges the engine, its dependencies and the runtime into the library of `Haylen.xcframework`, and the Lua layer with the Lua player into the library of `HaylenLua.xcframework`.
if(HAYLEN_BUILD_FRAMEWORK)
  include(haylen-framework)
endif()

# The sprite benchmark runs on the real GPU of the desktop, so it is an app like any other.
if(HAYLEN_BUILD_BENCHMARKS AND HAYLEN_DESKTOP)
  haylen_add_app(haylen-sprite-benchmark CPP
    SOURCES bench/SpriteBenchmark.cpp
    CONTENT "${CMAKE_CURRENT_LIST_DIR}/bench/sprite-benchmark"
  )

  # The benchmarks of the renderer take `--graphics-api` to draw their frames offscreen through a device of the build instead of the null device of the headless host, with the offscreen contexts of OpenGL that the GPU tests draw through.
  add_library(haylen-benchmark-gpu STATIC
    bench/support/BenchmarkGpu.cpp
    bench/support/OffscreenSwapchain.cpp
    tests/gpu/support/OffscreenSurface.cpp
    ${HAYLEN_GPU_OFFSCREEN_SOURCES}
  )
  target_include_directories(haylen-benchmark-gpu PUBLIC "${CMAKE_CURRENT_LIST_DIR}/bench" "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/tests")
  target_link_libraries(haylen-benchmark-gpu PUBLIC haylen::engine haylen::gpu ${HAYLEN_GPU_OFFSCREEN_LIBRARIES})
  haylen_enable_warnings(haylen-benchmark-gpu)

  # The procedural benchmark times the heavy generation, geometry and destruction algorithms on the CPU, so it is a plain executable on the headless host.
  add_executable(haylen-procedural-benchmark bench/ProceduralBenchmark.cpp)
  target_link_libraries(haylen-procedural-benchmark PRIVATE haylen::headless)
  set_target_properties(haylen-procedural-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-procedural-benchmark)

  # The algorithm benchmark times path finding, crowds, spatial queries, ray casts and batch math on the CPU, so it is a plain executable too. The physics world links the debug drawing of the renderer, whose GPU calls the null GPU device of the headless host satisfies.
  add_executable(haylen-algorithm-benchmark bench/AlgorithmBenchmark.cpp)
  target_link_libraries(haylen-algorithm-benchmark PRIVATE haylen::engine haylen::headless)
  set_target_properties(haylen-algorithm-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-algorithm-benchmark)

  # The physics benchmark times the steps, fluids, queries, transform sync, debug drawing and Lua events of physics worlds on the CPU, inside an engine on the headless host.
  add_executable(haylen-physics-benchmark bench/PhysicsBenchmark.cpp)
  target_include_directories(haylen-physics-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-physics-benchmark PRIVATE haylen::lua haylen::headless haylen-benchmark-gpu)
  set_target_properties(haylen-physics-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-physics-benchmark)

  # The UI benchmark flings collections of a hundred thousand items on the headless host, which times what the frame thread spends on long lists and counts their binds and cells.
  add_executable(haylen-ui-benchmark bench/UiBenchmark.cpp)
  target_include_directories(haylen-ui-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-ui-benchmark PRIVATE haylen::lua haylen::headless haylen-benchmark-gpu)
  set_target_properties(haylen-ui-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-ui-benchmark)

  # The render benchmark times whole frames of sprites, shapes, text, rich text and interface components on the headless host, with the draw calls, uploads and allocations of each frame.
  add_executable(haylen-render-benchmark bench/RenderBenchmark.cpp)
  target_include_directories(haylen-render-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-render-benchmark PRIVATE haylen::lua haylen::headless haylen-benchmark-gpu)
  set_target_properties(haylen-render-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-render-benchmark)

  # The particle benchmark times the simulation and the drawing of emitters on the CPU, inside an engine on the headless host.
  add_executable(haylen-particle-benchmark bench/ParticleBenchmark.cpp)
  target_include_directories(haylen-particle-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-particle-benchmark PRIVATE haylen::lua haylen::headless haylen-benchmark-gpu)
  set_target_properties(haylen-particle-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-particle-benchmark)

  # The node benchmark times the update and the draw of large node trees on the CPU, inside an engine on the headless host.
  add_executable(haylen-node-benchmark bench/NodeBenchmark.cpp)
  target_include_directories(haylen-node-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-node-benchmark PRIVATE haylen::lua haylen::headless haylen-benchmark-gpu)
  set_target_properties(haylen-node-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-node-benchmark)

  # The animation benchmark times animated batches and crowds animated from Lua on the CPU, inside an engine on the headless host.
  add_executable(haylen-animation-benchmark bench/AnimationBenchmark.cpp)
  target_include_directories(haylen-animation-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-animation-benchmark PRIVATE haylen::lua haylen::headless haylen-benchmark-gpu)
  set_target_properties(haylen-animation-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-animation-benchmark)

  # The Lua benchmark runs the sprite package in bench/lua-benchmark on the headless host, which times what Lua costs to update and draw many sprites through tables and through float buffers.
  add_executable(haylen-lua-benchmark bench/LuaBenchmark.cpp)
  target_include_directories(haylen-lua-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-lua-benchmark PRIVATE haylen::lua haylen::headless)
  set_target_properties(haylen-lua-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-lua-benchmark)

  # The core benchmark times the frame loop, events, owners, timers, tweens, jobs and asset loading on the headless host.
  add_executable(haylen-core-benchmark bench/CoreBenchmark.cpp)
  target_include_directories(haylen-core-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
  target_link_libraries(haylen-core-benchmark PRIVATE haylen::lua haylen::headless)
  set_target_properties(haylen-core-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-core-benchmark)

  # The content benchmark times building, opening and reading a protected release, catalog lookups, the planning of update ranges and the local store, with the largest allocation of each.
  add_executable(haylen-content-benchmark bench/ContentBenchmark.cpp)
  target_include_directories(haylen-content-benchmark PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src" "${CMAKE_CURRENT_LIST_DIR}/lua/src")
  target_link_libraries(haylen-content-benchmark PRIVATE haylen-content-writer haylen::lua haylen::headless)
  set_target_properties(haylen-content-benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<0:>")
  haylen_enable_warnings(haylen-content-benchmark)
endif()

if((HAYLEN_BUILD_TOOLS OR HAYLEN_BUILD_TESTS) AND HAYLEN_DESKTOP)
  add_subdirectory(tools/content)
endif()

if(HAYLEN_BUILD_TESTS AND HAYLEN_DESKTOP)
  enable_testing()
  add_subdirectory(tests)
  add_subdirectory(lua/tests)
elseif(HAYLEN_BUILD_TESTS AND HAYLEN_GPU_TESTS_ALONE)
  include("${CMAKE_CURRENT_LIST_DIR}/tests/gpu/gpu-tests.cmake")
endif()
