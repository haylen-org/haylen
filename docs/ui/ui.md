# 🪟 UI

This guide teaches you how to build the interface of an app, step by step. You mount a GUI, lay it out and place it on the screen. Then you react to the player, give the GUI a look, move the focus with every device, share the screen with the game and scroll long content. It is for anyone who builds menus, HUDs, settings screens, dialogs or touch controls. The reference pages [`haylen.ui`](../lua-api/ui.md) and [`haylen.imgui`](../lua-api/imgui.md) list every property, function, event and error.

The examples often come from the Tiny Island sample. Its theme is `samples/games/tiny-island/content/ui/theme.json`, its screens are in `samples/games/tiny-island/source/scenes/`, its HUD is `samples/games/tiny-island/source/ui/hud.lua`, and its shared building blocks are in `samples/games/tiny-island/source/ui/widgets.lua`.

## Pick a module

Haylen has two interface modules. Both draw over the app in design units, with the active theme.

- The module `haylen.ui` builds a retained GUI. Retained means that you build a tree of components once and then change it by id. The engine keeps the tree, checks every property and draws it every frame.
- The module `haylen.imgui` gives you Dear ImGui windows in immediate mode. Immediate mode means that your code builds the window again every frame from its own state.

| You want | Use |
| --- | --- |
| Menus, HUDs, settings, dialogs and anything else players see | `haylen.ui` |
| Textured art, translations, safe area placement and touch controls | `haylen.ui` |
| Navigation with gamepads, keyboards and TV remotes | `haylen.ui` |
| Screens stored as data in JSON | `haylen.ui` |
| Debug panels, cheat menus, live tuning and inspectors | `haylen.imgui` |
| Windows whose content changes shape every frame, such as a list of entities | `haylen.imgui` |
| Plots, drag values and quick tools that never ship to players | `haylen.imgui` |

A retained GUI keeps the Lua work of each frame low, because nothing runs until something changes. Immediate mode code is shorter for tools, but it runs Lua for every widget every frame and has the ImGui look. Most of this guide covers `haylen.ui`. The section [immediate mode UI](#immediate-mode-ui-with-haylenimgui) covers `haylen.imgui`.

## Mount a GUI

A GUI is a tree of nodes. A node is a plain Lua table with a `kind`, such as `'button'`, an optional `id`, its children and the properties of its kind. Every kind is also a function of the module, so `ui.button{text = 'Play'}` builds a button node. When the kind comes from a variable, `ui.node('button', {text = 'Play'})` does the same. Children go in the array part of the node or in a `children` list, but never in both.

The function [`ui.mount(tree, options)`](../lua-api/ui.md#uimounttree-options) turns the tree into components and returns a [`Gui`](../lua-api/ui.md#gui). From then on, the engine draws it over the app every frame. This scene shows a pause screen:

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local pause = {}

function pause:enter()
    self.gui = ui.mount(ui.column{
        justify = 'center',
        padding = 64,
        ui.panel{
            width = 640,
            align = 'center',
            ui.label{text = 'Paused', font = 'title', textAlign = 'center', align = 'center'},
            ui.button{id = 'resume', text = 'Resume', variant = 'primary', align = 'stretch', onClick = function()
                scene.pop()
            end},
        },
    }, {owner = self})
    self.gui:command('resume', 'focus')
end

return pause
```

### A GUI lives with its owner

The option `owner = self` makes the scene own the GUI. You never unmount such a GUI yourself:

- The GUI unmounts when the scene unloads.
- It draws only while its scene shows. When another scene is pushed over it, the GUI hides and keeps its state, such as the text in its fields, until its scene shows again.
- During a scene transition, it draws into the image of its own scene. So it never flashes over the other scene.

A GUI without a scene as owner stays on screen above every scene until you call `gui:unmount()`, even when the scenes change. That suits the HUD of an [autoload](../core/lifecycle.md), which lives as long as the app.

### Every property is checked

The engine checks every property when it builds the tree. A misspelled key or a wrong value raises an error that names the kind and the key, and nothing is mounted. For example, `width = 'big'` on a label raises `The property "width" of a "label" must be a non-negative number or "auto".`. A GUI also has limits on its depth and its node count, which the [screen format](../lua-api/ui.md#screen-format) lists.

### Screens in JSON

A tree is plain data, so a screen can also live in a JSON file of the package. JSON cannot hold functions, so you attach the handlers with `set` after you mount the screen.

```json
{
    "kind": "column",
    "align": "center",
    "children": [
        {"kind": "label", "text": {"key": "menu.title"}, "font": "title"},
        {"kind": "button", "id": "play", "text": {"key": "menu.play"}, "variant": "primary"}
    ]
}
```

```lua
local assets = require('haylen.assets')
local ui = require('haylen.ui')

local menu = ui.mount(assets.json('ui/main-menu.json'))
menu:set('play', {onClick = function()
    print('Play.')
end})
```

An empty Lua table becomes an empty JSON object. List properties such as `items` accept it as an empty list, as the [screen format](../lua-api/ui.md#screen-format) explains.

## Lay out the nodes

Layout decides where each node sits and how big it is. Containers place their children, and a few common properties fine-tune every node.

### Containers

| Container | What it does |
| --- | --- |
| `column` | Places its children from top to bottom. |
| `row` | Places its children from left to right, or from right to left in a [right-to-left](#complex-scripts-and-right-to-left-interfaces) interface. With `wrap = true`, it breaks them into lines. |
| `grid` | Places its children in cells of equal width, row after row. |
| `stack` | Draws its children on top of each other, the later ones above. |
| `scroll` | Shows one child in an area that scrolls, as [scroll areas](#scroll-areas) shows. |
| `card`, `panel` | Columns on a themed surface. They keep the pointer from reaching the app behind them. |

### Space, size and growth

A few properties shape almost every layout:

- The property `gap` is the space between the children of a container. It defaults to the theme metric `itemSpacing`.
- The property `padding` is the space inside a container, between its edges and its children. Give one number for every side, `{vertical, horizontal}`, or `{top, right, bottom, left}`.
- The property `margin` is the space around a node, on top of the `gap` of its parent.
- The properties `width` and `height` fix a size, and `minWidth`, `maxWidth`, `minHeight` and `maxHeight` bound it. The value `'auto'` lets the content decide.
- The property `aspectRatio` keeps the width divided by the height, such as `16 / 9` for a video frame.
- The property `grow` gives a child a share of the free space of its column or row.

A growing child starts from nothing, not from the size of its content. So two children with `grow = 1` split the free space in half, and long text never pushes a row past its bounds. A child that reaches its maximum size leaves the rest of the space to the others. A row measures its growing children at the width they get. So text that wraps there reports every line, and the node below starts after it. A `spacer` takes room and draws nothing, so `ui.spacer{grow = 1}` pushes the nodes after it to the far end.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    padding = {24, 32},
    gap = 12,
    ui.row{
        gap = 16,
        ui.label{text = 'Inventory', font = 'heading'},
        ui.spacer{grow = 1},
        ui.button{text = 'Sort'},
    },
    ui.row{
        gap = 16,
        ui.card{grow = 1, ui.label{text = 'Tools'}},
        ui.card{grow = 2, ui.label{text = 'Materials, which take twice the room'}},
    },
    ui.image{image = 'ui/preview.png', aspectRatio = 16 / 9, maxWidth = 640},
})
```

### Alignment

Two properties place children. The property `justify` of a column or a row places the children along its main axis, in the space that growing children leave: `start`, `center`, `end`, `spaceBetween`, `spaceAround` or `spaceEvenly`. The property `align` of a child places it across the other axis: `start`, `center`, `end` or `stretch`. In a grid, `align` places the child inside its cell. In a stack or at the root, it works in both directions.

The property `alignItems` of a container sets the `align` of every child that sets none. Without either, each kind has a default: rows center their children, small kinds such as labels and buttons start, and big kinds such as lists stretch. The [common properties](../lua-api/ui.md#common-properties) list the default of every kind.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    justify = 'center',
    alignItems = 'center',
    gap = 24,
    ui.label{text = 'Tiny Island', font = 'title'},
    ui.button{text = 'Play', variant = 'primary', width = 480},
    ui.button{text = 'Options', width = 480},
    ui.button{text = 'Credits', variant = 'link', align = 'end'},
})
```

### Lines that wrap and grids that fit

A row with `wrap = true` breaks its children into lines, like a row of tags or of shop items. A grid with `minColumnWidth` fits as many columns as its width allows. Both follow the size of the screen, which matters on phones that turn and in the [physical scale mode](#scale-and-screen-density).

```lua
local ui = require('haylen.ui')

local items = {}
for index = 1, 12 do
    items[index] = ui.card{ui.label{text = 'Item ' .. index}}
end

ui.mount(ui.column{
    ui.row{wrap = true, gap = 8, lineGap = 8,
        ui.chip{text = 'Tools'}, ui.chip{text = 'Food'}, ui.chip{text = 'Boats'}, ui.chip{text = 'Maps'},
    },
    ui.grid{minColumnWidth = 280, columns = 6, gap = 16, children = items},
})
```

### Text that fits

Text measures in whole design units. So a label, a button or a choice placed at its measured size always shows its whole text, whatever the scaling of the screen. It ends with an ellipsis only when its space is really too small. A label with `minScale` draws smaller instead, which suits a translation that runs long. A button whose label is too wide wraps it and grows taller.

Cards and panels add padding from two places. Their own `padding` applies when it is set, and otherwise the theme metric `panelPadding` applies. The `padding` of their [theme surface](#surfaces) always adds on top, so the content clears thick frame borders.

### A HUD layout

The Tiny Island HUD shows the usual shape of a game HUD. A padded root column holds a top row. The row has the survivor panel with its meters on the left, a growing spacer, and the day panel and the pause button on the right. A toast sits under the row. A second growing spacer pushes the touch controls to the bottom.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    padding = 32,
    ui.row{
        align = 'stretch',
        gap = 24,
        ui.panel{width = 620, align = 'start', gap = 10,
            ui.label{text = 'Survivor', font = 'heading'},
            ui.progress{id = 'health', value = 0.8, tone = 'success', text = 'Health'},
            ui.progress{id = 'food', value = 0.6, tone = 'warning', text = 'Food'},
        },
        ui.spacer{grow = 1},
        ui.panel{width = 420, align = 'start', gap = 8,
            ui.label{id = 'day', text = 'Day 1', font = 'heading'},
            ui.label{id = 'phase', text = 'Morning'},
        },
        ui.button{id = 'pause', icon = 'ui/pause.png', variant = 'icon', align = 'start'},
    },
    ui.toast{id = 'notice', duration = 4},
    ui.spacer{grow = 1},
    ui.row{id = 'touch', align = 'stretch',
        ui.touchStick{action = 'move', mode = 'floating', touchOnly = true, width = 760, height = 420, align = 'end'},
        ui.spacer{grow = 1},
        ui.touchButton{action = 'attack', image = 'ui/attack.png', size = 190, touchOnly = true, align = 'end'},
    },
})
```

## Place it on the screen

Sizes and positions are design units of the `design` resolution in `app.json`, which is 1920 by 1080 in Tiny Island and by default. The metrics of the built-in themes suit that resolution. The [rendering guide](../graphics/rendering.md) explains the scaling policies that map design units to the screen. The functions [`viewport.setScaling`](../lua-api/viewport.md#viewportsetscalingpolicy) and [`viewport.setDesignSize`](../lua-api/viewport.md#viewportsetdesignsizewidth-height) change them while the app runs, and every GUI lays out in the new areas from the next frame on.

### The safe area

The app draws over the whole screen on every platform, under notches, dynamic islands, rounded corners and gesture bars. The safe area is the part of the screen that they leave free. By default, `ui.mount` lays out the root of a GUI inside the safe area, so its controls are never hidden. Every Tiny Island screen, the HUD included, keeps that default.

A GUI that paints a background to the edges of the screen mounts with `placement = 'screen'`. It then keeps its controls inside the safe area with a `safeArea` node or with anchors. Toasts always appear at the top or the bottom edge of the safe area, stacked so they never cover each other.

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/title.png', fit = 'cover', align = 'stretch'},
    ui.safeArea{ui.column{justify = 'end', padding = 32, ui.button{text = 'Start', variant = 'primary', align = 'end'}}},
}, {placement = 'screen'})
```

### Anchors

An [anchor](../lua-api/ui.md#anchors) takes a node out of the layout of its parent and pins it to an area: the safe area, the whole screen, or the bounds of its parent. A node can sit at a corner, at the middle of an edge or at the center, or it can stretch along an edge, a band or the whole area. The property `margin` keeps it away from the edges. Anchored nodes follow the safe area when it changes, such as when a phone turns.

A node anchored with `anchorTo = 'parent'` moves and resizes with its parent. That suits touch controls in the corners of a play area that shares the screen with panels.

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/sky.png', fit = 'cover', anchor = 'stretch', anchorTo = 'screen'},
    ui.button{id = 'pause', icon = 'ui/pause.png', variant = 'icon', anchor = 'topRight', margin = 16},
    ui.panel{anchor = 'stretchBottom', height = 120, margin = {0, 24}, ui.label{text = 'Wood 12'}},
}, {placement = 'screen'})

ui.mount(ui.row{
    ui.stack{grow = 1, align = 'stretch',
        ui.playArea{id = 'stage', align = 'stretch'},
        ui.touchStick{action = 'move', mode = 'floating', touchOnly = true, width = 360, height = 300, anchor = 'bottomLeft', anchorTo = 'parent', margin = 24},
    },
    ui.panel{width = 420, ui.button{text = 'Restart'}},
})
```

### Layers and drawing order

GUIs draw in the order of their `layer` option, and GUIs on the same layer in the order you mounted them. So a later GUI covers an earlier one, with everything it holds. Inside a GUI, nodes draw in the order of the tree. A scroll draws its content at the place of its node, so the nodes after it cover what scrolls, such as a sheet anchored to the bottom.

### Testing the safe area on a desktop

A desktop window has no notch, so Haylen can simulate one. Set `debug.safeArea` in `app.json`, or call [`viewport.setSafeAreaSimulation`](../lua-api/viewport.md#viewportsetsafeareasimulationvalue) while the app runs. It takes a device, such as `'iphoneNotch'`, `'iphoneDynamicIsland'`, `'ipad'`, `'androidGestureBar'` or `'television'`, whose insets follow the window as it turns between landscape and portrait. It also takes insets in window points. The option `debug.showSafeArea`, or [`ui.setSafeAreaVisible`](../lua-api/ui.md#uisetsafeareavisiblevisible), shades what lies outside the safe area and prints its insets where they cover no interface.

```json
{
    "name": "Notch Test",
    "debug": {"safeArea": "iphoneDynamicIsland", "showSafeArea": true}
}
```

## React to the player

Components report what the player does as events, such as `click` for a button or `change` for a slider. Your code answers them with handlers.

### Handlers

A handler is a key of a node that starts with `on` and an upper-case letter, and holds a function. The handler `onClick` answers the event `click`, and `onChange` answers `change`. The handler receives one table with the values of the event, plus `id`, `name` and `gui`. So a handler can change its own GUI through `event.gui:set`. The section of each kind in the [reference](../lua-api/ui.md#component-kinds) lists its events and their values.

```lua
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

ui.mount(ui.settingsForm{
    ui.settingsRow{label = 'Music', ui.slider{id = 'music', value = preferences.get('music', 0.7), width = 440, showValue = true, onChange = function(event)
        preferences.set('music', event.value)
    end}},
    ui.settingsRow{label = 'Subtitles', ui.toggle{checked = preferences.get('subtitles', true), onChange = function(event)
        preferences.set('subtitles', event.checked)
    end}},
})
```

Handlers run once per frame, in the update after the GUI drew, before the scenes update. A node with handlers and no `id` gets an id from the engine, such as `#1`. An error inside a handler stops the app and shows the error screen with the message and its Lua stack trace. Handlers leave with their node.

### Change a mounted GUI

You change a mounted GUI by node id instead of building it again:

- [`gui:set(id, properties)`](../lua-api/ui.md#guisetid-properties) changes some properties of a node and keeps the others. It checks the merged result first, so a bad value leaves the node as it was. Its `onX` keys add or replace handlers.
- [`gui:replaceChildren(id, children)`](../lua-api/ui.md#guireplacechildrenid-children) replaces every child of a node, such as the rows of a shop.
- [`gui:get(id)`](../lua-api/ui.md#guigetid) and [`gui:has(id)`](../lua-api/ui.md#guihasid) read the properties of a node and tell whether it exists.
- [`gui:bounds(id)`](../lua-api/ui.md#guiboundsid) returns where a node last drew, which suits a tutorial that points at a button.
- [`gui:command(id, 'focus')`](../lua-api/ui.md#guicommandid-name-arguments) moves the focus to a node.

Values that the player changed, such as typed text or a moved slider, stay in the component. A `set` changes them only when it names that same property.

A `set` costs a check of the merged properties, so set only what changed. The Tiny Island HUD checks a dozen values every frame and calls `set` only when one of them changed. It compares a translation by the plain value it shows, so it sets the day label again only when the day number changes.

```lua
local ui = require('haylen.ui')

local hud = {shown = {}}
hud.gui = ui.mount(ui.column{
    ui.label{id = 'day', text = ''},
    ui.progress{id = 'health', value = 1},
})

-- Sets a property only when its value changed since the last frame.
function hud:show(id, key, value, compared)
    local shownKey = id .. '.' .. key
    compared = compared or value
    if self.shown[shownKey] ~= compared then
        self.shown[shownKey] = compared
        self.gui:set(id, {[key] = value})
    end
end

function hud:update(game)
    self:show('health', 'value', game.health)
    self:show('health', 'tone', game.health < 0.3 and 'danger' or 'success')
    self:show('day', 'text', {key = 'hud.day', args = {day = game.day}}, game.day)
end

hud:update({health = 0.25, day = 3})
```

### Move, fade and animate nodes

The method [`gui:transform(id)`](../lua-api/ui.md#guitransformid) returns the transform of a node: an offset, a scale, an opacity and a tint, on top of the place that the layout gives the node. Tweens animate a transform in C++, without running Lua every frame, so menus can slide and buttons can pulse cheaply.

```lua
local math2d = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local menu = ui.mount(ui.column{align = 'center', gap = 16,
    ui.label{id = 'title', text = 'Tiny Island', font = 'title'},
    ui.button{id = 'play', text = 'Play', variant = 'primary'},
})

local title = menu:transform('title')
title.opacity = 0
title.offset = math2d.vec2(0, -40)
tween.to(title, 0.6, {opacity = 1, offset = math2d.vec2(0, 0)}, {ease = 'backOut'})
```

### Events of every node

Every node can also report its life and the pointer. The events `mount` and `unmount` tell when the node joins and leaves its GUI. The events `show` and `hide` tell when it starts and stops drawing. The events `hover`, `press`, `drag`, `release` and `scroll` report the pointer with positions and distances, which a custom control or a draggable card builds on. A node reports them only while it has a handler for them, because watching them costs a check every frame. The [reference](../lua-api/ui.md#the-events-of-every-kind) gives their values.

The function [`ui.onEvent(listener)`](../lua-api/ui.md#uioneventlistener-options) hears every event of every mounted GUI. It suits screens loaded from JSON, a sound for every click and analytics.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local ui = require('haylen.ui')

local click = assets.load('sfx/click.wav')
ui.onEvent(function(event)
    if event.name == 'click' then
        audio.play(click, {bus = 'ui'})
    end
end)
```

## Give it a look

A theme holds every color, metric, font and surface that the components use. No component draws a literal color, so a theme changes the look of every GUI at once.

### Themes

The engine ships the theme `dark`, which is active at start, and the theme `light`. The function `ui.setTheme(name)` switches every GUI and every ImGui window at once.

An app adds its own theme with a JSON file and [`ui.loadTheme(path, base)`](../lua-api/ui.md#uiloadthemepath-base). The new theme starts as a copy of the theme `base`, `dark` by default, so the file lists only what it changes. Loading a file whose `name` matches a registered theme replaces that theme, and the change shows at once when it is active. Unknown keys, roles, metrics and surfaces are errors, so a typo never goes unnoticed. The [theme files](../lua-api/ui.md#theme-files) reference lists every key.

```json
{
    "name": "harbor",
    "colors": {"accent": "#FF2E9E62", "panel": "#FF1E2A3A"},
    "metrics": {"controlHeight": 72, "panelPadding": 24},
    "fontFiles": {"rounded": "fonts/rounded.ttf"},
    "fonts": {"title": {"font": "rounded", "size": 64}, "button": {"font": "rounded"}}
}
```

```lua
local ui = require('haylen.ui')

ui.setTheme(ui.loadTheme('ui/harbor.json', 'dark'))
```

Tiny Island loads its theme this way at startup, in its `source/main.lua`.

### Colors, metrics and fonts

Colors come in families of roles:

- The roles `window`, `panel`, `raised` and `tooltip` fill surfaces, and `overlay` dims the app behind modal dialogs.
- The roles `hover`, `pressed`, `selection` and `focus` mark states.
- The roles `border` and `borderStrong` draw lines, and `scrollbar` and `scrollbarHover` draw the thumbs of scroll bars.
- The roles `text`, `textMuted`, `textDisabled` and `onTooltip` write text.
- The accent and the four tones `success`, `warning`, `danger` and `information` each come in four roles. There is a fill, and the ink on that fill, such as `onAccent`. There is a subtle background, such as `accentBackground`, and a text color that reads on the window, such as `accentText`.

So a `progress` bar with `tone = 'danger'` fills with the `danger` color. A `badge` with the same tone uses `dangerBackground` and `dangerText`, or `danger` and `onDanger` when it is `solid`.

Metrics size the controls. The metric `controlHeight` sets the height of buttons, fields and sliders. The metric `itemSpacing` is the default gap, and `panelPadding` pads cards, panels and dialogs. More specific metrics size single parts, such as `progressHeight`, `sliderTrackHeight`, `toggleWidth` and `iconSize`.

Every part that a component draws reads its theme. The shadows of floating surfaces read `shadow`, `shadowSize` and `shadowOffset`. The caret of fields reads `caret`. Panels and popups round their corners by `panelRadius`, and the rows of a popup and the title bar of a window follow it inside. The space between a box and its text is `contentSpacing`, and the focus ring keeps `focusGap` from its control. Disabled nodes draw at `disabledOpacity`. Tooltips and context menus wait for `tooltipDelay` and `longPressDuration`. The reference lists every [color](../lua-api/ui.md#theme-colors) and [metric](../lua-api/ui.md#theme-metrics).

Labels choose a font role: `body`, `caption`, `button`, `heading`, `title` or `monospace`. Fonts are sized when drawn, so one registered font serves every role and size. The size of a role is the size of its em square, like every text size of the engine. So one type scale draws every font at the same scale, and a font with tall ascenders or deep descenders gets taller lines instead of smaller letters. A role can name a font family registered with `ui.addFont(name, family)`. Then the components draw the characters that its faces lack from the fallback fonts of the family, so one theme serves every language, Japanese included. A role with `bold` or `italic` draws with those faces of the family. A role with `ligatures = false` keeps a glyph for every letter, as the `monospace` role does for code. Each fallback draws at the size of the em square of its face, so symbols match the letters around them.

Text drawn straight over the game needs an outline to stay readable. The Tiny Island titles and captions use `color = 'onAccent'` with a dark `outline` for that reason.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{align = 'center',
    ui.label{text = 'Day 3', font = 'title', color = 'onAccent', outline = '#FF1B1E2B', outlineWidth = 4},
    ui.progress{value = 0.2, tone = 'danger', text = 'Fire'},
    ui.badge{text = 'Raiders', tone = 'danger', solid = true},
})
```

### Surfaces

A surface paints a part of a component with a nine-slice image instead of flat colors, such as the paper panels and wooden dialogs of a textured game. A nine-slice image is cut into nine pieces. The four corners keep their size, while the edges and the center stretch or tile to fill the bounds. The built-in themes have no surfaces. The reference lists [every surface and every key of an image](../lua-api/ui.md#theme-surfaces).

The key `slice` is measured in texture pixels and drawn at `scale`, while `padding` is in design units. Insets of four numbers run clockwise from the top: top, right, bottom and left. A theme may leave out the hover or pressed surface of a button, and the normal surface of the same button stands in for it. Tiny Island gives its buttons hover states with the same image and a different `tint`.

A surface image ignores the color the component would use, so every tone of a progress bar would look alike. With `colorize = true`, the image is multiplied with that color, so one light image serves every color. Tiny Island draws the fill of every HUD bar from one light image, `track-fill.png`. The tone of each `progress` node turns it green for health, orange for food and fuel, and red when one runs low.

The fill of toggles, sliders and progress bars stays inside the `padding` of the `track` surface, so a framed bar keeps its frame visible around the fill. Tiny Island pads its groove by 5 units, so the fill sits inside its outline. A toggle draws its `track` in every state. Its `trackFill` fills the groove and fades in as the `knob` slides on. So the track never moves, an empty groove means off and a full one means on.

```json
{
    "name": "island",
    "imageFilter": "linear",
    "surfaces": {
        "track": {"image": "ui/track.png", "slice": 28, "scale": 0.93, "padding": 5, "filter": "linear"},
        "trackFill": {"image": "ui/track-fill.png", "slice": 16, "scale": 1.3, "colorize": true, "filter": "linear"},
        "knob": {"image": "ui/knob.png", "colorize": true, "filter": "linear"},
        "touchButtonPressed": {"image": "ui/touch-button-pressed.png", "filter": "linear"}
    }
}
```

### One node in another look

Every node takes the common properties `theme` and `style`, which reach the node and every node inside it. The property `theme` draws the subtree with another registered theme, such as a light card in a dark screen. The property `style` replaces single values of the theme, such as one button that is rounder and greener than the others. It takes the sections `colors`, `metrics`, `fonts` and `surfaces` of a theme file.

Styles nest, so a value that a style leaves out comes from the style or theme around the node. A color of a style can name a theme color role, such as `'accentText'`. The UI looks that role up every frame. So a styled node follows a theme switch, higher contrast and reduced transparency without being built again. The property `cursor` sets the shape of the mouse cursor over a node.

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{style = {colors = {accent = '#FF2E9E62'}, metrics = {controlRadius = 40}},
    ui.button{text = 'Rounder and greener', variant = 'primary', cursor = 'pointingHand'},
    ui.button{text = 'Selected', variant = 'toolbar', style = {colors = {text = 'accentText'}}},
    ui.card{theme = 'light', ui.label{text = 'A light card'}},
})
```

### The Tiny Island theme

The file `samples/games/tiny-island/content/ui/theme.json` turns the `dark` theme into the casual fantasy look of the game, with art painted for it in high definition. It is a good model for a textured theme:

- The key `imageFilter` is `linear`, so the icons, portraits and logo of the screens stay smooth when the screen scales them. Every surface sets `"filter": "linear"` for the same reason.
- The key `fontFiles` registers the rounded display font of the game, and `fonts` gives it to every role but `monospace`.
- The key `colors` sets a slate indigo palette with white text and a royal blue accent. Green, orange and red tones color the HUD bars. A yellow `focus` color rings the navigation target and marks the chosen class name.
- The key `metrics` makes controls larger, with `controlHeight` 84. It lowers `panelPadding` to 10, because the panel image brings its own padding, and sizes bars, sliders, check boxes and the switch to the art.
- The surfaces `panel`, `card`, `dialog`, `toast` and `menu` use one panel image at different scales and paddings. The surface `banner` uses the blue ribbon with only left and right borders, so the ribbon stretches in the middle and keeps its folded ends.
- The three button variants use slate, blue and red buttons. The hover states tint the normal image. The pressed states use the pressed art, whose face sits lower on a thinner bevel.
- The surface `track` is a dark groove with `padding` for its outline. The surface `trackFill` is a light pill with `colorize`, which the accent, a tone or the toggle state colors. The surface `knob` is a light knob with `colorize`, so a held knob darkens with the `pressed` color.
- The surfaces `stickBase`, `stickKnob`, `touchButton` and `touchButtonPressed` give the touch controls a ring, a knob and a round button with its pressed state.
- The keys `check`, `checkChecked`, `field`, `fieldFocused`, `tab` and `tabSelected` are the remaining surfaces. Chips and badges keep the flat colors of the palette.

The switch of the settings screen shows how the surfaces work together. Off, it is the empty groove with the knob at its start. On, the blue fill covers the groove and the knob sits at its end. Held, the knob darkens, and focused, the yellow ring surrounds it. The same three images draw every state.

### The fantasy theme of the test project

The folder `samples/tests/content/interface/themes/fantasy/` holds an image theme in the casual fantasy look of Tiny Island, made for any app to take. It has `theme.json`, the atlas `atlas.png` with every piece of the theme, and eight icons of 128 pixels in `icons/`. Copy the folder into the `content/` folder of your app under the same path, with the font `fonts/lilita-one-regular.ttf` and its license, and load it on top of `dark`. The test `GUI-033` of the test project shows every component in it next to the default theme, at several UI scales.

```lua
local ui = require('haylen.ui')

ui.setTheme(ui.loadTheme('interface/themes/fantasy/theme.json', 'dark'))
```

- Every surface names the atlas in `image` and its piece in `source`. So the whole theme is one texture of 1152 by 764 pixels, and its draws batch together. The tool `samples/tests/tools/interface/pack-fantasy-theme.py` packs the painted pieces into the atlas, writes the theme file and cuts the sheet of icons.
- The buttons of the three variants have a normal and a pressed piece, and the hover states tint the normal piece. Chips reuse the slate button at a smaller scale, the selected chip reuses the focused field, and the selected segment reuses the blue button.
- One panel piece draws `panel`, `card`, `dialog`, `window`, `menu` and `toast` at different scales and paddings. The surface `titleBar`, a blue plate, caps the window as its title bar and gives dialog titles their band. The red round `close` button closes windows and toasts, with a pressed piece and a tint for the hover.
- Fields, check boxes and radio marks have an empty piece, and a focused, checked or selected piece. The slots of slot grids reuse the field.
- Switches, sliders and progress bars share the dark groove of `track`, the light pill of `trackFill` and the light `knob`. Both light pieces use `colorize`, so the accent and every tone color them: green health, blue mana and loading.
- Tabs have a slate and a blue piece. The rows of lists, trees, tables and popups and the cells of collections take the blue selection plate of `cellSelected`, which `cellHover` and `cellPressed` tint.
- The scroll bars draw a light thumb with `colorize` in a dark groove of `scrollbarTrack`. They keep the lane and the gap of the engine, 10 units here.
- The badge is a light pill with `colorize`. So a solid badge takes the fill of its tone, and a badge that is not solid takes its background. The theme makes those backgrounds opaque dark tones, so the frame of the badge still shows. The text of a badge sits in the middle of the pill at every scale.
- The tooltip is a dark box of its own. The metrics size the controls to the art, with `controlHeight` 72 and `choiceSize` 46.

## Move the focus with every device

Players reach your interface with a mouse, a finger, a keyboard, a gamepad or a TV remote. The pointer and fingers press what they touch. The other devices act on the focus: one control at a time that a ring marks. Every GUI works with every device without extra code, as long as you give it a sensible first focus.

### The pointer

A click or a tap presses the control under it and moves the focus there. The function `ui.usingPointer()` returns `true` while the pointer is over something the interface owns. That is a control, an open dialog or menu, an ImGui window, or a card, panel or touch control. Empty space inside columns, rows and stacks lets the pointer through to the game. So gameplay that reacts to raw clicks checks `ui.usingPointer()` first. The function `ui.usingKeyboard()` returns `true` while a text field has the focus, so gameplay checks it before it reads raw keys.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if input.mousePressed('left') and not ui.usingPointer() then
            print('Walk to the clicked tile.')
        end
        if input.keyPressed('space') and not ui.usingKeyboard() then
            print('Jump.')
        end
    end,
})
```

### Keyboards and gamepads

Buttons, choices, inputs, rows, slots, play areas and the other interactive parts of a GUI take the focus. The [navigation actions](../lua-api/ui.md#navigation-actions) move it:

- The arrow keys, the directional pad and the left stick move the focus to the nearest control in that direction. A node can name its neighbor with `focusLeft`, `focusRight`, `focusUp` or `focusDown`. Sliders, range sliders, steppers, segmented controls and carousel dots use left and right themselves.
- Enter, Space and the south button press the focused control. Escape and the east button go back.
- The menu key and the north button open the context menu around the focus.
- Page Up, Page Down, Home, End and the shoulder buttons page through collections.
- Tab walks the controls in drawing order, also from one text field on to the next.

An app remaps any of them by defining an action with the same name in its action map, and the others keep their built-in bindings.

A screen gives the first focus with `autofocus = true` on a node, or with `gui:command(id, 'focus')`. Every Tiny Island screen does this, so a gamepad player can start at once. Set `focusable = false` to keep a HUD button out of navigation, and call `ui.clearFocus()` to drop the focus when gameplay starts.

The focus stays inside its GUI. Dialogs, popovers and menus keep it until they close, and then give it back to where it was. A node with `focusScope = true` keeps it the same way. A GUI that stops drawing, such as the GUI of a covered scene, remembers its focused control and gives it the focus again once it draws. The property `focusWrap` wraps the focus around the ends of a row or a column. A scroll brings the focused control into view.

Going back is the job of the root of a GUI. The action `uiCancel` first closes an open popup or dialog, or puts back a carried item. Otherwise it sends `cancel` to the innermost focus scope or to the root of the GUI, where a screen goes back with an `onCancel` handler.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Menu = {}
Menu.__index = Menu

function Menu:enter()
    self.gui = ui.mount(ui.column{
        justify = 'center',
        alignItems = 'center',
        gap = 16,
        focusWrap = 'vertical',
        onCancel = function() scene.pop() end,
        ui.button{text = 'Continue', variant = 'primary', width = 480, autofocus = true},
        ui.button{text = 'New game', width = 480},
        ui.button{text = 'Options', width = 480},
    }, {owner = self})
end

scene.push(setmetatable({}, Menu))
```

### The focus ring

The focused control shows a ring in the `focus` color of the theme. It follows the corners of the control. The ring appears once the player navigates with the keyboard, a gamepad or a remote, and a click or a touch hides it again. A focus that your code moves keeps the ring as the player last saw it. It shows the ring at once when a gamepad was the last device used. So a mouse or touch player never sees a ring appear on its own. While the ring is hidden, the first direction only shows it, and the first accept shows it and presses the focused control. Dialogs give the focus to their last button.

### Who gets a press

The keyboard and gamepads drive both the interface and the game, so the focus decides who hears each press. A press that the interface answers never reaches the actions of the app, like a click on a button. While a control has the focus, every navigation key and button and the left stick belong to the interface. A key that closes a popup, a dialog or a carried item stays with the interface too. So an action bound to the same key, such as a `pause` bound to Escape, fires only when the interface answers nothing. The keys and buttons of the interface are those of the navigation actions, remapped or built in. While a text field edits, every key belongs to the field, as the [text input guide](text-input.md#how-a-field-edits) explains. The [input guide](../input/input.md#who-owns-the-keyboard-and-the-gamepad) describes the whole model.

### TV remotes

An Apple TV and an Android TV have no pointer, so the focus ring shows from the start.

- The Siri Remote of an Apple TV moves the focus with swipes on its touch surface and with clicks on its edges. A click presses the focused control, and Menu goes back as `uiCancel`. Its play and pause button reports the pause key and the west button.
- The remote of an Android TV moves the focus with its directional pad, presses with select, and goes back with Back as `uiCancel`. Gamepads work as well.

On the root screen of an app, Menu and Back leave the app, as Apple and Google ask. On other screens, set [`window.setBackLeavesApp`](../lua-api/window.md#windowsetbackleavesappenabled) to `false` to keep them in the app. An open popup or dialog always keeps them.

## Share the screen with the game

Many screens show the game next to their controls, such as a level with a side panel. A [play area](../lua-api/ui.md#uiplayareaproperties) marks the part of a GUI where the game shows, so the focus can go to the game too:

- While the play area has the focus, the directions, accept and menu reach the action map of the game. They never move the focus or press a control.
- Cancel, Tab and the action `uiFocus`, the View button of gamepads by default, stay with the interface. Tab and `uiFocus` move the focus to the controls and back.
- A click or a touch on the play area gives it the focus and still reaches the game.
- The method `gui:bounds(id)` tells where the play area lies, so a camera can draw the world inside it.
- The function `ui.focusOwner()` returns `'playArea'`, `'control'` or `'none'`.

A screen where the game plays next to controls gives the first focus to its play area. Then the keys play the game, and Tab, `uiFocus`, a click or a tap reach the controls. A play area with a `radius` rounds the corners of the game, with the `cornerColor` around them.

```lua
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.defineAction({name = 'move', type = 'vector', up = {'key:up'}, down = {'key:down'}, left = {'key:left'}, right = {'key:right'}, bindings = {'stick:left'}})

local Level = {}
Level.__index = Level

function Level:enter()
    self.x, self.y = 0, 0
    self.gui = ui.mount(ui.row{
        onCancel = function() scene.pop() end,
        ui.playArea{id = 'world', grow = 1, radius = 16, autofocus = true},
        ui.panel{width = 360,
            ui.label{text = 'Level 1', font = 'heading'},
            ui.slider{id = 'zoom', min = 1, max = 4, value = 2},
            ui.button{text = 'Restart'},
        },
    }, {owner = self})
end

-- The hero walks while the world has the focus, and the slider takes the arrows once Tab moved the focus to it.
function Level:update(dt)
    local dx, dy = input.vector('move')
    self.x = self.x + dx * 180 * dt
    self.y = self.y + dy * 180 * dt
end

function Level:render()
    local area = self.gui:bounds('world')
    if area then
        graphics2d.beginScreen()
        graphics2d.drawCircle(area.x + area.width / 2 + self.x, area.y + area.height / 2 + self.y, 24, '#FFF2B23A')
    end
end

scene.push(setmetatable({}, Level))
```

## Scroll long content

Content taller than its room goes into an area that scrolls. Short content goes into a `scroll`. Long lists of data go into a `collection`, as [long lists](#long-lists) explains.

### Scroll areas

A [`scroll`](../lua-api/ui.md#uiscrollproperties) shows one child in an area that scrolls up and down, or sideways with `axis = 'horizontal'`. It measures as tall as its content, so it scrolls only when something bounds it. That is a `height`, a `maxHeight`, or `grow = 1` inside a column, which makes it scroll inside the room its parent leaves. With `snap = true`, it settles on the item nearest to where the player let go. The command `scrollTo` scrolls it from code, to an offset or to a node inside it.

```lua
local ui = require('haylen.ui')

local entries = {}
for index = 1, 40 do
    entries[index] = ui.label{id = 'entry' .. index, text = 'Quest log entry ' .. index}
end

local cards = {}
for index = 1, 8 do
    cards[index] = ui.card{width = 360, ui.label{text = 'Level ' .. index}}
end

local journal = ui.mount(ui.column{padding = 32, gap = 24,
    ui.label{text = 'Journal', font = 'title'},
    ui.scroll{id = 'log', grow = 1, ui.column{gap = 8, children = entries}},
    ui.scroll{axis = 'horizontal', snap = true, height = 240, ui.row{gap = 24, children = cards}},
})
journal:command('log', 'scrollTo', {node = 'entry40', align = 'end'})
```

### Scroll bars

Every area that scrolls keeps its scroll bar in a lane of its own, beside the content. The bar never covers the content and never touches it, so text never runs under a thumb. Content that fits keeps the whole width, and the layout never jumps back and forth as the bar comes and goes. A `scroll` and a `collection` also fade their content into the theme color `scrollFade` along each edge where it goes on. So a card cut by the edge reads as content that continues, not as a broken card. The reference describes the [lane, its metrics and the fade](../lua-api/ui.md#scroll-bars).

### Scrolling with wheels and trackpads

Every area that scrolls follows the device that moves it:

- A trackpad, and the other precise devices of macOS and the web, move the content by the points of the screen they move, at once. So the content follows the fingers.
- Each step of a mouse wheel moves the content by the theme metric `scrollLine`, 64 UI units by default, gliding there. Shift turns a mouse wheel sideways.
- The option `scrollSpeed` of the `input` section of `app.json`, 1 by default, multiplies every scroll before the UI and the app see it. The function `input.setScrollSpeed()` changes it while the app runs, which suits a setting of the player. A theme or a `style` changes `scrollLine` for its areas.

The trackpads and the mouse of macOS report the phases of their gestures. So a `scroll` and a `collection` move under them the way a finger moves them on a touch screen. The content follows the fingers and stretches past an end, with a resistance that grows the farther it goes. Once the fingers leave, the content flings on with the speed it had and bounces at an end, or springs back at once when it lies past one. The momentum that macOS adds after the fingers leave moves nothing there, and fingers that touch the trackpad again stop the fling. The areas without a motion of their own follow that momentum instead: the `textArea`, the body of a `dialog` and the popups. The precise devices of the web report no phases, so they move the content within its range and never stretch it.

Each gesture moves one area:

- A gesture of the wheel or the trackpad keeps the axis it starts along, the one it moves most. It goes to the innermost area under the pointer that can move along that axis in that direction, or else to the innermost one that scrolls along that axis. So a vertical gesture over a horizontal list scrolls the page around it, and only a sideways gesture scrolls the list.
- The gesture stays with that area until it pauses for a moment, or until the pointer moves away while it rests. It stays even when the content brings another area under the pointer. An area at its end leaves the next gesture that scrolls further to the area around it.
- On a touch screen, a finger that drags goes to an area once it travels past a small distance. It picks the innermost area it pressed whose axis the drag follows more than the other axis, and stays with it until it lifts. So a finger that drags up over a horizontal list scrolls the page, and one that drags sideways scrolls only the list.
- A `scroll` and a `collection` fling on after the finger lets go, and stretch past their ends. The focus brings the control it moves to into view at once.

A touch screen tells taps from gestures. A finger that travels past that small distance, or that shares the screen with another finger, is a swipe, a drag or a pinch. So the button, row, chip or other control it pressed lets it go and never clicks, even when the finger lifts over it. Some controls follow the finger and keep it however far it travels. They are a `slider`, a `rangeSlider`, the handle of a `splitter`, the title bar of a `window`, a `carousel`, the picker of a `colorField`, a scroll bar, a notice swiped away and a text field being edited. The windows of `haylen.imgui` keep every finger.

An item that the player drags scrolls the area around it. That covers a row of a draggable `list`, a slot of a `slotGrid` and the lifted item of a reorderable `collection`. Once the item nears an end that the innermost area under it can still move toward, the area scrolls, faster the closer the item gets. A finger picks such an item up only after a long press, so a finger that moves at once scrolls the area instead.

The `scroll` event of a node reports the same distances in UI units. The windows of [`haylen.imgui`](../lua-api/imgui.md) scroll by the same steps and points, in their own units.

## Long lists

The kind [`collection`](../lua-api/ui.md#uicollectionproperties) shows any number of items of several types, in a vertical or horizontal list or in a grid, the way the native lists of phones do. It keeps only the cells it needs. It builds a cell from the template of a type only when no cell of that type is free. It binds the cell to the item it shows, and gives the cell to another item of the same type once it leaves the view. So a list of a hundred thousand items keeps a few dozen cells, and costs about what a list of a hundred does.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'quests', height = 640, selection = 'single', types = {
    chapter = {template = ui.sectionTitle{part = 'title', bind = {text = 'title'}}, sticky = true, interactive = false},
    quest = {template = ui.row{gap = 16,
        ui.label{part = 'name', bind = {text = 'name'}, grow = 1},
        ui.toggle{part = 'done', bind = {checked = 'done'}},
    }},
}, onSelect = function(event) print('Opened ' .. event.item .. '.') end})

local quests = {}
for chapter = 1, 20 do
    quests[#quests + 1] = {id = 'chapter-' .. chapter, type = 'chapter', title = 'Chapter ' .. chapter}
    for quest = 1, 50 do
        quests[#quests + 1] = {id = chapter .. '-' .. quest, type = 'quest', name = 'Quest ' .. quest, done = false}
    end
end
gui:collection('quests'):setItems(quests)
```

### Items, types and cells

Items are Lua tables that your app keeps. Give the collection a list of them with `setItems`, every item with a unique `id` and a `type`. The collection reads only the fields that a cell binds. It changes along with your list through `insert`, `remove`, `move` and `replace`. After you change the fields of items in place, `reload` binds them again. A second `setItems` compares the items by id, so the items that stay keep their sizes, their selection, the focus and their cells. A list too long to build at once loads in pages with `setPages`, whose loader returns a page or a promise of one.

A type has a template: a node tree in which nodes name themselves with `part`, and map their properties to fields of the item with `bind`. When the player changes a bound control, the new value writes back to the item before the handlers run. A binder, a Lua function that `setBinder` registers, fills in what a binding cannot express. Binders and page loaders run in one step per frame, before the GUIs draw, which is the only place where Lua runs while the UI prepares a frame. The state that the UI keeps for a control, such as the knob of a toggle, its focus and its hover, follows the item and not the cell. That works because the ImGui ids of the components of a cell come from the item and from their place in the template. The [reference](../lua-api/ui.md#collections) describes types, templates, bindings and every method of [`UiCollection`](../lua-api/ui.md#uicollection) and [`UiCell`](../lua-api/ui.md#uicell).

### Layout, sizes and anchoring

A list places one item per line. A grid fills `lanes` cells across its axis, or as many as fit at `minCellSize`, and a type with a `span` takes several lanes or the whole line. Every line is as long as its longest cell. An item that was never laid out takes the `estimatedSize` of its type, which the measured items of the type teach when the type declares none. The lines keep their lengths in a Fenwick tree. This structure finds the line at an offset, and the offset of a line, in steps that grow only with the logarithm of the line count. With `cellAspect`, grid cells take their length from their width and need no measuring.

The view holds what the player sees while the items or their sizes change. Before a change, it keeps the focused item, or the first measured item in view. After the change, it puts that item back where it was. So items inserted above, items measured above for the first time and a reordered list never move what shows. A collection with `stickToEnd` that showed its end keeps showing it. The method `scrollTo` aims again every frame while the items on the way are measured. The methods `saveState` and `restoreState` keep a place by item rather than by offset.

### Scrolling and the focus

A collection scrolls like a `scroll`: with the wheel, the trackpad, a finger that flings it, and its scroll bar. With `snap`, it settles on items or whole views. A pull at the start of a `refreshable` list reports `refresh`, so the app can load new items.

The keyboard, gamepads and TV remotes move the focus between items with the directions. The actions `uiPagePrevious`, `uiPageNext`, `uiFirst` and `uiLast`, on Page Up, Page Down, Home, End and the shoulder buttons, page through the innermost collection around the focus. The collection binds the items around the focused one, so the focus moves on to items that do not show yet and scrolls them in. The property `focusAlign = 'center'` keeps the focused item in the middle of the view, as TV lists do. An item whose cell the wheel or a finger took out of view keeps the focus, and the next direction brings it back first. With `rememberFocus`, the focus that enters the collection lands on its last focused item. With `focusWrap` along the axis, the focus wraps from the last item to the first.

### What a long list costs

The command `haylen bench --suite ui` runs the UI benchmark, `engine/bench/UiBenchmark.cpp`, on the headless host. A list and a grid of a hundred thousand items take a fling every 45 frames for 900 frames, and then rest for 300 frames. The items have three types: a sticky header, a plain type and a type with a binder. For each phase, the benchmark prints the average and the worst CPU time of a frame, the items bound, the binder calls and the cells created. At its end, it prints the cells alive. These are the numbers of an Apple M5 Pro in Release:

| Layout | Phase | Frame ms | Worst ms | Binds | Binder calls | Cells created | Cells alive |
| --- | --- | --- | --- | --- | --- | --- | --- |
| List | Fling | 0.063 | 0.365 | 1174 | 385 | 33 | 37 |
| List | Rest | 0.046 | 0.247 | 0 | 0 | 0 | 37 |
| Grid | Fling | 0.088 | 0.481 | 817 | 683 | 93 | 37 |
| Grid | Rest | 0.067 | 0.318 | 0 | 0 | 0 | 35 |

A change of the items costs only the items it touches. The methods `reload` and `replace` check only the ids they add against the set of ids that the collection holds. They rebind only their items, and leave the lines alone while the type of the item stays. The methods `insert`, `remove` and `move` arrange the lines again only from the first item they change. So appending a page to a list of a hundred thousand items costs the items of the page. The one part that grows with the item count is the shift of the Lua list itself, which `table.insert` makes the same way.

A frame at rest binds nothing, calls no binder and creates no cell. The engine tests check that it calls no Lua and that the collection allocates nothing in it. A frame of a fling binds only the items that enter the window. The cells stay bounded by the window, its prefetch and the pools of the types. The components inside the cells keep their own costs, since every visible container measures its children every frame. So cells with few parts keep long lists cheapest.

## Text entry

The text kinds `textField`, `secretField`, `textArea`, `filterField` and `numberField` let the player type. They edit through the native text input of the platform. So phones, tablets, TVs and mobile browsers open their on-screen keyboard. Every platform also brings what its own fields have, such as input methods for Japanese and Chinese, autocorrection, dictation, the emoji picker and paste. The properties `keyboard`, `returnKey`, `autocorrect`, `autocapitalize` and `maxLength` choose the keyboard, as the [reference](../lua-api/ui.md#uitextfieldproperties) lists.

The native field stays hidden, and the field of the UI draws the text, the caret and the selection itself. The suggestions of the keyboard stay in the keyboard. A finger starts editing a field when it taps it, as on a phone, and the caret goes where the tap lifted. A finger that presses a field and drags scrolls the area around it instead. A finger that drags over the field being edited also scrolls the area, while the field keeps editing, since a finger selects no text by dragging. The mouse places the caret and selects text by dragging, as on desktops.

When the on-screen keyboard would cover the focused field, every mounted GUI moves up together until the field shows, and moves back when the keyboard closes. The GUIs hold their place while a press is down. So a tap on a send button next to the field still lands on the button, even when it ends the editing and closes the keyboard. The events `keyboardShown` and `keyboardHidden` of [`haylen.events`](../lua-api/events.md) tell the app where the keyboard is. The [text input guide](text-input.md) explains how each platform does all this.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{gap = 16,
    ui.formField{label = 'Email', ui.textField{placeholder = 'you@example.com', keyboard = 'email', returnKey = 'next'}},
    ui.formField{label = 'Password', ui.secretField{returnKey = 'go', onSubmit = function(event)
        print('Signing in.')
    end}},
    ui.formField{label = 'About you', ui.textArea{rows = 4, maxLength = 500}},
})
```

## Text and translations

Every text property takes a string, a number or a translation such as `{key = 'hud.day', args = {day = 3}}`. The UI resolves a translation through [`haylen.localization`](../lua-api/localization.md) every time it draws the text. So switching the language changes every mounted GUI at once, without building anything again. Plural forms pick their `zero`, `one` or `other` text from the `count` argument.

Once a GUI has drawn, its frames allocate no memory. A literal text reaches the renderer without a copy. The texts that a frame makes, such as the bullets of a password field or a line shortened with an ellipsis, live in storage that the next frames reuse. A translation is the exception, since the catalog formats it again, with its arguments, every time the text draws.

Tiny Island wraps translations in a small helper, `widgets.text`, and its settings screen switches the language live through `localization.setLanguage`.

```lua
local ui = require('haylen.ui')

local widgets = {}

function widgets.text(key, arguments)
    return {key = key, args = arguments}
end

ui.mount(ui.column{
    ui.label{text = widgets.text('menu.best', {count = 12})},
    ui.button{text = widgets.text('menu.play'), variant = 'primary'},
})
```

Styled text goes in a [`richText`](../lua-api/ui.md#uirichtextproperties) node. It reads the BBCode markup of the [text guide](../graphics/text.md). The markup gives bold and italic, colors, outlines and shadows, lists and tables, and inline images and input prompt icons. It also gives links that the player can focus and activate, and animated effects with a typewriter reveal for dialogue.

```lua
local ui = require('haylen.ui')

ui.mount(ui.richText{id = 'npc', text = 'The [b]old sailor[/b] says: [i]"Mind the [color=#FF6A6A]crabs[/color]."[/i] [url=more]Ask more[/url]', revealSpeed = 30, onLink = function(event)
    print('The player asked for ' .. event.link .. '.')
end})
```

## Complex scripts and right-to-left interfaces

ImGui lays out and draws the widgets, while the text layout of the engine sets their text. Every component measures its text with the shaped layout of the font family of its role. It draws the text through the 2D renderer, at its place among the ImGui draws and inside their clip, moved, scaled and tinted by the transforms around it. So labels, buttons, fields, lists, tables, tooltips, dialogs and rich text show Arabic, Hebrew, Persian, Urdu, Hindi, Thai and every other script that the fonts of the family cover. They join letters, build conjuncts, place marks and order mixed text in both directions, as the [text guide](../graphics/text.md#scripts-and-directions) describes. The UI caches layouts by text and style, so a label that stays the same shapes once. The fonts of ImGui itself only serve the text editing of fields, which draws nothing, and the windows of `haylen.imgui`.

Every paragraph of the UI reads in the direction of its first strong letter. So an Arabic label reads from the right and an English one from the left, in any interface. The direction of the layout is separate, and it is left to right unless the app changes it. The call `ui.setDirection('rightToLeft')` mirrors every GUI. Rows start from the right, check boxes put their box on the right and sliders fill from the right. The arrow keys move sliders the way they point. A node with `direction = 'rightToLeft'` or `'leftToRight'` sets the direction of its own subtree. The property `language` sets the language that its text is shaped for, which is otherwise the current language. The reference lists [everything that mirrors](../lua-api/ui.md#right-to-left-interfaces).

A language declares its direction in its catalog with `"@direction": "rightToLeft"`, as [`haylen.localization`](../lua-api/localization.md) describes. The call `ui.setDirection('auto')` makes the UI follow it. Then switching to Arabic or Hebrew mirrors every mounted GUI from the next frame, and switching back restores it. Apps opt in, because a game may prefer to keep its HUD in one layout whatever the language. Anchors and four-sided padding stay where they are, and images never flip.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

ui.addFont('world', graphics.newFontFamily({regular = assets.font('fonts/body.ttf'), fallbacks = {assets.font('fonts/noto-sans-arabic-regular.ttf'), assets.font('fonts/noto-sans-devanagari-regular.ttf')}}))
local role = {font = 'world'}
ui.setTheme(ui.addTheme({name = 'world', fonts = {body = role, caption = role, button = role, heading = role, title = role}}, 'dark'))
ui.setDirection('auto')
localization.setLanguage('ar')
ui.mount(ui.card{
    ui.label{text = {key = 'menu.welcome'}},
    ui.row{ui.button{text = {key = 'menu.play'}, variant = 'primary'}, ui.button{text = {key = 'menu.quit'}}},
    ui.column{direction = 'leftToRight', ui.label{text = 'v1.4.2'}},
})
```

Text fields show complex scripts and right-to-left text the same way, and their caret moves over whole characters in the order they show, as the [text input guide](text-input.md#complex-scripts-and-right-to-left-text) explains.

## Pictures

Pictures, such as the `image` of `ui.image`, `ui.icon`, `ui.avatar` and list items, and the `icon` of buttons, are image files or SVG documents of the package. Prefer SVG for icons and illustrations. The UI rasterizes an SVG document on the job system for the pixels it covers on the screen, in steps a quarter of an octave apart. Until a new raster arrives, it draws with the nearest raster it has. So icons stay sharp on every density, at every UI scale and while they animate, from files of a few hundred bytes.

The color `currentColor` of an SVG document paints white, so the UI can tint it. The `color` of `ui.icon` tints an icon drawn with it. Toolbar and icon buttons tint it like their text, and the `iconColor` of a button tints it in every variant. The pictures of the items of lists, trees and menus take the color of the text of their item in every theme and state. So a line icon stays readable on a light theme and on a dark one. An item names another theme color with `iconColor`, or keeps the colors of a painted picture with `iconColor = false`.

The rasters that no frame drew lately go once they pass a memory budget, `ImageLibrary::kRasterBudget` in `engine/src/ui/ImageLibrary.hpp`. So a long list of pictures keeps only the memory of the pictures in view. An SVG document that changes during hot reload shows once the app restarts. The property `radius` of `ui.image` rounds the corners of a picture, such as a photo on a card.

An image made while the app runs, such as a minimap from a render target, takes a name that starts with `@` through [`ui.setImage`](../lua-api/ui.md#uisetimagename-source). Every picture of the UI then shows it like a file.

```lua
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

ui.setImage('@minimap', graphics.newRenderTarget(240, 160))
ui.mount(ui.row{gap = 16,
    ui.icon{image = 'icons/heart.svg', size = 48, color = 'danger'},
    ui.image{image = 'ui/photo.jpg', width = 320, height = 200, fit = 'cover', radius = 16},
    ui.image{image = '@minimap', width = 240, height = 160},
    ui.button{text = 'Settings', icon = 'icons/gear.svg', iconColor = 'accentText'},
})
```

## Touch controls

The kinds [`touchStick`](../lua-api/ui.md#uitouchstickproperties) and [`touchButton`](../lua-api/ui.md#uitouchbuttonproperties) put on-screen controls in a GUI. They drive the virtual sticks and buttons that the [action map](../input/input.md#the-action-map) reads. A stick with `action = 'move'` feeds every action bound to `virtualStick:move`. A button with `action = 'attack'` feeds every action bound to `virtual:attack`. So gameplay code reads `input.vector('move')` and `input.pressed('attack')` the same way for keyboards, gamepads and touch.

- Each control follows its own finger, so a stick and several buttons work at the same time.
- A stick reports a vector of length 0 to 1, without its dead zone. A stick with `mode = 'floating'` centers where the finger lands inside its area, and one with `mode = 'following'` also follows a finger that leaves its ring. So give them a generous `width` and `height`.
- With `touchOnly = true`, a control shows only while the last input came from a touch screen, as `input.lastDevice()` reports it. Keyboard and gamepad players never see it.
- A control that stops drawing lets go of what it held. The actions read the values of the controls at the start of the next frame.
- A `touchButton` also reports `press` and `release`, for feedback such as sounds. With `slideIn = true`, it also takes a finger that slides onto it, so a thumb that rolls from one button to the next types sequences the way action games read them.

The Tiny Island HUD puts a floating stick in a large area at the bottom left, and three buttons at the bottom right, all touch-only. They sit inside a row whose visibility follows the touch controls setting of the player. The buttons show the icons of the attack and the special move of the chosen class.

```lua
local input = require('haylen.input')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

input.loadActions({actions = {
    {name = 'move', type = 'vector', bindings = {'virtualStick:move', 'stick:left'}},
    {name = 'attack', type = 'button', bindings = {'virtual:attack', 'key:j', 'button:south'}},
    {name = 'special', type = 'button', bindings = {'virtual:special', 'key:k', 'button:west'}},
    {name = 'interact', type = 'button', bindings = {'virtual:interact', 'key:e', 'button:north'}},
}})

ui.mount(ui.row{
    id = 'touch',
    align = 'stretch',
    visible = preferences.get('touch', true),
    ui.touchStick{action = 'move', radius = 140, mode = 'floating', touchOnly = true, width = 760, height = 420, align = 'end'},
    ui.spacer{grow = 1},
    ui.column{
        gap = 20,
        align = 'end',
        ui.row{
            gap = 24,
            ui.touchButton{action = 'interact', image = 'ui/log.png', size = 120, touchOnly = true},
            ui.touchButton{action = 'special', image = 'ui/special.png', size = 140, touchOnly = true},
        },
        ui.touchButton{action = 'attack', image = 'ui/attack.png', size = 190, align = 'end', touchOnly = true},
    },
}, {placement = 'screen'})
```

The theme surfaces `stickBase`, `stickKnob`, `touchButton` and `touchButtonPressed` give the controls their art, and a `style` gives art to one control. Without them, the controls draw flat in the theme colors.

```lua
local ui = require('haylen.ui')

local art = {
    stickBase = {image = 'ui/stick-base.png', filter = 'linear'},
    stickKnob = {image = 'ui/stick-knob.png', filter = 'linear'},
}
ui.mount(ui.touchStick{action = 'move', mode = 'following', radius = 130, width = 600, height = 420, style = {surfaces = art}}, {placement = 'screen'})
```

## Scale and screen density

A GUI lays out in UI units, and the scale mode decides how big a UI unit is:

- In the default mode `'design'`, a UI unit is a design unit. The interface grows and shrinks with the screen like the rest of the app. That suits games drawn for one screen shape, and televisions.
- In the mode `'physical'`, a UI unit keeps the same physical size on every screen, from the density that the platform reports. So buttons and text are as large on a phone as on a desktop monitor. The shorter side of the visible area always holds at least 480 units. The room that the UI has then changes with the screen, and layouts that grow, wrap and fit their columns follow it.

Set the mode with `ui.setScaleMode('physical')`, or with `"ui": {"scaleMode": "physical"}` in `app.json`. The factor of `ui.setScale`, such as a size setting of the player, multiplies both modes. The function `ui.scale()` reports the factor, the design units that a UI unit spans and the points of the screen that it spans. The reference describes [the scale functions](../lua-api/ui.md#scale).

```lua
local preferences = require('haylen.preferences')
local system = require('haylen.system')
local ui = require('haylen.ui')

if system.info().deviceKind == 'phone' then
    ui.setScaleMode('physical')
end
ui.setScale(preferences.get('interfaceSize', 1))
```

## Smooth edges

Every curve of the components fades over one pixel of the screen, in both scale modes, at any scale factor, screen density and graphics API. The components draw their rounded surfaces, borders, focus rings, shadows, circles, rings, arcs, pills, check marks, arrows, crosses, lines, selections and carets as [shapes](../graphics/rendering.md#shapes-and-smooth-edges) of the 2D renderer. The shader of a shape covers it exactly from the distance to its edge. So the shapes stay smooth however much the UI or the transform of a node scales them, and inside the image of a transition.

- A surface paints its fill and its border in one shape. So the border runs inside the same edge, follows its corners and meets the fill without a seam.
- The focus ring is the border of a shape, `focusGap` away from its control, with corners that are concentric to the corners of the control.
- The shadow of a floating surface is the shape of the surface, moved down by `shadowOffset`. Its edge fades over twice `shadowSize`, centered on the edge, as a blur does. So the shadow starts at half the `shadow` color along the edge of the surface, and clears `shadowSize` away from it. A toast floats lower than dialogs, windows and popups. So its shadow is smaller, nearer and lighter, and the notices of a stack never darken each other.
- The highlight of a selected, hovered or pressed row of a list or a collection inside a card or a panel rounds its corners to match. The corners that meet the corners of the card take the radius of the card less the space between them, and the other corners keep the radius of the rows.
- The shapes reach the renderer at their place among the ImGui draws, through callbacks of the draw list, and so do the texts, rich texts and nine-slices of the components. Draws in a row in one window, under the same clip and texture, share one callback. The shapes of neighboring controls merge into one draw call, and each frame keeps every draw in storage that later frames reuse.

The pictures of the components, such as the rounded portrait of an avatar, and the windows of `haylen.imgui` go through ImGui. ImGui tessellates and fades their curves in pixels of the screen, as [immediate mode UI](#immediate-mode-ui-with-haylenimgui) explains. The nine-slice images of a [theme surface](#surfaces) keep the edges of their art, sampled with the `filter` that the surface asks for, such as `linear` for painted art. The test `GUI-031` of the test project shows every curved component and shape at the design and physical scales, next to the pixels of their edges magnified.

## Accessibility

The UI works with the accessibility features of each platform on its own. You add only what a component cannot know.

### Screen readers

The GUIs describe themselves to the screen readers of each platform. Every control, text and heading becomes an element with its role, label, value and state. The focus of the screen reader and the focus of the UI stay in step. Screen readers press and adjust controls through the same paths as the keyboard and gamepads. The common properties `accessibilityLabel`, `accessibilityHint`, `accessibilityRole` and `accessibilityHidden` say what a kind cannot know, such as the name of an icon button. The page of [`haylen.accessibility`](../lua-api/accessibility.md#screen-readers) lists what each kind says and where each platform reads it.

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.button{icon = 'icons/bag.svg', variant = 'icon', accessibilityLabel = 'Inventory'},
    ui.button{icon = 'icons/map.svg', variant = 'icon', accessibilityLabel = 'Map', accessibilityHint = 'Shows the whole island'},
})
```

### Accessibility settings

The UI follows the [accessibility settings](../lua-api/accessibility.md) of the system without any code:

- While the system asks for higher contrast, the UI draws the high contrast theme that the chosen theme names. The engine ships `darkHighContrast` and `lightHighContrast`, with white or black text, borders of full contrast, and a yellow or deep blue focus ring.
- While the system asks for reduced transparency, the UI draws the colors `window`, `panel`, `raised` and `tooltip` of the theme opaque.
- The text scales with the text size of the system. Every font role grows, and the controls around the text grow with it. That lasts until the app sets a scale of its own with [`ui.setTextScale`](../lua-api/ui.md#uisettextscalescale), such as from a setting of the player.

The function `ui.theme()` keeps returning the theme that the app chose. The function `ui.themeColor` and the other readers of the theme return what the UI draws.

```lua
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

-- The player may pick a size, or keep the size of the system.
ui.setTextScale(preferences.get('textScale', 'system'))
```

## Component catalog

The engine ships 64 component kinds, and `ui.kinds()` lists them. This table groups them by purpose, and each group links to its section of the reference.

| Purpose | Kinds |
| --- | --- |
| [Containers](../lua-api/ui.md#containers) | `column`, `row`, `grid`, `stack`, `scroll` (vertical or horizontal, with snapping), `card`, `panel`, `spacer`, `divider`, `tabs`, `accordion`, `carousel`, `formField`, `splitter`, `safeArea` |
| [Text](../lua-api/ui.md#text) | `label`, `richText` (markup with links, effects and a typewriter reveal), `pageHeader`, `sectionTitle`, `emptyState`, `alert` |
| [Buttons](../lua-api/ui.md#buttons) | `button` (variants `default`, `primary`, `destructive`, `toolbar`, `icon` and `link`), `imageButton`, `chip`, `menuButton`, `popover` |
| [Choices](../lua-api/ui.md#choices) | `checkbox`, `toggle`, `radioGroup`, `combo`, `segmentedControl` |
| [Inputs](../lua-api/ui.md#inputs) | `textField`, `secretField`, `textArea`, `filterField`, `numberField`, `slider`, `rangeSlider`, `stepper`, `colorField`, `keyCapture` |
| [Indicators](../lua-api/ui.md#indicators) | `badge`, `statusIndicator`, `busyIndicator`, `progress`, `circularProgress`, `icon`, `image`, `avatar` |
| [Lists and grids](../lua-api/ui.md#lists-and-grids) | `list` (with draggable rows), `tree`, `table`, `slotGrid`, `collection` (any number of items of several types, as [long lists](#long-lists) explains) |
| [Settings](../lua-api/ui.md#settings) | `settingsForm`, `settingsRow`, `settingsActions` |
| [Overlays](../lua-api/ui.md#overlays) | `dialog`, `toast`, `window`, `contextMenu` |
| [Touch controls](../lua-api/ui.md#touch-controls) | `touchStick`, `touchButton` |
| [Play areas](../lua-api/ui.md#play-areas) | `playArea` |

Games reach for a few kinds often:

- A `circularProgress` with `variant = 'cooldown'` shades an ability icon while it recharges.
- A `stepper` and a `segmentedControl` pick settings with left and right, the way console menus do.
- A `slotGrid` holds inventories and hotbars. It moves items between slots, draggable lists and other GUIs, with the pointer, and by carrying them with a gamepad or a remote.
- A `keyCapture` reads the next key, mouse button, gamepad button or stick for a controls screen, and returns a binding that the action map takes.
- A `window` floats a draggable panel over the game, and a `contextMenu` opens actions on a right click, a long press or `uiMenu`.

The Tiny Island screens use a small part of the catalog. The menu is the logo as an `image` over a column of buttons. The class selection screen uses a `pageHeader` with `banner = true`, `imageButton` portraits whose `tint` dims the classes that are not chosen, and `progress` bars for the stats. The settings sheet is a `panel` with a `settingsForm` of `settingsRow` nodes that hold sliders, a combo and toggles. The HUD uses panels, icons, labels, progress bars, an icon button, a toast and the touch controls.

## Immediate mode UI with haylen.imgui

The module [`haylen.imgui`](../lua-api/imgui.md) is Dear ImGui for Lua. Your app builds its windows again every frame from its own state, usually in the `renderUi` callback of a scene. Widgets that edit a value return whether it changed, and the new value.

```lua
local imgui = require('haylen.imgui')
local scene = require('haylen.scene')

scene.push({
    speed = 120,
    renderUi = function(self)
        if imgui.beginWindow('Tuning', {x = 20, y = 20, width = 420, height = 200}) then
            local changed
            changed, self.speed = imgui.sliderFloat('Speed', self.speed, 0, 400)
            if imgui.button('Reset') then
                self.speed = 120
            end
        end
        imgui.endWindow()
    end,
})
```

ImGui windows use the colors, metrics and body font of the active theme. They pad the thumb of their scroll bars by the [gap](#scroll-bars) on every side, so their content keeps it too. While the pointer is over one, `ui.usingPointer()` returns `true`. ImGui tessellates the curves it draws itself by the pixels they span on the screen. Those are the rounded corners of its windows and frames and the circles of its widgets. It fades every edge over one pixel of the screen, whatever the scale of the UI and the density of the screen. Calls outside a running frame, such as at the top level of `source/main.lua`, raise an error. The debug overlay of the engine is built with it: the full mode of the statistics that F3 cycles through, which [`haylen.debug`](../lua-api/debug.md) controls.

## A GUI of your own in Lua

An app can also draw a GUI of its own from Lua, every frame or retained, on the same APIs that the built-in UI uses:

- Drawing takes rectangles, circles, lines, polygons, meshes, nine-slices, textures and text from [`haylen.graphics2d`](../lua-api/graphics2d.md). Use `measureText` to lay text out, `pushClip` to keep a list inside its box, and render targets and shaders for masks and effects.
- Input takes the mouse, the fingers, the keys, typed characters and gamepads from [`haylen.input`](../lua-api/input.md). The function [`input.editText`](../lua-api/input.md#text-fields-of-the-app) edits a field through the native text input, with the on-screen keyboard and input method composition.
- The cursor comes from `window.setCursor`, the safe area from `viewport.safeRect` and the scale from `ui.scale`.

The test `GUI-027` of the test project is such a GUI. It has buttons, a check box, a slider that holds the pointer while it drags, a text field, a clipped scrolling list, and focus that keys, gamepads and remotes move.

## From C++

C++ code reaches the same system through `haylen::plugins::UiPlugin` (`haylen/plugins/UiPlugin.hpp`), which `engine.getPlugin<plugins::UiPlugin>()` returns. The [architecture guide](../core/architecture.md) explains plugins, and the [Lua guide](../core/lua.md) explains the scripting model.

- GUIs: `createGui` builds a `haylen::ui::Gui` from JSON, and `mount` shows it and publishes `guiMounted` on the event bus of the engine. The method `unmount` publishes `guiUnmounted`. The plugin publishes every `haylen::ui::Event` of a GUI through its `events` signal.
- Themes: the plugin loads and switches themes, `haylen::ui::Theme` in `haylen/ui/Theme.hpp`. The method `UiPlugin::addFontFamily` registers a `haylen::text::FontFamily` under a font name, which reaches the backend as `Backend::FontFiles`, its TrueType faces and fallbacks.
- Components: `UiPlugin::getComponents()` returns the `haylen::ui::ComponentRegistry`, where a C++ plugin registers its own component kinds. A kind is a subclass of `haylen::ui::Component`, which reads its properties with a `PropertyReader` and draws through the `haylen::ui::Context` it receives. Lua then builds it with `ui.<kind>{...}`, like the built-in kinds.
- Drawing: `UiPlugin::getBackend()` returns the `haylen::ui::Backend`, which owns the Dear ImGui context, for C++ code that draws immediate mode windows. Its `addRenderCallback` lets a component draw with the 2D renderer at its place among the ImGui draws. Its `addShape`, `addText`, `addRichText` and `addNineSlice` draw a `graphics2d::Shape`, a text, a `text::RichText` and a `graphics2d::NineSlice` there without a callback of their own. That is how the components draw their curves, texts and theme images. Its `beginChild` begins a child window that draws at its place among the draws of its parent, as `scroll` does, so the nodes after it cover it.
- Long lists: a `haylen::ui::Collection` node (`haylen/ui/Collection.hpp`) is what `Gui::getCollection(id)` finds. A C++ app gives it items through a `haylen::ui::CollectionSource`, such as the ready `haylen::ui::CollectionItems`. It binds cells with `Collection::setBinder` and a `haylen::ui::CollectionCell`, and places items its own way with a `haylen::ui::CollectionLayout`.
- Focus: `UiPlugin::getFocus()` returns the `haylen::ui::FocusNavigator`, which tells which GUI and node hold the focus and whether the ring shows. `Gui::setSharedFocus` joins a GUI to the navigation space of the GUIs that share the focus. `UiPlugin::getNavigation()` returns the `haylen::ui::NavigationInput` with the navigation actions. A C++ component makes an ImGui item a focus target with `FocusNavigator::addTarget` while it draws, and keeps a direction for itself by overriding `Component::usesFocusDirection`.
- Screen: the methods `core::Engine::setSafeAreaSimulation`, `setFoldSimulation` and `setBackLeavesApp` are the C++ side of the safe area simulation, the [fold simulation](../lua-api/window.md#folds-and-dual-screens) and the back button. The methods `setScaling` and `setDesignSize` change the mapping of design space that the UI lays out in, while the app runs.
- Text fields reach the platform through `haylen::platform::TextInput`, as the [text input guide](text-input.md#from-c) describes.
