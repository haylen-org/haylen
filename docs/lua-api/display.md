# 📖 `haylen.display`

The module `haylen.display` lists the displays of the device: the monitors of a desktop, the screen of a phone, a tablet or a TV, the external displays attached to them and the screens that a browser shows the page. It tells which display shows the window, asks a browser to show every screen and opens outputs: pictures of the app on displays other than the one of its window. The [displays guide](../platform/displays.md) explains how displays work on each platform.

```lua
local display = require('haylen.display')
```

## Displays

### display.list()

Returns every display of the device as a list of tables, in the order of the platform. The list always holds at least one display, and the primary one is flagged. A browser lists only the screen of the page until the person allows more with [`display.request()`](#displayrequest).

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | The id of the display. It stays the same while the display stays connected, and across runs where the platform knows the display again. The place of a display in the list changes as other displays connect and disconnect, so apps keep ids, not numbers. |
| `name` | string | The name that the system gives the display, or `''` where the system names none, as on iPhones, iPads and Android TVs without one. |
| `bounds` | `Rect` | The whole display, in points. |
| `workArea` | `Rect` | The part of the display that apps may use, in points: without the taskbar, the Dock and the menu bar of a desktop, the bars of the system of a phone or the edges that a TV crops. |
| `scale` | number | The pixels in a point, such as `2` on a Retina display. |
| `refreshRate` | number | The frames per second that the display shows, or `0` where the platform does not tell it, as browsers never do. |
| `primary` | boolean | `true` for the primary display: the main monitor of a desktop and the built-in screen of a phone or a tablet. |
| `modes` | list | The modes that the display can run in, each a table with `width` and `height` in pixels, `refreshRate` and `scale`. The function [`window.setFullscreen`](window.md#windowsetfullscreenenabled-options) takes one for exclusive fullscreen. The list is empty where the platform has no modes, as on the web. |

The displays of a desktop share the points of the desktop, from the top left corner of the primary display, as the [desktop guide](../platform/desktop.md#desktop-points) explains. Each display of a phone, a tablet, a TV or Android has its own points from its own top left corner. Browsers place their screens in CSS pixels of the screen space of the device.

```lua
local display = require('haylen.display')

for _, shown in ipairs(display.list()) do
    print(shown.id, shown.name, shown.bounds, shown.scale .. 'x', shown.refreshRate .. ' Hz')
    for _, mode in ipairs(shown.modes) do
        print('', mode.width .. 'x' .. mode.height, mode.refreshRate .. ' Hz')
    end
end
```

### display.current()

Returns the display that shows the window, as a table like those of `display.list()`. On desktops it is the display that holds most of the window. While the window is on a display that the platform has not reported yet, it returns the primary display.

```lua
local display = require('haylen.display')

local shown = display.current()
print('The window is on', shown.name, 'at', shown.refreshRate, 'frames per second.')
```

### display.request()

Asks the platform to list every display and returns a promise. The promise resolves with `true` once `display.list()` holds every display. Desktops, phones, tablets, TVs and Android list every display from the start, so their promise resolves at once.

A browser shows the page only its own screen until the person allows the page to see the others, through the Window Management API. The function then asks the person, which a browser allows only shortly after a click or a key. The screens that the answer adds reach the app as `displayConnected` events. The promise rejects with a failure table whose `code` is one of these:

| Code | Meaning |
| --- | --- |
| `denied` | The person or the browser did not let the page see every screen. |
| `unsupported` | The browser has no Window Management API, as Firefox and Safari do not. |

The app asked for the answer, so a rejection that nothing awaits never stops the app.

```lua
local async = require('async')
local display = require('haylen.display')

async.spawn(function()
    local listed, message, failure = display.request():await()
    if listed then
        print('Every display:', #display.list())
    elseif failure.code == 'unsupported' then
        print('This browser shows only the screen of the page.')
    else
        print(message)
    end
end)
```

## Outputs

### display.open(id, options)

Opens an output on the display with the id and returns it as an [`Output`](#output). An output is what the app shows on a display other than the one of its window. The app draws into the render target of the output with any canvas, and the engine shows the target on the display at the end of every frame, as large as it fits, with bars of the clear color around it. The [displays guide](../platform/displays.md#content-on-another-display) explains where outputs work and how they fit the engine.

| Option | Default | Meaning |
| --- | --- | --- |
| `fullscreen` | `true` | On a desktop, `true` opens a window that covers the display, and `false` a window of `width` by `height` points in the middle of it, which the person can move and resize. Other platforms always cover the display. |
| `width`, `height` | `1280`, `720` | The size of the window of a desktop output that is not fullscreen, in points. |
| `title` | `''` | The title of the window of a desktop output. |
| `clearColor` | `'#FF000000'` | The color of the bars around the target. |

The output opens without a target. The event `outputShown` announces that the display shows it, with the size of its target in pixels. A display that is not connected raises `There is no display "<id>" to open an output on. The function "display.list()" lists the displays.`, the display of the window raises `The display "<id>" shows the window of the app, so it shows no output.`, and a display that shows another output raises `The display "<id>" shows another output already.`.

```lua
local display = require('haylen.display')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local board = nil
local camera = graphics2d.newCamera()

events.on('displayConnected', function(info)
    board = display.open(info.display, {clearColor = '#FF101820'})
end)

scene.push({
    render = function(self)
        if board and board.shown then
            camera.x, camera.y = board.width / 2, board.height / 2
            graphics2d.beginTarget(board.target, camera, {clear = '#FF203040'})
            graphics2d.drawCircle(board.width / 2, board.height / 2, 120 * board.scale, '#FFFFD040')
        end
    end,
})
```

### Output

An output of the app on another display, which `display.open` returns.

| Property | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The number of the output, which its events carry as `output`. |
| `display` | string | The id of its display. |
| `state` | string | `'opening'` until the display shows it, `'shown'`, `'hidden'` while the platform holds it back, and `'closed'`. |
| `shown` | boolean | `true` while the display shows it. |
| `ending` | string or `nil` | Once closed, why: `'app'` for `output:close()`, `'person'` for a window that the person closed, `'disconnected'` for a display that went away, or `'unsupported'` where the platform or the graphics API cannot show it there. |
| `width`, `height` | integer | The size of its target in pixels, the size of the drawable of its display, or `0` before it first shows. |
| `scale` | number | The pixels in a point of its display. |
| `target` | `RenderTarget` or `nil` | The render target that the app draws into, `nil` before the output first shows. It changes when the display changes its size. |
| `clearColor` | `Color` | The color of the bars around the target. It can be set. |

### output:close()

Closes the output, which publishes `outputClosed` with the ending `'app'`. Closing a closed output does nothing.

```lua
local display = require('haylen.display')

local second = display.list()[2]
if second and second.id ~= display.current().id then
    local board = display.open(second.id)
    board:close()
end
```

## Events

The displays publish these events on [`haylen.events`](events.md#engine-events). A change that touches several displays publishes one event for each, the displays that went away first.

| Event | When | Data |
| --- | --- | --- |
| `displayConnected` | A display connected, or a browser showed the page another screen. | `{display, name}`, the id and the name of the display. |
| `displayDisconnected` | A display disconnected. | `{display, name}` |
| `displayChanged` | A display changed its bounds, work area, scale, refresh rate, modes or name, such as when the taskbar moved or the resolution changed. | `{display, name}` |
| `windowDisplayChanged` | The window moved to another display, as `display.current()` reports it. | `{display}`, the id of the display. |
| `outputShown` | The display shows an output, the first time or after it was hidden. | `{output, display, width, height, scale}`, where `output` is the `id` of the output and the size is in pixels. |
| `outputResized` | The target of an output that shows changed its size with its display. | `{output, display, width, height, scale}` |
| `outputHidden` | The platform holds an output back, such as while the app is in the background. | `{output}` |
| `outputClosed` | An output closed. | `{output, display, ending, reason}`, where `ending` is one of the endings of [`Output`](#output) and `reason` comes with `'unsupported'`. |
| `outputInput` | The pointer over the window of an output pressed, released, moved or scrolled. The input of the window of the app never hears it. | `{output, type, x, y}` in the points of the output, where `type` is `'mouseDown'`, `'mouseUp'`, `'mouseMove'`, `'mouseScroll'`, `'touchBegan'`, `'touchMoved'`, `'touchEnded'` or `'touchCancelled'`, with `button` for the presses of the mouse, `scrollX` and `scrollY` for a scroll, and `touches`, a list of `{id, x, y, changed}`, for the fingers. |

```lua
local display = require('haylen.display')
local events = require('haylen.events')

events.on('displayConnected', function(info)
    print('Connected:', info.name, display.list()[#display.list()].bounds)
end)
events.on('windowDisplayChanged', function(info)
    print('The window moved to', info.display)
end)
```

## C++

C++ code reads the displays through `platform::Displays` of `haylen/platform/Displays.hpp`, which `core::Engine::getDisplays()` returns, and opens outputs through `platform::Outputs` of `haylen/platform/Outputs.hpp`, which `core::Engine::getOutputs()` returns. Its `open(id, options)` returns a `std::shared_ptr<platform::Output>` with the `getTarget()` that the app draws into. Its `getAll()` returns the `platform::Display` values of `haylen/platform/Display.hpp`, `find(id)` returns the display with an id or null, `getWindowDisplay()` returns the display of the window, and `request(callback)` calls back on a later frame with a `platform::Window::DisplayAccess`. The platform reports its displays through `platform::Window::getDisplays()` and `getDisplayId()`.

## Errors

These messages are raised by the functions of the module.

| Message | Cause |
| --- | --- |
| `The person or the browser did not let the app see every display.` | The rejection of `display.request()` with the code `denied`. |
| `The browser cannot list the screens of the device, since it has no Window Management API.` | The rejection of `display.request()` with the code `unsupported`. |
| `There is no display "<id>" to open an output on. The function "display.list()" lists the displays.` | The function `display.open` got a display that is not connected. |
| `The display "<id>" shows the window of the app, so it shows no output.` | The function `display.open` got the display of the window. |
| `The display "<id>" shows another output already.` | The function `display.open` got a display that shows an open output. |
| `The window of an output needs a positive "width" and "height" in points.` | The options of `display.open` have a size that is not positive. |
