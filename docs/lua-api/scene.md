# haylen.scene

The module `haylen.scene` manages the stack of scenes that make up the app, such as the title screen, a level, a pause menu or a dialog. A scene is a Lua table with optional hooks. Every scene loads before it enters, asynchronously when it needs to, and unloads after it exits, and every change of the stack runs through one pipeline: the transition covers the current scene, the next scene loads, optionally behind a loading view, enters, and the transition reveals it. Only the top scene receives input and updates, when its process mode lets it run, while scenes below a transparent scene keep rendering, which is how overlays such as pause menus work. The [lifecycle guide](../lifecycle.md) explains the whole pipeline with its order of hooks and events, the pause and the process modes.

```lua
local scene = require('haylen.scene')
```

## Scene tables

A scene is any table. The engine looks up its hooks by name every time it calls them, so methods inherited through a metatable or [`haylen.class`](../lua.md#classes) work, and every hook receives the scene table as `self`. All hooks are optional.

| Field | Called | Arguments |
| --- | --- | --- |
| `load` | Before the scene enters, as a task the scene owns, so it may wait on promises with `:await()`. The load ends when the function returns, or when the promise it returns settles, and when every group it preloads loaded. | `self` and the [load context](#load-context). |
| `enter` | When the scene enters the stack through `push` or `replace`. | `self` and the `params` of the change. |
| `enterTransitionFinished` | On the top scene once a change ended and its transition finished, when input reaches it again. | `self` |
| `exitTransitionStarted` | On the top scene when a change starts to take it off the screen. | `self` |
| `exit` | When the scene leaves the stack through `pop`, `popTo`, `popToRoot`, `replace` or `clear`. | `self` |
| `unload` | Right after `exit`, when the load failed or when a preload is cancelled. It releases what the scene loaded, so it also handles a load that stopped halfway. | `self` |
| `pause` | When another scene is pushed on top of it. | `self` |
| `resume` | When the scenes above it are popped and it is on top again. | `self` |
| `paused` | When the game pause stops the scene, by its process mode. | `self` |
| `unpaused` | When the game pause lets the scene run again, by its process mode. | `self` |
| `event` | For every platform event, on the top scene only. Input events arrive only while its process mode runs and no change holds input back. | `self` and the event table described in [Events](#events). |
| `fixedUpdate` | On the top scene, zero or more times per frame, once for each fixed step, while its process mode runs. | `self` and the fixed step length in seconds, which is `haylen.fixedStep()`. |
| `update` | On the top scene, once per frame, while its process mode runs and no scene covers it. | `self` and the scaled frame duration in seconds. |
| `render` | On every visible scene, from bottom to top. | `self` |
| `renderUi` | On every visible scene, from bottom to top, after every `render` call. | `self` |
| `transparent` | Read every frame. When it is `true`, the scene below also renders. | Not a function. |
| `processMode` | Read every frame. One of `'inherit'`, the default, `'pausable'`, `'whenPaused'`, `'always'` or `'disabled'`. | Not a function. |

The visible scenes are the top scene and every scene below it down to the first one that is not transparent. An error raised in a hook stops the app and shows the error screen with the message and its stack trace, except in `load`, where it fails the load as [Errors](#errors) describes. Everything the scene owns ends when it unloads: the tasks of `scene.spawn`, the listeners of `scene.listen`, and the timers, tweens, event listeners, signal connections and UI documents created with the scene as `owner`. So no callback or coroutine of a scene runs once it is gone.

A table is one scene for as long as the engine holds it, so the same table cannot be on the stack twice, and a table whose scene unloaded may be pushed again, which loads it again.

```lua
local scene = require('haylen.scene')
local graphics2d = require('haylen.graphics2d')

local Level = {}
Level.__index = Level

function Level.new(name)
    return setmetatable({name = name, elapsed = 0}, Level)
end

function Level:enter(params)
    print('entering ' .. self.name .. ' on ' .. params.difficulty)
end

function Level:update(dt)
    self.elapsed = self.elapsed + dt
end

function Level:paused()
    print('the game paused')
end

function Level:render()
    graphics2d.beginScreen()
    graphics2d.drawText(nil, self.name .. string.format(' %.1f', self.elapsed), 80, 80, {size = 48, color = '#FFFFFFFF'})
end

scene.push(Level.new('Forest'), {params = {difficulty = 'hard'}})
```

## States

Every scene goes through the same states, which `scene.state(scene)` returns.

| State | Meaning |
| --- | --- |
| `'created'` | A change or a preload holds the scene, which has not started loading. |
| `'loading'` | Its `load` runs. |
| `'loaded'` | It loaded and waits to enter, such as a preloaded scene. |
| `'entering'` | It entered, or came back to the top, and the transition still reveals it. |
| `'active'` | It is on the stack and its transition finished. |
| `'covered'` | Another scene was pushed on top of it. |
| `'exiting'` | It leaves the stack and the transition still takes it off the screen. |
| `'exited'` | It left the stack and unloads next. |
| `'unloaded'` | It unloaded, and it can load again. |

## Load context

The hook `load` receives a context, a `haylen.SceneLoad` userdata, which lives until the scene enters or unloads. Using it later raises `This "haylen.SceneLoad" was already released.`

| Member | Meaning |
| --- | --- |
| `context.params` | The `params` of the change that loads the scene, or of its preload. |
| `context:progress(value, message)` | Reports the progress of the work the scene does itself, from 0 to 1, with an optional message for the loading view. A value outside that range raises `A load progress runs from 0 to 1.` |
| `context:preload(groups)` | Loads one [preload group of `haylen.assets`](assets.md#preload-groups), or a list of them, and holds the load until they loaded. It returns a promise that resolves with `true` once all of them loaded. An asset that fails fails the load, and the promise rejects with the error. Calling it once the load is over raises `The scene load is over.` |

The progress of the load, which loading views receive and `scene.loadProgress` returns, is the mean of the progress the scene reports and of the progress of each group it preloads. Heavy work stays in the background, since the asset manager decodes on the worker pools and creates the GPU resources of the decoded assets within the [upload budget](assets.md#assetssetuploadbudgetseconds) of each frame.

```lua
local scene = require('haylen.scene')
local assets = require('haylen.assets')
local async = require('async')

assets.defineGroup('forest', {'images/'})

local forest = {}

function forest:load(context)
    context:progress(0, 'reading the save')
    self.save = {day = context.params.day}
    async.sleep(100):await()
    context:progress(0.5, 'planting trees')
    context:preload('forest'):await()
    self.trees = 12
end

function forest:enter()
    print('day ' .. self.save.day .. ' with ' .. self.trees .. ' trees')
end

function forest:unload()
    self.trees = nil
end

scene.push(forest, {duration = 0.6, params = {day = 3}})
```

A `load` that returns a promise ends when the promise settles, so a load can hand its work to a promise of Varn instead of waiting in its own coroutine.

```lua
local scene = require('haylen.scene')
local assets = require('haylen.assets')

scene.push({
    load = function(self)
        return assets.loadAsync('data/level.json')
    end,
})
```

## Changes

The functions `push`, `replace`, `pop`, `popTo` and `popToRoot` request a change and return a promise. A change starts at the next scene update, and changes requested while another one runs wait for it and run in request order. The promise resolves with `true` once the change ended, or with `false` when `scene.clear` dropped it, and it rejects with the error when the next scene failed to load, so a coroutine can wait for it with `:await()`, which returns `nil` and the error then.

Every change takes an optional table of options. Unknown keys raise `Unknown option "<key>".`, and `pop`, `popTo` and `popToRoot` take only the transition keys and `onComplete`, since nothing loads.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `duration` | number | `0` | Length of the whole transition in seconds, without the time it holds for the load. Without a duration the change switches at once and draws nothing. |
| `ease` | curve | `'linear'` | Easing of the progress, in any form of the [easing curves of `haylen.tween`](tween.md#easing). |
| `effect` | string or table | `'fade'` | The name of a [built-in effect](#built-in-effects), or a table that draws its own, as [Custom effects](#custom-effects) describes. |
| `direction` | string | `'left'` | Where the motion, wipe, wave or page turn of a built-in effect goes: `'left'`, `'right'`, `'up'`, `'down'`, `'upLeft'`, `'upRight'`, `'downLeft'` or `'downRight'`. |
| `color` | Color | `'#FF000000'` | The color that fades and irises pass through, which also shows behind effects that uncover the screen, given as a `Color`, a `'#AARRGGBB'` string or a color table of `haylen.math`. |
| `blockInput` | boolean | `true` | Holds input back from the scenes and the action map until the change ends, from the start of the load to the end of the reveal. Actions whose bindings are still held when the change ends stay up until they are released. |
| `onComplete` | function | `nil` | Called with `true` once the change ended, or with `false` when it was dropped or failed. |
| `params` | any | `nil` | Reaches the `load` and `enter` hooks of the next scene. A preloaded scene keeps the `params` of its preload. |
| `loading` | table | `nil` | A [loading view](#loading-views) to show while the next scene loads. |
| `loadingDelay` | number | `0` | Seconds the load has to take before the loading view appears, so quick loads never flash it. |
| `minimumLoadingTime` | number | `0` | Seconds the loading view stays once it appeared, even when the load finishes earlier. |
| `loadingFadeOut` | number | `0.25` | Seconds a loading view over the covered frame of an effect that covers the screen takes to fade out into it once the load is done and the view stayed its minimum time. The value `0` takes the view away at once. |
| `unloadBeforeLoad` | boolean | `true` | With an effect that covers the screen, the replaced scene exits and unloads at full cover, before the next scene loads, for a lower peak of memory. Turned off, the next scene loads while the effect covers the screen and the replaced scene stays loaded until the next scene loaded. |
| `onError` | function | `nil` | Receives the message of a failed load instead of the log or the error screen, and may route the app to another scene. |

A negative `loadingDelay`, `minimumLoadingTime` or `loadingFadeOut` raises `The loading delay, the minimum loading time and the loading fade-out cannot be negative.`, a custom effect table with `direction` or `color` raises `A custom transition effect takes no color or direction.`, and an effect of another type raises `The transition effect must be the name of a built-in effect or a table with a "render" method.` Transitions and loads run on real time, so they go on at full speed while the game is paused or the time scale is zero.

### Effects that cover the screen

The fade and the other effects that show one scene at a time run in three phases.

1. **Cover**: The top scene receives `exitTransitionStarted` and the effect takes it off the screen, while it still updates and renders. The next scene of a `push` starts loading now, and so does the next scene of a `replace` that keeps its scene loaded.
2. **Hold**: At full cover the stack changes. A replaced scene exits and unloads, and then the next scene starts loading, a pushed-over scene receives `pause`, and popped scenes exit and unload while the scene below them receives `resume`. The effect holds the covered frame, drawing no scene at all, until the next scene loaded, with the loading view over it once the delay passed, which then fades out into the covered frame.
3. **Reveal**: The next scene receives `enter` and the rest of the effect shows it. Once the effect ends, it receives `enterTransitionFinished`.

The transition itself is the loading screen when the change has no loading view: the covered frame, such as the color of a fade, stays until the load is done.

```lua
local scene = require('haylen.scene')
local async = require('async')

local cave = {
    load = function(self)
        async.sleep(500):await()
    end,
    enter = function(self)
        print('the cave appears once the fade covered the screen and the cave loaded')
    end,
}

scene.push({name = 'forest'})
scene.replace(cave, {duration = 0.8, color = '#FF1B1E2B'})
```

### Effects that show both scenes

Crossfades, slides, pushes and the other effects that show both scenes at once load the next scene first, while the current scenes stay on the screen and keep running. Then the top scene receives `exitTransitionStarted`, the stack changes and the next scene enters, and the effect plays with both scenes alive, each captured into its own image every frame. The scenes that leave the stack exit and unload at the exit point of the effect, its end for the built-in effects, in the update that reaches it and before that frame renders, so nothing draws them after they exit.

```lua
local scene = require('haylen.scene')

scene.push({name = 'map'})
scene.push({name = 'inventory', load = function(self) self.items = {'rope', 'lamp'} end}, {effect = 'slideIn', direction = 'up', duration = 0.4, ease = 'quadOut'})
```

### Without a transition

A change without a duration loads the next scene while the current scenes stay on the screen, with the loading view over them once the delay passed, and then switches within one update: `exitTransitionStarted` on the top scene, the exits and unloads, `pause` or `resume`, `enter` and `enterTransitionFinished`.

```lua
local scene = require('haylen.scene')

scene.push({name = 'title'})
scene.replace({name = 'credits', load = function(self) self.lines = {'art', 'music', 'code'} end})
```

### Loading views

A loading view is a table with optional hooks, the same ones as a scene, so an ordinary scene table can serve as one. It appears once the load took longer than `loadingDelay`, over the covered frame during the hold of an effect that covers the screen, or over the current scenes before an effect that shows both scenes or a change without an effect. It stays at least `minimumLoadingTime` once it appeared. Over the covered frame it then fades out into it over `loadingFadeOut` seconds, with the engine blending everything it draws, its UI documents included, so the view needs no fade of its own, and over the current scenes it goes away at once. It exits right before the next scene enters. It draws like the top scene, before the drawing of the engine plugins such as UI documents, it runs on real time, and everything it owns ends when it exits.

| Field | Called | Arguments |
| --- | --- | --- |
| `enter` | When the view appears. | `self` |
| `exit` | When the view goes away. | `self` |
| `update` | Once per frame while it shows. | `self`, the real frame duration in seconds, the progress of the load from 0 to 1 and its message. |
| `render`, `renderUi` | Every frame while it shows. | `self`, the progress and the message. |

Ordinary loading scenes remain possible too: a scene on the stack can preload the next scene with `scene.preload`, draw `scene.loadProgress` and replace itself once the preload resolved.

```lua
local scene = require('haylen.scene')
local graphics2d = require('haylen.graphics2d')
local async = require('async')

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
            context:progress(step / 10, 'building the world')
            async.sleep(100):await()
        end
    end,
}

scene.push({name = 'menu'})
scene.replace(world, {duration = 0.6, loading = bar, loadingDelay = 0.2, minimumLoadingTime = 0.5, loadingFadeOut = 0.3})
```

### Errors

An error raised in `load`, a promise it returned that rejected, or an asset of a group it preloads that failed fails the load. The scene unloads, the event `sceneLoadFailed` announces it, and the change ends without it: its promise rejects with the error and `onComplete` receives `false`.

- When the scene on top before the change is still there, the change keeps it. An effect that covers the screen reveals it again, and it receives `enterTransitionFinished`. The failure goes to `onError` when the change has one, and to the log otherwise.
- When that scene already unloaded, because a replace unloaded it before the load, the failure goes to `onError`, which may route the app to another scene, and to the error screen without one. The first scene of the app has nothing to fall back to either.

```lua
local scene = require('haylen.scene')

local fallback = {name = 'offline'}
local online = {
    load = function(self)
        error('the server is not reachable')
    end,
}

scene.push({name = 'title'})
scene.replace(online, {duration = 0.5, onError = function(message)
    print('could not load: ' .. message)
    scene.replace(fallback)
end})
```

### Built-in effects

| Effect | What it shows | Direction | Kind |
| --- | --- | --- | --- |
| `'fade'` | The outgoing scene fades to the color and the incoming one fades back from it. | None. | Covers the screen. |
| `'crossFade'` | The outgoing scene fades into the incoming one. | None. | Shows both scenes. |
| `'moveIn'` | The incoming scene moves in toward the direction and covers the outgoing one, which stays still. | Any. | Shows both scenes. |
| `'slideIn'` | The incoming scene slides in toward the direction while the outgoing one slides a third of the way and darkens, like a navigation stack. | Any. | Shows both scenes. |
| `'push'` | Both scenes move together toward the direction, the incoming one pushing the outgoing one out. | Any. | Shows both scenes. |
| `'shrinkGrow'` | The outgoing scene shrinks away while the incoming one grows in, over the color. | None. | Shows both scenes. |
| `'flipX'` | The outgoing scene turns in perspective around the vertical axis and the incoming one turns in on its back, over the color. | Left or right. | Covers the screen. |
| `'flipY'` | The same turn around the horizontal axis. | Up or down. | Covers the screen. |
| `'zoomFlip'` | A flip that moves away while it turns, around the vertical axis for left and right and the horizontal axis for up and down. | Any. | Covers the screen. |
| `'rotoZoom'` | The outgoing scene spins twice and shrinks away, and the incoming one spins and grows back. | None. | Covers the screen. |
| `'jumpZoom'` | The outgoing scene shrinks and jumps out toward the direction, and the incoming one jumps in and grows. | Any. | Covers the screen. |
| `'splitColumns'` | The outgoing scene splits into three columns that slide up and down out of view, and the incoming one closes back from them. | Up or down, for the first column. | Covers the screen. |
| `'splitRows'` | The same with three rows that slide left and right. | Left or right, for the first row. | Covers the screen. |
| `'turnOffTiles'` | The tiles of the outgoing scene turn off in random order and uncover the incoming one. | None. | Shows both scenes. |
| `'fadeTiles'` | The tiles of the outgoing scene shrink away in a wave that travels toward the direction, such as `'upRight'` for the classic fade toward the top right. | Any. | Shows both scenes. |
| `'pageTurn'` | The outgoing scene curls away like a page turned toward the direction and shows its paper back. | Any. | Shows both scenes. |
| `'radialClockwise'` | A hand sweeps clockwise from twelve o'clock and uncovers the incoming scene. | None. | Shows both scenes. |
| `'radialCounterclockwise'` | The same sweep counterclockwise. | None. | Shows both scenes. |
| `'wipe'` | An edge moves toward the direction and uncovers the incoming scene. | Any. | Shows both scenes. |
| `'inOut'` | The incoming scene grows out of the center. | None. | Shows both scenes. |
| `'outIn'` | The outgoing scene shrinks into the center and uncovers the incoming one. | None. | Shows both scenes. |
| `'iris'` | A circle closes on the outgoing scene to the color and opens on the incoming one. | None. | Covers the screen. |
| `'dissolve'` | The incoming scene appears in random order, a few pixels at a time. | None. | Shows both scenes. |
| `'pixelate'` | The outgoing scene breaks into growing blocks and the incoming one comes back out of them. | None. | Covers the screen. |

The effects that cover the screen do so halfway. An unknown name raises `The option "effect" of "replace" is invalid: unknown value '<name>'.`, and the direction raises the same for an unknown direction. Every effect runs through the eased progress, so `ease` shapes the motion of the effects that move.

```lua
local scene = require('haylen.scene')
local graphics2d = require('haylen.graphics2d')

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

The drawing of the engine plugins, such as mounted UI documents, autoloads and the debug overlay, goes with the current scenes: into the outgoing image during the cover, over the covered frame during the hold, and into the incoming image during the reveal. A UI document mounted with the scene as its `owner` goes away when the scene unloads, and a covered scene hides the documents it keeps in `pause`.

### Custom effects

An effect is a table, which may be an instance of a class, with a `render` method and optional `switchProgress` and `exitProgress` fields. Every frame of the transition, `render(effect, progress, outgoing, incoming)` draws over the whole frame with the eased progress from 0 to 1 and the two images as [textures of `haylen.graphics`](graphics.md), usually on a screen canvas of [`haylen.graphics2d`](graphics2d.md) that it begins itself, where the visible area covers each image exactly.

The field `switchProgress`, `0.5` by default, is where the effect covers the whole screen: the stack changes there and the effect holds there while the next scene loads, drawing with the last frame of the outgoing scenes. An effect that shows both scenes throughout uses a `switchProgress` of `0`, and its `exitProgress`, `1` by default, is where the leaving scenes exit, after which the outgoing image keeps their last frame. The incoming image stays empty until the next scene enters. The call raises `A transition effect needs a "render" method.`, `A transition effect needs a "switchProgress" between 0 and 1.`, `A transition effect that covers the screen at its "switchProgress" takes no "exitProgress".` or `A transition effect needs an "exitProgress" between 0 and 1.` for effects that break these rules.

```lua
local scene = require('haylen.scene')
local graphics2d = require('haylen.graphics2d')

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
scene.replace({name = 'battle', enter = function() print('the battle begins') end}, {duration = 0.8, ease = 'quadInOut', effect = door})
```

## Functions

### scene.push(scene, options)

Puts `scene` on top of the stack once it loaded and returns a promise. The current top scene receives `pause` and the new scene `enter`. The argument `scene` must be a table, otherwise the call raises `bad argument #1 to 'push' (table expected, got string)` or the equivalent for the given type, and a `processMode` other than the five modes raises an error at once. A scene that is still on the stack when the change starts stops the app with `The scene is already on the stack.`

```lua
local scene = require('haylen.scene')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

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

Removes the top scene, puts `scene` in its place once it loaded and returns a promise. The old scene receives `exit` and `unload`, before the new scene loads with an effect that covers the screen, and at the exit point of an effect that shows both scenes. On an empty stack it behaves like `push`.

```lua
local scene = require('haylen.scene')
local async = require('async')

local game = {enter = function(self) print('game started') end}
local title = {}

scene.push(title)
async.spawn(function()
    local done = scene.replace(game, {duration = 0.6, color = '#FF1B1E2B'}):await()
    print('replaced', done)
end)
```

### scene.pop(options)

Removes the top scene and returns a promise. The scene below, if any, receives `resume`, and the removed scene receives `exit` and `unload`. Popping an empty stack does nothing.

```lua
local scene = require('haylen.scene')

local town = {resume = function(self) print('back in town') end}
local shop = {exit = function(self) print('leaving the shop') end}

scene.push(town)
scene.push(shop)
scene.pop({duration = 0.3, color = '#FFFFFFFF', onComplete = function(done) print('shop closed', done) end})
```

### scene.popTo(level, options)

Removes scenes from the top until `level` scenes remain and returns a promise. The scene left on top receives `resume`, and every removed scene receives `exit` and `unload`, from the top down. A stack with `level` scenes or fewer stays as it is, and the promise resolves at once.

```lua
local scene = require('haylen.scene')

scene.push({name = 'title'})
scene.push({name = 'world map', resume = function(self) print('back on the world map') end})
scene.push({name = 'dungeon'})
scene.push({name = 'inventory'})

-- Leaving the dungeon closes the inventory too.
scene.popTo(2, {duration = 0.4})
```

### scene.popToRoot(options)

Removes every scene except the bottom one and returns a promise, like `scene.popTo(1, options)`.

```lua
local scene = require('haylen.scene')

local title = {resume = function(self) print('title screen again') end}
scene.push(title)
scene.push({name = 'level'})
scene.push({name = 'game over'})

scene.popToRoot({duration = 0.5})
```

### scene.preload(scene, params)

Starts loading `scene` in the background with its `load` hook and `params`, without changing the stack, and returns a promise that resolves with `true` once it loaded, with `false` when the preload was cancelled or cleared, or rejects with the error of a failed load. A later `push` or `replace` of the scene takes it at once, or waits only for the rest of its load, and it keeps the `params` of its preload: a change that brings it with `params` stops the app with `A preloaded scene keeps the "params" of its preload.` Preloading a scene that is loaded or on the stack raises `The scene is already loaded or on the stack.`

```lua
local scene = require('haylen.scene')
local async = require('async')
local graphics2d = require('haylen.graphics2d')

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
            scene.preload(level, 'slot 1'):await()
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

Unloads a preloaded scene that no change took, stopping its load when it still runs, and resolves the promise of its preload with `false`. A scene that is not preloaded raises `The scene is not preloaded.`

```lua
local scene = require('haylen.scene')
local async = require('async')

local bonus = {load = function(self) async.sleep(5000):await() end}
scene.preload(bonus)
scene.cancelPreload(bonus)
print(scene.state(bonus)) -- unloaded
```

### scene.clear()

Removes every scene right away: the scenes on the stack receive `exit` and `unload`, from the top down, then the scenes still leaving through a transition, and then the next scene of a pending change and the preloaded scenes receive `unload`. The loading view goes away, and pending changes and preloads are dropped, including changes that those hooks request, whose promises resolve with `false`. Every scene leaves even when a hook raises an error, and the first error is raised again once the stack is empty. It is the way to start over, and it works from any hook, even the `enter` of a scene that is being pushed.

```lua
local scene = require('haylen.scene')

local title = {enter = function(self) print('title screen') end}

local function backToTitle()
    scene.clear()
    scene.push(title)
end

backToTitle()
```

### scene.size()

Returns the number of scenes on the stack. Requested changes count only once they apply.

```lua
local scene = require('haylen.scene')

scene.push({})
require('haylen.timer').after(0.5, function()
    print('scenes on the stack:', scene.size())
end)
```

### scene.top()

Returns the table of the top scene, the same table that was pushed, or `nil` when the stack is empty or the top scene was pushed from C++.

```lua
local scene = require('haylen.scene')

local battle = {name = 'battle'}
scene.push(battle)

require('haylen.timer').after(0.5, function()
    print(scene.top() == battle)
end)
```

### scene.at(index)

Returns the table of the scene at `index`, counted from 1 at the bottom of the stack, `false` for a scene pushed from C++, or `nil` past either end.

```lua
local scene = require('haylen.scene')

scene.push({name = 'title'})
scene.push({name = 'level'})

require('haylen.timer').after(0.5, function()
    print(scene.at(1).name, scene.at(2).name, scene.at(3)) -- title level nil
end)
```

### scene.list()

Returns a sequence with the tables of every scene from the bottom of the stack to the top, with `false` in place of scenes pushed from C++.

```lua
local scene = require('haylen.scene')

scene.push({name = 'title'})
scene.push({name = 'settings'})

require('haylen.timer').after(0.5, function()
    for index, entry in ipairs(scene.list()) do
        print(index, entry.name)
    end
end)
```

### scene.transitioning()

Returns `true` while a change is waiting to start, loads its next scene or plays its transition.

```lua
local scene = require('haylen.scene')
local input = require('haylen.input')

input.loadActions({actions = {{name = 'confirm', type = 'button', bindings = {'key:enter', 'button:south'}}}})

scene.push({
    update = function(self, dt)
        if not scene.transitioning() and input.pressed('confirm') then
            scene.replace({}, {duration = 0.5})
        end
    end,
})
```

### scene.loadingViewOpacity()

Returns how much of the loading view of the pending change shows: `1` while it shows, falling to `0` while it fades out into the covered frame, and `0` without a view. A view that plays sounds can fade them with it.

```lua
local scene = require('haylen.scene')
local audio = require('haylen.audio')
local assets = require('haylen.assets')

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
scene.replace({load = function() require('async').sleep(1000):await() end}, {duration = 0.6, loading = view})
```

### scene.state(scene)

Returns the [state](#states) of the scene of a table, `'unloaded'` for a table whose scene the engine no longer holds, or `nil` for a table that never was a scene.

```lua
local scene = require('haylen.scene')

local level = {}
print(scene.state(level)) -- nil
scene.push(level)
print(scene.state(level)) -- created
require('haylen.timer').after(0.5, function()
    print(scene.state(level)) -- active
end)
```

### scene.loadProgress(scene)

Returns the progress of the load of a scene, from 0 to 1, and its message: the running load of a scene that loads, 1 once it loaded, and 0 before it starts loading or after it unloaded.

```lua
local scene = require('haylen.scene')
local async = require('async')

local level = {load = function(self, context) context:progress(0.25, 'reading the map') async.sleep(200):await() end}
scene.preload(level)
local value, message = scene.loadProgress(level)
print(value, message) -- 0.25 reading the map
```

### scene.listen(owner, source, fn, options)

Connects `fn` to a signal of [`haylen.signal`](signal.md), or subscribes it to the events named by a string on [`haylen.events`](events.md), for as long as `owner` lives, and returns the [`Connection`](signal.md#connection). The argument `options` takes the options of `sig:connect` or `events.on`. A scene owner ends its listeners when it unloads. The argument `owner` must be a table or a userdata, otherwise the call raises `An owner must be a table or a userdata, not <type>.`

```lua
local scene = require('haylen.scene')
local signal = require('haylen.signal')
local events = require('haylen.events')

local scoreChanged = signal.new('scoreChanged')

local hud = {}
function hud:enter()
    scene.listen(self, scoreChanged, function(score) print('score', score) end)
    scene.listen(self, 'playerDied', function() print('game over') end, {once = true})
end

scene.push(hud)
require('haylen.timer').after(0.1, function()
    scoreChanged:emit(120)
    events.emit('playerDied')
end)
```

### scene.spawn(owner, fn)

Runs `fn` as a task that `owner` holds, the way `async.spawn` runs a task: it may wait on promises with `:await()`, and an error it raises reaches the error screen with the stack of the task. When the owner is released, such as a scene when it unloads, the task stops for good and its pending to-be-closed variables close, so it never resumes, even when a promise it waits for settles later. A task that releases its own owner stops at its next wait. Tasks started with `async.spawn` belong to nobody and run to their end unless `task.cancel()` of the handle it returns stops them. The argument `owner` must be a table or a userdata.

```lua
local scene = require('haylen.scene')
local async = require('async')
local platform = require('haylen.platform')

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

The table `scene.Scene` is a class of [`haylen.class`](../lua.md#classes) to build scenes on. Its instances have two methods: `self:listen(source, fn, options)`, which calls `scene.listen` with the scene as the owner, and `self:spawn(fn)`, which calls `scene.spawn` with the scene as the owner.

```lua
local haylen = require('haylen')
local scene = require('haylen.scene')
local events = require('haylen.events')
local async = require('async')

local Level = haylen.class('Level', scene.Scene)

function Level:init(name)
    self.name = name
end

function Level:enter()
    self:listen('checkpoint', function(id) print(self.name .. ' checkpoint ' .. id) end)
    self:spawn(function()
        async.sleep(1000):await()
        print(self.name .. ' has run for a second')
    end)
end

scene.push(Level('Forest'))
require('haylen.timer').after(0.1, function() events.emit('checkpoint', 2) end)
```

## Events

The `event` hook receives one table per platform event. Its `type` field names the event, and positions are in design units, the same space the app draws in. Lifecycle changes such as the app going to the background, and the lifecycle of scenes and of every change, also reach the listeners of [`haylen.events`](events.md#engine-events), which is the better place to react to them.

| `type` | Extra fields |
| --- | --- |
| `'keyDown'`, `'keyUp'` | `key` (key name such as `'space'`, `'a'`, `'escape'` or `'left'`), `repeat` (`true` for auto-repeated key downs), `modifiers` (a table with the booleans `shift`, `control`, `alt` and `super` for the modifier keys held during the event). |
| `'character'` | `character` (the typed text as a UTF-8 string), `modifiers`. |
| `'mouseDown'`, `'mouseUp'` | `button` (`'left'`, `'right'` or `'middle'`), `x`, `y`, `modifiers`. |
| `'mouseMove'` | `x`, `y`, `dx`, `dy` (the movement of this event, also while the mouse is locked), `modifiers`. |
| `'mouseScroll'` | `scrollX`, `scrollY`, `modifiers`. |
| `'mouseEnter'`, `'mouseLeave'` | None. |
| `'touchBegan'`, `'touchMoved'`, `'touchEnded'`, `'touchCancelled'` | `touches`, a sequence of `{id, x, y, changed}` tables where `changed` marks the touches that caused the event, `modifiers`. |
| `'resized'` | None. |
| `'suspended'`, `'resumed'` | None. The app went to the background or came back. |
| `'focusGained'`, `'focusLost'` | None. |
| `'quitRequested'` | None. The player asked to close the window. |
| `'lowMemory'` | None. The platform is short of memory. |
| `'textEdited'` | `field`, the id of the text field of the UI, and `text`, what its native field holds now. The UI applies these edits itself. |
| `'textAction'` | `field` and `action`: `'submit'`, `'next'`, `'cancel'` or `'dismissed'`, which the UI applies to its text field itself. |
| `'keyboardChanged'` | `frame`, the rectangle the on-screen keyboard covers in design units, empty while it is hidden. |
| `'networkChanged'` | `online`, whether the device has a network. |
| `'interruptionBegan'`, `'interruptionEnded'` | None. The system interrupted the app, such as for a phone call, or gave it back. |
| `'windowMoved'`, `'monitorsChanged'` | None. The desktop window moved, or the monitors of the desktop changed, as the `windowMoved` and `windowMonitorsChanged` events of [`haylen.events`](events.md#engine-events) report with their details. |

The full list of key names is in the `haylen.input` reference. Gameplay should read input through the action map of `haylen.input`, and events are best for text entry and pointer tracking.

```lua
local scene = require('haylen.scene')

local nameEntry = {text = ''}

function nameEntry:event(event)
    if event.type == 'character' then
        self.text = self.text .. event.character
    elseif event.type == 'keyDown' and event.key == 'backspace' and #self.text > 0 then
        self.text = self.text:sub(1, utf8.offset(self.text, -1) - 1)
    elseif event.type == 'mouseDown' then
        print('clicked at', event.x, event.y, 'with', event.button)
    elseif event.type == 'touchBegan' then
        for _, touch in ipairs(event.touches) do
            if touch.changed then
                print('finger', touch.id, 'at', touch.x, touch.y)
            end
        end
    end
end

scene.push(nameEntry)
```
