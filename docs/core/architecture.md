# 🏛️ Architecture

This guide explains how Haylen is put together. It covers the libraries that the engine builds, where each module lives, how subsystems plug into the engine as plugins, how a backend plugs in under it, how a frame runs, which thread owns what, how GPU resources and Lua values are released, what an app package is, and what the web runtime offers to a browser editor. It is written for people who work on the engine and for C++ developers who extend it. If you only write Lua, start with the [Lua guide](lua.md) and the [Lua API reference](../lua-api/README.md).

## Libraries and products

The folder `engine/` is a CMake project of its own. It builds these targets, and the [embedding guide](../tools/embedding.md) explains how another project uses them.

| Target | Alias | What it is |
| --- | --- | --- |
| `haylen-engine` | `haylen::engine` | The static library with the whole C++ API and every Lua binding. It draws through the `gpu::Device` of the backend and reaches the platform through `platform::Host`. It never includes a backend and never calls the APIs of the operating system. Of the content format it holds only the reading side, which opens, verifies, decrypts, decodes and downloads releases. |
| `haylen-content-writer` | | The static library with the writing side of the content format: `ContentBuilder`, `ReleaseBuilder`, `ShardWriter`, `CatalogWriter`, `ManifestWriter`, `ChannelWriter`, `RecordSealer`, `Compressor`, `Chunker`, `RecordCache`, `LuaCompiler` and `SigningKey`. Apps never link it. The content tool, the tests, the fuzzers and the content benchmark link it, and it builds only for them. |
| `haylen-platform` | `haylen::platform` | The static library with the parts of the runtime that compile on every platform: the keyboard translator, the gamepad slots, the memory warning flag, the state that the platform reported last, and the readers of the platform folders that need no platform. Since they compile everywhere, every host tests them. The runtime and the tests link it. |
| `haylen-gpu` | `haylen::gpu` | The static library with `gpu::DeviceFactory` and the GPU device of every graphics API that `HAYLEN_GRAPHICS_APIS` names, each in its folder under `engine/src/gpu`, such as `vulkan`, whose `device.cmake` adds it to the library. The runtime and the GPU tests link it, and the SDK merges it into the runtime. |
| `haylen-runtime` | `haylen::runtime` | The object library that turns the engine into an app. It holds `platform::Player`, `platform::NativeHost`, the services of one platform folder, which CMake picks when it configures the build, and the [backend](#backends) that `HAYLEN_BACKEND_DIR` names. The backend opens the window, runs and draws the frames, and holds the entry point. With the Sokol backend the entry point is `main` on Linux, `WinMain` on Windows, `sokol_main` on Android and the web, and `haylen_main` on Apple platforms. |
| `haylen-headless-host` | `haylen::headless` | The object library with the headless host, a host without a window or a GPU. The tests, the headless player, the benchmarks and the content tool use it. It holds the null GPU device `gpu::NullDevice` that the host draws through, and `HeadlessPlayer`, which runs an app package on it. Only desktop platforms build it. |
| `haylen` | | The player executable. It links the runtime with `engine/src/platform/LuaPlayer.cpp` and runs any app package. |
| `haylen-headless` | | The headless player. Its entry point `engine/src/platform/headless/HeadlessMain.cpp` runs `HeadlessPlayer` on any app package, for automatic runs. It builds with the player on desktop platforms. |
| `haylen_add_app` | | The CMake function that builds the app of one package for the current platform. |

Every executable defines the static function `haylen::core::Application::create()`, which `haylen/core/Application.hpp` declares. In `LuaPlayer.cpp` it returns a `lua::Application`, which runs `source/main.lua`. A C++ app returns its own `core::Application`.

## Source layout

Public headers live in `engine/include/haylen/<module>/`, and the code that implements them lives in `engine/src/<module>/`, with the same folder names. The namespace of a context follows its folder. So `engine/include/haylen/core/Engine.hpp` declares `haylen::core::Engine`, and `engine/include/haylen/plugins/UiPlugin.hpp` declares `haylen::plugins::UiPlugin`.

Everything specific to 2D sits under a `2d` folder on both sides. Each folder under `2d` maps to a namespace with the suffix `2d`, which is also the name of its Lua module. So `engine/include/haylen/2d/graphics/Renderer.hpp` declares `haylen::graphics2d::Renderer`.

This table names the main classes of each folder. The headers of a folder list all of its public classes.

| Folder | What it holds |
| --- | --- |
| `core` | `Engine`, with the error screen it draws when a script fails, and `Application`. `Scene`, `SceneLoad`, `SceneView`, `LoadingView` and `SceneManager`, with the `TransitionEffect` interface. `ProcessMode`, `AppConfig`, `FrameClock`, `FrameQueue`, `JobSystem`, `TimerScheduler` and `MotionReduction`. The tweens: `Tween`, `PropertyTween`, `Timeline`, `TweenManager`, `TweenTrack`, `PropertyTrack`, `TweenProperty`, `TweenValue` and `TweenMotion`. `Signal` with `Connection`, `ScopedConnection` and `ConnectionScope`, and `EventBus` with `LifecycleEvent`. The containers `ObjectPool`, `RingBuffer` and `FloatBuffer`. `Log`, `Json` with `JsonValidator`, `JsonNumber` and `JsonBytes`, `MessagePack`, `Utf8` and `Version`. |
| `ai` | `StateMachine`, `BehaviorTree` with its `Blackboard`, `UtilitySelector` with `ResponseCurve`, and `InfluenceMap`, which Lua sees as `haylen.ai`. |
| `plugins` | The `Plugin` interface, `PluginRegistry` and every built-in plugin. |
| `math` | `Vec2`, `Rect`, `Insets`, `Color`, `Transform2D`, `FloatRange`, `Random`, `Noise2D`, `Easing` with `EasingCurve`, `Geometry` with `Circle`, `Segment`, `Polygon`, `Ray`, `Raycast` and `RayHit`, `MarchingSquares`, `Spline`, `Spring`, `ShuffleBag`, `WeightedChoice`, `PoissonDisk`, the filters `LowPassFilter`, `MovingAverage`, `MedianFilter`, `HampelFilter` and `KalmanFilter`, and the scalar helpers of `Math`. |
| `io` | `Package` with the `PackageReader` that reads one of its files in ranges, `MemoryPackage`, `PackageWatcher` and the package paths of `Path`. The folder, zip, composite and overlay packages and their readers are private: `DirectoryPackage`, `FileReader`, `ZipPackage`, `ZipArchive`, `ZipReader`, `CompositePackage`, `OverlayPackage` and `MemoryReader`. |
| `content` | The public part of the protected content of release builds, in `haylen/content/`: `Bootstrap`, which a release build compiles in with the identity of the app, its trusted public keys and its key provider, the `KeyProvider` interface with `EmbeddedKeyProvider`, which holds the sealed content keys of the app, and `CompositeKeyProvider`, which asks several providers in turn. `ContentManager` brings content to the device, with the `Delivery` of each file, the `PackSource` interface of stores and the `Error` of failures. Keys never reach Lua, so the bootstrap and the key providers have no Lua binding. |
| `content` (private) | The HPAK shards with `ShardWriter`, `ShardReader` and `ShardSet`, the virtual package of a manifest in `HpakPackage` and `HpakFileReader`, and `ReleasePackage`, which opens the app and content manifests of a release as one package. `ReleaseBuilder` builds the release of an app package into its two domains, `LuaCompiler` compiles Lua modules into the bytecode of releases, `ContentBuilder` builds the shards and catalog of one domain with updates that reuse earlier chunks, and `RecordCache` is the build cache of sealed records. `Compatibility`, the local store, the downloader, the updater and the update screen live here too. The folder `crypto/` holds `Digest`, `Hasher`, `Aead`, `ContentKey`, `KeyRing`, `SigningKey` and `VerifyingKey`. The folder `format/` holds the wire formats: `ChunkRecord`, `ShardHeader`, `ShardIndex`, `ShardReference`, `Catalog`, `CatalogWriter`, `Manifest`, `ChannelDescriptor`, `Chunker`, `Compression`, `BinaryReader` and `BinaryWriter`. The [content guide](../content/content.md) describes them. |
| `storage` | `UserStorage`, `SaveSlots`, `ResumeState` and `Preferences`. The save slots and the preferences check with the private `StoredJson` that what they write fits JSON text. |
| `assets` | `Manager`, which loads, caches and preloads assets in groups, with `GroupManifest`, the format of the groups. It loads the dependencies that an asset type names, such as the images of an effect file, through the cache before it finalizes the asset. |
| `gpu` | `Device`, the graphics device of the RHI of Haylen, and `Surface`, the window it draws into. The device covers the buffers, images of every type with their mip levels, samplers, programs and pipelines that the renderers create, the passes with their color, resolve and depth stencil attachments and the draws they record, debug groups, the copies of render targets to the CPU, the programs it prepares ahead, the `Capabilities` that say what it can do, the shader `Language` it runs, and the `Api` it draws through, whose names `kApiNames` lists. The private `NullDevice` is the device of the headless host. |
| `graphics` | The GPU layer for every dimension, over the `gpu::Device` of the backend: `Device`, with its shared white texture, the name of its GPU and the reads of render targets, `Texture` with mipmaps, `RenderTarget`, `BlendMode`, `Viewport`, `Gradient` and `Shader`, whose packages the private `ShaderResource` reads, checks and prepares ahead on the GPU device. `Image` fills, blits, crops, extrudes, bleeds and halves pixels, with `ImageFilters`, `ImagePalette` and `ImagePlaceholder`. `VectorImage` holds SVG images that rasterize at any scale. `Video` plays a video file into a texture with the `VideoDecoder` of the platform. |
| `text` | `Font`, the interface of the fonts that shape and lay text out for any renderer, with the signed distance field `TrueTypeFont` shaped by HarfBuzz and the `BitmapFont` of BMFont files and grids. `FontFamily`, `Style`, `Layout`, `Alignment` and `Direction`. Rich text: `RichText` with `RichTextDocument`, `RichTextOptions`, `RichTextRegistry` and `Effect`. The markup parser, the layout builder, the layout cache, the bidirectional paragraph, the segmenter and the Thai phrase breaker are the private `MarkupParser`, `LayoutBuilder`, `LayoutCache`, `BidiParagraph`, `Segmenter` and `PhraseBreaker`. |
| `input` | `Input` with `Touch`, `TouchPhase`, `GamepadState` and `InputDevice`. The enums `Key`, `MouseButton`, `GamepadButton` and `GamepadAxis` with their names in `Controls`, and `KeyModifiers`. `ActionMap` with `ActionMaps`, `VirtualInput`, `PointerEmulation`, `GestureRecognizer` with `Gesture`, `InputRecording` and `InputPlayback`, and `AppTextField`. |
| `audio` | `Mixer`, which plays voices and music through named buses, `Sound`, the effects `Delay`, `Filter`, `Limiter` and `Reverb` with their `Effect` interface, the audio `Session` and the `OutputSettings`. |
| `ui` | `Backend`, which runs Dear ImGui over the app, `Gui` with its `Placement` and `Event` records, `Context`, `Theme`, `Scaling`, `FocusNavigator`, the `AccessibilityTree`, and `Component` with `ComponentRegistry`, `PropertyReader` and `TextValue`. Collections are `Collection` with its cells, items, layouts and sources. The built-in components live one class per file in `src/ui/components/`, grouped by family. They draw through the private `Widgets`, `Typography`, `Surfaces` and `TextEditor` helpers, where `TextFieldLayout` places the caret of text fields by the shaped text. |
| `platform` | `Bridge`, the channel of JSON and byte buffers to native code. `VideoStream`, `AudioStream` and `PluginStreams`, the streams that the native parts of plugins feed. `AppPlugin`, the plugins of `app.json` as the engine sees them. `System` with `SystemInfo`, `Theme` and `Battery`, `Accessibility` with its settings and elements, and `MediaSession`. `Dialogs` with `DialogRequest` and `DialogResult`, and `Screens` with `ScreenRequest`, the screens of plugins that take over the app until they end with one result. `Event` with `TouchPoint`, and `Window` with `WindowPlacement`, `Monitor`, the `Fold` of foldable and dual-screen devices, and the `FoldSimulation` that replaces it. The public Apple headers of the bridge and of native plugins live in `apple/`. |
| `net` | `WebSocket`, with the Poco transport of native builds and the browser transport of the web build. |
| `conversation` | `Conversation`, the graph of a branching conversation read from `.conversation` files, `ConversationState`, one walk through it with its variables, visits, choices and saves, and `Expression`, the conditions and assignments of its steps. |
| `localization` | The `Catalog` of translated text, `PluralRules`, `NumberFormat`, `DateFormat` and `ListFormat`. |
| `debug` | `Profiler` with `ProfileScope` and `ProfileSample`, `TraceRecorder`, the `LogLine` records of the debug overlay, the `Stats` snapshot, the `StatsDisplay` that draws the compact statistics, `Monitor`, `Console`, `Breadcrumb`, and `ObjectCounter` with `TrackedObject` and `TrackedCount`, which count the objects of every type. |
| `lua` | The public C++ toolkit for Lua bindings, which the engine and the projects that extend it use, and `lua::Application`. |
| `2d/graphics` | `Renderer`, with its text and rich text drawing, vector images, captures, image blends and canvas overlays. `Sprite`, `SpriteBatch` with the `PartColors` of recolored sprites, and `StaticSpriteBatch`. `TileLayer`, which bakes grids of cells by chunks. `Stroke` with the private `StrokeBuilder`, `Mesh`, `Shape`, `Material`, `DrawOrder`, and groups and masks that render into pooled images. `Camera`, `CameraRig`, `PixelCanvas`, `Parallax`, `NineSlice`, `PostProcess` and `ImageBlend`. `SceneTransition`, the built-in scene transition effects. The private `VectorAtlas` keeps the rasters of vector images in shared pages. |
| `2d/animation`, `2d/particles`, `2d/lighting` | `Animation`, `Animator`, `SpriteAtlas` with the frame data of `SpriteFrame`, `AtlasBuilder`, which packs atlases at run time, `AnimatedBatch`, `StateGraph` with `StateGraphPlayer`, `Chain`, and the skeletons of `SkeletonData`, `SkeletonAnimation` and `Skeleton`. The particle `Emitter`, `System`, `Effect`, `Trail`, `Ribbon` and `ImageShape`. `Light` with `LightFlicker`, and `Occluder`. |
| `2d/physics` | The Box2D wrapper: the handles `World`, `Body`, `Shape` and `Joint`, `CollisionFilter`, contact and sensor events, and raycast hits. A handle keeps the generation of its world. So once that world is gone, the handle reports `isValid() == false` and throws `std::logic_error` naming the destroyed world, even after a new world takes its Box2D slot and hands out the same ids. A world of more than one thread runs the tasks of its steps on the job system through the private `StepTasks`. The helpers built on worlds are `Rope`, `Ragdoll`, `Vehicle`, `TopDownVehicle`, `Mover`, `Grabber`, `ForceField`, `Fluid`, `Terrain`, `Explosion`, `Fracture`, `PathPredictor`, `Raycaster` and `RayBatch`. Vehicles, force fields and fluids act in every step through the step hooks of their world. The [physics guide](../2d/physics.md) explains the system. |
| `2d/tiled` | Tiled maps: the `Map` model with its layers, objects and tilesets, `World` files, `MapRenderer`, which also builds maps at run time, `MapWriter`, which writes them as Tiled JSON, `MapQuery`, `AutoMapper`, `TileCollision`, which merges the collision of a tile layer into chain loops, and `ObjectFactories`. |
| `2d/nodes` | The retained tree of 2D nodes: `NodeTree` with its contiguous arrays, the `Node` handles that watch it, `NodeProperties`, `NodeContent`, `NodeSerializer`, which reads and writes descriptions, and `Prefab`. |
| `2d/navigation` | `Grid` with `GridSearch` and `HierarchicalPathfinder`, `Graph` with `GraphSearch`, `DijkstraMap`, `FlowField`, `NavMesh`, `SteeringAgent`, `Wanderer` and `Crowd`. |
| `2d/spatial` | The spatial structures `HashGrid`, `QuadTree`, `AabbTree`, `KdTree` and `OverlapWorld`, `ScreenPicker`, and the grid algorithms `CellGrid`, `GridRay`, `Bresenham`, `FieldOfView`, `VisibilityPolygon`, `FloodFill`, `ConnectedComponents` and `UnionFind`. |
| `2d/procedural` | `Scatter` over a `Region`, `CellularAutomaton`, `DrunkardWalk`, `Dungeon`, `Maze`, `WaveFunctionCollapse`, `Delaunay`, `Voronoi` and `Autotile`. |

Lua bindings sit next to the module they bind, such as `engine/src/2d/physics/Physics2DLua.cpp`. Plugins that apps construct or reference have public headers in `engine/include/haylen/plugins/`, such as `UiPlugin.hpp`. The other plugins keep their headers in `engine/src/plugins/`, such as `AudioPlugin.hpp`.

The folder `engine/src/lua/` holds the implementation of the binding toolkit and the parts of the Lua runtime of an app:

- `Environment` routes `require` to the `source/` folder of the package and to the `source/` folders of its plugins.
- `ScriptedScene` and `ScriptedLoadingView` stand behind Lua scenes and loading views.
- `Task` runs the cancellable coroutines of load hooks and `scene.spawn`.
- `Owners` ties listeners, tasks and GUIs to their owner.
- `Autoloads` keeps the autoloads of the app, and `haylen.jobs` lives here too.

### The platform folder

The folder `engine/src/platform/` holds the boundary with the operating system. These classes are shared by every platform.

| Class | What it does |
| --- | --- |
| `Host` | The interface between the engine and a platform, which [the host boundary](#the-host-boundary) describes. |
| `NativeHost` | The host of real devices, from which every backend derives its host. |
| `NativeWindow` | The native handles of the window of the backend. |
| `Player` | Plays packages for the backend, with `ReportedState`, what the platform reported last, and `MemoryWarning`. `LuaPlayer.cpp` is the source of the player. |
| `Services` | The class whose static methods every platform folder implements. |
| `BridgeRelay` and `PlatformLua` | Carry the answers of the bridge and bind it to Lua. `StreamsLua` binds the streams. |
| `SystemState` | The theme, the battery and the accessibility settings that the platform reports, with `SystemLua` and `AccessibilityLua`. |
| `DialogRelay` and `DialogsLua` | Carry the answers of native dialogs and bind them to Lua. |
| `AccessibilityRelay` | Carries the actions of screen readers. |
| `ScreenRelay` | Keeps the screen of a plugin that shows and carries the ends of screens. |
| `ConsoleRelay` | Carries the lines of the developer console that come from the development server or the web page. |
| `CrashGuard` | Captures the native crashes of the process into a report that the next start reads. |
| `DevelopmentSession` and `DevelopmentConnection` | Hold the state of [development](#development-and-hot-reload). |
| `NativeViews` | The edges that native views of plugins reserve and the covers of the app. |
| `PluginLoadOrder` | Orders the native parts of plugins from the package, where the platform reads them there. |
| `native/` | The native libraries, callbacks and C interface of `haylen.native`: `NativeLibraries`, `NativeSignature`, `NativeCallback`, `NativeCallbacks`, `NativeApi` and `NativeLua`. |
| `headless/` | The headless host and the headless player. |

Each platform has a folder of its own. These are its main classes, and each folder holds more classes for the details of its platform, such as text input, gamepads and desktop windows.

| Folder | Main classes |
| --- | --- |
| `apple/` | `AppleServices.mm`. `AppleSystem`, `AppleTheme`, `AppleAccessibility` and `AppleBattery`, with what the device is and reports. `AppleVideoDecoder` with `HaylenVideoLoader`, which decode videos with AVFoundation. `HaylenAccessibilityView` with its `HaylenAccessibilityElement` objects, which serve the elements of the UI to the screen readers of Apple platforms. `AppleSplash`, which keeps the launch screen over the window of iOS, tvOS and Mac Catalyst apps until the splash ends. `AppleDialogs` with `HaylenDialog`, and `ApplePresenter`, which presents from the topmost view controller. `AppleBridge` behind `HaylenBridge`. The native plugin host: `ApplePlugins`, which the application delegates of the backend hand the events of the app, `AppleNotifications`, which reaches the notification center through the Objective-C runtime alone, the plugin contexts with the `HaylenRequirements` that check the project of the app, the overlay, the streams `HaylenVideoStream` and `HaylenAudioStream`, and the screens of `AppleScreens` with `HaylenScreen`, its container `HaylenScreenController` and `HaylenScreenSceneDelegate`, the scenes of screen windows. They stand behind `HaylenPlugin.h`, `HaylenNotificationPlugin.h` and `HaylenRequirements.h`. `AppleRuntime` follows whether an app runs, and `CatalystInput` serves Mac Catalyst. |
| `android/` | `AndroidServices.cpp` with its JNI exports. `AndroidActivity` with the GameActivity of the app, `AndroidAssetPackage`, `AndroidGamepads` with the state of the controllers in `AndroidGamepadStates`, `AndroidKeys` with the back button, `AndroidTextInput` and `JavaBridge`. `AndroidDeviceInfo` and `AndroidBatteryStatus` read what the device is and reports. `AndroidGlesVersion` holds the OpenGL ES version that the context asks for. `AndroidDialogJson` holds the JSON of the dialogs of the Java side, and `AndroidStreams` stands behind the streams of Java plugins. `AndroidVideoDecoder` decodes videos with MediaExtractor and MediaCodec, with `AndroidYuvImage`, which turns their pictures into RGBA. |
| `web/` | `WebServices.cpp`, `WebPage` with the exports that the page calls, `WebVideoDecoder`, which plays videos in video elements of the page, `WebDialogJson` with the JSON of the dialogs of the page, and `BrowserWebSocket`. |
| `windows/` | Its services with `WindowsSystem`, `WindowsPowerStatus` and `WindowsText`. `WindowsVideoDecoder` with `WindowsPackageStream`, which decode videos with Media Foundation. `WindowsDialogs`, which shows the dialogs on a thread of their own. |
| `linux/` | Its services with `LinuxSystem` and `LinuxSystemReader`, which reads the files of the system. `LinuxGlib`, which loads GIO at run time. `LinuxSettings`, which follows the color scheme and the accessibility settings of the settings portal. `LinuxDialogs` with GTK 3 loaded at run time, `LinuxFilePattern` and `LinuxGamepads`. |

The classes `PluginLoadOrder`, `WebDialogJson`, `WindowsPowerStatus`, `LinuxSystemReader`, `LinuxFilePattern`, `AndroidDeviceInfo`, `AndroidBatteryStatus`, `AndroidDialogJson`, `AndroidGamepadStates`, `AndroidGlesVersion` and `AndroidYuvImage` compile on every platform into `haylen-platform`, so every host tests them.

### The other folders

- **The content tool.** The tool `haylen-content` lives in `engine/tools/content/`, in the namespace `haylen::content`. `ContentTool` is its command line, `KeyStore` the key folder of an app, and `SecureRandom` reads the random generator of the system. `ReleaseInspector` inspects, verifies and compares releases, and `ChannelPublisher` publishes content to update channels and compacts publication trees. The library `haylen-content-tool` holds them for the tool and the tests, on top of `haylen-content-writer`. Only desktop hosts build it, since releases are built there.
- **Backends.** The folder `engine/src/backends/` holds one folder per backend, such as `sokol/`, which [backends](#backends) describes.
- **Shaders.** The folder `engine/shaders/` holds the sources in the [Haylen shader language](../graphics/shaders.md), compiled at build time, with the shader library in `include/haylen/` that app shaders include too.
- **Native projects.** The folder `engine/platform/` holds the Gradle project of the Android libraries, the web shell with `haylen-runtime.js`, and the Apple `Info.plist` and launch screen templates. Its Android module `haylen` hosts apps in `HaylenActivity`, a GameActivity of the AndroidX games libraries, next to `HaylenLinkActivity` for links and the plugin host with its overlays and requirements. Its modules `haylen-plugins`, `haylen-links` and `haylen-coroutines` bring the provider of plugins, the link activity and the suspending handlers to the apps that want them.
- **The rest.** The folders `engine/cmake/`, `engine/tests/` and `engine/bench/` hold the CMake modules, the tests and the benchmarks.

### Dependencies

Every dependency is pinned with a SHA-256 hash in `engine/cmake/haylen-dependencies.cmake`. The engine declares nlohmann/json before Varn, so both share one copy. Varn brings Lua, libuv, Poco, OpenSSL, libzip and zlib, which the engine reuses. The engine adds Sokol, which only the Sokol backend uses, Dear ImGui, Box2D, miniaudio, zstd, Monocypher, stb, the core of msdfgen, HarfBuzz, SheenBidi, libunibreak, the Thai model of BudouX and, for the tests, GoogleTest.

## Plugins

Every subsystem of the engine is a plugin. A plugin derives from `haylen::plugins::Plugin` in `haylen/plugins/Plugin.hpp`, returns its name from `getName`, and overrides the hooks it needs. Every hook has an empty default and runs on the frame thread. A plugin wires its subsystem into the engine and installs its Lua modules, while the subsystem itself stays in its own context folder.

| Hook | When it runs |
| --- | --- |
| `start(core::Engine&)` | Once, when the engine starts or when the plugin is added to a running engine. |
| `installLua(core::Engine&, lua_State*)` | Right after `start`. It registers Lua modules with `lua::Binding::preload`, which adds them to `package.preload` through the Varn runtime. |
| `event(core::Engine&, const Event&)` | For every platform event, the lifecycle events `Suspended`, `Resumed`, `Resized`, `FocusGained`, `FocusLost`, `QuitRequested` and `LowMemory` included. |
| `beginFrame(core::Engine&, float)` | At the start of every frame that is not in the background, after asynchronous results arrive and before any update. |
| `fixedUpdate(core::Engine&, float)` | Zero or more times per frame, once per fixed step. |
| `update(core::Engine&, float)` | Once per frame, with the scaled frame time. |
| `render(core::Engine&)` | After the scenes of the current view draw the world. |
| `renderUi(core::Engine&, const core::SceneView&)` | After the scenes of each view of the frame draw their UI, once per view. So a plugin draws what belongs to the scenes of the view, and most plugins draw only in the current view. |
| `renderOverlay(core::Engine&)` | Once per frame, above everything else, scene transitions and the error screen included. It suits debug displays and the update screen of content. |
| `endFrame(core::Engine&)` | After the frame is submitted, even while the error screen shows. |
| `isCapturingBack()` | Asked when the player presses the back button of the platform. A plugin that returns `true` takes the press, so it never leaves the app, such as the UI while a popup is open. |
| `packageChanged(core::Engine&, paths)` | In development, after the assets read from the changed paths of the package reloaded. A plugin then rebuilds what it built from files outside the asset cache, such as the texts of localization and the themes of the UI. |
| `stop(core::Engine&)` | Once, when the engine stops, in reverse order of registration. |

The class `plugins::BuiltInPlugins` in `engine/src/plugins/BuiltInPlugins.cpp` registers the built-in plugins in the order they depend on each other. They use exactly the interface that other projects use, and their names follow the Lua modules they install.

| Plugin | Lua modules | Other duties |
| --- | --- | --- |
| `core` | `haylen`, `haylen.log`, `haylen.viewport`, `haylen.window`, `haylen.collections`, `haylen.timer`, `haylen.scene`, `haylen.signal`, `haylen.events`, `haylen.math`, `haylen.tween`, `haylen.ai` | Runs the autoloads of the app. |
| `jobs` | `haylen.jobs` | Resumes Lua jobs within the budget of the frame during `update`. |
| `input` | `haylen.input` | |
| `graphics2d` | `haylen.graphics`, `haylen.graphics2d`, `haylen.lighting2d` | |
| `assets` | `haylen.assets` | |
| `content` | `haylen.content` | Starts the content manager, which holds the start of the app while its required content downloads, drives its downloads every frame, and draws the update screen while the app waits. |
| `text` | | Owns the registry of the effects and icons that rich text markup names, and registers the asset types `bitmapFont` and `gridFont`. |
| `animation2d` | `haylen.animation2d` | Registers the asset types `atlas` and `skeleton`. |
| `particles2d` | `haylen.particles2d` | Registers the asset types `particles` and `imageShape`. |
| `audio` | `haylen.audio` | Registers the asset type `sound`. |
| `video` | `haylen.video` | Opens the videos of the app with the decoder of the platform, and keeps each one current at the start of every frame until it is released. |
| `physics2d`, `tiled`, `nodes2d`, `spatial2d`, `procedural2d`, `navigation2d` | `haylen.physics2d`, `haylen.tiled`, `haylen.nodes2d`, `haylen.spatial2d`, `haylen.procedural2d`, `haylen.navigation2d` | `tiled` registers the asset types `tiled`, `tiledWorld` and `tiledRules`, and `nodes2d` registers `prefab`. |
| `localization` | `haylen.localization` | Rebuilds the texts of a changed folder in development. |
| `conversation` | `haylen.conversation` | Registers the asset type `conversation`. |
| `storage` | `haylen.storage`, `haylen.preferences` | Loads the preferences on start, and writes them on suspend and stop. |
| `debug` | `haylen.debug` | Draws the compact statistics with the renderer and the full debug overlay with ImGui, which F3 cycles by default. It runs the debug drawings over every canvas through a canvas overlay of the renderer, which F4 switches by default. It samples monitors and publishes object events. Plugins add drawings with `DebugPlugin::addDrawer`, as the `physics2d` plugin does for the worlds that Lua makes. |
| `hotReload` | `haylen.hotReload` | Applies the changes of an app in development, as [development and hot reload](#development-and-hot-reload) describes. |
| `ui` | `haylen.ui`, `haylen.imgui` | Starts the ImGui frame in `beginFrame`. At the start of the first `renderUi` of a frame, before any GUI draws, it lets the collections of mounted GUIs bind their cells and run their Lua binders. It draws mounted GUIs in `renderUi`: the GUIs of the scenes of each view into the image of the view, a leaving view in an ImGui window of its own that draws before the frame ends and takes no input, and the GUIs of no scene with the current view. |
| `net` | `haylen.net` | Delivers WebSocket events in `beginFrame`. |
| `platform` | `haylen.platform`, `haylen.system`, `haylen.dialogs`, `haylen.accessibility` | Connects the bridge relay of native code and the relay of native dialogs to the running app. Keeps the textures of the video streams that the app draws current at the start of every frame. |
| `native` | `haylen.native` | Keeps the Lua functions of the native callbacks of the app, and drops them when it stops. |

### Adding a plugin

A project adds a plugin of its own from its `Application`. `Engine::addPlugin` registers the plugin. When the engine already runs, which is always the case inside `Application::start`, it calls `start` and `installLua` right away.

```cpp
class App final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.addPlugin(std::make_unique<ScorePlugin>());
    }
};
```

Plugins added this way run after the built-in ones in every phase and stop before them. Code finds a plugin in two ways:

- `engine.getPlugin<ScorePlugin>()` throws `std::logic_error` when the plugin is missing.
- `engine.getPlugins().find<ScorePlugin>()` and `engine.getPlugins().find("score")` return a null pointer when it is missing.

Plugins talk to each other only through public methods, never through the internals of another plugin. The [Lua guide](lua.md#extending-the-engine-from-c) shows a complete plugin with a Lua module of its own.

## The host boundary

The portable engine reaches the platform only through `platform::Host`, declared in `engine/src/platform/Host.hpp`. `Host` extends the public `platform::Window` interface, which covers the framebuffer size, the DPI scale, fullscreen, the title, the cursor, the mouse lock, the on-screen keyboard, the clipboard, quit requests, the orientation, the fold, the status bar, text input, transparency and the modes of desktop windows. `Host` adds what the engine needs to run.

| Method | Purpose |
| --- | --- |
| `getPlatformName()` | The name that the field `platform` of the root `haylen` Lua module reports, such as `macos` or `web`. |
| `createGpuDevice()` | The `gpu::Device` of the backend, which draws into the window of the host and which the `graphics::Device` of the engine owns. One exists while an app runs. |
| `getAudioSetup()` | Whether the `audio::Mixer` opens a device, and the backend that its device plays through where the platform needs one of its own, such as the audio output of the page in browsers. |
| `isFrameTransparent()` | Whether the window composes the alpha of every frame with the desktop. The renderer reads it when it submits a frame. |
| `getUserDataDirectory(identifier)`, `persistUserData()` | Where `storage::UserStorage` keeps the files of an app, and how the platform makes them durable. |
| `getSafeAreaInsets()` | The area of the screen that UI must avoid, in framebuffer pixels. |
| `getStatusBarHeight()` | The height in framebuffer pixels of the status bar that shows over the top of the app, or `0` while none shows. |
| `pollGamepads(span)` | Fills the gamepad states once per frame. |
| `dispatchPlatformCall(id, method, params, buffers)`, `cancelPlatformCall(id)` | Hands a bridge call to native code, with the byte buffers of its parameters, and tells native code that the app gave the call up. |
| `getReservedInsets()` | The edges of the screen that native views of plugins reserve: the largest reservation on each edge, in framebuffer pixels. The engine adds them to the safe area. |
| `isAppCovered()` | Whether native UI of plugins covers the app, which makes it inactive, halted and muted. |
| `getNativePlugins()` | The ids of the plugins whose native part the platform loaded. `platform.plugins()` reports them with the ones that native libraries declare. |
| `reportError(report)` | Hands the JSON report of the error that stopped the app, `{message, file, line, traceback, frames}`, to native code. Native code passes it to the page on the web and to the native parts of plugins elsewhere. The engine hands the same report to the error handlers of native libraries itself. |
| `getSystemInfo()` | What the device and its operating system are, as a `SystemInfo`. The engine reads it once per app and adds the name of the GPU of its graphics device. |
| `isAccessibilityTreeWanted()`, `updateAccessibility(elements, focused)`, `announce(text, interrupt)` | Whether the platform wants the tree of accessible elements of the UI, the tree itself whenever it changed, and the announcements of the app, for the screen readers of the platform. Screen readers act on the elements through `AccessibilityRelay`. |
| `openVideo(file, path)` | A `VideoDecoder` of a video file of the package for the decoder of the platform, opened on a worker of the I/O pool, or null where the platform decodes no video. |
| `setNowPlaying(value)` | Shows what the app plays in the now-playing and lock-screen controls of the system, where the platform has them. The commands of those controls, the media keys and headset buttons reach `MediaSession::receive`. |
| `getTheme()`, `getBattery()`, `getAccessibilitySettings()` | The theme, the battery and the accessibility settings as the platform last reported them. The engine reads them once per frame to publish `systemThemeChanged`, `batteryChanged` and `accessibilitySettingsChanged`. |
| `openUrl(url, callback)`, `vibrate(seconds)`, `rumbleGamepad(index, low, high, seconds)` | Open a url with the app that the system picks, calling back once from any thread with whether an app took it. Vibrate the device, and rumble the motors of a gamepad, where they can. |
| `getNetworkRequirement()` | The sentence that the network errors of the engine end with while the project of the app lacks what network access needs. Only Android reports one, for an app without the permission `INTERNET`, and the engine hands it to the net plugin when an app starts. |
| `showDialog(id, request, folder)`, `cancelDialog(id)` | Show a native dialog, and close a dialog that the app gave up. The platform answers once, from any thread, through `DialogRelay::resolve` with the same id. It copies picked files that have no path of their own into `folder`. |
| `openScreen(request)`, `cancelScreen(id)` | Open the screen of a plugin over the app once the engine covered it, and dismiss a screen that the app gave up. The platform ends the screen once, from any thread, through `ScreenRelay::finish` with the same id. It keeps the id, the plugin, the screen and the state where they survive the end of the process, as the [plugin guide](../platform/plugins.md#the-native-contract) describes. |
| `guardCrashes(report)`, `setCrashContext(json)` | Capture the native crashes of the process into a report file, and publish the context that the report of a crash carries. |
| `getDevelopmentSession()` | The [development session](#development-and-hot-reload) of a process that plays an app while it is being made, or null for every app that ships. |
| `isLaunched()` | Whether another app of the process launched the app that plays. |

### The native host and the player

The target `haylen-runtime` implements the host with `platform::NativeHost` (`engine/src/platform/NativeHost.hpp`). It forwards to the `Services` of the platform folder compiled into it. The host of the backend derives from it and adds what its window does, as [backends](#backends) describes.

Some state belongs to the process and outlives the apps that restart under it. The platform services update it from any thread.

- The reservations and covers of the native host live in a `NativeViews`.
- The theme and the battery live in a `SystemState`.
- The screen of a plugin that shows lives in the `ScreenRelay`, and the covers of native libraries live in `NativeApi`.
- The host reads the `SystemInfo` of the platform services once per process.

The entry point of the backend hands the command line to `platform::Player` (`engine/src/platform/Player.hpp`). The player then does these steps:

1. It opens the package named on the command line, or the one bundled with the app, and turns on development mode only for `--dev`.
2. It reads `app.json` and calls `Application::create()` and `Application::configure`.
3. It keeps the configuration that the backend opens the window with, which `Player::getOpeningConfig` returns.
4. It creates and drives the `Engine`.

A package that cannot be opened, or an `app.json` that fails validation, starts a stand-in application that shows the error on the screen. The process keeps the status that the last app quit with, or `1` when the engine could not start, and a desktop process exits with it. With the Sokol backend the entry point returns it on Windows and Linux, and `HaylenAppDelegate` ends a macOS app with it, because AppKit would end the process with `0`.

### The headless host

The target `haylen-headless-host` implements the host with `HeadlessHost` for tests. It draws through `gpu::NullDevice`, which checks every call of the engine against the contract of the GPU device and draws nothing. It uses a mixer without a device and a user data folder that the test picks. It records bridge calls, dialogs, screens, opened urls, vibrations and error reports instead of dispatching them. Its own `NativeViews`, native plugin list, system info, theme, battery and network requirement stand in for the platform. The [testing guide](../tools/testing.md) describes it in detail.

### Services of the platform

- **The bridge.** Platform services such as sign-in or purchases go through `platform::Bridge`, with JSON requests and replies that carry byte buffers next to their JSON, as the [platform bridge guide](../platform/platform-bridge.md) describes.
- **Streams.** Video and audio that native code produces all the time, such as a camera or a microphone, go through the streams of `platform::PluginStreams`, which belong to the process. A `VideoStream` keeps the newest frame that any thread pushed. An `AudioStream` holds a lock-free ring that one thread writes and the voice that plays it reads, as the [plugin guide](../platform/plugins.md#streams) describes.
- **System and dialogs.** What the device is, its theme and battery, opening urls and vibrating are engine services of `platform::System`, reached with `engine.getSystem()`. Native message boxes and file pickers belong to `platform::Dialogs`, reached with `engine.getDialogs()`. It checks each request, gives it an id unique in the process, keeps its callback, timeout and cancellation, and hands it to the host. Both deliver every answer on the frame thread, as the references of [`haylen.system`](../lua-api/system.md#c) and [`haylen.dialogs`](../lua-api/dialogs.md#c) describe.
- **Screens.** The screens of plugins, such as a paywall or a sign-in page, go through `platform::Screens`, reached with `engine.getScreens()`. It opens one screen at a time while the app is active, and covers the app before the platform shows the screen. It routes the end of the screen to the call, or to the next app when the app that opened it is gone, as the [plugin guide](../platform/plugins.md#plugin-screens) describes.

## Backends

A backend opens the window, runs the frames, draws them on the GPU and holds the entry point of the process. The engine, the player that plays packages, the native host, the services of each platform and the Lua API are the same for every backend. So an app never depends on the backend that runs it. The engine ships the Sokol backend in `engine/src/backends/sokol/`, which opens the window with `sokol_app` and draws with `sokol_gfx`.

| Layer | Where | What it does |
| --- | --- | --- |
| Engine | `haylen-engine` | The C++ API and the Lua bindings, which reach the platform through `platform::Host` and draw through `gpu::Device`. |
| Player | `platform::Player` in `engine/src/platform/Player.hpp` | The runtime that every backend shares. It reads the command line, opens the package it names or the bundled one, creates the engine, and replaces it whenever the app restarts or launches another. It hands the app the events of the window and the platform, routes the back button of the platform and ends the splash screen. |
| Native host | `platform::NativeHost` in `engine/src/platform/NativeHost.hpp` | The `Host` of real devices. It forwards to the `Services` of the platform folder compiled into the runtime, and keeps the state that outlives the apps of the process, such as `NativeViews` and `SystemState`. |
| Native window | `platform::NativeWindow` in `engine/src/platform/NativeWindow.hpp` | The window of the backend as the system sees it. The services of a platform read it to place dialogs, text fields, views, menus and accessibility over the app. It is an `NSWindow` on macOS, a `UIWindow` on iOS, iPadOS, Mac Catalyst and tvOS, an `HWND` on Windows, an X11 `Window` and its `Display` on Linux, and an `ANativeWindow` and its `GameActivity` on Android, with the size and scale of the framebuffer. |
| Backend | `engine/src/backends/<name>/` | The window, the frames, the events, the GPU device and the entry point. |

A backend implements three interfaces of the engine and holds a runtime that drives the player:

1. **A host** derived from `platform::NativeHost`, which adds what its window does: the size and scale of the framebuffer, fullscreen, the title, the cursor, the mouse lock, the clipboard, whether the window can be transparent, closing the window, its focus, and `createGpuDevice()`, which returns the GPU device of the backend.
2. **A GPU device** that implements `gpu::Device` (`engine/include/haylen/gpu/Device.hpp`). It creates the buffers, images, samplers, programs and pipelines that the renderers of the engine use, records their passes and draws, and writes dynamic buffers and images. It copies render targets back to the CPU with `readImage`. Where its graphics API compiles programs when they are made, `prepareProgram` starts compiling them ahead. It reports the shader `Language` it runs, the graphics `Api` it draws through, the name of its GPU, its limits and its pools. Its methods run on the frame thread, except `prepareProgram`, which may run on any thread. Methods that create an object throw `std::runtime_error` with a clear sentence when the device has no room for it or rejects it.
3. **A window** derived from `platform::NativeWindow`. The backend installs it with `NativeWindow::install` when its window opens and removes it when the window closes.
4. **A runtime** that owns the entry point and drives the player:
    - It calls `Player::create` with the command line and its host.
    - It opens the window with the configuration that `Player::getOpeningConfig` returns, installs its window and calls `Player::initialize`.
    - It calls `Player::frame` with the seconds since the last frame.
    - It turns the events of its window into `platform::Event` values for `Player::handleEvent`.
    - It asks `Player::routeBack` where a press of the back button of the platform goes.
    - It calls `Player::cleanup` when the window closes.

### Adding a backend to the build

The folder of a backend holds a `backend.cmake`, which `engine/CMakeLists.txt` includes once it defined the target `haylen-runtime`. The cache path `HAYLEN_BACKEND_DIR` names that folder, `engine/src/backends/sokol` by default, and the configuration stops when the folder holds no `backend.cmake`. The backend is named after its folder, such as `sokol`. `engine/cmake/haylen-platform.cmake` sets that name as `HAYLEN_BACKEND`, and the SDK records it in `haylen-config.cmake`.

The file `backend.cmake` adds the sources of the backend to `haylen-runtime` with `target_sources` and links what they need. It also sets the cache variables `HAYLEN_BACKEND_TEST_SOURCES` and `HAYLEN_BACKEND_TEST_LIBRARIES`, the sources and libraries of the tests of the backend, which the test suite of the engine compiles into `haylen-tests`. The graphics API comes from `HAYLEN_GRAPHICS_API`, as the [build guide](../tools/build.md#cmake-options) describes, and `HAYLEN_GRAPHICS` holds it with `AUTO` resolved, for the backend to read.

The device of the headless host, `gpu::NullDevice` in `engine/src/gpu/`, implements the same contract without a GPU and checks every call against it. So the tests of the engine catch a renderer that breaks the contract that every backend relies on, as the [testing guide](../tools/testing.md#the-headless-host) describes. The test `BackendIsolationTest` of `tools/test_haylen.py` keeps the layers apart: no Lua binding includes a backend, a host or a platform folder, and no file outside the folder of a backend and its tests includes the headers of a backend or calls its library.

### The Sokol backend

The Sokol backend is made of these classes. The C++ ones live in the namespace `haylen::sokol`.

| Class | File | What it does |
| --- | --- | --- |
| `Host` | `Host.hpp` | The host, whose window `sokol_app` opens. |
| `GpuDevice` | `GpuDevice.hpp` | The GPU device on `sokol_gfx`. It works with `Adapter`, which names the GPU, `ImageReadback`, which copies render targets back through the graphics API of the build, and `Precompiler`, which compiles the sources of programs ahead on the I/O pool through Metal on Apple platforms. Sokol keeps one global context, so a second device throws `Only one graphics device can exist at a time.`. |
| `Runtime` | `Runtime.hpp` | The `sapp_desc` of the window, its callbacks and the entry points: `haylen_main` on Apple platforms, where `sokol_app` leaves `main` to the app, `sokol_main` on Android and the web, `WinMain` on Windows and `main` on Linux. |
| `Window` | `Window.hpp` | The native window of `sokol_app`. |
| `Events` | `Events.hpp` | Turns the events of `sokol_app` into engine events. `engine/tests/backends/sokol/EventsTests.cpp` tests it. |
| `HaylenAppDelegate`, `HaylenSceneDelegate` and `AppleClasses` | `apple/` | The application delegates of macOS and of iOS, tvOS and Mac Catalyst, which derive from those of `sokol_app` and hand the events of the app to native plugins, and the class that names them and the view controller of the window to `sokol_app`. |

## The frame

`Engine::frame(frameSeconds)` in `engine/src/core/Engine.cpp` runs one frame in this order.

1. **Covers.** The app takes the covers of the host and of native libraries, and the screen of a plugin that shows. So native UI of plugins that started or stopped covering the app makes it inactive, halted and muted, or brings it back. A screen that the app asked for in the frame before reaches the platform once the cover holds.
2. **Viewport and input.** The viewport updates from the framebuffer size, the design size, the scaling policy and the safe area, widened by the edges that native views reserve. The host fills the gamepad states, and the action map updates from every device. The action map is blocked while the app is halted or a scene change holds input back, so held actions read as up until they are released.
3. **Clock.** The frame clock advances with the frame time, cut to `maxFrameTime` and scaled by the time scale, or by zero while the app is halted. Touches and gestures update.
4. **Recovery and start.** An app that leaves the error screen publishes `appRecovered` here. An app whose start waited for its holds starts in the first frame after they end, as [`Engine::holdStart`](#resource-ownership) describes.
5. **News of the platform.**
    - The engine publishes changes of fullscreen, orientation, safe area, the fold and gamepad connections on its `EventBus`.
    - Unless the app is in the background, the asset manager finalizes the assets that finished decoding and creates their GPU objects. It goes on until the upload budget of the frame runs out, and it finalizes at least one per frame.
    - `Bridge::pump()` delivers bridge replies and native events, with the batched events of a name as one list. `System::pump()` delivers the answers of opened urls and publishes the changes of the theme and the battery. The media session and the accessibility of the app deliver their commands and changes, and tweens learn whether the system asks for reduced motion. `Dialogs::pump()` delivers the answers of native dialogs, their cancellations and their timeouts. `Screens::pump()` delivers the ends of screens, their cancellations and their timeouts, and sends the end of a screen that an earlier app opened as the retained event `<plugin>.screenRestored`.
    - All of this happens before the event loop runs, and the engine applies the cover of the app again after it. So a task that awaits a reply resumes in the frame that the reply reaches, and sees the app uncovered once the screen that answered it ended.
6. **The event loop.** `Runtime::poll(budget)` advances Varn's event loop within a budget of a quarter of the frame time, between 1 and 8 milliseconds. Promises settle, `async` tasks and scene tasks resume, Varn timers fire, HTTP and socket callbacks run, and completions posted from worker threads run, such as decoded assets that join the upload queue. The loop works in passes. Each pass takes the jobs that were queued and the timers that were due when it started, and then serves every socket until it would block. Passes repeat while they make progress and the budget lasts. So a task that awaits a promise settled during a pass, or a timer of zero milliseconds, resumes in the next pass of the same frame, or in the next frame once the budget is spent. A poll overruns its budget by at most one job and one pass over the sockets, so tasks that wake in great numbers spread over frames instead of holding one up. `Engine::getEventLoopPoll` keeps what the poll reported, which the debug overlay and `debug.stats()` show.
7. **`beginFrame`.** Unless the app is in the background, every plugin runs `beginFrame`. There the platform plugin uploads the newest frame of every video stream that the app draws into its texture.
8. **Fixed steps.** For each step the clock owes, the scenes run `fixedUpdate`, fixed-step tweens advance, and then every plugin runs `fixedUpdate`.
9. **Updates.** App timers, tweens, plugin `update` and the scene update run, in that order. Inside the scene update, loads that finished or failed settle, the pending scene change moves through its phases, the loading view updates and the top scene runs `update`.
10. **Audio.** The audio mixer updates with the real frame time, also while the app is halted.
11. **Drawing.** The renderer begins the frame. `SceneManager::prepareViews` works out the views of the frame, the `core::SceneView` values that plugins read through `getViews`. For each view, the scenes and the loading view draw the world, the plugins run `render` in the current view, the scenes and the loading view draw their UI, and the plugins run `renderUi` with the view.
    - During a scene transition, the scenes before and after the change do this inside a capture of their own image. The plugins draw with the current scenes, the UI draws the GUIs of each scene into its image, and the view of the effect draws the effect on the screen.
    - While the transition holds the covered screen for a load, no scene draws, and the loading view and the plugins draw over the effect.
    - The error screen draws instead of the scenes while it shows. The engine then paints the background of the status bar, and every plugin runs `renderOverlay`.
12. **Submit.** The renderer submits the whole frame to the GPU device, which draws it into the window of the host. Every plugin then runs `endFrame`.
13. **The end of the frame.** The `FrameQueue` runs the calls posted during the frame, such as queued events and deferred signal slots. The graphics device destroys released GPU objects, input closes the frame by clearing its pressed and released edges, and the profiler closes the frame.

A halted app, which the lifecycle options decide by the app state, skips steps 8 and 9 and advances the clock by zero. An app in the background, or under an opaque screen of a plugin, also skips drawing, like a covered app once it drew its covered frame, as the [lifecycle guide](lifecycle.md) explains. The game pause of `Engine::setPaused` gates scenes, autoloads, timers and tweens by their `ProcessMode` inside those steps.

### Events between frames

Platform events arrive between frames through `Engine::handleEvent`. The mouse and the fingers first reach the app as the device that the app asked for, which may turn one event into several, and the scroll speed of the app scales every scroll. Input then converts positions to design coordinates. Lifecycle events update the app state and emit the engine signals `appStateChanged`, `resized`, `quitRequested` and `lowMemory`, with the matching events on the `EventBus`. Then every plugin and the top scene receive the event, even when a listener of the engine event failed. So a plugin that keeps data, such as the preferences, still saves it when the app goes to the background.

### Errors

An exception that app code throws during an update, a draw, the submission or an event goes to `Engine::reportError`. The engine then does these things:

1. It keeps the first error as a `lua::Error` and logs its report as plain text.
2. It hands the JSON report to the host and to the error handlers of native libraries.
3. It emits `errorRaised` and queues the event `appError`.
4. From then on it skips the fixed steps, the updates, scene events and the draw hooks of scenes and plugins, and draws the error screen instead.

Varn's event loop, the bridge, `beginFrame` and `endFrame` keep running, so the hot reload plugin and the web page can restart the app.

The engine records where every error came from through `Engine::PhaseScope`. The start of the app, scene hooks that load, enter, exit, unload, pause or resume, autoloads that start or stop, and tasks that failed run in the phase `Lifecycle`. An error that leaves such a scope counts as a lifecycle error wherever the engine reports it. Frame code and reloads run in the phases `Frame` and `Reload`.

The engine leaves the error screen in three ways:

- `Engine::clearError` leaves the error screen of an error that is not of the lifecycle at once. Hot reload calls it once a reload fixed the code.
- `Engine::recover` leaves the error screen at the start of the next frame. The error screen of an app that `Engine::setRecoverable` made recoverable calls it. The engine publishes `appRecovered` with the error before the updates, and the app runs on.
- `Engine::requestRestart` asks for a restart from a hook. The runtime replaces the engine after the frame, when no engine code is on the stack.

### The error screen

Every protected call of the engine runs with the message handler of `lua::Runtime`. The handler reads the stack of the failing coroutine with `lua_getstack` and `lua_getinfo` and turns the error into a `lua::Error`: the message, the script position that Lua put in front of it, and the frames from the innermost call outward. Each frame has its source, line, kind (Lua, C or main chunk) and function name as Lua describes it.

- A function reads the way the calling code names it, such as `global 'error'`, `upvalue 'spawn'` or `method 'hit'`, and a main chunk reads `main chunk`.
- A Lua function without a name, such as a callback that native code calls, reads `function <file:line>`, with the line where it is defined. A native function without a name reads `?`.
- A stack deeper than 21 levels keeps its innermost 10 and its outermost 11 around one frame that reads `... N levels skipped`. So a runaway recursion reports quickly.
- The handler leaves out the levels that belong to the engine: the native function of `Runtime::protectedRun` and the body of engine tasks.

Varn hands every failure that no caller receives to the one handler of `async.onFailure`. `lua::Environment` sets that handler before any app code runs. The [Lua guide](lua.md#failures-nobody-hears) lists these failures. Among them, the rejections of promises that nothing awaited or caught reach the handler with the kind `rejection`, and the engine reports them with a message that says so. A to-be-closed variable of a cancelled task fails after its stack is gone, so its error comes without frames.

The failed stack of such a failure is gone by the time the handler runs. So the engine builds the frames from the list of frames that comes with the error, which holds the fields that `lua_getinfo` fills for each level, without the native entry of Varn that ran the task or the callback. A task error shows the stack of the task only. Both paths build their frames with the same code of `lua::Runtime`, so the frames of a task read exactly like those of a protected call. Varn keeps the innermost 32 and the outermost 32 levels of a stack deeper than 64, around one frame that counts the levels it left out, which becomes the same `... N levels skipped` frame.

`Error::getTraceback` renders the frames as aligned columns without tab characters, and `Error::toJson` adds the frames as a list for the web page.

The class `core::ErrorScreen` draws the error with the renderer alone, so it works even when the UI is what failed. It shows the name and version of the app, the platform and the version of the engine, the message wrapped to the safe area, the file and line, the lines around the error line when the error points into a file of the package, and the stack. The [Lua guide](lua.md#using-the-error-screen) describes its buttons, keys and scrolling. Gamepads reach the screen through `ErrorScreen::update` once per frame, and TV remotes also send arrow keys and Enter. In development, a hint says whether saving a fix resumes the app or restarts it. The log receives the same report that the clipboard does.

## Threads and async

The frame thread owns the GPU, the Lua state, scenes, UI, audio control and physics. It never blocks.

`JobSystem`, reached with `engine.getJobs()`, runs work in the background on the Varn runtime that the engine owns.

| Method | Where the work runs |
| --- | --- |
| `post(work)` | Varn's `taskPool()`, for CPU work. |
| `postIo(work)` | Varn's `ioPool()`, for blocking I/O. |
| `postToFrame(work)` | The frame thread, during the next poll of the event loop. |
| `run(work, completion)` | `work` on the task pool, then `completion` on the frame thread with a `JobSystem::Result` that holds the value or the error message. |
| `runIo(work, completion)` | The same, with `work` on the I/O pool, for work that blocks, such as reading files. |
| `parallelFor(begin, end, grainSize, body)` | Splits the range into chunks, which task pool workers and the calling thread take one at a time. The caller must be the frame thread. It runs every chunk that no worker started, so it never waits behind unrelated work queued on the pool, such as a navigation mesh that builds in the background. It returns when every chunk is done and throws the first failure again. |

An exception never escapes posted work. The job system hands it to the frame thread, where it becomes an engine error.

These parts of the engine use the pools:

- The asset manager decodes images, sounds and maps on the task pool, and creates their GPU objects on the frame thread within the upload budget of each frame. An asset that streams, such as a streamed sound, opens a reader of its file and reads its header on the I/O pool.
- The renderer and the particle emitters use `parallelFor` to convert sprites and update particles, and so do physics fluids and ray batches.
- Physics worlds of more than one thread post the tasks of each Box2D step to the task pool through `StepTasks`. The frame thread runs the chunks that no worker took yet, the way `parallelFor` does.

Lua has a single state, so Lua code never runs on worker threads. Slow engine operations return a Varn promise that a task awaits, and long Lua computations run as `haylen.jobs` coroutines that share a time budget each frame.

Other threads hand their results to the frame thread:

- Native bridge handlers, native dialogs and opened urls may answer from any thread. Each WebSocket connection runs on a thread of its own in native builds, which the frame thread never waits for. They all hand their results to thread-safe queues that the frame thread drains at the start of the next frame.
- Video streams take frames from any thread into a slot of the newest frame, which the frame thread swaps out under a lock. Audio streams take samples from any thread into a ring that the thread that mixes reads without a lock.
- Each voice of a streamed sound decodes on the I/O pool, in fills that the mixer posts from the frame thread, into a ring of decoded frames that the thread that mixes reads without a lock. So the thread that mixes never reads a package, decrypts or waits.
- Log listeners receive lines on whatever thread wrote them.

Code on the frame thread never iterates a container while the loop body can run Lua or a user callback. It copies the container first, into storage it reuses, the way `UiPlugin` copies its mounted GUIs. Or it reaches the elements by index up to the count they had before the loop, the way `Engine::frame` reaches the plugins, whose registry only grows while the app runs.

The engine creates some static objects with `new` and never destroys them. Among them are the log listeners, the object counters and their registry, the default options and empty values that methods hand out, the state of the process that `platform::Player` keeps, the window of the Sokol backend, and the platform state behind the bridge, native calls and text input. The reason is that a process can end with an `exit()` on a thread other than the frame thread, as the iOS simulator does on a background queue when its render server dies. `exit()` runs the destructors of static objects on that thread while the frame thread still logs, counts objects and plays the app. The workers of the native WebSocket connections are the one exception: they stop their connections at exit and wait for them, so the static state of Poco and OpenSSL outlives every connection thread.

The web build has a single thread, like Varn. The poll of the event loop runs the pool jobs posted since the last frame, and `JobSystem::parallelFor` runs the whole range on the frame thread. So the page needs neither `SharedArrayBuffer` nor cross-origin isolation headers.

## What the base costs

The frame loop reuses its storage. So a steady frame with scenes, timers and tweens allocates nothing in C++, which `EngineTest.SteadyFramesAllocateNothing` checks, and only scripts allocate, through the allocator of Lua.

The command `haylen bench --suite core` builds `haylen-core-benchmark` in Release and times the base of the engine on the headless host. These are the average milliseconds of each measurement on an Apple M5 Pro, the middle of three runs. The machine was loaded by other work, so they are a ceiling.

| Measurement | Milliseconds |
| --- | --- |
| A frame of an app at rest | 0.034 |
| A frame of an app with a Lua scene that updates | 0.039 |
| 1000 emits of an event to 100 C++ listeners | 0.42 |
| 1000 emits of an event from Lua to 100 Lua listeners | 4.1 |
| 1000 emits of an event from C++ to 100 Lua listeners | 5.3 |
| 1000 emits of a signal to 100 Lua listeners | 4.1 |
| 1000 owners, each with a listener, a timer and a tween, created and ended | 9.3 |
| An update of 10000 timers that wait | 0.040 |
| An update of 10000 timers that fire every frame | 0.58 |
| An update of 10000 tweens of native values | 0.33 |
| A frame with 1000 tweens of Lua tables | 0.16 |
| A frame with 1000 tweens of Lua tables that a scene owns | 0.20 |
| 1000 jobs from the frame thread to a worker and back | 5.4 |
| `parallelFor` over a million values | 0.080 |
| 1000 JSON assets loaded in the background, with the frames that wait for them | 12.2 |
| 1000 JSON assets loaded on the frame thread | 3.9 |

Due timers fire from a heap. So an update grows with the timers that fire, not with every timer that the scheduler holds. An owner hands its process mode once per frame to everything it owns, so the tweens of a scene cost about what tweens without an owner cost.

## Resource ownership

One `Engine` owns every service of one running app: the Varn runtime and its Lua state, the job system, the graphics device, audio, user storage, the bridge, the system, the dialogs, the renderer, assets, scenes and plugins. Restarting an app destroys the engine and creates a new one.

The GPU device, the renderer and the interface keep state of the process, so only one engine runs at a time. Creating another while one runs throws `Only one engine can run at a time. Destroy the engine that runs before creating another.`. The GPU device of the Sokol backend also throws `Only one graphics device can exist at a time.` for a second device, since Sokol keeps one global context.

The engine holds the start of an app with `Engine::holdStart` while the content it needs to start downloads behind the update screen. The app then starts in the first frame after the last hold ends.

### Shutting down

The engine shuts down in a fixed order:

1. It clears the scene stack, which also unloads a scene that is still loading and the preloaded scenes.
2. It stops the application, and stops the plugins in reverse order of registration.
3. It clears timers and tweens, cancels pending asset callbacks, and closes the bridge, the system and the dialogs, which close the native dialogs that are still open.
4. It stops the Varn runtime, which closes the Lua state and joins the worker pools.
5. Only then does it release audio, plugins, scenes, assets, the default font, the renderer and, last, the graphics device.

So a plugin releases every Lua value it holds in `stop`, because the Lua state is gone by the time the plugin is destroyed.

### GPU objects

GPU objects are created only on the frame thread, through `graphics::Device` (`createTexture`, `createDynamicTexture`, `createDynamicAlphaTexture`, `createRenderTarget`, `replaceTexture`) and the renderer. Texture and render target handles share their resource. When the last handle dies, on any thread, the resource hands its GPU objects to the `ResourceGraveyard` in `engine/src/graphics/ResourceGraveyard.hpp`. `Device::collectGarbage()` then destroys them on the frame thread after the frame is submitted, so no queued draw ever uses a destroyed object.

Replacing the pixels and the size of a texture with `replaceTexture` buries the old image the same way, and a dynamic texture stays dynamic. That is how the texture of a video stream follows a new frame size under every handle. Dynamic textures, such as the atlases of fonts and of the UI and the textures of video streams, otherwise change in place. `updateTexture` keeps the last pixels that a texture received, and the renderer sends every changed texture once per frame, before its passes, with `Device::uploadTextures`. The pixels of every changed texture reach the GPU device in one `writeImages` call. The device of the Sokol backend puts them into one staging buffer, which grows to the largest upload of a frame, and copies them from there into the images. So a changed texture uploads all of its pixels once per frame, however many glyphs its draws added.

The GPU device holds room for a bounded number of objects of each kind, which `gpu::Device::getPools()` reports. The device of the Sokol backend sets the pools of `sokol_gfx` up front with the sizes that `sokol::GpuDevice` declares in `engine/src/backends/sokol/GpuDevice.hpp`, and `gpu::NullDevice` of the headless host bounds its pools the same way.

| Pool | Size | Bounds |
| --- | --- | --- |
| `images` | 4096 | Textures and render targets alive at once. |
| `buffers` | 4096 | Baked static sprite batches and draw buffers. |
| `programs` | 512 | Shader programs, those of the renderer and those of custom shaders. |
| `pipelines` | 2048 | Pipelines, one per program, blend mode and kind of target. |

Going past a limit throws an error that names it, such as `The graphics device has no room for another texture. At most 4096 textures and render targets can exist at once.`, and the app stops on the error screen.

### Assets and Lua values

Assets live while something holds them, a preload group included. The asset manager keeps only weak references. On a `LowMemory` event it drops the entries of released assets before the `lowMemory` signal reaches the app.

C++ code that keeps a Lua value alive uses `lua::Reference`. Userdata keep their Lua callbacks in a table stored as their user value, so a callback that refers back to its owner never keeps it alive forever.

### Scenes and tasks

Scenes own what they create. The `SceneManager` drives every change as one pipeline of phases, start, load, cover, hold and reveal, and every scene through the states of `core::Scene::State`, from its `load` with a `core::SceneLoad` to its `unload`, as the [lifecycle guide](lifecycle.md#scene-lifecycle) describes.

When a scene unloads, its `ConnectionScope` ends in C++. In Lua, `lua::Owners` releases everything that the scene table owns: first the tasks of its load hook and of `scene.spawn`, so none of its code resumes, then its GUIs, listeners, timers and tweens.

A `lua::Task` runs a Lua function in a coroutine of its own and cancels it through Varn, the way `task.cancel()` cancels a task of `async.spawn`. No promise resumes the coroutine again, and Varn closes it, which runs its to-be-closed variables. That happens at once when the task waits, and right after its next await when the task cancels itself. A job of `haylen.jobs` that waits for anything but `jobs.checkpoint` ends the same way.

## App packages

An app package is a folder or a `.zip` file with `app.json`, the Lua modules under `source/` with `source/main.lua` as the entry point, and the assets under `content/`. For every plugin that `app.json` lists, it also holds `plugins/<id>/plugin.json` with the Lua modules of the plugin under `plugins/<id>/source/`. Nothing else in the folder belongs to the package, so the platform projects of an app can sit next to it. The [Lua guide](lua.md#the-app-package) describes its layout, and [`haylen.config`](../lua-api/haylen.md#haylenconfig) lists every field of `app.json`.

The class `io::Package`, in `haylen/io/Package.hpp`, is the view of a package. It is read-only and safe to use from several threads.

- `Package::open` opens a folder or a zip file. A zip file stays on disk and reads each entry when it is asked for.
- `Package::openZip` also takes the bytes of a zip file.
- `io::MemoryPackage`, in `haylen/io/MemoryPackage.hpp`, holds files in memory that can be replaced while the app runs, which is what a browser editor uses.

Paths are relative and normalized, and a path that is absolute or leaves the root of the package throws.

- The methods `readAsset`, `openAssetReader`, `assetExists` and `listAssets` take paths relative to `content/`, which is how every asset path in the engine works, and `require` reads modules from `source/`.
- The method `getFileSize` returns the size of a file. `openReader` returns an `io::PackageReader` that reads any range of the file and that several threads may share, and `readRange` reads one range. So files of any size stream in bounded memory.
- The method `read` returns a whole file of at most `Package::kMaxReadSize` bytes and throws for a larger one, and `readRange` throws for a longer range the same way.

`Path::kAppConfigFile`, `Path::kSourceDirectory`, `Path::kContentDirectory` and `Path::kPluginsDirectory` in `haylen/io/Path.hpp` name the entries, and `Path::plugin(id, relative)` builds the path of a file of a plugin. The method `core::AppConfig::fromPackage` reads the `app.json` of a package and checks that every plugin it lists has its `plugin.json` there.

Each app finds its package in a place that depends on the platform.

| Platform | Where the package is |
| --- | --- |
| Desktop player | The folder or zip file named on the command line, as in `haylen --dev samples/games/tiny-island`. |
| Windows and Linux apps | `app/` next to the executable, whose entries the build links to the package folder, or `app.zip` in the same place. |
| macOS, iOS and tvOS apps | `app/` in the bundle resources, or `app.zip` in the same folder. |
| Web | The zip file that the page of the web template hands over at run time, a zip file that the page names by URL, or `/app` in the virtual file system for C++ apps that preload it at build time. |
| Android | `app/` inside the assets of the APK, with `haylen-package-index.json` listing its files, because Android cannot list asset folders recursively. |

The [distribution guide](../tools/distribution.md) explains how `haylen.py` puts the package into the platform templates, and the [build guide](../tools/build.md) how `haylen_add_app` deploys it for C++ apps.

## The web runtime

The web build is the same runtime, controlled by the page. The file `engine/platform/web/haylen-runtime.js` is linked as a pre-js file and defines `Module.haylen`, which a browser editor uses to run apps without reloading the page. The page sets some of its fields before the module loads, such as the package to download and whether the app runs in development, as the [page setup](../tools/build.md#page-setup) describes. Its functions replace, edit, restart, stop and pause the app, run lines of the developer console, register the page side of the platform bridge and create the contexts of the web modules of plugins. Its callbacks receive the log, the errors, the starts and stops of apps, the statistics and the reports of hot reload. The [runtime API](../tools/build.md#runtime-api) lists every member.

How the runtime behaves:

- Callbacks run right after the frame that produced them, so they may call back into the runtime, even to restart the app.
- Every start creates a new `Engine` with a fresh Lua state, and the previous one is destroyed between frames.
- A script error never stops the runtime. The engine shows the error screen, and the page and the web modules of plugins receive the error with the message, the script position and the stack. The runtime then waits for the next `run`, `restart` or `loadZip`. On other platforms the report of the error goes to the log and, through `Host::reportError`, to the platform services.
- The runtime draws into `Module.canvas`, which must be a canvas element with an id, and follows its size with a `ResizeObserver`.
- It plays audio through an `AudioWorkletNode`. The processor `haylen-audio-worklet.js` loads from the folder of the script, and the engine mixes the blocks that the processor plays on the page thread, as the [audio guide](../audio/audio.md#sessions-and-interruptions) describes.
- User data lives under `/persistent`, a file system kept in IndexedDB.
- The command `haylen engine --platform web` builds the player for WebGPU and for WebGL2. The page of the web template picks WebGPU when the browser offers an adapter, and `?graphicsApi=webGpu` or `?graphicsApi=webGl2` forces one of them. The [distribution guide](../tools/distribution.md#web-loader) covers the page and the [build guide](../tools/build.md#web-builds) the web builds.

## Development and hot reload

An app runs in development when the player gets `--dev` or `--dev-server`, or when a page sets `Module.haylen.development`. The [Lua guide](lua.md#hot-reload) explains what a save does to the app. This section explains how the engine does it.

**The session.** The runtime gives its host a `platform::DevelopmentSession`, which belongs to the process and outlives every engine. At launch, the session takes a snapshot of `app.json`, `source/`, `content/` and the `plugin.json` and `source/` of every plugin of the folder. It holds the queue of changed paths. Because the session keeps the snapshot and the queue, changes saved while the app restarts reach the next app.

**The scan.** Every half second of real time, also while the app is halted, `HotReloadPlugin` posts a scan of the folder to the I/O pool, and the scan queues the changed paths in the session. The plugin takes the queue at the start of a frame, even from the error screen. A scan ignores hidden files, backup files and files under the `source/` folders that are not Lua, and it leaves files that are still being written for the next scan.

**The module graph.** In development, the plugin replaces `require` with the one of `lua::ModuleGraph`. The graph records, for every module of the package:

- its name, its source and its dependencies,
- the owner of what its top level registered without an owner, which the bindings take from `lua::Owners::pushDefault`,
- the values it kept with `hotReload.keep`, each with an owner of what its `create` registered,
- a `lua::ModuleSnapshot` of what it produced.

**A batch of changes.** A batch runs `lua::ModuleReloader` over the changed modules in the order they depend on each other. For each module:

1. The module runs again in a staging environment, with a staging owner.
2. `lua::ModuleClosures` pairs the variables that its old and new functions capture, by name, and the new functions join the old variables.
3. The new value and the new globals merge into the live ones, against the snapshot.
4. The old owner ends.

A module that fails changes nothing, and the batch goes on with the next one. Once the batch reloaded, `lua::ValueRemap` walks the Lua heap from the registry once and puts the new functions wherever the old ones were. The `reloaded` hooks and the `moduleReloaded` events follow, and then the first error of a module or a hook shows on the error screen.

**The web and the development server.** The runtime of the web plays every package, except a protected release, under an `io::OverlayPackage`. So the files that the page sends with `setFile` replace the files of the package that plays, and `applyChanges` queues their paths in the session of a page in development. A page in development that names a development server, and an app launched with `--dev-server <address>`, connect to it through `platform::DevelopmentConnection`. It is a `net::WebSocket` that reconnects on its own. It writes every file that the server pushes into the overlay, after it checks that the file belongs to the package, queues its path in the session, and answers with the report of the batch.

The command `haylen.py run <app> --platform web` serves the site with that server on the same port. The command `run` with a device, a simulator, the emulator or this Mac launches the app with `--dev-server`. On Android the address travels as the extra `dev.haylen.developmentServer` of the launch intent, which `JavaBridge::getDevelopmentServer` reads for a debuggable app, as the [distribution guide](../tools/distribution.md#development-mode) describes.
