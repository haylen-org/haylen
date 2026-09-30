# haylen.window

The module `haylen.window` controls the native window, or the canvas on the web, that the app runs in: its size, fullscreen state, title, mouse cursor, on-screen keyboard, screen orientation and clipboard, and on desktops its decorations, level, taskbar presence, focus, frame, monitors, dragging and the clicks that pass through it. The initial window settings come from the `window` section of `app.json`. Use this module for options menus, text entry, mouse capture and apps that live on the desktop. Drawing sizes are in design units, so layout code normally uses `haylen.viewport` instead of the window size.

```lua
local window = require('haylen.window')
```

Functions that take a boolean require a real `true` or `false`. Other values raise an error such as `bad argument #1 to 'setFullscreen' (boolean expected, got number)`.

## Functions

### window.framebufferSize()

Returns the framebuffer width and height in pixels, as two numbers. On high-density displays these are physical pixels, not window points.

```lua
local window = require('haylen.window')

local width, height = window.framebufferSize()
print(string.format('rendering at %dx%d pixels', width, height))
```

### window.dpiScale()

Returns the ratio between framebuffer pixels and window points, for example `2.0` on a Retina display and `1.0` on a standard one.

```lua
local window = require('haylen.window')

if window.dpiScale() >= 2 then
    print('high density display')
end
```

### window.fullscreen()

Returns `true` when the window is fullscreen.

```lua
local window = require('haylen.window')

print('fullscreen:', window.fullscreen())
```

### window.setFullscreen(enabled)

Switches between fullscreen and windowed mode. Nothing happens when the window is already in the requested mode. Every change, from the app or from the player, publishes the `windowFullscreenChanged` event of [`haylen.events`](events.md#engine-events) on the next frame.

```lua
local window = require('haylen.window')

local function toggleFullscreen()
    window.setFullscreen(not window.fullscreen())
end

toggleFullscreen()
```

### window.resizable()

Returns `true` when the player can resize the window. It starts as `window.resizable` in `app.json`.

```lua
local window = require('haylen.window')

print('resizable:', window.resizable())
```

### window.setResizable(enabled)

Lets the player resize the window, or keeps it at its current size. The setting survives switching fullscreen on and off. Windows that always fill the screen, on Android, iOS, tvOS and the web, ignore it and keep reporting the value that was set.

```lua
local window = require('haylen.window')

-- A pixel art game keeps its window at an exact multiple of its resolution.
window.setResizable(false)
```

### window.setTitle(title)

Sets the window title. On the web it sets the document title.

```lua
local window = require('haylen.window')

window.setTitle('Tiny Island - Day 3')
```

### window.setCursor(name)

Sets the mouse cursor shape shown over the window. An unknown name raises `bad argument #1 to 'setCursor' (unknown value 'spinning')`.

| Name | Cursor |
| --- | --- |
| `'default'` | The platform default cursor. |
| `'arrow'` | Arrow pointer. |
| `'iBeam'` | Text insertion bar. |
| `'crosshair'` | Crosshair. |
| `'pointingHand'` | Hand used for links and buttons. |
| `'resizeHorizontal'` | Left and right arrows. |
| `'resizeVertical'` | Up and down arrows. |
| `'resizeDiagonalDown'` | Arrows from the top left to the bottom right. |
| `'resizeDiagonalUp'` | Arrows from the bottom left to the top right. |
| `'resizeAll'` | Arrows in four directions. |
| `'notAllowed'` | Blocked action. |

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

Locks the mouse to the window and hides it, or releases it. While the mouse is locked its position stops changing, and relative movement is read with `input.mouseDelta()` from `haylen.input`. Browsers only grant the lock in response to a click or key press.

```lua
local window = require('haylen.window')
local input = require('haylen.input')

local aim = {x = 0, y = 0}
window.setMouseLocked(true)

require('haylen.scene').push({
    update = function(self, dt)
        local dx, dy = input.mouseDelta()
        aim.x = aim.x + dx
        aim.y = aim.y + dy
    end,
})
```

### window.setKeyboardVisible(visible)

Shows or hides the plain on-screen keyboard, for apps that take typing without a text field of the UI, such as a typing game. It opens through the same native text input as the text fields of [`haylen.ui`](ui.md), with an empty field of its own, so phones, tablets, TVs and browsers show their keyboard and input methods work. Typed text arrives as `character` events in the scene `event` callback and through `input.text()` from `haylen.input`, a correction erases with `backspace` key events and types again, return, tab and escape arrive as `enter`, `tab` and `escape` key events, and text the input method still composes arrives once it is committed. Desktop apps type with the physical keyboard either way. Safari on iOS only opens the keyboard from a tap, so there a text field of the UI is the reliable way to type. The [text input guide](../text-input.md) describes each platform.

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

### window.orientation()

Returns the orientation of the screen, `'landscape'` or `'portrait'`. Phones, tablets and mobile browsers report the way the screen is turned, while desktop windows, Mac Catalyst windows and TVs always count as landscape. The `windowOrientationChanged` event of [`haylen.events`](events.md) announces every change.

```lua
local window = require('haylen.window')

if window.orientation() == 'portrait' then
    print('the menu stacks its buttons')
end
```

### window.lockOrientation(orientation)

Keeps the screen in `'landscape'`, `'portrait'` or `'any'` orientation, which lets it turn freely again. The file `app.json` sets the orientations an app starts with, and this changes them while it runs. Android locks the activity, iPhones and iPads lock the app and turn the screen right away, and browsers lock through the Screen Orientation API, which usually works only for a fullscreen page on a phone. Desktop windows, Mac Catalyst and TVs ignore it. An unknown name raises an error.

```lua
local window = require('haylen.window')

-- A minigame played in portrait, and the menu back in any orientation.
window.lockOrientation('portrait')
window.lockOrientation('any')
```

### window.clipboard()

Returns the text on the system clipboard, or an empty string when it holds no text. The engine reserves 64 KiB for clipboard text.

```lua
local window = require('haylen.window')

local code = window.clipboard()
if #code > 0 then
    print('pasted invite code:', code)
end
```

### window.setClipboard(text)

Puts `text` on the system clipboard.

```lua
local window = require('haylen.window')

local seed = 184467
window.setClipboard('Island seed: ' .. seed)
```

### window.hasPointerDevice()

Returns `false` on a device the player cannot point at, an Apple TV or an Android TV, where a remote or a gamepad moves the focus instead, and `true` everywhere else. The UI shows its focus ring from the start on such a device.

```lua
local window = require('haylen.window')

local hint = window.hasPointerDevice() and 'Click to start' or 'Press select to start'
print(hint)
```

### window.backLeavesApp()

Returns whether the back button of the platform leaves the app, which is `true` at start.

```lua
local window = require('haylen.window')

print(window.backLeavesApp())
```

### window.setBackLeavesApp(enabled)

Decides whether the back button of the platform, the Menu button of the Apple TV remote and the Back button of Android, leaves the app. Apple and Google ask that it leaves from the root screen, where the Apple TV goes back to its home screen and Android closes the app, so an app keeps the default there and sets `false` on the screens back returns from. The press then reaches the app as the escape key, which the UI reads as `uiCancel`. While a UI popup or dialog is open the app keeps the press either way, and it closes them. On Android 13 and later the system plays its predictive back animation only when back leaves the app. Other platforms have no such button, so it changes nothing there.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Shop = {}
Shop.__index = Shop

function Shop:enter()
    window.setBackLeavesApp(false)
    self.document = ui.mount(ui.column{onCancel = function() scene.pop() end, ui.button{text = 'Buy', autofocus = true}})
end

function Shop:exit()
    self.document:unmount()
    window.setBackLeavesApp(true)
end

scene.push(setmetatable({}, Shop))
```

## Desktop windows

Windows, macOS and Linux run apps in windows that can drop their title bar, float above other windows, stay out of the taskbar, refuse the focus, let clicks through and move with the mouse, which suits games that live on the desktop, such as a strip above the taskbar. The [desktop guide](../desktop.md) shows how they fit together and what each system allows. Phones, tablets, TVs and browsers run apps in a window that fills the screen or the page: there the setters keep and report their values without changing anything, `frame()` returns the screen in points and `monitors()` returns the screen as the only monitor.

Frames and monitors are in desktop points, with the origin at the top left corner of the primary monitor and y growing down. A desktop point is a point of macOS, and on Windows and Linux a pixel divided by the scale of the primary monitor, so a window of 1280 by 720 points in `app.json` has a frame of 1280 by 720 points.

### window.canBeTransparent()

Returns `true` when the window opened able to be transparent, which `window.transparent` in `app.json` decides before the window opens, because it chooses how the system composes the window.

```lua
local window = require('haylen.window')

print('can be transparent:', window.canBeTransparent())
```

### window.transparent()

Returns `true` while the desktop shows through the transparent pixels of the window. A window that opens transparent clears to `#00000000` unless `clearColor` says otherwise, so everything the app does not draw lets the desktop through.

```lua
local window = require('haylen.window')

if window.transparent() then
    print('the desktop shows around the hero')
end
```

### window.setTransparent(enabled)

Makes the window opaque, or transparent again. An opaque window shows alpha 1 everywhere, whatever the clear color and the blend modes of the app, and on macOS it gets the background and the shadow of a normal window, so a window mode that needs a regular window turns transparency off together with `window.setDecorated(true)`. Only a window that opened transparent can turn transparent, and on any other window `true` raises `The window opened opaque, so it cannot turn transparent. Set "window.transparent" in "app.json" to open a window that can.`

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

print('decorated:', window.decorated())
```

### window.setDecorated(enabled)

Gives the window a title bar and a border, or takes them away. The content area keeps its place and its size. A window without decorations moves through `window.startDrag()`, `window.setFrame` or `window.place`, and on Windows the player cannot resize it with the mouse.

```lua
local window = require('haylen.window')

window.setDecorated(false)
```

### window.alwaysOnTop()

Returns `true` while the window stays above normal windows.

```lua
local window = require('haylen.window')

print('always on top:', window.alwaysOnTop())
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

print('in the taskbar:', window.showInTaskbar())
```

### window.setShowInTaskbar(enabled)

Shows the window in the taskbar and the window switcher, or leaves it out. On macOS it shows or hides the Dock icon and the menu bar of the app. A packaged macOS app that never wants a Dock icon also sets `showInTaskbar` to `false` in `app.json`, which keeps the icon from flashing while the app starts.

```lua
local window = require('haylen.window')

window.setShowInTaskbar(false)
```

### window.focusable()

Returns `true` while clicking the window activates the app and gives it the keyboard.

```lua
local window = require('haylen.window')

print('focusable:', window.focusable())
```

### window.setFocusable(enabled)

With `false`, the window never activates the app or takes the keyboard, not even when clicked, so the player keeps typing in the app they were using while the game takes clicks. Mouse buttons and moves still reach the app, and key presses go to the other app.

```lua
local window = require('haylen.window')

window.setFocusable(false)
```

### window.frame()

Returns the content area of the window, without its title bar and border, as a `Rect` in desktop points.

```lua
local window = require('haylen.window')

local frame = window.frame()
print(string.format('the window is at %d, %d', frame.x, frame.y))
```

### window.setFrame(x, y, width, height)

Moves the content area of the window to `x`, `y` and resizes it to `width` by `height`, all in desktop points. The size must be positive. The change reaches the app as `windowMoved` and `windowResized` events of [`haylen.events`](events.md#engine-events) when the window lands there.

```lua
local window = require('haylen.window')

local area = window.currentMonitor().workArea
window.setFrame(area.x, area:bottom() - 180, area.width, 180)
```

### window.place(position)

Moves the window to a position that reads like `window.position` in `app.json`, keeping its size unless the position fills the area: `'center'`, a point `{x = 40, y = 60}`, or a table with these fields.

| Field | Default | Meaning |
| --- | --- | --- |
| `anchor` | `'center'` | The side or the corner the window touches: `'center'`, `'top'`, `'bottom'`, `'left'`, `'right'`, `'topLeft'`, `'topRight'`, `'bottomLeft'` or `'bottomRight'`. |
| `area` | `'work'` | Either `'work'` for the area without the taskbar, the Dock and the menu bar, or `'full'` for the whole monitor. |
| `monitor` | `'primary'` | Either `'primary'`, or the number of a monitor in the order of `window.monitors()`. A number past the last monitor places the window on the primary one. |
| `offset` | `{0, 0}` | Points added to the anchored position. |
| `fill` | `'none'` | The values `'width'`, `'height'` or `'both'` stretch the window across the area. |

Anchored positions land on whole points. A position that cannot be read raises an error such as `A window position has an unknown anchor: "middle".`.

```lua
local window = require('haylen.window')

-- A strip across the bottom of the work area, just above the taskbar or the Dock.
window.place({anchor = 'bottom', fill = 'width'})
```

### window.monitors()

Returns the monitors of the desktop as a list of tables, with the primary monitor flagged. The list has at least one monitor.

| Field | Meaning |
| --- | --- |
| `name` | The name the system gives the monitor. |
| `bounds` | The whole monitor as a `Rect` in desktop points. |
| `workArea` | The monitor without the taskbar, the Dock and the menu bar, as a `Rect` in desktop points. |
| `scale` | Pixels in a point, such as `2` on a Retina display or a Windows monitor at 200 percent. |
| `primary` | The value is `true` for the primary monitor. |

Every change of the monitors, such as a monitor that connects or a taskbar that moves, publishes `windowMonitorsChanged` on [`haylen.events`](events.md#engine-events).

```lua
local window = require('haylen.window')

for number, monitor in ipairs(window.monitors()) do
    print(number, monitor.name, monitor.bounds, monitor.scale, monitor.primary)
end
```

### window.currentMonitor()

Returns the monitor that holds most of the window, as a table like the ones of `window.monitors()`, or the primary monitor while the window is off every monitor.

```lua
local window = require('haylen.window')

local monitor = window.currentMonitor()
print('the window is on', monitor.name, 'at scale', monitor.scale)
```

### window.startDrag()

Moves the window with the mouse for as long as the left button that just went down stays down, so the player drags a window without a title bar by its content. Call it while the button is still down, such as from a `mouseDown` event of a scene or the `onPress` of a touch button of [`haylen.ui`](ui.md), and not from `onClick`, which runs once the button is up. The system moves the window, and the app hears the release of the button once the drag ends. It does nothing when the button is already up.

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

print('passthrough:', window.mousePassthrough())
```

### window.setMousePassthrough(value, units)

Lets clicks pass through the window to the desktop and the windows behind it. The value `false` gives the whole window the mouse again, `true` lets every click through, and a list of regions lets clicks through everywhere except the regions, which keep the mouse for the app. A region is a `Rect`, a table `{x, y, width, height}` or `{x = 0, y = 0, width = 10, height = 10}`, or a polygon as a list of at least three points. Regions are in design units by default, converted with the viewport of the moment, so an app whose layout changes, or whose window resizes, gives them again. The argument `units` set to `'pixels'` takes framebuffer pixels instead. The option `window.mousePassthrough` in `app.json` starts the app with `true`.

Apps usually give their regions every frame, around what the player can click. The window keeps the mouse it has while a button is down, so a press that starts over the app also ends there. While clicks pass through, the app still hears the mouse move over the window on Windows and macOS, while on Linux it hears the mouse only over the regions.

```lua
local scene = require('haylen.scene')
local window = require('haylen.window')

local hero = {x = 400, y = 900, width = 96, height = 96}
local menuButton = {x = 1760, y = 960, width = 140, height = 100}

scene.push({
    update = function(self, dt)
        hero.x = (hero.x + 120 * dt) % 1920
        window.setMousePassthrough({hero, menuButton})
    end,
})
```
