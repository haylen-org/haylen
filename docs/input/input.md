# 🎮 Input

This guide explains how a Haylen app reads its players. It covers the action map that gameplay reads and the timing of presses. It covers every device the engine supports: keyboards, mice, touch screens, gamepads and TV remotes. It also covers on-screen controls, how the game and the UI share the same devices, local multiplayer, command buffers and recorded replays. Read it before you write the controls of a game or a screen that players drive with a gamepad or a remote. The [`haylen.input` reference](../lua-api/input.md) lists every function, binding, event and error.

## Two levels of input

The module [`haylen.input`](../lua-api/input.md) gives an app two levels of input.

- The **devices** level reports each device as it is: which key is down, where the mouse is, which fingers touch the screen, how far a stick leans.
- The **action map** sits on top. It gives names to gameplay actions, such as `move` or `attack`, and binds each one to any number of keys, mouse buttons, gamepad controls and on-screen controls.

All gameplay input goes through the action map. Gameplay code reads `input.pressed('attack')` and never asks which device pressed it, so it works with every device and every platform without extra code. The UI reads the devices through its own focus navigation, which works with every device too. Menus, tools and text entry read the devices directly when they need them.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

-- One action for every device: keys, a gamepad, a TV remote and an on-screen stick.
input.loadActions({actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'attack', type = 'button', bindings = {'key:space', 'mouse:left', 'button:south', 'virtual:attack'}},
}})

scene.push({
    x = 0,
    y = 0,
    update = function(self, dt)
        local dx, dy = input.vector('move')
        self.x = self.x + dx * 200 * dt
        self.y = self.y + dy * 200 * dt
        if input.pressed('attack') then
            print('Attack at ' .. self.x .. ', ' .. self.y .. '.')
        end
    end,
})
```

## Frame timing

The engine reads the events that arrived since the last frame once, at the start of every frame. Then it updates the devices, the gamepads, the gestures and the action map. A frame edge, such as `input.keyPressed`, `input.mouseReleased`, `input.gamepadPressed` or the action state `input.pressed`, stays true for that whole frame, in every scene callback. On the next frame it is false again.

- Read frame edges in `update`. The hook `fixedUpdate` can run zero times or several times in one frame, so it could miss an edge or see it twice.
- In `fixedUpdate`, read actions with `input.stepPressed`, `input.stepReleased` and `input.stepDown`. At the start of every fixed step, the engine moves the edges that arrived since the step before into the state of the step. So each press reaches exactly one step, the first one after it, whatever the frame rate. A tap shorter than a step still holds its action for one step. What a step sees never depends on the frame rate, which simulations, replays and games exact to the step need.
- The function `input.consumePress(action)` returns `true` once per press, in `update` or in `fixedUpdate`. The `buffer` of an action keeps a press ready for that many seconds of fixed steps. This is the usual jump buffer: a jump pressed just before landing still jumps.
- When the window loses the focus or the app is suspended, the keys, modifier keys and mouse buttons that are held are released in that frame. Touches and gestures in progress are cancelled, without taps or swipes. So a player never comes back to a stuck key.
- A key that goes down and up again within one frame still counts as a press for the actions bound to it.
- Positions are in design coordinates, the space that `design` of `app.json` sets. The [rendering guide](../graphics/rendering.md) shows how design coordinates map to the screen.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south'}, buffer = 0.12},
    {name = 'move', type = 'vector', left = {'key:left'}, right = {'key:right'}, bindings = {'stick:left'}},
}})

scene.push({
    x = 0,
    velocity = 0,
    grounded = true,
    -- The simulation runs in fixed steps, so it reads the step state of the actions.
    fixedUpdate = function(self, step)
        local dx = input.vector('move')
        self.x = self.x + dx * 240 * step
        if self.grounded and input.consumePress('jump') then
            self.velocity = -520
            self.grounded = false
        end
        if input.stepReleased('jump') and self.velocity < 0 then
            self.velocity = self.velocity * 0.5
        end
    end,
})
```

## The action map

The action map names the gameplay actions and binds each one to any number of inputs. The engine updates the actions once at the start of every frame, in the order they were defined, so every scene of that frame sees the same state.

### Kinds of actions

An action has one of three types. Each type mixes its bindings into a value in its own way.

- A **button** action, such as `jump`, takes the strongest of its bindings, from `0` to `1`. It is down while that value is at least the press threshold, `0.5` by default.
- An **axis** action, such as `throttle`, has `positive` and `negative` bindings. Its value is the strongest positive binding minus the strongest negative binding, from `-1` to `1`.
- A **vector** action, such as `move`, has `up`, `down`, `left` and `right` bindings, and sticks in `bindings`. Its direction is right minus left and down minus up, plus every stick, cut to a length of `1`.

Digital bindings, such as keys and buttons, count as `0` or `1`. Analog bindings, such as triggers and stick axes, count by how far they are pushed. A vector action bound to keys moves at full speed in straight lines and at the same speed on diagonals, because the sum is cut to a length of `1`. A stick bound to a button action counts by how far it leans. The reference lists the [action types](../lua-api/input.md#action-types) with their binding lists.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'throttle', type = 'axis', positive = {'key:up', 'axis:rightTrigger+'}, negative = {'key:down', 'axis:leftTrigger+'}},
    {name = 'move', type = 'vector', left = {'key:a'}, right = {'key:d'}, up = {'key:w'}, down = {'key:s'}, bindings = {'stick:left', 'virtualStick:move'}},
}})

scene.push({
    update = function(self, dt)
        print(input.down('jump'), input.value('throttle'), input.vector('move'))
    end,
})
```

### Bindings

A binding is a string with a source and a name, such as `key:space`, `mouse:left`, `button:south`, `axis:rightTrigger+`, `stick:left`, `virtual:attack` or `virtualStick:move`. Gamepad buttons have names by their place, so `button:south` is the bottom face button whatever its label says. Gamepad bindings read every connected gamepad and take the strongest, unless the action map of a [player](#local-multiplayer) picks one gamepad or none. The reference lists every [binding form](../lua-api/input.md#bindings), every [key name](../lua-api/input.md#key-names) and every [gamepad name](../lua-api/input.md#gamepad-names).

Many keyboards cannot report every set of keys held at once. A third or fourth key pressed together with some others never arrives, which players call ghosting. So keep the keys that one player holds together apart in the default bindings, such as WASD with the space bar, or the arrows with the right Control key. Let players pick keys their keyboard reports, as [remapping](#remapping) shows. The test `INP-001` of the test project shows every key that the keyboard reports, which tells whether a set of keys reaches the app.

### Loading actions

An action map document is a table, or a JSON file, with an `actions` list. Each action has a `name`, a `type` and the binding lists of its type. Keep the actions of an app in a file of its `content` folder, and load it with `input.loadActions(path)` before the first scene. The function also takes a table. It replaces the whole map, and it changes nothing when the document is not valid. Mistakes raise errors that name the problem, such as `The input binding "key:spcae" is invalid. Use a binding such as "key:space", "mouse:left", "button:south", "axis:leftX+" or "stick:left".`.

```json
{
    "actions": [
        {"name": "jump", "type": "button", "bindings": ["key:space", "button:south", "virtual:jump"], "buffer": 0.12},
        {"name": "move", "type": "vector", "left": ["key:a", "key:left"], "right": ["key:d", "key:right"], "bindings": ["stick:left", "virtualStick:move"]}
    ]
}
```

```lua
local input = require('haylen.input')

input.loadActions('input/actions.json')
print(table.concat(input.actionNames(), ', '))
```

Single actions change at run time too. The function `input.defineAction(action)` adds an action, or replaces the action with the same name in its place. The functions `input.removeAction(name)` and `input.clearActions()` remove actions, and `input.saveActions()` returns the whole map as a document. The function `input.setPressThreshold(value)` changes the value at which every action counts as down.

### Reading actions

Read actions in `update` with five functions:

- `input.down(action)` is `true` while the action is down.
- `input.pressed(action)` is `true` in the frame the action went down.
- `input.released(action)` is `true` in the frame the action went up.
- `input.value(action)` is the value of the action.
- `input.vector(action)` is the direction of a vector action, as two numbers.

An action name that was never defined reads as up and `0`. So code can read optional actions without checks.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    charge = 0,
    update = function(self, dt)
        if input.down('fire') then
            self.charge = math.min(1, self.charge + dt)
        end
        if input.released('fire') then
            print('Shot with a charge of ' .. self.charge .. '.')
            self.charge = 0
        end
    end,
})
```

### Remapping

A controls screen lets the player pick their own bindings. Read an action with `input.actionDefinition(name)`, swap one binding and define it again with `input.defineAction`. The [`keyCapture`](../lua-api/ui.md#uikeycaptureproperties) control of the UI listens for the next key, mouse button, gamepad button or axis and reports it as a binding string. The module [`haylen.preferences`](../lua-api/preferences.md) keeps a remapped map across launches. The function `preferences.capture()` stores it under `input.actions`, and `preferences.apply()` loads it back. So an app loads its default actions first, and calls `preferences.apply()` afterwards.

```lua
local input = require('haylen.input')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

input.loadActions('input/actions.json')
preferences.apply()

ui.mount(ui.settingsRow{
    label = 'Jump',
    ui.keyCapture{value = input.actionDefinition('jump').bindings[1], onChange = function(event)
        local jump = input.actionDefinition('jump')
        jump.bindings[1] = event.value
        input.defineAction(jump)
        preferences.capture()
        preferences.save()
    end},
})
```

### When actions read as up

Some presses never reach the actions, so a press that meant something else never fires an action by mistake.

- While a scene change holds input back, every action reads as up and `0`. A key, button, stick or touch control still held when the change ends keeps its actions up until it is released. So the press that opened a menu never reaches the menu as a second press. The navigation actions of the UI, such as `uiCancel`, follow the same rule.
- A press that the UI takes or answers itself reads as up for the whole press, as [UI and gameplay input](#ui-and-gameplay-input) explains.

## Every device

### Keyboard

The functions `input.keyDown(key)`, `input.keyPressed(key)` and `input.keyReleased(key)` take key names such as `'space'`, `'leftShift'`, `'a'` or `'f3'`. The repeats of a held key are not new presses. The function `input.anyKeyPressed()` suits title screens. The function `input.modifiers()` returns the `shift`, `control`, `alt` and `super` state of the latest key event.

Text is not keys. The function `input.text()` returns the UTF-8 text typed in this frame, which follows the keyboard layout and the input methods of the platform. The [text fields of the UI](../ui/ui.md#text-entry) already handle typing, so most apps never read it. A GUI that the app draws itself edits text with the native text input through [`input.editText`](../lua-api/input.md#text-fields-of-the-app).

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if input.modifiers().control and input.keyPressed('z') then
            print('Undo.')
        elseif input.keyDown('leftShift') then
            print('Running.')
        end
    end,
})
```

Each platform delivers keys in its own way. The engine keeps one rule everywhere: a key whose press reached the app always reports its release to the app. So no key stays held.

- On Android, hardware keyboards report their keys with modifiers and repeats, and type characters. Keys that the app does not know, such as the volume keys, go on to the system. The Back button reaches the app as `escape` only while the app keeps back, as [TV remotes](#tv-remotes) explains, or while a text field edits. A view over the app that has the focus, such as a field of a plugin, takes the keys it wants first. A key whose press went to such a view never reports its release to the app. A key whose press the app took reports its release to the app, even when a view took the focus in between. That view may be the hidden field of a text field that the press started editing.
- On macOS, the hidden field of a text field passes the release on to the view of the app.
- iOS, iPadOS, tvOS and Mac Catalyst read a hardware keyboard through `GCKeyboard`. It reports every key by its place on the keyboard, with its release, whatever view has the focus, and without repeats. Typed characters and `input.text()` come from the native field of a text field there. So an app reads them while a text field, or the plain keyboard of `window.setKeyboardVisible()`, edits.
- The web page hands a release to the app whenever its press reached the app.

### Mouse

Mouse buttons are `'left'`, `'right'` and `'middle'`. The function `input.mousePosition()` returns the position of the mouse in design coordinates. The function `input.mouseDelta()` returns the movement of this frame, which keeps working while [`haylen.window`](../lua-api/window.md) locks the mouse. The function `input.mouseScroll()` returns the scroll of the wheel or the trackpad, in steps of a wheel. The function `input.setScrollSpeed()` and the key `input.scrollSpeed` of `app.json` make every scroll faster or slower. The function `input.mouseInside()` tells whether the mouse is over the window.

A camera turns a point in design coordinates, the space of the mouse, into a place in the world with `camera:screenToWorld`. It counts the viewport of the camera too.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
    end,
    update = function(self, dt)
        if input.mousePressed('left') then
            local worldX, worldY = self.camera:screenToWorld(input.mousePosition())
            print('Clicked the world at ' .. worldX .. ', ' .. worldY .. '.')
        end
        local _, scroll = input.mouseScroll()
        if scroll ~= 0 then
            self.camera:zoomAt(1 + scroll * 0.1, input.mousePosition())
        end
    end,
})
```

### Touch

The function `input.touches()` returns the fingers on the screen, with the fingers that lifted in this frame. Each finger has an `id` that stays the same while it is down. It also has its position `x` and `y`, and where it landed in `startX` and `startY`. The fields `dx` and `dy` hold its movement since the last frame, and `duration` how long it has been down. Its `phase` is `'began'`, `'moved'`, `'stationary'`, `'ended'` or `'cancelled'`. Fingers in the phase `'ended'` or `'cancelled'` leave the list on the next frame. The function `input.findTouch(id)` follows one finger.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

-- Drags an object with the first finger that lands on the screen.
scene.push({
    x = 400,
    y = 300,
    update = function(self, dt)
        local finger = self.finger and input.findTouch(self.finger)
        if not finger then
            local first = input.touches()[1]
            self.finger = first and first.phase == 'began' and first.id or nil
            return
        end
        self.x, self.y = self.x + finger.dx, self.y + finger.dy
        if finger.phase == 'ended' or finger.phase == 'cancelled' then
            self.finger = nil
        end
    end,
})
```

An app can treat one device as the other:

- With `input.setMouseAsTouch(true)`, or `"input": {"mouseAsTouch": true}` in `app.json`, the left mouse button becomes a finger for the whole engine. So touch controls, gestures and touch code run on a desktop as on a phone.
- With `input.setTouchAsMouse(true)`, or `"touchAsMouse": true`, the first finger down also drives the mouse. So code that reads `input.mousePressed` and `input.mousePosition` runs on a touch screen, while the touches keep working.

### Gestures

The function `input.gestures()` returns the gestures recognized in this frame: `'tap'`, `'doubleTap'`, `'longPress'`, `'swipe'` and `'pinch'`. Each gesture has a position. A swipe has its movement. A pinch has its scale, its rotation and the movement of its center, which zoom, turn and pan with two fingers. While no finger is down, the left mouse button acts as a finger, so gestures also work with a mouse. The function `input.setGestureSettings` changes the limits, such as `longPressDuration` or `swipeMinDistance`, and `mouse = false` stops the mouse from acting as a finger.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    enter = function(self)
        self.camera = graphics2d.newCamera()
        self.baseZoom = 1
    end,
    update = function(self, dt)
        for _, gesture in ipairs(input.gestures()) do
            if gesture.type == 'pinch' then
                self.camera.zoom = {self.baseZoom * gesture.scale, self.baseZoom * gesture.scale}
            elseif gesture.type == 'doubleTap' then
                self.camera.zoom = {1, 1}
            end
        end
        if #input.touches() < 2 then
            self.baseZoom = self.camera.zoom.x
        end
    end,
})
```

### Gamepads

The engine tracks up to eight gamepads, with indices from `1` to `8`. Functions take `1` when the index is left out. The face buttons have names by their place: `south`, `east`, `west` and `north`. So `south` is the bottom face button whatever its label says. The other buttons are `leftShoulder`, `rightShoulder`, `leftStick`, `rightStick`, `back`, `start`, `guide` and the directional pad. The axes are `leftX`, `leftY`, `rightX`, `rightY`, `leftTrigger` and `rightTrigger`.

- The functions `input.gamepadDown`, `input.gamepadPressed` and `input.gamepadReleased` read buttons. The functions `input.gamepadConnected` and `input.gamepadName` describe the gamepad.
- The function `input.gamepadAxis(axis, index)` returns one axis with the dead zone removed and the rest stretched to the full range. Sticks go from `-1` to `1`, with positive y pointing down. Triggers go from `0` to `1`.
- The function `input.gamepadStick(side, index)` returns a whole stick, with the dead zone applied to its distance from the center, which keeps diagonals smooth.
- The dead zone is the small lean near the center that reads as `0`, since sticks rarely rest exactly at the center. The function `input.setGamepadDeadzone(value)` sets it for every axis and stick, from `0` to below `1`, and it is `0.2` by default.
- The function `input.gamepadLabels(index)` tells how the gamepad labels its face buttons, so prompts show the label the player sees while bindings keep the names by place.
- The function `input.rumble(index, {low, high, seconds})` runs the low and the high frequency motors of a gamepad. The engine stops every gamepad when the app loses the focus or stops. The [reference](../lua-api/input.md#inputrumbleindex-options) lists how each platform rumbles.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        if not input.gamepadConnected(1) then
            return
        end
        local x, y = input.gamepadStick('left')
        self.aim = {x = x, y = y}
        if input.gamepadPressed('rightShoulder') then
            input.rumble(1, {low = 0.6, high = 0.2, seconds = 0.15})
        end
    end,
})
```

Each platform reads gamepads through its own API:

- Apple platforms use the GameController framework.
- Windows uses XInput.
- Linux reads the joystick devices under `/dev/input`, in the layout of the common driver of controllers with lettered face buttons. Controllers whose drivers number their buttons another way may swap the west and north buttons there.
- Android reads the input events of GameActivity, which reach the app wherever the focus is inside its window. Android sends the sticks and the triggers only to the window with the focus, such as a dialog over the app. So they come to rest when the window of the app loses the focus, and the next movement of a stick brings its position back.
- Browsers use the Gamepad API. The engine reads the gamepads that a browser gives its `standard` mapping and leaves the others out, since their buttons have no known places. Browsers reveal a gamepad to the page only after one of its buttons is pressed.

Windows reports at most four gamepads, the limit of XInput. Browsers report as many as they expose, usually four. The other platforms fill all eight slots. Every platform keeps a gamepad at its index for as long as it stays connected. So a gamepad that disconnects never moves the others, and a gamepad that connects takes the lowest free index.

### TV remotes

A TV remote is a small set of directions, a select button, a back button and a play and pause button. The engine turns them into keys and gamepad buttons, so the same actions and the same UI navigation work on a TV.

- The Siri Remote of the Apple TV is read as a gamepad. A swipe on its touch surface moves the left stick and, past half its travel, presses the directional pad. A click of the surface presses `south`, and play and pause presses `west`. Through UIKit, the remote also presses the arrow keys, `enter` and the `pause` key.
- The remote of an Android TV presses the arrow keys with its directional pad, `enter` with select, and the `pause` key with play and pause.
- The Menu button of the Apple TV remote and the Back button of Android are the back button of the platform. On the root screen of an app they leave the app, as Apple and Google ask. Elsewhere, [`window.setBackLeavesApp(false)`](../lua-api/window.md#windowsetbackleavesappenabled) keeps them in the app, where they reach it as the `escape` key, which the UI reads as `uiCancel`. An open popup or dialog always keeps them. On the Apple TV, the Escape key of a keyboard is the Menu button of the remote, so it goes back the way the remote does.

On a TV there is no pointer, so players drive every screen with the focus of the UI, as [the play area](#the-play-area) shows.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local window = require('haylen.window')

-- The directional pad and the swipes of a remote steer, and select or a click jumps.
input.loadActions({actions = {
    {name = 'move', type = 'vector', left = {'key:left', 'button:dpadLeft'}, right = {'key:right', 'button:dpadRight'}, up = {'key:up', 'button:dpadUp'}, down = {'key:down', 'button:dpadDown'}, bindings = {'stick:left'}},
    {name = 'jump', type = 'button', bindings = {'key:enter', 'button:south'}},
    {name = 'pause', type = 'button', bindings = {'key:pause', 'key:escape', 'button:start'}},
}})

scene.push({
    enter = function(self)
        -- The back button of the remote pauses the run instead of leaving the app.
        window.setBackLeavesApp(false)
    end,
    update = function(self, dt)
        if input.pressed('pause') then
            print('Open the pause menu.')
        end
    end,
    exit = function(self)
        window.setBackLeavesApp(true)
    end,
})
```

### Platform events

A scene with an `event(self, e)` hook receives every platform event as a table as soon as it arrives, such as `keyDown`, `mouseDown`, `touchBegan`, `focusLost` or `suspended`. A screen that remaps keys uses it to take the next key the player presses, whatever the action map says. To react when the app leaves the foreground, listen to the lifecycle events instead, such as `appBackground`, as the [lifecycle guide](../core/lifecycle.md#app-states) explains. The reference lists [every event](../lua-api/input.md#events) with its fields.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

-- Waits for the next key and binds it to the jump.
scene.push({
    event = function(self, e)
        if e.type == 'keyDown' and not e['repeat'] then
            input.defineAction({name = 'jump', type = 'button', bindings = {'key:' .. e.key, 'button:south'}})
            scene.pop()
        end
    end,
})
```

## Touch controls and virtual inputs

A phone has no keys, so a touch game draws its controls on the screen. These controls write virtual inputs: named buttons and sticks that the action map reads through `virtual:` and `virtualStick:` bindings.

The usual writers are the [`touchButton` and `touchStick` controls](../ui/ui.md#touch-controls) of `haylen.ui`. A `touchButton` with `action = 'attack'` holds the virtual button `attack` down while a finger presses it. A `touchStick` with `action = 'move'` sets the virtual stick `move` to a vector of length `0` to `1`. Each control follows its own finger, so a stick and several buttons work at the same time. A control that stops drawing releases what it held.

A control with `touchOnly = true` shows only while the last input came from a touch screen. A keyboard or gamepad player never sees it, and it appears as soon as the player touches the screen. A hidden control releases its virtual input, so switching devices never leaves a button held.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.loadActions({actions = {
    {name = 'move', type = 'vector', left = {'key:a'}, right = {'key:d'}, up = {'key:w'}, down = {'key:s'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'attack', type = 'button', bindings = {'key:space', 'button:south', 'virtual:attack'}},
}})

scene.push({
    enter = function(self)
        self.gui = ui.mount(ui.row{
            align = 'end',
            ui.touchStick{action = 'move', mode = 'floating', touchOnly = true, width = 600, height = 400},
            ui.spacer{grow = 1},
            ui.touchButton{action = 'attack', text = 'A', size = 160, touchOnly = true},
        }, {owner = self})
    end,
    update = function(self, dt)
        local dx, dy = input.vector('move')
        if input.pressed('attack') then
            print('Attack toward ' .. dx .. ', ' .. dy .. '.')
        end
    end,
})
```

App code can write virtual inputs itself, for controls of its own, with `input.setVirtualButton(name, down)`, `input.setVirtualStick(name, x, y)` and `input.clearVirtual()`. Values written during a frame reach the actions at the start of the next frame, and that holds for touch controls too. A virtual input that app code wrote stays as it was until the code writes it again. So give the controls of the app names of their own, apart from the names of UI controls.

## Prompts for the last device

The function `input.lastDevice()` returns the kind of device that the player used last: `'keyboardMouse'`, `'touch'` or `'gamepad'`. Use it to show matching prompts, such as "Press Space" or "Press A". Moving the mouse alone does not change it. Neither do key repeats or a gamepad button or stick that stays held. So a player who holds a trigger and then presses a key gets keyboard prompts. The function `input.gamepadLabels(index)` tells which letter or shape the bottom button shows.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local pads = {letters = 'Press A', swappedLetters = 'Press B', shapes = 'Press Cross', unknown = 'Press the bottom button'}

scene.push({
    update = function(self, dt)
        local device = input.lastDevice()
        if device == 'gamepad' then
            self.prompt = pads[input.gamepadLabels(1)]
        elseif device == 'touch' then
            self.prompt = 'Tap the screen'
        else
            self.prompt = 'Press Space'
        end
    end,
})
```

## UI and gameplay input

The same devices drive both the UI and the game. The mouse clicks buttons of the HUD and swings swords. The arrows, the gamepad and the remote move the focus of a menu and walk the hero. Two rules decide which side hears a press, so the two never fight over the same input. The pointer goes to what is under it. The keyboard, gamepads and remotes go to what has the focus.

### The pointer

The action map reads `mouse:` bindings as released for a press that went down while the UI owned the pointer, until the button is released. So an action bound to `mouse:left` never goes down when the player clicks a HUD button. A drag that started in the game keeps its action down while it crosses the UI. The mouse functions of `haylen.input` still report every click.

The function `ui.usingPointer()` returns `true` while the pointer is over something the UI owns, and touch controls count as UI. The function `ui.usingKeyboard()` returns `true` while a text field has the focus. Code that reads the devices directly checks them first, so a tap on a touch button never counts as a tap on the world.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if input.mousePressed('left') and not ui.usingPointer() then
            print('Select the unit at ' .. table.concat({input.mousePosition()}, ', ') .. '.')
        end
        if input.keyPressed('h') and not ui.usingKeyboard() then
            print('Toggle the help.')
        end
    end,
})
```

### Who owns the keyboard and the gamepad

The keyboard, gamepads and TV remotes drive the UI through its navigation actions, and the game through the action map. The focus decides which side hears a press:

- **A control has the focus.** The UI owns the navigation input. The directions, accept, cancel, menu, Tab and `uiFocus` move and press its controls. The actions `uiPagePrevious`, `uiPageNext`, `uiFirst` and `uiLast` page through its collections. The action map reads their keys, buttons and the left stick as up.
- **A play area has the focus.** The game owns the directions, accept and menu. So the arrows, WASD, the directional pad, the left stick, Space, Enter and the south button reach the action map and never move the focus. Cancel, Tab and `uiFocus` still belong to the UI. So Escape, the east button and the Menu button of a TV remote reach the `onCancel` handler of the GUI. Tab and `uiFocus` move the focus to the controls of the GUI and back.
- **Nothing has the focus.** The action map reads every key. The UI takes only the presses it answers itself, such as the cancel that closes a popup.

A press stays with the side that took it until it is released. A key held in the game keeps moving the game after Tab moved the focus to a control, and the control never sees it repeat. A key held on a control stays with the UI after the focus moved to the play area.

### The play area

A [play area](../lua-api/ui.md#uiplayareaproperties) is the part of a GUI where the game shows, usually the space a screen leaves between its controls. It is a place the focus can go, which stands for the game.

- A click or a touch on it gives it the focus and still reaches the game, which reads `mouse:` bindings and touches as usual. A click on a control gives the focus to the control.
- The action `uiFocus` moves the focus from the play area to the control that had it last, or to the first control of the GUI, and back. It is the View button of gamepads by default. An app remaps it like any navigation action, such as to the `pause` key that the play and pause button of a TV remote sends.
- The command `gui:command(id, 'focus')` moves the focus from code, and `ui.focusOwner()` returns `'control'`, `'playArea'` or `'none'`.

This complete example shows a level next to a side panel. The hero walks with the arrows, WASD, the stick or the remote while the world has the focus. Tab, the View button or the play and pause button move the focus to the zoom slider, which then takes the arrows. A click on the world gives the focus back to the game.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.loadActions({actions = {
    {name = 'move', type = 'vector', up = {'key:up', 'key:w', 'button:dpadUp'}, down = {'key:down', 'key:s', 'button:dpadDown'}, left = {'key:left', 'key:a', 'button:dpadLeft'}, right = {'key:right', 'key:d', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:enter', 'button:south', 'virtual:jump'}},
    {name = 'uiFocus', type = 'button', bindings = {'button:back', 'key:pause'}},
}})

local Level = {}
Level.__index = Level

function Level:enter()
    self.x, self.y, self.zoom = 0, 0, 2
    self.gui = ui.mount(ui.row{
        onCancel = function()
            scene.pop()
        end,
        ui.playArea{id = 'world', grow = 1, autofocus = true},
        ui.column{
            width = 360,
            ui.slider{id = 'zoom', min = 1, max = 4, value = 2, onChange = function(event)
                self.zoom = event.value
            end},
            ui.touchButton{action = 'jump', text = 'Jump', touchOnly = true},
        },
    }, {owner = self})
end

function Level:update(dt)
    local dx, dy = input.vector('move')
    self.x = self.x + dx * 180 * dt
    self.y = self.y + dy * 180 * dt
    if input.pressed('jump') then
        print('Jump, with the focus on ' .. ui.focusOwner() .. '.')
    end
end

scene.push(setmetatable({}, Level))
```

### Going back

A screen goes back with the `onCancel` handler of its GUI root, not with an action of its own. The action `uiCancel` reaches the root only when no popup, combo list, dialog or edit answers it first. So the Escape that closes a combo list never leaves the screen, and the next Escape does. A game that pauses with Escape has two choices. It gives its screen a play area, whose GUI hears the cancel, or it leaves the focus empty, so that its `pause` action hears it.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Settings = {}
Settings.__index = Settings

function Settings:enter()
    self.gui = ui.mount(ui.column{
        onCancel = function()
            scene.pop()
        end,
        ui.combo{id = 'quality', autofocus = true, items = {{id = 'low', text = 'Low'}, {id = 'high', text = 'High'}}, selected = 'high'},
    })
end

function Settings:exit()
    self.gui:unmount()
end

scene.push(setmetatable({}, Settings))
```

### What the UI captures

A press that the UI answers itself never triggers an action, the same way the action map leaves alone a mouse button that went down over the UI. At the end of every frame, the UI captures for the next frame the keys, gamepad buttons and gamepad axes of the presses it will answer:

- The bindings of every navigation action and Tab, while a control has the focus.
- The bindings of `uiCancel` and Tab while a play area has the focus. By default `uiCancel` is Escape, the east button and the Menu button of a TV remote.
- The bindings of `uiCancel` while something waits for a cancel. That is an open popup, combo list, menu or dialog, a control being edited, a carried item, or the focus inside a window that closes.
- The bindings of `uiFocus` while a GUI has the focus or draws a play area.
- Every key while a text field edits. That includes iOS, iPadOS, tvOS and Mac Catalyst, where a hardware keyboard still reports keys while the native field edits.
- Every key and gamepad button while a `keyCapture` listens.

A key, button or axis pressed while it is captured belongs to the UI until it is released. An action bound to it reads as up for that whole press, even after the popup it closed is gone. A captured axis holds the stick it belongs to. The functions `input.keyCaptured(key)`, `input.gamepadButtonCaptured(button)` and `input.gamepadAxisCaptured(axis)` tell whether a press belongs to the UI, for apps that also read the keyboard directly, whose functions keep reporting every key.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

scene.push({
    update = function(self, dt)
        -- The Escape that closed a popup belongs to the UI, so it never quits the level.
        if input.keyPressed('escape') and not input.keyCaptured('escape') then
            print('Escape reached the game.')
        end
    end,
})
```

## Local multiplayer

In a local multiplayer game, each player gets an action map of their own. The function [`input.newActionMap(options)`](../lua-api/input.md#inputnewactionmapoptions) makes a map that reads only the devices its options name: the keyboard, the mouse, one gamepad or none, and the virtual controls. The option `virtualPrefix` goes in front of the names of `virtual:` bindings, so two touch layouts on one screen drive two players. Every map loads the same documents as the default map. It updates at the start of every frame and every fixed step, and it leaves the engine once Lua lets go of it. The functions of the module, such as `input.down`, read the default map, which `input.actions()` returns as a map too.

A join screen finds the gamepad that pressed a button with `input.findGamepadPress('start')`, which returns the lowest index of a gamepad that pressed it in this frame. It gives that player a map that reads only that gamepad. The events `gamepadConnected` and `gamepadDisconnected` of [`haylen.events`](../lua-api/events.md#engine-events) carry the index. So a game pauses when the gamepad of a player leaves, and resumes when it comes back at the same index.

```lua
local events = require('haylen.events')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')

local players = {
    -- The keyboard player never reads a gamepad.
    {map = input.newActionMap({gamepad = false, virtual = false, actions = 'input/fight.json'})},
}

scene.push({
    enter = function(self)
        events.on('gamepadDisconnected', function(info)
            for _, player in ipairs(players) do
                if player.pad == info.gamepad then
                    haylen.setPaused(true)
                end
            end
        end, {owner = self})
    end,
    update = function(self, dt)
        -- A gamepad that presses Start joins as a new player.
        local pad = input.findGamepadPress('start')
        if pad and #players < 4 then
            players[#players + 1] = {pad = pad, map = input.newActionMap({keyboard = false, mouse = false, virtual = false, gamepad = pad, actions = 'input/fight.json'})}
        end
        for index, player in ipairs(players) do
            if player.map:pressed('attack') then
                print('Player ' .. index .. ' attacks.')
            end
        end
    end,
})
```

In a fixed step, `map:stepButtons(list)` returns one integer per player, with a bit for each action of the list that is down in the step. Command buffers, recordings and bots keep this one number per player and step.

## Command buffers

Action games read moves, not single buttons. A double tap runs. A jump and an attack on the same step make a jump attack. A sequence of defend, forward and attack makes a special. A press stays kept for a few steps, so it lands when the character can act. And the clock stops during hit stop, the short freeze when a blow lands.

A [`CommandBuffer`](../lua-api/input.md#commandbuffer) does this in C++ for each character. It names its buttons and their lifetimes in steps, and the back and forward buttons of a character that faces right. It also names the commands, with their steps, time limits and priorities. In every fixed step, `buffer:update(map)` reads the step state of the actions named like its buttons, or an integer of buttons from `map:stepButtons()` or a recording. The method `buffer:peek()` names the kept command, and `buffer:consume(name)` takes it once the character acts on it. A step is one call that allocates nothing, and the same presses always type the same commands.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

local player = input.newActionMap({gamepad = 1, actions = 'input/fight.json'})
local commands = input.newCommandBuffer({
    buttons = {'left', 'right', 'jump', 'attack', 'defend'},
    lifetime = 8,
    directions = {back = 'left', forward = 'right'},
    commands = {
        {name = 'dash', steps = {'forward', 'forward'}, maxTicks = 10, priority = 1},
        {name = 'special', steps = {'defend', 'forward', 'attack'}, maxTicks = 20, priority = 3},
    },
})

scene.push({
    facing = 1,
    hitStop = 0,
    fixedUpdate = function(self, step)
        self.hitStop = math.max(0, self.hitStop - step)
        commands.frozen = self.hitStop > 0
        commands.facing = self.facing
        commands:update(player)
        local typed = commands:peek()
        if typed and self.hitStop == 0 and commands:consume(typed) then
            print('The fighter does ' .. typed .. '.')
        end
    end,
})
```

## Recording and replay

A recorder keeps the buttons of every player on every fixed step. The function [`input.newRecorder({buttons, players, data})`](../lua-api/input.md#inputnewrecorderoptions) makes one. Each step writes the buttons as masks, or reads them from action maps. The recording also keeps the identifier and the fixed rate of the app, and a string of the app, such as `random:state()` of each seeded generator.

The methods `recording:bytes()` and `input.loadRecording(bytes)` move a recording through files, saves and servers. The function `input.newPlayback(recording)` hands the steps back, so a replay feeds `playback:mask(player)` wherever the game read the masks of the players, such as a [command buffer](#command-buffers).

A run replays exactly when the app follows three rules:

1. It reads input only through those masks.
2. It restores its generators with `random:setState(playback.data)`.
3. It steps its simulation in `fixedUpdate`.

The optional hash of each step finds the first step where a replay goes another way. Automated playtests run a recording on the headless player, which fails on any error. Physics stays deterministic across thread counts. The math functions of the C library, such as sine, may differ between platforms, which matters only for replays that cross platforms.

```lua
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')

local buttons = {'left', 'right', 'jump'}
local random = m.random(7)
local recorder = input.newRecorder({buttons = buttons, data = random:state()})
local playback = nil

scene.push({
    fixedUpdate = function(self, step)
        local mask
        if playback then
            mask = playback:mask(1)
            playback:advance()
        else
            mask = input.actions():stepButtons(buttons)
            recorder:write(1, mask)
            recorder:advance()
        end
        self.jumping = mask & 0x4 ~= 0
    end,
    -- Replays the run from its first step with the same random numbers.
    replay = function(self)
        playback = input.newPlayback(input.loadRecording(recorder:bytes()))
        random:setState(playback.data)
    end,
})
```

## The Tiny Island actions

The sample Tiny Island defines five actions in `samples/games/tiny-island/content/input/actions.json`, and loads them in `source/main.lua` before the first scene.

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
| `move` | WASD or the arrow keys | The left stick or the directional pad | A floating `touchStick` |
| `attack` | Space or a left click | South (A) | A `touchButton` |
| `special` | Left Shift or a right click | West (X) | A `touchButton` |
| `interact` | E | East (B) | A `touchButton` |
| `pause` | Escape | Start | The pause button of the HUD |

The action `move` mixes three kinds of input in one action: the directions of keys and of the directional pad, the left stick, and the virtual stick of the HUD. The player reads it once and never asks which device moved it.

```lua
local input = require('haylen.input')

local player = {}

function player:update(dt)
    local mx, my = input.vector('move')
    self.x, self.y = self.x + mx * self.speed * dt, self.y + my * self.speed * dt
    self.guarding = self.class.special == 'guard' and input.down('special')
    if input.pressed('attack') and self.cooldown <= 0 and not self.guarding then
        self:attack()
    elseif self.class.special ~= 'guard' and input.pressed('special') and self.specialCooldown <= 0 then
        self:special()
    end
end

return player
```

- One action can mean different things for each class. The warrior guards while `special` is held, so the game reads `input.down('special')`. The other classes fire their special once per press with `input.pressed('special')`.
- The game reads `interact` with `input.down`, which doubles the reach for feeding the fire while it is held.
- The gameplay and pause scenes read `input.pressed('pause')`, so Escape and Start open and close the pause menu during a run.
- The settings and class selection screens go back with the `onCancel` handler of their GUI root. The `uiCancel` action of the UI fires it with Escape and the east button, as [going back](#going-back) describes.
- The HUD in `samples/games/tiny-island/source/ui/hud.lua` drives the virtual inputs with a floating `touchStick` for `move`, and `touchButton` controls for `attack`, `special` and `interact`, all with `touchOnly = true`. The pause button of the HUD is a plain icon button that opens the pause menu directly.
- When the pause menu covers the run, the gameplay scene hides the HUD and calls `input.clearVirtual()`, so nothing stays pressed while the game is paused.

## From C++

C++ code reaches the same input through these classes:

- The class `haylen::input::Input` of `haylen/input/Input.hpp`, which `engine.getInput()` returns, holds the state of the devices.
- The class `haylen::input::ActionMap` of `haylen/input/ActionMap.hpp`, which `engine.getActions()` returns, loads the same JSON documents. It also defines and removes actions one by one in code, with `ActionMap::Action` and `ActionMap::Binding`.
- The class `haylen::input::ActionMaps` of `haylen/input/ActionMaps.hpp`, which `engine.getActionMaps()` returns, holds every map. The UI plugin hands the keys and buttons it captures to every map through it. Its method `create(devices)` makes the map of a player with its `ActionMap::Devices`.
- The class `haylen::input::CommandBuffer` of `haylen/input/CommandBuffer.hpp` types commands from the masks of each step.
- The function `engine.getVirtualInput()` returns the `haylen::input::VirtualInput` that touch controls write, and `engine.getGestures()` returns the `haylen::input::GestureRecognizer`.
- The class `haylen::input::Controls` of `haylen/input/Controls.hpp` turns keys, mouse buttons, gamepad buttons and gamepad axes into the names that bindings and Lua use, and back.

The [architecture guide](../core/architecture.md) shows where the frame updates them, and the [Lua guide](../core/lua.md) covers the scripting side.

```cpp
#include "haylen/core/Engine.hpp"
#include "haylen/input/ActionMap.hpp"

// Moves a hero with the action "move" of the default map, from any device.
void steer(haylen::core::Engine& engine, haylen::math::Vec2& hero, float dt) {
    const haylen::input::ActionMap& actions = engine.getActions();
    hero += actions.getVector("move") * (200.0F * dt);
}
```
