# haylen.input

Keyboard, mouse, touch, gesture and gamepad state, and the action map that turns all of them into named gameplay actions. Gameplay reads actions such as `jump` or `move` so that every device and on-screen control works without extra code, and menus or tools read the raw devices when they need them.

```lua
local input = require('haylen.input')
```

## Frame timing

The engine updates device state and the action map once at the start of every frame from the events that arrived before it. States such as `keyPressed`, `mouseReleased`, `gamepadPressed` and the action `pressed` stay `true` for that whole frame, in every scene callback, and are `false` again on the next frame. Read edges in `update`, because `fixedUpdate` can run zero or several times in one frame. Positions are in design coordinates, the coordinate space set by `design` in `app.json`.

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

Returns `true` while the current press of a key belongs to the interface of [`haylen.ui`](ui.md), such as the Escape that closes a combo list or a key typed into a text field, as the [input guide](../input.md#ui-and-gameplay-input) lists. The press stays with the interface until the key is released, and meanwhile `key:` bindings of the [action map](#action-map) read it as up. The raw keyboard functions keep reporting the key.

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

Returns how far the mouse moved in this frame, in design units, as two numbers. It keeps reporting movement while the mouse is locked with `window.setMouseLocked()`.

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

Returns the wheel movement of this frame as two numbers, horizontal and vertical, in the wheel units the platform reports.

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

Returns `true` while the mouse is inside the window.

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

Returns `true` while the interface of [`haylen.ui`](ui.md) owns the pointer, which is what `ui.usingPointer()` reported at the end of the previous frame. Meanwhile `mouse:` bindings of the [action map](#action-map) read as released, so a click on a menu button never swings a sword bound to `mouse:left`. The raw mouse functions above keep reporting the buttons, and virtual buttons of touch controls keep driving their actions.

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
| `dx`, `dy` | number | The movement of a swipe, from where the finger landed to where it lifted. Other gestures report 0. |
| `scale` | number | The spread of a pinch divided by the spread when the second finger landed. Other gestures report 1. |

A tap fires when a finger lifts quickly without moving far. The second tap of a double tap reports both a `'tap'` and a `'doubleTap'` in the same frame. A long press fires once while the finger is still down, and that finger ends without a tap. A swipe fires when a finger lifts after moving far enough quickly enough. A pinch fires every frame in which one of exactly two fingers moves, and fingers that were part of a pinch never end as taps or swipes. A finger the system cancels, and every finger or mouse press still down when the window loses focus or the app is suspended, ends without a gesture.

```lua
local input = require('haylen.input')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
        self.baseZoom = 1
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
            end
        end
        if #input.touches() < 2 then
            self.baseZoom = self.camera.zoom.x
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

Up to four gamepads are tracked. Gamepad indices start at 1, default to 1 and raise a bad argument error with `gamepad index out of range` outside 1 to 4. Buttons and axes use the names listed in [Gamepad names](#gamepad-names).

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

Returns `true` while the current press of a gamepad button belongs to the interface, such as the east button that closes a popup, and `button:` bindings read it as up meanwhile. The argument `index` picks the gamepad from 1 to 4 and defaults to 1.

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

Returns `true` while the current lean of a gamepad axis past the press threshold belongs to the interface, such as the left stick that moves the focus between the controls of a menu, and `axis:` and `stick:` bindings of that axis read it as 0 meanwhile. The argument `axis` is one of `'leftX'`, `'leftY'`, `'rightX'`, `'rightY'`, `'leftTrigger'` or `'rightTrigger'`, and `index` picks the gamepad from 1 to 4 and defaults to 1.

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

## Devices

### input.lastDevice()

Returns the kind of device the player used last: `'keyboardMouse'`, `'touch'` or `'gamepad'`. Key presses and mouse button presses select `'keyboardMouse'`, touches select `'touch'`, and a gamepad button going down or an axis leaving the dead zone selects `'gamepad'`. Only the press counts, so key repeats and a gamepad button or stick that stays held never take over from a device the player used afterwards. Use it to show the matching button prompts or on-screen controls.

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

Gamepad bindings read every connected gamepad and use the strongest one, unless `input.setGamepadIndex()` picks one gamepad. Mouse bindings read as released while the interface owns the pointer, as [`input.pointerCaptured()`](#inputpointercaptured) explains, and key and gamepad button bindings read as up for a press the interface answers itself, as [`input.keyCaptured()`](#inputkeycapturedkey) explains. While a scene change holds input back, as the `blockInput` option of [`haylen.scene`](scene.md) sets, every action reads as up and 0, and an action whose bindings are still held when the change ends stays up until they are released. Action names that were never defined read as not down and 0.

An action map document is a table or a JSON file with an `actions` list. Each action has `name`, `type` and the binding lists its type uses.

```json
{
  "actions": [
    {"name": "jump", "type": "button", "bindings": ["key:space", "key:w", "button:south", "virtual:jump"]},
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

### input.setGamepadIndex(index)

Makes gamepad bindings read only the gamepad at `index`, from 1 to 4, which suits local multiplayer. Passing `nil` reads every gamepad again.

```lua
local input = require('haylen.input')

input.setGamepadIndex(2)
input.setGamepadIndex(nil)
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
| `'mouseScroll'` | The fields `scrollX` and `scrollY` are the wheel movement. |
| `'touchBegan'`, `'touchMoved'`, `'touchEnded'`, `'touchCancelled'` | The field `touches` is a list of every finger with `id`, `x`, `y` and `changed`, which is `true` for the fingers this event is about. |
| `'textEdited'` | The field `field` is the id of the text field of the UI and `text` what its native field holds. |
| `'textAction'` | The field `field` is the id of the text field and `action` is `'submit'`, `'next'`, `'cancel'` or `'dismissed'`. |
| `'keyboardChanged'` | The field `frame` is the rectangle the on-screen keyboard covers, empty while it is hidden. |
| `'networkChanged'` | The field `online` is whether the device has a network. |
| `'mouseEnter'`, `'mouseLeave'`, `'resized'`, `'suspended'`, `'resumed'`, `'focusGained'`, `'focusLost'`, `'quitRequested'`, `'lowMemory'`, `'interruptionBegan'`, `'interruptionEnded'` | No other fields. |

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
| `gamepad index out of range` | A gamepad index is outside 1 to 4. It comes inside a bad argument error. |
| `expected left or right` | The function `input.gamepadStick()` received another side. It comes inside a bad argument error. |
| `Unknown option "<key>".` | The function `input.setGestureSettings()` received an unknown field. |
| `The gamepad dead zone must be at least 0 and below 1.` | The function `input.setGamepadDeadzone()` received a value outside that range. |
| `The press threshold must be above 0 and at most 1.` | The function `input.setPressThreshold()` received a value outside that range. |
| `Expected the action map to be a JSON object.` | An action map document is not a table with string keys. |
| `Expected an action to be a JSON object.` | An entry of the `actions` list is not a table with string keys. |
| `Unknown key "<key>" in the action map.` | An action map document has a key other than `actions`. |
| `Unknown key "<key>" in an action.` | An action has a key other than `name`, `type` and the binding lists. |
| `The action map needs a list of actions.` | An action map document has no `actions` list. |
| `An action needs a name and a type.` | An action lacks a string `name` or `type`. |
| `The type of an action must be "button", "axis" or "vector", not "<type>".` | An action type is not `button`, `axis` or `vector`. |
| `The "<list>" of an action must be a list of bindings.` | A binding list is not a list. |
| `The input binding "<binding>" is invalid. Use a binding such as "key:space", "mouse:left", "button:south", "axis:leftX+" or "stick:left".` | A binding is not a string in one of the formats in [Action map](#action-map). |
| `The <type> action "<name>" does not read "<list>".` | An action has a binding list its type never reads. |
| `The vector action "<name>" takes only sticks in "bindings", not "<binding>".` | The `bindings` of a vector action hold something other than a stick. |
| `The action name "<name>" is used by more than one action.` | Two actions of an action map document have the same name. |
