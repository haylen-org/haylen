# 📚 Haylen documentation

The documentation of Haylen has two parts. The guides explain how each block of the engine works, and the reference lists the whole Lua API. Each block has its own index with its pages. New to Haylen? Start with the [quick start](../README.md#quick-start), then read the [Lua guide](core/lua.md) and the [lifecycle guide](core/lifecycle.md).

## Guides

| Name | Description | Link |
| --- | --- | --- |
| 📐 2D | The 2D block holds the modules that build 2D games and apps: retained node trees, frame and skeletal animation, particles, lighting, physics, Tiled maps, navigation, spatial queries and procedural generation. Their Lua modules end with `2d`, so 3D modules can grow next to them. | [`2d/README.md`](2d/README.md) |
| 🔊 Audio | Audio mixes sounds and music through buses with effects, follows the pause and the sessions of each platform and places sounds in the world. | [`audio/README.md`](audio/README.md) |
| 📦 Content | Content brings the files of an app to the device and into memory, protects them in releases and updates them. | [`content/README.md`](content/README.md) |
| ⚙️ Core | The core runs an app: the engine and its plugins, the frame, the threads, the Lua runtime, scenes, events, timers, tweens and jobs, and the values and services that every other module builds on. | [`core/README.md`](core/README.md) |
| 🎨 Graphics | Graphics draws everything an app shows: the GPU device and its graphics APIs, textures and render targets, the 2D renderer with its canvases, cameras and batching, shaders, text in every script and the mapping of design units to the screen. | [`graphics/README.md`](graphics/README.md) |
| 🎮 Input | Input reads every device and gives gameplay named actions, with the timing that action games need. | [`input/README.md`](input/README.md) |
| 🌐 Network | Network reaches servers and peers from every platform, with every result on the frame thread. | [`network/README.md`](network/README.md) |
| 📱 Platform | Platform covers the window and the device, the services every platform offers and the ways an app reaches native code. | [`platform/README.md`](platform/README.md) |
| 🛠️ Tools | Tools build, test, debug and ship the engine and its apps. | [`tools/README.md`](tools/README.md) |
| 🪟 UI | The UI draws the interface players see and the tools developers use, in design units, inside the safe area and with themes. | [`ui/README.md`](ui/README.md) |

## Reference

| Name | Description | Link |
| --- | --- | --- |
| 📖 Lua API | Every Lua module of Haylen, with its functions, types, options, events and errors. | [`lua-api/README.md`](lua-api/README.md) |
