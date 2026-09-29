# haylen.window

`haylen.window` controls the native window, or the canvas on the web, that the app runs in: its size, fullscreen state, title, mouse cursor, on-screen keyboard, screen orientation and clipboard. The initial window settings come from the `window` section of `app.json`. Use this module for options menus, text entry and mouse capture. Drawing sizes are in design units, so layout code normally uses `haylen.viewport` instead of the window size.

```lua
local window = require('haylen.window')
```

Functions that take a boolean require a real `true` or `false`. Other values raise an error such as `bad argument #1 to 'setFullscreen' (boolean expected, got number)`.

## Functions

### window.size()

Returns the framebuffer width and height in pixels, as two numbers. On high-density displays these are physical pixels, not window points.

```lua
local window = require('haylen.window')

local width, height = window.size()
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

Switches between fullscreen and windowed mode. Nothing happens when the window is already in the requested mode. Every change, from the app or from the player, publishes the `window_fullscreen_changed` event of [haylen.events](events.md#engine-events) on the next frame.

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
| `'ibeam'` | Text insertion bar. |
| `'crosshair'` | Crosshair. |
| `'pointing_hand'` | Hand used for links and buttons. |
| `'resize_horizontal'` | Left and right arrows. |
| `'resize_vertical'` | Up and down arrows. |
| `'resize_diagonal_down'` | Arrows from the top left to the bottom right. |
| `'resize_diagonal_up'` | Arrows from the bottom left to the top right. |
| `'resize_all'` | Arrows in four directions. |
| `'not_allowed'` | Blocked action. |

```lua
local window = require('haylen.window')

local function hoverButton(hovered)
    window.setCursor(hovered and 'pointing_hand' or 'default')
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

### window.showKeyboard(visible)

Shows or hides the plain on-screen keyboard, for apps that take typing without a text field of the UI, such as a typing game. It opens through the same native text input as the text fields of [haylen.ui](ui.md), with an empty field of its own, so phones, tablets, TVs and browsers show their keyboard and input methods work. Typed text arrives as `character` events in the scene `event` callback and through `input.text()` from `haylen.input`, a correction erases with `backspace` key events and types again, return, tab and escape arrive as `enter`, `tab` and `escape` key events, and text the input method still composes arrives once it is committed. Desktop apps type with the physical keyboard either way. Safari on iOS only opens the keyboard from a tap, so there a text field of the UI is the reliable way to type. The [text input guide](../text-input.md) describes each platform.

```lua
local window = require('haylen.window')

local nameField = {text = '', editing = false}

local function beginEditing()
    nameField.editing = true
    window.showKeyboard(true)
end

local function endEditing()
    nameField.editing = false
    window.showKeyboard(false)
end

beginEditing()
endEditing()
```

### window.orientation()

Returns the orientation of the screen, `'landscape'` or `'portrait'`. Phones, tablets and mobile browsers report the way the screen is turned, while desktop windows, Mac Catalyst windows and TVs always count as landscape. The `window_orientation_changed` event of [haylen.events](events.md) announces every change.

```lua
local window = require('haylen.window')

if window.orientation() == 'portrait' then
    print('the menu stacks its buttons')
end
```

### window.lockOrientation(orientation)

Keeps the screen in `'landscape'`, `'portrait'` or `'any'` orientation, which lets it turn freely again. `app.json` sets the orientations an app starts with, and this changes them while it runs. Android locks the activity, iPhones and iPads lock the app and turn the screen right away, and browsers lock through the Screen Orientation API, which usually works only for a fullscreen page on a phone. Desktop windows, Mac Catalyst and TVs ignore it. An unknown name raises an error.

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

Decides whether the back button of the platform, the Menu button of the Apple TV remote and the Back button of Android, leaves the app. Apple and Google ask that it leaves from the root screen, where the Apple TV goes back to its home screen and Android closes the app, so an app keeps the default there and sets `false` on the screens back returns from. The press then reaches the app as the escape key, which the UI reads as `ui_cancel`. While a UI popup or dialog is open the app keeps the press either way, and it closes them. On Android 13 and later the system plays its predictive back animation only when back leaves the app. Other platforms have no such button, so it changes nothing there.

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
