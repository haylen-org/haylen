# 📖 `haylen.input`

Keyboard, mouse, touch, gesture and gamepad state, and the action map that turns all of them into named gameplay actions. Gameplay reads actions such as `jump` or `move` so that every device and on-screen control works without extra code, and menus or tools read the raw devices when they need them.

```lua
local input = require('haylen.input')
```

## Frame timing

The engine updates device state and the action map once at the start of every frame from the events that arrived before it. States such as `keyPressed`, `mouseReleased`, `gamepadPressed` and the action `pressed` stay `true` for that whole frame, in every scene callback, and are `false` again on the next frame. Read frame edges in `update`, because `fixedUpdate` can run zero or several times in one frame, and read actions in `fixedUpdate` with the step functions [`input.stepPressed()`](#inputsteppressedaction), [`input.stepReleased()`](#inputstepreleasedaction) and [`input.stepDown()`](#inputstepdownaction), which give each press to exactly one fixed step. Positions are in design coordinates, the coordinate space set by `design` in `app.json`.

## Keyboard

Keys are named with the strings listed in [Key names](#key-names). An unknown name raises a bad argument error with `unknown value '<name>'`.

### input.keyDown(key)

Returns `true` while the key is held.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyDown('leftShift') then
            print('running')
        end
    end,
})
```

### input.keyPressed(key)

Returns `true` in the frame the key went down. Key repeat from holding a key does not count as a new press.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyPressed('escape') then
            scene.pop()
        end
    end,
})
```

### input.keyReleased(key)

Returns `true` in the frame the key went up. Keys held when the window loses focus or the app is suspended are released in that frame.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyReleased('space') then
            print('charge released')
        end
    end,
})
```

### input.anyKeyPressed()

Returns `true` when any key went down in this frame, which suits title screens.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.anyKeyPressed() or input.mousePressed() then
            print('start the game')
        end
    end,
})
```

### input.modifiers()

Returns a table with the boolean fields `shift`, `control`, `alt` and `super`, taken from the latest key event. They all read `false` again from the frame the window loses focus or the app is suspended.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        local modifiers = input.modifiers()
        if modifiers.control and input.keyPressed('s') then
            print('save')
        end
    end,
})
```

### input.text()

Returns the text typed in this frame as a UTF-8 string, which is empty when nothing was typed. It follows the keyboard layout and input methods of the platform, so use it for text fields instead of key names.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local name = ''

scene.push({
    update = function(self, dt)
        name = name .. input.text()
        if input.keyPressed('backspace') then
            name = name:sub(1, -2)
        end
    end,
})
```

### input.keyCaptured(key)

Returns `true` while the current press of a key belongs to the interface of [`haylen.ui`](ui.md), such as the Escape that closes a combo list or a key typed into a text field, as the [input guide](../input/input.md#ui-and-gameplay-input) lists. The press stays with the interface until the key is released, and meanwhile `key:` bindings of the [action map](#action-map) read it as up. The raw keyboard functions keep reporting the key.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyPressed('escape') and not input.keyCaptured('escape') then
            print('escape reached the game')
        end
    end,
})
```

## Mouse

Mouse buttons are named `'left'`, `'right'` and `'middle'`, and the button argument defaults to `'left'`.

### input.mouseDown(button)

Returns `true` while the mouse button is held.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.mouseDown() then
            print('painting')
        end
    end,
})
```

### input.mousePressed(button)

Returns `true` in the frame the mouse button went down.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.mousePressed('right') then
            print('open the context menu')
        end
    end,
})
```

### input.mouseReleased(button)

Returns `true` in the frame the mouse button went up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.mouseReleased('middle') then
            print('stop panning')
        end
    end,
})
```

### input.mousePosition()

Returns the mouse position in design coordinates as two numbers. Convert it to world coordinates with `camera:screenToWorld()` from [`haylen.graphics2d`](graphics2d.md), which takes design coordinates and accounts for the camera viewport.

```lua
local input = require('haylen.input')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        local x, y = input.mousePosition()
        local worldX, worldY = self.camera:screenToWorld(x, y)
        print(x, y, worldX, worldY)
    end,
})
```

### input.mouseFramebufferPosition()

Returns the mouse position in framebuffer pixels as two numbers, before any conversion to design coordinates. It suits code that draws or reads pixels of the window itself.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        local x, y = input.mouseFramebufferPosition()
        print('pixel', x, y)
    end,
})
```

### input.mouseDelta()

Returns how far the mouse moved in this frame, in design units, as two numbers. It keeps reporting movement while the mouse is locked with `window.setMouseLocked()`. A pointer that comes into the window, or a finger that lands while it drives the mouse, arrives without moving, so the mouse never jumps from where it left.

```lua
local input = require('haylen.input')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        if input.mouseDown('middle') then
            local dx, dy = input.mouseDelta()
            self.camera.x = self.camera.x - dx
            self.camera.y = self.camera.y - dy
        end
    end,
})
```

### input.mouseScroll()

Returns the scroll of this frame as two numbers, horizontal and vertical, in steps of a wheel, with positive values for scrolling to the left and up. A step of a mouse wheel counts as 1, and a trackpad counts every 40 points of the screen it moves as a step. Chromium and Safari report most mouse wheels to a page in points, about 100 for a step, so there a step counts as about 2.5. The scroll speed of [`input.setScrollSpeed()`](#inputsetscrollspeedspeed) multiplies both.

```lua
local input = require('haylen.input')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        local _, scroll = input.mouseScroll()
        self.camera:zoomAt(1 + scroll * 0.1, input.mousePosition())
    end,
})
```

### input.mouseInside()

Returns `true` while the mouse is inside the window. A finger that drives the mouse with `input.setTouchAsMouse()` takes it out of the window when it lifts, as a touch screen has no hover.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.showCursor = input.mouseInside()
    end,
})
```

### input.pointerCaptured()

Returns `true` while the interface of [`haylen.ui`](ui.md) owns the pointer, which is what `ui.usingPointer()` reported at the end of the previous frame. A mouse button pressed meanwhile stays with the interface until it is released, so `mouse:` bindings of the [action map](#action-map) read it as released and a click on a menu button never swings a sword bound to `mouse:left`, and a button pressed in the game stays with the game while the pointer passes over the interface. The interface learns what the pointer is over while it draws, so a press with no hover before it, such as the tap of a finger that drives the mouse, goes to the game unless the interface owned the pointer in the frame before. The raw mouse functions above keep reporting the buttons, and virtual buttons of touch controls keep driving their actions.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.mousePressed() and not input.pointerCaptured() then
            print('clicked the world')
        end
    end,
})
```

## Touch

### input.touches()

Returns a list of the fingers on the screen, including fingers that lifted in this frame. Each touch is a table with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | Identifier that stays the same while the finger is down. |
| `x`, `y` | number | Current position in design coordinates. |
| `startX`, `startY` | number | Where the finger landed. |
| `dx`, `dy` | number | Movement since the previous frame, adding up every move the frame received. |
| `phase` | string | One of `'began'`, `'moved'`, `'stationary'`, `'ended'` or `'cancelled'`. |
| `duration` | number | Seconds since the finger landed. |

A finger that lands and moves in the same frame keeps the `'began'` phase for that frame. Fingers in the `'ended'` or `'cancelled'` phase leave the list on the next frame. A platform may give a new finger the identifier of a finger that lifted in the same frame, and then the list holds both for that frame, the lifted finger first.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        for _, touch in ipairs(input.touches()) do
            if touch.phase == 'began' then
                print('finger', touch.id, 'at', touch.x, touch.y)
            elseif touch.phase == 'ended' then
                print('finger', touch.id, 'lifted after', touch.duration)
            end
        end
    end,
})
```

### input.findTouch(id)

Returns the finger with the identifier `id` as a table with the fields of `input.touches()`, or `nil` when no such finger is on the screen. It suits code that follows one finger from the frame it landed. When a new finger takes the identifier of a finger that lifted in the same frame, it returns the lifted finger for that frame.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if self.finger == nil then
            local first = input.touches()[1]
            self.finger = first and first.id
        end
        local touch = self.finger and input.findTouch(self.finger)
        if touch == nil then
            self.finger = nil
        else
            self.dragX, self.dragY = touch.x - touch.startX, touch.y - touch.startY
        end
    end,
})
```

## Gestures

The engine recognizes taps, double taps, long presses, swipes and pinches from touches every frame. While no finger is down, the left mouse button acts as a finger too, so gestures also work on desktop.

### input.gestures()

Returns a list of the gestures recognized in this frame. Each gesture is a table with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | string | One of `'tap'`, `'doubleTap'`, `'longPress'`, `'swipe'` or `'pinch'`. |
| `x`, `y` | number | Where the gesture happened in design coordinates. A pinch reports the center between the two fingers. |
| `dx`, `dy` | number | The movement of a swipe, from where the finger landed to where it lifted, and the movement of the center of a pinch since exactly two fingers started touching the screen, which pans a map or a camera with two fingers. Other gestures report 0. |
| `scale` | number | The spread of a pinch divided by the spread when exactly two fingers started touching the screen, which a third finger starts again when it comes or goes. Other gestures report 1. |
| `rotation` | number | How far a pinch turned since exactly two fingers started touching the screen, in radians from -π to π, positive in the direction that turns the x axis toward the y axis. Other gestures report 0. |

A tap fires when a finger lifts quickly without moving far. The second tap of a double tap reports both a `'tap'` and a `'doubleTap'` in the same frame. A long press fires once while the finger is still down, and that finger ends without a tap. A swipe fires when a finger lifts after moving far enough quickly enough. A pinch fires every frame in which one of exactly two fingers moves, and fingers that were part of a pinch never end as taps or swipes. A finger the system cancels, and every finger or mouse press still down when the window loses focus or the app is suspended, ends without a gesture.

```lua
local input = require('haylen.input')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
        self.baseZoom = 1
        self.baseRotation = 0
    end,
    update = function(self, dt)
        for _, gesture in ipairs(input.gestures()) do
            if gesture.type == 'doubleTap' then
                self.camera.zoom = {1, 1}
            elseif gesture.type == 'swipe' and math.abs(gesture.dx) > math.abs(gesture.dy) then
                print(gesture.dx > 0 and 'next page' or 'previous page')
            elseif gesture.type == 'longPress' then
                print('menu at', gesture.x, gesture.y)
            elseif gesture.type == 'pinch' then
                self.camera.zoom = {self.baseZoom * gesture.scale, self.baseZoom * gesture.scale}
                self.camera.rotation = self.baseRotation + gesture.rotation
            end
        end
        if #input.touches() < 2 then
            self.baseZoom = self.camera.zoom.x
            self.baseRotation = self.camera.rotation
        end
    end,
})
```

### input.setGestureSettings(settings)

Changes the thresholds of gesture recognition. Only the fields present change, and unknown keys raise `Unknown option "<key>".`. Distances are in design units and times in seconds.

| Field | Type | Default | Meaning |
| --- | --- | --- | --- |
| `tapMaxDuration` | number | `0.3` | Longest press that still counts as a tap. |
| `tapMaxMovement` | number | `24` | Farthest a finger can move and still tap or long press. |
| `doubleTapInterval` | number | `0.35` | Longest time between the two taps of a double tap. |
| `doubleTapDistance` | number | `48` | Farthest apart the two taps of a double tap can be. |
| `longPressDuration` | number | `0.5` | Time a finger must stay down for a long press. |
| `swipeMinDistance` | number | `90` | Shortest movement that counts as a swipe. |
| `swipeMaxDuration` | number | `0.5` | Longest time a swipe can take. |
| `mouse` | boolean | `true` | Lets the left mouse button act as a finger while no finger is down. |

```lua
local input = require('haylen.input')

input.setGestureSettings({longPressDuration = 0.8, swipeMinDistance = 60, mouse = false})
```

### input.gestureSettings()

Returns every threshold of gesture recognition as a table with the fields `input.setGestureSettings()` accepts, so a settings screen can show and change them.

```lua
local input = require('haylen.input')

local settings = input.gestureSettings()
input.setGestureSettings({longPressDuration = settings.longPressDuration * 1.5})
```

## Gamepads

Up to eight gamepads are tracked. Gamepad indices start at 1, default to 1 and raise a bad argument error with `gamepad index out of range` outside 1 to 8. Buttons and axes use the names listed in [Gamepad names](#gamepad-names).

### input.gamepadConnected(index)

Returns `true` when a gamepad is connected at the index. The `gamepadConnected` and `gamepadDisconnected` events of [`haylen.events`](events.md#engine-events) announce the changes with the index and the name.

```lua
local input = require('haylen.input')

for index = 1, 4 do
    if input.gamepadConnected(index) then
        print('player', index, 'is ready')
    end
end
```

### input.gamepadName(index)

Returns the name the platform reports for the gamepad, or an empty string when none is connected.

```lua
local input = require('haylen.input')

print(input.gamepadName(1))
```

### input.gamepadLabels(index)

Returns how the gamepad labels its face buttons, so prompts show what the player sees on them: `'letters'` with A at the bottom and B on the right, `'swappedLetters'` with B at the bottom and A on the right, `'shapes'`, or `'unknown'`, also for a gamepad that is not connected. Bindings keep naming the buttons by their place, so `button:south` is the bottom button whatever its label. Apple platforms read the symbol of the bottom button, Windows reads only gamepads with letters, and Android, Linux and browsers tell the labels from the vendor of the gamepad, which leaves gamepads of other makers `'unknown'`.

```lua
local input = require('haylen.input')

local prompts = {letters = 'A', swappedLetters = 'B', shapes = 'Cross', unknown = 'South'}
print('Press ' .. prompts[input.gamepadLabels(1)] .. ' to jump.')
```

### input.rumble(index, options)

Runs the motors of a gamepad with `{low, high, seconds}`: the low and the high frequency motors from 0 to 1, 0 by default, for the seconds, 0.2 by default. Both motors at 0 stop the gamepad, a new rumble replaces the one that runs, and the engine stops every gamepad when the app loses the focus or stops. Intensities outside 0 to 1 raise `The motors of a rumble run from 0 to 1.`, seconds that are not positive raise `A rumble lasts a positive number of seconds.`, and an unknown key raises `Unknown option "<key>".`.

| Platform | Rumble |
| --- | --- |
| iOS, iPadOS, tvOS, Mac Catalyst and macOS | The haptics engine of the controller plays one continuous event, whose intensity follows the stronger motor and whose sharpness the share of the high frequency motor. |
| Android | The vibrators of the controller, one motor on each when it has two and the stronger motor on one, with the permission `VIBRATE` of the template. |
| Windows | Both motors of XInput controllers. |
| Linux | The force feedback of the event device of the controller, where the user may write it, which the rules of most distributions allow for the user at the computer. |
| Web | The dual rumble of the Gamepad API, in browsers that offer it. |

```lua
local input = require('haylen.input')

input.rumble(1, {low = 0.8, high = 0.3, seconds = 0.25})
```

### input.gamepadDown(button, index)

Returns `true` while the button is held.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.gamepadDown('rightShoulder') then
            print('aiming')
        end
    end,
})
```

### input.gamepadPressed(button, index)

Returns `true` in the frame the button went down.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.gamepadPressed('start', 2) then
            print('player 2 paused the game')
        end
    end,
})
```

### input.gamepadReleased(button, index)

Returns `true` in the frame the button went up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.gamepadReleased('west') then
            print('throw')
        end
    end,
})
```

### input.gamepadButtonCaptured(button, index)

Returns `true` while the current press of a gamepad button belongs to the interface, such as the east button that closes a popup, and `button:` bindings read it as up meanwhile. The argument `index` picks the gamepad from 1 to 8 and defaults to 1.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.gamepadPressed('east') and not input.gamepadButtonCaptured('east') then
            print('east reached the game')
        end
    end,
})
```

### input.gamepadAxisCaptured(axis, index)

Returns `true` while the current lean of a gamepad axis past the press threshold belongs to the interface, such as the left stick that moves the focus between the controls of a menu, and `axis:` and `stick:` bindings of that axis read it as 0 meanwhile. The argument `axis` is one of `'leftX'`, `'leftY'`, `'rightX'`, `'rightY'`, `'leftTrigger'` or `'rightTrigger'`, and `index` picks the gamepad from 1 to 8 and defaults to 1.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if math.abs(input.gamepadAxis('leftX')) > 0.5 and not input.gamepadAxisCaptured('leftX') then
            print('the stick steers the game')
        end
    end,
})
```

### input.gamepadAxis(axis, index)

Returns the value of one axis with the dead zone removed and the rest rescaled to the full range. Sticks go from -1 to 1 with positive y pointing down, and triggers go from 0 to 1.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        local gas = input.gamepadAxis('rightTrigger')
        local brake = input.gamepadAxis('leftTrigger')
        print(gas - brake)
    end,
})
```

### input.gamepadStick(side, index)

Returns the position of the `'left'` or `'right'` stick as two numbers. The dead zone is applied to the stick radius, which keeps diagonals smooth, and the length never exceeds 1. Any other side raises a bad argument error with `expected left or right`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local x, y = 0, 0

scene.push({
    update = function(self, dt)
        local sx, sy = input.gamepadStick('left')
        x = x + sx * 200 * dt
        y = y + sy * 200 * dt
    end,
})
```

### input.setGamepadDeadzone(deadzone)

Sets the dead zone of every gamepad axis and stick, from 0 to below 1, and the default is 0.2. Any other value raises `The gamepad dead zone must be at least 0 and below 1.` and keeps the current dead zone. Movement inside the dead zone reads as 0 and does not make the gamepad the last used device.

```lua
local input = require('haylen.input')

input.setGamepadDeadzone(0.12)
```

### input.gamepadDeadzone()

Returns the dead zone of every gamepad axis and stick, as `input.setGamepadDeadzone()` set it.

```lua
local input = require('haylen.input')

input.setGamepadDeadzone(input.gamepadDeadzone() + 0.05)
```

### input.setScrollSpeed(speed)

Multiplies every scroll of the wheels and the trackpads by `speed` before the UI, `input.mouseScroll()` and the `mouseScroll` events see it, 1 by default, which suits a setting of the player for how fast the wheel scrolls. The `app.json` key `input.scrollSpeed` sets it at start. A value that is not positive raises `The scroll speed must be a positive number.` and keeps the current speed.

```lua
local input = require('haylen.input')

input.setScrollSpeed(0.5)
```

### input.scrollSpeed()

Returns the speed of every scroll, as `input.setScrollSpeed()` set it.

```lua
local input = require('haylen.input')

print(input.scrollSpeed()) -- 1
```

## Devices

### input.lastDevice()

Returns the kind of device the player used last: `'keyboardMouse'`, `'touch'` or `'gamepad'`. Key presses and mouse button presses select `'keyboardMouse'`, touches select `'touch'`, and a gamepad button going down or an axis leaving the dead zone selects `'gamepad'`. Only the press counts, so key repeats and a gamepad button or stick that stays held never take over from a device the player used afterwards, and neither do the keys of the plain on-screen keyboard that `window.setKeyboardVisible()` opens, which the player types with the device in hand. Use it to show the matching button prompts or on-screen controls.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local prompts = {keyboardMouse = 'Press Space', touch = 'Tap the screen', gamepad = 'Press A'}

scene.push({
    update = function(self, dt)
        self.prompt = prompts[input.lastDevice()]
    end,
})
```

### input.setMouseAsTouch(enabled)

Makes the mouse act as a finger, so an app made for touch screens runs with a mouse. The left button becomes a touch with an id that no real finger has, which begins, moves while the button is held and ends, and `input.lastDevice()` reports `'touch'`, so controls with `touchOnly` show. Moves without a button disappear, as touch screens have no hover, while the right and middle buttons and the wheel stay mouse input. The UI, gestures, touch controls and the app all see the finger. A press keeps the device it started as until it ends, so turning the emulation on or off while the button is held never leaves a finger or a button down. The `app.json` key `input.mouseAsTouch` sets it at start, and a desktop build that tests touch controls is the usual use.

```lua
local input = require('haylen.input')

input.setMouseAsTouch(true)
print(input.mouseAsTouch())
```

### input.mouseAsTouch()

Returns `true` while the mouse acts as a finger.

```lua
local input = require('haylen.input')

if input.mouseAsTouch() then
    print('The mouse is a finger.')
end
```

### input.setTouchAsMouse(enabled)

Makes a finger act as the mouse as well, so an app that reads the mouse runs on a touch screen. The first finger down drives the left button and the mouse position until it lifts, the mouse leaves the window with it, and the next finger down takes over from then. A finger that drives the mouse keeps driving it until it lifts, also when the emulation turns off meanwhile. The touches still arrive as touches, so touch controls and gestures keep working, and `input.lastDevice()` stays `'touch'`. The `app.json` key `input.touchAsMouse` sets it at start.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.setTouchAsMouse(true)
scene.push({
    update = function(self, dt)
        if input.mousePressed('left') then
            local x, y = input.mousePosition()
            print('Pressed at ' .. x .. ', ' .. y)
        end
    end,
})
```

### input.touchAsMouse()

Returns `true` while a finger acts as the mouse.

```lua
local input = require('haylen.input')

print(input.touchAsMouse())
```

## Text fields of the app

A GUI that the app draws itself, such as one written in Lua over `haylen.graphics2d`, edits text through the native text input of the platform as the text fields of `haylen.ui` do, with the on-screen keyboard, autocorrection and the input methods that compose Japanese, Chinese or Korean text. The app keeps the text and draws it, the platform reports every edit as a `textEdited` event with the field id `4294967296`, and its actions as `textAction`. One field edits at a time, and a text field of `haylen.ui` that takes the focus takes the keyboard over.

### input.editText(options)

Starts editing the text field of the app, or follows it when it changes or moves. Call it again every frame or whenever the text, the selection or the place change. The native field takes the text and the selection only when they differ from what the user typed last, so an app that shows the typed text back keeps a composition going.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | string | required | The text of the field. |
| `selectionStart`, `selectionEnd` | integer | the other end, or the end of the text | The selection in code points, the caret when both are equal. A field with one end given has the caret there. |
| `x`, `y`, `width`, `height` | number | `0` | The place of the field in design coordinates, where the platform shows its input method windows and which it keeps above the on-screen keyboard. |
| `keyboard` | string | `'text'` | `'text'`, `'multiline'`, `'number'`, `'decimal'`, `'phone'`, `'email'`, `'url'`, `'search'` or `'password'`. |
| `returnKey` | string | `'default'` | `'default'`, `'done'`, `'go'`, `'next'`, `'search'` or `'send'`. |
| `autocapitalize` | string | `'sentences'` | `'none'`, `'sentences'`, `'words'` or `'characters'`. |
| `autocorrect` | boolean | `true` | Lets the system correct words. |
| `maxLength` | integer | `0` | The most code points the field takes, or `0` for no limit. |

Errors: `A text field needs its "text" as a string.`, `The selection of a text field must run from 0 to the length of its text, with "selectionStart" not past "selectionEnd".`, `The option "<option>" of a text field must be <names>.` and `Unknown option "<key>".`.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

local field = {text = '', caret = 0}

scene.push({
    enter = function(self)
        input.editText({text = field.text, x = 200, y = 300, width = 600, height = 64, returnKey = 'done'})
    end,
    event = function(self, event)
        if event.type == 'textEdited' and event.field == 4294967296 then
            field.text, field.caret, field.composing = event.text, event.selectionStart, event.compositionStart
            input.editText({text = field.text, selectionStart = field.caret, x = 200, y = 300, width = 600, height = 64, returnKey = 'done'})
        elseif event.type == 'textAction' and event.field == 4294967296 then
            input.finishText()
        end
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRectOutline({x = 200, y = 300, width = 600, height = 64}, 2, '#FFFFFFFF')
    end,
})
```

### input.finishText()

Ends editing the text field of the app and hides the on-screen keyboard.

```lua
local input = require('haylen.input')

input.finishText()
```

### input.editingText()

Returns `true` while the text field of the app edits.

```lua
local input = require('haylen.input')

print(input.editingText())
```

## Action map

The action map gives names to gameplay actions and binds each one to any number of keys, mouse buttons, gamepad controls and virtual controls. Actions update once per frame in the order they were defined.

| Type | Bindings | Reports |
| --- | --- | --- |
| `'button'` | `bindings` | The field `value` is the strongest binding from 0 to 1. The action is down while the value is at least the press threshold, 0.5 by default. |
| `'axis'` | `positive` and `negative` | The field `value` is the strongest positive binding minus the strongest negative binding, from -1 to 1. The action is down while the absolute value is at least the press threshold. |
| `'vector'` | `up`, `down`, `left`, `right` and stick bindings in `bindings` | The field `vector` is right minus left and down minus up, plus every stick binding, limited to a length of 1. The field `value` is the length, and the action is down while it is at least the press threshold. |

An action takes only the binding lists its type reads, so a `positive` list on a button raises `The button action "<name>" does not read "positive".`, and the `bindings` of a vector action hold only `stick:` and `virtualStick:` bindings, so a key there raises `The vector action "<name>" takes only sticks in "bindings", not "key:w".`.

Bindings are strings with a source and a name.

| Binding | Meaning |
| --- | --- |
| `key:<key>` | A key from [Key names](#key-names), such as `key:space`. |
| `mouse:<button>` | A mouse button, such as `mouse:left`. |
| `button:<button>` | A gamepad button from [Gamepad names](#gamepad-names), such as `button:south`. |
| `axis:<axis>+` or `axis:<axis>-` | One direction of a gamepad axis, such as `axis:leftY-` for pushing the left stick up or `axis:rightTrigger+`. |
| `stick:left` or `stick:right` | A whole gamepad stick, for vector actions. |
| `virtual:<name>` | A virtual button set with `input.setVirtualButton()` or an on-screen `touchButton`. |
| `virtualStick:<name>` | A virtual stick set with `input.setVirtualStick()` or an on-screen `touchStick`, for vector actions. |

Gamepad bindings read every connected gamepad and use the strongest one, unless the [devices](#mapdevices) of the map pick one gamepad or none. Mouse bindings read as released for a press the interface took, as [`input.pointerCaptured()`](#inputpointercaptured) explains, and key and gamepad button bindings read as up for a press the interface answers itself, as [`input.keyCaptured()`](#inputkeycapturedkey) explains. While a scene change holds input back, as the `blockInput` option of [`haylen.scene`](scene.md) sets, every action reads as up and 0, and an action whose bindings are still held when the change ends stays up until they are released. Action names that were never defined read as not down and 0.

An action map document is a table or a JSON file with an `actions` list. Each action has `name`, `type` and the binding lists its type uses, and an optional `buffer` in seconds, 0 by default, which keeps a press available to [`input.consumePress()`](#inputconsumepressaction) for that long after the first fixed step that saw it. A buffer that is not a number from 0 raises `The buffer of an action must be a number of seconds from 0.`.

```json
{
  "actions": [
    {"name": "jump", "type": "button", "bindings": ["key:space", "key:w", "button:south", "virtual:jump"], "buffer": 0.12},
    {"name": "throttle", "type": "axis", "positive": ["key:up", "axis:rightTrigger+"], "negative": ["key:down", "axis:leftTrigger+"]},
    {"name": "move", "type": "vector", "up": ["key:w"], "down": ["key:s"], "left": ["key:a", "key:left"], "right": ["key:d", "key:right"], "bindings": ["stick:left", "virtualStick:move"]}
  ]
}
```

### input.loadActions(document)

Replaces the action map with a document, given as a table or as a path to a JSON file in the content folder. Nothing changes when the document is invalid. A document that is not a table with string keys raises `Expected the action map to be a JSON object.`, and an action that is not one raises `Expected an action to be a JSON object.`. Unknown keys raise `Unknown key "<key>" in the action map.` or `Unknown key "<key>" in an action.`, a missing list raises `The action map needs a list of actions.`, an action without a name or type raises `An action needs a name and a type.`, bad types raise `The type of an action must be "button", "axis" or "vector", not "<type>".`, a binding list that is not a list raises `The "<list>" of an action must be a list of bindings.`, bad bindings raise `The input binding "<binding>" is invalid. Use a binding such as "key:space", "mouse:left", "button:south", "axis:leftX+" or "stick:left".` and a name used by two actions raises `The action name "<name>" is used by more than one action.` An empty Lua table counts as an empty list.

```lua
local input = require('haylen.input')

input.loadActions('input.json')

input.loadActions({actions = {
    {name = 'fire', type = 'button', bindings = {'mouse:left', 'axis:rightTrigger+'}},
    {name = 'aim', type = 'vector', bindings = {'stick:right'}},
}})
```

### input.saveActions()

Returns the current action map as a document table, in the same format `input.loadActions()` accepts. Use it with [`haylen.preferences`](preferences.md) to keep rebound controls, or call `preferences.capture()`, which stores it under `input.actions`.

```lua
local input = require('haylen.input')
local preferences = require('haylen.preferences')

preferences.set('controls', input.saveActions())
preferences.save()
input.loadActions(preferences.get('controls'))
```

### input.actionNames()

Returns a list of the action names in definition order.

```lua
local input = require('haylen.input')

for _, name in ipairs(input.actionNames()) do
    print(name, input.down(name))
end
```

### input.defineAction(action)

Adds one action given as a table in the format of an action in the document, or replaces the action with the same name in its place, keeping the definition order. It raises the errors of `input.loadActions()`, and nothing changes when the action is invalid.

```lua
local input = require('haylen.input')

input.defineAction({name = 'dash', type = 'button', bindings = {'key:leftShift', 'button:east'}})
```

### input.actionDefinition(name)

Returns the action with that name as a table in the document format, or `nil` when there is none. Binding lists that are empty are left out. A remapping screen reads an action, swaps one binding and defines it again.

```lua
local input = require('haylen.input')

local jump = input.actionDefinition('jump')
if jump then
    jump.bindings[1] = 'key:w'
    input.defineAction(jump)
end
```

### input.removeAction(name)

Removes the action with that name. A name that was never defined changes nothing.

```lua
local input = require('haylen.input')

input.removeAction('dash')
```

### input.clearActions()

Removes every action.

```lua
local input = require('haylen.input')

input.clearActions()
input.loadActions('input.json')
```

### input.setPressThreshold(threshold)

Sets the value at which every action counts as down, above 0 and at most 1, 0.5 by default. A lower threshold makes light trigger pulls and small stick movements press actions. Any other value raises `The press threshold must be above 0 and at most 1.` and keeps the current threshold.

```lua
local input = require('haylen.input')

input.setPressThreshold(0.3)
```

### input.down(action)

Returns `true` while the action is down.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.down('fire') then
            print('firing')
        end
    end,
})
```

### input.pressed(action)

Returns `true` in the frame the action went down.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.pressed('jump') then
            print('jump')
        end
    end,
})
```

### input.released(action)

Returns `true` in the frame the action went up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.released('jump') then
            print('cut the jump short')
        end
    end,
})
```

### input.value(action)

Returns the value of the action, as described in the [Action map](#action-map) table.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local speed = 0

scene.push({
    update = function(self, dt)
        speed = speed + input.value('throttle') * 300 * dt
    end,
})
```

### input.vector(action)

Returns the direction of a vector action as two numbers, with a length of at most 1. Other action types return `0, 0`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local x, y = 0, 0

scene.push({
    update = function(self, dt)
        local dx, dy = input.vector('move')
        x = x + dx * 180 * dt
        y = y + dy * 180 * dt
    end,
})
```

### input.stepPressed(action)

Returns `true` in the fixed step that first comes after the action went down. Every press reaches exactly one step, also when its frame runs no step or several steps, so a simulation in `fixedUpdate` never misses a press or sees it twice. A press and a release that arrive before the same step count as a press in that step and a release in the next one.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    attacks = 0,
    fixedUpdate = function(self, step)
        if input.stepPressed('attack') then
            self.attacks = self.attacks + 1
        end
    end,
})
```

### input.stepReleased(action)

Returns `true` in the fixed step that first comes after the action went up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    fixedUpdate = function(self, step)
        if input.stepReleased('jump') and self.rising then
            self.velocity = self.velocity * 0.5
        end
    end,
})
```

### input.stepDown(action)

Returns `true` while the action is down in the current fixed step, and also in the step after a press that was released before any step saw it, so a tap shorter than a step still holds the action for one step.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    charge = 0,
    fixedUpdate = function(self, step)
        self.charge = input.stepDown('fire') and self.charge + step or 0
    end,
})
```

### input.consumePress(action)

Returns `true` once for each press of the action and takes the press, so the next call returns `false` until the action goes down again. A press stays available from its frame until the end of the first fixed step after it, plus the `buffer` of the action beyond that step, measured in fixed steps, which is the usual jump buffer. It works in `update` and in `fixedUpdate`, and input that a scene change holds back drops the press.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.defineAction({name = 'jump', type = 'button', bindings = {'key:space', 'button:south'}, buffer = 0.12})

scene.push({
    fixedUpdate = function(self, step)
        if self.grounded and input.consumePress('jump') then
            self.velocity = -520
        end
    end,
})
```

### input.findGamepadPress(button)

Returns the lowest index of a gamepad that pressed the button in this frame, or that pressed any button when `button` is `nil`, or `nil` when none did. A join screen gives the player who pressed the gamepad a map that reads it.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local players = {}

scene.push({
    update = function(self, dt)
        local pad = input.findGamepadPress('start')
        if pad and not players[pad] then
            players[pad] = input.newActionMap({keyboard = false, mouse = false, virtual = false, gamepad = pad, actions = 'input.json'})
        end
    end,
})
```

## Action maps of players

Each player of a local multiplayer game reads an action map of their own, which loads the same documents and offers the same functions as the default map, as methods. The default map is a map too, which `input.actions()` returns, and the functions of the module read it. Every map updates at the start of every frame and every fixed step, takes the presses the interface captures like the default map, and leaves the engine once Lua lets go of it.

### input.newActionMap(options)

Creates an action map that reads the devices of its options, every device by default.

| Option | Default | Meaning |
| --- | --- | --- |
| `keyboard` | `true` | Whether `key:` bindings read the keyboard. |
| `mouse` | `true` | Whether `mouse:` bindings read the mouse. |
| `gamepad` | every gamepad | The index of the one gamepad that `button:`, `axis:` and `stick:` bindings read, from 1 to 8, or `false` for none. |
| `virtual` | `true` | Whether `virtual:` and `virtualStick:` bindings read the virtual controls. |
| `virtualPrefix` | `''` | Text that goes in front of the name of every virtual binding, so `virtual:jump` reads the virtual button `p2.jump` with the prefix `p2.`. |
| `actions` | none | A document table or the path of a JSON file to load, as `map:load()` takes. |

```lua
local input = require('haylen.input')

local first = input.newActionMap({gamepad = false, actions = 'input.json'})
local second = input.newActionMap({keyboard = false, mouse = false, gamepad = 1, actions = 'input.json'})
```

### input.actions()

Returns the default action map, which the functions of the module read and change. Two calls return values that compare equal.

```lua
local input = require('haylen.input')

input.actions().devices = {gamepad = 1}
```

### map:load(document), map:save(), map:names(), map:define(action), map:definition(name), map:remove(name), map:clear()

Load, save, list, define, read and remove the actions of the map, with the formats and errors of `input.loadActions()`, `input.saveActions()`, `input.actionNames()`, `input.defineAction()`, `input.actionDefinition()`, `input.removeAction()` and `input.clearActions()`.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 2})
player:load('input.json')
player:define({name = 'taunt', type = 'button', bindings = {'button:north'}})
print(#player:names(), player:definition('taunt').bindings[1])
```

### map:down(action), map:pressed(action), map:released(action), map:value(action), map:vector(action)

Read the actions of the map in the frame, as `input.down()`, `input.pressed()`, `input.released()`, `input.value()` and `input.vector()` read the default map.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 2, actions = 'input.json'})

scene.push({
    x = 0,
    update = function(self, dt)
        local dx = player:vector('move')
        self.x = self.x + dx * 200 * dt
    end,
})
```

### map:stepDown(action), map:stepPressed(action), map:stepReleased(action), map:consumePress(action)

Read the actions of the map in fixed steps and take their presses, as `input.stepDown()`, `input.stepPressed()`, `input.stepReleased()` and `input.consumePress()` read the default map.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 1, actions = 'input.json'})

scene.push({
    fixedUpdate = function(self, step)
        if player:stepPressed('attack') then
            print('Attack.')
        end
    end,
})
```

### map:stepButtons(list)

Returns one integer whose bit `n - 1` is set when the action `list[n]` is down in this fixed step, as `stepDown` tells, for command buffers, recordings and bots that keep one number per player and step. The list holds at most 32 action names, and a name the map does not define reads as up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local buttons = {'up', 'down', 'left', 'right', 'attack', 'jump', 'defend'}
local player = input.newActionMap({gamepad = 1, actions = 'input.json'})

scene.push({
    fixedUpdate = function(self, step)
        local mask = player:stepButtons(buttons)
        if mask & (1 << 4) ~= 0 then
            print('Attack is down.')
        end
    end,
})
```

### map.devices

The devices the map reads, as a table with the keys of the options of `input.newActionMap()` without `actions`, where `gamepad` is left out while the map reads every gamepad. Assigning a table replaces every device setting, and keys left out take their defaults.

```lua
local input = require('haylen.input')

local player = input.newActionMap()
player.devices = {keyboard = false, mouse = false, gamepad = 3}
print(player.devices.gamepad)
```

### map.pressThreshold

The value at which every action of the map counts as down, above 0 and at most 1, 0.5 by default, with the error of `input.setPressThreshold()`.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 1})
player.pressThreshold = 0.3
```

## Command buffers

A command buffer turns the buttons held on each fixed step into one kept command, the way action games read moves: a single press, a chord of buttons on one step, a double tap or a sequence of presses such as defend, forward and attack, typed within a number of steps. When several commands complete on one step, the highest priority wins, and the later one on a tie, a press on a later step replaces the kept command, and the kept command expires after its lifetime in steps unless the buffer is frozen, such as during hit stop. Time counts in steps, never in seconds, so the same presses always type the same commands, which replays and tests rely on.

### input.newCommandBuffer(options)

Creates a buffer.

| Option | Default | Meaning |
| --- | --- | --- |
| `buttons` | required | The button names, from 1 to 32, which are also the order of presses within one step. Every button types a single-press command with its own name. |
| `lifetime` | `1` | How many steps a typed command lasts, or `false` to keep it until it is consumed or replaced. A lifetime of 1 lasts only the step it was typed on, even while frozen. |
| `lifetimes` | none | A table from button names to the lifetime of their single presses, such as `{left = 1, right = 1}`. |
| `directions` | none | `{back = name, forward = name}`, the buttons of a character that faces right, which lets steps name `'forward'` and `'back'`. |
| `commands` | `{}` | The named commands, each `{name, steps, maxTicks, priority, lifetime, clearsHistory}`, where `steps` lists button names, `'forward'` or `'back'`, `maxTicks` bounds the steps between the first and the last press, `priority` is 0 by default, `lifetime` defaults to the lifetime of the buffer and `clearsHistory`, `true` by default, forgets the typed presses once the command completes. |

A chord is a command with `maxTicks = 0`, whose buttons go down on the same step, and a double tap is two presses of one button with nothing pressed between them.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local commands = input.newCommandBuffer({
    buttons = {'defend', 'up', 'down', 'left', 'right', 'jump', 'attack'},
    lifetime = 10,
    lifetimes = {up = 1, down = 1, left = 1, right = 1},
    directions = {back = 'left', forward = 'right'},
    commands = {
        {name = 'run', steps = {'forward', 'forward'}, maxTicks = 9, priority = 1},
        {name = 'jumpAttack', steps = {'jump', 'attack'}, maxTicks = 0, priority = 2, clearsHistory = false},
        {name = 'forwardSpecial', steps = {'defend', 'forward', 'attack'}, priority = 3, lifetime = false},
    },
})
local player = input.newActionMap({gamepad = 1, actions = 'input.json'})

scene.push({
    facing = 1,
    fixedUpdate = function(self, step)
        commands.facing = self.facing
        commands:update(player)
        local typed = commands:peek()
        if typed == 'forwardSpecial' or typed == 'attack' then
            commands:consume(typed)
            print('Typed ' .. typed .. '.')
        end
    end,
})
```

### buffer:update(mapOrMask)

Advances the buffer by one step with the buttons down on it: an action map, whose actions named like the buttons give their step state, as `map:stepDown()` tells, so a tap shorter than a step still counts, or an integer whose bit `n - 1` holds the button `n`, such as `map:stepButtons()` returns or a recording plays back.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'jump', 'attack'}, lifetime = 6})
commands:update(0x2)
print(commands:peek())
```

### buffer:settle(mapOrMask)

Takes the buttons held now as already down without a step, so a button held across a pause or the start of a round never types a press.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'jump', 'attack'}})
commands:settle(0x2)
commands:update(0x2)
print(commands:peek())
```

### buffer:peek()

Returns the name of the kept command, or `nil`, without taking it. The name is a string the buffer already holds, so a step allocates nothing.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'attack'}, lifetime = 4})
commands:update(1)
if commands:peek() == 'attack' then
    print('An attack waits.')
end
```

### buffer:consume(name)

Takes the kept command when it is `name` and returns `true`, or returns `false`. A name the buffer does not have raises `The command buffer has no command "<name>".`.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'attack'}, lifetime = 4})
commands:update(1)
print(commands:consume('attack'), commands:consume('attack'))
```

### buffer:held(button)

Returns `true` while the button is down on the last step. A name the buffer does not have raises `The command buffer has no button "<name>".`.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'defend'}})
commands:update(1)
print(commands:held('defend'))
```

### buffer:clear()

Forgets the kept command and the typed presses and keeps the buttons that are held, such as when a character comes back to life.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'attack'}, lifetime = 10})
commands:update(1)
commands:clear()
print(commands:peek())
```

### buffer.frozen, buffer.facing, buffer.tick

The property `frozen` stops lifetimes longer than one step from counting, `facing` is 1 for a character that faces right and -1 for one that faces left, which swaps the `'forward'` and `'back'` steps, and `tick` counts the steps the buffer took, read only.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'left', 'right'}, directions = {back = 'left', forward = 'right'}})
commands.facing = -1
commands.frozen = true
commands:update(0)
print(commands.tick)
```

## Recordings

A recording keeps the buttons of every player on every fixed step of a run, as masks whose bit `n - 1` holds the button `n`, with the identifier and the fixed rate of the app and data of the app, such as the states of its random generators. A playback hands the steps back in place of the devices, which makes replays, ghosts, attract modes, automated playtests on the headless player and the reproduction of bug reports. A replay reproduces a run when the app reads input only through the masks, seeds every generator it uses from the recording and steps its simulation in `fixedUpdate`. An optional hash of the state of the app on each step tells the step where a replay first diverges. Steps that repeat the same masks cost one entry, so a minute of play takes a few hundred bytes.

### input.newRecorder(options)

Creates a recording with `{buttons, players, data}`: the names of up to 32 buttons, from 1 to 16 players, 1 by default, and a string of the app, empty by default, such as the state of a random generator. Errors include `An input recording needs from 1 to 32 buttons.`, `An input recording needs from 1 to 16 players.` and `Unknown option "<key>".`.

```lua
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')

local buttons = {'left', 'right', 'jump'}
local random = m.random(42)
local recorder = input.newRecorder({buttons = buttons, players = 1, data = random:state()})

scene.push({
    fixedUpdate = function(self, step)
        recorder:write(1, input.actions())
        recorder:advance()
    end,
})
```

### recording:write(player, maskOrMap)

Sets the buttons of a player for the step that `advance` closes: an integer mask, or an action map whose actions named like the buttons give their step state. A player outside the recording raises `The input recording has players from 1 to <n>.`.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'left', 'right'}, players = 2})
recorder:write(1, 0x1)
recorder:write(2, input.actions())
recorder:advance()
```

### recording:advance(hash)

Closes the step with the masks written for it, 0 for a player without one, and with an optional integer hash of 32 bits of the state of the app, such as `world:stateHash()` of a physics world, which a playback compares to find where a replay diverges.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:write(1, 1)
recorder:advance(12345)
print(recorder.steps)
```

### recording:bytes()

Returns the recording as a string of bytes, which a file under [`storage.root()`](storage.md), a save or a request to a server keeps.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:advance()
local bytes = recorder:bytes()
print(#bytes)
```

### input.loadRecording(bytes)

Reads the bytes of `recording:bytes()`. Bytes that are not a whole recording raise `The input recording is malformed: <reason>.`, a recording of another app raises `The recording belongs to another app.`, and one made at another fixed rate raises `The recording was made at <rate> fixed steps a second, and the app runs at <rate>.`, since its steps would mean other times.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:advance()
local copy = input.loadRecording(recorder:bytes())
print(copy.steps, copy.players, copy.buttons[1], copy.data)
```

### recording.steps, recording.players, recording.buttons, recording.data

The number of steps, the number of players, the list of button names and the data of the recording, read only.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'left', 'right'}, players = 2, data = 'level 3'})
print(recorder.steps, recorder.players, #recorder.buttons, recorder.data)
```

### input.newPlayback(recording)

Creates a playback of a recording that stands before its first step.

```lua
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')

local function replay(bytes)
    local playback = input.newPlayback(input.loadRecording(bytes))
    local random = m.random(0)
    random:setState(playback.data)
    scene.push({
        fixedUpdate = function(self, step)
            if playback.finished then
                return
            end
            local mask = playback:mask(1)
            self.jumping = mask & 0x4 ~= 0
            playback:advance()
        end,
    })
end
```

### playback:mask(player), playback:hash(), playback:advance(), playback:seek(step)

The method `mask` returns the buttons of a player on the current step, 0 once the playback finished, `hash` returns the hash of the step or `nil`, `advance` moves to the next step and `seek` moves to a step from 0 to the number of steps, where a step outside raises `The playback has steps from 0 to <n>.`.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
for step = 1, 3 do
    recorder:write(1, step % 2)
    recorder:advance(step)
end
local playback = input.newPlayback(recorder)
playback:seek(2)
print(playback:mask(1), playback:hash())
```

### playback.step, playback.steps, playback.finished, playback.data

The steps played so far, the number of steps, whether every step was played and the data of the recording, read only.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}, data = 'seed 9'})
recorder:advance()
local playback = input.newPlayback(recorder)
playback:advance()
print(playback.step, playback.steps, playback.finished, playback.data)
```

## Virtual controls

Virtual buttons and sticks are named inputs that code or on-screen controls write and the action map reads through `virtual:` and `virtualStick:` bindings. The `touchButton` and `touchStick` components of [`haylen.ui`](ui.md) write them from their `action` property, and an app can write them itself for custom controls. Values written during a frame reach the actions at the start of the next frame.

### input.setVirtualButton(name, down)

Presses or releases a virtual button.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'jump', type = 'button', bindings = {'key:space', 'virtual:jump'}}}})

scene.push({
    update = function(self, dt)
        local down = false
        for _, touch in ipairs(input.touches()) do
            if touch.x > 1000 and touch.phase ~= 'ended' and touch.phase ~= 'cancelled' then
                down = true
            end
        end
        input.setVirtualButton('jump', down)
    end,
})
```

### input.setVirtualStick(name, x, y)

Sets the position of a virtual stick. Vectors longer than 1 are shortened to a length of 1.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'move', type = 'vector', bindings = {'stick:left', 'virtualStick:move'}}}})

scene.push({
    update = function(self, dt)
        local x, y = 0, 0
        for _, touch in ipairs(input.touches()) do
            if touch.startX < 400 then
                x = (touch.x - touch.startX) / 80
                y = (touch.y - touch.startY) / 80
            end
        end
        input.setVirtualStick('move', x, y)
    end,
})
```

### input.clearVirtual()

Releases every virtual button and centers every virtual stick.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    exit = function(self)
        input.clearVirtual()
    end,
})
```

## Events

A scene with an `event(self, e)` callback receives every platform event as a table as soon as it arrives, before the frame that uses it. The `type` field names the event, and the other fields depend on the type. Positions and movements are in design coordinates. Key, character, mouse and touch events also carry `modifiers`, a table with the boolean fields `shift`, `control`, `alt` and `super` for the modifier keys held when the event happened.

| Type | Fields |
| --- | --- |
| `'keyDown'`, `'keyUp'` | The field `key` is the key name, or `'unknown'` for keys without one. The field `repeat` is `true` for repeats generated by holding a key. |
| `'character'` | The field `character` is the typed character as a UTF-8 string. |
| `'mouseDown'`, `'mouseUp'` | The field `button` is the mouse button name. The fields `x` and `y` are the mouse position. |
| `'mouseMove'` | The fields `x` and `y` are the mouse position. The fields `dx` and `dy` are the movement of this event, which keeps coming while the mouse is locked. |
| `'mouseScroll'` | The fields `scrollX` and `scrollY` are the scroll, with positive values for scrolling to the left and up, in points of the screen when `precise` is `true`, as a trackpad reports it, and in steps of a wheel otherwise. |
| `'touchBegan'`, `'touchMoved'`, `'touchEnded'`, `'touchCancelled'` | The field `touches` is a list of every finger with `id`, `x`, `y` and `changed`, which is `true` for the fingers this event is about. |
| `'textEdited'` | The field `field` is the id of the text field, a field of the UI or the one of [`input.editText`](#inputedittextoptions), `text` what its native field holds, `selectionStart` and `selectionEnd` the selection in code points, and `compositionStart` and `compositionEnd` the text an input method still composes, present only while it composes. |
| `'textAction'` | The field `field` is the id of the text field and `action` is `'submit'`, `'next'`, `'cancel'` or `'dismissed'`. |
| `'keyboardChanged'` | The field `frame` is the rectangle the on-screen keyboard covers, empty while it is hidden. |
| `'networkChanged'` | The field `online` is whether the device has a network. |
| `'mouseEnter'`, `'mouseLeave'`, `'resized'`, `'suspended'`, `'resumed'`, `'focusGained'`, `'focusLost'`, `'quitRequested'`, `'lowMemory'`, `'interruptionBegan'`, `'interruptionEnded'`, `'windowMoved'`, `'monitorsChanged'`, `'audioServicesReset'` | No other fields. |

The name `repeat` is a Lua keyword, so read it as `e['repeat']`.

```lua
local scene = require('haylen.scene')

scene.push({
    event = function(self, e)
        if e.type == 'keyDown' and not e['repeat'] then
            print('key', e.key)
        elseif e.type == 'character' then
            print('typed', e.character)
        elseif e.type == 'mouseDown' and e.modifiers.shift then
            print('shift click', e.button, e.x, e.y)
        elseif e.type == 'mouseMove' then
            print('moved by', e.dx, e.dy)
        elseif e.type == 'touchBegan' then
            for _, touch in ipairs(e.touches) do
                if touch.changed then
                    print('finger', touch.id, touch.x, touch.y)
                end
            end
        elseif e.type == 'focusLost' then
            print('paused')
        end
    end,
})
```

## Key names

| Group | Names |
| --- | --- |
| Letters | `a` to `z` |
| Digits | `digit0` to `digit9` |
| Function keys | `f1` to `f25` |
| Keypad | `keypad0` to `keypad9`, `keypadDecimal`, `keypadDivide`, `keypadMultiply`, `keypadSubtract`, `keypadAdd`, `keypadEnter`, `keypadEqual` |
| Symbols | `space`, `apostrophe`, `comma`, `minus`, `period`, `slash`, `semicolon`, `equal`, `leftBracket`, `backslash`, `rightBracket`, `graveAccent`, `world1`, `world2` |
| Editing and navigation | `escape`, `enter`, `tab`, `backspace`, `insert`, `delete`, `right`, `left`, `down`, `up`, `pageUp`, `pageDown`, `home`, `end` |
| Locks and system | `capsLock`, `scrollLock`, `numLock`, `printScreen`, `pause`, `menu` |
| Modifiers | `leftShift`, `leftControl`, `leftAlt`, `leftSuper`, `rightShift`, `rightControl`, `rightAlt`, `rightSuper` |

## Gamepad names

| Kind | Names |
| --- | --- |
| Face buttons | `south`, `east`, `west`, `north` |
| Shoulders and sticks | `leftShoulder`, `rightShoulder`, `leftStick`, `rightStick` |
| Menu buttons | `back`, `start`, `guide` |
| D-pad | `dpadUp`, `dpadDown`, `dpadLeft`, `dpadRight` |
| Axes | `leftX`, `leftY`, `rightX`, `rightY`, `leftTrigger`, `rightTrigger` |

## Errors

| Message | Cause |
| --- | --- |
| `unknown value '<name>'` | A key, mouse button, gamepad button or gamepad axis name does not exist. It comes inside a bad argument error. |
| `gamepad index out of range` | A gamepad index is outside 1 to 8. It comes inside a bad argument error. |
| `The "gamepad" of an action map must be an index from 1 to 8, or false for none.` | The option `gamepad` of `input.newActionMap()` or of `map.devices` is not a gamepad index or `false`. |
| `Unknown option "<key>".` | The options of `input.newActionMap()` or `map.devices` hold an unknown key. |
| `The list of the buttons of a step holds action names, and entry <n> is not one.` | The list of `map:stepButtons()` holds something other than a string. |
| `The command "<name>" names the button "<button>", which the buffer does not have.` | A step or a direction of `input.newCommandBuffer()` names a button that is not in `buttons`. |
| `The lifetimes name the button "<button>", which the buffer does not have.` | The `lifetimes` of `input.newCommandBuffer()` name a button that is not in `buttons`. |
| `A command lasts a whole number of steps from 1, or false to last until it is consumed.` | A lifetime is 0, negative, fractional or not a number. |
| `A command buffer needs from 1 to 32 buttons.` | The `buttons` of `input.newCommandBuffer()` are empty or more than 32. |
| `A command needs a name and at least one step.` | A command has no name or no steps. |
| `The command "<name>" has more than 16 steps.` | A command is longer than the history of the buffer. |
| `The command "<name>" names a direction, which needs the directions of the buffer.` | A step names `'forward'` or `'back'` in a buffer without `directions`. |
| `The directions of a command buffer name the "back" and the "forward" buttons.` | The `directions` lack one of the two names. |
| `The command name "<name>" is used by more than one command.` | Two commands, or a command and a button, share a name. |
| `The command buffer has no command "<name>".` | The method `consume` received a name the buffer does not have. |
| `The command buffer has no button "<name>".` | The method `held` received a name the buffer does not have. |
| `at most 32 actions fit in the buttons of a step` | The list of `map:stepButtons()` is longer than 32. It comes inside a bad argument error. |
| `expected left or right` | The function `input.gamepadStick()` received another side. It comes inside a bad argument error. |
| `Unknown option "<key>".` | The function `input.setGestureSettings()` received an unknown field. |
| `The gamepad dead zone must be at least 0 and below 1.` | The function `input.setGamepadDeadzone()` received a value outside that range. |
| `The motors of a rumble run from 0 to 1.` | The `low` or `high` of `input.rumble()` is outside 0 to 1. |
| `A rumble lasts a positive number of seconds.` | The `seconds` of `input.rumble()` are not positive. |
| `The press threshold must be above 0 and at most 1.` | The function `input.setPressThreshold()` received a value outside that range. |
| `Expected the action map to be a JSON object.` | An action map document is not a table with string keys. |
| `Expected an action to be a JSON object.` | An entry of the `actions` list is not a table with string keys. |
| `Unknown key "<key>" in the action map.` | An action map document has a key other than `actions`. |
| `Unknown key "<key>" in an action.` | An action has a key other than `name`, `type`, `buffer` and the binding lists. |
| `The action map needs a list of actions.` | An action map document has no `actions` list. |
| `An action needs a name and a type.` | An action lacks a string `name` or `type`. |
| `The type of an action must be "button", "axis" or "vector", not "<type>".` | An action type is not `button`, `axis` or `vector`. |
| `The "<list>" of an action must be a list of bindings.` | A binding list is not a list. |
| `The input binding "<binding>" is invalid. Use a binding such as "key:space", "mouse:left", "button:south", "axis:leftX+" or "stick:left".` | A binding is not a string in one of the formats in [Action map](#action-map). |
| `The <type> action "<name>" does not read "<list>".` | An action has a binding list its type never reads. |
| `The vector action "<name>" takes only sticks in "bindings", not "<binding>".` | The `bindings` of a vector action hold something other than a stick. |
| `The action name "<name>" is used by more than one action.` | Two actions of an action map document have the same name. |
| `The buffer of an action must be a number of seconds from 0.` | The `buffer` of an action is not a number or is negative. |
