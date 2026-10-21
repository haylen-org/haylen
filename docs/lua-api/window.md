# 📖 `haylen.window`

The module `haylen.window` controls the window that the app runs in, or the canvas on the web. It covers the size, fullscreen, the title, the mouse cursor, the on-screen keyboard, the clipboard, the orientation and the status bar of phones, the back button and the fold of foldable devices. On desktops it also covers frameless, transparent, always-on-top and click-through windows, their frame and the monitors. The `window` section of `app.json` sets the window when the app starts, as the [Lua guide](../core/lua.md) lists. Drawing works in design units, so layout code usually reads [`haylen.viewport`](viewport.md) instead of the window size. The [desktop guide](../platform/desktop.md) explains how the desktop options fit together.

```lua
local window = require('haylen.window')
```

A function that takes a boolean needs a real `true` or `false`. Any other value raises a bad argument error, such as `bad argument #1 to 'setFullscreen' (boolean expected, got number)`.

## Size and title

### window.framebufferSize()

Returns the width and the height of the framebuffer in pixels, as two numbers. On screens of high density these are physical pixels, not window points.

```lua
local window = require('haylen.window')

local width, height = window.framebufferSize()
print(string.format('Rendering at %dx%d pixels.', width, height))
```

### window.dpiScale()

Returns how many framebuffer pixels fit in one window point. It is `2.0` on a Retina display and `1.0` on a standard one. On Android a point is a density-independent pixel, so a phone of 420 dots per inch reports `2.625`. On the web a point is a CSS pixel.

```lua
local window = require('haylen.window')

if window.dpiScale() >= 2 then
    print('A screen of high density.')
end
```

### window.fullscreen()

Returns `true` when the window is fullscreen.

```lua
local window = require('haylen.window')

print('Fullscreen:', window.fullscreen())
```

### window.setFullscreen(enabled)

Switches between fullscreen and a window. Nothing happens when the window already is in that mode. Every change, from the app or from the player, publishes the event `windowFullscreenChanged` on the next frame.

On the web the page puts the element that holds the canvas in fullscreen, so the dialogs and the views of plugins stay over the app. Browsers allow fullscreen only shortly after a click, a tap or a key of the person. So the `fullscreen` key of `app.json` leaves a page as it is when it loads.

```lua
local window = require('haylen.window')

local function toggleFullscreen()
    window.setFullscreen(not window.fullscreen())
end

toggleFullscreen()
```

### window.resizable()

Returns `true` when the player can resize the window. It starts as `window.resizable` of `app.json`.

```lua
local window = require('haylen.window')

print('Resizable:', window.resizable())
```

### window.setResizable(enabled)

Lets the player resize the window, or keeps it at its current size. The setting survives a switch to fullscreen and back. Windows that always fill the screen, on Android, iOS, tvOS and the web, ignore it and keep reporting the value that was set.

```lua
local window = require('haylen.window')

-- A pixel art game keeps its window at an exact multiple of its resolution.
window.setResizable(false)
```

### window.setTitle(title)

Sets the title of the window. On the web it sets the title of the document.

```lua
local window = require('haylen.window')

window.setTitle('Tiny Island - Day 3')
```

## Mouse and cursor

### window.setCursor(name)

Sets the shape of the mouse cursor over the window. An unknown name raises a bad argument error, such as `bad argument #1 to 'setCursor' (unknown value 'spinning')`.

| Name | Cursor |
| --- | --- |
| `'default'` | The default cursor of the platform. |
| `'arrow'` | An arrow. |
| `'iBeam'` | The bar of text input. |
| `'crosshair'` | A crosshair. |
| `'pointingHand'` | The hand of links and buttons. |
| `'resizeHorizontal'` | Arrows to the left and the right. |
| `'resizeVertical'` | Arrows up and down. |
| `'resizeDiagonalDown'` | Arrows from the top left to the bottom right. |
| `'resizeDiagonalUp'` | Arrows from the bottom left to the top right. |
| `'resizeAll'` | Arrows in four directions. |
| `'notAllowed'` | An action that is not allowed. |

```lua
local window = require('haylen.window')

local function hoverButton(hovered)
    window.setCursor(hovered and 'pointingHand' or 'default')
end

hoverButton(true)
```

### window.setCursorVisible(visible)

Shows or hides the mouse cursor while it is over the window.

```lua
local window = require('haylen.window')

-- The app draws its own crosshair sprite.
window.setCursorVisible(false)
```

### window.setMouseLocked(locked)

Locks the mouse to the window and hides it, or frees it. While the mouse is locked its position stops changing, and `input.mouseDelta()` of [`haylen.input`](input.md) reads how it moves. Browsers grant the lock only in answer to a click or a key. The error screen frees and shows the mouse. An app that restarts starts with the mouse free, the cursor shown in its default shape and the on-screen keyboard hidden.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local window = require('haylen.window')

local aim = {x = 0, y = 0}
window.setMouseLocked(true)

scene.push({
    update = function(self, dt)
        local dx, dy = input.mouseDelta()
        aim.x = aim.x + dx
        aim.y = aim.y + dy
    end,
})
```

### window.hasPointerDevice()

Returns `false` on a device that the player cannot point at, an Apple TV or an Android TV, where a remote or a gamepad moves the focus instead. It returns `true` everywhere else. On such a device the UI shows its focus ring from the start.

```lua
local window = require('haylen.window')

local hint = window.hasPointerDevice() and 'Click to start' or 'Press select to start'
print(hint)
```

## Keyboard and clipboard

### window.setKeyboardVisible(visible)

Shows or hides the plain on-screen keyboard. Use it for an app that takes typing without a text field of the UI, such as a typing game. The keyboard opens through the same native text input as the text fields of [`haylen.ui`](ui.md), with an empty field of its own. So phones, tablets, TVs and browsers show their keyboard, and input methods work.

- Typed text arrives as `character` events in the `event` callback of the scene, and through `input.text()` of `haylen.input`.
- A correction erases with `backspace` key events and types again.
- Return, tab and escape arrive as the key events `enter`, `tab` and `escape`.
- Text that the input method still composes arrives once it is committed.

Desktop apps type with the physical keyboard either way. Safari on iOS opens the keyboard only from a tap, so there a text field of the UI is the reliable way to type. The [text input guide](../ui/text-input.md) describes each platform.

```lua
local window = require('haylen.window')

local nameField = {text = '', editing = false}

local function beginEditing()
    nameField.editing = true
    window.setKeyboardVisible(true)
end

local function endEditing()
    nameField.editing = false
    window.setKeyboardVisible(false)
end

beginEditing()
endEditing()
```

### window.clipboard()

Returns the text on the clipboard of the system, or an empty string when it holds no text. The engine keeps up to 64 KiB of clipboard text on macOS, Windows, Linux, the Apple TV and the web.

- iOS shows its notice that the app pasted.
- Android gives the clipboard only to the app that has the focus.
- An Apple TV has no clipboard, so the text stays inside the app.
- Browsers give a page only the text that the person pastes into it. So the function returns the text of the last paste of the page.

```lua
local window = require('haylen.window')

local code = window.clipboard()
if #code > 0 then
    print('Pasted the invite code ' .. code .. '.')
end
```

### window.setClipboard(text)

Puts `text` on the clipboard of the system. An Apple TV keeps it inside the app. Browsers let a page write the clipboard only shortly after an input of the person, such as a click or a key.

```lua
local window = require('haylen.window')

local seed = 184467
window.setClipboard('Island seed: ' .. seed)
```

## Orientation and status bar

### window.orientation()

Returns the orientation of the screen, `'landscape'` or `'portrait'`. Phones, tablets and mobile browsers report the way the screen is turned. Desktop windows, Mac Catalyst windows and TVs always count as landscape. The event `windowOrientationChanged` announces every change.

```lua
local window = require('haylen.window')

if window.orientation() == 'portrait' then
    print('The menu stacks its buttons.')
end
```

### window.lockOrientation(orientation)

Keeps the screen in `'landscape'` or `'portrait'`, or lets it turn freely again with `'any'`. The file `app.json` sets the orientations an app starts with, and this function changes them while it runs. An unknown name raises a bad argument error.

- Android locks the activity.
- iPhones and iPads lock the app and turn the screen right away.
- Browsers lock through the Screen Orientation API, which usually works only for a fullscreen page on a phone.
- Desktop windows, Mac Catalyst and TVs ignore it.

```lua
local window = require('haylen.window')

-- A minigame played in portrait, and the menu back in any orientation.
window.lockOrientation('portrait')
window.lockOrientation('any')
```

### window.statusBar()

Returns how the status bar of a phone or a tablet shows over the top of the app. The value `'hidden'` hides it. The values `'auto'`, `'light'` and `'dark'` show it with the content color of the system theme, with light content or with dark content. The key `window.statusBar` of `app.json` sets it when the app starts, and it is `'hidden'` by default. Platforms without a status bar keep reporting the value that was set.

```lua
local window = require('haylen.window')

if window.statusBar() ~= 'hidden' then
    print('The header sits below the status bar.')
end
```

### window.setStatusBar(value)

Hides the status bar with `'hidden'`, or shows it with `'auto'`, `'light'` or `'dark'` content. The value `'auto'` takes dark content in the light theme of the system and light content in its dark theme. Light content suits a dark background, and dark content suits a light one. An unknown name raises a bad argument error.

- iPhones and iPads show it through the view controller of the window. UIKit also hides it in landscape on an iPhone.
- Android shows it over the app, which keeps drawing from edge to edge. A hidden status bar shows for a while after a swipe from the top edge, like the navigation bar.
- Desktops, Mac Catalyst, TVs and browsers ignore it. The status bar of a browser belongs to the browser.

The [safe area](viewport.md#viewportsaferect) leaves the status bar out while it shows. So UI anchored to the safe area moves below it, and the event `windowSafeAreaChanged` reports the change.

```lua
local window = require('haylen.window')

-- The light header of a shop shows the clock in dark content, and a dark game hides it.
window.setStatusBar('dark')
window.setStatusBar('hidden')
```

### window.statusBarColor()

Returns the color that the engine paints behind the status bar, as a `Color`, or `nil` while it paints none and the app shows what it draws there. The key `window.statusBarColor` of `app.json` sets it when the app starts.

```lua
local window = require('haylen.window')

print(window.statusBarColor())
```

### window.setStatusBarColor(color)

Paints `color` behind the status bar while the status bar shows, over everything the app drew there. The value `nil` stops the painting. The status bar of current iPhones, iPads and Android devices has no background of its own, since the app draws under it. So on every platform with a status bar, the engine paints the color over the part of the visible area that the status bar covers. Where the letterbox bars of the [design scaling](viewport.md) cover the top of the screen, the clear color of the bars shows behind the status bar instead.

```lua
local window = require('haylen.window')

window.setStatusBar('light')
window.setStatusBarColor('#FF1E2A44')
window.setStatusBarColor(nil)
```

## Back button

### window.backLeavesApp()

Returns whether the back button of the platform leaves the app. It is `true` when the app starts.

```lua
local window = require('haylen.window')

print(window.backLeavesApp())
```

### window.setBackLeavesApp(enabled)

Decides whether the back button of the platform leaves the app. That button is the Menu button of the Apple TV remote and the Back button of Android. Other platforms have no such button, so the setting changes nothing there.

Apple and Google ask that back leaves the app from its root screen. There the Apple TV goes back to its home screen, and Android closes the app. So an app keeps the default on its root screen, and sets `false` on the screens that back returns from. The press then reaches the app as the escape key, which the UI reads as `uiCancel`. While a popup or a dialog of the UI is open, the app keeps the press either way, and the press closes it. On Android 13 and later the system plays its predictive back animation only when back leaves the app.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Shop = {}
Shop.__index = Shop

function Shop:enter()
    window.setBackLeavesApp(false)
    self.gui = ui.mount(ui.column{onCancel = function() scene.pop() end, ui.button{text = 'Buy', autofocus = true}})
end

function Shop:exit()
    self.gui:unmount()
    window.setBackLeavesApp(true)
end

scene.push(setmetatable({}, Shop))
```

## Folds and dual screens

Foldable phones and tablets fold their screen along a line, and dual-screen devices join two screens with a hinge. The window reports that fold, the posture the device is held in and the segments of the window on each side of the fold. An app uses them to lay its content out around the fold. It may put a map on one side and its controls on the other, or a game on the top half and its buttons on the bottom half of a tabletop. Positions are in design units, like [`viewport.safeRect`](viewport.md#viewportsaferect). The event `windowFoldChanged` announces every change, such as when the device folds, unfolds or turns, or the window moves across the hinge.

| Platform | Where the fold comes from |
| --- | --- |
| Android | Jetpack WindowManager. Its `FoldingFeature` reports the bounds, the orientation and the state of the fold, whether it separates the window and whether a hinge hides part of it, on foldable phones and tablets and on dual-screen devices. |
| Web | The Viewport Segments API, which splits the viewport into segments at the fold, and the Device Posture API, which tells whether the device is folded. Browsers that offer them include Chrome on foldable Android devices. The fold is the gap between the first two segments, so a browser that reports a posture without segments reports no fold. |
| iOS, iPadOS, Mac Catalyst, tvOS, macOS, Windows, Linux | None. No Apple device folds, and the desktops give apps no API for the fold or the hinge of a device. The posture is always `'flat'`, and the window is one segment, unless a [simulation](#windowsetfoldsimulationvalue) replaces the fold. |

### window.fold()

Returns the fold across the window as a table, or `nil` when no fold crosses it.

| Field | Type | Meaning |
| --- | --- | --- |
| `bounds` | `Rect` | The area of the fold in design units. It has no width or no height for a seamless fold, and it covers the hinge of a dual-screen device. |
| `axis` | string | `'vertical'` for a fold from the top to the bottom of the window, and `'horizontal'` for one from side to side. |
| `state` | string | `'flat'` or `'halfOpened'`. |
| `separating` | boolean | Whether the fold splits the window into two segments that content should not cross. A half opened fold and a hinge do. |
| `occluding` | boolean | Whether a hinge hides what lies under the bounds. |

```lua
local window = require('haylen.window')

local fold = window.fold()
if fold and fold.occluding then
    print('Keep the buttons out of', fold.bounds)
end
```

### window.posture()

Returns how the device is held: `'tabletop'` with a horizontal fold half opened, `'book'` with a vertical fold half opened, and `'flat'` with a fold opened flat or without a fold.

```lua
local window = require('haylen.window')

if window.posture() == 'tabletop' then
    print('The game goes on the top half and the buttons on the bottom half.')
end
```

### window.segments()

Returns the areas of the window on each side of a separating fold, as a list of two `Rect` values in design units. The left or top area comes first, and neither holds the bounds of the fold. When the fold does not separate the window, or there is no fold, the list holds the whole window.

```lua
local events = require('haylen.events')
local window = require('haylen.window')

local function layOut()
    local segments = window.segments()
    local map, controls = segments[1], segments[2] or segments[1]
    print('The map in', map, 'and the controls in', controls)
end

layOut()
events.on('windowFoldChanged', layOut)
```

### window.foldSimulation()

Returns the simulated fold as a preset name or a table with `axis`, `state` and `hinge`, or `nil` while the device reports its own fold.

```lua
local window = require('haylen.window')

print(window.foldSimulation())
```

### window.setFoldSimulation(value)

Replaces the fold that the device reports, to test a layout for foldable and dual-screen devices on any screen. The simulated fold lies across the middle of the window. The value `nil` goes back to the fold of the device. The option `debug.fold` of `app.json` sets a simulation when the app starts.

The argument `value` is a preset name, or a table with these keys:

| Key | Default | Meaning |
| --- | --- | --- |
| `axis` | `'vertical'` | `'vertical'` or `'horizontal'`. |
| `state` | `'flat'` | `'flat'` or `'halfOpened'`. |
| `hinge` | `0` | The width of the hinge in window points. A half opened fold, and a hinge wider than 0, separate the window. A hinge hides what lies under it. |

| Preset | Fold |
| --- | --- |
| `'book'` | A vertical seamless fold, half opened. |
| `'tabletop'` | A horizontal seamless fold, half opened. |
| `'flat'` | A vertical seamless fold opened flat, which does not separate the window. |
| `'dualScreen'` | A vertical hinge of 34 points between two screens, which separates the window and hides what lies under it. |

A name that is no preset raises `There is no simulated fold named "<name>". Use "book", "tabletop", "flat" or "dualScreen".`. The [errors](#errors) section lists the messages of a table with a wrong key or value.

```lua
local window = require('haylen.window')

window.setFoldSimulation('tabletop')
window.setFoldSimulation({axis = 'horizontal', hinge = 24})
window.setFoldSimulation(nil)
```

C++ code reads the fold in framebuffer pixels with `core::Engine::getFold()`. It returns a `platform::Fold` of `haylen/platform/Fold.hpp`, whose `getPosture()` and `getSegments(size)` give the posture and the segments. The function `Engine::setFoldSimulation` takes a `platform::FoldSimulation`. The platform reports its own fold through `platform::Window::getFold()`.

## Desktop windows

On Windows, macOS and Linux a window can drop its title bar, float above other windows, stay out of the taskbar, refuse the focus, let clicks pass through and move with the mouse. The [desktop guide](../platform/desktop.md) shows how these fit together and what each system allows.

Phones, tablets, TVs and browsers run apps in a window that fills the screen or the page. There the setters keep and report their values without changing anything. The function `frame()` returns the screen in points, and `monitors()` returns the screen as the only monitor.

Frames and monitors use desktop points, from the top left corner of the primary monitor, as the [desktop guide](../platform/desktop.md#desktop-points) explains.

### window.canBeTransparent()

Returns `true` when the window opened able to be transparent. The key `window.transparent` of `app.json` decides it before the window opens, because it chooses how the system composes the window.

```lua
local window = require('haylen.window')

print('Can be transparent:', window.canBeTransparent())
```

### window.transparent()

Returns `true` while the desktop shows through the transparent pixels of the window. A window that opens transparent clears to `#00000000` unless `clearColor` says otherwise, so the desktop shows wherever the app draws nothing.

```lua
local window = require('haylen.window')

if window.transparent() then
    print('The desktop shows around the hero.')
end
```

### window.setTransparent(enabled)

Makes the window opaque, or transparent again. An opaque window shows alpha 1 everywhere, whatever the clear color and the blend modes of the app. On macOS it also gets the background and the shadow of a normal window. So a window mode that needs a regular window turns transparency off together with `window.setDecorated(true)`.

Only a window that opened transparent can turn transparent. On any other window `true` raises `The window opened opaque, so it cannot turn transparent. Set "window.transparent" in "app.json" to open a window that can.`.

```lua
local window = require('haylen.window')

local function showNormalWindow()
    window.setTransparent(false)
    window.setDecorated(true)
    window.setMousePassthrough(false)
end

local function showStrip()
    window.setDecorated(false)
    window.setTransparent(true)
    window.place({anchor = 'bottom', fill = 'width'})
end

showNormalWindow()
showStrip()
```

### window.decorated()

Returns `true` while the window has a title bar and a border.

```lua
local window = require('haylen.window')

print('Decorated:', window.decorated())
```

### window.setDecorated(enabled)

Gives the window a title bar and a border, or takes them away. The content area keeps its place and its size. A window without decorations moves through `window.startDrag()`, `window.setFrame` or `window.place`. On Windows the player cannot resize it with the mouse.

```lua
local window = require('haylen.window')

window.setDecorated(false)
```

### window.alwaysOnTop()

Returns `true` while the window stays above normal windows.

```lua
local window = require('haylen.window')

print('Always on top:', window.alwaysOnTop())
```

### window.setAlwaysOnTop(enabled)

Keeps the window above normal windows, or lets other windows cover it again. On macOS the window also shows on every space while it is on top.

```lua
local window = require('haylen.window')

window.setAlwaysOnTop(true)
```

### window.showInTaskbar()

Returns `true` while the window has a taskbar button, and on macOS while the app has a Dock icon.

```lua
local window = require('haylen.window')

print('In the taskbar:', window.showInTaskbar())
```

### window.setShowInTaskbar(enabled)

Shows the window in the taskbar and the window switcher, or leaves it out. On macOS it shows or hides the Dock icon and the menu bar of the app. A packaged macOS app that never wants a Dock icon also sets `showInTaskbar` to `false` in `app.json`, which keeps the icon from flashing while the app starts.

```lua
local window = require('haylen.window')

window.setShowInTaskbar(false)
```

### window.focusable()

Returns `true` while a click on the window activates the app and gives it the keyboard.

```lua
local window = require('haylen.window')

print('Focusable:', window.focusable())
```

### window.setFocusable(enabled)

With `false`, the window never activates the app or takes the keyboard, not even when clicked. The player keeps typing in the app they were using, while the game takes clicks. Mouse buttons and moves still reach the app, and key presses go to the other app.

```lua
local window = require('haylen.window')

window.setFocusable(false)
```

### window.frame()

Returns the content area of the window, without its title bar and border, as a `Rect` in desktop points.

```lua
local window = require('haylen.window')

local frame = window.frame()
print(string.format('The window is at %d, %d.', frame.x, frame.y))
```

### window.setFrame(x, y, width, height)

Moves the content area of the window to `x`, `y` and resizes it to `width` by `height`, all in desktop points. The change reaches the app as the events `windowMoved` and `windowResized` when the window lands there. A width that is not positive raises `bad argument #3 to 'setFrame' (positive width expected)`, and a height that is not positive raises `bad argument #4 to 'setFrame' (positive height expected)`.

```lua
local window = require('haylen.window')

local area = window.currentMonitor().workArea
window.setFrame(area.x, area:bottom() - 180, area.width, 180)
```

### window.place(position)

Moves the window to a position that reads like `window.position` of `app.json`. It keeps the size of the window, unless the position fills the area. The position is one of these:

- `'center'`, the middle of the work area of the primary monitor.
- A point, such as `{x = 40, y = 60}`, in desktop points.
- An anchored placement, a table with these fields:

| Field | Default | Meaning |
| --- | --- | --- |
| `anchor` | `'center'` | The side or the corner the window touches: `'center'`, `'top'`, `'bottom'`, `'left'`, `'right'`, `'topLeft'`, `'topRight'`, `'bottomLeft'` or `'bottomRight'`. |
| `area` | `'work'` | `'work'` for the area without the taskbar, the Dock and the menu bar, or `'full'` for the whole monitor. |
| `monitor` | `'primary'` | `'primary'`, or the number of a monitor from `1`, in the order of `window.monitors()`. A number past the last monitor places the window on the primary one. |
| `offset` | `{0, 0}` | Points added to the anchored position. |
| `fill` | `'none'` | `'width'`, `'height'` or `'both'` stretch the window across the area. |

Anchored positions land on whole points. A position that cannot be read raises an error that names the problem, such as `A window position has the unknown "anchor" "middle".`. The [errors](#errors) section lists them.

```lua
local window = require('haylen.window')

-- A strip across the bottom of the work area, just above the taskbar or the Dock.
window.place({anchor = 'bottom', fill = 'width'})
window.place({anchor = 'topRight', monitor = 2, offset = {-16, 16}})
```

### window.monitors()

Returns the monitors of the desktop as a list of tables. The list has at least one monitor, and the primary one is flagged. Every change of the monitors, such as a monitor that connects or a taskbar that moves, publishes the event `windowMonitorsChanged`.

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name that the system gives the monitor. |
| `bounds` | `Rect` | The whole monitor, in desktop points. |
| `workArea` | `Rect` | The monitor without the taskbar, the Dock and the menu bar, in desktop points. |
| `scale` | number | The pixels in a point, such as `2` on a Retina display or a Windows monitor at 200 percent. |
| `primary` | boolean | `true` for the primary monitor. |

```lua
local window = require('haylen.window')

for number, monitor in ipairs(window.monitors()) do
    print(number, monitor.name, monitor.bounds, monitor.scale, monitor.primary)
end
```

### window.currentMonitor()

Returns the monitor that holds most of the window, as a table like those of `window.monitors()`. While the window is off every monitor, it returns the primary monitor.

```lua
local window = require('haylen.window')

local monitor = window.currentMonitor()
print('The window is on', monitor.name, 'at scale', monitor.scale)
```

### window.startDrag()

Moves the window with the mouse for as long as the left button that just went down stays down. So the player drags a window without a title bar by its content. Call it while the button is still down, such as from a `mouseDown` event of a scene or from the `onPress` of a touch button of [`haylen.ui`](ui.md). Do not call it from `onClick`, which runs once the button is up. The system moves the window, and the app hears the release of the button once the drag ends. The function does nothing when the button is already up.

```lua
local scene = require('haylen.scene')
local window = require('haylen.window')

local Strip = {}

function Strip:event(event)
    if event.type == 'mouseDown' and event.button == 'left' and event.y < 40 then
        window.startDrag()
    end
end

scene.push(Strip)
```

### window.mousePassthrough()

Returns where clicks pass through the window: `'off'`, `'whole'` or `'regions'`.

```lua
local window = require('haylen.window')

print('Passthrough:', window.mousePassthrough())
```

### window.setMousePassthrough(value, units)

Lets clicks pass through the window to the desktop and the windows behind it. The argument `value` is one of these:

- `false` gives the whole window the mouse again.
- `true` lets every click through.
- A list of regions lets clicks through everywhere except the regions, which keep the mouse for the app. A region is a `Rect`, a table `{x, y, width, height}` or `{x = 0, y = 0, width = 10, height = 10}`, or a polygon as a list of at least three points.

Regions are in design units by default, converted with the viewport of the moment. So an app whose layout changes, or whose window resizes, gives them again. The argument `units` set to `'pixels'` takes framebuffer pixels instead. The key `window.mousePassthrough` of `app.json` starts the app with `true`.

Apps usually give their regions every frame, around what the player can click. The window keeps the mouse it has while a button is down, so a press that starts over the app also ends there. While clicks pass through, the app still hears the mouse move over the window on Windows and macOS. On Linux it hears the mouse only over the regions.

A region that is neither a rectangle nor a polygon raises `A mouse passthrough region is a rectangle or a polygon, not a <type>.`. A polygon with fewer than three points raises `A mouse passthrough polygon needs at least three points.`. A `value` that is neither a boolean nor a table raises a bad argument error, and other `units` raise `bad argument #2 to 'setMousePassthrough' ('design' or 'pixels' expected)`.

```lua
local scene = require('haylen.scene')
local window = require('haylen.window')

local hero = {x = 400, y = 900, width = 96, height = 96}
local menuButton = {x = 1760, y = 960, width = 140, height = 100}

scene.push({
    update = function(self, dt)
        hero.x = (hero.x + 120 * dt) % 1920
        window.setMousePassthrough({hero, menuButton, {{0, 1060}, {1920, 1060}, {960, 1000}}})
    end,
})
```

## Events

The window publishes these events on [`haylen.events`](events.md#engine-events), which lists the data of each one.

| Event | When |
| --- | --- |
| `windowResized` | The framebuffer changed size. |
| `windowFullscreenChanged` | The window entered or left fullscreen. |
| `windowFocusGained`, `windowFocusLost` | The window gained or lost the keyboard focus. |
| `windowOrientationChanged` | The screen turned between landscape and portrait. |
| `windowSafeAreaChanged` | The safe area moved, such as when the status bar showed. |
| `windowFoldChanged` | The fold across the window changed, or a simulation replaced it. |
| `windowMoved` | The desktop window moved. |
| `windowMonitorsChanged` | A monitor or a work area changed. |

```lua
local events = require('haylen.events')
local window = require('haylen.window')

events.on('windowMonitorsChanged', function()
    window.place({anchor = 'bottom', fill = 'width'})
end)
```

## Errors

These messages are raised by the functions of the module.

| Message | Cause |
| --- | --- |
| `bad argument #1 to '<function>' (boolean expected, got <type>)` | A function that takes a boolean got another value. |
| `bad argument #1 to '<function>' (unknown value '<name>')` | The functions `setCursor`, `lockOrientation` or `setStatusBar` got a name they do not know. |
| `The window opened opaque, so it cannot turn transparent. Set "window.transparent" in "app.json" to open a window that can.` | The function `window.setTransparent(true)` ran on a window that opened opaque. |
| `bad argument #3 to 'setFrame' (positive width expected)` | The width of `window.setFrame` is not positive. |
| `bad argument #4 to 'setFrame' (positive height expected)` | The height of `window.setFrame` is not positive. |
| `A window position is "center", a point with "x" and "y", or an object with an "anchor".` | The position of `window.place` has another form. |
| `A window position has the unknown "<key>" <value>.` | The `anchor`, `area` or `fill` of a position has a value it does not take. |
| `A window position needs a number for "<key>".` | The `x`, `y` or an item of `offset` is not a number. |
| `A window position names its monitor with "primary" or a number from 1.` | The `monitor` of a position has another value. |
| `A window position offset is a pair of numbers.` | The `offset` of a position is not a list of two numbers. |
| `Unknown key "<key>" in window position.` | A position has a key that it does not take. |
| `A mouse passthrough region is a rectangle or a polygon, not a <type>.` | A region of `window.setMousePassthrough` is another value. |
| `A mouse passthrough polygon needs at least three points.` | A polygon region has fewer than three points. |
| `bad argument #2 to 'setMousePassthrough' ('design' or 'pixels' expected)` | The `units` of `window.setMousePassthrough` are another value. |
| `There is no simulated fold named "<name>". Use "book", "tabletop", "flat" or "dualScreen".` | The preset of `window.setFoldSimulation` does not exist. |
| `A simulated fold is a preset name, such as "book", or an object with "axis", "state" and "hinge".` | The value of `window.setFoldSimulation` is another type. |
| `The axis of a simulated fold is "vertical" or "horizontal", not "<axis>".` | The `axis` of a simulated fold has another value. |
| `The state of a simulated fold is "flat" or "halfOpened", not "<state>".` | The `state` of a simulated fold has another value. |
| `The hinge of a simulated fold is a width of at least 0 window points.` | The `hinge` of a simulated fold is negative or not a number. |
| `Unknown key "<key>" in a simulated fold.` | A simulated fold has a key that it does not take. |
