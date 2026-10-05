# haylen.ui

The module `haylen.ui` builds the app interface, such as menus, HUDs, settings screens, dialogs and on-screen touch controls, as retained trees of themed components. An app mounts a tree once, changes nodes by id later and reacts to the player with `onX` handlers. Use it for every interface the player sees. For debug windows rebuilt every frame, use [`haylen.imgui`](imgui.md) instead.

```lua
local ui = require('haylen.ui')
```

## GUIs

A GUI is a tree of nodes. Every node is one flat table with a `kind`, an optional `id`, its children and the properties of its kind. The function `ui.mount` builds the components, draws them over the app every frame and returns a [`Gui`](#gui) that stays mounted until it is unmounted. Several GUIs can be mounted at once. A scene mounts its GUIs with itself as `owner`, so they show only while the scene shows, go through scene transitions with it and unmount when it unloads, and a GUI without a scene owner stays on screen above the scenes.

Sizes and positions are design units of the design resolution in `app.json`. The metrics of the built-in themes suit the default design resolution of 1920 by 1080.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    enter = function(self)
        self.menu = ui.mount(ui.column{
            align = 'center',
            gap = 24,
            ui.label{text = 'Tiny Island', font = 'title'},
            ui.button{id = 'play', text = 'Play', variant = 'primary', onClick = function(event)
                event.gui:set('status', {text = 'Loading the island'})
            end},
            ui.label{id = 'status', text = ''},
        }, {owner = self})
    end,
})
```

## Functions

### ui.mount(tree, options)

Builds a GUI from the node table `tree`, shows it and returns its [`Gui`](#gui). The options table is optional, and unknown keys raise `Unknown option "<key>".`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `placement` | string | `'safe'` | The value `'safe'` lays the root out inside the safe area of the screen, away from notches and system bars. The value `'screen'` lays it out over the whole visible screen. |
| `layer` | integer | `0` | GUIs draw in ascending layer order, and GUIs on the same layer draw in the order they were mounted, so later ones cover earlier ones. |
| `owner` | table or userdata | `nil` | Unmounts the GUI when the owner is released, such as a scene when it unloads, or collected, like the other [owners of `haylen.events`](events.md#owners). A scene of the stack as owner also makes the GUI part of the scene: it draws only while the scene shows, and during a transition it goes into the image of its scene. |

The root fills the whole area when its `align` is `stretch`, which is the default of containers. With `start`, `center` or `end` the root keeps its measured size and sits at the top left, the center or the bottom right of the area.

A placement other than `safe` or `screen` raises `The "placement" option must be "safe" or "screen".`, and an owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.` A tree that breaks the [screen format](#screen-format) or has an invalid property raises an error that names the problem, such as `There is no UI component kind named "spaceship".` or `The property "width" of a "label" must be a non-negative number or "auto".`, and nothing is mounted. Mounting publishes the `guiMounted` event of [`haylen.events`](events.md) with the GUI.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.row{
    align = 'start',
    padding = 16,
    gap = 12,
    ui.icon{image = 'ui/wood.png'},
    ui.label{id = 'wood', text = 0},
}, {placement = 'screen', layer = 1})
```

A scene that owns its GUIs needs no `exit` of its own to unmount them, and no `pause` and `resume` to hide them while another scene covers it. Its GUIs never show over the scene that covers it, not even while the transition plays.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    enter = function(self)
        ui.mount(ui.label{text = 'Level 1'}, {owner = self})
    end,
})
```

### ui.node(kind, properties)

Returns a node table of the given kind. It sets `kind` on the `properties` table and returns that same table, or a new table when `properties` is omitted. It is the long form of `ui.<kind>{...}`, useful when the kind comes from a variable. An unknown kind raises `There is no UI component kind named "<kind>".`.

```lua
local ui = require('haylen.ui')

local kind = 'button'
local tree = ui.node('column', {
    id = 'list',
    children = {ui.node(kind, {text = 'First'}), ui.node(kind, {text = 'Second'})},
})
local gui = ui.mount(tree)
```

### ui.&lt;kind&gt;(properties)

Every component kind is also a function of the module, so `ui.button{text = 'Play'}` is the same as `ui.node('button', {text = 'Play'})`. Reading any other missing name from the module raises `The module "haylen.ui" has no member "<name>".`.

```lua
local ui = require('haylen.ui')

local card = ui.card{
    ui.sectionTitle{text = 'Inventory'},
    ui.label{text = 'Empty'},
}
ui.mount(card)
```

### ui.kinds()

Returns a sorted list of the names of every component kind, the 64 kinds documented on this page.

```lua
local ui = require('haylen.ui')

for _, kind in ipairs(ui.kinds()) do
    print(kind)
end
```

### ui.setTheme(name)

Switches the theme of every GUI and of [`haylen.imgui`](imgui.md) windows. The engine ships the themes `dark`, which is active at start, and `light`. An unknown name raises `The UI has no theme named "<name>".`.

```lua
local ui = require('haylen.ui')

ui.setTheme('light')
```

### ui.theme()

Returns the name of the active theme.

```lua
local ui = require('haylen.ui')

if ui.theme() == 'dark' then
    ui.setTheme('light')
end
```

### ui.themes()

Returns a sorted list of the names of every registered theme.

```lua
local ui = require('haylen.ui')

print(table.concat(ui.themes(), ', '))
```

### ui.loadTheme(path, base)

Reads a [theme file](#themes) from the package assets, registers the fonts its `fontFiles` lists and registers the theme under its name. The theme starts as a copy of the registered theme `base`, which defaults to `'dark'`, so the file only lists what it changes. A theme with the name of a registered theme replaces it, and when that theme is active the change shows at once. Returns the theme name without switching to it.

Errors raised:

- The error `The UI has no theme named "<base>" to start from.` for an unknown base.
- The error `The theme "<name>" uses the font "<font>", which is neither registered nor listed in "fontFiles".` when a font role names an unknown font.
- The error of the asset system when the file is missing, and the errors of the [theme format](#themes) for an invalid file.

```lua
local ui = require('haylen.ui')

local name = ui.loadTheme('themes/wood.json', 'light')
ui.setTheme(name)
```

### ui.addTheme(definition, base)

Registers a theme from a table in the [theme file](#themes) format, the way `ui.loadTheme` registers a file, and returns the theme name without switching to it. The theme starts as a copy of the registered theme `base`, which defaults to `'dark'`. It raises the errors of `ui.loadTheme`, and a `definition` that is not a table raises an argument error.

```lua
local ui = require('haylen.ui')

local name = ui.addTheme({
    name = 'night',
    colors = {accent = '#FF00AA88'},
    metrics = {controlHeight = 72},
    fonts = {title = {size = 64}},
}, 'dark')
ui.setTheme(name)
```

### ui.themeColor(role)

Returns a [color role](#theme-colors) of the active theme as a `haylen.Color` from [`haylen.math`](math.md). An unknown role raises `The theme has no color role named "<role>".`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawRect({40, 40, 200, 12}, ui.themeColor('accent'))
    end,
})
```

### ui.themeMetric(name)

Returns a [metric](#theme-metrics) of the active theme in design units. An unknown metric raises `The theme has no metric named "<name>".`

```lua
local ui = require('haylen.ui')

local rowHeight = ui.themeMetric('listRowHeight')
ui.mount(ui.list{height = rowHeight * 5, items = {{id = 'first', text = 'First'}}})
```

### ui.themeFont(role)

Returns a [font role](#theme-fonts) of the active theme as a table with the fields `font`, the registered font name, `size`, in design units, and `bold` and `italic`, the style of the face the role draws with. An unknown role raises `The theme has no font role named "<role>".`

```lua
local ui = require('haylen.ui')

local title = ui.themeFont('title')
print(title.font, title.size, title.bold, title.italic)
```

### ui.themeImageFilter()

Returns the texture filter that the pictures of components load with in the active theme, `'nearest'` or `'linear'`, which the `imageFilter` key of a [theme](#themes) sets.

```lua
local ui = require('haylen.ui')

ui.setTheme(ui.addTheme({name = 'painted', imageFilter = 'linear'}))
print(ui.themeImageFilter())
```

### ui.themeSurface(surface)

Returns the image the active theme paints a [surface](#theme-surfaces) with, or `nil` when the theme paints it with flat colors. The table holds `slice`, a `haylen.NineSlice` for `graphics2d.drawNineSlice`, `scale`, `padding` as `{top, right, bottom, left}`, `tint`, a `haylen.Color`, and `colorize`. An unknown surface raises `The theme has no surface named "<surface>".`

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    render = function(self)
        local panel = ui.themeSurface('panel')
        if panel then
            graphics2d.beginScreen()
            graphics2d.drawNineSlice(panel.slice, {100, 100, 400, 240}, panel.tint, nil, panel.scale)
        end
    end,
})
```

### ui.addFont(name, source)

Registers a font under `name`, from a TrueType or OpenType file of the package assets, or from a `FontFamily` or a `Font` of [`haylen.graphics`](graphics.md). Theme font roles, `imgui.pushFont` and the `[font=name]` tags of `ui.richText` refer to fonts by this name. Fonts are sized when drawn, so one registered font serves every size. The built-in font is named `default`.

A family gives every text component of the roles that name it its faces and fallback fonts. Labels, buttons and the other components draw with its regular face, or with its bold, italic or bold italic face when the role asks for that style and the family has the face, and draw every character a face lacks from the fallback fonts, such as Japanese text from a CJK fallback, at the size of the em square of the face. The component `ui.richText` draws with every face and fallback of the family, mono faces included. A theme role can name a family whose regular face is a TrueType or OpenType font, and those components take the TrueType faces and fallbacks of the family. A bitmap font or a family with a bitmap regular face serves `ui.richText` and `[font=name]` tags only.

A name that is already registered raises `The UI already has a font named "<name>".`, and a file that is not a font raises `The font "<name>" is not a TrueType or OpenType font.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

ui.addFont('serif', 'fonts/serif.ttf')
ui.addFont('story', graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), bold = assets.font('fonts/serif_bold.ttf'), fallbacks = {assets.font('fonts/cjk.ttf')}}))
ui.addFont('pixel', assets.font('fonts/pixel.fnt', {filter = 'nearest'}))

-- Labels draw Japanese from the fallback, and headings draw with the bold face.
ui.setTheme(ui.addTheme({name = 'story', fonts = {body = {font = 'story'}, heading = {font = 'story', bold = true}}}))
ui.mount(ui.column{ui.label{text = 'Chapter 1', font = 'heading'}, ui.label{text = '灯台守の物語'}})
```

### ui.usingPointer()

Returns `true` while the pointer is over something the interface owns: an interactive component, an open dialog, menu or picker, a [`haylen.imgui`](imgui.md) window, or a card, panel, touch stick or touch button, which keep the pointer from reaching the app. Empty space inside columns, rows and stacks lets the pointer through, even while a mouse button is held down on it. Apps check it before they treat a raw click or tap as a world action. The action map of [`haylen.input`](input.md) checks it on its own: while it is `true`, `mouse:` bindings read as released, so clicking a menu button never triggers an action bound to the same mouse button.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if input.mousePressed('left') and not ui.usingPointer() then
            print('clicked the world')
        end
    end,
})
```

### ui.usingKeyboard()

Returns `true` while the interface uses the keyboard, such as when a text field has the focus. Apps check it before they treat key presses as gameplay input.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if input.keyPressed('space') and not ui.usingKeyboard() then
            print('jump')
        end
    end,
})
```

### ui.focused()

Returns the [`Gui`](#gui) and the node id that hold the keyboard, gamepad and remote focus, or `nil` when no mounted GUI holds it. A focused node without an id returns the GUI alone. Inside a cell of a [`collection`](#uicollectionproperties) the node id is the id of the collection, followed by the id of the item and the part of the template that holds the focus, which is `nil` for a cell that takes the focus as a whole. The [focus guide](#focus-and-navigation) explains how the focus moves.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local menu = ui.mount(ui.column{
    ui.button{id = 'play', text = 'Play', autofocus = true},
    ui.button{id = 'quit', text = 'Quit'},
    ui.label{id = 'hint', text = ''},
})

scene.push({
    update = function(self, dt)
        local gui, id = ui.focused()
        if gui == menu then
            menu:set('hint', {text = id == 'quit' and 'Leave the island' or 'Start a new run'})
        end
    end,
})
```

### ui.focusOwner()

Returns what holds the keyboard, gamepad and remote focus: `'control'` for a control of a GUI, `'playArea'` for a [play area](#uiplayareaproperties), whose game gets the directions, accept and menu, or `'none'`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if ui.focusOwner() == 'control' then
            return
        end
        local dx, dy = input.vector('move')
        print('the game moves by ' .. dx .. ', ' .. dy)
    end,
})
```

### ui.clearFocus()

Takes the focus away from every GUI, which suits the start of gameplay, so a key that plays the game never presses a menu control left focused.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.button{id = 'pause', text = 'Pause', align = 'end'})
ui.clearFocus()
```

### ui.focusRingVisible()

Returns `true` while the focus ring shows, which happens once the player navigates with the keyboard, a gamepad or a remote, and always on a device without a pointer, such as a TV. A mouse click or a touch hides it again.

```lua
local ui = require('haylen.ui')

if ui.focusRingVisible() then
    print('the player navigates with keys, a gamepad or a remote')
end
```

### ui.safeAreaVisible()

Returns `true` while the debug view of the safe area shows.

```lua
local ui = require('haylen.ui')

print(ui.safeAreaVisible())
```

### ui.setSafeAreaVisible(visible)

Shows or hides the debug view of the safe area, which shades the screen outside the safe area, outlines it and prints its insets over everything. The `debug.showSafeArea` option of `app.json` turns it on at start. Together with [`viewport.setSafeAreaSimulation`](viewport.md#viewportsetsafeareasimulationvalue) it checks a layout against notches, rounded corners and gesture bars on a desktop.

```lua
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

viewport.setSafeAreaSimulation('iphoneDynamicIsland')
ui.setSafeAreaVisible(true)
```

### ui.setScaleMode(mode)

Sets how large the interface draws from the next frame. The mode `'design'`, the default, lays the UI out in the design units of the app, so it grows and shrinks with the screen like the rest of the app. The mode `'physical'` gives a UI unit the same size on every screen, three quarters of a point, so the default control height of 64 units is 48 points on a phone, a tablet and a desktop, and the shorter side of the visible area never holds fewer than 480 UI units, so a layout always has room. A point is the unit the platform reports through [`window.dpiScale()`](window.md): a point of Apple platforms, a density-independent pixel on Android and a CSS pixel on the web. Sizes, positions, `gui:bounds` and the values of events stay in design coordinates, and only the room the UI has changes, which a layout that grows with `grow`, `wrap` and `minColumnWidth` takes in its stride. The `app.json` key `ui.scaleMode` sets it at start. An unknown mode raises `The UI scale mode "<mode>" is unknown. It is "design" or "physical".`.

```lua
local system = require('haylen.system')
local ui = require('haylen.ui')

if system.info().deviceKind == 'phone' then
    ui.setScaleMode('physical')
end
```

### ui.scaleMode()

Returns the scale mode, `'design'` or `'physical'`.

```lua
local ui = require('haylen.ui')

print(ui.scaleMode())
```

### ui.setScale(factor)

Multiplies the size of the interface by a factor from 0.25 to 4 in both modes, such as a text size setting of the player. The `app.json` key `ui.scale` sets it at start. A factor out of range raises `The UI scale must be from 0.25 to 4.`.

```lua
local ui = require('haylen.ui')

ui.setScale(1.25)
```

### ui.scale()

Returns the factor `ui.setScale` set and the design units one UI unit spans in the last frame, which the physical mode derives from the density of the screen.

```lua
local ui = require('haylen.ui')

local factor, scale = ui.scale()
print('A control of 64 units spans ' .. 64 * scale .. ' design units.')
```

### ui.setDirection(direction)

Sets the direction of the whole UI: `'leftToRight'`, the default, `'rightToLeft'`, or `'auto'`, which follows the direction the current language of [`haylen.localization`](localization.md#localizationdirection) declares, so picking Arabic mirrors every GUI from the next frame. [Right-to-left interfaces](#right-to-left-interfaces) describes what mirrors. An unknown name raises an error.

```lua
local localization = require('haylen.localization')
local ui = require('haylen.ui')

ui.setDirection('auto')
localization.setLanguage('ar')
```

### ui.direction()

Returns the direction set for the whole UI, `'leftToRight'`, `'rightToLeft'` or `'auto'`, followed by the direction it draws in, `'leftToRight'` or `'rightToLeft'`, which an automatic direction takes from the language at the start of every frame.

```lua
local ui = require('haylen.ui')

local set, drawn = ui.direction()
print(set, drawn)
```

### ui.onEvent(listener, options)

Calls `listener(event)` for every event of every mounted GUI, with the same table [handlers](#events-and-handlers) receive, and returns a `haylen.Connection`. Its `disconnect()` method stops the listener, and its `connected` property is `true` until then. The optional options table takes `owner`, a table or userdata that ends the listener when it is released, such as a scene when it unloads, or collected, like the other [owners of `haylen.events`](events.md#owners). Unknown keys raise `Unknown option "<key>".`, and an owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.` Listeners run before the handlers of the node. It suits screens loaded from JSON, sound effects for every click and analytics. The field `event.gui` is `nil` for GUIs that C++ code mounted. An error raised inside a listener stops the app and shows the error screen with the message and its stack trace.

```lua
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local ui = require('haylen.ui')

local click = assets.load('sfx/click.wav')
local sounds = ui.onEvent(function(event)
    if event.name == 'click' then
        audio.play(click, {bus = 'ui'})
    end
end)
```

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local shop = {}

function shop:enter()
    ui.onEvent(function(event)
        if event.name == 'click' then
            print('clicked ' .. event.id)
        end
    end, {owner = self})
end

scene.push(shop)
```

## Gui

A `Gui` is the mounted tree `ui.mount` returns. Its methods find nodes by id. Reading a member it does not have raises `The type "haylen.Gui" has no member "<name>".`, and writing a read-only property raises `The type "haylen.Gui" has no writable property "<name>".`.

### gui:set(id, properties)

Changes the given properties of the node `id` and leaves the others as they are. The engine checks the merged properties before it changes anything, so an invalid value leaves the node and its handlers untouched. The `onX` keys add or replace [handlers](#events-and-handlers) of the node once the properties are accepted.

Values the player changed, such as typed text, a checked box or a selected item, stay as they are unless `properties` sets that same property.

Errors raised:

- The error `The GUI is not mounted.` after `unmount`.
- The error `The GUI has no node with the id "<id>".` for an unknown id.
- The error `The "set" method of a GUI changes properties only, so it takes an object without "kind", "id" or "children".` when `properties` holds `kind`, `id` or `children`.
- The property errors of the kind, such as `The property "min" of a "slider" must be smaller than "max".` or `The property "colour" of a "slider" does not exist.`.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.column{
    ui.label{id = 'day', text = 'Day 1'},
    ui.button{id = 'sleep', text = 'Sleep'},
})

local day = 1
hud:set('sleep', {onClick = function()
    day = day + 1
    hud:set('day', {text = 'Day ' .. day})
end})
```

### gui:replaceChildren(id, children)

Replaces every child of the node `id` with the nodes of the list `children`, and an empty list removes them all. The ids of the new nodes are checked before anything changes, so a failed replace leaves the GUI and its handlers as they were. Once the new children are in place, the handlers of removed nodes are dropped, and a new node that reuses the id of a removed one gets only the handlers of its own table.

Errors raised:

- The error `The GUI is not mounted.` after `unmount`.
- The error `The GUI has no node with the id "<id>".` for an unknown id.
- The error `The component kind "<kind>" takes at most <count> children.` when the node cannot hold that many children.
- The error `The UI id "<id>" is used more than once.` when a new id is already in use outside the replaced children.

```lua
local ui = require('haylen.ui')

local shop = ui.mount(ui.column{id = 'items'})

local function showItems(names)
    local rows = {}
    for index, name in ipairs(names) do
        rows[#rows + 1] = ui.button{id = 'buy-' .. index, text = name, onClick = function()
            print('bought ' .. name)
        end}
    end
    shop:replaceChildren('items', rows)
end

showItems({'Axe', 'Rope', 'Lantern'})
```

### gui:get(id)

Returns a copy of the properties of the node `id` as the tree created them, updated by every `set`, without `kind`, `id`, `children` and handlers. Returns `nil` for an unknown id. Player changes such as typed text are not part of it.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.label{id = 'gold', text = 10})
print(hud:get('gold').text)
```

### gui:has(id)

Returns `true` when the GUI holds a node with that id.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.column{ui.label{id = 'hint', text = 'Tap to start'}})
if hud:has('hint') then
    hud:set('hint', {visible = false})
end
```

### gui:bounds(id)

Returns the rectangle where the node was last drawn as a `haylen.Rect` with `x`, `y`, `width` and `height`, in the design coordinates of screen canvases. A `window`, `dialog` or `toast` reports the frame it drew, which follows a window the player drags. Returns `nil` for an unknown id and for a node that was not drawn yet, which is the case until the GUI has been drawn once and for a window, dialog or toast that is closed.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local hud = ui.mount(ui.button{id = 'map', text = 'Map'}, {placement = 'screen'})

scene.push({
    update = function(self, dt)
        local area = hud:bounds('map')
        if area then
            self.pointerTarget = {x = area.x + area.width / 2, y = area.y + area.height / 2}
        end
    end,
})
```

### gui:command(id, name, arguments)

Sends a command to the node `id`. The argument `arguments` is optional, and only `show` takes some.

- The command `'focus'` moves the keyboard, gamepad and remote focus to the node. The focus ring stays visible when the player was already navigating and shows at once when they last used a gamepad or have no pointer device, while a mouse or touch player sees it only once they start navigating. The first accept press also counts as navigation: it shows the ring and activates the focused node. The focusable kinds are `button`, `imageButton`, `menuButton`, `popover`, `chip`, `checkbox`, `toggle`, `radioGroup`, `combo`, `segmentedControl`, `textField`, `secretField`, `textArea`, `filterField`, `numberField`, `slider`, `rangeSlider`, `stepper`, `keyCapture`, `colorField`, `list`, `tree`, `slotGrid`, `accordion`, `carousel`, `playArea`, `collection` and a `richText` with links. A radio group, a list, a tree and a slot grid focus their selected entry, or their first entry that can be picked when none is selected or the selected one cannot take the focus, such as an item of a closed branch, an accordion focuses its first header, a collection keeps the focus on its focused item or focuses its first item in view, and rich text focuses its first link. Rich text written as literal markup takes the focus as soon as it is mounted or set, and translated rich text once it has been drawn.
- The command `'open'` opens a `contextMenu` below its child, as a right click would.
- The command `'show'` adds a notice to a `toast`, with the arguments `text`, `tone` and `duration`, which default to the properties of the toast. Every notice takes its place in the stack of the toast, so notices shown one after the other never cover each other. Unknown arguments raise `Unknown key "<key>" in the "show" command of a "toast".`, and wrong values raise the errors of the properties of the same names.

A kind without that command raises `The component kind "<kind>" does not answer the command "<name>".`, arguments raise `The "focus" command takes no arguments.` or `The "open" command takes no arguments.`, and an unknown id raises `The GUI has no node with the id "<id>".`.

```lua
local ui = require('haylen.ui')

local form = ui.mount(ui.column{
    ui.textField{id = 'name', placeholder = 'Your name'},
})
form:command('name', 'focus')
```

### gui:removeHandler(id, event)

Removes the handler of the event named `event`, such as `'click'`, from the node `id`, and returns `true` when the node had one. The error `The GUI is not mounted.` is raised after `unmount`, and an unknown id raises `The GUI has no node with the id "<id>".`.

```lua
local ui = require('haylen.ui')

local intro = ui.mount(ui.button{id = 'skip', text = 'Skip', onClick = function(event)
    print('skipped once')
    event.gui:removeHandler('skip', 'click')
end})
```

### gui:unmount()

Removes the GUI from the screen and forgets its handlers. Returns `true` when the GUI was mounted and `false` when it was already unmounted. An unmounted GUI still answers `get`, `has` and `bounds`, while `set` and `replaceChildren` raise `The GUI is not mounted.`. Unmounting publishes the `guiUnmounted` event of [`haylen.events`](events.md) with the GUI, and then ends every listener, timer and tween that has the GUI as its `owner`.

```lua
local events = require('haylen.events')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local banner = ui.mount(ui.label{id = 'score', text = 'Score 0', font = 'title', align = 'center'})

-- The listener belongs to the banner, so it ends when the banner is unmounted.
events.on('scoreChanged', function(score)
    banner:set('score', {text = 'Score ' .. score})
end, {owner = banner})

events.emit('scoreChanged', 120)
timer.after(2, function()
    banner:unmount()
    events.emit('scoreChanged', 200)
end)
```

### gui:transform(id)

Returns the transform of the node with the id, a `haylen.UiTransform` that moves, scales, fades and tints the node and its children on top of the place its layout gives it. The GUI keeps one transform object per node, so every call returns the same one while the node exists. An unknown id raises `The GUI has no node with the id "<id>".`. Changes show from the next frame.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `offset` | Vec2 | `(0, 0)` | Moves the node, with the areas where it takes clicks and taps, in design units. |
| `scale` | Vec2 | `(1, 1)` | Scales what the node draws around its center. |
| `opacity` | number | `1` | Multiplies the alpha of what the node draws, from `0` to `1`. |
| `tint` | Color | white | Multiplies the colors of what the node draws. |

Scale, opacity and tint change only how the node looks, so its input areas keep their layout size. They reach what the node draws in its GUI, while popups, tooltips and the content of scroll areas, which draw in windows of their own, keep their look. The four properties are native properties, so [`haylen.tween`](tween.md#native-properties) animates them without running Lua every frame. Replacing the node with `replaceChildren` releases its transform: reading it raises `This "haylen.UiTransform" was already released.`, a tween on it stops, and `transform` returns the transform of the new node.

```lua
local math2d = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local menu = ui.mount(ui.column{align = 'center', gap = 16,
    ui.label{id = 'title', text = 'Tiny Island', font = 'title'},
    ui.button{id = 'play', text = 'Play'},
})

local title = menu:transform('title')
title.opacity = 0
title.offset = math2d.vec2(0, -40)
tween.to(title, 0.6, {opacity = 1, offset = math2d.vec2(0, 0)}, {ease = 'backOut'})
tween.to(menu:transform('play'), 0.4, {scale = math2d.vec2(1.1, 1.1)}, {repeatCount = -1, loopMode = 'yoyo'})
```

### gui:collection(id)

Returns the [`UiCollection`](#uicollection) of the `collection` node `id`, the same object every time while the node exists. An unknown id raises `The GUI has no node with the id "<id>".`, a node of another kind raises `The node "<id>" is a "<kind>", not a "collection".`, and an unmounted GUI raises `The GUI is not mounted.`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}, {id = 'slot2', name = 'Slot 2'}})
print(saves.count)
```

### gui.visible

Readable and writable boolean, `true` after `mount`. A hidden GUI is neither drawn nor reports events, except the `release` of a touch button that was pressed when it was hidden, and it keeps its state for when it shows again.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local pause = ui.mount(ui.panel{align = 'center', ui.label{text = 'Paused'}})
pause.visible = false

scene.push({
    update = function(self, dt)
        if input.keyPressed('escape') then
            pause.visible = not pause.visible
        end
    end,
})
```

### gui.mounted

Read-only boolean, `true` from `mount` until `unmount`.

```lua
local ui = require('haylen.ui')

local toastLayer = ui.mount(ui.toast{id = 'note', text = 'Saved'})
if toastLayer.mounted then
    toastLayer:set('note', {open = true})
end
```

### gui.placement

Read-only string, `'safe'` or `'screen'`, as `ui.mount` received it.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.label{text = 'HP 10'}, {placement = 'screen'})
print(hud.placement)
```

## UiCollection

A `UiCollection` gives a [`collection`](#uicollectionproperties) its items and scrolls, focuses and reads it. Indices count from 1, as Lua lists do, and a target is the id of an item or its index. Reading a member it does not have raises `The type "haylen.UiCollection" has no member "<name>".`, writing a read-only property raises `The type "haylen.UiCollection" has no writable property "<name>".`, every member raises `The GUI is not mounted.` after `unmount`, and `This "haylen.UiCollection" was already released.` once `replaceChildren` removed the node.

| Member | Meaning |
| --- | --- |
| `setItems(list)` | Shows the items of a Lua list, which the GUI keeps and the collection reads. Every item is a table with a non-empty string `id`, unique in the list, a `type` unless the collection declares one type, and the fields its templates bind. A second call, with the same list changed in place or with another list, compares the items by id, so the items that stay keep their measured sizes, their selection and the focus. |
| `setPages(options)` | Shows `count` items that the function `load(first, count)` gives a page at a time, as a list of `count` items or a promise of one, such as the promise of `jobs.spawn`. The loader runs before the GUIs draw for the pages the view and its prefetch need, the items of a page on its way show the `placeholder` type, and at most `maxPages` pages stay, so the pages farthest from the view go. A second call with the same `load` keeps the pages it loaded. |
| `insert(index, items)` | Inserts the items of the list `items` into the list at `index`, from 1 to the count plus 1, and shows them. |
| `remove(index, count)` | Removes `count` items, 1 by default, from `index`. |
| `move(from, to)` | Moves one item. |
| `replace(index, item)` | Puts another item table at `index`, which keeps the cell when the id stays. |
| `reload(target)` | Binds again the items whose fields the app changed in place, given as a target or a list of targets. |
| `setBinder(type, binder)` | Registers `binder(cell, item, index)`, which runs for every cell of the type that receives an item, after its bindings, with a [`UiCell`](#uicell), the item table and its index. The function `nil` removes it. |
| `scrollTo(target, options)` | Scrolls to an item, which keeps aiming while the items on the way are measured. |
| `scrollBy(distance, options)` | Scrolls by a distance in design units. |
| `focus(target)` | Gives the focus to the item, scrolling it into view with the `focusAlign` of the collection. |
| `indexOf(id)` | Returns the index of the item with the id, or `nil`. |
| `cellOf(target)` | Returns the `UiCell` that shows the item, or `nil` while it has no cell. |
| `visibleRange()` | Returns the indices of the first and the last item in view, or `nil` before the collection drew items. |
| `saveState()` | Returns where the collection is, as a table with the id of the first item in view as `item`, how far its start lies from the start of the view as `distance`, and the item that has the focus as `focused`. |
| `restoreState(state)` | Brings the saved item back to its saved distance and gives the focus back to the saved item, for the items that still exist, whatever happened to the other items meanwhile. |
| `count` | Read-only number of items. |
| `scrollOffset` | Readable and writable offset of the view from the start of the content, in design units. Writing it jumps there. |
| `contentLength`, `viewportLength` | Read-only lengths along the axis, where the content counts the estimated length of the items not measured yet. |
| `focusedItem` | Read-only id of the item that has the focus, or `nil`. |
| `selected` | Read-only list of the ids of the selected items, in item order. |

The options of `scrollTo` are `align`, one of `'nearest'`, which scrolls the least that shows the whole item and nothing when it already shows, `'start'`, `'center'` or `'end'`, `'nearest'` by default, `offset`, how far inside the aligned edge the item stays, `0` by default, and `animated`, `true` by default, which follows the item with a spring and jumps close to a far item first. The options of `scrollBy` are `animated`, `true` by default.

Lua code may change the properties of the nodes while a binder or a page loader runs, but not the items of the collection, nor the nodes of its GUI.

Errors raised:

- `The item at index <index> of the collection "<id>" needs a non-empty string id.` and `The item at index <index> of the collection "<id>" must be a table.`
- `The items of the collection "<id>" use the id "<item>" more than once.`
- `The item "<item>" of the collection "<id>" has the type "<type>", which the collection does not declare.`, `The item "<item>" of the collection "<id>" needs a type, because the collection declares several.` and `The type of the item "<item>" of the collection "<id>" must be a string.`
- `The collection "<id>" has no item with the id "<item>".` and `The index <index> is outside the items of the collection "<id>", which holds <count>.`
- `The collection "<id>" has no type named "<type>".` for `setBinder`.
- `The collection "<id>" pages its items, so it changes them with "setPages".` for `insert`, `remove`, `move`, `replace` and `reload` of a paged collection, and `The collection "<id>" has no list of items. Give it one with "setItems".` for them before `setItems`.
- `The collection "<id>" cannot change its items while it binds cells.` for a change of the items from a binder or a page loader, and `The GUI cannot replace nodes or unmount while its collections bind cells.` for `replaceChildren` and `unmount`, which stop the app from inside a binder.
- `Unknown option "<key>".`, `The option "align" must be "nearest", "start", "center" or "end".`, `The option "count" is required.`, `The option "count" must be at least 0.`, `The option "pageSize" must be at least 1.`, the same for `maxPages`, and `The option "load" must be a function.`
- `The page loader of the collection "<id>" must return a list of items or a promise of one.`, `The page loader of the collection "<id>" returned <count> items for a page of <length>.` and `The page loader of the collection "<id>" failed.` followed by the reason of a rejected promise, which stop the app like an error inside a binder.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Inventory = {}
Inventory.__index = Inventory

function Inventory:enter()
    self.items = {}
    for index = 1, 500 do
        self.items[index] = {id = 'item-' .. index, name = 'Item ' .. index, count = index % 9 + 1}
    end
    self.gui = ui.mount(ui.column{padding = 24, gap = 16,
        ui.row{gap = 12,
            ui.button{text = 'Add', onClick = function() self.list:insert(1, {{id = 'new-' .. #self.items, name = 'New item', count = 1}}) end},
            ui.button{text = 'Go to 250', onClick = function() self.list:scrollTo(250, {align = 'center'}) end},
        },
        ui.collection{id = 'items', grow = 1, selection = 'single', types = {item = {template = ui.row{gap = 16,
            ui.label{part = 'name', bind = {text = 'name'}, grow = 1},
            ui.badge{part = 'count', bind = {text = 'count'}},
        }}}, onSelect = function(event) print('picked ' .. event.item) end},
    }, {owner = self})
    self.list = self.gui:collection('items')
    self.list:setItems(self.items)
    if self.saved then
        self.list:restoreState(self.saved)
    end
end

function Inventory:exit()
    self.saved = self.list:saveState()
end

scene.push(setmetatable({}, Inventory))
```

## UiCell

A `UiCell` is a cell of a collection while it shows one item, which a binder receives and `cellOf` returns. Once the cell shows another item, `bound` is `false` and every other member raises `This cell no longer shows the item "<item>".`.

| Member | Meaning |
| --- | --- |
| `set(part, properties)` | Changes properties of a part of this cell, checked like `gui:set`. The part takes its template values again before the cell shows another item. |
| `transform(part)` | Returns the [`UiTransform`](#guitransformid) of a part, which a new transform replaces before the cell shows another item, so a tween on it stops with the item. |
| `item`, `index`, `type` | Read-only id, index and type of the item the cell shows. |
| `bound` | Read-only, `true` while the cell shows the item it was handed out for. |

An unknown part raises `The template of the type "<type>" has no part named "<part>".`, and a `set` with `kind`, `id`, `children`, `part` or `bind` raises `The "set" method of a cell changes properties only, so it takes an object without "kind", "id", "children", "part" or "bind".`.

```lua
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'scores', height = 600, types = {score = {template = ui.row{
    ui.label{part = 'name', bind = {text = 'name'}, grow = 1},
    ui.label{part = 'points', text = '', font = 'heading'},
}}}})
local scores = gui:collection('scores')

-- The binder writes what a binding cannot, and the best score of the list pulses while it shows.
scores:setBinder('score', function(cell, item, index)
    cell:set('points', {text = item.points .. ' points', color = index == 1 and 'warningText' or 'text'})
    if index == 1 then
        tween.to(cell:transform('points'), 0.4, {opacity = 0.5}, {repeatCount = -1, loopMode = 'yoyo'})
    end
end)
scores:setItems({{id = 'ana', name = 'Ana', points = 1200}, {id = 'leo', name = 'Leo', points = 950}})
```

## Screen format

A node is a table with these keys, and the same shape works as JSON.

| Key | Type | Meaning |
| --- | --- | --- |
| `kind` | string | Component kind, one of the kinds on this page. It is required. |
| `id` | string | Optional name of the node, unique within the GUI and not empty. The methods `set`, `replaceChildren`, `get`, `bounds` and `command` find nodes by it. Ids starting with `#` are reserved for the engine. |
| `children` | list of nodes | Child nodes, for kinds that hold children. |
| any other key | depends on the kind | A property of the kind, from the [common properties](#common-properties) or the properties of the kind. |
| `onX` | function | Lua only. A [handler](#events-and-handlers) for the event `x`. |

In Lua the children can also go in the array part of the node, which lets trees read like `ui.column{ui.label{...}, ui.button{...}}`. A node takes its children from `children` or from its array part, never from both.

A GUI has at most 64 levels, the root included, and 20000 nodes, counted over the whole GUI when `replaceChildren` adds children. A node table that holds itself, such as `c[1] = c`, raises the same error instead of nesting without end. Every property is checked when the tree is built, so a misspelled key or a wrong value is reported instead of ignored.

Errors raised for invalid trees:

- `A UI node needs a kind.` and `There is no UI component kind named "<kind>".`
- The errors `The id of a "<kind>" must be a non-empty string.`, which reads `of an "<kind>"` for a kind that starts with a vowel, and `The UI id "<id>" is used more than once.`
- `UI ids starting with "#" are reserved for nodes the engine names.`
- `The children of a "<kind>" must be a list.` and `The component kind "<kind>" takes at most <count> children.`
- `A UI node takes children either in its "children" list or in its array part, not both.`
- `UI node keys must be strings.` and `A UI node with handlers needs a string id.`
- `A GUI is limited to 64 levels and 20000 nodes.`
- The error `The property "<key>" of a "<kind>" does not exist.` for an unknown property, and the value errors listed in [Value types](#value-types).

Screens can live in JSON files in the package assets. Handlers cannot be written in JSON, so the app attaches them with `set` after mounting.

```json
{
    "kind": "column",
    "align": "center",
    "gap": 24,
    "children": [
        {"kind": "label", "text": {"key": "menu.title"}, "font": "title"},
        {"kind": "button", "id": "play", "text": {"key": "menu.play"}, "variant": "primary"},
        {"kind": "button", "id": "quit", "text": {"key": "menu.quit"}}
    ]
}
```

```lua
local assets = require('haylen.assets')
local ui = require('haylen.ui')

local menu = ui.mount(assets.json('ui/main-menu.json'))
menu:set('play', {onClick = function() print('play') end})
menu:set('quit', {onClick = function() print('quit') end})
```

Lua has no separate empty list, so an empty table converts to an empty JSON object. List properties such as `items`, `rows`, `columns`, `buttons`, `expanded`, the `children` of tree items and the `cells` of table rows accept an empty object as an empty list, so `{}` works in Lua and a JSON file with an empty list keeps working after it passes through Lua. Both `children = {}` and `replaceChildren(id, {})` work too.

## Value types

| Type | Accepted values |
| --- | --- |
| text | A string, a number, or a translation such as `{key = 'menu.play', args = {n = 2}}`, which [`haylen.localization`](localization.md) translates every time the text is drawn, so a language change shows at once. Whole numbers show without decimals. |
| color | A string `'#RRGGBB'` or `'#AARRGGBB'`, such as `'#FF2E7D32'`. |
| theme color | The name of a [theme color role](#theme-colors), such as `'accent'` or `'textMuted'`. |
| length | A non-negative number, or `'auto'` to let the content decide. |
| insets | One number for every side, `{vertical, horizontal}`, or `{top, right, bottom, left}`. Every value is non-negative. |
| image | A texture path relative to the package `content/` folder. Images load in the background, and a component draws nothing in their place until they arrive. An image that fails to load stops the app with `The UI image "<path>" could not be loaded.` followed by the reason. |
| tone | `'neutral'`, `'accent'`, `'success'`, `'warning'`, `'danger'` or `'information'`. |
| variant | One of `'default'`, `'primary'`, `'destructive'`, `'toolbar'`, `'icon'` or `'link'`, described under [button](#uibuttonproperties). |
| items | A list of [items](#items). |

A value of the wrong type raises an error that names the key and the kind, such as `The property "<key>" of a "<kind>" must be "true" or "false".`, which a kind that starts with a vowel reads as `of an "<kind>"`. The endings are `must be "true" or "false".`, `must be a string.`, `must be a number.`, `must be a whole number.`, `must be at least <minimum>.`, `must be from <minimum> to <maximum>.`, `must be a color such as "#FF2E7D32".`, `must name a theme color such as "accent" or "textMuted".`, `must be <names>.` for a name outside its list, such as `must be "start", "center", "end" or "stretch".`, `must be a non-negative number or "auto".`, `must be one, two or four non-negative numbers.` and `must be text, a number or a translation such as "{key = 'menu.play'}".`.

## Common properties

Every kind accepts these properties.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `visible` | boolean | `true` | A hidden node takes no room and is not drawn. |
| `enabled` | boolean | `true` | A disabled node is drawn dimmed and ignores the player. |
| `tooltip` | text | none | Text shown next to the pointer after it rests on the node for half a second. |
| `grow` | number from 0 to 1000 | `0` | Share of the free space the node takes along a column or a row. A growing node starts from nothing rather than from its content, so two nodes with `grow = 1` split the free space in half and a growing `scroll` scrolls inside its share. A column or row that takes the size of its content gives every growing node at least the room its content needs. |
| `width` | length | `'auto'` | Fixed width. |
| `height` | length | `'auto'` | Fixed height. |
| `minWidth` | number | `0` | Smallest width. |
| `maxWidth` | number | unbounded | Largest width. |
| `minHeight` | number | `0` | Smallest height. |
| `maxHeight` | number | unbounded | Largest height. |
| `align` | string | depends on the kind | One of `'start'`, `'center'`, `'end'` or `'stretch'`. A column places the node across its width with it, a row across its height, a grid inside its cell, and a stack or the GUI root in both directions. Without `align`, rows center their children. Everywhere else labels, buttons, image buttons, menu buttons, popovers, chips, check boxes, toggles, radio groups, badges, status indicators, circular progress indicators, icons, images, avatars and touch controls use `start`, the busy indicator uses `center`, and every other kind stretches. |
| `anchor` | string | none | Takes the node out of the layout of its parent and places it against the safe area or the screen, as [Anchors](#anchors) describes. The value `'none'` puts it back in the layout. |
| `anchorTo` | string | `'safe'` | Area an anchored node is placed in: `'safe'` or `'screen'`. |
| `margin` | insets | `0` | Space around the node: its parent places it that far from the nodes and edges around it, and an anchored node keeps that distance from the edges of its area. |
| `aspectRatio` | number from 0.01 to 100 | none | The width divided by the height, which the node keeps. Its height follows its width, its width follows a fixed `height`, and without a fixed size it takes the whole width it is offered. A place of another shape shrinks around its center to the ratio. |
| `focusable` | boolean | `true` | The value `false` keeps the controls of the node and of every node inside it out of keyboard, gamepad and remote navigation, and a click presses them without moving the focus there, which suits HUD buttons that share keys with gameplay. |
| `autofocus` | boolean | `false` | Gives the node the focus when it appears, such as when its GUI mounts or its dialog opens, unless the focus is already on a control of the same GUI and window. |
| `focusScope` | boolean | `false` | Keeps the focus inside the node while it is there, and makes the node hear `cancel`. |
| `focusWrap` | string | `'none'` | The values `'horizontal'`, `'vertical'` or `'both'` wrap a move that would leave the node around to its other side. |
| `focusLeft`, `focusRight`, `focusUp`, `focusDown` | string | none | Id of the node the focus moves to in that direction instead of the nearest one. A node that names itself keeps the focus in that direction. |
| `direction` | string | `'inherit'` | The value `'leftToRight'` or `'rightToLeft'` lays the node and every node inside it out in that direction, whatever the direction of the UI, and `'inherit'` takes the direction of its parent. [Right-to-left interfaces](#right-to-left-interfaces) describes what mirrors. |
| `language` | string | inherited | BCP 47 tag of the language of the text of the node and of every node inside it, such as `'ar'` or `'hi'`, which the shaper uses to pick the forms a language prefers. It defaults to the current language of [`haylen.localization`](localization.md). |
| `theme` | string | inherited | Name of a registered theme the node and every node inside it draw with, whatever the active theme, such as a light card in a dark screen. A name that is not registered stops the app with `The UI has no theme named "<name>".` when the node draws. |
| `style` | table | none | Values that replace those of the theme for the node and every node inside it, as described in [Styles](#styles). Setting it again replaces the whole style, and an empty table goes back to the theme. |
| `cursor` | string | none | Shape of the mouse cursor while it is over the node: `'default'`, `'arrow'`, `'iBeam'`, `'crosshair'`, `'pointingHand'`, `'resizeHorizontal'`, `'resizeVertical'`, `'resizeDiagonalDown'`, `'resizeDiagonalUp'`, `'resizeAll'` or `'notAllowed'`. The innermost node under the pointer with a cursor wins. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    align = 'start',
    width = 900,
    ui.button{text = 'Back', tooltip = 'Return to the map'},
    ui.spacer{grow = 1},
    ui.button{text = 'Buy', enabled = false, minWidth = 200},
})
```

## Anchors

An anchored node leaves the layout of its parent, takes no room there and draws over it, placed against the safe area or the whole visible screen. The GUI root may be anchored too, which places the whole GUI. The app draws under notches, dynamic islands, rounded corners and gesture bars on every platform, and anchors choose where each part of the interface sits.

| Anchor | Place |
| --- | --- |
| `'topLeft'`, `'top'`, `'topRight'` | Top corners and the middle of the top edge, at the measured size. |
| `'left'`, `'center'`, `'right'` | Middle of the left edge, the center and the middle of the right edge. |
| `'bottomLeft'`, `'bottom'`, `'bottomRight'` | Bottom corners and the middle of the bottom edge. |
| `'stretch'` | The whole area. |
| `'stretchTop'`, `'stretchBottom'` | The top or bottom edge, as wide as the area. |
| `'stretchLeft'`, `'stretchRight'` | The left or right edge, as tall as the area. |
| `'stretchHorizontal'`, `'stretchVertical'` | A band across the middle, as wide or as tall as the area. |

The property `margin` keeps the node away from the edges of its area, and `width` and `height` fix the size along axes that do not stretch. An unknown name raises `The property "anchor" of a "<kind>" must be "none" or an anchor name such as "topLeft", "center" or "stretchHorizontal".`, and an `anchorTo` other than `'safe'` or `'screen'` raises `The property "anchorTo" of a "<kind>" must be "safe" or "screen".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/sky.png', fit = 'cover', anchor = 'stretch', anchorTo = 'screen'},
    ui.button{id = 'pause', icon = 'ui/pause.png', variant = 'icon', anchor = 'topRight', margin = 16},
    ui.panel{anchor = 'stretchBottom', height = 120, margin = {0, 24}, ui.label{text = 'Wood 12'}},
    ui.label{text = 'Day 3', font = 'heading', anchor = 'top', margin = {24, 0}},
}, {placement = 'screen'})
```

## Right-to-left interfaces

Every text of every component is shaped and ordered for display by the text layout of the engine, so labels, buttons, fields, lists, tables, tooltips and rich text show Arabic, Hebrew, Persian, Urdu, Hindi, Thai and every other script their font role covers, and each paragraph reads in the direction of its first strong letter. The [text guide](../text.md#scripts-and-directions) explains shaping and directions.

The direction of the layout is separate from the direction of each text. The call `ui.setDirection('rightToLeft')` or a node with `direction = 'rightToLeft'` mirrors the layout inside it:

- Rows place their first child at the right, grids fill their rows from the right, and `start` and `end` of `align`, `justify` and `textAlign` name the right and the left.
- Check boxes, radio buttons and toggles put their box on the right of their text, buttons put their icon on the right, and list rows, form fields, settings rows, tabs, accordions, trees, tables and alerts start from the right.
- Sliders, range sliders, progress bars and toggles fill from the right, and the left and right arrow keys move sliders, steppers, segmented controls, carousels and splitters the way they point on screen.
- Steppers and carousels swap their ends, so the arrow that points left moves forward, the closed arrows of trees and accordions point left, the arrow of a combo stands at its left, and menus, combo lists, popovers and context menus open from the right edge of their button.
- Text fields line their text up on the right. In every direction the caret stands on the side of the letter it follows, the left and right arrow keys move it the way they point on screen, and a selection across a change of direction covers every run it spans.

Anchors, `padding` given as four sides and the `focusLeft` and `focusRight` properties name fixed sides, and images and icons draw as they are. A node with `direction = 'leftToRight'` inside a right-to-left UI keeps its own direction, for a phone number, a code or a game board.

```lua
local ui = require('haylen.ui')

ui.setDirection('rightToLeft')
ui.mount(ui.card{gap = 16,
    ui.row{gap = 12, ui.checkbox{text = 'تذكرني', checked = true}, ui.button{text = 'دخول', variant = 'primary'}},
    ui.slider{value = 0.3},
    ui.row{direction = 'leftToRight', gap = 8, ui.label{text = '+1 555 0100'}},
})
```

## Focus and navigation

Buttons, choices, inputs, list rows, slots and the other interactive parts of a GUI take the keyboard, gamepad and remote focus. The UI moves it with the navigation actions of [`haylen.input`](input.md#the-action-map), which the app remaps by defining an action with the same name in its action map, while the others keep their built-in bindings.

| Action | Built-in bindings | Use |
| --- | --- | --- |
| `uiAccept` | `key:enter`, `key:keypadEnter`, `key:space`, `button:south` | Presses the focused control. |
| `uiCancel` | `key:escape`, `button:east` | Goes back: closes the open popup or dialog, puts back a carried item, or sends `cancel`. |
| `uiLeft`, `uiRight`, `uiUp`, `uiDown` | the arrow keys, the directional pad and the left stick | Moves the focus. |
| `uiMenu` | `key:menu`, `button:north` | Opens the context menu around the focus. |
| `uiFocus` | `button:back` | Moves the focus from a play area to the control of its GUI that last had it, or to its first control, and back. |
| `uiPagePrevious`, `uiPageNext` | `key:pageUp` and `button:leftShoulder`, `key:pageDown` and `button:rightShoulder` | Scrolls the innermost collection around the focus by one view and moves the focus to the first item of the new view. |
| `uiFirst`, `uiLast` | `key:home`, `key:end` | Moves the focus to the first or the last item of the innermost collection around the focus. |

- A direction moves the focus to the nearest control in that direction, preferring controls in line with the focused one, unless the focused node names a neighbor with `focusLeft`, `focusRight`, `focusUp` or `focusDown`. Some controls use left and right themselves while they have the focus: sliders, range sliders, steppers, segmented controls and the page dots of a carousel.
- Tab and Shift Tab walk the controls in the order they draw, also out of a text field being edited, and a text field they reach starts editing.
- The focus stays inside its GUI, and a dialog, a popover and a menu keep it until they close, and then it returns to where it was. A GUI that stops drawing, such as the GUI of a covered scene or one with `visible` set to `false`, gives the focus back to the control that had it once it draws again, unless the focus moved elsewhere meanwhile. A `window` belongs to the navigation of its GUI, so a move reaches its controls from the rest of the GUI and brings it to the front, and closing it returns the focus to where it was. A node with `focusScope = true` keeps the focus while it is inside, and a move never enters a scope from outside, only `autofocus`, the `focus` command or a click do.
- The property `focusWrap` wraps a move that would leave a node around to its other side, such as the end of a row of cards back to its first card.
- A scroll brings the focused control into view, and so does a collection, which also keeps the focus on an item whose cell the wheel or a finger took out of view. The first direction or accept after that only brings the item back into view, and the next one moves on or presses it. A collection binds the items around the focused one, so the focus reaches items that do not show yet, and the property `focusWrap` of a collection along its axis wraps from its last item to its first even when they do not show.
- The focused node reports `focus` and its previous node reports `blur`, as long as that node still draws.
- The action `uiCancel` closes the open popup or dialog, puts back an item carried from a slot grid or a list, and otherwise sends `cancel` to the innermost node with `focusScope` around the focus, or to the root of the GUI that holds the focus, or of the topmost GUI when nothing has it. A screen goes back from its root handler, as in the example below.
- The ring shows around the focus once the player navigates, and a click or a touch hides it. On a device without a pointer, such as an Apple TV or an Android TV, it shows from the start. The function `ui.focusRingVisible()` tells whether it shows.
- The first press of a direction while the ring is hidden only shows where the focus is, and the first accept shows the ring and presses the control.
- A [`playArea`](#uiplayareaproperties) takes the focus like a control, from a click, a touch, Tab, `uiFocus`, a move or the `focus` command. While it has the focus the directions, accept and menu belong to the game, so they never move the focus or press a control, and cancel, Tab and `uiFocus` stay with the UI. The function [`ui.focusOwner`](#uifocusowner) tells which side has the focus.
- A press the UI answers itself never reaches the actions of the app. Every navigation key, button and the left stick while a control has the focus, the keys and buttons of `uiCancel` and `uiFocus` and Tab while a play area has the focus, the keys and buttons of `uiCancel` that close a popup, a dialog, a closable window or a carried item or that end the editing of a control, every key while a text field edits and every key and button while a `keyCapture` listens read as up in the action map until they are released, and [`input.keyCaptured`](input.md#inputkeycapturedkey), [`input.gamepadButtonCaptured`](input.md#inputgamepadbuttoncapturedbutton-index) and [`input.gamepadAxisCaptured`](input.md#inputgamepadaxiscapturedaxis-index) tell when a press belongs to the UI. A press that started in the game stays with the game until it is released, even once the focus moved to a control.

On an Apple TV, a swipe on the touch surface of the Siri Remote moves the focus, a click presses the focused control, and Menu is `uiCancel`. On an Android TV, the directional pad of the remote and gamepads move it, select presses, and Back is `uiCancel`. Both platforms leave the app when the player goes back from its root screen, as Apple and Google ask, while [`window.setBackLeavesApp`](window.md#windowsetbackleavesappenabled) keeps the press inside the app on other screens. An open popup or dialog always keeps it.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Options = {}
Options.__index = Options

function Options:enter()
    window.setBackLeavesApp(false)
    self.gui = ui.mount(ui.column{
        onCancel = function()
            scene.pop()
        end,
        align = 'center',
        focusWrap = 'vertical',
        ui.toggle{id = 'music', text = 'Music', autofocus = true},
        ui.toggle{id = 'sound', text = 'Sound'},
        ui.button{id = 'back', text = 'Back', focusUp = 'sound', onClick = function()
            scene.pop()
        end},
    })
end

function Options:exit()
    self.gui:unmount()
    window.setBackLeavesApp(true)
end

scene.push(setmetatable({}, Options))
```

## Items

Kinds that offer choices take an `items` list. Every item is a table with a unique `id`.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `id` | string | required | Name of the item, unique within the list and not empty. Selection properties such as `selected` refer to it. |
| `text` | text | none | Text of the item. |
| `caption` | text | none | Second line under the text, shown by `list` and `tree`. |
| `image` | image | none | Picture before the text, shown by `list` and `tree`. |
| `enabled` | boolean | `true` | A disabled item cannot be picked. |
| `children` | list of items | none | Nested items, accepted only by `tree`. |

Item errors name the property and the kind: a repeated id raises `The property "items" of a "<kind>" uses the item id "<id>" more than once.`, an item without an id raises `The property "items" of a "<kind>" needs a non-empty id for every item.`, an item that is not a table raises `The property "items" of a "<kind>" must hold objects with an id.`, nested items outside a tree raise `The property "items" of a "<kind>" cannot nest items, which only a tree does.`, and tree children that are not a list raise `The property "items.children" of a "tree" must be a list of items.`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.list{
    items = {
        {id = 'wood', text = 'Wood', caption = 'Used for walls', image = 'ui/wood.png'},
        {id = 'gold', text = 'Gold', enabled = false},
    },
})
```

## Events and handlers

Components report what the player does as events. A node table key that starts with `on` followed by an upper-case letter and holds a function is a handler, and it answers the event named by the rest of the key with a lower-case first letter: `onClick` answers `click` and `onChange` answers `change`. A handler key that holds anything other than a function is read as a property and raises `The property "<key>" of a "<kind>" does not exist.`. The engine does not check that the kind reports the event, so a handler for an event the kind never reports is never called.

A node with handlers and no `id` gets an id from the engine, such as `#1`, which handlers see as `event.id`. The method `set` adds or replaces handlers, and `gui:removeHandler` removes one.

A handler receives one table that holds the values of the event plus these fields.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | Id of the node that reported the event. |
| `name` | string | Name of the event, such as `'click'`. |
| `gui` | Gui | GUI that holds the node. |

Event values never use the names `id`, `name` and `gui`, so the item of a `select` arrives as `event.item` and the button of an `answer` as `event.button`, next to the node id in `event.id`.

Events are collected while the GUI draws and handed to the handlers once per frame, in the engine update of the next frame, before the scenes update. An error raised inside a handler stops the app and shows the error screen with the message and its stack trace.

| Kind | Event | Values |
| --- | --- | --- |
| `button`, `imageButton` | `click` | none |
| `chip` | `change` | `selected` (boolean) |
| `chip` | `remove` | none |
| `checkbox`, `toggle` | `change` | `checked` (boolean) |
| `radioGroup`, `combo`, `segmentedControl` | `change` | `value` (item id) |
| `stepper` | `change` | `value` (number, or item id for a stepper of items) |
| `rangeSlider` | `change` | `low` and `high` (numbers) |
| `keyCapture` | `change` | `value` (binding string such as `'key:w'`) |
| `keyCapture` | `cancel` | none |
| `textField`, `secretField`, `textArea`, `filterField` | `change` | `value` (string) |
| `textField`, `secretField`, `filterField` | `submit` | `value` (string) |
| `numberField`, `slider` | `change` | `value` (number) |
| `colorField` | `change` | `value` (color string `'#AARRGGBB'`) |
| `tabs`, `menuButton`, `contextMenu`, `list`, `tree`, `table`, `slotGrid` | `select` | `item` (item, row or slot id) |
| `tree`, `accordion` | `toggle` | `item` (item id), `expanded` (boolean) |
| `richText` | `link` | `link` (payload of the link) |
| `richText` | `linkHover` | `link` (payload of the link), `hovered` (boolean) |
| `list` with `draggable`, `slotGrid` | `dragStart` | `item` (id of the row or slot picked up or dragged) |
| `list` with `draggable`, `slotGrid` | `drop` | `item` (id of the row or slot dropped on), `source` (id of the node the item left), `sourceItem` (its row or slot id) |
| `carousel` | `change` | `page` (number, from 1) |
| `collection` | `select` | `item` (item id), `index` (number, from 1), `selected` (boolean, with a selection) |
| `collection` | `itemFocus` | `item` (item id), `index` (number, from 1) |
| `collection` | `visibleChange` | `first`, `last` (numbers, from 1) |
| `collection` | `scrollEnd` | `offset` (number), `item` (item id) |
| `collection` | `endReached`, `startReached` | `count` (number) |
| `collection` | `refresh` | none |
| `collection` | the events of the parts of its cells | their values, `part` (part name), `cell` (`item`, `index` and `type`) |
| `window` | `move` | `x`, `y` (numbers) |
| `window` | `close` | none |
| `splitter` | `resize` | `ratio` (number) |
| `dialog` | `answer` | `button` (button id) |
| `dialog`, `toast` | `dismiss` | none |
| `touchButton` | `press`, `release` | none, for any finger, in place of the `press` and `release` of every kind |
| every kind | `focus`, `blur` | none |
| a node with `focusScope`, the GUI root | `cancel` | none |
| every kind | `mount`, `unmount`, `show`, `hide`, `hover`, `press`, `release`, `drag`, `scroll` | as [the events of every kind](#the-events-of-every-kind) describes |

### The events of every kind

Every node reports these events, each only while the node has a handler for it, since watching them costs a check every frame. They reach the listeners of `ui.onEvent` for those nodes too, and a GUI mounted from C++ turns them on with `Gui::listen`.

| Event | Values | When |
| --- | --- | --- |
| `mount` | none | The node joined a mounted GUI, when its GUI mounts or when `replaceChildren` adds it, reported in the update after the GUI next draws. |
| `unmount` | none | The node left its GUI, because the GUI unmounted or `replaceChildren` removed the node. It runs at once, before `unmount` or `replaceChildren` returns, since the handlers of a node end with it, and a node reports it only after it reported `mount`. An error in the handler shows the error screen without stopping the change. |
| `show` | none | The node draws again after a frame in which it did not, such as when it first draws, when it or a node around it becomes visible, when its tab or page shows or when its GUI or scene shows again. |
| `hide` | none | The node stopped drawing, the opposite of `show`. A node that stops drawing while hovered or pressed first reports `hover` with `hovered = false` and `release` with `inside = false`. |
| `hover` | `hovered` (boolean) | The pointer moved over the node or away from it. |
| `press` | `x`, `y` (numbers), `button` (`'left'`, `'right'` or `'middle'`) | A mouse button or a finger pressed the node. |
| `drag` | `x`, `y`, `deltaX`, `deltaY` (numbers), `button` | The pointer moved while the press that started on the node holds, wherever the pointer is. |
| `release` | `x`, `y` (numbers), `button`, `inside` (boolean) | The press that started on the node let go, with `inside` telling whether the pointer was still over the node. |
| `scroll` | `deltaX`, `deltaY` (numbers) | The mouse wheel or a trackpad scrolled over the node, in wheel steps, with positive `deltaY` for scrolling up. |

Positions are in the design coordinates of screen canvases, like `gui:bounds`. Every node under the pointer that listens reports the pointer events, so a press on a button inside a card reaches both when both listen, and a disabled node reports none of them. The pointer events follow the visible part of the node and stop while a window, a dialog or a popup above it takes the pointer.

```lua
local math2d = require('haylen.math')
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.label{id = 'readout', text = 'Drag the card.'},
    ui.card{id = 'card', width = 240, height = 160, anchor = 'center',
        onPress = function(event) event.gui:set('readout', {text = 'Pressed with ' .. event.button .. '.'}) end,
        onDrag = function(event)
            local shape = event.gui:transform('card')
            shape.offset = shape.offset + math2d.vec2(event.deltaX, event.deltaY)
        end,
        onRelease = function(event) event.gui:set('readout', {text = event.inside and 'Dropped on the card.' or 'Dropped outside.'}) end,
        onHover = function(event) event.gui:set('card', {style = event.hovered and {colors = {raised = '#FF343B52'}} or {}}) end,
        onScroll = function(event) print('Scrolled ' .. event.deltaY) end,
        onShow = function() print('The card shows.') end,
        onMount = function() print('The card joined the GUI.') end,
        onUnmount = function() print('The card left the GUI.') end,
    },
})
```

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.checkbox{id = 'music', text = 'Music', checked = true, onChange = function(event)
        print(event.id .. ' ' .. event.name .. ' ' .. tostring(event.checked))
    end},
    ui.slider{id = 'volume', value = 0.8, onChange = function(event)
        event.gui:set('music', {text = 'Music ' .. math.floor(event.value * 100 + 0.5) .. '%'})
    end},
})
```

## Containers

### ui.column(properties)

Lays out any number of children from top to bottom. Children with `grow` share the free height by their factors, each within its `minHeight` and `maxHeight`, and `justify` places the children in the height they leave. Each child sits across the width by its `align`, or by the `alignItems` of the column when it sets none, and a stretched child stays within its `minWidth` and `maxWidth`. The `margin` of a child keeps the space around it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gap` | number from 0 to 10000 | theme `itemSpacing` | Space between children. |
| `padding` | insets | `0` | Space between the edges and the children. |
| `justify` | string | `'start'` | Where the free space goes: `'start'`, `'center'` and `'end'` place the children together, `'spaceBetween'` puts it between them, `'spaceAround'` gives every child an equal share on both sides, and `'spaceEvenly'` makes every space, the ends included, equal. Children start at the nearest whole unit. |
| `alignItems` | string | none | The `align` of every child that sets none: `'start'`, `'center'`, `'end'` or `'stretch'`. Without it each kind keeps its own default. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    padding = {24, 32},
    gap = 12,
    justify = 'center',
    ui.label{text = 'Wood 12'},
    ui.label{text = 'Stone 4'},
    ui.button{text = 'Build', align = 'end'},
})
```

### ui.row(properties)

Lays out any number of children from left to right, or from right to left in a [right-to-left](#right-to-left-interfaces) node. It takes the properties of `column`. Children with `grow` share the width the others leave, so wide content never pushes the row past its bounds. Each child sits across the height by its `align`, which defaults to `center` in a row.

A row with `wrap = true` breaks its children into lines that each take as many children as fit at their natural width, and a child wider than the row takes a line of its own. Every line is as tall as its tallest child, places its children by `justify`, and shares its free width among its children that grow.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `wrap` | boolean | `false` | Breaks the children into lines. |
| `lineGap` | number from 0 to 10000 | `gap` | Space between lines. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    align = 'start',
    padding = 16,
    gap = 16,
    ui.avatar{name = 'Ana Souza', size = 48},
    ui.label{text = 'Ana', grow = 1},
    ui.badge{text = 'Level 3', tone = 'accent'},
})
```

### ui.grid(properties)

Lays out any number of children in cells of equal width, filling each row before the next. Every row is as tall as its tallest child, and each child sits inside its cell by its `align` in both directions, or by the `alignItems` of the grid when it sets none. A stretched child fills its cell within its size bounds.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | integer from 1 to 64 | `2` | Number of cells per row, and the most cells a row fits with `minColumnWidth`. |
| `minColumnWidth` | number from 0 to 10000 | `0` | Above zero, every row fits as many cells as are at least this wide, so the columns follow the width of the grid. |
| `gap` | number from 0 to 10000 | theme `itemSpacing` | Space between cells. |
| `rowGap` | number from 0 to 10000 | `gap` | Space between rows. |
| `padding` | insets | `0` | Space between the edges and the cells. |
| `alignItems` | string | none | The `align` of every child that sets none. |

```lua
local ui = require('haylen.ui')

local slots = {}
for index = 1, 8 do
    slots[index] = ui.card{ui.label{text = 'Slot ' .. index}}
end
ui.mount(ui.grid{columns = 4, gap = 8, padding = 16, children = slots})
```

### ui.stack(properties)

Draws any number of children on top of each other, the later ones above. Each child sits by its own `align` in both directions, or by the `alignItems` of the stack when it sets none, and `stretch` fills the whole stack within the size bounds of the child.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `padding` | insets | `0` | Space between the edges and the children. |
| `alignItems` | string | none | The `align` of every child that sets none. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/map.png', fit = 'cover', align = 'stretch'},
    ui.label{text = 'You are here', align = 'center', outline = '#FF000000'},
    ui.button{text = 'Close', align = 'end'},
}, {placement = 'screen'})
```

### ui.scroll(properties)

Shows one child in an area that scrolls up and down or sideways. The mouse wheel, the scrollbar and, on touch screens, dragging the content scroll it, and a drag along the scrolling direction takes the finger from the control it started on, so a list of buttons still scrolls. The focus scrolls to the focused control. A vertical scroll measures as tall as its content, so give it a `height` or a `maxHeight` to make it scroll, and a horizontal scroll needs a `width`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `scrollbar` | boolean | `true` | Shows the scrollbar. |
| `axis` | string | `'vertical'` | Either `'vertical'` or `'horizontal'`, which lays the child out at its full width and scrolls it sideways, also with the mouse wheel. |
| `snap` | boolean | `false` | Settles, once the player lets go, on the start of the item of the child nearest to the scrolled position, such as a card of a row. |

A horizontal scroll lays its child out at the width of its content. Kinds that otherwise fill the width they get, `list`, `tree`, `table`, `accordion`, `carousel` and `splitter`, take the width of their content there, and grids and splitters give each of their children its own. An axis other than `'vertical'` or `'horizontal'` raises `The property "axis" of a "scroll" must be "vertical" or "horizontal".`.

```lua
local ui = require('haylen.ui')

local lines = {}
for index = 1, 40 do
    lines[index] = ui.label{text = 'Quest log entry ' .. index}
end
ui.mount(ui.scroll{height = 600, ui.column{gap = 8, children = lines}})

local cards = {}
for index = 1, 8 do
    cards[index] = ui.card{width = 360, ui.label{text = 'Level ' .. index}}
end
ui.mount(ui.scroll{axis = 'horizontal', snap = true, width = 1200, height = 240, align = 'end', ui.row{gap = 24, children = cards}})
```

### ui.card(properties)

A column on a raised surface with a border, drawn with the theme `card` surface when the theme has one. It takes the properties of `column`. When `padding` is zero the theme `panelPadding` metric pads the content. A card keeps the pointer from reaching the app behind it.

```lua
local ui = require('haylen.ui')

ui.mount(ui.card{
    align = 'center',
    width = 600,
    ui.sectionTitle{text = 'Crafting'},
    ui.label{text = 'Combine two items to make a new one.'},
})
```

### ui.panel(properties)

A column on the panel color without a border, drawn with the theme `panel` surface when the theme has one. It takes the properties of `column`, pads like a card and keeps the pointer from reaching the app behind it.

```lua
local ui = require('haylen.ui')

ui.mount(ui.panel{
    align = 'end',
    width = 420,
    ui.label{text = 'Objectives'},
    ui.checkbox{text = 'Build a shelter'},
    ui.checkbox{text = 'Find water'},
}, {placement = 'screen'})
```

### ui.spacer(properties)

Takes room and draws nothing. It has no properties of its own, so it uses `width`, `height` or `grow`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.label{text = 'Score'},
    ui.spacer{grow = 1},
    ui.label{text = '1200'},
})
```

### ui.divider(properties)

Draws a line through the middle of its bounds, as thick as the theme `borderWidth` metric. A vertical divider in a row needs a `height` or `align = 'stretch'`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `vertical` | boolean | `false` | Draws a vertical line instead of a horizontal one. |
| `color` | theme color | `'border'` | Color of the line. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.label{text = 'Audio'},
    ui.divider{},
    ui.row{
        ui.label{text = 'Left'},
        ui.divider{vertical = true, height = 40, color = 'borderStrong'},
        ui.label{text = 'Right'},
    },
})
```

### ui.tabs(properties)

A strip of tabs over the child of the selected tab. It takes one child per item, in the order of the items, and shows only the child of the selected tab.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The tabs. Items use `id`, `text` and `enabled`. |
| `selected` | string | first item | Id of the selected tab. |

Picking another tab reports `select` with the item id as `item`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.tabs{
    id = 'journal',
    items = {{id = 'quests', text = 'Quests'}, {id = 'map', text = 'Map'}},
    selected = 'quests',
    onSelect = function(event)
        print('switched ' .. event.id .. ' to ' .. event.item)
    end,
    ui.label{text = 'Find the lighthouse'},
    ui.image{image = 'ui/map.png'},
})
```

### ui.accordion(properties)

Sections under headers that open and close, one child per item in the order of the items. A click, a tap or accept on a header opens or closes its section and reports `toggle` with the item id as `item` and the new state as `expanded`. Opening a section closes the others unless `multiple` is set, and each section it closes reports `toggle` with `expanded` set to `false` first. The headers take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The sections. Items use `id`, `text` and `enabled`. |
| `expanded` | list of strings | none | Ids of the open sections. The player opens and closes sections afterwards, and setting it again replaces the open sections. |
| `multiple` | boolean | `false` | Lets several sections stay open. |

A value of `expanded` that is not a list of strings raises `The property "expanded" of an "accordion" must be a list of item ids.`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.accordion{
    width = 800,
    items = {{id = 'controls', text = 'Controls'}, {id = 'goals', text = 'Goals'}},
    expanded = {'controls'},
    onToggle = function(event)
        print(event.item .. ' is now ' .. (event.expanded and 'open' or 'closed'))
    end,
    ui.label{text = 'Move with the left stick and chop with the south button.'},
    ui.label{text = 'Survive ten nights on the island.'},
})
```

### ui.carousel(properties)

Pages shown one at a time that slide sideways, one child per page, such as a tutorial or a level picker. A swipe or a drag of the pages, the arrow buttons, which draw above pages that fill the carousel, and the page dots change the page and report `change` with the page number, counted from 1, as `page`. The page dots take the focus, and left and right turn the pages from there. The controls of the other pages stay out of navigation.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `page` | integer, from 1 | `1` | The page shown. |
| `loop` | boolean | `false` | Goes from the last page on to the first and back. |
| `indicators` | boolean | `true` | Shows the page dots under the pages. Without them the whole carousel takes the focus. |
| `arrows` | boolean | `true` | Shows the arrow buttons on the sides. |
| `interval` | number from 0 to 3600 | `0` | Seconds before the next page turns on its own, looping. The value `0` turns pages only when the player does. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.carousel{
    width = 1000,
    height = 520,
    align = 'center',
    interval = 6,
    onChange = function(event)
        print('page ' .. event.page)
    end,
    ui.image{image = 'tutorial/move.png'},
    ui.image{image = 'tutorial/chop.png'},
    ui.image{image = 'tutorial/build.png'},
})
```

### ui.formField(properties)

A label above one child control, with a help line or an error line under it. The error replaces the help and uses the danger text color.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `label` | text | none | Text above the control. |
| `help` | text | none | Hint under the control. |
| `error` | text | none | Error under the control, shown instead of `help`. |
| `required` | boolean | `false` | Adds ` *` to the label. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.formField{
    id = 'nameField',
    label = 'Name',
    help = 'Shown on the leaderboard',
    required = true,
    ui.textField{placeholder = 'Your name', onChange = function(event)
        event.gui:set('nameField', {error = #event.value < 3 and 'Too short' or ''})
    end},
})
```

### ui.splitter(properties)

Two children beside each other, or one above the other, with a handle between them that the player drags. Without a `height`, a splitter side by side is as tall as its taller child at its share of the width, and a stacked splitter is as tall as both children.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ratio` | number from 0.05 to 0.95 | `0.5` | Share of the space the first child takes. |
| `vertical` | boolean | `false` | Stacks the children instead of placing them side by side. |

Releasing the handle reports `resize` with the new `ratio`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.splitter{
    ratio = 0.3,
    onResize = function(event)
        print('split at ' .. event.ratio)
    end,
    ui.list{items = {{id = 'a', text = 'Alpha'}, {id = 'b', text = 'Beta'}}},
    ui.label{text = 'Details'},
})
```

### ui.safeArea(properties)

Keeps its one child inside the safe area of the screen, and the child fills the part of the safe area the safe area node covers. It suits GUIs mounted with `placement = 'screen'` that draw a background to the edges but hold controls a notch must not hide. It has no properties of its own.

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/title.png', fit = 'cover', align = 'stretch'},
    ui.safeArea{ui.column{justify = 'end', ui.button{text = 'Start', align = 'end'}}},
}, {placement = 'screen'})
```

## Text

### ui.label(properties)

Text in one of the theme fonts.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The text. |
| `font` | string | `'body'` | Font role: `'body'`, `'caption'`, `'button'`, `'heading'`, `'title'` or `'monospace'`. |
| `color` | theme color | `'text'` | Color of the text. |
| `textAlign` | string | `'start'` | One of `'start'`, `'center'` or `'end'` inside the label bounds, where start is the left of a left-to-right UI and the right of a right-to-left one. |
| `wrap` | boolean | `true` | Wraps long text onto more lines. Without wrapping every line of the text, each one ended by a line break, stays on its own line and ends with an ellipsis when it does not fit, and the block of lines sits in the middle of the height of the label. |
| `outline` | color | none | Draws an outline around the letters in this color. |
| `outlineWidth` | number from 0 to 16 | `2` | Thickness of the outline. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.label{text = 'Chapter 1', font = 'title', color = 'accentText'},
    ui.label{text = {key = 'story.intro'}, textAlign = 'center'},
    ui.label{text = 'A very long hint that stays on one line', wrap = false, width = 300},
    ui.label{text = 'Over the sea', outline = '#FF000000', outlineWidth = 3},
})
```

### ui.richText(properties)

Text written in the BBCode markup of the [text guide](../text.md#markup), with bold, italic, colors, outlines, shadows, glows, lists, rules, tables, inline images and icons, links, hints, animated effects and a typewriter reveal. The 2D renderer draws it inside the clip of the UI, from the family of its theme font role, so a font registered as a family with `ui.addFont` gives it real bold and italic faces and fallback fonts. The `[font=name]` tags name fonts registered with `ui.addFont`, `[img=path]` images load through the UI like `ui.image`, and `[icon=name]` shows icons registered with `graphics2d.registerTextIcon`. Links are focusable items that the pointer, the keyboard and gamepads activate, and hints show as tooltips. The [transforms](#guitransformid) of the node and of the nodes around it scale, fade and tint its text like the rest of the UI.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The markup. Malformed markup raises the error of `graphics2d.newRichText`. |
| `font` | string | `'body'` | Font role: `'body'`, `'caption'`, `'button'`, `'heading'`, `'title'` or `'monospace'`. |
| `color` | theme color | `'text'` | Color of text without a `[color]` tag. |
| `textAlign` | string | `'start'` | One of `'start'`, `'end'`, `'left'`, `'center'`, `'right'` or `'fill'` for paragraphs without their own alignment, where start and end follow the direction of the UI and left and right name fixed sides. Every paragraph reads in the direction of its first strong letter unless its markup sets `[p dir]`. |
| `wrap` | boolean | `true` | Wraps the paragraphs at the width of the node. Without wrapping the text keeps its natural width and aligns as one block. |
| `revealSpeed` | number | `0` | Characters per second of the typewriter reveal, which starts again whenever the text or the theme changes. 0 shows everything at once. |
| `visibleCharacters` | integer | `-1` | Shows only the first characters, from where the reveal continues. -1 shows everything. |

| Event | Values | When |
| --- | --- | --- |
| `link` | `link` | A link was clicked, tapped or activated with the focus. The field `link` is the payload of `[url=payload]`, or the text of `[url]text[/url]`. |
| `linkHover` | `link`, `hovered` | The pointer entered a link, with `hovered` `true`, or left it, with `hovered` `false`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{padding = 40, gap = 16,
    ui.richText{id = 'intro', text = '[b]Welcome![/b] Read the [url=rules]rules[/url] or [url=play]start playing[/url].\n[ul]Collect [color=gold]wood[/color]\nKeep the [wave]fire[/wave] burning[/ul]', revealSpeed = 40, onLink = function(event)
        print('open', event.link)
    end, onLinkHover = function(event)
        print(event.link, event.hovered)
    end},
    ui.richText{text = '[center]Press [icon=confirm] to continue[/center]', font = 'caption', textAlign = 'center'},
})
```

### ui.pageHeader(properties)

A page title in the title font with an optional caption under it. With `banner` it sits on the theme `banner` surface, such as a ribbon, or on the accent color.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | text | none | The title. |
| `caption` | text | none | Text under the title. |
| `banner` | boolean | `false` | Draws the banner behind the text and writes the text in the `onAccent` color. |
| `textAlign` | string | `'start'` | `'start'`, `'center'` or `'end'`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.pageHeader{title = 'Settings', caption = 'Tune the island', banner = true, textAlign = 'center'},
})
```

### ui.sectionTitle(properties)

A heading that starts a section, in the heading font.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The heading. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.sectionTitle{text = 'Audio'},
    ui.toggle{text = 'Music'},
})
```

### ui.emptyState(properties)

A placeholder for a view with nothing to show yet, with an optional picture, a title and a message, centered.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | Picture above the title. |
| `imageSize` | number from 0 to 4096 | `128` | Width and height of the picture. |
| `title` | text | none | Title in the heading font. |
| `message` | text | none | Message in the muted text color. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.emptyState{
    image = 'ui/empty_chest.png',
    title = 'Nothing here',
    message = 'Chop some trees to fill your inventory.',
})
```

### ui.alert(properties)

A message box with a colored bar on its start edge.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `tone` | tone | `'information'` | Colors of the bar, the background and the title. |
| `title` | text | none | Title in the button font. |
| `message` | text | none | Message text. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.alert{tone = 'warning', title = 'Careful', message = 'Night is coming.'})
```

## Buttons

### ui.button(properties)

A button that reports `click` when pressed. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | Label of the button. |
| `icon` | image | none | Picture before the label, as large as the theme `iconSize` metric. |
| `variant` | variant | `'default'` | Look of the button. |
| `checked` | boolean | `false` | Marks the button as active with the selection color, which suits toolbar toggles. |

The variants look like this:

| Variant | Look |
| --- | --- |
| `'default'` | Raised fill with a border, or the theme `button` surfaces. |
| `'primary'` | Accent fill, or the theme `buttonPrimary` surfaces. |
| `'destructive'` | Danger fill, or the theme `buttonDestructive` surfaces. |
| `'toolbar'` | No fill until hovered, with the icon tinted like the text. |
| `'icon'` | A square as tall as the theme `controlHeight` metric, with no fill until hovered. |
| `'link'` | Text only in the accent text color, underlined while hovered. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.button{text = 'Play', variant = 'primary', onClick = function() print('play') end},
    ui.button{text = 'Delete save', variant = 'destructive'},
    ui.button{icon = 'ui/hammer.png', variant = 'toolbar', checked = true},
    ui.button{icon = 'ui/gear.png', variant = 'icon', tooltip = 'Settings'},
    ui.button{text = 'Credits', variant = 'link'},
})
```

### ui.imageButton(properties)

A picture that works as a button, with optional pictures for hover and press and optional text on top in the `onAccent` color. It measures as large as its picture times `scale`. Without a hover picture it darkens while hovered, and without a pressed picture it moves down a little while held. It reports `click` and can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | Normal picture. |
| `hoverImage` | image | none | Picture while hovered. |
| `pressedImage` | image | none | Picture while held. |
| `text` | text | none | Text drawn centered on the picture. |
| `scale` | number from 0 to 64 | `1` | Size of the button relative to the picture. |
| `tint` | color | `'#FFFFFFFF'` | Color multiplied with the pictures. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.imageButton{
    image = 'ui/wood_button.png',
    hoverImage = 'ui/wood_button_hover.png',
    pressedImage = 'ui/wood_button_pressed.png',
    text = 'Start',
    scale = 2,
    onClick = function() print('start') end,
})
```

### ui.chip(properties)

A small pill that toggles when pressed and reports `change` with `selected`. A removable chip has its own cross button that reports `remove`. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | Label of the chip. |
| `selected` | boolean | `false` | Whether the chip is on. |
| `removable` | boolean | `false` | Adds the remove button. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.chip{id = 'wood', text = 'Wood', selected = true, onChange = function(event)
        print('wood filter ' .. tostring(event.selected))
    end},
    ui.chip{id = 'stone', text = 'Stone', removable = true, onRemove = function(event)
        event.gui:set('stone', {visible = false})
    end},
})
```

### ui.menuButton(properties)

A button that opens a menu of items and reports `select` with the item id as `item`. It takes the properties of `button` except `checked`, plus `items`, and it can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | Menu entries. Items use `id`, `text` and `enabled`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.menuButton{
    text = 'File',
    items = {{id = 'save', text = 'Save'}, {id = 'load', text = 'Load', enabled = false}},
    onSelect = function(event)
        print('picked ' .. event.item)
    end,
})
```

### ui.popover(properties)

A button that opens a floating panel holding its one child. It takes the properties of `button` except `checked`, plus `contentWidth`, and it can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `contentWidth` | number from 0 to 10000 | `480` | Width of the floating panel. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.popover{
    text = 'Help',
    contentWidth = 520,
    ui.label{text = 'Drag the map to look around and tap a tile to walk there.'},
})
```

## Choices

### ui.checkbox(properties)

A box with a check mark and a label. Pressing it flips `checked` and reports `change` with `checked`. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | Label after the box. |
| `checked` | boolean | `false` | Whether the box is checked. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.checkbox{text = 'Show hints', checked = true, onChange = function(event)
    print('hints ' .. tostring(event.checked))
end})
```

### ui.toggle(properties)

A switch with a label. Off, its track takes the theme `track` color with the `knob` at its start, and on, the `accent` color covers the track with the knob at its end. A change slides the knob across over the theme `transitionDuration` while the color of the on state fades in or out over the whole track, so the track itself never moves or changes its shape. The theme `track`, `trackFill` and `knob` surfaces draw it when the theme has them, with `trackFill` inside the padding of `track`. It takes the properties of `checkbox`, reports `change` with `checked` and can take the focus.

```lua
local ui = require('haylen.ui')

ui.mount(ui.toggle{text = 'Vibration', onChange = function(event)
    print('vibration ' .. tostring(event.checked))
end})
```

### ui.radioGroup(properties)

A group of options of which one is picked. Picking another option reports `change` with the item id as `value`. It can take the focus, which goes to the selected option.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The options. Items use `id`, `text` and `enabled`. |
| `selected` | string | none | Id of the picked option. |
| `horizontal` | boolean | `false` | Places the options in a row instead of a column. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.radioGroup{
    items = {{id = 'easy', text = 'Easy'}, {id = 'hard', text = 'Hard'}},
    selected = 'easy',
    horizontal = true,
    onChange = function(event)
        print('difficulty ' .. event.value)
    end,
})
```

### ui.combo(properties)

A field that shows the selected item and opens a list of the others. Picking another item reports `change` with the item id as `value`. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The choices. Items use `id`, `text` and `enabled`. |
| `selected` | string | none | Id of the selected item. |
| `placeholder` | text | none | Text shown while no item is selected. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.combo{
    width = 400,
    items = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}},
    placeholder = 'Language',
    onChange = function(event)
        print('language ' .. event.value)
    end,
})
```

### ui.segmentedControl(properties)

A row of joined segments of which one is selected, such as a switch between views. A click or a tap picks a segment and reports `change` with the item id as `value`. It takes the focus as a whole: left and right move the selection, skipping disabled segments, and accept moves it to the next segment, starting over after the last one. Without a selection, accept picks the first segment that can be picked.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The segments, which share the width. Items use `id`, `text` and `enabled`. |
| `selected` | string | none | Id of the selected segment. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.segmentedControl{
    width = 600,
    items = {{id = 'daily', text = 'Daily'}, {id = 'weekly', text = 'Weekly'}, {id = 'all', text = 'All time'}},
    selected = 'daily',
    onChange = function(event)
        print('leaderboard ' .. event.value)
    end,
})
```

## Inputs

### ui.textField(properties)

A one-line text entry. Typing reports `change` and Enter, or the return key of the on-screen keyboard, reports `submit`, both with the text as `value`. It can take the focus.

The focused field edits through the native text input of the platform, which opens the on-screen keyboard on phones, tablets, TVs and mobile browsers and brings input methods, autocorrection, dictation and native paste everywhere. The keyboard options below choose that keyboard. While the input method composes text, as Japanese and Chinese input do, the field underlines the composing text. When the on-screen keyboard would cover the focused field, the whole UI moves up until the field shows above it. The [text input guide](../text-input.md) describes what each platform does.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | string | `''` | The text. The text the player types stays in the component when `set` changes other properties. |
| `placeholder` | text | none | Hint shown while the field is empty. |
| `maxLength` | integer from 0 to 1048576 | `0` | Largest length in characters (Unicode code points). The value `0` sets no limit. |
| `keyboard` | `'text'`, `'number'`, `'decimal'`, `'phone'`, `'email'`, `'url'` or `'search'` | `'text'` | The on-screen keyboard: letters, digits, digits with a decimal separator, a phone pad, or the letters with the keys of addresses, links or searches. Text areas and secret fields have keyboards of their own. |
| `returnKey` | `'default'`, `'done'`, `'go'`, `'next'`, `'search'` or `'send'` | `'default'` | Label of the return key of the on-screen keyboard. With `'next'` the return key moves the focus to the next field, like Tab, instead of reporting `submit`. |
| `autocorrect` | boolean | `true` for `'text'`, `'search'` and text areas, `false` otherwise | Whether the keyboard corrects spelling and offers suggestions. |
| `autocapitalize` | `'none'`, `'sentences'`, `'words'` or `'characters'` | `'sentences'` for `'text'` and text areas, `'none'` otherwise | Which letters the keyboard capitalizes on its own. |

Escape, or the escape key of a hardware keyboard on a phone, brings back the text the field had when it took the focus and lets the focus go. Closing the on-screen keyboard, or leaving the field, lets the focus go and keeps the text.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.textField{
        placeholder = 'Name your island',
        maxLength = 24,
        autocapitalize = 'words',
        returnKey = 'next',
        onChange = function(event) print('typing ' .. event.value) end,
    },
    ui.textField{
        placeholder = 'Email',
        keyboard = 'email',
        returnKey = 'done',
        onSubmit = function(event) print('email ' .. event.value) end,
    },
})
```

### ui.secretField(properties)

A `textField` that hides what the player types, for passwords and codes. It takes the properties and reports the events of `textField`, except `keyboard`: it always uses the password keyboard, which neither corrects nor capitalizes by default and keeps input methods that compose text away.

```lua
local ui = require('haylen.ui')

ui.mount(ui.secretField{placeholder = 'Password', returnKey = 'go', onSubmit = function(event)
    print('password has ' .. #event.value .. ' bytes')
end})
```

### ui.textArea(properties)

A text entry of several lines, where Enter, and the return key of the on-screen keyboard, starts a new line. It takes the properties of `textField` except `keyboard`, plus `rows`, reports `change` and can take the focus. It never reports `submit`, and it shows its `placeholder` while it is empty.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `rows` | integer from 1 to 200 | `4` | Height in lines of text. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.textArea{rows = 6, value = 'Dear diary,\n', onChange = function(event)
    print('diary has ' .. #event.value .. ' bytes')
end})
```

### ui.filterField(properties)

A search field with a magnifier and a button that clears it. It takes the properties and reports the events of `textField`, except `keyboard`: it always uses the search keyboard. Clearing reports `change` with an empty `value`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.filterField{placeholder = 'Search recipes', onChange = function(event)
    print('filter ' .. event.value)
end})
```

### ui.numberField(properties)

A number between a minimum and a maximum, changed with its minus and plus buttons or typed into the middle. A change reports `change` with the number as `value`. A typed number inside the range reports it at once, and one outside it waits until the player submits or leaves the field, which brings it into the range. Text that is not a finite number, such as `nan`, changes nothing, and the field shows the number again once the editing ends. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number | `0` | The number, kept between `min` and `max`. |
| `min` | number | `0` | Smallest value. |
| `max` | number | `100` | Largest value. |
| `step` | number, at least 0 | `1` | Amount the buttons add or remove. |
| `decimals` | integer from 0 to 6 | `0` | Decimals shown. |
| `returnKey` | `'default'`, `'done'`, `'go'`, `'next'`, `'search'` or `'send'` | `'default'` | Label of the return key of the on-screen keyboard, as in `textField`. |

The on-screen keyboard shows digits, with a decimal separator when `decimals` is above zero. Numeric keypads have no minus sign, so a field whose `min` is negative types on the text keyboard. A minimum above the maximum raises `The property "min" of a "numberField" must not be greater than "max".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.numberField{value = 2, min = 1, max = 8, onChange = function(event)
    print('players ' .. event.value)
end})
```

### ui.slider(properties)

A track with a knob the player drags. A change reports `change` with the number as `value`. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number | `0` | The number, kept between `min` and `max`. |
| `min` | number from -1e15 to 1e15 | `0` | Value at the start of the track. |
| `max` | number from -1e15 to 1e15 | `1` | Value at the end of the track. |
| `step` | number, at least 0 | `0` | Snaps the value to multiples of the step once the player moves it, with the pointer or with left and right. The value `0` lets it move freely. |
| `showValue` | boolean | `false` | Shows the value after the track, in the room the wider of `min` and `max` takes. |
| `decimals` | integer from 0 to 6 | `2` | Decimals of the shown value. |

Only the player changes the value, so a slider reports `change` only when the player moves it, and a `value` given off its step stays as it was given until then. A minimum that is not smaller than the maximum raises `The property "min" of a "slider" must be smaller than "max".`, and an end outside the range raises `The property "min" of a "slider" must be from -1e+15 to 1e+15.` or the same error for `max`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.slider{value = 0.8, showValue = true, onChange = function(event)
    print('volume ' .. event.value)
end})
```

### ui.rangeSlider(properties)

A track with two knobs that pick a range, such as a price filter. The pointer drags the knob nearer to where it presses, up to the other knob. It takes the focus as a whole: left and right move the active knob and accept switches to the other knob, which shows its own ring. A change reports `change` with `low` and `high`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `low` | number | `0` | Start of the range. |
| `high` | number | `1` | End of the range, at least `low`. |
| `min` | number from -1e15 to 1e15 | `0` | Value at the start of the track. |
| `max` | number from -1e15 to 1e15 | `1` | Value at the end of the track. |
| `step` | number, at least 0 | `0` | Snaps both ends to multiples of the step. The value `0` lets them move freely, and left and right then move by a twentieth of the track. |
| `showValue` | boolean | `false` | Shows the range after the track, in the room the wider of `min` and `max` takes at both ends. |
| `decimals` | integer from 0 to 6 | `2` | Decimals of the shown range. |

A minimum that is not smaller than the maximum raises `The property "min" of a "rangeSlider" must be smaller than "max".`, an end of the track outside its range raises `The property "min" of a "rangeSlider" must be from -1e+15 to 1e+15.` or the same error for `max`, and a `low` above `high` raises `The property "low" of a "rangeSlider" must not be greater than "high".`. A `set` of one end past the other end, where the player moved it, takes that end along.

```lua
local ui = require('haylen.ui')

ui.mount(ui.rangeSlider{min = 0, max = 500, low = 50, high = 300, step = 10, showValue = true, decimals = 0, onChange = function(event)
    print('from ' .. event.low .. ' to ' .. event.high)
end})
```

### ui.stepper(properties)

A value between two arrows that step it, a number or one of a list of options, the way console settings pick a difficulty. The arrows step it, a click or a tap on the value moves to the next one, and a change reports `change` with the number or the item id as `value`. It takes the focus as a whole: left and right step it and accept moves to the next value, starting over after the last one.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number | `0` | The number, kept between `min` and `max`, for a stepper without items. |
| `min` | number | `0` | Smallest number. |
| `max` | number | `10` | Largest number. |
| `step` | number above 0 | `1` | Amount one step adds or removes. |
| `decimals` | integer from 0 to 6 | `0` | Decimals shown. |
| `items` | items | empty | Options to step through instead of a number, skipping disabled ones. Items use `id`, `text` and `enabled`. |
| `selected` | string | first item | Id of the selected option. |
| `wrap` | boolean | `false` | Lets the arrows and left and right go from one end on to the other. |

A minimum above the maximum raises `The property "min" of a "stepper" must not be greater than "max".`, and a step of zero or less raises `The property "step" of a "stepper" must be greater than zero.`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.stepper{value = 2, min = 1, max = 4, onChange = function(event)
        print('players ' .. event.value)
    end},
    ui.stepper{items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = 'normal', wrap = true, onChange = function(event)
        print('difficulty ' .. event.value)
    end},
})
```

### ui.colorField(properties)

A swatch with the color written as text that opens a color picker. A change reports `change` with the color as a `'#AARRGGBB'` string in `value`. It can take the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | color | `'#FFFFFFFF'` | The color. |
| `alpha` | boolean | `true` | Lets the player change the opacity. Without it every picked color is opaque. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.colorField{value = '#FF2E7D32', alpha = false, onChange = function(event)
    print('flag color ' .. event.value)
end})
```

### ui.keyCapture(properties)

A field that shows an [action map binding](input.md#bindings) and, once pressed, listens for the next key, mouse button, gamepad button or gamepad axis and takes it as its binding, which a controls screen hands to `input.defineAction`. A binding it takes reports `change` with the binding string as `value`, such as `'key:w'`, `'mouse:right'`, `'button:south'` or `'axis:leftX+'`. Sticks and triggers count once they travel well away from where they rested when listening started. While it listens it keeps every input from the rest of the UI, and a binding of `cancelWith` stops it and reports `cancel`. It takes the focus, and accept starts listening.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | string | `''` | The binding, shown for people, such as `Left Shift` for `'key:leftShift'` and `1` for `'key:digit1'`. |
| `placeholder` | text | none | Text shown while there is no binding. |
| `prompt` | text | `'…'` | Text shown while it listens. |
| `sources` | list of strings | every source | Kinds of input it takes: `'key'`, `'mouse'`, `'button'` and `'axis'`. It ignores the others and goes on listening. |
| `cancelWith` | list of bindings | `{'key:escape'}` | Bindings that stop listening instead of becoming the value. |

A value that is not a binding raises `The property "value" of a "keyCapture" must be a binding such as "key:space".`.

```lua
local input = require('haylen.input')
local ui = require('haylen.ui')

ui.mount(ui.settingsRow{
    label = 'Jump',
    ui.keyCapture{value = 'key:space', prompt = 'Press a key', width = 320, onChange = function(event)
        input.defineAction({name = 'jump', type = 'button', bindings = {event.value}})
    end},
})
```

## Indicators

### ui.badge(properties)

A small pill with short text, such as a count or a state.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | Text of the badge. |
| `tone` | tone | `'neutral'` | Colors of the badge. |
| `solid` | boolean | `false` | Uses the strong fill of the tone instead of its subtle background. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.badge{text = 'New', tone = 'accent', solid = true},
    ui.badge{text = 3, tone = 'danger'},
})
```

### ui.statusIndicator(properties)

A colored dot with optional text, such as an online state.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | Text after the dot. |
| `tone` | tone | `'success'` | Color of the dot. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.statusIndicator{text = 'Server offline', tone = 'danger'})
```

### ui.busyIndicator(properties)

A spinning ring that shows work in progress.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `size` | number from 4 to 1024 | `48` | Width and height. |
| `color` | theme color | `'accent'` | Color of the ring. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    align = 'center',
    ui.busyIndicator{size = 64, color = 'information'},
    ui.label{text = 'Loading'},
})
```

### ui.progress(properties)

A bar filled up to a value between 0 and 1, drawn with the theme `track` and `trackFill` surfaces when the theme has them, which suits health and loading bars.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number from 0 to 1 | `0` | Filled share of the bar. |
| `tone` | tone | `'accent'` | Color of the fill. The tone `'neutral'` draws like `'accent'`. |
| `text` | text | none | Text centered on the bar. |

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.progress{id = 'health', value = 1, tone = 'success', text = 'HP'})

local function damage(health)
    hud:set('health', {value = health, tone = health < 0.3 and 'danger' or 'success'})
end

damage(0.25)
```

### ui.circularProgress(properties)

A value between 0 and 1 drawn around a circle: a ring that fills clockwise from the top, or a shade over a picture that uncovers it clockwise as the value falls, like the cooldown of an ability.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number from 0 to 1 | `0` | Filled share of the ring, or share of the picture still shaded. |
| `variant` | string | `'ring'` | `'ring'` or `'cooldown'`. |
| `size` | number from 0 to 2048 | `0` | Width and height. The value `0` uses the theme `circularProgressSize` metric. |
| `thickness` | number from 0 to 512 | `0` | Width of the ring. The value `0` uses the theme `circularProgressThickness` metric. |
| `tone` | tone | `'accent'` | Color of the ring, and of the circle behind a cooldown without a picture. |
| `text` | text | none | Text in the middle, such as the seconds left. A cooldown outlines it with the `window` color, so it reads over any picture. |
| `image` | image | none | Picture under the shade of a cooldown. |

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.row{
    gap = 24,
    ui.circularProgress{id = 'loading', value = 0.4, text = '40%'},
    ui.circularProgress{id = 'dash', variant = 'cooldown', image = 'ui/dash.png', size = 96},
})

local function showCooldown(left, total)
    hud:set('dash', {value = left / total, text = left > 0 and math.ceil(left) or ''})
end

showCooldown(2.5, 4)
```

### ui.icon(properties)

A small square picture, such as an item or resource icon, optionally tinted with a theme color.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture. |
| `size` | number from 0 to 1024 | `0` | Width and height. The value `0` uses the theme `iconSize` metric. |
| `color` | theme color | none | Tint of the picture. Without it the picture keeps its colors. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.icon{image = 'ui/coin.png'},
    ui.icon{image = 'ui/heart.png', size = 48, color = 'danger'},
})
```

### ui.image(properties)

A picture that measures as large as its texture times `scale` and fits into the bounds it gets.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture. |
| `fit` | string | `'contain'` | The value `'contain'` shows the whole picture inside the bounds, `'cover'` fills the bounds and crops what spills over, and `'fill'` stretches the picture to the bounds. |
| `scale` | number from 0 to 64 | `1` | Measured size relative to the texture. |
| `tint` | color | `'#FFFFFFFF'` | Color multiplied with the picture. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.image{image = 'ui/logo.png', scale = 2},
    ui.image{image = 'ui/banner.png', fit = 'cover', height = 200, align = 'stretch', tint = '#C0FFFFFF'},
})
```

### ui.avatar(properties)

A round picture of a player, or the initials of the first two words of the name when there is no picture.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture. |
| `name` | text | none | Name whose initials show without a picture. |
| `size` | number from 8 to 1024 | `64` | Diameter. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.avatar{name = 'Ana Souza'},
    ui.avatar{image = 'ui/players/bia.png', size = 96},
})
```

## Collections

### ui.list(properties)

Rows of items, as tall as the theme `listRowHeight` metric, with an optional picture and caption. Pressing a row selects it and reports `select` with the item id as `item`. Pressing the selected row reports it again. The rows take the focus.

A draggable list moves rows the way a [slot grid](#uislotgridproperties) moves slots: the pointer drags a row onto a row or a slot of any list or slot grid, and the keyboard, gamepads and remotes carry it with accept. It reports `dragStart` and `drop` like a slot grid, and accept carries rows instead of selecting them.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The rows. Items use `id`, `text`, `caption`, `image` and `enabled`. |
| `selected` | string | none | Id of the highlighted row. |
| `draggable` | boolean | `false` | Lets the player drag and carry rows and drop items on them. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.list{
    items = {
        {id = 'slot1', text = 'Slot 1', caption = 'Day 12, 3 hours'},
        {id = 'slot2', text = 'Slot 2', caption = 'Empty'},
    },
    selected = 'slot1',
    onSelect = function(event)
        print('picked the save slot ' .. event.item)
    end,
})
```

### ui.tree(properties)

Nested items that open and close. Pressing the arrow of an item with children opens or closes it and reports `toggle` with the item id as `item` and the new state as `expanded`. Pressing the rest of a row selects it and reports `select` with the item id as `item`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The items. Items use every item key, including `children`. |
| `selected` | string | none | Id of the highlighted item. |
| `expanded` | list of strings | none | Ids of the open items. The player opens and closes items afterwards, and setting it again replaces the open items. |

A value of `expanded` that is not a list of strings raises `The property "expanded" of a "tree" must be a list of item ids.`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.tree{
    items = {
        {id = 'tools', text = 'Tools', children = {{id = 'axe', text = 'Axe'}, {id = 'pick', text = 'Pickaxe'}}},
        {id = 'food', text = 'Food', children = {{id = 'fish', text = 'Fish'}}},
    },
    expanded = {'tools'},
    onToggle = function(event)
        print(event.item .. ' is now ' .. (event.expanded and 'open' or 'closed'))
    end,
})
```

### ui.table(properties)

Rows of cells under a header. Columns with a `width` keep it, and the other columns share the rest. Pressing a row selects it and reports `select` with the row id as `item`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | list of columns | empty | The columns. |
| `rows` | list of rows | empty | The rows. |
| `selected` | string | none | Id of the highlighted row. |

A column is a table with these keys.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | Header text. |
| `width` | number from 0 to 10000 | shared | Fixed width of the column. |
| `align` | string | `'start'` | One of `'start'`, `'center'` or `'end'` for the header and the cells. |

A row is a table with a unique `id` string and a `cells` list of texts, one per column. Rows without an id or cells raise `The property "rows" of a "table" must hold objects with an id and a list of cells.`, a repeated id raises `The property "rows" of a "table" uses the row id "<id>" more than once.`, an unknown column key raises `Unknown key "<key>" in "table.columns".`, a column width out of range raises `The property "columns" of a "table" has a width that is not a number from 0 to 10000.`, and an unknown column alignment raises `The property "columns" of a "table" has an "align" other than "start", "center" or "end".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.table{
    columns = {{text = 'Player'}, {text = 'Score', width = 160, align = 'end'}},
    rows = {
        {id = 'ana', cells = {'Ana', 1200}},
        {id = 'bia', cells = {'Bia', 950}},
    },
    selected = 'ana',
})
```

### ui.slotGrid(properties)

A grid of square slots that hold pictures and counts, such as an inventory, a chest or a hotbar. A click or a tap selects a slot and reports `select` with the slot id as `item`. The pointer drags the item of a slot onto another slot or a row of any slot grid or draggable list, in the same GUI or another one, and the keyboard, gamepads and remotes carry it: accept picks the focused slot up, the focus moves, and accept drops it on another slot, while accept on the carried slot or `uiCancel` puts it back. Picking up reports `dragStart` with the slot id as `item`, and a drop reports `drop` on the node it lands on, with the slot or row it lands on as `item`, the id of the node the item left as `source` and its slot or row as `sourceItem`. The grid only reports moves, so the app moves its items and sets the new slots. Every slot takes the focus.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `slots` | list of slots | empty | The slots, in reading order. |
| `columns` | integer from 1 to 64 | `4` | Slots per row. |
| `slotSize` | number from 0 to 1024 | `0` | Width and height of each slot. The value `0` uses the theme `slotSize` metric. |
| `gap` | number from 0 to 1024 | half the theme `itemSpacing` | Space between slots. |
| `selected` | string | none | Id of the highlighted slot. |
| `draggable` | boolean | `true` | Lets the player drag and carry items. Without it accept selects a slot. |

A slot is a table with a unique `id` string and optional `image`, `count` (text drawn in its corner) and `enabled`. Slots without an id raise `The property "slots" of a "slotGrid" must hold objects with an id.`, a repeated id raises `The property "slots" of a "slotGrid" uses the slot id "<id>" more than once.`, and an unknown key raises `Unknown key "<key>" in "slotGrid.slots".`.

```lua
local ui = require('haylen.ui')

local bag = {wood = 12, stone = 4}
local order = {'wood', 'stone', 'empty1', 'empty2'}

local function slots()
    local list = {}
    for index, name in ipairs(order) do
        list[index] = {id = name, image = bag[name] and ('items/' .. name .. '.png') or nil, count = bag[name]}
    end
    return list
end

local inventory = ui.mount(ui.slotGrid{id = 'bag', columns = 4, slots = slots(), onDrop = function(event)
    local from, to
    for index, name in ipairs(order) do
        if name == event.sourceItem then from = index end
        if name == event.item then to = index end
    end
    if event.source == 'bag' and from and to then
        order[from], order[to] = order[to], order[from]
        event.gui:set('bag', {slots = slots()})
    end
end})
```

### ui.collection(properties)

Shows any number of items of several types in a vertical or horizontal list or in a grid that scrolls along one axis. It builds cells from the templates of its types, binds them to the items in view and reuses them as the player scrolls, so its cost follows the visible items, not the item count, and a list of a hundred thousand items costs about what a list of a hundred does. The app gives it its items through the [`UiCollection`](#uicollection) that [`gui:collection(id)`](#guicollectionid) returns, as a Lua list with `setItems` or in pages with `setPages`. Its one child, such as an `emptyState`, shows while it has no items. The [long lists guide](../ui.md#long-lists) explains how it works and what it costs.

A vertical collection is as long as its content within its size bounds, like `scroll`, so it needs a `height`, a `maxHeight` or `grow` to scroll, and it fills the width it gets. A horizontal collection needs a `width` or `grow` along a row, and it is as tall as its `height`, or as the tallest cell it has measured, which never shrinks while it lives. The wheel scrolls the collection under the pointer, a horizontal one with the vertical wheel too, a finger drags it along its axis and flings it when it lets go, past its ends with resistance, the scrollbar on its end edge drags and pages it, and the focus scrolls the item that takes it into view. Mouse dragging of the content stays off, as in the lists of desktop platforms.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `types` | object | required | Item types mapped to their definitions, described below. |
| `axis` | string | `'vertical'` | The scroll axis, `'vertical'` or `'horizontal'`. |
| `layout` | string | `'linear'` | The value `'linear'` places one item per line, and `'grid'` places cells in lanes across the axis. |
| `lanes` | integer from 1 to 64 | `2` | Cells across the axis of a grid: columns of a vertical grid, rows of a horizontal one. |
| `minCellSize` | number from 0 to 10000 | `0` | Above zero, a grid fits as many lanes as cells at least this long across, instead of `lanes`. |
| `cellAspect` | number from 0 to 100 | `0` | Above zero, grid cells that span less than the whole line are this many times as long along the axis as across, so they need no measuring. |
| `gap` | number from 0 to 10000 | theme `itemSpacing` | Space between lines and between lanes. |
| `padding` | insets | `0` | Space between the edges of the view and the content, which scrolls with the content. |
| `placeholder` | string | none | The type that shows the items of pages that did not load yet. Without it those items take their estimated length and draw nothing. |
| `stickToEnd` | boolean | `false` | Lays short content against the end edge, and keeps the view at the end while items arrive when it was at the end, as a chat does. |
| `snap` | string | `'none'` | Once the content rests, `'item'` settles on the start of the nearest item, `'center'` centers the nearest item, and `'page'` settles on whole views. |
| `scrollbar` | boolean | `true` | Shows the scrollbar while the content is longer than the view. It fades while the content rests after touch input. |
| `prefetch` | number from 0 to 4 | `0.5` | Views bound ahead in the direction of scrolling, so items show bound when they arrive. |
| `poolSize` | integer from 0 to 256 | `8` | Detached cells each type keeps for reuse between frames. |
| `selection` | string | `'none'` | The values `'single'` and `'multiple'` let presses select items. |
| `selected` | list of strings | empty | Ids of the selected items. The player changes the selection afterwards, and setting it again replaces it. |
| `selectionFollowsFocus` | boolean | `false` | With single selection, selects the item that takes the focus, as TV lists do. |
| `focusAlign` | string | `'nearest'` | Where an item that takes the focus scrolls to: `'nearest'` only brings it into view, and `'start'`, `'center'` and `'end'` align it with that part of the view. |
| `rememberFocus` | boolean | `false` | A move that enters the collection from outside lands on the item that had the focus there last, while that item is near the view, instead of on the nearest item. |
| `refreshable` | boolean | `false` | Lets a touch drag past the start of a vertical collection report `refresh`. |
| `refreshing` | boolean | `false` | Shows the busy indicator at the start. A pull sets it, and the app sets it to `false` with `gui:set` when the refresh ends. |
| `endThreshold` | integer from 0 to 10000 | `5` | Items from either end at which `endReached` and `startReached` report. |
| `animateChanges` | boolean | `true` | Slides the items that a change moves, fades in the items it inserts and fades out the items it removes, for the items in view. |

The common properties apply too. The property `focusWrap` along the axis wraps the focus from the last item to the first and back, even when they do not show, and `focusScope` keeps the focus inside the collection.

#### Types and templates

Each entry of `types` names a type and holds its definition, and every item names its type in its `type` field, which a collection of one type does not need.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `template` | node | required | The tree every cell of the type is built from, in the screen format. |
| `estimatedSize` | number from 0 to 100000 | `0` | Length along the axis of an item of the type before it is measured. The value `0` learns it from the measured items of the type, starting from the theme `listRowHeight`. |
| `span` | integer from 1 to 64 or `'full'` | `1` | Lanes a cell of a grid takes, where `'full'` takes the whole line. |
| `sticky` | boolean | `false` | Pins the current item of the type at the start of the view while the items after it scroll under it, until the next one pushes it out, as section headers do. |
| `interactive` | boolean | automatic | Makes the whole cell one focus target that reports `select` when pressed and paints the cell surfaces of the theme behind it. By default a cell is interactive when its template has no part that takes the focus, so a row of labels and pictures takes the focus as a whole, and a row with a toggle lets the toggle take it. |

A template is a node tree with two keys that only templates take. The key `part` names a node of the template, unique in it, so binders, events and `cell:set` can refer to it, and templates take no `id`, since a template is built many times, and no handlers, since the events of every part arrive at the collection with the part name. The key `bind` maps properties of the node to fields of the item, such as `bind = {text = 'title', image = 'picture'}`, and the pseudo fields `'$index'`, the index of the item from 1, and `'$selected'`, whether it is selected, bind what the item does not hold. A field the item lacks gives the property its value in the template, so templates give a value to every property that some items leave out, which keeps reuse cheap.

Bindings write back. When the player changes a bound property, such as the `checked` of a toggle bound to `on`, the collection stores the new value in that field of the item before the handlers of the event run, so the handlers read the item already changed and the next bind of the item shows it. Before a cell shows another item, every part takes the values of its template again: properties that a binder or `cell:set` changed go back to their template values, and a part the player changed, or one whose changed property the template leaves out, is built again from the template. Controls keep the state the UI keeps for them, such as the knob of a toggle, by the item they show, never by the cell, so no state passes from one item to the next.

Templates do not hold a `scroll` or another `collection`, whose child windows and nested items a cell cannot reuse, and the cells of a collection never count against the node limit of their GUI.

Errors are reported when the GUI is mounted or set:

- `A "collection" needs at least one type in "types".` and `The property "types" of a "collection" must map type names to type definitions.`
- `The type "<type>" of a "collection" needs a "template" node.` and `Unknown key "<key>" in "collection.types.<type>".`
- `The "estimatedSize" of the type "<type>" must be a number from 0 to 100000.`, `The "span" of the type "<type>" must be a whole number from 1 to 64 or "full".`, and `The "sticky" of the type "<type>" must be "true" or "false".`, the same for `interactive`.
- `A node inside a collection template names itself with "part", not "id".` and `The "part" of a "<kind>" in the template of the type "<type>" must be a non-empty string.`
- `A collection template takes no handlers. Handle the events of its parts on the collection, where they arrive with "part" and "cell".`
- `The part "<part>" is used more than once in the template of the type "<type>".`
- `The "bind" of a "<kind>" in the template of the type "<type>" must map property names to field names.`
- `The template of the type "<type>" cannot hold a "scroll" or a "collection".`
- `The template of the type "<type>" has a problem.` followed by the error of the node, such as `The property "width" of a "label" must be a non-negative number or "auto".`
- `The property "placeholder" of a "collection" names the type "<type>", which it does not declare.`
- `The sticky type "<type>" of a grid collection must span the whole line.`
- `The property "selected" of a "collection" must be a list of item ids.` and the usual property errors, such as `The property "axis" of a "collection" must be "vertical" or "horizontal".`

An item whose bound value a part does not accept stops the app when its cell binds, with `The item "<item>" gives the part "<part>" a value it does not accept.` followed by the property error, which names `a "<kind>" of its cell` for a node without a part.

#### Events

| Event | Values | When |
| --- | --- | --- |
| `select` | `item`, `index`, and `selected` unless `selection` is `'none'` | An interactive cell was pressed with the pointer, a tap or accept. In multiple selection a press turns the selection of the item over, and a press on the selected item of a single selection reports it again. |
| `itemFocus` | `item`, `index` | The focus moved to another item of the collection. |
| `visibleChange` | `first`, `last` | The indices of the first and last items in view changed, at most once per frame. |
| `scrollEnd` | `offset`, `item` | Scrolling by the player or a `scrollTo` came to rest, with the first item in view whole. |
| `endReached`, `startReached` | `count` | The items in view came within `endThreshold` items of the end or the start, once per item count, so an app appends a page at a time. |
| `refresh` | none | A pull at the start went past the theme `refreshDistance` and let go. |
| the event of a part | its values, `part`, `cell` | A part of a cell reported it, such as `click` of a button or `change` of a toggle, with the name of the part as `part` and the item it shows as `cell`, a table with `item`, `index` and `type`. |

Indices count from 1. Every event of a part arrives on the collection, so a handler such as `onClick` on the collection answers the buttons of every cell and tells them apart by `event.part` and `event.cell.item`.

#### Examples

A list of mixed message types with a binder:

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Chat = {}
Chat.__index = Chat

function Chat:enter()
    self.sent = 0
    self.messages = {
        {id = 'day-1', type = 'day', text = 'Monday'},
        {id = 'm-1', type = 'incoming', author = 'Ana', text = 'Ready for the night watch?'},
        {id = 'm-2', type = 'outgoing', text = 'Almost, my bow needs a new string.'},
        {id = 'n-1', type = 'notice', text = 'Leo joined the camp'},
    }
    self.gui = ui.mount(ui.column{padding = 24, gap = 16, onCancel = function() scene.pop() end,
        ui.collection{id = 'chat', grow = 1, stickToEnd = true, types = {
            day = {template = ui.label{part = 'text', bind = {text = 'text'}, font = 'caption', color = 'textMuted', textAlign = 'center'}, interactive = false},
            incoming = {template = ui.card{ui.label{part = 'author', bind = {text = 'author'}, font = 'caption', color = 'accentText'}, ui.label{part = 'text', bind = {text = 'text'}}}},
            outgoing = {template = ui.card{ui.label{part = 'text', bind = {text = 'text'}}, ui.label{part = 'state', text = '', font = 'caption', color = 'textMuted'}}},
            notice = {template = ui.alert{part = 'notice', bind = {message = 'text'}, tone = 'information'}, interactive = false},
        }, onSelect = function(event) print('opened the message ' .. event.item) end},
        ui.textField{id = 'draft', placeholder = 'Message', returnKey = 'send', onSubmit = function(event) self:send(event.value) end},
    }, {owner = self})
    self.chat = self.gui:collection('chat')
    self.chat:setItems(self.messages)
    self.chat:setBinder('outgoing', function(cell, item)
        cell:set('state', {text = item.pending and 'Sending' or 'Delivered'})
    end)
end

-- The new message lands at the end, and the chat stays at the end because it sticks to it.
function Chat:send(value)
    if value == '' then
        return
    end
    self.sent = self.sent + 1
    self.chat:insert(#self.messages + 1, {{id = 'out-' .. self.sent, type = 'outgoing', text = value, pending = true}})
    self.gui:set('draft', {value = ''})
end

scene.push(setmetatable({}, Chat))
```

A store grid with sticky sections and multiple selection:

```lua
local ui = require('haylen.ui')

local catalog = {}
for section, title in ipairs({'Tools', 'Food', 'Boats'}) do
    catalog[#catalog + 1] = {id = 'section-' .. section, type = 'section', title = title}
    for item = 1, 30 do
        catalog[#catalog + 1] = {id = title .. '-' .. item, type = 'product', name = title .. ' ' .. item, price = (item * 7) .. ' coins'}
    end
end

local gui = ui.mount(ui.collection{
    id = 'store',
    layout = 'grid',
    minCellSize = 320,
    cellAspect = 0.75,
    selection = 'multiple',
    padding = 24,
    types = {
        section = {template = ui.sectionTitle{part = 'title', bind = {text = 'title'}}, span = 'full', sticky = true},
        product = {template = ui.card{
            ui.label{part = 'name', bind = {text = 'name'}, wrap = false},
            ui.badge{part = 'price', bind = {text = 'price'}, tone = 'accent'},
            ui.label{part = 'chosen', text = 'In the cart', color = 'successText', bind = {visible = '$selected'}},
        }},
    },
    onSelect = function(event)
        print(event.item .. (event.selected and ' added to' or ' removed from') .. ' the cart')
    end,
})

local store = gui:collection('store')
store:setItems(catalog)

-- A filtered list compares with the list shown by id, so the products that stay keep their cells, selection and focus.
local function showOnly(prefix)
    local shown = {}
    for _, entry in ipairs(catalog) do
        if entry.type == 'section' or entry.id:sub(1, #prefix) == prefix then
            shown[#shown + 1] = entry
        end
    end
    store:setItems(shown)
end

showOnly('Tools')
```

Settings rows whose controls write back to their items:

```lua
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local rows = {
    {id = 'audio', type = 'header', title = 'Audio'},
    {id = 'music', type = 'slider', label = 'Music', value = preferences.get('music', 0.7)},
    {id = 'video', type = 'header', title = 'Video'},
    {id = 'fullscreen', type = 'toggle', label = 'Fullscreen', on = preferences.get('fullscreen', false)},
}
local fields = {slider = 'value', toggle = 'on'}

local gui = ui.mount(ui.panel{
    ui.collection{id = 'settings', grow = 1, types = {
        header = {template = ui.sectionTitle{part = 'title', bind = {text = 'title'}}, sticky = true, interactive = false},
        slider = {template = ui.settingsRow{part = 'row', bind = {label = 'label'}, ui.slider{part = 'value', bind = {value = 'value'}, width = 440}}},
        toggle = {template = ui.settingsRow{part = 'row', bind = {label = 'label'}, ui.toggle{part = 'value', bind = {checked = 'on'}}}},
    },
    -- The item already holds the new value when the handler runs, because the bound value writes back.
    onChange = function(event)
        local row = rows[event.cell.index]
        preferences.set(row.id, row[fields[row.type]])
    end},
})
gui:collection('settings'):setItems(rows)
```

A hundred thousand entries loaded in pages:

```lua
local jobs = require('haylen.jobs')
local ui = require('haylen.ui')

local gui = ui.mount(ui.column{padding = 24, gap = 12,
    ui.label{id = 'where', text = 'Entry 1'},
    ui.collection{id = 'entries', grow = 1, placeholder = 'loading', types = {
        entry = {template = ui.row{gap = 16,
            ui.label{part = 'number', bind = {text = '$index'}, width = 160, color = 'textMuted'},
            ui.label{part = 'title', bind = {text = 'title'}, grow = 1},
        }, estimatedSize = 64},
        loading = {template = ui.row{height = 64, ui.busyIndicator{size = 32}}, interactive = false},
    }, onVisibleChange = function(event)
        event.gui:set('where', {text = 'Entries ' .. event.first .. ' to ' .. event.last})
    end},
})

local entries = gui:collection('entries')

-- Each page builds in a job that shares the frame budget, so a fast fling never stalls the frame.
entries:setPages{count = 100000, pageSize = 100, load = function(first, count)
    return jobs.spawn(function()
        local page = {}
        for index = first, first + count - 1 do
            page[#page + 1] = {id = 'entry-' .. index, type = 'entry', title = 'Entry ' .. index}
            jobs.checkpoint()
        end
        return page
    end)
end}

entries:scrollTo(50000, {align = 'center', animated = false})
```

## Settings

### ui.settingsForm(properties)

A column of settings rows, section titles and actions, with extra room above every `sectionTitle` child except the first. It holds any number of children and has no properties of its own.

```lua
local ui = require('haylen.ui')

ui.mount(ui.settingsForm{
    ui.sectionTitle{text = 'Video'},
    ui.settingsRow{label = 'Fullscreen', ui.toggle{}},
    ui.sectionTitle{text = 'Audio'},
    ui.settingsRow{label = 'Music', ui.slider{value = 0.7}},
})
```

### ui.settingsRow(properties)

A setting with its label and caption on the left and its one child control on the right. The label column is as wide as the theme `settingsLabelWidth` metric, at most half the row.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `label` | text | none | Name of the setting. |
| `caption` | text | none | Explanation under the label, in the caption font. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.settingsRow{
    label = 'Language',
    caption = 'Changes every text at once',
    ui.combo{width = 360, items = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}}, selected = 'en'},
})
```

### ui.settingsActions(properties)

A row of buttons at the end of a form, such as cancel and save, lined up to the end edge. It holds any number of children and has no properties of its own.

```lua
local ui = require('haylen.ui')

ui.mount(ui.settingsForm{
    ui.settingsRow{label = 'Subtitles', ui.toggle{checked = true}},
    ui.settingsActions{
        ui.button{text = 'Cancel'},
        ui.button{text = 'Save', variant = 'primary'},
    },
})
```

## Overlays

### ui.dialog(properties)

A modal window over the whole screen with a title, a message, optional children and answer buttons, over a backdrop in the theme `overlay` color that dims everything behind it. The dialog and its backdrop fade in together when it opens and fade out together when it closes, over the theme `transitionDuration`, and a closing dialog takes no more answers. It takes no room in the layout that holds it. The last button starts with the focus. Pressing a button closes the dialog and reports `answer` with the button id as `button`. Escape or the east gamepad button closes a dismissible dialog and reports `dismiss`. A dialog the player closed shows again when `set` sets `open = true`. A dialog stays inside the screen, and its title, message and children scroll above the buttons when they are taller than it. One dialog shows at a time, so a dialog that opens while another one shows waits until that one closes. A dialog closes when its node stops drawing, because it, a node around it or its GUI was hidden, and shows again once it draws while `open` is still `true`. The popups of menu buttons, popovers, combos, color fields and context menus close the same way.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `open` | boolean | `false` | Shows the dialog. |
| `title` | text | none | Title in the heading font. |
| `message` | text | none | Message under the title. |
| `dismissible` | boolean | `true` | Lets Escape and the east gamepad button close the dialog. |
| `buttons` | list of buttons | empty | Answer buttons, lined up at the bottom right in list order. |

A button is a table with a unique `id` string, a `text` and a `variant`. Buttons without an id raise `The property "buttons" of a "dialog" must hold objects with an id.`, a repeated id raises `The property "buttons" of a "dialog" uses the id "<id>" more than once.`, and an unknown variant raises `The property "buttons" of a "dialog" has a variant other than "default", "primary", "destructive", "toolbar", "icon" or "link".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.button{text = 'Quit', onClick = function(event)
        event.gui:set('confirm', {open = true})
    end},
    ui.dialog{
        id = 'confirm',
        title = 'Quit the app?',
        message = 'Your progress is saved.',
        buttons = {{id = 'stay', text = 'Stay'}, {id = 'quit', text = 'Quit', variant = 'destructive'}},
        onAnswer = function(event) print('the player chose ' .. event.button) end,
        onDismiss = function(event) print('the dialog was dismissed') end,
        ui.checkbox{text = 'Do not ask again'},
    },
})
```

### ui.toast(properties)

Short notices at an edge of the safe area. The notice of the toast shows while `open` is `true`, and the `show` [command](#guicommandid-name-arguments) adds more notices to the same toast. Every notice slides in from the edge and fades in, stays for its duration, then fades out while the notices after it move up into its place. The notices of every toast at the same position form one stack, in the order they were asked to show, so they never cover each other, and when the stack already shows the theme `toastLimit` notices, the next ones wait their turn and their time starts once they show. A toast takes no room in the layout that holds it and ignores the pointer. Setting `open = true` again shows the notice of the toast again from the start, at the end of the stack, and the end of that notice reports `dismiss`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `open` | boolean | `false` | Shows the notice of the toast. |
| `text` | text | none | The notice. |
| `tone` | tone | `'information'` | Color of the bar on its start edge. |
| `duration` | number from 0 to 3600 | `3` | Seconds on screen. The value `0` keeps the notice until `open` is set to `false`. |
| `position` | string | `'top'` | Where the stack sits: `'top'` or `'bottom'` in the middle of the edge, or `'topStart'`, `'topEnd'`, `'bottomStart'` and `'bottomEnd'` at its sides, where start is the left of a left-to-right UI. |

```lua
local ui = require('haylen.ui')

local notices = ui.mount(ui.toast{id = 'saved', text = 'Progress saved', tone = 'success', position = 'bottom'})

local function showSaved()
    notices:set('saved', {open = true})
end

showSaved()
notices:command('saved', 'show', {text = 'Two new items in the chest', tone = 'information', duration = 5})
notices:command('saved', 'show', {text = 'The storm is coming', tone = 'warning'})
```

### ui.window(properties)

A floating window with a title bar that the pointer drags around, holding a column of children, such as an inventory or a map over the game. It takes no room in the layout that holds it and sizes itself to its children unless it has a `width` or `height`. It takes the properties of `column` for its children, pads like a panel and draws with the theme `window` surface. Dragging the title bar reports `move` with the new `x` and `y` once the pointer lets go, and closing reports `close`. Moves reach the controls of the window from the rest of its GUI and bring it to the front, and `uiCancel` closes a closable window while the focus is inside it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | text | none | Title in the title bar. |
| `x`, `y` | number | the middle of the safe area | Position of the top left corner from the top left of the screen. Setting them moves the window there, and setting only one keeps the other coordinate where the window is. |
| `open` | boolean | `true` | Shows the window. A window the player closed shows again when `set` sets `open = true`. |
| `closable` | boolean | `false` | Adds the close button to the title bar and lets `uiCancel` close the window. |
| `movable` | boolean | `true` | Lets the pointer drag the title bar. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.window{id = 'map', title = 'Map', x = 120, y = 120, width = 640, closable = true,
        onClose = function(event) print('map closed') end,
        onMove = function(event) print('map at ' .. event.x .. ', ' .. event.y) end,
        ui.image{image = 'ui/map.png', fit = 'contain', height = 400},
        ui.button{text = 'Center on me', autofocus = true},
    },
}, {placement = 'screen'})
```

### ui.contextMenu(properties)

A menu of items that opens over its one child: with a right click on it, a long press on a touch screen, `uiMenu` while the focus is inside it, or the `open` command. Picking an item reports `select` with the item id as `item`, and `uiCancel` or a click outside closes the menu. The menu takes the focus while it is open and gives it back when it closes.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | Menu entries. Items use `id`, `text`, `image` and `enabled`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.contextMenu{
    items = {{id = 'rename', text = 'Rename'}, {id = 'delete', text = 'Delete'}},
    onSelect = function(event)
        print('save slot: ' .. event.item)
    end,
    ui.button{text = 'Save slot 1', width = 480},
})
```

## Play areas

### ui.playArea(properties)

The part of a screen where the game shows, as a place the focus can go, such as the space a level leaves between its controls. It draws nothing but the focus ring and has no properties of its own, so it uses `width`, `height` or `grow`. While it has the focus, the directions, accept and menu of the keyboard, gamepads and TV remotes reach the action map of the app instead of moving the focus or pressing a control, while cancel still reaches the `onCancel` handlers of the GUI and Tab and `uiFocus` move the focus to the controls of the GUI and back. It never takes the pointer: a click or a touch on it gives it the focus and reaches the game too, which reads `mouse:` bindings and touches as usual, unless a control drawn over it, such as a touch button, takes the press. It reports `focus` and `blur` like every node, and `gui:bounds(id)` tells where it lies, so a camera can draw the world inside it. The [input guide](../input.md#who-owns-the-keyboard-and-the-gamepad) explains the whole model.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.defineAction({name = 'move', type = 'vector', up = {'key:up'}, down = {'key:down'}, left = {'key:left'}, right = {'key:right'}, bindings = {'stick:left'}})

scene.push({
    enter = function(self)
        self.gui = ui.mount(ui.row{
            ui.playArea{id = 'world', grow = 1, autofocus = true, onFocus = function()
                print('the arrows move the hero')
            end},
            ui.column{width = 360, ui.button{id = 'map', text = 'Map'}},
        }, {owner = self})
    end,
    update = function(self, dt)
        local dx, dy = input.vector('move')
        if dx ~= 0 or dy ~= 0 then
            print('the hero walks ' .. dx .. ', ' .. dy)
        end
    end,
})
```

## Touch controls

On-screen controls drive the virtual buttons and sticks of the action layer, which [`haylen.input`](input.md) actions read through the bindings `virtual:<name>` and `virtualStick:<name>`. Every control follows its own finger, so a stick and several buttons work at the same time, and the mouse drives them when no finger is down. They keep the pointer from reaching the app behind them. A control that stops drawing, because it, a container around it or its GUI was hidden or removed, releases the virtual button or stick it held in that frame. A touch button that was pressed then reports `release`, unless its GUI was unmounted, and a control that shows again under a finger that is still down waits for a new press. A control with `enabled = false` lets go the same way, ignores fingers and the mouse, and waits for a new press once it is enabled again.

### ui.touchStick(properties)

A virtual analog stick that sets the virtual stick `action` to a vector of length 0 to 1 while a finger drags it, drawn with the `stickBase` and `stickKnob` surfaces of its theme or [style](#styles) when they have images, and with circles in the theme colors otherwise. Several sticks work at once, each with its own finger. It reports only [the events of every kind](#the-events-of-every-kind).

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `action` | string | required | Name of the virtual stick it drives. |
| `radius` | number from 8 to 2048 | `110` | Radius of the stick ring. |
| `deadZone` | number from 0 to 0.95 | `0.15` | Share of the radius near the center that counts as zero. |
| `mode` | string | `'fixed'` | Where the ring sits: `'fixed'` keeps it at the center of the control, `'floating'` centers it where the finger lands inside the control, and `'following'` also moves it after a finger that leaves the ring, so the finger always sits on its rim, inside the control. Without a finger the ring rests at the center of the control. |
| `touchOnly` | boolean | `false` | Shows the stick only while the last input came from a touch screen. |

A missing or empty action raises `The property "action" of a "touchStick" names the virtual stick to drive and cannot be empty.`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.loadActions({actions = {
    {name = 'move', type = 'vector', bindings = {'virtualStick:move', 'stick:left'}},
}})
ui.mount(ui.touchStick{action = 'move', radius = 140, mode = 'floating', touchOnly = true, align = 'end'}, {placement = 'screen'})

scene.push({
    update = function(self, dt)
        local x, y = input.vector('move')
        self.speedX, self.speedY = x * 200, y * 200
    end,
})
```

### ui.touchButton(properties)

A virtual button that holds the virtual button `action` down while a finger or the mouse presses it, and reports `press` and `release`. It is drawn with the theme `touchButton` and `touchButtonPressed` surfaces when the theme has them.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `action` | string | required | Name of the virtual button it drives. |
| `text` | text | none | Label in the middle of the button. |
| `image` | image | none | Picture in the middle of the button. |
| `size` | number from 8 to 2048 | `120` | Width and height. |
| `touchOnly` | boolean | `false` | Shows the button only while the last input came from a touch screen. |

A missing or empty action raises `The property "action" of a "touchButton" names the virtual button to drive and cannot be empty.`.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.loadActions({actions = {
    {name = 'jump', type = 'button', bindings = {'virtual:jump', 'key:space', 'button:south'}},
}})
ui.mount(ui.row{
    align = 'end',
    padding = 48,
    ui.touchButton{action = 'jump', text = 'A', onPress = function() print('pressed') end, onRelease = function() print('released') end},
}, {placement = 'screen'})

scene.push({
    update = function(self, dt)
        if input.pressed('jump') then
            print('jump')
        end
    end,
})
```

## Themes

A theme holds every color, metric, font and surface the components use, so no component draws a literal color. The engine ships `dark` and `light`, and an app adds its own with a JSON file in the package assets loaded by `ui.loadTheme`, or with the same definition as a Lua table passed to `ui.addTheme`. A theme file starts from a base theme and lists only what it changes. The active theme also styles [`haylen.imgui`](imgui.md) windows.

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | string | Name of the theme, required and not empty. |
| `colors` | object | Color roles mapped to colors. |
| `metrics` | object | Metrics mapped to non-negative numbers. |
| `fonts` | object | Font roles mapped to `{"font": name, "size": number, "bold": boolean, "italic": boolean}`, where every key is optional. |
| `fontFiles` | object | Font names mapped to TrueType or OpenType files in the package assets, which `ui.loadTheme` registers unless a font with that name exists. |
| `surfaces` | object | Surfaces mapped to nine-slice images, or to `null` to go back to flat colors. |
| `imageFilter` | string | The texture filter of the pictures components show, such as images, icons and image buttons: `'nearest'`, the default, or `'linear'`. A theme switch to another filter loads the pictures again with it. |

Theme files raise these errors: `Unknown key "<key>" in the theme.`, `A theme needs a name.`, `The image filter of the theme must be "nearest" or "linear".`, `The section "<name>" of the theme must be an object.`, `The theme has no color role named "<name>".`, `The theme has no metric named "<name>".`, `The theme has no surface named "<name>".`, `The theme font "<name>" must be a known role with an object value.`, `The theme font "<name>" must name its font with a string.`, `The theme font "<name>" must set "bold" to "true" or "false".`, the same for `italic`, `The theme font file "<name>" must be a path.`, `The theme color "<name>" must be a color such as "#FF2E7D32".`, `The theme metric "<name>" must be a non-negative number.` and `The size of the theme font "<name>" must be a non-negative number.`.

```json
{
    "name": "wood",
    "colors": {"accent": "#FF8B5A2B", "text": "#FF1B1E2B"},
    "metrics": {"controlHeight": 80, "panelPadding": 36},
    "fontFiles": {"pixel": "fonts/pixel.ttf"},
    "fonts": {"title": {"font": "pixel", "size": 72}, "body": {"size": 34}},
    "surfaces": {
        "panel": {"image": "ui/panel.png", "slice": 8, "scale": 2, "padding": [4, 6]},
        "button": {"image": "ui/button.png", "slice": 6, "padding": 4},
        "buttonPressed": {"image": "ui/button-pressed.png", "slice": 6, "padding": 4},
        "track": {"image": "ui/bar.png", "slice": 4, "filter": "linear"},
        "trackFill": {"image": "ui/bar-fill.png", "slice": 4, "fill": "tile"}
    }
}
```

```lua
local ui = require('haylen.ui')

ui.setTheme(ui.loadTheme('themes/wood.json', 'light'))
ui.mount(ui.panel{
    align = 'center',
    ui.pageHeader{title = 'Workshop'},
    ui.progress{value = 0.5},
    ui.button{text = 'Craft'},
})
```

## Styles

A style overrides the theme for one node and every node inside it, so any value a component reads, every color, metric, font and surface, changes for one instance without a theme of its own. It takes the sections of a [theme file](#themes): `colors`, `metrics`, `fonts`, whose roles set only the keys they list, and `surfaces`, whose images load in the background the first time they draw, while the flat colors stand in. A surface set to `false` paints with flat colors whatever the theme has. Values a style leaves out come from the style or theme around the node, so styles nest.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.button{text = 'A plain button'},
    ui.row{gap = 12, style = {colors = {accent = '#FF2E9E62', accentHover = '#FF3DBE7A'}, metrics = {controlHeight = 80, controlRadius = 40}, fonts = {button = {size = 36, bold = true}}},
        ui.button{text = 'Rounder and greener', variant = 'primary'},
        ui.toggle{text = 'Green too', checked = true},
    },
    ui.panel{theme = 'light', ui.label{text = 'A light panel in a dark screen'}},
    ui.button{text = 'Wooden', style = {surfaces = {button = {image = 'ui/wood.png', slice = 12, filter = 'linear'}}}},
})
```

A style raises the errors of its values, each starting with the property that holds it: `The property "style" of a "<kind>" must be a table with "colors", "metrics", "fonts" or "surfaces".`, `Unknown key "<key>" in the property "style" of a "<kind>".`, `The property "style" of a "<kind>" must hold a table in "<section>".`, `The property "style" of a "<kind>" has no color role named "<name>".`, the same for metrics, font roles and surfaces, `The color "<name>" of the property "style" of a "<kind>" must be a color such as "#FF2E7D32".`, `The metric "<name>" of the property "style" of a "<kind>" must be a non-negative number.`, `The font "<name>" of the property "style" of a "<kind>" must be a table.`, `... must name its font with a string.`, `... must set "bold" to "true" or "false".`, `The size of the font "<name>" of the property "style" of a "<kind>" must be positive.` and `The image of the surface "<name>" of the property "style" of a "<kind>" must be a path.`. The other values of a surface image are checked once its image has loaded, with the errors of theme surfaces.

## Theme colors

Colors are `'#RRGGBB'` or `'#AARRGGBB'` strings. The four tones `success`, `warning`, `danger` and `information`, like the accent, come as a fill, the ink written on that fill, a subtle background and a text color readable on the window.

| Role | Dark | Light | Used for |
| --- | --- | --- | --- |
| `window` | `#FF1B1E2B` | `#FFF4F5F9` | Background of ImGui windows and the outline of progress text. |
| `panel` | `#FF232739` | `#FFFFFFFF` | Fill of panels and dialogs. |
| `raised` | `#FF2C3147` | `#FFFFFFFF` | Fill of cards, buttons, fields and chips. |
| `tooltip` | `#F20F111A` | `#F21B1E2B` | Background of tooltips and toasts. |
| `overlay` | `#A0000000` | `#66000000` | The backdrop behind dialogs and the base of touch controls. |
| `shadow` | `#66000000` | `#291B1E2B` | The soft shadow under dialogs, windows, toasts, tooltips and the popups of combos, menus, popovers and color fields. |
| `hover` | `#14FFFFFF` | `#0F000000` | Layer over hovered controls and rows. |
| `pressed` | `#24FFFFFF` | `#1F000000` | Layer over pressed controls. |
| `selection` | `#404C7DFF` | `#334C7DFF` | Selected rows, checked buttons and selected text. |
| `focus` | `#FF7AA2FF` | `#FF3A66E0` | Border of focused fields and the focus ring of the navigation target. |
| `caret` | `#FF7AA2FF` | `#FF3A66E0` | The text cursor of fields. |
| `border` | `#FF3A4058` | `#FFD6D9E4` | Borders and dividers. |
| `borderStrong` | `#FF525A7A` | `#FFB3B8CC` | Strong borders, hovered fields and empty slider tracks. |
| `scrollbar` | `#FF3A4058` | `#FFC9CDDB` | Scrollbar grab. |
| `scrollbarHover` | `#FF525A7A` | `#FFA9AEC2` | Hovered scrollbar grab. |
| `track` | `#FF3A4058` | `#FFCDD1DD` | Track of a switch that is off. |
| `knob` | `#FFFFFFFF` | `#FFFFFFFF` | Knob of switches. |
| `text` | `#FFE8EAF2` | `#FF1B1E2B` | Main text. |
| `textMuted` | `#FFA3A8BF` | `#FF5C6380` | Captions, placeholders, help lines and inactive tabs. |
| `textDisabled` | `#FF6A7090` | `#FFA3A8BF` | Text of disabled ImGui items. |
| `onTooltip` | `#FFE8EAF2` | `#FFF4F5F9` | Text of tooltips and toasts. |
| `accent` | `#FF4C7DFF` | `#FF3A66E0` | Accent fill of primary buttons, checked boxes, slider fills and the selected tab. |
| `accentHover` | `#FF6690FF` | `#FF4C7DFF` | Hovered accent fill and hovered links. |
| `accentStrong` | `#FF3A66E0` | `#FF2C52C0` | Pressed accent fill. |
| `onAccent` | `#FFFFFFFF` | `#FFFFFFFF` | Text and marks on the accent fill. |
| `accentBackground` | `#264C7DFF` | `#1F3A66E0` | Subtle accent background of selected chips and avatars. |
| `accentText` | `#FF8FB0FF` | `#FF2C52C0` | Accent text such as links. |
| `success` | `#FF3DBE7A` | `#FF2E9E62` | Fill of the success tone. |
| `onSuccess` | `#FFFFFFFF` | `#FFFFFFFF` | Ink on the success fill. |
| `successBackground` | `#263DBE7A` | `#1F2E9E62` | Subtle background of the success tone. |
| `successText` | `#FF6FDCA0` | `#FF1F7A49` | Text of the success tone. |
| `warning` | `#FFF2B23A` | `#FFD9941C` | Fill of the warning tone. |
| `onWarning` | `#FF1B1E2B` | `#FFFFFFFF` | Ink on the warning fill. |
| `warningBackground` | `#26F2B23A` | `#1FD9941C` | Subtle background of the warning tone. |
| `warningText` | `#FFF7CB70` | `#FF9A6508` | Text of the warning tone. |
| `danger` | `#FFE5534B` | `#FFD0433B` | Fill of the danger tone and destructive buttons. |
| `dangerHover` | `#FFEE6B64` | `#FFE5534B` | Hovered destructive buttons. |
| `dangerStrong` | `#FFC8423B` | `#FFB0352E` | Pressed destructive buttons. |
| `onDanger` | `#FFFFFFFF` | `#FFFFFFFF` | Ink on the danger fill. |
| `dangerBackground` | `#26E5534B` | `#1FD0433B` | Subtle background of the danger tone. |
| `dangerText` | `#FFFF8A84` | `#FFA8322B` | Text of the danger tone and form errors. |
| `information` | `#FF3AA8E0` | `#FF2A8CC0` | Fill of the information tone. |
| `onInformation` | `#FFFFFFFF` | `#FFFFFFFF` | Ink on the information fill. |
| `informationBackground` | `#263AA8E0` | `#1F2A8CC0` | Subtle background of the information tone. |
| `informationText` | `#FF7FCBF2` | `#FF1D6A93` | Text of the information tone. |

## Theme metrics

Both built-in themes share these metrics, in design units.

| Metric | Default | Used for |
| --- | --- | --- |
| `controlHeight` | `64` | Height of buttons, fields, combos, sliders and the tab strip. |
| `controlRadius` | `12` | Corner radius of controls, panels and windows. |
| `controlPaddingX` | `24` | Horizontal padding inside buttons, fields, alerts and toasts. |
| `controlPaddingY` | `12` | Vertical padding of settings rows, alerts, toasts and text areas. |
| `itemSpacing` | `16` | Default gap of columns, rows and grids and the space between parts of a component. |
| `panelPadding` | `28` | Default padding of cards, panels and dialogs and the margin of toasts. |
| `borderWidth` | `2` | Width of borders and dividers. |
| `focusWidth` | `3` | Thickness of the focus ring around the navigation target and of the line under the selected tab. |
| `scrollbarSize` | `16` | Width of scrollbars. |
| `iconSize` | `36` | Size of button icons, row pictures and icons without a size, and the indent of tree levels. |
| `choiceSize` | `36` | Size of check boxes and radio marks. |
| `sliderTrackHeight` | `10` | Height of the slider track. |
| `sliderKnobSize` | `34` | Size of the slider knob. |
| `toggleWidth` | `72` | Width of the toggle switch. |
| `toggleHeight` | `38` | Height of the toggle switch. |
| `progressHeight` | `22` | Smallest height of progress bars. |
| `badgePaddingX` | `14` | Horizontal padding of badges. |
| `badgePaddingY` | `4` | Vertical padding of badges. |
| `tabPaddingX` | `24` | Horizontal padding of each tab. |
| `listRowHeight` | `64` | Height of list, tree and table rows and the smallest height of settings rows. |
| `dialogWidth` | `760` | Width of dialogs. |
| `toastWidth` | `560` | Width of toasts. |
| `tooltipWidth` | `520` | Width at which tooltips wrap. |
| `settingsLabelWidth` | `420` | Width of the label column of settings rows. |
| `caretWidth` | `2` | Width of the text cursor in fields and [`haylen.imgui`](imgui.md) inputs. |
| `circularProgressSize` | `72` | Size of circular progress indicators without a size. |
| `circularProgressThickness` | `8` | Width of the ring of circular progress indicators without a thickness. |
| `slotSize` | `96` | Size of the slots of slot grids without a slot size. |
| `windowTitleHeight` | `56` | Height of the title bar of windows. |
| `pageIndicatorSize` | `14` | Size of the page dots of carousels. |
| `toggleKnobInset` | `4` | Space between the knob of a switch and the edge of its track. |
| `transitionDuration` | `0.15` | Seconds the transitions of the UI take: the knob of a switch, the fades of dialogs and their backdrops, and the fades and moves of toasts. |
| `toastLimit` | `3` | Notices one stack of toasts shows at once, while the others wait. The value `0` shows them all. |
| `toneBarWidth` | `6` | Width of the bar of the tone of toasts and alerts. |
| `panelRadius` | `16` | Corner radius of panels, cards, dialogs, windows, toasts and popups, which the rows of a popup and the title bar of a window follow inside it. |
| `contentSpacing` | `12` | Space between the box, the switch or the icon of a control and its text. |
| `focusGap` | `3` | Space between a control and its focus ring. |
| `disabledOpacity` | `0.5` | Opacity of disabled nodes. |
| `tooltipDelay` | `0.5` | Seconds the pointer rests on a node before its tooltip shows. |
| `longPressDuration` | `0.5` | Seconds a finger holds a context menu before it opens. |
| `shadowSize` | `24` | Reach of the soft shadow of floating surfaces. The value `0` turns shadows off. |
| `shadowOffset` | `6` | How far below its surface a shadow falls. |
| `chipHeight` | `48` | Height of chips. |
| `strokeWidth` | `2` | Width of the lines of drawn icons, such as the cross of a removable chip and the lens of a filter field. |
| `rowPadding` | `16` | Padding at the start and end of the rows of lists, trees, tables, popups and accordions. |
| `checkRadius` | `8` | Corner radius of check boxes. |
| `menuPadding` | `8` | Space between the edge of the popup of a combo, a menu button or a context menu and its rows. |
| `splitterSize` | `10` | Thickness of the handle of splitters. |
| `cellRadius` | `12` | Corner radius of the backgrounds of interactive collection cells and of the focus ring around them. |
| `refreshDistance` | `120` | How far a pull at the start of a refreshable collection goes before it refreshes. |

## Theme fonts

Every role starts with the built-in font `default` in its regular style. A role that sets `bold` or `italic` draws with that face of its font when the font is a family that has it, and with its regular face otherwise, and `ui.richText` of the role starts in that style as if inside `[b]` or `[i]`. The size of a role is the height of its regular face from ascent to descent, and `ui.richText` draws at the em size that makes it match the other components.

| Role | Size | Used for |
| --- | --- | --- |
| `body` | `30` | Labels, fields, list rows and most text. |
| `caption` | `24` | Captions, badges, chips, tooltips and form field labels. |
| `button` | `30` | Button and tab labels, alert titles and avatar initials. |
| `heading` | `38` | Section titles, dialog titles and empty state titles. |
| `title` | `56` | Page header titles. |
| `monospace` | `26` | The color field text. |

A font role with an empty font name or a size of zero raises `A theme font needs a font name and a positive size.`, and a font name that is neither registered nor listed in `fontFiles` makes `ui.loadTheme` fail.

## Theme surfaces

A surface paints a part of a component with a nine-slice image instead of flat colors, such as the wooden panels and ribbons of a textured game theme. The corners keep their size times `scale`, the edges and the center stretch or tile, and `padding` moves the content away from thick borders. The built-in themes have no surfaces. A hover or pressed surface the theme leaves out falls back to the normal surface of the same button.

| Surface | Used by |
| --- | --- |
| `panel` | `panel`. |
| `card` | `card`. |
| `dialog` | `dialog`. |
| `tooltip` | Tooltips of every node. The text stays inside the `padding` of the surface. |
| `toast` | `toast`. |
| `banner` | `pageHeader` with `banner = true`. |
| `button`, `buttonHover`, `buttonPressed` | Default, toolbar and icon buttons, menu buttons, popovers, dialog buttons and the number field buttons. |
| `buttonPrimary`, `buttonPrimaryHover`, `buttonPrimaryPressed` | Primary buttons. |
| `buttonDestructive`, `buttonDestructiveHover`, `buttonDestructivePressed` | Destructive buttons. |
| `field`, `fieldFocused` | Text fields, secret fields, text areas, filter fields, number fields, combos, color fields, steppers and key captures. |
| `check`, `checkChecked` | Check boxes. |
| `track`, `trackFill`, `knob` | Toggles, sliders, range sliders and progress bars. The fill of toggles, sliders and progress bars stays inside the `padding` of the `track` surface, so a framed bar keeps its frame, and a toggle fills its groove as its knob slides on. |
| `stickBase`, `stickKnob` | The ring and the knob of touch sticks. |
| `touchButton`, `touchButtonPressed` | Touch buttons. |
| `tab`, `tabSelected` | Tabs. |
| `chip`, `chipSelected` | Chips. |
| `badge` | Badges. |
| `segment`, `segmentSelected` | The strip of segmented controls and their selected segment. |
| `menu` | The popups of combos, menu buttons, context menus, popovers and color fields. |
| `window` | Windows. |
| `slot`, `slotHighlighted` | Slots of slot grids, and the selected or hovered slot. |
| `cell`, `cellHover`, `cellPressed`, `cellSelected` | The backgrounds of interactive collection cells: always, while hovered, while pressed and while selected. Without images the state surfaces paint the `hover`, `pressed` and `selection` colors, and `cell` paints nothing. |

A surface image is an object with these keys.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | string | required | Texture path relative to the package `content/` folder. |
| `source` | `[x, y, width, height]` | whole texture | Region of the texture that holds the frame. |
| `slice` | insets | `0` | Border sizes cut from `source`, as one, two or four numbers like padding. |
| `pieces` | list of nine `[x, y, width, height]` | none | The nine regions given one by one instead of `source` and `slice`, in reading order: top left, top, top right, left, center, right, bottom left, bottom and bottom right. |
| `scale` | number | `1` | Scale of the borders on screen. |
| `padding` | insets | `0` | Space between the edge of the image and the content. |
| `tint` | color | `'#FFFFFFFF'` | Color multiplied with the image. |
| `colorize` | boolean | `false` | Also multiplies the image with the color the component would fill the area with, such as the tone of a progress bar or the accent of a slider, so one light image serves every color. |
| `filter` | string | `'nearest'` | Texture filter, `'nearest'` or `'linear'`. |
| `fill` | string | `'stretch'` | Either `'stretch'` or `'tile'` for the edges and the center. |

A surface image raises `The image of the theme surface "<name>" must be a path.`, `The filter of the theme surface "<name>" must be "nearest" or "linear".`, `The fill of the theme surface "<name>" must be "stretch" or "tile".`, `The "colorize" of the theme surface "<name>" must be "true" or "false".`, `The pieces of the theme surface "<name>" must hold nine rectangles.`, `The source of the theme surface "<name>" must be four numbers: x, y, width and height.` or `The tiled edges and center of the theme surface "<name>" must each be at least 1 unit wide and tall at its scale.` for invalid values, and the `scale`, `padding`, `slice` and `tint` raise the same errors as theme metrics and colors.

```json
{
    "name": "parchment",
    "surfaces": {
        "dialog": {"image": "ui/parchment.png", "source": [0, 0, 96, 96], "slice": [24, 20], "scale": 2, "padding": 12},
        "badge": {"image": "ui/badge.png", "pieces": [[0, 0, 4, 4], [4, 0, 4, 4], [8, 0, 4, 4], [0, 4, 4, 4], [4, 4, 4, 4], [8, 4, 4, 4], [0, 8, 4, 4], [4, 8, 4, 4], [8, 8, 4, 4]], "tint": "#FFFFE0B0"},
        "card": null
    }
}
```

```lua
local ui = require('haylen.ui')

ui.setTheme(ui.loadTheme('themes/parchment.json'))
```
