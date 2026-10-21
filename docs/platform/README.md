# 📱 Platform

Platform covers the window and the device, the services every platform offers and the ways an app reaches native code.

## Guides

| Name | Description | Link |
| --- | --- | --- |
| Desktop | Frameless, transparent, always-on-top and click-through windows, their placement and monitors, menus and quitting. | [`desktop.md`](desktop.md) |
| Native code | Native libraries called from Lua, their callbacks and threads, packaging them with an app and integrating SDKs with a C API. | [`native.md`](native.md) |
| Platform bridge | The JSON bridge between Lua and native code: calls, events, bytes, timeouts, screens and the native side on every platform. | [`platform-bridge.md`](platform-bridge.md) |
| Plugins | Native plugins from `haylen.py plugin new` to publishing: `plugin.json`, the Lua API, the services of the engine, screens, requirements, the Apple, Android, web and desktop parts and the demo plugin. | [`plugins.md`](plugins.md) |

## Lua API

| Name | Description | Link |
| --- | --- | --- |
| `haylen.accessibility` | The accessibility settings of the system with their changes: text scale, reduced motion, high contrast, reduced transparency and screen readers. | [`lua-api/accessibility.md`](../lua-api/accessibility.md) |
| `haylen.dialogs` | Native message boxes and pickers of files to open, of a destination to save to and of a folder, which never block the app. | [`lua-api/dialogs.md`](../lua-api/dialogs.md) |
| `haylen.native` | Native libraries for Varn's `ffi`, found where the app ships them, their symbols, callbacks from any thread and the interface of the engine for libraries. | [`lua-api/native.md`](../lua-api/native.md) |
| `haylen.platform` | The JSON bridge to native code, such as sign-in and purchases, with bytes next to the JSON, typed errors, timeouts, cancellation, calls without answers, retained and batched events, the handles of the plugins of the app and the video and audio streams of their native parts. | [`lua-api/platform.md`](../lua-api/platform.md) |
| `haylen.system` | What the device and its operating system are, the theme and the battery with their changes, opening urls and vibrating. | [`lua-api/system.md`](../lua-api/system.md) |
| `haylen.window` | Window size, fullscreen, title, cursor, on-screen keyboard, clipboard, orientation, status bar, back button and folds, and on desktops frameless, transparent, always-on-top and click-through windows with their frame and monitors. | [`lua-api/window.md`](../lua-api/window.md) |
