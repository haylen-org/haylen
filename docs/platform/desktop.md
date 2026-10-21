# 🖥️ Desktop apps

This guide is for apps that run on Windows, macOS and Linux and want more than a plain window. On the desktop a Haylen window can drop its title bar and border, and let the desktop show through its transparent pixels. It can float above the other windows, stay out of the taskbar and the Dock, refuse the keyboard focus, let clicks pass through to the windows behind it and move when the player drags its content. Together these make apps that live on the desktop: a strip above the taskbar where a little hero walks and fights while the player works, a pet that walks along the bottom of the screen, or an overlay that shows information over other apps. The guide shows how the options fit together and what each system allows. The [`haylen.window` reference](../lua-api/window.md#desktop-windows) lists every function, and `samples/games/taskbar-quest` is a complete game built this way.

## The window options

The `window` section of `app.json` opens the window with these options already in effect. So a frameless window never shows a title bar, not even for its first frame. Each option also changes while the app runs.

```json
{
    "window": {
        "width": 1280,
        "height": 180,
        "decorated": false,
        "transparent": true,
        "alwaysOnTop": true,
        "showInTaskbar": true,
        "focusable": false,
        "mousePassthrough": true,
        "resizable": false,
        "position": {"anchor": "bottom", "area": "work", "fill": "width"}
    }
}
```

| Option | What it does | Changes while the app runs with |
| --- | --- | --- |
| `decorated` | The value `false` removes the title bar and the border. | `window.setDecorated` |
| `transparent` | The window opens able to be transparent, and transparent, so the desktop shows through its transparent pixels. | `window.setTransparent`, while the window opened transparent. |
| `alwaysOnTop` | The window floats above normal windows. | `window.setAlwaysOnTop` |
| `showInTaskbar` | The value `false` leaves the window out of the taskbar and the window switcher. On macOS it also removes the Dock icon and the menu bar. | `window.setShowInTaskbar` |
| `focusable` | The value `false` keeps clicks from activating the app or taking the keyboard. | `window.setFocusable` |
| `mousePassthrough` | The value `true` lets every click through, until the app gives regions. | `window.setMousePassthrough` |
| `position` | Where the window opens. | `window.place`, `window.setFrame` |

Every start of the app applies these options again. So an app that restarts from its package, such as after a hot reload, gets the window it asks for, while the window stays where the player moved it.

## Placing the window

### Desktop points

Frames and monitors are in desktop points. The origin is the top left corner of the primary monitor, and y grows down.

- On macOS a desktop point is a point of the system.
- On Windows and Linux a desktop point is a pixel divided by the scale of the primary monitor. The window size of `app.json` scales the same way, so a window of 1280 by 180 points has a frame of 1280 by 180 points on every system.

### Where the window goes

The option `position` of `app.json` and the function [`window.place`](../lua-api/window.md#windowplaceposition) take the same values:

- `"center"`, the middle of the work area.
- A point, such as `{"x": 40, "y": 60}`.
- An anchored placement, which names the side or the corner the window touches with `anchor`. It may also name the `area`, the whole monitor or the work area without the taskbar, the Dock and the menu bar, the `monitor`, an `offset` in points and a `fill` that stretches the window across the area. The [reference](../lua-api/window.md#windowplaceposition) lists the fields and their defaults.

The function `window.monitors()` returns every monitor with its whole area in `bounds`, the work area in `workArea`, and the number of pixels in a point in `scale`. The function `window.frame()` returns the content area of the window, `window.setFrame` moves and resizes it, and `window.currentMonitor()` returns the monitor that holds most of it.

Two events of [`haylen.events`](../lua-api/events.md#engine-events) follow the desktop. The event `windowMoved` follows every move, by the player, the app or the system. The event `windowMonitorsChanged` follows monitors that connect, disconnect or change, and work areas that change when the taskbar or the Dock moves.

```lua
local events = require('haylen.events')
local window = require('haylen.window')

-- Keep the strip above the taskbar when the monitors change.
events.on('windowMonitorsChanged', function()
    window.place({anchor = 'bottom', fill = 'width'})
end)
```

## Transparency

A transparent window lets the desktop show through wherever the app draws nothing. It clears to `#00000000` unless `clearColor` says otherwise.

The engine keeps its colors premultiplied by their alpha, from the first draw to the window. Premultiplied means that each color channel is already multiplied by the coverage of the pixel. So every part of the engine blends correctly over the transparent clear:

- Sprites, text and the UI blend over the transparent clear.
- Lit canvases keep the coverage of the scene through the light map, and add the coverage of emissive light.
- Post-processing grades colors around the coverage, and fades cover the canvas with an opaque color.
- Scene transitions draw their captured scenes premultiplied.

A pixel with half its coverage shows half of the desktop behind it, the way the system composes the window. An additive draw adds its alpha too, so a glow over the desktop covers part of what is behind it.

The option `transparent` of `app.json` decides, before the window opens, whether the window can be transparent at all, because it chooses how the system composes the window. Such a window can then turn opaque and transparent again with `window.setTransparent`. While it is opaque, the engine clears the alpha of every frame to 1 and draws no alpha at all, whatever the clear color and the blend modes. So no pixel lets the desktop through, and on macOS the window also gets the background and the shadow of a normal window. A window that opened opaque stays opaque.

```lua
local window = require('haylen.window')

if window.canBeTransparent() then
    window.setTransparent(not window.transparent())
end
```

## Dragging the window

A window without a title bar moves with `window.startDrag()`. The function hands the window to the system for as long as the left mouse button stays down. The system moves the window smoothly, even while the app is busy, and the app hears the release of the button once the drag ends. So call it on the press of the button, never on a click, which comes after the release.

```lua
local scene = require('haylen.scene')
local window = require('haylen.window')

local grip = {x = 0, y = 140, width = 48, height = 40}

scene.push({
    event = function(self, event)
        local inside = event.type == 'mouseDown' and event.x >= grip.x and event.x < grip.x + grip.width and event.y >= grip.y and event.y < grip.y + grip.height
        if inside and event.button == 'left' then
            window.startDrag()
        end
    end,
})
```

## Clicks through the window

The call `window.setMousePassthrough(true)` lets every click through the window, for an overlay that only shows information. A list of regions lets clicks through everywhere except the regions, which keep the mouse for the app. In a strip the regions are the hero, the enemies, the buttons and the grip, while the empty sky around them passes clicks to the desktop.

Regions are rectangles or polygons in design units, converted with the viewport of the moment, or in framebuffer pixels with `'pixels'`. An app whose clickable things move gives its regions every frame, which costs little.

```lua
local scene = require('haylen.scene')
local window = require('haylen.window')

local hero = {x = 300, y = 110, width = 64, height = 64}
local buttons = {x = 1400, y = 20, width = 180, height = 60}

scene.push({
    update = function(self, dt)
        hero.x = hero.x + 60 * dt
        window.setMousePassthrough({hero, buttons, {{0, 180}, {1600, 180}, {1600, 200}, {0, 200}}})
    end,
})
```

The window keeps the mouse it has while a button is down. So a press that starts on a region also ends in the app, and a drag that starts on the desktop behind is not cut off when it crosses a region.

## Focus and the taskbar

A game that the player keeps next to their work should not take the keyboard when clicked. The option `focusable = false` keeps the app inactive: clicks reach the app, and key presses go to the app the player was typing in. Such a game is played with the mouse. A normal window mode with `window.setFocusable(true)` can bring the keyboard back.

The option `showInTaskbar = false` makes the window a tool window without a taskbar button on Windows and Linux. On macOS it makes the app an accessory app without a Dock icon or a menu bar. A macOS app that `haylen.py` or `haylen_add_app` packages with `showInTaskbar` set to `false` in `app.json` also declares `LSUIElement`, so its Dock icon never appears, not even while it starts. A window out of the taskbar needs a way to quit, such as a button of the UI.

```lua
local window = require('haylen.window')

-- A normal window mode that takes the keyboard and has a taskbar button.
window.setFocusable(true)
window.setShowInTaskbar(true)
```

## Menus and quitting

On macOS the player, the apps of the Apple template and C++ apps are regular apps. They have a Dock icon and come to the front when they start. They have the standard main menu:

- The app menu, with About, Hide (Command+H), Hide Others (Option+Command+H), Show All and Quit (Command+Q).
- The Edit menu, with Undo (Command+Z), Redo (Shift+Command+Z), Cut (Command+X), Copy (Command+C), Paste (Command+V), Delete and Select All (Command+A). These act on the text field that is being edited. They stay disabled while none is, so the keys then reach the app.
- The Window menu, with Minimize (Command+M), Zoom and Enter Full Screen. The system enables each one only while the window allows it.

Quit in the menu, Command+Q, Quit in the Dock and logging out all close the window as its close button does. So the app hears `appQuitRequested` and stops through the [lifecycle](../core/lifecycle.md) of the engine. On Windows, Alt+F4 closes the window the same way. On Linux the shortcut of the window manager does, usually Alt+F4.

An app without a taskbar button has neither a Dock icon nor a menu bar on macOS, and a window that refuses the focus never receives these shortcuts. Such apps quit through their own UI, such as a button that calls `haylen.quit()`. The process exits with the status that [`haylen.quit`](../lua-api/haylen.md#haylenquitoptions) gives it, and with 0 when the window closes.

```lua
local haylen = require('haylen')
local ui = require('haylen.ui')

ui.mount(ui.button{text = 'Quit', anchor = 'topRight', margin = 16, onClick = function() haylen.quit() end})
```

## The taskbar strip

A strip above the taskbar puts these pieces together:

1. The file `app.json` opens a frameless, transparent window above the others. The window is anchored to the bottom of the work area and fills its width. Its design size is shaped like the strip, with `"expand"` scaling, so the visible width follows the monitor.
2. The scene draws the ground, the hero and the UI, and leaves the rest of the strip transparent.
3. Every frame, the scene gives the regions of what the player can click. Presses on the ground or on a grip call `window.startDrag()`.
4. A button switches to a normal window and back. The normal window takes `setTransparent(false)`, `setDecorated(true)`, `setAlwaysOnTop(false)`, `setFocusable(true)`, `setMousePassthrough(false)` and a frame in the middle of the screen. The strip comes back with its options, `setTransparent(true)` and `window.place({anchor = 'bottom', fill = 'width'})`.
5. The event `windowMonitorsChanged` places the strip again when the taskbar moves or a monitor changes.

```json
{
    "name": "Strip",
    "window": {"width": 1280, "height": 200, "decorated": false, "transparent": true, "alwaysOnTop": true, "focusable": false, "resizable": false, "mousePassthrough": true, "position": {"anchor": "bottom", "fill": "width"}},
    "design": {"width": 1600, "height": 200, "scaling": "expand"}
}
```

```lua
local window = require('haylen.window')

local function showNormalWindow()
    window.setTransparent(false)
    window.setDecorated(true)
    window.setAlwaysOnTop(false)
    window.setFocusable(true)
    window.setMousePassthrough(false)
    window.place('center')
end

local function showStrip()
    window.setDecorated(false)
    window.setAlwaysOnTop(true)
    window.setFocusable(false)
    window.setTransparent(true)
    window.place({anchor = 'bottom', fill = 'width'})
end

showNormalWindow()
showStrip()
```

## Platforms

| Platform | What works |
| --- | --- |
| macOS | Everything. The window becomes a borderless window that can still take the keyboard when it is focusable. While on top it floats at the level of panels and joins every space. The Dock icon hides through the activation policy of the app. AppKit decides whether a window takes the mouse for the whole window. So passthrough follows the mouse every frame and lets the window ignore it outside the regions, while the app keeps hearing the mouse move over the window. An unfocusable window takes clicks without activating the app. |
| Windows | Everything with the Direct3D 11 graphics API, the default. Transparency renders into a DirectComposition swapchain with premultiplied alpha, and builds that draw with OpenGL open opaque windows. A frameless window is a popup without a sizing border, so the app resizes it itself. Passthrough makes the window layered and transparent to clicks outside the regions, following the mouse every frame, while the app keeps hearing the mouse move. The system moves dragged windows in its own move loop, during which the app keeps drawing. An unfocusable window does not activate when clicked. |
| Linux | X11 with a window manager that follows the Extended Window Manager Hints, as the common desktops do. Transparency needs a compositing window manager that offers 32-bit ARGB visuals. Without one the app stops with a clear error instead of opening an opaque window. Decorations follow the Motif hints, and a window manager may ignore them. Passthrough sets the input shape of the window, so the app hears the mouse only over the regions. Desktop points follow `Xft.dpi`, and the work area of each monitor is the work area of the desktop inside the monitor. Wayland sessions run the app through XWayland. |
| Web | The canvas is the window, and the page gives it no desktop. A transparent app has a transparent canvas, and the page of the template drops its own background. So whatever holds the page shows through, such as an editor that embeds it in a frame. The other options keep their values without effect. The function `window.frame()` returns the canvas, and `window.monitors()` returns the canvas as the only monitor. |
| iOS, iPadOS, tvOS, Android | Apps fill the screen, so the desktop options do not apply. The setters keep their values without effect. The function `window.frame()` returns the screen in points, and `window.monitors()` returns the screen as the only monitor. |

## Shipping a desktop app

The player runs any app during development, and the project of each platform ships it. That is the macOS target of the Apple template, and the CMake projects of the Windows and Linux templates. The tool `haylen.py` builds and installs those with `haylen run <app> --platform windows` or `--platform linux`, as the [distribution guide](../tools/distribution.md#windows-and-linux) describes.

- The executable takes the name, the version, the description, the company and the copyright of `app.json`. The version resource shows them on Windows, and the desktop entry and the AppStream metainfo show them on Linux.
- On Windows the window and its taskbar button take the first icon of the executable, the icon of the project in `resources/app.ico`. On Linux the class of the window is the identifier of `app.json`. The identifier names the desktop entry and the icons that the project installs, so the desktop shows them for the window.
- The command `haylen dist <app> --platform windows` makes the installer of the app with Inno Setup, and `--platform linux` makes a `.deb` and an AppImage, as [dist](../tools/distribution.md#dist) describes.
- Windows builds sign the executable, the installer and the uninstaller with the certificate that the environment names. Continuous integration takes it from the secrets `WINDOWS_CERTIFICATE` and `WINDOWS_CERTIFICATE_PWD`, as [signing](../tools/distribution.md#signing) describes. Without a certificate the app stays unsigned, and the build says so.

```sh
haylen run ~/apps/strip --platform windows
haylen dist ~/apps/strip --platform linux
```

## C++

C++ apps reach the same options through `platform::Window`, which `core::Engine::getWindow()` returns. It has `setDecorated`, `setAlwaysOnTop`, `setShowInTaskbar`, `setFocusable`, `getFrame` and `setFrame`, `setMousePassthrough` with regions in framebuffer pixels, `startDrag`, `getMonitors` and `getCurrentMonitor`, and `canBeTransparent`, `isTransparent` and `setTransparent`. The class `platform::WindowPlacement` reads and resolves the positions of `app.json`, and `core::AppConfig::Window` holds the options of `app.json`.

```cpp
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/WindowPlacement.hpp"

class StripApp final : public haylen::core::Application {
  public:
    void start(haylen::core::Engine& engine) override {
        haylen::platform::Window& window = engine.getWindow();
        const auto placement = haylen::platform::WindowPlacement::fromJson({{"anchor", "bottom"}, {"fill", "width"}});
        window.setFrame(placement.resolve(window.getMonitors(), window.getFrame().getSize()));
        window.setMousePassthrough(haylen::platform::Window::Passthrough::Whole, {});
    }
};
```
