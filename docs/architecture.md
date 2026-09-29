# Architecture

This guide explains how Haylen is put together: the libraries it builds, where each module lives, how built-in subsystems plug into the engine, how a frame runs, which thread owns what, how GPU resources and Lua values are released, what an app package is and what the web runtime offers to a browser editor. It is written for engine contributors and for C++ developers who extend the engine. App developers who only write Lua can start with the [Lua guide](lua.md) and the [Lua API reference](lua-api.md).

## Libraries and products

`engine/` is a standalone CMake project. It builds these targets, and the [embedding guide](embedding.md) explains how another project consumes them.

| Target | Alias | What it is |
| --- | --- | --- |
| `haylen_engine` | `haylen::engine` | Static library with the whole C++ API and every Lua binding. It talks to the GPU through Sokol gfx and never calls Sokol app or operating system APIs. |
| `haylen_platform` | `haylen::platform` | Static library with the portable pieces of the runtime: Sokol event translation, the desktop bridge handlers and the memory warning flag. The runtime and the tests link it. |
| `haylen_runtime` | `haylen::runtime` | Object library that turns the engine into an app. It holds the entry point, `sokol_main` or `haylen_main` on Apple platforms, the Sokol host and the services of one platform folder, chosen at configure time. |
| `haylen_headless` | `haylen::headless` | Static library with the headless host used by the tests. It is built only for desktop platforms. |
| `haylen` | | The player executable. It links the runtime with `engine/src/platform/sokol/LuaPlayer.cpp` and runs any app package. |
| `haylen_add_app` | | CMake function that builds the app of one package for the current platform. |

The executable always defines the static `haylen::core::Application::create()`, declared in `haylen/core/Application.hpp`. `LuaPlayer.cpp` returns a `lua::Application`, which runs `source/main.lua`, and a C++ app returns its own `core::Application`.

## Source layout

Public headers live in `engine/include/haylen/<module>/` and implementation files in `engine/src/<module>/`, with the same folder names. The namespace of a context follows its folder, so `engine/include/haylen/core/Engine.hpp` declares `haylen::core::Engine` and `engine/include/haylen/plugins/UiPlugin.hpp` declares `haylen::plugins::UiPlugin`. Everything specific to 2D sits under a `2d` folder on both sides, and each folder under `2d` maps to a namespace with the `2d` suffix, the name of its Lua module, so `engine/include/haylen/2d/graphics/Renderer.hpp` declares `haylen::graphics2d::Renderer`.

| Folder | Contents |
| --- | --- |
| `core` | `Engine` with the error screen it draws when a script fails, `Application`, `Scene` and `SceneManager` with the `TransitionEffect` interface, `ProcessMode`, `AppConfig`, `FrameClock`, `FrameQueue`, `JobSystem`, `TimerScheduler`, the tweens (`Tween`, `PropertyTween`, `Timeline`, `TweenManager`, `TweenTrack`, `PropertyTrack`, `TweenProperty`, `TweenValue` and `TweenMotion`), `Signal` with `Connection`, `ScopedConnection` and `ConnectionScope`, `EventBus` with `LifecycleEvent`, `Log`, `Json` with `JsonValidator`, `Utf8` and `Version`. |
| `ai` | `StateMachine`, which Lua sees as `haylen.ai`. |
| `plugins` | The `Plugin` interface, `PluginRegistry` and every built-in plugin. |
| `math` | `Vec2`, `Rect`, `Insets`, `Color`, `Transform2D`, `FloatRange`, `Random`, `Noise2D`, `Easing`, `Geometry` with `Circle` and `Segment`, `PoissonDisk` and the scalar helpers of `Math`. |
| `io` | `Package`, `MemoryPackage`, `PackageWatcher` and the package paths of `Path`. |
| `storage` | `UserStorage`, `SaveSlots` and `Preferences`. |
| `assets` | `Manager`, which loads, caches and preloads assets in groups. |
| `graphics` | Dimension-agnostic GPU layer: `Device` with its shared white texture, `Texture`, `Image`, `RenderTarget`, `BlendMode` and `Viewport`. |
| `text` | `Font`, the interface of the fonts that shape and lay text out for any renderer, with the signed distance field `TrueTypeFont` shaped by HarfBuzz and the `BitmapFont` of BMFont files and grids, `FontFamily`, `TextStyle`, `TextLayout`, `TextAlign` and `Direction`, and rich text: `RichText` with `RichTextDocument`, `RichTextOptions`, `RichTextRegistry` and `TextEffect`. The markup parser, the layout builder, the layout cache, the bidirectional paragraph, the segmenter and the Thai phrase breaker are the private `MarkupParser`, `LayoutBuilder`, `LayoutCache`, `BidiParagraph`, `Segmenter` and `PhraseBreaker`. |
| `input` | `Input` with `Touch`, `GamepadState` and `InputDevice`, the `Key`, `MouseButton`, `GamepadButton` and `GamepadAxis` enums with their names in `Controls`, `KeyModifiers`, `ActionMap`, `VirtualInput`, and `GestureRecognizer` with `Gesture`. |
| `audio` | `Mixer`, which plays voices and music through named buses, and `Sound`. |
| `ui` | `Backend`, which runs Dear ImGui over the app, `Document` with its `Placement` and `Event` records, `Context`, `Theme`, and `Component` with `ComponentRegistry`, `PropertyReader` and `TextValue`. The built-in components live one class per file in `src/ui/components/`, grouped by family, and draw through the private `Widgets`, `Typography`, `Surfaces` and `TextEditor` helpers, where `TextFieldLayout` places the caret of text fields by the shaped text. |
| `platform` | `Bridge`, the JSON channel to native code, `Event` with `TouchPoint`, `Window` and the public Apple bridge headers. |
| `net` | `WebSocket`, with the Poco transport of native builds and the browser transport of the web build. |
| `localization`, `debug` | The `Catalog` of translated text, and `Profiler` with `ProfileScope`, `ProfileSample`, the `LogLine` records of the debug overlay, the `Stats` snapshot, the `StatsDisplay` that draws the compact statistics, `Monitor`, and `ObjectCounter` with `TrackedObject` and `TrackedCount`, which count the objects of every type. |
| `lua` | The public C++ toolkit for writing Lua bindings, used by the engine and by projects that extend it, and `lua::Application`. |
| `2d/graphics` | `Renderer` with its text and rich text drawing, captures and image blends, `Sprite`, `SpriteBatch`, `StaticSpriteBatch`, `Camera`, `Parallax`, `NineSlice`, `PostProcess`, `ImageBlend` and `SceneTransition`, the built-in scene transition effects. |
| `2d/animation`, `2d/particles`, `2d/lighting` | `Animation`, `Animator` and `SpriteAtlas`, particle `Emitter` and `Effect`, and `Light` with `LightFlicker`. |
| `2d/physics` | The Box2D wrapper: `World`, `Body`, `Shape` and `Joint` handles, `CollisionFilter`, contact and sensor events and raycast hits. |
| `2d/tiled` | Tiled maps: the `Map` model with its layers, objects and tilesets, `World` files, `MapRenderer` and `ObjectFactories`. |
| `2d/navigation`, `2d/spatial` | The grid A\* `Grid` with `SteeringAgent` and `Wanderer`, and the `HashGrid` spatial hash. |

Lua bindings sit next to the module they bind, such as `engine/src/2d/physics/Physics2DLua.cpp`. Plugins that apps construct or reference have public headers in `engine/include/haylen/plugins/`, such as `UiPlugin.hpp`, and the others keep their headers in `engine/src/plugins/`, such as `AudioPlugin.hpp`. `engine/src/lua/` holds the implementation of the binding toolkit, the `Environment` that routes `require` to the `source/` folder of the package, the `ScriptedScene` and `ScriptedLoadingView` behind Lua scenes and loading views, the `Task` that runs cancellable coroutines for load hooks and `scene.spawn`, the `Owners` that tie listeners, tasks and documents to their owner, and `haylen.jobs`.

`engine/src/platform/` holds the boundary with the operating system: `Host`, `Services` (the class whose static methods every platform folder implements), the bridge with its `BridgeRelay` and its `PlatformLua` binding, `native/` with the native libraries, callbacks and C interface of `haylen.native` (`NativeLibraries`, `NativeSignature`, `NativeCallback`, `NativeCallbacks`, `NativeApi` and `NativeLua`), `sokol/` (`SokolHost`, `SokolRuntime` with the entry point, `SokolEvents`, `MemoryWarning` and `LuaPlayer.cpp`), `headless/` and one folder per platform. `apple/` holds `AppleServices.mm`, `AppleBridge` behind `HaylenBridge` and the Mac Catalyst `CatalystInput`, `android/` holds `AndroidServices.cpp` with its JNI exports, `AndroidActivity`, `AndroidAssetPackage`, `AndroidGamepads` and `JavaBridge`, `web/` holds `WebServices.cpp`, `WebPage` with the exports the page calls and `BrowserWebSocket`, `windows/` and `linux/` hold their services with `WindowsMethods`, `LinuxMethods` and `LinuxGamepads`, and `desktop/` holds the `DesktopMethods` they share.

The other engine folders are `engine/shaders/` (sokol-shdc sources compiled at build time, with the shader library in `include/haylen/` that app shaders include too), `engine/platform/` (the Android library module and its Gradle script, the web shell and `haylen-runtime.js`, and the Apple Info.plist and launch screen templates), `engine/cmake/`, `engine/tests/` and `engine/bench/`.

Every dependency is pinned with a SHA-256 hash in `engine/cmake/haylen-dependencies.cmake`. The engine declares nlohmann/json and libuv before Varn, so both share one copy. Varn brings Lua, Poco, OpenSSL, libzip and zlib, which the engine reuses, and the engine adds Sokol, Dear ImGui, Box2D, miniaudio, zstd, stb, the core of msdfgen, HarfBuzz, SheenBidi, libunibreak, the Thai model of BudouX and, for the tests, GoogleTest.

## Plugins

Every subsystem is a plugin. A plugin derives from `haylen::plugins::Plugin` in `haylen/plugins/Plugin.hpp`, returns its name from `getName` and overrides the hooks it needs. Every hook has an empty default and runs on the frame thread. A plugin wires its subsystem into the engine and installs its Lua modules, while the subsystem itself stays in its own context folder.

| Hook | When it runs |
| --- | --- |
| `start(core::Engine&)` | Once, when the engine starts or when the plugin is added to a running engine. |
| `installLua(core::Engine&, lua_State*)` | Right after `start`, to register Lua modules in `package.preload`. |
| `event(core::Engine&, const Event&)` | For every platform event, including the lifecycle events `Suspended`, `Resumed`, `Resized`, `FocusGained`, `FocusLost`, `QuitRequested` and `LowMemory`. |
| `beginFrame(core::Engine&, float)` | At the start of every frame, after asynchronous results arrive and before any update. |
| `fixedUpdate(core::Engine&, float)` | Zero or more times per frame, once per fixed step. |
| `update(core::Engine&, float)` | Once per frame, with the scaled frame time. |
| `render(core::Engine&)` | After the scenes render the world. |
| `renderUi(core::Engine&)` | After the scenes render their UI. |
| `endFrame(core::Engine&)` | After the frame is submitted, even while the error screen shows. |
| `stop(core::Engine&)` | Once, when the engine stops, in reverse registration order. |

`plugins::BuiltInPlugins` in `engine/src/plugins/BuiltInPlugins.cpp` registers the built-in plugins in dependency order. They use exactly the interface that other projects use, and their names follow the Lua modules they install.

| Plugin | Lua modules | Other duties |
| --- | --- | --- |
| `core` | `haylen`, `haylen.log`, `haylen.timer`, `haylen.window`, `haylen.viewport`, `haylen.scene`, `haylen.signal`, `haylen.events`, `haylen.math`, `haylen.tween`, `haylen.ai` | Runs the autoloads of the app. |
| `jobs` | `haylen.jobs` | Resumes Lua jobs within the frame budget during `update`. |
| `input` | `haylen.input` | |
| `graphics2d` | `haylen.graphics`, `haylen.graphics2d`, `haylen.lighting2d` | |
| `assets` | `haylen.assets` | |
| `text` | | Owns the registry of the effects and icons rich text markup names, and registers the `bitmapFont` and `gridFont` asset types. |
| `animation2d` | `haylen.animation2d` | Registers the `atlas` asset type. |
| `particles2d` | `haylen.particles2d` | Registers the `particles` asset type. |
| `audio` | `haylen.audio` | Registers the `sound` asset type. |
| `physics2d`, `tiled`, `spatial2d`, `navigation2d` | `haylen.physics2d`, `haylen.tiled`, `haylen.spatial2d`, `haylen.navigation2d` | `tiled` registers the `tiled` and `tiled_world` asset types. |
| `localization` | `haylen.localization` | |
| `storage` | `haylen.storage`, `haylen.preferences` | Loads preferences on start and writes them on suspend and stop. |
| `debug` | `haylen.debug` | Draws the compact statistics with the renderer and the full debug overlay with ImGui, cycled with F3 by default, samples monitors and publishes object events. |
| `hotReload` | | Watches the package folder of an app in development. |
| `ui` | `haylen.ui`, `haylen.imgui` | Starts the ImGui frame in `beginFrame` and draws mounted documents in `renderUi`. |
| `net` | `haylen.net` | Delivers WebSocket events in `beginFrame`. |
| `platform` | `haylen.platform` | Answers the `engine.info` and `app.version` bridge methods and connects the bridge relay of native code to the running app. |
| `native` | `haylen.native` | Keeps the Lua functions of the native callbacks of the app and drops them when it stops. |

### Adding a plugin

A project adds its own plugin from its `Application`. `Engine::addPlugin` registers the plugin, and when the engine is already running, which is always the case inside `Application::start`, it calls `start` and `installLua` right away.

```cpp
class App final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        engine.addPlugin(std::make_unique<ScorePlugin>());
    }
};
```

Plugins added this way run after the built-in ones in every phase and stop before them. Code finds a plugin with `engine.getPlugin<ScorePlugin>()`, which throws `std::logic_error` when it is missing, or with `engine.getPlugins().find<ScorePlugin>()` and `engine.getPlugins().find("score")`, which return a null pointer. Plugins talk to each other only through public methods, never through each other's internals. The [Lua guide](lua.md#extending-the-engine-from-c) shows a complete plugin with its own Lua module.

## The host boundary

The portable engine reaches the platform only through `platform::Host`, declared in `engine/src/platform/Host.hpp`. `Host` extends the public `platform::Window` interface (framebuffer size, DPI scale, fullscreen, title, cursor, mouse lock, on-screen keyboard, clipboard and quit requests) and adds what the engine needs to run.

| Method | Purpose |
| --- | --- |
| `getPlatformName()` | The name that the `platform` field of the root `haylen` Lua module reports, such as `macos` or `web`. |
| `getGraphicsSetup()` | The Sokol gfx environment the `graphics::Device` is created with. |
| `getAudioSetup()` | Whether the `audio::Mixer` opens a device. |
| `getFrameTarget()` | The swapchain the renderer submits each frame to. |
| `getUserDataDirectory(identifier)`, `persistUserData()` | Where `storage::UserStorage` keeps the files of an app, and how the platform makes them durable. |
| `getSafeAreaInsets()` | The screen area that UI must avoid, in framebuffer pixels. |
| `pollGamepads(span)` | Fills the gamepad states once per frame. |
| `dispatchPlatformCall(id, method, params)` | Hands a bridge call to native code. |

`haylen_runtime` implements the host with `SokolHost` on top of sokol_app and the `Services` of the platform folder compiled into it. Its entry point in `engine/src/platform/sokol/SokolRuntime.cpp`, `sokol_main`, or `haylen_main` on Apple platforms where `sokol_app` leaves `main` to the app, forwards to `SokolRuntime`, which opens the package named on the command line or the one bundled with the app, turns on development mode only for `--dev`, reads `app.json`, calls `Application::create()` and `Application::configure`, creates the window from the configuration and then creates and drives the `Engine`. A package that cannot be opened or a `app.json` that fails validation starts a stand-in application that shows the error on screen.

`haylen_headless` implements the host with `HeadlessHost` for tests. It uses the Sokol dummy backend, a mixer without a device and a user data folder chosen by the test, and it records bridge calls instead of dispatching them. The [testing guide](testing.md) describes it in detail.

Platform services such as sign-in or purchases go through `platform::Bridge` with JSON requests and replies, described in the [platform bridge guide](platform_bridge.md).

## The frame

`Engine::frame(frameSeconds)` in `engine/src/core/Engine.cpp` runs one frame in this order.

1. The viewport updates from the framebuffer size, the design size, the scaling policy and the safe area.
2. The host fills the gamepad states and the action map updates from every device, blocked while the app is halted or a scene change holds input back, so held actions read as up until they are released.
3. The frame clock advances with the frame time, clamped to `maxFrameTime` and scaled by the time scale, and touches and gestures update.
4. The engine publishes changes of fullscreen, orientation, safe area and gamepad connections on its `EventBus`. Unless the app is in the background, the asset manager finalizes the assets that finished decoding, creating their GPU objects, until the upload budget of the frame runs out and at least one per frame. `Runtime::poll()` advances Varn's event loop. Promises settle, `async` coroutines and scene tasks resume, Varn timers fire, HTTP and socket callbacks run, and completions posted from worker threads run, such as decoded assets that join the upload queue. Then `Bridge::pump()` delivers bridge replies and native events.
5. Every plugin runs `beginFrame`.
6. The fixed steps run. For each step the clock owes, the scenes run `fixedUpdate`, fixed-step tweens advance and then every plugin runs `fixedUpdate`.
7. App timers, tweens, plugin `update` and the scene update run, in that order. Inside the scene update, loads that finished or failed settle, the pending scene change moves through its phases, the loading view updates and the top scene runs `update`.
8. The audio mixer updates.
9. The renderer begins the frame. The scenes and the loading view render the world, plugins `render`, the scenes and the loading view render their UI and plugins `renderUi`. During a scene transition the scenes before and after the change do this inside a capture of their own image, the plugins drawing with the current scenes, and the view of the effect draws the effect on the screen. While the transition holds the covered screen for a load, no scene renders, and the loading view and the plugins draw over the effect.
10. The renderer submits the whole frame to the host's swapchain.
11. Every plugin runs `endFrame`, and the `FrameQueue` runs the calls posted during the frame, such as queued events and deferred signal slots. The graphics device destroys released GPU objects, input closes the frame by clearing its pressed and released edges, and the profiler closes the frame.

A halted app, which the lifecycle options decide by the app state, skips steps 6 and 7 and advances the clock by zero, and an app in the background also skips rendering, as the [lifecycle guide](lifecycle.md) explains. The game pause of `Engine::setPaused` gates scenes, autoloads, timers and tweens by their `ProcessMode` inside those steps.

Platform events arrive between frames through `Engine::handleEvent`. Input converts positions to design coordinates, lifecycle events update the app state and emit the engine signals (`appStateChanged`, `resized`, `quitRequested` and `lowMemory`) and the matching events on the `EventBus`, and then every plugin and the top scene receive the event.

An exception thrown by app code during an update, a render, the submission or an event goes to `Engine::reportError`. The engine keeps the first error as a `lua::Error`, logs its plain-text report and emits `errorRaised`. From then on it skips the fixed steps, the updates, scene events and the render hooks of scenes and plugins, and it draws the error screen instead. Varn's event loop, the bridge, `beginFrame` and `endFrame` keep running, so the hot reload plugin and the web page can restart the app. When a hook asks for a restart with `Engine::requestRestart`, the runtime replaces the engine after the frame, when no engine code is on the stack.

### The error screen

Every protected call of the engine runs with the message handler of `lua::Runtime`, which reads the stack of the failing coroutine with `lua_getstack` and `lua_getinfo` and turns the error into a `lua::Error`: the message, the script position Lua put in front of it and the frames from the innermost call outward, each with its source, line, function name as Lua describes it and kind (Lua, C or main chunk). The handler leaves out the levels that belong to the engine, which are the native function of `Runtime::protectedRun` and, for `async.spawn` and `async.run`, the task wrapper of the `haylen.tasks` chunk together with the `xpcall` above it, so a task error shows the stack of the task only. `Error::getTraceback` renders the frames as aligned columns without tab characters, and `Error::toJson` adds the frames as a list for the web page.

`core::ErrorScreen` draws the error with the renderer alone, so it works even when the UI is what failed. It shows the app name and version, the platform and the engine version, the message wrapped to the safe area, the file and line, the lines around the error line when the error points into a file of the package, and the stack. Content taller than the screen scrolls with the mouse wheel, a drag and the arrow keys. `C` copies the plain-text report to the clipboard and `R` restarts the app, and both actions are buttons that answer to a click or a tap. When hot reload watches the package, a hint says that saving a file reloads the app. The log receives the same report the clipboard does.

## Threads and async

The frame thread owns the GPU, the Lua state, scenes, UI, audio control and physics. It never blocks.

`JobSystem`, reached with `engine.getJobs()`, runs background work on the Varn runtime that the engine owns.

| Method | Where the work runs |
| --- | --- |
| `post(work)` | Varn's `taskPool()`, for CPU work. |
| `postIo(work)` | Varn's `ioPool()`, for blocking I/O. |
| `postToFrame(work)` | The frame thread, during the next `Runtime::poll()`. |
| `run(work, completion)` | `work` on the task pool, then `completion` on the frame thread with a `JobSystem::Result` that holds the value or the error message. |
| `parallelFor(begin, end, grainSize, body)` | Chunks of the range on the task pool, with the first chunk on the calling thread, which must be the frame thread. It returns when every chunk is done and rethrows the first failure. |

An exception that escapes posted work never escapes the job. The job system hands it to the frame thread, where it becomes an engine error. The asset manager decodes images, sounds and maps on the task pool and creates their GPU objects on the frame thread within the upload budget of each frame, and the renderer and the particle emitters use `parallelFor` for sprite conversion and particle updates.

Lua has a single state, so Lua code never runs on worker threads. Slow engine operations return a Varn promise that a coroutine awaits, and long Lua computations run as `haylen.jobs` coroutines that share a time budget each frame. Native bridge handlers may answer from any thread, and each WebSocket connection runs on a thread of its own in native builds. Both hand their results to thread-safe queues that the frame thread drains at the start of the next frame. Log listeners receive lines on whatever thread wrote them.

Code on the frame thread never iterates a container while the loop body can run Lua or a user callback. It copies the container first, the way `Engine::frame` copies the plugin list and `UiPlugin` copies its mounted documents.

The web build is single-threaded like Varn. `Runtime::poll()` runs the pool jobs posted since the last frame, and `JobSystem::parallelFor` runs the whole range on the frame thread. The page therefore needs neither `SharedArrayBuffer` nor cross-origin isolation headers.

## Resource ownership

One `Engine` owns every service of one running app: the Varn runtime and its Lua state, the job system, the graphics device, audio, user storage, the bridge, the renderer, assets, scenes and plugins. Restarting an app destroys the engine and creates a new one. Sokol keeps a single global device, so only one engine can exist at a time, and a second `graphics::Device` throws `Only one graphics device can exist at a time.`.

The engine shuts down in a fixed order. It clears the scene stack, which also unloads a scene that is still loading and the preloaded scenes, stops the application and stops the plugins in reverse registration order. Then it clears timers and tweens, cancels pending asset callbacks, closes the bridge and stops the Varn runtime, which closes the Lua state and joins the worker pools. Only then does it release audio, plugins, scenes, assets, the default font, the renderer and finally the graphics device. A plugin therefore releases every Lua value it holds in `stop`, because the Lua state is gone by the time the plugin is destroyed.

GPU objects are created only on the frame thread, through `graphics::Device` (`createTexture`, `createAlphaTexture`, `createRenderTarget`, `replaceTexture`) and the renderer. Texture and render target handles share their resource. When the last handle dies, on any thread, the resource hands its Sokol objects to the `ResourceGraveyard` in `engine/src/graphics/ResourceGraveyard.hpp`, and `Device::collectGarbage()` destroys them on the frame thread after the frame is submitted, so no queued draw ever uses a destroyed object. Replacing the pixels of a texture buries the old image the same way.

Sokol preallocates its resource pools, and the `Gpu` class of `engine/src/graphics/Gpu.hpp` sets their sizes.

| Constant | Value | Bounds |
| --- | --- | --- |
| `kImagePoolSize` | 4096 | Textures and render targets alive at once. |
| `kViewPoolSize` | 8192 | Texture and attachment views. |
| `kBufferPoolSize` | 4096 | Baked static sprite batches and draw buffers. |
| `kShaderPoolSize` | 512 | Shader programs, the renderer's own and those of custom shaders. |
| `kPipelinePoolSize` | 2048 | Pipelines, one per program, blend mode and kind of target. |

Going past a limit throws an error that names it, such as `The graphics device has no room for another texture. At most 4096 textures and render targets can exist at once.`, and the app stops on the error screen.

Assets live while something holds them, including a preload group. The asset manager keeps only weak references, and on a `LowMemory` event it drops the entries of released assets before the `lowMemory` signal reaches the app. C++ code that keeps a Lua value alive uses `lua::Reference`, and userdata keep their Lua callbacks in a table stored as their user value, so a callback that refers back to its owner never keeps it alive forever.

Scenes own what they create. The `SceneManager` drives every change as one pipeline of phases, start, load, cover, hold and reveal, and every scene through the states of `core::Scene::State`, from its `load` with a `core::SceneLoad` to its `unload`, as the [lifecycle guide](lifecycle.md#scene-lifecycle) describes. When a scene unloads, its `ConnectionScope` ends in C++, and in Lua `lua::Owners` releases everything the scene table owns: the tasks of its load hook and of `scene.spawn` first, so none of its code resumes, then its UI documents, listeners, timers and tweens. A `lua::Task` runs a Lua function in a coroutine of its own and cancels it by closing the coroutine, which runs its to-be-closed variables. A promise of Varn that still waits on a closed coroutine resumes it later, so the closed coroutine is reset around a function that ends at once, and that resume runs none of the code of the task. A task cancelled while its own code runs closes on the event loop once it waits, before any promise can resume it, because the loop runs its jobs in order.

## App packages

An app package is a folder or a `.zip` file with `app.json`, the Lua modules under `source/` with `source/main.lua` as the entry point, and the assets under `content/`. Nothing else in the folder belongs to the package, so the platform projects of an app can sit next to it. The [Lua guide](lua.md) describes its layout and every `app.json` field.

`io::Package`, in `haylen/io/Package.hpp`, is the read-only, thread-safe view of a package. `Package::open` opens a folder or a zip file, `Package::openZip` also accepts the bytes of a zip file, and `io::MemoryPackage`, in `haylen/io/MemoryPackage.hpp`, holds files in memory that can be replaced while the app runs, which is what a browser editor uses. Paths are relative and normalized, and a path that is absolute or leaves the package root throws. `readAsset`, `assetExists` and `listAssets` take paths relative to `content/`, which is how every asset path in the engine works, and `require` reads modules from `source/`. `Path::kAppConfigFile`, `Path::kSourceDirectory` and `Path::kContentDirectory` in `haylen/io/Path.hpp` name the three entries.

Each app finds its package in a platform-specific place.

| Platform | Package location |
| --- | --- |
| Desktop player | The folder or zip file named on the command line, as in `haylen --dev samples/games/tiny-island`. |
| Windows and Linux apps | `app/` next to the executable, whose entries the build links to the package folder, or `app.zip` in the same place. |
| macOS, iOS and tvOS apps | `app/` in the bundle resources, or `app.zip` in the same folder. |
| Web | The zip file that the page of the web template hands over at runtime, a zip file the page names by URL, or `/app` in the virtual file system for C++ apps that preload it at build time. |
| Android | `app/` inside the APK assets, with `haylen-package-index.json` listing its files because Android cannot list asset folders recursively. |

The [distribution guide](distribution.md) explains how `make.py` puts the package into the platform templates, and the [build guide](build.md) how `haylen_add_app` deploys it for C++ apps.

## The web runtime

The web build is the same runtime controlled by the page. `engine/platform/web/haylen-runtime.js` is linked as a pre-js file and defines `Module.haylen`, which a browser editor uses to run apps without reloading the page.

| Member | Purpose |
| --- | --- |
| `packageUrl` | Set before the module loads, names a zip file that is downloaded and run instead of the bundled package. The default shell reads it from the `package` parameter of the address. |
| `loadZip(bytes)` | Replaces the running app with a zipped package. |
| `clearFiles()`, `setFile(path, content)`, `removeFile(path)` | Build an in-memory package file by file. `content` is a string or bytes. |
| `run()` | Starts the in-memory package from `source/main.lua`. |
| `restart()` | Starts the last playing app again, which reloads every script. |
| `stop()` | Ends the app and leaves an empty screen. |
| `setPaused(paused)`, `pause()`, `resume()`, `paused()` | Pause the app on its last frame. The engine sees a pause as a suspend. |
| `reloadAsset(path)` | Reloads the cached assets read from a changed file without restarting. |
| `register(method, handler)`, `unregister(method)`, `emit(event, payload)` | The page side of the platform bridge. |
| `onLog(level, line)` | Receives every log line of the engine and the app. |
| `onError({message, file, line, traceback, frames})` | Receives the error that stopped the app, split by `lua::Error`, with the stack as text and as a list of frames. |
| `onStarted({name, identifier, version})`, `onStopped()` | Report when an app starts and stops. |
| `onStats(stats)` | Receives frame statistics about once per second: frame rate, frame times, renderer counters, cached assets, audio voices and profiler scopes. |

Callbacks run right after the frame that produced them, so they may call back into the runtime, even to restart the app. Every start creates a new `Engine` with a fresh Lua state, and the previous one is destroyed between frames.

Hot reload works in two ways. On desktop, a folder package passed to the player with `--dev` is an app in development, and `HotReloadPlugin` scans its `app.json`, `source/` and `content/` every half second: a changed file under `source/` or a changed `app.json` restarts the app, even from the error screen, and a changed file under `content/` reloads the assets read from it without a restart. In the browser, the editor sends changed files with `setFile` and then calls `reloadAsset` for assets or `run` for scripts.

A script error never stops the runtime. The engine shows the error screen, the page receives `onError` with the message, the script position and the stack, and the runtime waits for the next `run`, `restart` or `loadZip`. On other platforms the report of the error goes to the log.

The runtime draws into `Module.canvas`, which must be a canvas element with an id, and follows its size with a `ResizeObserver`. User data lives under `/persistent`, a file system kept in IndexedDB. `python3 make.py engine --platform web` builds the player for WebGPU and for WebGL2, and the page of the web template picks WebGPU when the browser offers an adapter, while `?backend=webgpu` or `?backend=webgl2` forces one of them. The [distribution guide](distribution.md#web-loader) covers the page and the [build guide](build.md#web-builds) the web builds.
