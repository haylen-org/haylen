# haylen.imgui

The module `haylen.imgui` exposes Dear ImGui immediate mode windows and widgets to Lua. The app calls it every frame to rebuild windows from its current state, and the widgets return what the player changed. Use it for debug panels, cheat menus, level tweaking and tools. For the interface players see, use the retained, themed components of [`haylen.ui`](ui.md).

```lua
local imgui = require('haylen.imgui')
```

## Frames and windows

Every call must happen while a frame is running, which covers the `update`, `fixedUpdate`, `render` and `renderUi` callbacks of scenes, timer and tween callbacks and [`haylen.ui`](ui.md) handlers. A call anywhere else, such as at the top level of `source/main.lua` or in a callback of [`haylen.platform`](platform.md), raises `The module "haylen.imgui" can only be used while a frame is running.`. The `renderUi` callback of a scene is the usual place.

Positions and sizes are design units, with the origin at the top left of the visible area. Windows are drawn over the app and use the colors, metrics and body font of the active [`haylen.ui`](ui.md) theme, and their scroll bars keep the [gap](ui.md#scroll-bars) of the theme from their content. While the pointer is over a window, `ui.usingPointer()` returns `true`.

Widgets are identified by their label within the current window. Text after `##` in a label is part of the identity but is not shown, so `'Delete##slot1'` and `'Delete##slot2'` are two buttons that both read `Delete`. The function `imgui.pushId` scopes the identities of the widgets of a loop.

Misuse that Dear ImGui detects, such as an `endWindow` without a `beginWindow`, raises `The Dear ImGui check "<check>" failed at "<file>:<line>".` in the call that caused it. The functions `imgui.tableNextRow` outside a table and `imgui.treePop` without an open tree node raise errors of their own, listed with those functions. An error inside a scene callback shows the error screen, and the next frame closes whatever the failed script left open.

A `beginWindow` whose `endWindow` never comes is found only when the frame draws the interface, after every script of the frame has run. It stops the app with `The Dear ImGui check "(0) && "Missing End()"" failed at "<file>:<line>".`, where `Missing EndChild()` or `Missing EndTable()` takes the place of `Missing End()` when a region or a table inside that window was left open too. That error carries no Lua stack trace, so the code to look at is a path that begins a window without ending it. A `beginChild`, `beginTable`, `beginTabBar`, `treeNode` or `pushId` left open inside a window that does end is reported by its `endWindow` instead, with the stack trace of the script.

Functions that edit a value take the current value and return two values: `true` when the player changed it this frame, and the new value. The app keeps the value and passes it back on the next frame.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    speed = 120,
    godMode = false,
    renderUi = function(self)
        if imgui.beginWindow('Cheats', {x = 20, y = 20, width = 420, height = 300}) then
            local changed
            changed, self.speed = imgui.sliderFloat('Speed', self.speed, 0, 400)
            changed, self.godMode = imgui.checkbox('God mode', self.godMode)
            if imgui.button('Heal') then
                print('healed')
            end
        end
        imgui.endWindow()
    end,
})
```

## Windows

### imgui.beginWindow(name, options)

Starts a window and returns two booleans: whether its content is visible, and whether it is still open. The content is not visible while the window is collapsed. The argument `name` is the title and the identity of the window. The function `imgui.endWindow` must follow every `beginWindow`, whatever it returned. The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `x`, `y` | number | none | Position of the top left corner the first time the window appears. It applies only when both are given. |
| `width`, `height` | number | none | Size the first time the window appears. It applies only when both are given. |
| `closable` | boolean | `false` | Adds a close button. The second result is `false` in the frame the player presses it. |
| `noTitleBar` | boolean | `false` | Hides the title bar. |
| `noResize` | boolean | `false` | Stops the player from resizing the window. |
| `noMove` | boolean | `false` | Stops the player from moving the window. |
| `autoResize` | boolean | `false` | Sizes the window to its content every frame. |

A closable window keeps showing for as long as the app calls `beginWindow`, so the app remembers that the player closed it.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    showStats = true,
    renderUi = function(self)
        if not self.showStats then
            return
        end
        local visible, open = imgui.beginWindow('Stats', {x = 1400, y = 20, closable = true, autoResize = true, noMove = true})
        if visible then
            imgui.text('Enemies 12')
        end
        imgui.endWindow()
        self.showStats = open
    end,
})
```

### imgui.endWindow()

Ends the window `beginWindow` started.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Log')
        imgui.text('App started')
        imgui.endWindow()
    end,
})
```

### imgui.setNextWindowPos(x, y, always)

Sets the position of the top left corner of the next window. Without `always`, or with `false`, it applies only the first time the window appears, and the player can move it afterwards. With `true` it applies every frame.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.setNextWindowPos(20, 900, true)
        imgui.beginWindow('Pinned', {noTitleBar = true, autoResize = true})
        imgui.text('Build 42')
        imgui.endWindow()
    end,
})
```

### imgui.setNextWindowSize(width, height, always)

Sets the size of the next window, with `always` working as in `setNextWindowPos`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.setNextWindowSize(600, 400)
        imgui.beginWindow('Inventory')
        imgui.text('Empty')
        imgui.endWindow()
    end,
})
```

### imgui.beginChild(id, width, height, border)

Starts a scrolling region inside the current window and returns whether it is visible. The arguments `width` and `height` default to `0`, which uses the remaining space of the window. The argument `border` draws a border around the region when truthy. The function `imgui.endChild` must follow every `beginChild`, whatever it returned.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Console')
        if imgui.beginChild('lines', 0, 300, true) then
            for line = 1, 50 do
                imgui.text('Line ' .. line)
            end
        end
        imgui.endChild()
        imgui.endWindow()
    end,
})
```

### imgui.endChild()

Ends the region `beginChild` started.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Notes')
        imgui.beginChild('body')
        imgui.textWrapped('Remember to water the crops before night falls.')
        imgui.endChild()
        imgui.endWindow()
    end,
})
```

## Text

### imgui.text(text)

Shows a line of text.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Info')
        imgui.text('Wood 12')
        imgui.endWindow()
    end,
})
```

### imgui.textColored(color, text)

Shows text in a color, given as a `haylen.Color`, a `'#RRGGBB'` or `'#AARRGGBB'` string, or a table with `r`, `g`, `b` and optional `a` from 0 to 1.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Health')
        imgui.textColored('#FFFF4040', 'Low health')
        imgui.textColored({r = 0.4, g = 1, b = 0.4}, 'Shield up')
        imgui.endWindow()
    end,
})
```

### imgui.textWrapped(text)

Shows text that wraps at the edge of the window.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Help')
        imgui.textWrapped('Press E to open the crafting menu and drag two items together to combine them.')
        imgui.endWindow()
    end,
})
```

## Widgets

### imgui.button(label, width, height)

Shows a button and returns `true` in the frame the player presses it. The arguments `width` and `height` default to `0`, which sizes the button to its label.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Debug')
        if imgui.button('Spawn enemy', 240, 48) then
            print('spawned')
        end
        imgui.endWindow()
    end,
})
```

### imgui.checkbox(label, checked)

Shows a check box for the boolean `checked` and returns `changed, checked`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    wireframe = false,
    renderUi = function(self)
        imgui.beginWindow('Render')
        local changed
        changed, self.wireframe = imgui.checkbox('Show colliders', self.wireframe)
        if changed then
            print('colliders ' .. tostring(self.wireframe))
        end
        imgui.endWindow()
    end,
})
```

### imgui.sliderFloat(label, value, min, max, format)

Shows a slider for a number between `min` and `max` and returns `changed, value`. The argument `format` is a `printf` format for the shown number and defaults to `'%.3f'`. It converts the number at most once, with a conversion such as `%.1f` or `%e`, and writes a percent sign as `%%`. Any other format raises an error, because it would make `printf` read values that do not exist.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    gravity = 9.8,
    renderUi = function(self)
        imgui.beginWindow('Physics')
        local _
        _, self.gravity = imgui.sliderFloat('Gravity', self.gravity, 0, 30, '%.1f')
        imgui.endWindow()
    end,
})
```

### imgui.sliderInt(label, value, min, max)

Shows a slider for an integer between `min` and `max` and returns `changed, value`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    wave = 1,
    renderUi = function(self)
        imgui.beginWindow('Waves')
        local _
        _, self.wave = imgui.sliderInt('Wave', self.wave, 1, 20)
        imgui.endWindow()
    end,
})
```

### imgui.dragFloat(label, value, speed, min, max)

Shows a number the player changes by dragging and returns `changed, value`. The argument `speed` is the change per pointer unit and defaults to `1`. The arguments `min` and `max` default to `0`, and when both are `0` the value has no limits.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    zoom = 1,
    renderUi = function(self)
        imgui.beginWindow('Camera')
        local _
        _, self.zoom = imgui.dragFloat('Zoom', self.zoom, 0.01, 0.25, 4)
        imgui.endWindow()
    end,
})
```

### imgui.inputText(label, value, hint)

Shows a text entry for the string `value` and returns `changed, value`. The argument `hint` is shown while the entry is empty.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    command = '',
    renderUi = function(self)
        imgui.beginWindow('Console')
        local _
        _, self.command = imgui.inputText('Command', self.command, 'give wood 10')
        if imgui.button('Run') then
            print('run ' .. self.command)
        end
        imgui.endWindow()
    end,
})
```

### imgui.inputFloat(label, value, step)

Shows a number entry and returns `changed, value`. A `step` above `0` adds minus and plus buttons that change the value by it. The argument `step` defaults to `0`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    damage = 1.5,
    renderUi = function(self)
        imgui.beginWindow('Weapon')
        local _
        _, self.damage = imgui.inputFloat('Damage', self.damage, 0.5)
        imgui.endWindow()
    end,
})
```

### imgui.inputInt(label, value, step)

Shows an integer entry with minus and plus buttons that change the value by `step`, which defaults to `1`, and returns `changed, value`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    gold = 100,
    renderUi = function(self)
        imgui.beginWindow('Wallet')
        local _
        _, self.gold = imgui.inputInt('Gold', self.gold, 10)
        imgui.endWindow()
    end,
})
```

### imgui.colorEdit(label, color)

Shows a color editor with a swatch that opens a picker and returns `changed, color`. The argument `color` is a `haylen.Color`, a `'#RRGGBB'` or `'#AARRGGBB'` string or a table with `r`, `g`, `b` and optional `a`, and the result is a `haylen.Color`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    tint = '#FF336699',
    renderUi = function(self)
        imgui.beginWindow('Lighting')
        local changed, color = imgui.colorEdit('Ambient', self.tint)
        if changed then
            self.tint = color:toHex()
        end
        imgui.endWindow()
    end,
})
```

### imgui.combo(label, current, items)

Shows a drop-down list of the strings in `items` with the 1-based index `current` selected and returns `changed, index`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

local weathers = {'Sun', 'Rain', 'Storm'}

scene.push({
    weather = 1,
    renderUi = function(self)
        imgui.beginWindow('World')
        local changed
        changed, self.weather = imgui.combo('Weather', self.weather, weathers)
        if changed then
            print('weather ' .. weathers[self.weather])
        end
        imgui.endWindow()
    end,
})
```

### imgui.selectable(label, selected)

Shows a line that highlights while `selected` is truthy and returns `true` in the frame the player clicks it.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

local levels = {'Beach', 'Forest', 'Cave'}

scene.push({
    level = 'Beach',
    renderUi = function(self)
        imgui.beginWindow('Levels')
        for _, name in ipairs(levels) do
            if imgui.selectable(name, name == self.level) then
                self.level = name
            end
        end
        imgui.endWindow()
    end,
})
```

### imgui.progressBar(fraction, width, height, overlay)

Shows a bar filled to `fraction`, from 0 to 1. The argument `width` defaults to `-1`, which fills the width of the window, and `height` defaults to `0`, which uses the height of a line. The argument `overlay` is text drawn on the bar, and without it the bar shows the percentage.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    loaded = 0.35,
    renderUi = function(self)
        imgui.beginWindow('Loading')
        imgui.progressBar(self.loaded, -1, 0, 'Textures')
        imgui.progressBar(0.8, 300, 24)
        imgui.endWindow()
    end,
})
```

### imgui.plotLines(label, values, overlay, min, max, width, height)

Plots the list of numbers `values` as a line graph. The argument `overlay` is text drawn over the graph. The arguments `min` and `max` bound the vertical scale, and without them the graph scales to the values. The arguments `width` and `height` default to `0`, which uses the default size.

```lua
local debug = require('haylen.debug')
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Frames')
        imgui.plotLines('ms', debug.frameHistory(), 'frame time', 0, 50, 400, 120)
        imgui.endWindow()
    end,
})
```

### imgui.plotHistogram(label, values, overlay, min, max, width, height)

Plots the list of numbers `values` as bars, with the arguments of `plotLines`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Loot')
        imgui.plotHistogram('Drops', {4, 9, 2, 7}, 'per chest', 0, 10, 300, 100)
        imgui.endWindow()
    end,
})
```

### imgui.image(texture, width, height)

Shows a `haylen.Texture`, such as one from [`haylen.assets`](assets.md) or [`haylen.graphics`](graphics.md). The arguments `width` and `height` default to the size of the texture.

```lua
local assets = require('haylen.assets')
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

local portrait = assets.texture('ui/portrait.png')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Character')
        imgui.image(portrait, 128, 128)
        imgui.endWindow()
    end,
})
```

## Trees and headers

### imgui.treeNode(label)

Shows a node that the player opens and closes and returns whether it is open. Call `imgui.treePop` after its content only when it returned `true`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Entities')
        if imgui.treeNode('Player') then
            imgui.text('x 120, y 48')
            imgui.treePop()
        end
        imgui.endWindow()
    end,
})
```

### imgui.treePop()

Ends the content of a node `treeNode` opened. Without an open node in the current window it raises `There is no open tree node for "imgui.treePop" to close.`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Scene')
        if imgui.treeNode('Layers') then
            imgui.text('Ground')
            imgui.text('Props')
            imgui.treePop()
        end
        imgui.endWindow()
    end,
})
```

### imgui.collapsingHeader(label)

Shows a header that the player opens and closes and returns whether it is open. It needs no closing call.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Settings')
        if imgui.collapsingHeader('Audio') then
            imgui.text('Music 70%')
        end
        imgui.endWindow()
    end,
})
```

## Tabs

### imgui.beginTabBar(id)

Starts a tab bar and returns whether it is visible. Call `imgui.endTabBar` only when it returned `true`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Editor')
        if imgui.beginTabBar('sections') then
            if imgui.beginTabItem('Map') then
                imgui.text('Map tools')
                imgui.endTabItem()
            end
            if imgui.beginTabItem('Items') then
                imgui.text('Item tools')
                imgui.endTabItem()
            end
            imgui.endTabBar()
        end
        imgui.endWindow()
    end,
})
```

### imgui.endTabBar()

Ends the tab bar `beginTabBar` started, as the example of `beginTabBar` shows.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Tabs')
        if imgui.beginTabBar('only') then
            imgui.endTabBar()
        end
        imgui.endWindow()
    end,
})
```

### imgui.beginTabItem(label)

Adds a tab to the current tab bar and returns whether it is the selected tab. Call `imgui.endTabItem` after its content only when it returned `true`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Tabs')
        if imgui.beginTabBar('bar') then
            if imgui.beginTabItem('General') then
                imgui.text('General settings')
                imgui.endTabItem()
            end
            imgui.endTabBar()
        end
        imgui.endWindow()
    end,
})
```

### imgui.endTabItem()

Ends the content of the tab `beginTabItem` selected, as the example of `beginTabItem` shows.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Tabs')
        if imgui.beginTabBar('bar') then
            if imgui.beginTabItem('About') then
                imgui.text('Tiny Island 1.0')
                imgui.endTabItem()
            end
            imgui.endTabBar()
        end
        imgui.endWindow()
    end,
})
```

## Tables

### imgui.beginTable(id, columns)

Starts a table with `columns` columns, borders and alternating row colors, and returns whether it is visible. Call `imgui.endTable` only when it returned `true`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

local scores = {{'Ana', 1200}, {'Bia', 950}}

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Scores')
        if imgui.beginTable('scores', 2) then
            imgui.tableSetupColumn('Player')
            imgui.tableSetupColumn('Score')
            imgui.tableHeadersRow()
            for _, entry in ipairs(scores) do
                imgui.tableNextRow()
                imgui.tableNextColumn()
                imgui.text(entry[1])
                imgui.tableNextColumn()
                imgui.text(tostring(entry[2]))
            end
            imgui.endTable()
        end
        imgui.endWindow()
    end,
})
```

### imgui.endTable()

Ends the table `beginTable` started.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Grid')
        if imgui.beginTable('grid', 3) then
            imgui.tableNextRow()
            imgui.tableNextColumn()
            imgui.text('A1')
            imgui.endTable()
        end
        imgui.endWindow()
    end,
})
```

### imgui.tableSetupColumn(label)

Names the next column of the current table. Call it once per column before `tableHeadersRow`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Items')
        if imgui.beginTable('items', 2) then
            imgui.tableSetupColumn('Item')
            imgui.tableSetupColumn('Count')
            imgui.tableHeadersRow()
            imgui.endTable()
        end
        imgui.endWindow()
    end,
})
```

### imgui.tableHeadersRow()

Draws the header row with the column names `tableSetupColumn` gave.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Header')
        if imgui.beginTable('header', 1) then
            imgui.tableSetupColumn('Name')
            imgui.tableHeadersRow()
            imgui.endTable()
        end
        imgui.endWindow()
    end,
})
```

### imgui.tableNextRow()

Starts a new row of the current table. Outside a table, including after a `beginTable` that returned `false`, it raises `There is no open table for "imgui.tableNextRow" to add a row to.`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Rows')
        if imgui.beginTable('rows', 1) then
            for row = 1, 3 do
                imgui.tableNextRow()
                imgui.tableNextColumn()
                imgui.text('Row ' .. row)
            end
            imgui.endTable()
        end
        imgui.endWindow()
    end,
})
```

### imgui.tableNextColumn()

Moves to the next cell of the current row and returns whether that cell is visible.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Cells')
        if imgui.beginTable('cells', 2) then
            imgui.tableNextRow()
            if imgui.tableNextColumn() then
                imgui.text('Left')
            end
            if imgui.tableNextColumn() then
                imgui.text('Right')
            end
            imgui.endTable()
        end
        imgui.endWindow()
    end,
})
```

## Popups and tooltips

### imgui.openPopup(id)

Marks the popup `id` as open. The function `imgui.beginPopup` with the same id then shows it until the player clicks outside it or the app closes it.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Save')
        if imgui.button('Delete save') then
            imgui.openPopup('confirm')
        end
        if imgui.beginPopup('confirm') then
            imgui.text('Delete the save?')
            if imgui.button('Yes') then
                print('deleted')
                imgui.closeCurrentPopup()
            end
            imgui.endPopup()
        end
        imgui.endWindow()
    end,
})
```

### imgui.beginPopup(id)

Starts the content of the popup `id` and returns whether it is open. Call `imgui.endPopup` only when it returned `true`, as the example of `openPopup` shows.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Menu')
        if imgui.button('Options') then
            imgui.openPopup('options')
        end
        if imgui.beginPopup('options') then
            imgui.selectable('Rename', false)
            imgui.endPopup()
        end
        imgui.endWindow()
    end,
})
```

### imgui.endPopup()

Ends the content of the popup `beginPopup` started.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Popup')
        if imgui.button('Info') then
            imgui.openPopup('info')
        end
        if imgui.beginPopup('info') then
            imgui.text('Version 1.0')
            imgui.endPopup()
        end
        imgui.endWindow()
    end,
})
```

### imgui.closeCurrentPopup()

Closes the popup whose content is being built, as the example of `openPopup` shows.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Pick')
        if imgui.button('Pick a color') then
            imgui.openPopup('colors')
        end
        if imgui.beginPopup('colors') then
            if imgui.selectable('Red', false) then
                imgui.closeCurrentPopup()
            end
            imgui.endPopup()
        end
        imgui.endWindow()
    end,
})
```

### imgui.isItemHovered()

Returns `true` while the pointer is over the widget drawn last.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Tools')
        imgui.button('Paint')
        if imgui.isItemHovered() then
            imgui.setTooltip('Paints the selected tile')
        end
        imgui.endWindow()
    end,
})
```

### imgui.setTooltip(text)

Shows a tooltip next to the pointer for this frame.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Hints')
        imgui.text('Hover me')
        if imgui.isItemHovered() then
            imgui.setTooltip('Hello')
        end
        imgui.endWindow()
    end,
})
```

## Layout

### imgui.separator()

Draws a horizontal line.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Sections')
        imgui.text('Top')
        imgui.separator()
        imgui.text('Bottom')
        imgui.endWindow()
    end,
})
```

### imgui.sameLine(offset, spacing)

Places the next widget on the same line as the previous one. The argument `offset` is the position from the start of the window and defaults to `0`, which follows the previous widget. The argument `spacing` defaults to `-1`, which uses the theme spacing.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Playback')
        imgui.button('Play')
        imgui.sameLine()
        imgui.button('Pause')
        imgui.sameLine(0, 40)
        imgui.button('Stop')
        imgui.endWindow()
    end,
})
```

### imgui.spacing()

Adds a small vertical space.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Spaced')
        imgui.text('First')
        imgui.spacing()
        imgui.text('Second')
        imgui.endWindow()
    end,
})
```

### imgui.dummy(width, height)

Adds an empty space of the given size.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Gap')
        imgui.text('Above')
        imgui.dummy(0, 60)
        imgui.text('Below')
        imgui.endWindow()
    end,
})
```

### imgui.setNextItemWidth(width)

Sets the width of the next widget.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    name = 'Ana',
    renderUi = function(self)
        imgui.beginWindow('Player')
        imgui.setNextItemWidth(200)
        local _
        _, self.name = imgui.inputText('Name', self.name)
        imgui.endWindow()
    end,
})
```

### imgui.pushId(id)

Adds `id` to the identity of the widgets that follow until `imgui.popId`, so widgets with the same label in a loop stay apart.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    slots = {'Axe', 'Rope', 'Lantern'},
    renderUi = function(self)
        imgui.beginWindow('Slots')
        for index, item in ipairs(self.slots) do
            imgui.pushId('slot' .. index)
            imgui.text(item)
            imgui.sameLine()
            if imgui.button('Drop') then
                print('dropped ' .. item)
            end
            imgui.popId()
        end
        imgui.endWindow()
    end,
})
```

### imgui.popId()

Removes the identity `pushId` added, as the example of `pushId` shows.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Pair')
        imgui.pushId('left')
        imgui.button('Use')
        imgui.popId()
        imgui.pushId('right')
        imgui.button('Use')
        imgui.popId()
        imgui.endWindow()
    end,
})
```

### imgui.pushFont(name, size)

Uses the font registered under `name` at `size` for the widgets that follow until `imgui.popFont`. Fonts are registered with `ui.addFont` or listed in the `fontFiles` of a theme, and the built-in font is `default`. An unknown name raises `The UI has no font named "<name>".`.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Title')
        imgui.pushFont('default', 48)
        imgui.text('Big text')
        imgui.popFont()
        imgui.endWindow()
    end,
})
```

### imgui.popFont()

Goes back to the font in use before the last `pushFont`.

```lua
local imgui = require('haylen.imgui')
local ui = require('haylen.ui')
local scene = require('haylen.scene')

ui.addFont('pixel', 'fonts/pixel.ttf')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('Fonts')
        imgui.pushFont('pixel', 32)
        imgui.text('Pixel font')
        imgui.popFont()
        imgui.text('Theme font')
        imgui.endWindow()
    end,
})
```

## Tools

### imgui.showDemoWindow()

Shows the Dear ImGui demo window, which shows every widget Dear ImGui offers.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.showDemoWindow()
    end,
})
```

### imgui.showMetricsWindow()

Shows the Dear ImGui metrics window, which inspects windows, draw lists and the state of Dear ImGui.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.showMetricsWindow()
    end,
})
```

### imgui.framerate()

Returns the frame rate Dear ImGui measures, in frames per second, averaged over the last frames.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    renderUi = function(self)
        imgui.beginWindow('FPS', {autoResize = true})
        imgui.text(string.format('%.0f FPS', imgui.framerate()))
        imgui.endWindow()
    end,
})
```
