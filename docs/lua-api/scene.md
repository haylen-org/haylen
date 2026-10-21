# 📖 `haylen.scene`

The module `haylen.scene` manages the stack of scenes of an app. A scene is one screen of the app, such as the title screen, a level, a pause menu or a dialog. Use this module to push, replace and pop scenes, to load them with progress, to play transitions between them and to tie listeners and tasks to a scene. The [lifecycle guide](../core/lifecycle.md#scene-lifecycle) explains how a change runs step by step, and the [Lua guide](../core/lua.md#scenes) shows how to write a scene.

```lua
local scene = require('haylen.scene')
```

## Scene tables

A scene is any Lua table. It may define hooks, which are functions that the engine calls at moments of its life, and a few fields that the engine reads. Every hook and every field is optional. The engine looks a hook up by name each time it calls it, and it passes the table as `self`. So methods that the table inherits through a metatable or a [class](../core/lua.md#classes) work too.

| Field | When the engine calls or reads it | Arguments |
| --- | --- | --- |
| `load` | Before the scene enters, as a task that the scene owns, so it may wait on promises with `:await()`. The load ends when the function returns, or when the promise it returns settles, and once every group it preloads loaded. | `self` and the load context, a [`SceneLoad`](#sceneload). |
| `enter` | When the scene enters the stack through `push` or `replace`. | `self` and the `params` of the change. |
| `enterTransitionFinished` | On the top scene, once a change ended and its transition finished. Input reaches the scene again from then on. | `self` |
| `exitTransitionStarted` | On the top scene, when a change starts to take it off the screen. | `self` |
| `exit` | When the scene leaves the stack through `pop`, `popTo`, `popToRoot`, `replace` or `clear`. | `self` |
| `unload` | Right after `exit`, after a failed load or when its preload is cancelled. It releases what the scene loaded, so it must also handle a load that stopped halfway. | `self` |
| `pause` | When another scene is pushed on top of it. | `self` |
| `resume` | When the scenes above it are popped and it is on top again. | `self` |
| `paused` | When the game pause stops the scene, by its process mode. | `self` |
| `unpaused` | When the game pause lets the scene run again, by its process mode. | `self` |
| `reloaded` | In development, after a save reloaded Lua modules in place. Every scene of the stack hears it, from the bottom up, as [`haylen.hotReload`](hotReload.md#the-reloaded-hook) describes. | `self` and a table with `modules` and `paths`, the names and the files of the reloaded modules. |
| `event` | For every platform event, on the top scene only. Input events arrive only while its process mode runs and no change holds input back. No event arrives while the error screen shows. | `self` and the event table that [platform events](#platform-events) describes. |
| `fixedUpdate` | On the top scene, once for each fixed step of the frame, which may be zero or several times. It runs only while its process mode runs and no scene covers it. | `self` and the length of a fixed step in seconds, which [`haylen.fixedStep()`](haylen.md#haylenfixedstep) returns. |
| `update` | On the top scene, once per frame, while its process mode runs and no scene covers it. | `self` and the scaled time of the frame in seconds. |
| `render` | On every visible scene, from the bottom up. | `self` |
| `renderUi` | On every visible scene, from the bottom up, after every `render`. | `self` |
| `transparent` | Read every frame. When it is `true`, the scene below renders too. | Not a function. |
| `processMode` | Read every frame. It is `'inherit'`, the default, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`, as [process modes](../core/lifecycle.md#pause-and-process-modes) explain. | Not a function. |
| `name` | Read by the debug overlay, which lists the scenes of the stack by name. Without a name, the overlay shows `Scene`. | Not a function. |

The visible scenes are the top scene and every scene below it, down to the first scene that is not transparent. So a transparent pause menu keeps the level under it on the screen.

A wrong `processMode` raises `The "processMode" must be "inherit", "pausable", "whenPaused", "always" or "disabled".`. The functions `push`, `replace` and `preload` check it at once. A field that turns wrong later stops the app with the same message.

Everything that a scene owns ends when the scene unloads. That covers the task of a `load` that has not finished, the tasks of [`scene.spawn`](#scenespawnowner-fn), the listeners of [`scene.listen`](#scenelistenowner-source-fn-options), and the timers, tweens, event listeners, signal connections and GUIs created with the scene as their `owner`. The tasks stop for good and close their to-be-closed variables. So no callback or task of a scene runs once the scene is gone, as [subscription scopes](../core/lifecycle.md#subscription-scopes) explain.

A table is one scene for as long as the engine holds it, so the same table cannot be on the stack twice. A table whose scene unloaded may be pushed again, and then it loads again.

An error raised in a hook stops the app and shows the error screen with the message and its stack trace. The hook `load` is different: its error fails the load, as [errors](#errors) describes.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local Level = {}
Level.__index = Level

function Level.new(name)
    return setmetatable({name = name, elapsed = 0}, Level)
end

function Level:enter(params)
    print('The level ' .. self.name .. ' starts on ' .. params.difficulty .. '.')
end

function Level:update(dt)
    self.elapsed = self.elapsed + dt
end

function Level:paused()
    print('The game paused.')
end

function Level:render()
    graphics2d.beginScreen()
    graphics2d.drawText(nil, string.format('%s %.1f', self.name, self.elapsed), 80, 80, {size = 48, color = '#FFFFFFFF'})
end

scene.push(Level.new('Forest'), {params = {difficulty = 'hard'}})
```

## Changes

The functions `scene.push`, `scene.replace`, `scene.pop`, `scene.popTo` and `scene.popToRoot` ask for a change of the stack. A change starts in the next update of the scenes, not inside the call. A change asked for while another one runs waits for it, and the changes run in the order they were asked for.

Each of these functions returns a promise. The promise resolves with `true` once the change ended, and with `false` when [`scene.clear`](#sceneclear) dropped it. It rejects with the error when the change fails, such as when the next scene failed to load, so `:await()` then returns `nil` and the error. The engine reports a failed load itself, so a rejected promise that nothing waits for never stops the app.

### Options

Every change takes an optional table of options. The functions `pop`, `popTo` and `popToRoot` load nothing, so they take only the transition keys, from `duration` to `blockInput`, and `onComplete`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `duration` | number | `0` | The length of the whole transition in seconds, without the time it holds for the load. With `0`, no built-in effect draws and the change switches at once. |
| `ease` | curve | `'linear'` | The easing of the progress of the transition, in any form of the [easing curves of `haylen.tween`](tween.md#easing). |
| `effect` | string or table | `'fade'` | The name of a [built-in effect](#built-in-effects), or a table that draws its own effect, as [custom effects](#custom-effects) describes. |
| `direction` | string | `'left'` | Where the motion, wipe, wave or page turn of a built-in effect goes: `'left'`, `'right'`, `'up'`, `'down'`, `'upLeft'`, `'upRight'`, `'downLeft'` or `'downRight'`. |
| `color` | Color | `'#FF000000'` | The color that fades and irises pass through. It also shows behind the effects that uncover the screen. It takes any of the [accepted forms](math.md#accepted-forms) of a color. |
| `reducedMotion` | string | `'keep'` | What the transition does when it starts while the system asks for [reduced motion](accessibility.md). The value `'keep'` plays it as it is, `'shorten'` plays it in a quarter of its time, and `'skip'` switches at once without its effect. |
| `blockInput` | boolean | `true` | Holds input back until the change ends, the load included. The `event` hook of the scenes, the action map and the focus navigation of the UI see no key, button or touch. An action whose bindings are still held when the change ends stays up until they are released. |
| `onComplete` | function | none | Called with `true` once the change ended, or with `false` when it was dropped or failed. |
| `params` | any | none | A value that reaches the `load` and `enter` hooks of the next scene. A preloaded scene keeps the `params` of its preload. |
| `loading` | table | none | A [loading view](#loading-views) to show while the next scene loads. |
| `loadingDelay` | number | `0` | The seconds the load must take before the loading view appears, so quick loads never flash it. |
| `minimumLoadingTime` | number | `0` | The seconds the loading view stays once it appeared, even when the load ends earlier. |
| `loadingFadeOut` | number | `0.25` | The seconds a loading view over the covered frame takes to fade out into it, once the load is done and the view stayed its minimum time. The value `0` takes the view away at once. |
| `unloadBeforeLoad` | boolean | `true` | With an effect that covers the screen, the replaced scene exits and unloads at full cover, before the next scene loads, so less memory is in use at once. With `false`, the next scene starts loading when the transition starts, and the replaced scene stays loaded until the next scene loaded. |
| `onError` | function | none | Receives the message of a failed load instead of the log or the error screen. It may send the app to another scene. |

An unknown option raises `Unknown option "<key>".`. A value of the wrong type raises `The option "<key>" of "<function>" is invalid: <reason>.`. An unknown name of an effect, a direction or a `reducedMotion` raises the same message, such as `The option "effect" of "replace" is invalid: unknown value 'spin'.`. A `loading` that is not a table, or an `onError` or `onComplete` that is not a function, raises a `bad argument` error. A negative `loadingDelay`, `minimumLoadingTime` or `loadingFadeOut` raises `The loading delay, the minimum loading time and the loading fade-out cannot be negative.`.

Transitions, loads and loading views run on real time, so neither the game pause nor the time scale slows them, as the [lifecycle guide](../core/lifecycle.md#scene-lifecycle) explains.

```lua
local scene = require('haylen.scene')

local title = {name = 'title'}
local game = {name = 'game'}

scene.push(title)
scene.replace(game, {
    duration = 0.8,
    effect = 'fade',
    color = '#FF1B1E2B',
    ease = 'quadInOut',
    reducedMotion = 'shorten',
    params = {level = 1},
    onComplete = function(done)
        print(done and 'The game is on the screen.' or 'The change did not finish.')
    end,
})
```

### scene.push(scene, options)

Puts `scene` on top of the stack once it loaded, and returns a promise. The scene that was on top receives `pause`, and the new scene receives `enter`. The [options](#options) are optional.

The argument `scene` must be a table, or the call raises `bad argument #1 to 'push' (table expected, got <type>)`. A scene that is still on the stack, or still leaving it, when the change starts stops the app with `The scene is already on the stack.`.

```lua
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'pause', type = 'button', bindings = {'key:escape', 'button:start'}}}})

local pauseMenu = {
    transparent = true,
    processMode = 'whenPaused',
    enter = function(self) haylen.setPaused(true) end,
    exit = function(self) haylen.setPaused(false) end,
    update = function(self, dt)
        if input.pressed('pause') then
            scene.pop()
        end
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({0, 0, 1920, 1080}, '#99000000')
        graphics2d.drawText(nil, 'Paused', 960, 540, {size = 72, color = '#FFFFFFFF', anchor = {0.5, 0.5}})
    end,
}

local level = {
    update = function(self, dt)
        if input.pressed('pause') then
            scene.push(pauseMenu)
        end
    end,
}

scene.push(level)
```

### scene.replace(scene, options)

Takes the top scene off the stack, puts `scene` in its place once it loaded, and returns a promise. The old scene receives `exit` and `unload`. With an effect that covers the screen, that happens at full cover, before the new scene loads. With `unloadBeforeLoad` set to `false`, it happens once the new scene loaded, right before it enters. With an effect that shows both scenes, it happens at the exit point of the effect, and without a transition at the switch. On an empty stack, `replace` works like `push`.

```lua
local async = require('async')
local scene = require('haylen.scene')

local title = {name = 'title'}
local game = {enter = function(self) print('The game started.') end}

scene.push(title)
async.spawn(function()
    local done = scene.replace(game, {duration = 0.6, color = '#FF1B1E2B'}):await()
    print('The title was replaced: ' .. tostring(done) .. '.')
end)
```

### scene.pop(options)

Takes the top scene off the stack and returns a promise. The removed scene receives `exit` and `unload`, and the scene below, if any, receives `resume`. Popping an empty stack changes nothing, and the promise resolves with `true`.

```lua
local scene = require('haylen.scene')

local town = {resume = function(self) print('Back in town.') end}
local shop = {exit = function(self) print('Leaving the shop.') end}

scene.push(town)
scene.push(shop)
scene.pop({duration = 0.3, color = '#FFFFFFFF', onComplete = function(done) print('The shop closed: ' .. tostring(done) .. '.') end})
```

### scene.popTo(level, options)

Takes scenes off the top until `level` scenes remain, and returns a promise. The scenes leave from the top down, and each one receives `exit` and `unload`. The scene left on top receives `resume`. The value `0` takes every scene off. A stack with `level` scenes or fewer stays as it is: the change ends as soon as it starts, and the promise resolves with `true`. A negative `level` raises `bad argument #1 to 'popTo' (expected a non-negative integer)`.

```lua
local scene = require('haylen.scene')

scene.push({name = 'title'})
scene.push({name = 'world map', resume = function(self) print('Back on the world map.') end})
scene.push({name = 'dungeon'})
scene.push({name = 'inventory'})

-- Leaving the dungeon closes the inventory too.
scene.popTo(2, {duration = 0.4})
```

### scene.popToRoot(options)

Takes every scene off the stack except the bottom one, and returns a promise. It is the same as `scene.popTo(1, options)`.

```lua
local scene = require('haylen.scene')

local title = {resume = function(self) print('The title screen shows again.') end}
scene.push(title)
scene.push({name = 'level'})
scene.push({name = 'game over'})

scene.popToRoot({duration = 0.5})
```

### scene.clear()

Removes every scene at once, inside the call and without a transition. It runs these steps in order.

1. The loading view of the running change exits.
2. The scenes on the stack receive `exit` and `unload`, from the top down.
3. The scenes that still leave through a transition receive `exit` and `unload`.
4. The next scene of the running change, when it is loading or loaded, and every preloaded scene receive `unload`.
5. The running change, the waiting changes and the preloads are dropped, with the changes that the hooks of these steps asked for. Their promises resolve with `false`.

Every scene leaves even when a hook raises an error, and the first error is raised again once the stack is empty. Use `clear` to start over. It works from any hook, even from the `enter` of a scene that is being pushed.

```lua
local scene = require('haylen.scene')

local title = {enter = function(self) print('The title screen shows.') end}

local function backToTitle()
    scene.clear()
    scene.push(title)
end

backToTitle()
```

## Transitions

A transition is an effect that plays while the stack changes. A change has one when its `duration` is above `0`. Effects come in two kinds, and each kind runs a change on its own timeline, which the [lifecycle guide](../core/lifecycle.md#scene-lifecycle) draws with every hook and event. A change without a transition loads the next scene first, while the current scenes stay on the screen, and then switches within one update.

### Effects that cover the screen

An effect of this kind shows one scene at a time, such as `'fade'` or `'iris'`. Its out phase hides the current scenes until the screen is fully covered. There the stack changes, and the effect holds the covered frame while the next scene loads, with the loading view over it. Its in phase shows the next scene. So a change without a loading view uses the covered frame, such as the color of a fade, as its loading screen. The guide shows [the timeline of these effects](../core/lifecycle.md#effects-that-cover-the-screen).

### Effects that show both scenes

An effect of this kind shows both scenes at once, such as `'crossFade'` or `'slideIn'`. The next scene loads first, while the current scenes stay on the screen and keep running. Then the stack changes, and the effect plays with both scenes alive, each one rendered into its own image every frame. The scenes that leave exit at the exit point of the effect, which is its end for the built-in effects. The guide shows [the timeline of these effects](../core/lifecycle.md#effects-that-show-both-scenes).

### Built-in effects

The option `effect` names one of these effects. The third column tells which values of the option `direction` an effect uses. The effects that cover the screen do it halfway through, at a progress of `0.5`. Every effect follows the eased progress, so `ease` shapes the motion of the effects that move.

| Effect | What it shows | Direction | Kind |
| --- | --- | --- | --- |
| `'fade'` | The outgoing scene fades to the color, and the incoming one fades back from it. | None. | Covers the screen. |
| `'crossFade'` | The outgoing scene fades into the incoming one. | None. | Shows both scenes. |
| `'moveIn'` | The incoming scene moves in toward the direction and covers the outgoing one, which stays still. | Any. | Shows both scenes. |
| `'slideIn'` | The incoming scene slides in toward the direction. The outgoing one slides a third of the way and darkens, like a navigation stack. | Any. | Shows both scenes. |
| `'push'` | Both scenes move together toward the direction, and the incoming one pushes the outgoing one out. | Any. | Shows both scenes. |
| `'shrinkGrow'` | The outgoing scene shrinks away while the incoming one grows in, over the color. | None. | Shows both scenes. |
| `'flipX'` | The outgoing scene turns in perspective around the vertical axis, and the incoming one turns in on its back, over the color. | Left or right. | Covers the screen. |
| `'flipY'` | The same turn around the horizontal axis. | Up or down. | Covers the screen. |
| `'zoomFlip'` | A flip that moves away while it turns. It turns around the vertical axis for left and right, and around the horizontal axis for up and down. | Any. | Covers the screen. |
| `'rotoZoom'` | The outgoing scene spins twice and shrinks away, and the incoming one spins and grows back. | None. | Covers the screen. |
| `'jumpZoom'` | The outgoing scene shrinks and jumps out toward the direction, and the incoming one jumps in and grows. | Any. | Covers the screen. |
| `'splitColumns'` | The outgoing scene splits into three columns that slide up and down out of view, and the incoming one closes back from them. | Up or down, for the first column. | Covers the screen. |
| `'splitRows'` | The same with three rows that slide left and right. | Left or right, for the first row. | Covers the screen. |
| `'turnOffTiles'` | The tiles of the outgoing scene turn off in random order and uncover the incoming one. | None. | Shows both scenes. |
| `'fadeTiles'` | The tiles of the outgoing scene shrink away in a wave that travels toward the direction. Use `'upRight'` for the classic fade toward the top right. | Any. | Shows both scenes. |
| `'pageTurn'` | The outgoing scene curls away like a page turned toward the direction, and shows its paper back. | Any. | Shows both scenes. |
| `'radialClockwise'` | A hand sweeps clockwise from twelve o'clock and uncovers the incoming scene. | None. | Shows both scenes. |
| `'radialCounterclockwise'` | The same sweep, counterclockwise. | None. | Shows both scenes. |
| `'wipe'` | An edge moves toward the direction and uncovers the incoming scene. | Any. | Shows both scenes. |
| `'inOut'` | The incoming scene grows out of the center. | None. | Shows both scenes. |
| `'outIn'` | The outgoing scene shrinks into the center and uncovers the incoming one. | None. | Shows both scenes. |
| `'iris'` | A circle closes on the outgoing scene to the color, and opens on the incoming one. | None. | Covers the screen. |
| `'dissolve'` | The incoming scene appears in random order, a few pixels at a time. | None. | Shows both scenes. |
| `'pixelate'` | The outgoing scene breaks into growing blocks, and the incoming one comes back out of them. | None. | Covers the screen. |

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local function level(name, color)
    return {
        name = name,
        render = function(self)
            graphics2d.beginScreen()
            graphics2d.drawRect(graphics2d.canvasBounds(), color)
            graphics2d.drawText(nil, name, 960, 540, {size = 96, anchor = {0.5, 0.5}})
        end,
    }
end

scene.push(level('Forest', '#FF2E5E3A'))
scene.replace(level('Cave', '#FF2B2340'), {effect = 'slideIn', direction = 'left', duration = 0.5, ease = 'quadOut'})
scene.push(level('Map', '#FF1B3A5E'), {effect = 'pageTurn', direction = 'left', duration = 0.8})
scene.pop({effect = 'iris', color = '#FF000000', duration = 0.6})
scene.replace(level('Boss', '#FF5E1B1B'), {effect = 'fadeTiles', direction = 'upRight', duration = 0.7})
```

### Custom effects

The option `effect` also takes a table that draws the effect itself, which may be an instance of a class. A custom effect takes no `color` and no `direction`. The engine reads its fields `switchProgress` and `exitProgress` once, when the change is asked for.

| Field | Default | Meaning |
| --- | --- | --- |
| `render` | none | The method that draws the effect, which every custom effect needs. |
| `switchProgress` | `0.5` | The progress, from `0` to `1`, where the effect covers the whole screen. The stack changes there, and the effect holds there while the next scene loads. The value `0` makes an effect that shows both scenes from start to end, which starts once the next scene loaded. |
| `exitProgress` | `1` | The progress, from `0` to `1`, where the leaving scenes exit. Only an effect with a `switchProgress` of `0` takes it, since an effect that covers the screen exits at its `switchProgress`. |

The engine calls `effect:render(progress, outgoing, incoming)` every frame of the transition. The value `progress` is the eased progress, from `0` to `1`, and it stays at `switchProgress` while the screen is covered. The values `outgoing` and `incoming` are [textures](graphics.md#texture) that hold the scenes before and after the change, each with the pixel size of the viewport. The method draws over the whole frame, usually on a screen canvas of [`haylen.graphics2d`](graphics2d.md) that it begins itself, whose visible area covers each image exactly. The image `incoming` stays empty until the next scene enters. The image `outgoing` keeps the last frame of the scenes before the change once they stop rendering, such as during the hold of an effect that covers the screen or after the exit point.

An effect that breaks these rules raises one of these messages when the change is asked for: `A transition effect needs a "render" method.`, `A transition effect needs a "switchProgress" between 0 and 1.`, `A transition effect that covers the screen at its "switchProgress" takes no "exitProgress".` or `A transition effect needs an "exitProgress" between 0 and 1.`. A change that gives a custom effect together with the option `color` or `direction` raises `A custom transition effect takes no color or direction.`, and an `effect` that is neither a string nor a table raises `The transition effect must be the name of a built-in effect or a table with a "render" method.`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

-- The incoming scene opens like a door from the middle while the outgoing one darkens behind it.
local door = {
    switchProgress = 0,
    exitProgress = 1,
    render = function(self, progress, outgoing, incoming)
        graphics2d.beginScreen()
        local area = graphics2d.canvasBounds()
        local shade = 1 - progress * 0.6
        graphics2d.draw(outgoing, area.x, area.y, {pivotX = 0, pivotY = 0, width = area.width, height = area.height, color = {shade, shade, shade, 1}})
        local width = area.width * progress
        graphics2d.draw(incoming, area.x + (area.width - width) / 2, area.y, {pivotX = 0, pivotY = 0, width = width, height = area.height, source = {incoming.width * (1 - progress) / 2, 0, incoming.width * progress, incoming.height}})
    end,
}

scene.push({name = 'map'})
scene.replace({name = 'battle', enter = function() print('The battle begins.') end}, {duration = 0.8, ease = 'quadInOut', effect = door})
```

## Loading

Every scene loads before it enters, through its `load` hook and its [`SceneLoad`](#sceneload). A change loads its next scene on its own. These functions load a scene ahead of time, read the progress of a load and show a loading view while a change waits for its load. The [lifecycle guide](../core/lifecycle.md#loading-preloading-and-errors) explains how loads, preloads and failures fit into a change.

### scene.preload(scene, params)

Starts loading `scene` in the background with its `load` hook, without changing the stack. The optional value `params` reaches the `load` and `enter` hooks of the scene. The function returns a promise that resolves with `true` once the scene loaded, and with `false` when [`scene.cancelPreload`](#scenecancelpreloadscene) or `scene.clear` dropped it. It rejects with the error of a failed load. A failed preload reports its error only through this promise and the event `sceneLoadFailed`, so a promise that nothing waits for never stops the app.

A later `push` or `replace` of the scene takes it at once, or waits only for the rest of its load. The scene keeps the `params` of its preload. So a change that brings it with `params` stops the app with `A preloaded scene keeps the "params" of its preload.`. Preloading a scene that is loading, loaded or on the stack raises `The scene is already loaded or on the stack.`.

```lua
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local level = {
    load = function(self, context)
        self.slot = context.params
        async.sleep(300):await()
    end,
}

-- The title stays on the screen, draws the progress of the level and starts it once it loaded.
scene.push({
    enter = function(self)
        scene.spawn(self, function()
            scene.preload(level, 'Slot 1'):await()
            scene.replace(level, {duration = 0.4})
        end)
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({560, 700, 800 * scene.loadProgress(level), 24}, '#FFF2C14E')
    end,
})
```

### scene.cancelPreload(scene)

Unloads a preloaded scene that no change took, and stops its load when it still runs. The scene receives `unload`, and the promise of its preload resolves with `false`. A scene that is not preloaded raises `The scene is not preloaded.`.

```lua
local async = require('async')
local scene = require('haylen.scene')

local bonus = {load = function(self) async.sleep(5000):await() end}
scene.preload(bonus)
scene.cancelPreload(bonus)
print(scene.state(bonus)) -- unloaded
```

### scene.loadProgress(scene)

Returns two values: the progress of the load of a scene, from `0` to `1`, and its message. A scene that is loading returns the progress of its running load and the message it reported, which is empty when it reported none. A scene that loaded, or that is on the stack, returns `1`. A table whose scene has not started loading or unloaded returns `0`, and so does a table that never was a scene.

```lua
local async = require('async')
local scene = require('haylen.scene')

local level = {
    load = function(self, context)
        context:progress(0.25, 'Reading the map')
        async.sleep(200):await()
    end,
}

scene.preload(level)
local value, message = scene.loadProgress(level)
print(value, message) -- 0.25 Reading the map
```

### Loading views

A loading view is a table that shows while the next scene of a change loads. Give it to a change with the option `loading`. Its hooks are optional, like those of a scene, so an ordinary scene table can serve as one.

| Hook | When the engine calls it | Arguments |
| --- | --- | --- |
| `enter` | When the view appears. | `self` |
| `exit` | When the view goes away. | `self` |
| `update` | Once per frame while the view shows. | `self`, the real time of the frame in seconds, the progress of the load from `0` to `1` and its message. |
| `render`, `renderUi` | Every frame while the view shows. | `self`, the progress and the message. |

The view appears once the load took longer than `loadingDelay`. With an effect that covers the screen, it shows over the covered frame. Before an effect that shows both scenes, or in a change without an effect, it shows over the current scenes. Once it appeared, it stays at least `minimumLoadingTime`. Over the covered frame, it then fades out over `loadingFadeOut` seconds. The engine fades everything the view draws, its GUIs included, so the view needs no fade of its own. Over the current scenes, it goes away at once. A failed load takes the view away at once too. The view exits right before the next scene enters.

The view draws like the top scene, before the engine plugins draw, such as the GUIs. It runs on real time, so it animates while the game is paused. Everything that it owns ends when it exits.

A scene on the stack can be the loading screen instead. It preloads the next scene with `scene.preload`, draws `scene.loadProgress` and replaces itself once the preload resolved, as the example of [`scene.preload`](#scenepreloadscene-params) shows.

```lua
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

local bar = {
    enter = function(self) self.time = 0 end,
    update = function(self, dt, progress) self.time = self.time + dt end,
    render = function(self, progress, message)
        graphics2d.beginScreen()
        graphics2d.drawRect({560, 700, 800, 24}, '#FF3A3F55')
        graphics2d.drawRect({560, 700, 800 * progress, 24}, '#FFF2C14E')
        graphics2d.drawText(nil, message, 960, 660, {size = 36, anchor = {0.5, 0.5}})
    end,
}

local world = {
    load = function(self, context)
        for step = 1, 10 do
            context:progress(step / 10, 'Building the world')
            async.sleep(100):await()
        end
    end,
}

scene.push({name = 'menu'})
scene.replace(world, {duration = 0.6, loading = bar, loadingDelay = 0.2, minimumLoadingTime = 0.5, loadingFadeOut = 0.3})
```

### scene.loadingViewOpacity()

Returns how much of the loading view of the running change shows. It is `1` while the view shows, falls to `0` while it fades out into the covered frame, and is `0` without a view. A view that plays sounds can fade them with it.

```lua
local assets = require('haylen.assets')
local async = require('async')
local audio = require('haylen.audio')
local scene = require('haylen.scene')

local view = {
    enter = function(self)
        self.voice = audio.play(assets.load('music/waiting.ogg'), {loop = true})
    end,
    update = function(self, dt, progress)
        audio.setVolume(self.voice, scene.loadingViewOpacity())
    end,
    exit = function(self)
        audio.stop(self.voice)
    end,
}

scene.push({name = 'menu'})
scene.replace({load = function() async.sleep(1000):await() end}, {duration = 0.6, loading = view})
```

## SceneLoad

The hook `load` receives a `SceneLoad`, the load context, as its second argument. Messages name it `haylen.SceneLoad`. Use it to read the `params` of the change, to report progress and to preload asset groups. The context works until the scene enters or unloads, and using it after that raises `This "haylen.SceneLoad" was already released.`. Reading a member that it does not have raises `The type "haylen.SceneLoad" has no member "<name>".`.

The progress of the load is the mean of the progress that the scene reports and the progress of each group it preloads. Before the scene reports any progress or preloads a group, the progress is `0`. Once the load is done, it is `1`. Loading views and [`scene.loadProgress`](#sceneloadprogressscene) read it. The asset manager decodes off the frame thread, so a load keeps the frames smooth, as the [lifecycle guide](../core/lifecycle.md#loading-preloading-and-errors) explains.

```lua
local assets = require('haylen.assets')
local async = require('async')
local scene = require('haylen.scene')

assets.defineGroup('forest', {'images/'})

local forest = {}

function forest:load(context)
    context:progress(0, 'Reading the save')
    self.save = {day = context.params.day}
    async.sleep(100):await()
    context:progress(0.5, 'Planting trees')
    context:preload('forest'):await()
    self.trees = 12
end

function forest:enter()
    print('Day ' .. self.save.day .. ' has ' .. self.trees .. ' trees.')
end

function forest:unload()
    self.trees = nil
end

scene.push(forest, {duration = 0.6, params = {day = 3}})
```

A `load` that returns a promise ends when the promise settles. So a load can hand its work to a promise instead of waiting in its own task, and a promise that rejects fails the load.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

scene.push({
    load = function(self)
        return assets.loadAsync('data/level.json')
    end,
})
```

### context.params

The property `context.params` holds the `params` of the change that loads the scene, or of its preload. It is read-only, and it is `nil` without them.

```lua
local scene = require('haylen.scene')

scene.push({
    load = function(self, context)
        self.difficulty = context.params and context.params.difficulty or 'normal'
    end,
}, {params = {difficulty = 'hard'}})
```

### context:progress(value, message)

Reports the progress of the work that the scene does itself. The number `value` runs from `0` to `1`. The string `message` is optional and empty by default, and loading views show it. A value outside that range raises `A load progress runs from 0 to 1.`. Once the load is over, the call changes nothing.

```lua
local async = require('async')
local scene = require('haylen.scene')

scene.push({
    load = function(self, context)
        for chunk = 1, 4 do
            context:progress(chunk / 4, 'Building chunk ' .. chunk)
            async.sleep(50):await()
        end
    end,
})
```

### context:preload(groups)

Loads one [preload group of `haylen.assets`](assets.md#preload-groups), named by a string, or a list of them, and holds the load until all of them loaded. It returns a promise that resolves with `true` once every group loaded, and at once for an empty list. The progress of each group counts in the progress of the load.

An asset of a group that fails fails the load, and the promise rejects with `The asset group "<name>" could not load <path>: <reason>.`, with every failed asset after the first one joined by commas. The load reports that failure itself, so the promise never stops the app when nothing waits for it. A group whose assets are not on the device yet downloads them first, as the [loading guide](../content/loading.md#scenes) describes. A group that is not defined raises `The asset group "<name>" is not defined.`, and a call once the load is over raises `The scene load is over.`.

```lua
local assets = require('haylen.assets')
local scene = require('haylen.scene')

assets.defineGroup('cave', {'images/cave/'})
assets.defineGroup('cave-sounds', {'audio/cave/'})

scene.push({
    load = function(self, context)
        context:preload({'cave', 'cave-sounds'}):await()
        print('The cave is ready.')
    end,
})
```

## The stack

These functions read the stack and the scenes that the engine holds. A change counts in them only once it changed the stack.

### scene.size()

Returns the number of scenes on the stack.

```lua
local scene = require('haylen.scene')
local timer = require('haylen.timer')

scene.push({})
timer.after(0.5, function()
    print('Scenes on the stack: ' .. scene.size() .. '.')
end)
```

### scene.top()

Returns the table of the top scene, the same table that was pushed. It returns `nil` when the stack is empty or when the top scene was pushed from C++.

```lua
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local battle = {name = 'battle'}
scene.push(battle)

timer.after(0.5, function()
    print(scene.top() == battle) -- true
end)
```

### scene.at(index)

Returns the table of the scene at `index`, counted from `1` at the bottom of the stack. It returns `false` for a scene pushed from C++, and `nil` past either end of the stack.

```lua
local scene = require('haylen.scene')
local timer = require('haylen.timer')

scene.push({name = 'title'})
scene.push({name = 'level'})

timer.after(0.5, function()
    print(scene.at(1).name, scene.at(2).name, scene.at(3)) -- title level nil
end)
```

### scene.list()

Returns a list with the tables of every scene, from the bottom of the stack to the top. A scene pushed from C++ shows as `false`.

```lua
local scene = require('haylen.scene')
local timer = require('haylen.timer')

scene.push({name = 'title'})
scene.push({name = 'settings'})

timer.after(0.5, function()
    for index, entry in ipairs(scene.list()) do
        print(index, entry.name)
    end
end)
```

### scene.state(scene)

Returns the state of the scene of a table: `'created'`, `'loading'`, `'loaded'`, `'entering'`, `'active'`, `'covered'`, `'exiting'`, `'exited'` or `'unloaded'`. The [scene lifecycle](../core/lifecycle.md#scene-lifecycle) tells when a scene reaches each state, with its hook and its event. A table whose scene the engine no longer holds returns `'unloaded'`, and a table that never was a scene returns `nil`.

```lua
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local level = {}
print(scene.state(level)) -- nil
scene.push(level)
print(scene.state(level)) -- created
timer.after(0.5, function()
    print(scene.state(level)) -- active
end)
```

### scene.transitioning()

Returns `true` from the moment a change is asked for until it ends: while it waits to start, while its next scene loads and while its transition plays.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')

input.loadActions({actions = {{name = 'confirm', type = 'button', bindings = {'key:enter', 'button:south'}}}})

scene.push({
    update = function(self, dt)
        if not scene.transitioning() and input.pressed('confirm') then
            scene.replace({}, {duration = 0.5})
        end
    end,
})
```

## Owners

An owner is a table or a userdata that ends what it holds when it ends. A scene ends when it unloads, a GUI when it is unmounted, and any other owner when the garbage collector frees it, at the end of that frame. These functions tie a listener or a task to an owner, as [subscription scopes](../core/lifecycle.md#subscription-scopes) explain. An `owner` of another type raises `An owner must be a table or a userdata, not <type>.`.

### scene.listen(owner, source, fn, options)

Connects `fn` for as long as `owner` lives, and returns the [`Connection`](signal.md#connection). The argument `source` is a signal of [`haylen.signal`](signal.md), which `fn` then listens to like `sig:connect`, or the name of an event of [`haylen.events`](events.md), which `fn` then listens to like `events.on`. The table `options` is optional and takes the options of that function: `priority`, `once` and `deferred` for a signal, and `channel`, `priority`, `once` and `filter` for an event. An `owner` key in it is ignored, since the first argument is the owner. Any other `source` raises `bad argument #2 to 'listen' (signal or event name expected, got <type>)`.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')
local signal = require('haylen.signal')
local timer = require('haylen.timer')

local scoreChanged = signal.new('scoreChanged')

local hud = {}
function hud:enter()
    scene.listen(self, scoreChanged, function(score) print('The score is ' .. score .. '.') end)
    scene.listen(self, 'playerDied', function() print('The game is over.') end, {once = true})
end

scene.push(hud)
timer.after(0.1, function()
    scoreChanged:emit(120)
    events.emit('playerDied')
end)
```

### scene.spawn(owner, fn)

Runs `fn` as a task that `owner` holds, and returns nothing. Like a task of `async.spawn`, it may wait on promises with `:await()`, and an error it raises reaches the error screen with the stack of the task. When the owner ends, such as a scene that unloads, the task stops for good, the way `task.cancel()` of `async` stops a task. It never resumes, even when a promise it waits for settles later, and its to-be-closed variables close. A task that ends its own owner stops at its next wait, and an error it raises before then still reaches the error screen. A `fn` that is not a function raises `bad argument #2 to 'spawn' (function expected, got <type>)`.

A task of `async.spawn` belongs to nobody, and it runs to its end unless `task.cancel()` of its handle stops it. The [Lua guide](../core/lua.md#asynchronous-code) explains tasks and promises.

```lua
local platform = require('haylen.platform')
local scene = require('haylen.scene')

local profile = {}

function profile:enter()
    scene.spawn(self, function()
        local account = platform.call('profile.load', {id = 'me'}):await()
        -- This line never runs when the player left the profile before the answer came.
        self.name = account and account.name
    end)
end

scene.push(profile)
```

## Scene

The table `scene.Scene` is a [class](../core/lua.md#classes) to build scenes on with `haylen.class`. Its instances are ordinary scene tables with two more methods, which use the scene as the owner.

### level:listen(source, fn, options)

Calls `scene.listen(level, source, fn, options)` and returns its `Connection`.

### level:spawn(fn)

Calls `scene.spawn(level, fn)`.

```lua
local async = require('async')
local events = require('haylen.events')
local haylen = require('haylen')
local scene = require('haylen.scene')
local timer = require('haylen.timer')

local Level = haylen.class('Level', scene.Scene)

function Level:init(name)
    self.name = name
end

function Level:enter()
    self:listen('checkpoint', function(id) print('The ' .. self.name .. ' reached checkpoint ' .. id .. '.') end)
    self:spawn(function()
        async.sleep(1000):await()
        print('The ' .. self.name .. ' has run for a second.')
    end)
end

scene.push(Level('Forest'))
timer.after(0.1, function() events.emit('checkpoint', 2) end)
```

## Events

A scene hears two kinds of events: the platform events, through its hook `event`, and the scene events, which the scenes publish on the event bus.

### Platform events

The hook `event` of the top scene receives one table for every platform event, after the autoloads received it. Its field `type` names the event, and the other fields depend on the type. Positions and movements are in design units, the same space the app draws in. The events of keys, characters, the mouse buttons, mouse moves, scrolls and touches also carry `modifiers`, a table with the booleans `shift`, `control`, `alt` and `super` for the modifier keys held during the event.

| `type` | Other fields and meaning |
| --- | --- |
| `'keyDown'`, `'keyUp'` | The field `key` is the name of the key, such as `'space'`, `'a'`, `'escape'` or `'left'`, or `'unknown'` for a key without a name. The field `repeat` is `true` for the key downs that holding a key repeats. |
| `'character'` | The field `character` is the typed text as a UTF-8 string. |
| `'mouseDown'`, `'mouseUp'` | The field `button` is `'left'`, `'right'` or `'middle'`, and `x` and `y` are the position of the mouse. |
| `'mouseMove'` | The fields `x` and `y` are the position of the mouse, and `dx` and `dy` the movement of this event, which keeps coming while the mouse is locked. |
| `'mouseScroll'` | The fields `scrollX` and `scrollY` are the scroll, with positive values for scrolling to the left and up. They are points of the screen when `precise` is `true`, as a trackpad reports them, and steps of a wheel otherwise. |
| `'mouseEnter'`, `'mouseLeave'` | No other fields. |
| `'touchBegan'`, `'touchMoved'`, `'touchEnded'`, `'touchCancelled'` | The field `touches` is a list of every finger as a table with `id`, `x`, `y` and `changed`. The field `changed` is `true` for the fingers that caused the event. |
| `'resized'` | No other fields. |
| `'suspended'`, `'resumed'` | No other fields. The app went to the background or came back. |
| `'focusGained'`, `'focusLost'` | No other fields. |
| `'quitRequested'` | No other fields. The player asked to close the window, or to quit the app through its menu, Command+Q or the Dock on macOS. |
| `'lowMemory'` | No other fields. The platform is short of memory. |
| `'textEdited'` | The field `field` is the id of the text field, `text` what its native field holds now, and `selectionStart` and `selectionEnd` the selection. The fields `compositionStart` and `compositionEnd` exist only while an input method composes. The UI applies the edits of its own fields, and the app applies those of [`input.editText`](input.md#inputedittextoptions). |
| `'textAction'` | The field `field` is the id of the text field, and `action` is `'submit'`, `'next'`, `'cancel'` or `'dismissed'`. The UI applies the actions of its own fields. |
| `'keyboardChanged'` | The field `frame` is the rectangle that the on-screen keyboard covers, empty while it is hidden. |
| `'networkChanged'` | The field `online` tells whether the device has a network. |
| `'interruptionBegan'`, `'interruptionEnded'` | No other fields. The system interrupted the app, such as for a phone call, or gave it back. |
| `'windowMoved'`, `'monitorsChanged'` | No other fields. The desktop window moved, or the monitors of the desktop changed. The events `windowMoved` and `windowMonitorsChanged` of [`haylen.events`](events.md#engine-events) report them with their details. |
| `'audioServicesReset'` | No other fields. The audio services of iOS, tvOS or Mac Catalyst restarted, and the engine opened the audio device again. |

The name `repeat` is a keyword of Lua, so read it as `event['repeat']`. The [key names](input.md#key-names) of `haylen.input` list every value of `key`. Gameplay reads input through the action map of [`haylen.input`](input.md), and these events suit text entry and pointer tracking best. Changes of the app, such as going to the background, also reach the listeners of [`haylen.events`](events.md#engine-events), which is the better place to react to them.

```lua
local scene = require('haylen.scene')

local nameEntry = {text = ''}

function nameEntry:event(event)
    if event.type == 'character' then
        self.text = self.text .. event.character
    elseif event.type == 'keyDown' and event.key == 'backspace' and #self.text > 0 and not event['repeat'] then
        self.text = self.text:sub(1, utf8.offset(self.text, -1) - 1)
    elseif event.type == 'mouseDown' then
        print('Clicked at ' .. event.x .. ', ' .. event.y .. ' with the ' .. event.button .. ' button.')
    elseif event.type == 'touchBegan' then
        for _, touch in ipairs(event.touches) do
            if touch.changed then
                print('Finger ' .. touch.id .. ' at ' .. touch.x .. ', ' .. touch.y .. '.')
            end
        end
    end
end

scene.push(nameEntry)
```

### Scene events

The scenes publish their life on the event bus of [`haylen.events`](events.md#engine-events). Listeners receive one table. The field `scene` holds the scene table, and `false` for a scene pushed from C++. The fields `from` and `to` hold the top scenes before and after the change, and a field is missing when there is no such scene.

| Event | When | Table |
| --- | --- | --- |
| `sceneLoading` | A load starts. | `{scene}` |
| `sceneLoaded` | A load finished. | `{scene}` |
| `sceneLoadFailed` | A load failed, right before the scene unloads. The field `error` holds the message. | `{scene, error}` |
| `sceneEntered` | A scene entered the stack. | `{scene}` |
| `scenePaused` | A pushed scene covered the scene. | `{scene}` |
| `sceneResumed` | The scene is on top again. | `{scene}` |
| `sceneExitTransitionStarted` | A change started to take the top scene off the screen. | `{scene}` |
| `sceneEnterTransitionFinished` | A change ended, on the top scene after it. | `{scene}` |
| `sceneExited` | A scene left the stack. | `{scene}` |
| `sceneUnloaded` | A scene unloaded. | `{scene}` |
| `transitionOutStarted`, `transitionOutFinished` | The out phase of a transition, which hides the current scenes, started or finished. | `{from, to}` |
| `transitionInStarted`, `transitionInFinished` | The in phase of a transition, which shows the scenes after the change, started or finished. The field `to` is the scene that the change put or left on top. | `{from, to}` |

A change without an effect publishes no transition events. The lifecycle guide shows [when each event comes](../core/lifecycle.md#scene-lifecycle) in a change.

```lua
local events = require('haylen.events')
local scene = require('haylen.scene')

events.on('sceneEntered', function(event)
    print('The scene ' .. tostring(event.scene and event.scene.name) .. ' entered.')
end)

events.on('sceneLoadFailed', function(event)
    print('A scene could not load: ' .. event.error)
end)

events.on('transitionInFinished', function(transfer)
    print(transfer.to and 'The change ended.' or 'The stack is empty.')
end)

scene.push({name = 'title'})
```

## Errors

A failed load ends its change without the next scene, as the [lifecycle guide](../core/lifecycle.md#loading-preloading-and-errors) explains. The promise of the change rejects with the error, and `onComplete` receives `false`. The error goes to the `onError` of the change when it has one. Without one, it goes to the log as `A scene could not load: <message>` while the scene that was on top stays. It goes to the error screen when no such scene is left, such as after a `replace` that unloaded it first or for the first scene of the app.

```lua
local scene = require('haylen.scene')

local offline = {name = 'offline'}
local online = {
    load = function(self)
        error('The server is not reachable.')
    end,
}

scene.push({name = 'title'})
scene.replace(online, {duration = 0.5, onError = function(message)
    print('The online mode could not load: ' .. message)
    scene.replace(offline)
end})
```

A change whose hook raises an error, such as `enter` or `exit`, ends at that hook. The error screen shows the error, the promise of the change rejects with it, and `onComplete` receives `false`. The scenes stay as the failure left them, and the changes asked for after it run once the app recovers. So a listener of `appRecovered` can put the stack in order, such as with `scene.clear()` and a push of the menu.

These messages come from the functions and methods of the module.

| Message | Cause |
| --- | --- |
| `bad argument #1 to 'push' (table expected, got <type>)` | The scene of `push`, `replace`, `preload`, `cancelPreload`, `state` or `loadProgress` is not a table. The message names the function. |
| `The "processMode" must be "inherit", "pausable", "whenPaused", "always" or "disabled".` | The field `processMode` of a scene has another value. |
| `Unknown option "<key>".` | An options table has a key that the function does not take. |
| `The option "<key>" of "<function>" is invalid: <reason>.` | An option has a value of the wrong type, or an unknown name of an effect, a direction or a `reducedMotion`. |
| `The loading delay, the minimum loading time and the loading fade-out cannot be negative.` | The option `loadingDelay`, `minimumLoadingTime` or `loadingFadeOut` is below `0`. |
| `A custom transition effect takes no color or direction.` | A custom effect came with the option `color` or `direction`. |
| `The transition effect must be the name of a built-in effect or a table with a "render" method.` | The option `effect` is neither a string nor a table. |
| `A transition effect needs a "render" method.` | A custom effect has no `render` method. |
| `A transition effect needs a "switchProgress" between 0 and 1.` | The `switchProgress` of a custom effect is out of range. |
| `A transition effect that covers the screen at its "switchProgress" takes no "exitProgress".` | A custom effect with a `switchProgress` above `0` has an `exitProgress`. |
| `A transition effect needs an "exitProgress" between 0 and 1.` | The `exitProgress` of a custom effect is out of range. |
| `bad argument #1 to 'popTo' (expected a non-negative integer)` | The `level` of `popTo` is negative. |
| `The scene is already loaded or on the stack.` | The scene of `preload` is loading, loaded or on the stack. |
| `The scene is not preloaded.` | The scene of `cancelPreload` is not a preloaded scene. |
| `An owner must be a table or a userdata, not <type>.` | The owner of `listen` or `spawn` is another type. |
| `bad argument #2 to 'listen' (signal or event name expected, got <type>)` | The source of `listen` is neither a signal nor a string. |
| `bad argument #2 to 'spawn' (function expected, got <type>)` | The function of `spawn` is not a function. |
| `A load progress runs from 0 to 1.` | The value of `context:progress` is out of range. |
| `The asset group "<name>" is not defined.` | The method `context:preload` names a group that does not exist. |
| `The scene load is over.` | The method `context:preload` ran after the load ended. |
| `This "haylen.SceneLoad" was already released.` | The code used a load context after its scene entered or unloaded. |
| `The type "haylen.SceneLoad" has no member "<name>".` | The code read a member that a load context does not have. |
| `A Lua scene takes the "params" of a change from Lua.` | A change from C++ gave a Lua scene `params` that are not a Lua value. Reading `context.params` raises it, and so does the `enter` of the scene, which stops the app. |

These messages stop the app when a change starts, and the promise of the change rejects with them.

| Message | Cause |
| --- | --- |
| `The scene is already on the stack.` | A `push` or `replace` brings a scene that is on the stack or still leaving it. |
| `A preloaded scene keeps the "params" of its preload.` | A `push` or `replace` gives `params` to a preloaded scene. |

These messages fail a load.

| Message | Cause |
| --- | --- |
| `The asset group "<name>" could not load <path>: <reason>.` | An asset of a group of `context:preload` failed to load. |
| The message of the error | The hook `load` raised an error, or the promise it returned rejected. |
