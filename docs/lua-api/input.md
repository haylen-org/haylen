# 📖 `haylen.input`

The module `haylen.input` reads the keyboard, the mouse, touch screens, gestures, gamepads and TV remotes, and turns them into named actions through the action map. Gameplay reads actions, such as `jump` or `move`, so every device and every on-screen control works without extra code. Menus and tools read the devices themselves when they need to. The [input guide](../input/input.md) explains the action map, how the game and the UI share the devices, and what each device and platform does.

```lua
local input = require('haylen.input')
```

## Frame timing

The engine updates the devices and the action map once at the start of every frame. A state such as `input.keyPressed` or `input.pressed` stays `true` for that whole frame, in every scene callback, and is `false` again on the next frame. Read these frame states in `update`. In `fixedUpdate`, read actions with [`input.stepPressed`](#inputsteppressedaction), [`input.stepReleased`](#inputstepreleasedaction) and [`input.stepDown`](#inputstepdownaction), which give each press to exactly one fixed step. Positions are in design coordinates, the space that `design` of `app.json` sets. The [input guide](../input/input.md#frame-timing) explains why.

## Keyboard

Keys have the names that [Key names](#key-names) lists, such as `'space'`, `'leftShift'` or `'f3'`. An unknown name raises a bad argument error with `unknown value '<name>'`.

### input.keyDown(key)

Returns `true` while the key is held.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.running = input.keyDown('leftShift')
    end,
})
```

### input.keyPressed(key)

Returns `true` in the frame the key went down. The repeats of a held key are not new presses.

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

Returns `true` in the frame the key went up. The keys held when the window loses the focus or the app is suspended are released in that frame.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyReleased('space') then
            print('The charge was released.')
        end
    end,
})
```

### input.anyKeyPressed()

Returns `true` when any key went down in this frame. It suits title screens.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.anyKeyPressed() or input.mousePressed() then
            print('Start the game.')
        end
    end,
})
```

### input.modifiers()

Returns a table with the booleans `shift`, `control`, `alt` and `super`, taken from the latest key event. They all read `false` again from the frame the window loses the focus or the app is suspended.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.modifiers().control and input.keyPressed('s') then
            print('Save.')
        end
    end,
})
```

### input.text()

Returns the text typed in this frame as a UTF-8 string, or an empty string. It follows the keyboard layout and the input methods of the platform, so read it for text instead of key names. The text fields of [`haylen.ui`](ui.md) already handle typing.

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

Returns `true` while the current press of a key belongs to the UI of [`haylen.ui`](ui.md), such as the Escape that closes a combo list or a key typed into a text field. The press stays with the UI until the key is released. Meanwhile the `key:` bindings of the [action map](#action-map) read it as up, while the keyboard functions above keep reporting the key. The [input guide](../input/input.md#what-the-ui-captures) lists which presses the UI takes.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.keyPressed('escape') and not input.keyCaptured('escape') then
            print('Escape reached the game.')
        end
    end,
})
```

## Mouse

Mouse buttons are `'left'`, `'right'` and `'middle'`. The argument `button` is `'left'` when it is left out.

### input.mouseDown(button)

Returns `true` while the mouse button is held.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.painting = input.mouseDown()
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
            print('Open the context menu.')
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
            print('Stop panning.')
        end
    end,
})
```

### input.mousePosition()

Returns the position of the mouse in design coordinates, as two numbers. Turn it into a place in the world with `camera:screenToWorld` of [`haylen.graphics2d`](graphics2d.md), which takes design coordinates and counts the viewport of the camera.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        local x, y = input.mousePosition()
        self.worldX, self.worldY = self.camera:screenToWorld(x, y)
    end,
})
```

### input.mouseFramebufferPosition()

Returns the position of the mouse in pixels of the framebuffer, as two numbers, before any conversion to design coordinates. Use it for code that draws or reads the pixels of the window itself.

```lua
local input = require('haylen.input')

local x, y = input.mouseFramebufferPosition()
print('Pixel ' .. x .. ', ' .. y .. '.')
```

### input.mouseDelta()

Returns how far the mouse moved in this frame, in design units, as two numbers. It keeps reporting movement while `window.setMouseLocked` of [`haylen.window`](window.md) locks the mouse. A pointer that comes into the window, or a finger that lands while it drives the mouse, arrives without moving, so the mouse never jumps from where it left.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
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

Returns the scroll of this frame as two numbers, horizontal and vertical, in steps of a wheel. Positive values scroll to the left and up. A step of a mouse wheel counts as `1`, and a trackpad counts every 40 points of the screen it moves as one step. Chromium and Safari report most mouse wheels to a page in points, about 100 for a step, so there a step counts as about `2.5`. The speed of [`input.setScrollSpeed`](#inputsetscrollspeedspeed) multiplies both numbers.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
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

Returns `true` while the mouse is inside the window. A finger that drives the mouse with `input.setTouchAsMouse` takes the mouse out of the window when it lifts, since a touch screen has no hover.

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

Returns `true` while the UI of [`haylen.ui`](ui.md) owns the pointer, which is what `ui.usingPointer()` reported at the end of the frame before. A mouse button pressed meanwhile stays with the UI until it is released, and the `mouse:` bindings of the [action map](#action-map) read it as released. So a click on a menu button never swings a sword bound to `mouse:left`. A button pressed in the game stays with the game while the pointer passes over the UI.

The UI learns what the pointer is over while it draws. So a press with no hover before it, such as the tap of a finger that drives the mouse, goes to the game, unless the UI owned the pointer in the frame before. The mouse functions above keep reporting every button, and the virtual buttons of touch controls keep driving their actions.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.mousePressed() and not input.pointerCaptured() then
            print('The player clicked the world.')
        end
    end,
})
```

### input.setScrollSpeed(speed)

Multiplies every scroll of the wheels and the trackpads by `speed` before the UI, `input.mouseScroll` and the `mouseScroll` events see it. It is `1` by default, and it suits a setting of the player for how fast the wheel scrolls. The key `input.scrollSpeed` of `app.json` sets it at start. A value that is not positive raises `The scroll speed must be a positive number.` and keeps the current speed.

```lua
local input = require('haylen.input')

input.setScrollSpeed(0.5)
```

### input.scrollSpeed()

Returns the speed of every scroll, as `input.setScrollSpeed` set it.

```lua
local input = require('haylen.input')

print(input.scrollSpeed()) -- 1
```

## Touch

### input.touches()

Returns a list of the fingers on the screen, with the fingers that lifted in this frame. Each finger is a table with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | A number that stays the same while the finger is down. |
| `x`, `y` | number | The position now, in design coordinates. |
| `startX`, `startY` | number | Where the finger landed. |
| `dx`, `dy` | number | The movement since the frame before, adding up every move of the frame. |
| `phase` | string | One of `'began'`, `'moved'`, `'stationary'`, `'ended'` or `'cancelled'`. |
| `duration` | number | The seconds since the finger landed. |

- A finger that lands and moves in the same frame keeps the phase `'began'` for that frame.
- Fingers in the phase `'ended'` or `'cancelled'` leave the list on the next frame.
- A platform may give a new finger the `id` of a finger that lifted in the same frame. Then the list holds both for that frame, the lifted finger first.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        for _, touch in ipairs(input.touches()) do
            if touch.phase == 'began' then
                print('Finger ' .. touch.id .. ' landed at ' .. touch.x .. ', ' .. touch.y .. '.')
            elseif touch.phase == 'ended' then
                print('Finger ' .. touch.id .. ' lifted after ' .. touch.duration .. ' seconds.')
            end
        end
    end,
})
```

### input.findTouch(id)

Returns the finger with the number `id`, as a table with the fields of `input.touches`, or `nil` when no such finger is on the screen. It suits code that follows one finger from the frame it landed. When a new finger takes the number of a finger that lifted in the same frame, it returns the lifted finger for that frame.

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

The engine recognizes taps, double taps, long presses, swipes and pinches from the touches of every frame. While no finger is down, the left mouse button acts as a finger too, so gestures also work on desktops.

### input.gestures()

Returns a list of the gestures recognized in this frame. Each gesture is a table with these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `type` | string | One of `'tap'`, `'doubleTap'`, `'longPress'`, `'swipe'` or `'pinch'`. |
| `x`, `y` | number | Where the gesture happened, in design coordinates. A pinch reports the center between its two fingers. |
| `dx`, `dy` | number | For a swipe, the movement from where the finger landed to where it lifted. For a pinch, the movement of its center since exactly two fingers touched the screen, which pans a map with two fingers. Other gestures report `0`. |
| `scale` | number | For a pinch, the spread of the fingers divided by their spread when exactly two fingers touched the screen. A third finger that comes or goes starts it again. Other gestures report `1`. |
| `rotation` | number | For a pinch, how far it turned since exactly two fingers touched the screen, in radians from -π to π. It is positive in the direction that turns the x axis toward the y axis. Other gestures report `0`. |

- A tap fires when a finger lifts quickly without moving far.
- The second tap of a double tap reports both a `'tap'` and a `'doubleTap'` in the same frame.
- A long press fires once while the finger is still down, and that finger ends without a tap.
- A swipe fires when a finger lifts after it moved far enough, quickly enough.
- A pinch fires in every frame where one of exactly two fingers moves. Fingers of a pinch never end as taps or swipes.
- A finger that the system cancels ends without a gesture. So does every finger or mouse press still down when the window loses the focus or the app is suspended.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
        self.baseZoom, self.baseRotation = 1, 0
    end,
    update = function(self, dt)
        for _, gesture in ipairs(input.gestures()) do
            if gesture.type == 'doubleTap' then
                self.camera.zoom = {1, 1}
            elseif gesture.type == 'swipe' and math.abs(gesture.dx) > math.abs(gesture.dy) then
                print(gesture.dx > 0 and 'Next page.' or 'Previous page.')
            elseif gesture.type == 'longPress' then
                print('Open the menu at ' .. gesture.x .. ', ' .. gesture.y .. '.')
            elseif gesture.type == 'pinch' then
                self.camera.zoom = {self.baseZoom * gesture.scale, self.baseZoom * gesture.scale}
                self.camera.rotation = self.baseRotation + gesture.rotation
            end
        end
        if #input.touches() < 2 then
            self.baseZoom, self.baseRotation = self.camera.zoom.x, self.camera.rotation
        end
    end,
})
```

### input.setGestureSettings(settings)

Changes the limits of gesture recognition. Only the keys that `settings` holds change. Distances are in design units and times in seconds. An unknown key raises `Unknown option "<key>".`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `tapMaxDuration` | number | `0.3` | The longest press that still counts as a tap. |
| `tapMaxMovement` | number | `24` | The farthest a finger can move and still tap or long press. |
| `doubleTapInterval` | number | `0.35` | The longest time between the two taps of a double tap. |
| `doubleTapDistance` | number | `48` | The farthest apart the two taps of a double tap can be. |
| `longPressDuration` | number | `0.5` | The time a finger must stay down for a long press. |
| `swipeMinDistance` | number | `90` | The shortest movement that counts as a swipe. |
| `swipeMaxDuration` | number | `0.5` | The longest time a swipe can take. |
| `mouse` | boolean | `true` | Lets the left mouse button act as a finger while no finger is down. |

```lua
local input = require('haylen.input')

input.setGestureSettings({longPressDuration = 0.8, swipeMinDistance = 60, mouse = false})
```

### input.gestureSettings()

Returns every limit of gesture recognition, as a table with the keys that `input.setGestureSettings` takes. A settings screen can show and change them.

```lua
local input = require('haylen.input')

local settings = input.gestureSettings()
input.setGestureSettings({longPressDuration = settings.longPressDuration * 1.5})
```

## Gamepads

The engine tracks up to eight gamepads. The argument `index` picks one from `1` to `8`, and it is `1` when it is left out. An index outside that range raises a bad argument error with `gamepad index out of range`. Buttons and axes have the names that [Gamepad names](#gamepad-names) lists.

### input.gamepadConnected(index)

Returns `true` when a gamepad is connected at the index. The events `gamepadConnected` and `gamepadDisconnected` of [`haylen.events`](events.md#engine-events) announce each change with the index and the name.

```lua
local input = require('haylen.input')

for index = 1, 4 do
    if input.gamepadConnected(index) then
        print('Player ' .. index .. ' is ready.')
    end
end
```

### input.gamepadName(index)

Returns the name that the platform reports for the gamepad, or an empty string when none is connected.

```lua
local input = require('haylen.input')

print(input.gamepadName(1))
```

### input.gamepadLabels(index)

Returns how the gamepad labels its face buttons, so prompts show what the player sees on them.

| Labels | Meaning |
| --- | --- |
| `'letters'` | A at the bottom and B on the right. |
| `'swappedLetters'` | B at the bottom and A on the right. |
| `'shapes'` | Shapes instead of letters. |
| `'unknown'` | The labels are not known, or no gamepad is connected. |

Bindings keep naming the buttons by their place, so `button:south` is the bottom button whatever its label says. Apple platforms read the symbol of the bottom button. Windows reads only gamepads with letters. Android, Linux and browsers tell the labels from the maker of the gamepad, which leaves gamepads of other makers `'unknown'`.

```lua
local input = require('haylen.input')

local prompts = {letters = 'A', swappedLetters = 'B', shapes = 'Cross', unknown = 'South'}
print('Press ' .. prompts[input.gamepadLabels(1)] .. ' to jump.')
```

### input.rumble(index, options)

Runs the motors of a gamepad. The table `options` holds these keys.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `low` | number | `0` | The strength of the low frequency motor, from `0` to `1`. |
| `high` | number | `0` | The strength of the high frequency motor, from `0` to `1`. |
| `seconds` | number | `0.2` | How long the rumble lasts, above `0`. |

Both motors at `0` stop the gamepad. A new rumble replaces the one that runs. The engine stops every gamepad when the app loses the focus or stops. A strength out of range raises `The motors of a rumble run from 0 to 1.`, seconds that are not positive raise `A rumble lasts a positive number of seconds.`, and an unknown key raises `Unknown option "<key>".`.

| Platform | How it rumbles |
| --- | --- |
| iOS, iPadOS, tvOS, Mac Catalyst and macOS | The haptics engine of the controller plays one continuous event. Its intensity follows the stronger motor, and its sharpness follows the share of the high frequency motor. |
| Android | The vibrators of the controller, one motor on each when it has two, and the stronger motor on one. It needs the permission `VIBRATE`, which the template declares. |
| Windows | Both motors of XInput controllers. |
| Linux | The force feedback of the event device of the controller, when the user may write to it. The rules of most distributions allow it for the user at the computer. |
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
        self.aiming = input.gamepadDown('rightShoulder')
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
            print('Player 2 paused the game.')
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
            print('Throw.')
        end
    end,
})
```

### input.gamepadButtonCaptured(button, index)

Returns `true` while the current press of a gamepad button belongs to the UI, such as the east button that closes a popup. Meanwhile the `button:` bindings read it as up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.gamepadPressed('east') and not input.gamepadButtonCaptured('east') then
            print('The east button reached the game.')
        end
    end,
})
```

### input.gamepadAxis(axis, index)

Returns the value of one axis, with the dead zone removed and the rest stretched to the full range. Sticks go from `-1` to `1`, with positive y pointing down. Triggers go from `0` to `1`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        self.throttle = input.gamepadAxis('rightTrigger') - input.gamepadAxis('leftTrigger')
    end,
})
```

### input.gamepadStick(side, index)

Returns the position of the `'left'` or `'right'` stick as two numbers. The dead zone applies to the distance from the center, which keeps diagonals smooth, and the length never passes `1`. Another side raises a bad argument error with `expected left or right`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    x = 0,
    y = 0,
    update = function(self, dt)
        local sx, sy = input.gamepadStick('left')
        self.x = self.x + sx * 200 * dt
        self.y = self.y + sy * 200 * dt
    end,
})
```

### input.gamepadAxisCaptured(axis, index)

Returns `true` while the current lean of an axis past the press threshold belongs to the UI, such as the left stick that moves the focus between the controls of a menu. Meanwhile the `axis:` and `stick:` bindings of that axis read it as `0`. The argument `axis` is one of `'leftX'`, `'leftY'`, `'rightX'`, `'rightY'`, `'leftTrigger'` or `'rightTrigger'`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if math.abs(input.gamepadAxis('leftX')) > 0.5 and not input.gamepadAxisCaptured('leftX') then
            print('The stick steers the game.')
        end
    end,
})
```

### input.setGamepadDeadzone(deadzone)

Sets the dead zone of every axis and stick, from `0` to below `1`. It is `0.2` by default. Movement inside the dead zone reads as `0` and does not make the gamepad the last device used. Another value raises `The gamepad dead zone must be at least 0 and below 1.` and keeps the current dead zone.

```lua
local input = require('haylen.input')

input.setGamepadDeadzone(0.12)
```

### input.gamepadDeadzone()

Returns the dead zone of every axis and stick, as `input.setGamepadDeadzone` set it.

```lua
local input = require('haylen.input')

input.setGamepadDeadzone(input.gamepadDeadzone() + 0.05)
```

### input.findGamepadPress(button)

Returns the lowest index of a gamepad that pressed `button` in this frame, or `nil` when none did. When `button` is `nil`, any button counts. A screen where players join gives the player who pressed a button an [`ActionMap`](#actionmap) that reads that gamepad.

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

## Devices

### input.lastDevice()

Returns the kind of device that the player used last: `'keyboardMouse'`, `'touch'` or `'gamepad'`. Use it to show matching button prompts or on-screen controls.

- A key press or a mouse button press picks `'keyboardMouse'`.
- A touch picks `'touch'`.
- A gamepad button going down, or an axis leaving the dead zone, picks `'gamepad'`.

Only a new press counts. Key repeats, a gamepad button or stick that stays held, and the keys of the plain on-screen keyboard that `window.setKeyboardVisible` opens never take over from the device the player used last.

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

Makes the mouse act as a finger, so an app made for touch screens runs with a mouse. A desktop build that tests touch controls is the usual use. The key `input.mouseAsTouch` of `app.json` sets it at start.

- The left button becomes a touch with an `id` that no real finger has. The touch begins, moves while the button is held, and ends.
- The function `input.lastDevice` reports `'touch'`, so controls with `touchOnly` show.
- Moves without a button disappear, since touch screens have no hover. The right and middle buttons and the wheel stay mouse input.
- The UI, gestures, touch controls and the app all see the finger.
- A press keeps the device it started as until it ends. So turning this on or off while the button is held never leaves a finger or a button down.

```lua
local input = require('haylen.input')

input.setMouseAsTouch(true)
print(input.mouseAsTouch()) -- true
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

Makes a finger act as the mouse too, so an app that reads the mouse runs on a touch screen. The key `input.touchAsMouse` of `app.json` sets it at start.

- The first finger down drives the left button and the mouse position until it lifts. The mouse leaves the window with it, and the next finger down takes over.
- A finger that drives the mouse keeps driving it until it lifts, even when this turns off meanwhile.
- The touches still arrive as touches, so touch controls and gestures keep working, and `input.lastDevice` stays `'touch'`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.setTouchAsMouse(true)
scene.push({
    update = function(self, dt)
        if input.mousePressed('left') then
            local x, y = input.mousePosition()
            print('Pressed at ' .. x .. ', ' .. y .. '.')
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

A GUI that the app draws itself, such as one written in Lua over `haylen.graphics2d`, edits text through the native text input of the platform, as the text fields of `haylen.ui` do. It gets the on-screen keyboard, autocorrection and the input methods that compose Japanese, Chinese or Korean text. The app keeps the text and draws it. The platform reports every edit as a `textEdited` event with the field id `4294967296`, and its actions as a `textAction` event. One field edits at a time, and a text field of `haylen.ui` that takes the focus takes the keyboard over. The [text input guide](../ui/text-input.md) describes what each platform does.

### input.editText(options)

Starts editing the text field of the app, or follows it when it changes or moves. Call it again every frame, or whenever the text, the selection or the place change. The native field takes the text and the selection only when they differ from what the user typed last, so an app that shows the typed text back keeps a composition going.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | string | required | The text of the field. |
| `selectionStart`, `selectionEnd` | integer | the other end, or the end of the text | The selection in code points. When both are equal, it is the caret. With one end given, the caret is there. |
| `x`, `y`, `width`, `height` | number | `0` | The place of the field in design coordinates. The platform shows its input method windows there and keeps it above the on-screen keyboard. |
| `keyboard` | string | `'text'` | `'text'`, `'multiline'`, `'number'`, `'decimal'`, `'phone'`, `'email'`, `'url'`, `'search'` or `'password'`. |
| `returnKey` | string | `'default'` | `'default'`, `'done'`, `'go'`, `'next'`, `'search'` or `'send'`. |
| `autocapitalize` | string | `'sentences'` | `'none'`, `'sentences'`, `'words'` or `'characters'`. |
| `autocorrect` | boolean | `true` | Lets the system correct words. |
| `maxLength` | integer | `0` | The most code points the field takes, or `0` for no limit. |

A missing text raises `A text field needs its "text" as a string.`. A bad selection raises `The selection of a text field must run from 0 to the length of its text, with "selectionStart" not past "selectionEnd".`. An unknown name raises `The option "<option>" of a text field must be <names>.`, and an unknown key raises `Unknown option "<key>".`.

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

The action map gives names to gameplay actions and binds each one to any number of keys, mouse buttons, gamepad controls and virtual controls. The functions of this section read and change the default map. Each player of a local multiplayer game can have a map of their own, an [`ActionMap`](#actionmap), with the same functions as methods. The [input guide](../input/input.md#the-action-map) explains how to design actions.

### Action types

| Type | Binding lists | What it reports |
| --- | --- | --- |
| `'button'` | `bindings` | The `value` is the strongest binding, from `0` to `1`. The action is down while the value is at least the press threshold, `0.5` by default. |
| `'axis'` | `positive` and `negative` | The `value` is the strongest positive binding minus the strongest negative binding, from `-1` to `1`. The action is down while the size of the value is at least the press threshold. |
| `'vector'` | `up`, `down`, `left`, `right`, and sticks in `bindings` | The `vector` is right minus left and down minus up, plus every stick binding, cut to a length of `1`. The `value` is that length, and the action is down while it is at least the press threshold. |

An action takes only the binding lists of its type. So a `positive` list on a button raises `The button action "<name>" does not read "positive".`. The `bindings` of a vector action hold only `stick:` and `virtualStick:` bindings, so a key there raises `The vector action "<name>" takes only sticks in "bindings", not "key:w".`.

### Bindings

Each binding is a string with a source and a name.

| Binding | Meaning | Examples |
| --- | --- | --- |
| `key:<key>` | A key from [Key names](#key-names). | `key:space`, `key:leftShift`, `key:w` |
| `mouse:<button>` | A mouse button. | `mouse:left`, `mouse:right` |
| `button:<button>` | A gamepad button from [Gamepad names](#gamepad-names). | `button:south`, `button:dpadUp`, `button:start` |
| `axis:<axis>+` or `axis:<axis>-` | One direction of a gamepad axis, from `0` to `1`. | `axis:rightTrigger+`, and `axis:leftY-` for the left stick pushed up |
| `stick:left` or `stick:right` | A whole gamepad stick, for vector actions. | `stick:left` |
| `virtual:<name>` | A virtual button that `input.setVirtualButton` or a `touchButton` of the UI sets. | `virtual:attack` |
| `virtualStick:<name>` | A virtual stick that `input.setVirtualStick` or a `touchStick` of the UI sets, for vector actions. | `virtualStick:move` |

Digital bindings count as `0` or `1`. Analog bindings, such as triggers and stick axes, count by how far they are pushed.

- Gamepad bindings read every connected gamepad and take the strongest one, unless the [`devices`](#mapdevices) of the map pick one gamepad or none.
- Mouse bindings read as released for a press that the UI took, as [`input.pointerCaptured`](#inputpointercaptured) explains.
- Key, gamepad button and axis bindings read as up for a press that the UI answers itself, as [`input.keyCaptured`](#inputkeycapturedkey) explains.
- While a scene change holds input back, with the option `blockInput` of [`haylen.scene`](scene.md), every action reads as up and `0`. An action whose bindings are still held when the change ends stays up until they are released.
- An action name that was never defined reads as up and `0`.

### Documents

An action map document is a table, or a JSON file, with an `actions` list. Each action has a `name`, a `type`, the binding lists of its type, and an optional `buffer` in seconds, `0` by default. The buffer keeps a press ready for [`input.consumePress`](#inputconsumepressaction) that long after the first fixed step that saw it. A buffer that is not a number from `0` raises `The buffer of an action must be a number of seconds from 0.`.

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

Replaces the default map with a document, given as a table or as the path of a JSON file in the `content` folder. Nothing changes when the document is not valid. An empty Lua table counts as an empty list. These errors can happen:

- A document that is not a table with string keys raises `Expected the action map to be a JSON object.`, and an action that is not one raises `Expected an action to be a JSON object.`.
- An unknown key raises `Unknown key "<key>" in the action map.` or `Unknown key "<key>" in an action.`.
- A document without its list raises `The action map needs a list of actions.`.
- An action without a name or a type raises `An action needs a name and a type.`.
- An unknown type raises `The type of an action must be "button", "axis" or "vector", not "<type>".`.
- A binding list that is not a list raises `The "<list>" of an action must be a list of bindings.`.
- A bad binding raises `The input binding "<binding>" is invalid. Use a binding such as "key:space", "mouse:left", "button:south", "axis:leftX+" or "stick:left".`.
- A name that two actions use raises `The action name "<name>" is used by more than one action.`.

```lua
local input = require('haylen.input')

input.loadActions('input.json')

input.loadActions({actions = {
    {name = 'fire', type = 'button', bindings = {'mouse:left', 'axis:rightTrigger+'}},
    {name = 'aim', type = 'vector', bindings = {'stick:right'}},
}})
```

### input.saveActions()

Returns the default map as a document table, in the format that `input.loadActions` takes. Keep remapped controls with it, or let `preferences.capture()` of [`haylen.preferences`](preferences.md) store it under `input.actions`.

```lua
local input = require('haylen.input')
local preferences = require('haylen.preferences')

preferences.set('controls', input.saveActions())
preferences.save()
input.loadActions(preferences.get('controls'))
```

### input.actionNames()

Returns the names of the actions of the default map, in the order they were defined.

```lua
local input = require('haylen.input')

for _, name in ipairs(input.actionNames()) do
    print(name, input.down(name))
end
```

### input.defineAction(action)

Adds one action, given as a table in the format of an action of a document. An action with the same name is replaced in its place, so the order stays. It raises the errors of `input.loadActions`, and nothing changes when the action is not valid.

```lua
local input = require('haylen.input')

input.defineAction({name = 'dash', type = 'button', bindings = {'key:leftShift', 'button:east'}})
```

### input.actionDefinition(name)

Returns the action with that name as a table in the format of a document, or `nil` when there is none. Empty binding lists are left out. A remapping screen reads an action, swaps one binding and defines it again.

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

Removes every action of the default map.

```lua
local input = require('haylen.input')

input.clearActions()
input.loadActions('input.json')
```

### input.setPressThreshold(threshold)

Sets the value at which every action of the default map counts as down, above `0` and at most `1`. It is `0.5` by default. A lower threshold lets light trigger pulls and small stick moves press actions. Another value raises `The press threshold must be above 0 and at most 1.` and keeps the current threshold.

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
        self.firing = input.down('fire')
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
            print('Jump.')
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
            print('Cut the jump short.')
        end
    end,
})
```

### input.value(action)

Returns the value of the action, as [Action types](#action-types) describes.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    speed = 0,
    update = function(self, dt)
        self.speed = self.speed + input.value('throttle') * 300 * dt
    end,
})
```

### input.vector(action)

Returns the direction of a vector action as two numbers, with a length of at most `1`. Other types return `0, 0`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    x = 0,
    y = 0,
    update = function(self, dt)
        local dx, dy = input.vector('move')
        self.x = self.x + dx * 180 * dt
        self.y = self.y + dy * 180 * dt
    end,
})
```

### input.stepPressed(action)

Returns `true` in the first fixed step after the action went down. Every press reaches exactly one step, also when its frame runs no step or several steps. So a simulation in `fixedUpdate` never misses a press or sees it twice. A press and a release that both arrive before the same step count as a press in that step and a release in the next one.

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

Returns `true` in the first fixed step after the action went up.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    velocity = -400,
    fixedUpdate = function(self, step)
        if input.stepReleased('jump') and self.velocity < 0 then
            self.velocity = self.velocity * 0.5
        end
    end,
})
```

### input.stepDown(action)

Returns `true` while the action is down in the current fixed step. It is also `true` in the step after a press that was released before any step saw it, so a tap shorter than a step still holds the action for one step.

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

Returns `true` once for each press of the action, and takes the press. The next call returns `false` until the action goes down again. A press stays ready from its frame to the end of the first fixed step after it, and the `buffer` of the action keeps it ready that much longer, counted in fixed steps. That is the usual jump buffer: a jump pressed just before landing still jumps. It works in `update` and in `fixedUpdate`. Input that a scene change holds back drops the press.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.defineAction({name = 'jump', type = 'button', bindings = {'key:space', 'button:south'}, buffer = 0.12})

scene.push({
    grounded = true,
    fixedUpdate = function(self, step)
        if self.grounded and input.consumePress('jump') then
            self.velocity = -520
        end
    end,
})
```

## ActionMap

An `ActionMap`, named `haylen.ActionMap` in messages, is an action map that reads only the devices it names. Each player of a local multiplayer game gets one, so each player reads their own gamepad or touch layout. Make one with [`input.newActionMap`](#inputnewactionmapoptions). The default map, which the functions of the [action map](#action-map) section read, is an `ActionMap` too, and [`input.actions`](#inputactions) returns it.

Every map loads the same documents as the default map, with the same errors. It updates at the start of every frame and every fixed step. It loses the presses that the UI takes, like the default map. It leaves the engine once Lua lets go of it. Two values of the same map compare equal with `==`.

### input.newActionMap(options)

Makes an action map that reads the devices of `options`, every device by default.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `keyboard` | boolean | `true` | Whether `key:` bindings read the keyboard. |
| `mouse` | boolean | `true` | Whether `mouse:` bindings read the mouse. |
| `gamepad` | integer or `false` | every gamepad | The index of the one gamepad that `button:`, `axis:` and `stick:` bindings read, from `1` to `8`, or `false` for none. |
| `virtual` | boolean | `true` | Whether `virtual:` and `virtualStick:` bindings read the virtual controls. |
| `virtualPrefix` | string | `''` | Text that goes in front of the name of every virtual binding. With the prefix `p2.`, the binding `virtual:jump` reads the virtual button `p2.jump`. So two touch layouts on one screen drive two players. |
| `actions` | table or string | none | A document table, or the path of a JSON file, to load as `map:load` does. |

A bad `gamepad` raises `The "gamepad" of an action map must be an index from 1 to 8, or false for none.`, and an unknown key raises `Unknown option "<key>".`.

```lua
local input = require('haylen.input')

local keyboardPlayer = input.newActionMap({gamepad = false, actions = 'input.json'})
local padPlayer = input.newActionMap({keyboard = false, mouse = false, gamepad = 1, actions = 'input.json'})
local touchPlayer = input.newActionMap({keyboard = false, mouse = false, gamepad = false, virtualPrefix = 'p2.', actions = 'input.json'})
```

### input.actions()

Returns the default map, which the functions of the module read and change. Two calls return values that compare equal.

```lua
local input = require('haylen.input')

local default = input.actions()
default.devices = {gamepad = 1}
print(default == input.actions()) -- true
```

### map:load(document)

Replaces the actions of the map with a document, given as a table or as the path of a JSON file, as [`input.loadActions`](#inputloadactionsdocument) does.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 2})
player:load('input.json')
```

### map:save()

Returns the actions of the map as a document table, as [`input.saveActions`](#inputsaveactions) does.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 2, actions = 'input.json'})
local document = player:save()
print(#document.actions)
```

### map:names()

Returns the names of the actions of the map in the order they were defined, as [`input.actionNames`](#inputactionnames) does.

```lua
local input = require('haylen.input')

local player = input.newActionMap({actions = 'input.json'})
print(table.concat(player:names(), ', '))
```

### map:define(action)

Adds one action to the map, or replaces the action with the same name in its place, as [`input.defineAction`](#inputdefineactionaction) does.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 2})
player:define({name = 'taunt', type = 'button', bindings = {'button:north'}})
```

### map:definition(name)

Returns one action of the map as a table, or `nil`, as [`input.actionDefinition`](#inputactiondefinitionname) does.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 2})
player:define({name = 'taunt', type = 'button', bindings = {'button:north'}})
print(player:definition('taunt').bindings[1]) -- button:north
```

### map:remove(name)

Removes one action of the map, as [`input.removeAction`](#inputremoveactionname) does.

```lua
local input = require('haylen.input')

local player = input.newActionMap({actions = 'input.json'})
player:remove('taunt')
```

### map:clear()

Removes every action of the map.

```lua
local input = require('haylen.input')

local player = input.newActionMap({actions = 'input.json'})
player:clear()
```

### map:down(action)

Returns `true` while the action of the map is down, as [`input.down`](#inputdownaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 2, actions = 'input.json'})

scene.push({
    update = function(self, dt)
        self.blocking = player:down('defend')
    end,
})
```

### map:pressed(action)

Returns `true` in the frame the action of the map went down, as [`input.pressed`](#inputpressedaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 2, actions = 'input.json'})

scene.push({
    update = function(self, dt)
        if player:pressed('jump') then
            print('Player 2 jumps.')
        end
    end,
})
```

### map:released(action)

Returns `true` in the frame the action of the map went up, as [`input.released`](#inputreleasedaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 2, actions = 'input.json'})

scene.push({
    update = function(self, dt)
        if player:released('fire') then
            print('Player 2 lets the arrow go.')
        end
    end,
})
```

### map:value(action)

Returns the value of the action of the map, as [`input.value`](#inputvalueaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 2, actions = 'input.json'})

scene.push({
    update = function(self, dt)
        self.gas = player:value('throttle')
    end,
})
```

### map:vector(action)

Returns the direction of a vector action of the map, as [`input.vector`](#inputvectoraction) does.

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

### map:stepDown(action)

Returns `true` while the action of the map is down in the current fixed step, as [`input.stepDown`](#inputstepdownaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 1, actions = 'input.json'})

scene.push({
    charge = 0,
    fixedUpdate = function(self, step)
        self.charge = player:stepDown('fire') and self.charge + step or 0
    end,
})
```

### map:stepPressed(action)

Returns `true` in the first fixed step after the action of the map went down, as [`input.stepPressed`](#inputsteppressedaction) does.

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

### map:stepReleased(action)

Returns `true` in the first fixed step after the action of the map went up, as [`input.stepReleased`](#inputstepreleasedaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 1, actions = 'input.json'})

scene.push({
    fixedUpdate = function(self, step)
        if player:stepReleased('defend') then
            print('The guard drops.')
        end
    end,
})
```

### map:consumePress(action)

Returns `true` once for each press of the action of the map, and takes the press, as [`input.consumePress`](#inputconsumepressaction) does.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 1, actions = 'input.json'})

scene.push({
    grounded = true,
    fixedUpdate = function(self, step)
        if self.grounded and player:consumePress('jump') then
            self.velocity = -520
        end
    end,
})
```

### map:stepButtons(list)

Returns one integer for the current fixed step. Its bit `n - 1` is set when the action `list[n]` is down in the step, as `map:stepDown` tells. Command buffers, recordings and bots keep this one number per player and step. The list holds at most 32 action names, and a name that the map does not define reads as up. A longer list raises a bad argument error with `at most 32 actions fit in the buttons of a step`, and an entry that is not a string raises `The list of the buttons of a step holds action names, and entry <n> is not one.`.

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

The devices that the map reads, as a table with the keys of the options of `input.newActionMap` except `actions`. The key `gamepad` is left out while the map reads every gamepad. Assigning a table replaces every device setting, and the keys it leaves out take their defaults. It raises the errors of `input.newActionMap`.

```lua
local input = require('haylen.input')

local player = input.newActionMap()
player.devices = {keyboard = false, mouse = false, gamepad = 3}
print(player.devices.gamepad, player.devices.keyboard) -- 3 false
```

### map.pressThreshold

The value at which every action of the map counts as down, above `0` and at most `1`, `0.5` by default. Another value raises `The press threshold must be above 0 and at most 1.`.

```lua
local input = require('haylen.input')

local player = input.newActionMap({gamepad = 1})
player.pressThreshold = 0.3
```

## CommandBuffer

A `CommandBuffer`, named `haylen.CommandBuffer` in messages, turns the buttons held on each fixed step into one kept command, the way action games read moves. A command is a single press, a chord of buttons on one step, a double tap, or a sequence of presses such as defend, forward and attack, typed within a number of steps. Make one with [`input.newCommandBuffer`](#inputnewcommandbufferoptions). The [input guide](../input/input.md#command-buffers) shows when to use one.

- When several commands complete on one step, the highest priority wins, and the later one wins a tie.
- A press on a later step replaces the kept command.
- The kept command expires after its lifetime in steps, unless the buffer is frozen, such as during hit stop.
- Time counts in steps, never in seconds. So the same presses always type the same commands, which replays and tests need.
- A step is one call that allocates nothing.

### input.newCommandBuffer(options)

Makes a command buffer from these options.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `buttons` | list of strings | required | The button names, from 1 to 32. Their order is also the order of presses within one step. Each button types a single-press command with its own name. |
| `lifetime` | integer or `false` | `1` | How many steps a typed command lasts, or `false` to keep it until it is consumed or replaced. A lifetime of `1` lasts only the step it was typed on, even while frozen. |
| `lifetimes` | table | none | The lifetimes of the single presses of some buttons, by button name, such as `{left = 1, right = 1}`. |
| `directions` | table | none | `{back = name, forward = name}`, the buttons of a character that faces right. With them, the steps of a command may name `'forward'` and `'back'`. |
| `commands` | list | `{}` | The named commands, each a table with the keys below. |

| Command key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `name` | string | required | The name of the command. |
| `steps` | list of strings | required | Button names, or `'forward'` and `'back'`, in the order they are pressed. At most 16 steps. |
| `maxTicks` | integer | none | The most steps between the first and the last press. Without it, there is no limit. |
| `priority` | integer | `0` | Which command wins when several complete on one step. Single presses have priority `0`. |
| `lifetime` | integer or `false` | the `lifetime` of the buffer | How many steps the command lasts once typed. |
| `clearsHistory` | boolean | `true` | Forgets the typed presses once the command completes. |

A chord is a command with `maxTicks = 0`, whose buttons go down on the same step. A double tap is two presses of one button with nothing pressed between them. The [errors](#errors) section lists the messages of bad options.

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

Moves the buffer one step forward with the buttons that are down on it. The argument is either an action map, whose actions named like the buttons give their step state as `map:stepDown` tells, so a tap shorter than a step still counts. Or it is an integer whose bit `n - 1` holds the button `n`, such as `map:stepButtons` returns or a recording plays back. An integer outside 32 bits raises a bad argument error with `the buttons of a step are an integer of 32 bits`.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'jump', 'attack'}, lifetime = 6})
commands:update(0x2)
print(commands:peek()) -- attack
```

### buffer:settle(mapOrMask)

Takes the buttons held now as already down, without a step. So a button held across a pause or the start of a round never types a press. It takes the same arguments as `buffer:update`.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'jump', 'attack'}})
commands:settle(0x2)
commands:update(0x2)
print(commands:peek()) -- nil
```

### buffer:peek()

Returns the name of the kept command, or `nil`, without taking it. The name is a string that the buffer already holds, so a step allocates nothing.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'attack'}, lifetime = 4})
commands:update(1)
if commands:peek() == 'attack' then
    print('An attack waits.')
end
```

### buffer:consume(name)

Takes the kept command and returns `true` when it is `name`, or returns `false`. A name that the buffer does not have, as a button or a command, raises `The command buffer has no command "<name>".`.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'attack'}, lifetime = 4})
commands:update(1)
print(commands:consume('attack'), commands:consume('attack')) -- true false
```

### buffer:held(button)

Returns `true` while the button is down on the last step. A name that the buffer does not have raises `The command buffer has no button "<name>".`.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'defend'}})
commands:update(1)
print(commands:held('defend')) -- true
```

### buffer:clear()

Forgets the kept command and the typed presses, and keeps the buttons that are held, such as when a character comes back to life.

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'attack'}, lifetime = 10})
commands:update(1)
commands:clear()
print(commands:peek()) -- nil
```

### Properties

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `frozen` | boolean | `false` | Stops the lifetimes longer than one step from counting, such as during hit stop. |
| `facing` | number | `1` | `1` for a character that faces right and `-1` for one that faces left, which swaps the steps `'forward'` and `'back'`. Any negative number sets `-1`, and any other number sets `1`. |
| `tick` | integer | `0` | The steps the buffer took. Read-only. |

```lua
local input = require('haylen.input')

local commands = input.newCommandBuffer({buttons = {'left', 'right'}, directions = {back = 'left', forward = 'right'}})
commands.facing = -1
commands.frozen = true
commands:update(0)
print(commands.tick, commands.facing, commands.frozen) -- 1 -1 true
```

## Recordings

A recording keeps the buttons of every player on every fixed step of a run. The buttons of one player on one step are a mask, an integer whose bit `n - 1` holds the button `n`. A recording also keeps the identifier and the fixed rate of the app, and a string of the app, such as the states of its random generators. A playback hands the steps back in place of the devices. That makes replays, ghosts, attract modes, automated playtests on the headless player and the reproduction of bug reports. Steps that repeat the same masks cost one entry, so a minute of play takes a few hundred bytes. The [input guide](../input/input.md#recording-and-replay) explains how a run replays exactly.

An [`InputRecording`](#inputrecording) writes and stores the steps, and an [`InputPlayback`](#inputplayback) plays them back.

## InputRecording

An `InputRecording`, named `haylen.InputRecording` in messages, holds the masks of a run. Make one with [`input.newRecorder`](#inputnewrecorderoptions) to record, or with [`input.loadRecording`](#inputloadrecordingbytes) to read saved bytes.

### input.newRecorder(options)

Makes an empty recording for this app.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `buttons` | list of strings | required | The names of the buttons, from 1 to 32. Each name has at most 255 bytes. |
| `players` | integer | `1` | The number of players, from 1 to 16. |
| `data` | string | `''` | A string of the app, such as the state of a random generator, at most 1048576 bytes. |

These options raise `An input recording needs from 1 to 32 buttons.`, `An input recording needs from 1 to 16 players.`, `The names of an input recording have at most 255 bytes.`, `The data of an input recording has at most 1048576 bytes.` and `Unknown option "<key>".`.

```lua
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')

local random = m.random(42)
local recorder = input.newRecorder({buttons = {'left', 'right', 'jump'}, players = 1, data = random:state()})

scene.push({
    fixedUpdate = function(self, step)
        recorder:write(1, input.actions())
        recorder:advance()
    end,
})
```

### input.loadRecording(bytes)

Reads the bytes of `recording:bytes()` back into a recording. Bytes that are not a whole recording raise `The input recording is malformed: <reason>.`. A recording of another app raises `The recording belongs to another app.`. A recording made at another fixed rate raises `The recording was made at <rate> fixed steps a second, and the app runs at <rate>.`, since its steps would mean other times.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:advance()
local copy = input.loadRecording(recorder:bytes())
print(copy.steps, copy.players, copy.buttons[1], copy.data)
```

### recording:write(player, maskOrMap)

Sets the buttons of a player, from `1`, for the step that the next `advance` closes. The buttons are an integer mask, or an action map whose actions named like the buttons give their step state. A player outside the recording raises `The input recording has players from 1 to <n>.`. A mask outside 32 bits raises a bad argument error with `the buttons of a step are an integer of 32 bits`.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'left', 'right'}, players = 2})
recorder:write(1, 0x1)
recorder:write(2, input.actions())
recorder:advance()
```

### recording:advance(hash)

Closes the step with the masks written for it, and `0` for a player without one. The optional `hash` is an integer of 32 bits that sums up the state of the app, such as `world:stateHash()` of a physics world. A playback compares it to find the first step where a replay goes another way. A hash outside 32 bits raises a bad argument error with `a step hash is an integer of 32 bits`. A recording holds at most 67108864 steps, and a step past that raises `An input recording holds at most 67108864 steps.`.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:write(1, 1)
recorder:advance(12345)
print(recorder.steps) -- 1
```

### recording:bytes()

Returns the recording as a string of bytes. Keep it in a file under [`storage.root()`](storage.md), in a save, or send it to a server.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:advance()
print(#recorder:bytes() .. ' bytes.')
```

### Properties

Every property of a recording is read-only.

| Property | Type | Meaning |
| --- | --- | --- |
| `steps` | integer | The number of steps. |
| `players` | integer | The number of players. |
| `buttons` | list of strings | The names of the buttons. |
| `data` | string | The string of the app. |

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'left', 'right'}, players = 2, data = 'level 3'})
print(recorder.steps, recorder.players, #recorder.buttons, recorder.data) -- 0 2 2 level 3
```

## InputPlayback

An `InputPlayback`, named `haylen.InputPlayback` in messages, hands the steps of a recording back one at a time. Make one with [`input.newPlayback`](#inputnewplaybackrecording).

### input.newPlayback(recording)

Makes a playback of an [`InputRecording`](#inputrecording), standing before its first step.

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
            self.jumping = playback:mask(1) & 0x4 ~= 0
            playback:advance()
        end,
    })
end

return replay
```

### playback:mask(player)

Returns the buttons of a player, from `1`, on the current step, or `0` once the playback finished. A player outside the recording raises `The input recording has players from 1 to <n>.`.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:write(1, 1)
recorder:advance()
local playback = input.newPlayback(recorder)
print(playback:mask(1)) -- 1
```

### playback:hash()

Returns the hash of the current step, or `nil` when the step has none or the playback finished.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:advance(777)
local playback = input.newPlayback(recorder)
print(playback:hash()) -- 777
```

### playback:advance()

Moves to the next step. It does nothing once the playback finished.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
recorder:advance()
local playback = input.newPlayback(recorder)
playback:advance()
print(playback.finished) -- true
```

### playback:seek(step)

Moves to a step, from `0` to the number of steps. Another step raises `The playback has steps from 0 to <n>.`.

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}})
for step = 1, 3 do
    recorder:write(1, step % 2)
    recorder:advance(step)
end
local playback = input.newPlayback(recorder)
playback:seek(2)
print(playback:mask(1), playback:hash()) -- 1 3
```

### Properties

Every property of a playback is read-only.

| Property | Type | Meaning |
| --- | --- | --- |
| `step` | integer | The steps played so far. |
| `steps` | integer | The number of steps of the recording. |
| `finished` | boolean | Whether every step was played. |
| `data` | string | The string of the app that the recording keeps. |

```lua
local input = require('haylen.input')

local recorder = input.newRecorder({buttons = {'jump'}, data = 'seed 9'})
recorder:advance()
local playback = input.newPlayback(recorder)
playback:advance()
print(playback.step, playback.steps, playback.finished, playback.data) -- 1 1 true seed 9
```

## Virtual controls

Virtual buttons and sticks are named inputs that code or on-screen controls set, and that the action map reads through `virtual:` and `virtualStick:` bindings. The `touchButton` and `touchStick` controls of [`haylen.ui`](ui.md#touch-controls) set them from their `action` property. An app can also set them itself for controls of its own. A value set during a frame reaches the actions at the start of the next frame.

### input.setVirtualButton(name, down)

Presses or releases a virtual button. It stays as it was set until it is set again.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'jump', type = 'button', bindings = {'key:space', 'virtual:jump'}}}})

-- A finger on the right part of the screen holds the jump.
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

Sets the position of a virtual stick. A vector longer than `1` is cut to a length of `1`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'move', type = 'vector', bindings = {'stick:left', 'virtualStick:move'}}}})

-- A finger that drags on the left part of the screen steers.
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

A scene with an `event(self, e)` hook receives every platform event as a table, as soon as it arrives and before the frame that uses it. The field `type` names the event, and the other fields depend on the type. Positions and movements are in design coordinates. Key, character, mouse and touch events also carry `modifiers`, a table with the booleans `shift`, `control`, `alt` and `super` for the modifier keys held when the event happened.

| Type | Fields |
| --- | --- |
| `'keyDown'`, `'keyUp'` | The field `key` is the key name, or `'unknown'` for a key without one. The field `repeat` is `true` for the repeats of a held key. |
| `'character'` | The field `character` is the typed character as a UTF-8 string. |
| `'mouseDown'`, `'mouseUp'` | The field `button` is the mouse button name, and `x` and `y` are the position of the mouse. |
| `'mouseMove'` | The fields `x` and `y` are the position of the mouse. The fields `dx` and `dy` are the movement of this event, which keeps coming while the mouse is locked. |
| `'mouseScroll'` | The fields `scrollX` and `scrollY` are the scroll, positive to the left and up. They are in points of the screen when `precise` is `true`, as a trackpad reports them, and in steps of a wheel otherwise. |
| `'touchBegan'`, `'touchMoved'`, `'touchEnded'`, `'touchCancelled'` | The field `touches` is a list of every finger with `id`, `x`, `y` and `changed`, which is `true` for the fingers that this event is about. |
| `'textEdited'` | The field `field` is the id of the text field, a field of the UI or the one of [`input.editText`](#inputedittextoptions). The field `text` is what its native field holds, `selectionStart` and `selectionEnd` are the selection in code points, and `compositionStart` and `compositionEnd` are the text that an input method still composes, present only while it composes. |
| `'textAction'` | The field `field` is the id of the text field, and `action` is `'submit'`, `'next'`, `'cancel'` or `'dismissed'`. |
| `'keyboardChanged'` | The field `frame` is the rectangle that the on-screen keyboard covers, empty while it is hidden. |
| `'networkChanged'` | The field `online` tells whether the device has a network. |
| `'mouseEnter'`, `'mouseLeave'`, `'resized'`, `'suspended'`, `'resumed'`, `'focusGained'`, `'focusLost'`, `'quitRequested'`, `'lowMemory'`, `'interruptionBegan'`, `'interruptionEnded'`, `'windowMoved'`, `'monitorsChanged'`, `'audioServicesReset'` | No other fields. |

The name `repeat` is a word of Lua, so read it as `e['repeat']`.

```lua
local scene = require('haylen.scene')

scene.push({
    event = function(self, e)
        if e.type == 'keyDown' and not e['repeat'] then
            print('Key ' .. e.key .. '.')
        elseif e.type == 'character' then
            print('Typed ' .. e.character .. '.')
        elseif e.type == 'mouseDown' and e.modifiers.shift then
            print('Shift click with ' .. e.button .. ' at ' .. e.x .. ', ' .. e.y .. '.')
        elseif e.type == 'touchBegan' then
            for _, touch in ipairs(e.touches) do
                if touch.changed then
                    print('Finger ' .. touch.id .. ' at ' .. touch.x .. ', ' .. touch.y .. '.')
                end
            end
        elseif e.type == 'focusLost' then
            print('The window lost the focus.')
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

The face buttons are named by their place, so `south` is the bottom button whatever its label says.

| Kind | Names |
| --- | --- |
| Face buttons | `south`, `east`, `west`, `north` |
| Shoulders and sticks | `leftShoulder`, `rightShoulder`, `leftStick`, `rightStick` |
| Menu buttons | `back`, `start`, `guide` |
| Directional pad | `dpadUp`, `dpadDown`, `dpadLeft`, `dpadRight` |
| Axes | `leftX`, `leftY`, `rightX`, `rightY`, `leftTrigger`, `rightTrigger` |

## Errors

| Message | Cause |
| --- | --- |
| `unknown value '<name>'` | A key, mouse button, gamepad button or gamepad axis name does not exist. It comes inside a bad argument error. |
| `gamepad index out of range` | A gamepad index is outside `1` to `8`. It comes inside a bad argument error. |
| `expected left or right` | The function `input.gamepadStick` got another side. It comes inside a bad argument error. |
| `Unknown option "<key>".` | An options table has a key that the function does not take, such as the options of `input.newActionMap`, `map.devices`, `input.setGestureSettings`, `input.rumble`, `input.editText`, `input.newCommandBuffer`, its `directions` and commands, and `input.newRecorder`. |
| `The gamepad dead zone must be at least 0 and below 1.` | The function `input.setGamepadDeadzone` got a value out of range. |
| `The scroll speed must be a positive number.` | The function `input.setScrollSpeed` got `0` or less. |
| `The motors of a rumble run from 0 to 1.` | The `low` or `high` of `input.rumble` is out of range. |
| `A rumble lasts a positive number of seconds.` | The `seconds` of `input.rumble` are not positive. |
| `A text field needs its "text" as a string.` | The function `input.editText` got no `text` string. |
| `The selection of a text field must run from 0 to the length of its text, with "selectionStart" not past "selectionEnd".` | The function `input.editText` got a bad selection. |
| `The option "<option>" of a text field must be <names>.` | The `keyboard`, `returnKey` or `autocapitalize` of `input.editText` names an unknown value. |
| `The press threshold must be above 0 and at most 1.` | The function `input.setPressThreshold` or the property `map.pressThreshold` got a value out of range. |
| `Expected the action map to be a JSON object.` | An action map document is not a table with string keys. |
| `Expected an action to be a JSON object.` | An entry of the `actions` list is not a table with string keys. |
| `Unknown key "<key>" in the action map.` | An action map document has a key other than `actions`. |
| `Unknown key "<key>" in an action.` | An action has a key other than `name`, `type`, `buffer` and the binding lists. |
| `The action map needs a list of actions.` | An action map document has no `actions` list. |
| `An action needs a name and a type.` | An action lacks a string `name` or `type`. |
| `The type of an action must be "button", "axis" or "vector", not "<type>".` | An action has another type. |
| `The "<list>" of an action must be a list of bindings.` | A binding list is not a list. |
| `The input binding "<binding>" is invalid. Use a binding such as "key:space", "mouse:left", "button:south", "axis:leftX+" or "stick:left".` | A binding is not a string in one of the forms of [Bindings](#bindings). |
| `The <type> action "<name>" does not read "<list>".` | An action has a binding list that its type never reads. |
| `The vector action "<name>" takes only sticks in "bindings", not "<binding>".` | The `bindings` of a vector action hold something other than a stick. |
| `The action name "<name>" is used by more than one action.` | Two actions of a document have the same name. |
| `The buffer of an action must be a number of seconds from 0.` | The `buffer` of an action is not a number, or is negative. |
| `The "gamepad" of an action map must be an index from 1 to 8, or false for none.` | The `gamepad` of `input.newActionMap` or of `map.devices` is not a gamepad index or `false`. |
| `The list of the buttons of a step holds action names, and entry <n> is not one.` | The list of `map:stepButtons` holds something other than a string. |
| `at most 32 actions fit in the buttons of a step` | The list of `map:stepButtons` is longer than 32. It comes inside a bad argument error. |
| `A command buffer needs from 1 to 32 buttons.` | The `buttons` of `input.newCommandBuffer` are empty or more than 32. |
| `A command lasts a whole number of steps from 1, or false to last until it is consumed.` | A lifetime is `0`, negative, fractional or not a number. |
| `The lifetimes name the button "<button>", which the buffer does not have.` | The `lifetimes` of `input.newCommandBuffer` name a button that is not in `buttons`. |
| `The directions of a command buffer name the "back" and the "forward" buttons.` | The `directions` lack one of the two names. |
| `The command "<name>" names the button "<button>", which the buffer does not have.` | A step or a direction names a button that is not in `buttons`. A buffer without `directions` raises it for the steps `'forward'` and `'back'` too, with `directions` as the name for a direction. |
| `A command needs a name and at least one step.` | A command has no name or no steps. |
| `The command "<name>" has more than 16 steps.` | A command is longer than the history of the buffer. |
| `The command name "<name>" is used by more than one command.` | Two commands, or a command and a button, share a name. |
| `the buttons of a step are an integer of 32 bits` | The mask of `buffer:update`, `buffer:settle` or `recording:write` is negative or does not fit in 32 bits. It comes inside a bad argument error. |
| `The command buffer has no command "<name>".` | The method `buffer:consume` got a name that the buffer does not have. |
| `The command buffer has no button "<name>".` | The method `buffer:held` got a name that the buffer does not have. |
| `An input recording needs from 1 to 32 buttons.` | The `buttons` of `input.newRecorder` are empty or more than 32. |
| `An input recording needs from 1 to 16 players.` | The `players` of `input.newRecorder` are out of range. |
| `The names of an input recording have at most 255 bytes.` | A button name of `input.newRecorder` is longer. |
| `The data of an input recording has at most 1048576 bytes.` | The `data` of `input.newRecorder` is longer. |
| `The input recording has players from 1 to <n>.` | The method `recording:write` or `playback:mask` got another player. |
| `a step hash is an integer of 32 bits` | The hash of `recording:advance` is negative or does not fit in 32 bits. It comes inside a bad argument error. |
| `An input recording holds at most 67108864 steps.` | The method `recording:advance` would close one step too many. |
| `The input recording is malformed: <reason>.` | The bytes of `input.loadRecording` are not a whole recording. |
| `The recording belongs to another app.` | The function `input.loadRecording` got a recording of another app. |
| `The recording was made at <rate> fixed steps a second, and the app runs at <rate>.` | The function `input.loadRecording` got a recording made at another fixed rate. |
| `The playback has steps from 0 to <n>.` | The method `playback:seek` got another step. |
