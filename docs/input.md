# Input

[haylen.input](lua-api/input.md) gives apps two levels of input. The raw level reports keyboards, mice, touch screens, gestures and gamepads directly. The action map on top of it gives names to gameplay actions such as `move` or `attack` and binds each one to any number of keys, mouse buttons, gamepad controls and on-screen touch controls, so gameplay code reads `input.pressed('attack')` and works on every device without extra code. All gameplay input goes through the action map. Menus, tools and text entry read the raw devices when they need them.

This guide explains both levels, the virtual controls that on-screen touch controls drive, device detection, and the actions of the Tiny Island sample. The reference page lists every function, name and error.

## Frame timing

The engine updates device state, gamepads, gestures and the action map once at the start of every frame from the events that arrived before it. Edge states such as `input.keyPressed`, `input.mouseReleased`, `input.gamepadPressed` and the action `input.pressed` stay true for that whole frame, in every scene callback, and are false again on the next frame.

- Read edges in `update`. `fixedUpdate` can run zero or several times in one frame, so a press read there can be missed or seen twice.
- Keys, modifier keys and mouse buttons held when the window loses focus or the app is suspended are released in that frame, and touches and gestures in progress are cancelled without producing taps or swipes, so a player never comes back to a stuck key.
- A key that goes down and up again within one frame still counts as pressed by the actions bound to it.
- Positions are in design coordinates, the space set by `design` in `app.json`. See [Rendering](rendering.md) for how design coordinates map to the screen.

## Raw devices

### Keyboard

`input.keyDown(key)`, `input.keyPressed(key)` and `input.keyReleased(key)` take key names such as `'space'`, `'leftShift'`, `'a'` or `'f3'`, listed under [Key names](lua-api/input.md#key-names). Key repeat from holding a key is not a new press. `input.anyKeyPressed()` suits title screens, and `input.modifiers()` returns the `shift`, `control`, `alt` and `super` state of the latest key event.

Text entry reads `input.text()`, the UTF-8 text typed in this frame, which follows the keyboard layout and input methods of the platform. [UI text fields](ui.md) already handle typing, so most apps never read it.

On Android, hardware keyboards report their keys with modifiers and repeats and type characters, TV remotes report the directional pad as the arrow keys, select as `enter` and play and pause as `pause`, and the keys the app does not know, such as the volume keys, go on to the system. The Back button reaches the app as `escape` only while the app captures back or a text field edits, as [`window.setBackLeavesApp`](lua-api/window.md#windowsetbackleavesappenabled) describes. A view over the app that has the focus, such as a field of a plugin, takes the keys it wants first, and a key whose press went to such a view never reports its release to the app. The other way around, a key whose press the app took always reports its release to the app, also when a view took the focus in between, such as the hidden field of a text field that the press started editing, so no key stays held.

### Mouse

Mouse buttons are `'left'`, `'right'` and `'middle'`. `input.mousePosition()` returns the position in design coordinates, `input.mouseDelta()` the movement of this frame (which keeps working while the mouse is locked with [haylen.window](lua-api/window.md)), `input.mouseScroll()` the wheel movement and `input.mouseInside()` whether the mouse is over the window.

A camera converts screen points in design coordinates, the space of the mouse position, to world positions with `camera:screenToWorld`, which also accounts for its viewport.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')

local camera = graphics2d.newCamera()
local worldX, worldY = camera:screenToWorld(input.mousePosition())
```

### Touch

`input.touches()` returns the fingers on the screen, including fingers that lifted in this frame. Each touch has an `id` that stays the same while the finger is down, its position `x` and `y`, where it landed in `startX` and `startY`, its movement since the last frame in `dx` and `dy`, how long it has been down in `duration` and its `phase`: `'began'`, `'moved'`, `'stationary'`, `'ended'` or `'cancelled'`. Fingers in the `'ended'` or `'cancelled'` phase leave the list on the next frame.

### Gestures

`input.gestures()` returns the gestures recognized in this frame: `'tap'`, `'doubleTap'`, `'longPress'`, `'swipe'` and `'pinch'`, each with a position, the movement of a swipe and the scale of a pinch. While no finger is down, the left mouse button acts as a finger, so gestures also work with a mouse on desktop. `input.setGestureSettings` changes the thresholds, such as `longPressDuration` or `swipeMinDistance`, and `mouse = false` turns the mouse off as a finger.

```lua
for _, gesture in ipairs(input.gestures()) do
    if gesture.type == 'pinch' then
        camera.zoom = {baseZoom * gesture.scale, baseZoom * gesture.scale}
    elseif gesture.type == 'doubleTap' then
        camera.zoom = {1, 1}
    end
end
```

### Gamepads

Up to four gamepads are tracked, with indices from 1 to 4 that default to 1. Buttons use the positional names `south`, `east`, `west` and `north` for the face buttons, so `south` is A on an Xbox pad and Cross on a PlayStation pad, plus `leftShoulder`, `rightShoulder`, `leftStick`, `rightStick`, `back`, `start`, `guide` and the d-pad. Axes are `leftX`, `leftY`, `rightX`, `rightY`, `leftTrigger` and `rightTrigger`.

- `input.gamepadDown`, `input.gamepadPressed` and `input.gamepadReleased` read buttons, and `input.gamepadConnected` and `input.gamepadName` describe the pad.
- `input.gamepadAxis(axis, index)` returns one axis with the dead zone removed and the rest rescaled. Sticks go from -1 to 1 with positive y pointing down, and triggers from 0 to 1.
- `input.gamepadStick(side, index)` returns a whole stick with the dead zone applied to its radius, which keeps diagonals smooth.
- `input.setGamepadDeadzone(value)` sets the dead zone of every axis and stick, from 0 to below 1, 0.2 by default.

Each platform reads gamepads through its own API: GameController on Apple platforms, XInput on Windows, the joystick devices under `/dev/input` on Linux, the input events of GameActivity on Android, which reach the app wherever the focus is inside its window, and the Gamepad API in browsers. Android sends the sticks and the triggers only to the window with the focus, such as a dialog over the app, so they come to rest when the window of the app loses the focus, and the next movement of a stick brings its position back. Browsers only report gamepads that use their `standard` mapping, and they only reveal a gamepad to the page after one of its buttons is pressed. Every platform keeps a gamepad at its index for as long as it stays connected, so a pad that disconnects never moves the others, and a pad that connects takes the lowest free index.

### Events

A scene with an `event(self, e)` callback receives every platform event as a table as soon as it arrives, such as `keyDown`, `mouseDown`, `touchBegan`, `focusLost` or `suspended`. A key remapping screen uses it to take the next key the player presses, whatever the action map says. An app that reacts to leaving the foreground listens to the lifecycle events instead, such as `appBackground`, as the [lifecycle guide](lifecycle.md#app-states) explains.

```lua
function remap:event(event)
    if event.type == 'keyDown' and not event['repeat'] then
        self:bind('key:' .. event.key)
    end
end
```

## The action map

The action map names gameplay actions and binds each one to any number of inputs. Actions update once at the start of every frame, in the order they were defined, so every scene update of that frame sees the same state.

### Action types

| Type | Binding lists | Reports |
| --- | --- | --- |
| `button` | `bindings` | `value` is the strongest binding from 0 to 1. The action is down while the value is at least 0.5. |
| `axis` | `positive` and `negative` | `value` is the strongest positive binding minus the strongest negative binding, from -1 to 1. The action is down while the absolute value is at least 0.5. |
| `vector` | `up`, `down`, `left`, `right` and stick bindings in `bindings` | The direction is right minus left and down minus up, plus every stick binding, limited to a length of 1. `value` is its length, and the action is down while it is at least 0.5. |

Digital bindings count as 0 or 1, and analog bindings such as triggers and stick axes count by how far they are pushed. A vector action bound to keys moves at full speed in straight lines and at the same speed on diagonals, because the sum is limited to a length of 1. Only stick bindings belong in the `bindings` list of a vector action, and a stick bound to a button action counts by how far it is pushed.

### Binding strings

Every binding is a string with a source and a name.

| Binding | Meaning | Example |
| --- | --- | --- |
| `key:<key>` | A keyboard key by its [key name](lua-api/input.md#key-names). | `key:space`, `key:leftShift`, `key:w` |
| `mouse:<button>` | A mouse button. | `mouse:left`, `mouse:right` |
| `button:<button>` | A gamepad button by its [gamepad name](lua-api/input.md#gamepad-names). | `button:south`, `button:dpadUp`, `button:start` |
| `axis:<axis>+` or `axis:<axis>-` | One direction of a gamepad axis, from 0 to 1. | `axis:rightTrigger+`, `axis:leftY-` (the left stick pushed up) |
| `stick:left` or `stick:right` | A whole gamepad stick, for vector actions. | `stick:left` |
| `virtual:<name>` | A virtual button, written by a `touchButton` of the UI or by `input.setVirtualButton`. | `virtual:attack` |
| `virtualStick:<name>` | A virtual stick, written by a `touchStick` of the UI or by `input.setVirtualStick`. | `virtualStick:move` |

Gamepad bindings read every connected gamepad and use the strongest one. `input.setGamepadIndex(index)` makes them read one pad only, which suits local multiplayer, and `input.setGamepadIndex(nil)` reads every pad again.

### Loading actions

An action map document is a table or a JSON file with an `actions` list. Each action has a `name`, a `type` and the binding lists its type uses. `input.loadActions(document)` replaces the whole map and takes either a table or the path of a JSON file in the assets. Unknown keys, unknown types and malformed bindings raise errors that name the problem, such as `The input binding 'key:spcae' is invalid. Use a binding such as key:space, mouse:left, button:south, axis:leftX+ or stick:left.`

```lua
local input = require('haylen.input')

input.loadActions('input/actions.json')

input.loadActions({actions = {
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'throttle', type = 'axis', positive = {'key:up', 'axis:rightTrigger+'}, negative = {'key:down', 'axis:leftTrigger+'}},
    {name = 'move', type = 'vector', left = {'key:a'}, right = {'key:d'}, up = {'key:w'}, down = {'key:s'}, bindings = {'stick:left', 'virtualStick:move'}},
}})
```

`input.saveActions()` returns the current map in the same format, and `input.actionNames()` lists the action names in definition order. Single actions change at runtime too. `input.defineAction(action)` takes one action table in the same format and adds it, or replaces the action with the same name in place, `input.actionDefinition(name)` returns the table of one action or `nil`, and `input.removeAction(name)` and `input.clearActions()` remove actions. A remapping screen reads an action with `actionDefinition`, swaps one binding and defines it again. `input.setPressThreshold(value)` changes the value at which every action counts as down, above 0 and at most 1, 0.5 by default. [haylen.preferences](lua-api/preferences.md) keeps a remapped map across launches: `preferences.capture()` stores it under `input.actions`, and `preferences.apply()` loads it back, so an app loads its default actions first and calls `preferences.apply()` afterwards.

### Reading actions

| Function | Returns |
| --- | --- |
| `input.down(action)` | `true` while the action is down. |
| `input.pressed(action)` | `true` in the frame the action went down. |
| `input.released(action)` | `true` in the frame the action went up. |
| `input.value(action)` | The value described in the table above. |
| `input.vector(action)` | The direction of a vector action as two numbers with a length of at most 1, or `0, 0` for other types. |

An action name that was never defined reads as not down and 0, so optional actions need no checks.

While a scene change holds input back, every action reads as up and 0. A key, button, stick or touch control that is still held when the change ends keeps its actions up until it is released, so the press that opened a menu never reaches the menu as a second press. The navigation actions of the interface, such as `uiCancel`, follow the same rule.

## Virtual buttons and sticks

Virtual buttons and sticks are named inputs that on-screen controls or app code write and the action map reads through `virtual:` and `virtualStick:` bindings.

The usual writers are the `touchButton` and `touchStick` components of [haylen.ui](ui.md#touch-controls). A `touchButton` with `action = 'attack'` holds the virtual button `attack` down while a finger presses it, and a `touchStick` with `action = 'move'` sets the virtual stick `move` to a vector of length 0 to 1. Each control follows its own finger, so a stick and several buttons work at the same time, and a control that stops drawing releases what it held.

Apps can write virtual inputs themselves for custom controls.

| Function | Effect |
| --- | --- |
| `input.setVirtualButton(name, down)` | Presses or releases a virtual button. |
| `input.setVirtualStick(name, x, y)` | Sets a virtual stick, shortening vectors longer than 1. |
| `input.clearVirtual()` | Releases every virtual button and centers every virtual stick. |

Values written during a frame reach the actions at the start of the next frame, which applies to touch controls too. App code keeps a virtual input in the state it wrote until it writes it again, so give custom controls names of their own rather than sharing the names of UI controls.

## Last device and touch-only controls

`input.lastDevice()` returns the kind of device the player used last: `'keyboardMouse'`, `'touch'` or `'gamepad'`. Key presses and mouse button presses select `'keyboardMouse'`, touches select `'touch'`, and a gamepad button going down or an axis leaving the dead zone selects `'gamepad'`. Moving the mouse alone does not change it, and neither do key repeats or a gamepad button or stick that stays held, so a player who holds a trigger and then presses a key gets keyboard prompts. Use it to show matching button prompts.

```lua
local prompts = {keyboardMouse = 'Press Space', touch = 'Tap the screen', gamepad = 'Press A'}

function title:update(dt)
    local device = input.lastDevice()
    if device ~= self.device then
        self.device = device
        self.document:set('prompt', {text = prompts[device]})
    end
end
```

Touch controls use the same rule through their `touchOnly` property. A `touchStick` or `touchButton` with `touchOnly = true` shows only while the last input came from a touch screen. A keyboard or gamepad player never sees the controls, and they appear as soon as the player touches the screen. A hidden control releases its virtual input, so switching devices never leaves a button held.

## UI and gameplay input

The action map reads `mouse:` bindings as released while the interface owns the pointer, so an action bound to `mouse:left` never goes down when the player clicks a HUD button. The raw mouse and keyboard functions still report every click and key. `ui.usingPointer()` returns `true` while the pointer is over something the interface owns, and `ui.usingKeyboard()` returns `true` while a text field has the focus, so code that reads the raw devices checks them first.

```lua
local input = require('haylen.input')
local ui = require('haylen.ui')

function level:update(dt)
    if input.mousePressed('left') and not ui.usingPointer() then
        self:selectAt(input.mousePosition())
    end
end
```

Touch controls count as interface for `ui.usingPointer()`, so an app that checks it never treats a tap on a touch button as a tap on the world. The UI also moves its focus with the arrow keys and the d-pad and activates controls with Space, Enter and the south gamepad button, as the [UI guide](ui.md#pointer-keyboard-and-gamepad) explains. The action map reads the directions at the same time, so a menu with focusable controls usually covers a paused scene, as the Tiny Island pause menu does.

Input the interface answers itself never triggers an action, the same way the action map leaves mouse buttons alone while the interface owns the pointer. At the end of every frame the interface captures, for the next frame, the keys and gamepad buttons of the presses it will answer:

- the `uiCancel` bindings, Escape, the east button and the Menu button of a TV remote by default, while a popup, a combo list, a menu or a dialog is open, a control is being edited, an item is carried or the focus is inside a closable window,
- the `uiAccept` bindings, Enter, Space and the south button by default, while a control has the focus,
- every key while a text field edits, on Mac Catalyst too, where a hardware keyboard still reports keys while the native field edits,
- every key and gamepad button while a `keyCapture` listens.

A key or button pressed while it is captured belongs to the interface until it is released, so an action bound to it reads as up for that whole press, even after the popup it closed is gone. `input.keyCaptured(key)` and `input.gamepadButtonCaptured(button)` tell whether a press belongs to the interface, for apps that also read the raw keyboard, whose functions keep reporting every key.

A screen therefore goes back with the `onCancel` handler of its document root instead of an action of its own. `uiCancel` reaches the root only when no popup, combo list, dialog or edit answers it first, so the Escape that closes a combo list never leaves the screen, and the next Escape does.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Settings = {}
Settings.__index = Settings

function Settings:enter()
    self.document = ui.mount(ui.column{
        onCancel = function()
            scene.pop()
        end,
        ui.combo{id = 'quality', autofocus = true, items = {{id = 'low', text = 'Low'}, {id = 'high', text = 'High'}}, selected = 'high'},
    })
end

function Settings:exit()
    self.document:unmount()
end

scene.push(setmetatable({}, Settings))
```

## The Tiny Island actions

Tiny Island defines five actions in `samples/games/tiny-island/content/input/actions.json` and loads them in `source/main.lua` before the first scene.

```json
{
    "actions": [
        {
            "name": "move",
            "type": "vector",
            "up": ["key:w", "key:up", "button:dpadUp"],
            "down": ["key:s", "key:down", "button:dpadDown"],
            "left": ["key:a", "key:left", "button:dpadLeft"],
            "right": ["key:d", "key:right", "button:dpadRight"],
            "bindings": ["stick:left", "virtualStick:move"]
        },
        {"name": "attack", "type": "button", "bindings": ["key:space", "mouse:left", "button:south", "virtual:attack"]},
        {"name": "special", "type": "button", "bindings": ["key:leftShift", "mouse:right", "button:west", "virtual:special"]},
        {"name": "interact", "type": "button", "bindings": ["key:e", "button:east", "virtual:interact"]},
        {"name": "pause", "type": "button", "bindings": ["key:escape", "button:start"]}
    ]
}
```

| Action | Keyboard and mouse | Gamepad | Touch |
| --- | --- | --- | --- |
| `move` | WASD or the arrow keys | Left stick or d-pad | Floating `touchStick` |
| `attack` | Space or left click | South (A) | `touchButton` |
| `special` | Left Shift or right click | West (X) | `touchButton` |
| `interact` | E | East (B) | `touchButton` |
| `pause` | Escape | Start | The pause button of the HUD |

`move` combines three kinds of input in one action: the digital directions of keys and the d-pad, the left stick and the virtual stick of the HUD. The player entity reads it once and never asks which device moved it.

```lua
local mx, my = input.vector('move')
self.guarding = self.class.special == 'guard' and input.down('special')
if input.pressed('attack') and self.cooldown <= 0 then
    self:attack()
elseif self.class.special ~= 'guard' and input.pressed('special') and self.specialCooldown <= 0 then
    self:special()
end
```

The same action can mean different things per class. The warrior guards while `special` is held, so the game reads `input.down('special')`, and the other classes trigger their special once per press with `input.pressed('special')`. `interact` is read with `input.down` to double the reach for feeding the fire while it is held. The gameplay and pause scenes read `input.pressed('pause')`, so Escape and Start open and close the pause menu during a run. The settings and class selection screens go back with the `onCancel` handler of their document root, which the `uiCancel` action of the interface triggers with Escape and the east button, as [UI and gameplay input](#ui-and-gameplay-input) describes.

The HUD in `samples/games/tiny-island/source/ui/hud.lua` drives the virtual inputs with a floating `touchStick` for `move` and `touchButton` controls for `attack`, `special` and `interact`, all with `touchOnly = true`. The pause button of the HUD is a regular icon button that opens the pause menu directly. When the pause menu covers the run, the gameplay scene hides the HUD and calls `input.clearVirtual()`, so nothing stays pressed while the game is paused.

## From C++

The raw state lives in `haylen::input::Input` (`haylen/input/Input.hpp`), reached with `engine.getInput()`. `haylen::input::ActionMap` (`haylen/input/ActionMap.hpp`), reached with `engine.getActions()`, loads the same JSON documents and can also define and remove actions one by one in code with `ActionMap::Action` and `ActionMap::Binding`, and the UI plugin hands it the keys and buttons it captures with `ActionMap::setCapture`. `engine.getVirtualInput()` returns the `haylen::input::VirtualInput` that touch controls write, and `engine.getGestures()` returns the `haylen::input::GestureRecognizer`. `haylen::input::Controls` (`haylen/input/Controls.hpp`) converts keys, mouse buttons, gamepad buttons and gamepad axes to and from the names that bindings and Lua use. See [Architecture](architecture.md) for how the frame updates them and [Lua](lua.md) for the scripting side.
