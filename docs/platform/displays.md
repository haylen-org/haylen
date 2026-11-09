# 🖥️ Displays

This guide explains how Haylen sees the displays of a device: the monitors of a desktop, the screen of a phone, a tablet or a TV, the external displays attached to them and the screens of a browser. It covers what a display is, how its id stays the same, where each platform gets its displays from, how an app follows them as they change, how the window goes fullscreen on a chosen display and how the app shows content on another display. The reference of [`haylen.display`](../lua-api/display.md) lists every function and field.

## What a display is

A display is a surface that the device shows pictures on. Each one has an id, a name, its whole area in `bounds`, the part that apps may use in `workArea`, the pixels in a point in `scale`, the frames per second it shows in `refreshRate`, and the modes it can run in. One display is primary: the main monitor of a desktop, or the built-in screen of a phone or a tablet.

The app runs in one window, which one display shows. The function `display.current()` returns that display. On a desktop it is the display that holds most of the window.

```lua
local display = require('haylen.display')

local shown = display.current()
print(shown.name, shown.bounds.width .. 'x' .. shown.bounds.height, shown.refreshRate .. ' Hz')
```

## Ids, not numbers

The place of a display in the list changes as other displays connect and disconnect. So the second display of today may be the first one tomorrow. The id of a display stays the same while it stays connected. On the platforms that know a display again, it also stays the same across runs, so an app can save it in its preferences and find the display the next time.

| Platform | Id | Known again across runs |
| --- | --- | --- |
| macOS | The vendor, the model and the serial number of the monitor, such as `0610-a052-00000000`. Two monitors that report the same ones count up with `-2`, `-3` in the order of the screens. | Yes. |
| Windows | The device path of the monitor in the display configuration of Windows, which names the monitor and the port it is on. | Yes. |
| Linux | The name of the connector of RandR, such as `DP-1` or `HDMI-1`, or the name of the Wayland output in a Wayland session without X11. | Yes, while the monitor stays on its connector. |
| iOS and iPadOS | `main` for the built-in screen, and `external-1`, `external-2` and on for the other screens in the order the app first sees them. | Only `main`. |
| tvOS | `main`, the TV. | Yes. |
| visionOS | `main`, the window of the app, since visionOS has no screens. | Yes. |
| Android | The number of the display that the display manager gives, such as `0` for the built-in screen. | Only `0`. |
| Web | `screen-1`, `screen-2` and on, in the order the page first sees each screen. | No. |

## Points and areas

The displays of a desktop share the points of the desktop, with the origin at the top left corner of the primary display and y down, as the [desktop guide](desktop.md#desktop-points) explains. So `bounds` tells where each monitor sits next to the others, and `window.place` puts the window on any of them. Every other platform gives each display its own points, from its own top left corner, since its displays share no desktop. Browsers place their screens in the CSS pixels of the screen space of the device.

The work area leaves out what the system keeps for itself: the taskbar of Windows, the Dock and the menu bar of macOS, the work area of the window manager of Linux, the safe area of the window of the app on iPhones and iPads, the edges that a TV crops on an external display, the bars of the system of Android, and the space that a browser keeps for the bars of the system.

## Where the displays come from

| Platform | Source | Refresh rate and modes |
| --- | --- | --- |
| macOS | The screens of AppKit and the displays of Core Graphics. | The most frames per second of the screen, and every mode that the desktop can run in, the ones of low resolution included. |
| Windows | The monitors of the desktop and the display configuration, which gives the friendly name and the device path. | The rate of the current mode, and every mode in true color. |
| Linux | The monitors of RandR through X11, and their first output, also through XWayland in a Wayland session. A Wayland session without X11 reads the outputs that the compositor announces through SDL. | The rate of the mode of the output, and its modes. |
| iOS and iPadOS | The screen of the device, and the screen of every scene of the app, such as the scene that UIKit connects for an external display, or a window that Stage Manager moved to an external display. | The most frames per second of the screen, and the sizes of its modes. |
| tvOS | The TV. | The most frames per second of the screen. tvOS lists no modes. |
| visionOS | The window of the app in the space around the person, with its size in points, the scale of its traits and its safe area as the work area, since visionOS shows apps in windows and has no screens. | None, since visionOS tells neither. |
| Mac Catalyst | The screen of the window. | As on iPadOS. |
| Android | The display manager, which reports the built-in screen, external displays, cast screens and the secondary screens of a device. | The rate of the display and its modes. |
| Web | The screen of the page, and every screen of the device once the person allows it through the Window Management API. | None, since browsers tell neither. |

## Following the changes

The engine reads the displays when the app starts and again whenever the platform reports a change. Each display that the change touched publishes one event: `displayConnected`, `displayDisconnected` or `displayChanged`. The event `windowDisplayChanged` follows the window as it moves to another display. A window that moves to a display of another scale also changes its framebuffer, which `windowResized` reports, and the viewport follows on its own.

```lua
local display = require('haylen.display')
local events = require('haylen.events')

local function describe()
    for _, shown in ipairs(display.list()) do
        print(shown.id, shown.name, shown.primary and 'primary' or '')
    end
end

describe()
events.on('displayConnected', describe)
events.on('displayDisconnected', describe)
```

## The window on a display

On a desktop the window moves among the displays. The function `window.place` puts it on a display by its id or its number, and `window.setFullscreen(true, {display = id})` covers a display at the mode that the desktop runs in. A mode of the display makes the fullscreen exclusive: the display runs in that mode, such as a lower resolution or another refresh rate, while the window holds it, and goes back to its own mode when the window leaves fullscreen. A window that moves to a display of another scale keeps its size in points, and its framebuffer follows the scale.

| Platform | Placement | Fullscreen on a display | Exclusive mode |
| --- | --- | --- | --- |
| macOS | Yes. | Yes, in a space of its own, as macOS keeps fullscreen windows. | Yes. |
| Windows | Yes. | Yes. | Yes. |
| Linux | Yes, on X11. Wayland places windows itself, and the app runs through XWayland. | Yes. | Yes. |
| Web | No, the page has no desktop. | Yes, on any screen once the person let the page see every screen. | No, browsers have no modes. |
| iOS, iPadOS, tvOS, visionOS, Android | No. | The window always fills its display. | No. |

```lua
local display = require('haylen.display')
local window = require('haylen.window')

-- Play on the fastest display in its fastest mode.
local fastest, best = nil, nil
for _, shown in ipairs(display.list()) do
    for _, mode in ipairs(shown.modes) do
        if not best or mode.refreshRate > best.refreshRate then
            fastest, best = shown, mode
        end
    end
end
if fastest then
    window.setFullscreen(true, {display = fastest.id, mode = best})
end
```

## Content on another display

The app runs in one window, and it can show more on the other displays through outputs. An output is a picture of the app on a display other than the one of its window, such as a scoreboard on a TV next to a laptop, the map of a game on a second monitor, or the slides of a talk on a projector while the iPad shows the notes.

The function `display.open(id)` opens an output. The output has a render target of the size of its display in pixels. The app draws into it with any canvas, such as `graphics2d.beginTarget(output.target, camera)`, lit and post-processed like any render target. At the end of every frame the engine shows the target on the display, as large as it fits, with bars of the clear color around it. A frame in which the app draws nothing into the target shows what the target held, so a still picture costs one copy per frame.

```lua
local display = require('haylen.display')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local outputs = {}
local camera = graphics2d.newCamera()

events.on('displayConnected', function(info)
    outputs[info.display] = display.open(info.display, {clearColor = '#FF000000'})
end)
events.on('outputClosed', function(info)
    outputs[info.display] = nil
end)

scene.push({
    render = function(self)
        for _, output in pairs(outputs) do
            if output.shown then
                camera.x, camera.y = output.width / 2, output.height / 2
                graphics2d.beginTarget(output.target, camera, {clear = '#FF14202C'})
                graphics2d.drawText(nil, 'Score 42', output.width / 2, output.height / 2, {size = 96 * output.scale})
            end
        end
    end,
})
```

An output opens, shows, may hide and closes, and each step publishes an event. It shows once the platform gave it a surface, which `outputShown` announces with the size of its target. It hides while the platform holds its surface back, which `outputHidden` announces, and the target keeps its picture meanwhile. It closes when the app closes it, the person closes its window, its display disconnects, or the platform or the graphics API cannot show it there, which `outputClosed` announces with the ending. The pointer over the window of an output reaches the app as `outputInput`, in the points of the output, apart from the input of the window of the app. The keyboard stays with the app, and the window of the app keeps the focus while the person types in the window of an output.

| Platform | Output | Input | Status |
| --- | --- | --- | --- |
| macOS | A window of its own that covers the display, in a space of its own, or a window of the chosen size in the middle of it. | The mouse over the window. | Works with Metal. |
| Windows | A window of its own, as on macOS. | The mouse over the window. | Works with Direct3D 12, Direct3D 11, Vulkan and OpenGL. |
| Linux | A window of its own, as on macOS, on X11. | The mouse over the window. | Works with Vulkan and OpenGL. |
| iOS and iPadOS | A window in the scene that UIKit connects for an external display, such as a TV through a cable or AirPlay. From iOS 27, UIKit connects that scene only for an app that registered the scene accessory of external displays, which [`system.requestPermission('screens')`](../lua-api/system.md#systemrequestpermissionname) does, and the display mirrors the app until then. | None, since UIKit gives the scene of an external display no input. | Works with Metal. A display that Stage Manager extends the windows of an iPad to has no such scene, so an output there ends as `unsupported`. |
| tvOS, visionOS, Mac Catalyst | None, since these platforms show an app on the display of its window alone. | None. | Ends as `unsupported`. |
| Android | A `Presentation` of the display manager that covers the display, such as an external display, a cast screen or the secondary screen of a device, with a view whose surface the device draws into. Android shows presentations only on displays other than the built-in one, so an output on the built-in screen of an app that runs on another display ends as `unsupported` with the reason of Android. The window manager of Android takes a display a moment after the display manager reports it, so an output opened right as `displayConnected` arrives may end the same way, and opening it again a moment later shows it. | The touches on the display, where it takes touch. | Works with Vulkan and OpenGL ES. |
| Web | A window of the browser that the page opens on the screen of the output through the Window Management API, which covers the work area of that screen or sits in the middle of it, with a canvas that fills it. A browser opens a window only shortly after a click or a key of the person, so an app opens an output from a button, and a window that the browser blocks ends the output as `unsupported` with the reason. A page cannot make that window fullscreen, since fullscreen needs a gesture inside it. | The mouse over the canvas. | Draws through WebGPU. A WebGL2 context belongs to its one canvas and shares no object with another one, so with WebGL2 the output ends as `unsupported`. |

The engine draws each output through the GPU device of the app, the one that draws the window. The [architecture guide](../core/architecture.md#outputs-on-other-displays) explains how outputs fit the frame, and the [guide of the render hardware interface](../graphics/rhi.md#surfaces) lists the devices that draw into several surfaces.

## Every screen of a browser

A page sees only the screen it shows on, until the app holds the [permission](../lua-api/system.md#permissions) `'screens'`. The function `system.requestPermission('screens')` asks for it through the Window Management API, which Chrome and Edge offer. A browser asks the person only shortly after a click or a key, so an app asks from a button. The screens that the answer adds reach the app as `displayConnected` events. A browser without the API rejects the promise with the code `unsupported`, and a refusal answers `'denied'`. On iPhones and iPads from iOS 27 the request lets the scene of an external display connect, as the table of outputs above explains. Every other platform answers `'granted'` at once, since the list already holds every display.

```lua
local async = require('async')
local display = require('haylen.display')
local system = require('haylen.system')
local ui = require('haylen.ui')

ui.mount(ui.button{text = 'Use every screen', onClick = function()
    async.spawn(function()
        local state, message = system.requestPermission('screens'):await()
        print(state == 'granted' and ('Screens: ' .. #display.list()) or message or state)
    end)
end})
```

## C++

C++ code reads the displays through `platform::Displays`, which `core::Engine::getDisplays()` returns, and each display is a `platform::Display`. The platform reports them through `platform::Window::getDisplays()` and `getDisplayId()`, and a browser lists every screen once the app holds the permission `Permission::Kind::Screens`.

```cpp
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/platform/Displays.hpp"

class DisplaysApp final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        for (const haylen::platform::Display& display : engine.getDisplays().getAll()) {
            haylen::core::Log::info("{} {} at {} Hz", display.id, display.name, display.refreshRate);
        }
    }
};
```
