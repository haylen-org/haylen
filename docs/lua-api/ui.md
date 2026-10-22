# 📖 `haylen.ui`

The module `haylen.ui` builds the interface that players see, such as menus, HUDs, settings screens, dialogs and touch controls. You describe a tree of themed components, mount it once and then change its nodes by id. This page lists every component kind with its properties and events, and every function, type and error of the module. The [UI guide](../ui/ui.md) teaches how to build a GUI step by step. For debug windows that you rebuild every frame, use [`haylen.imgui`](imgui.md) instead.

```lua
local ui = require('haylen.ui')
```

A GUI is a tree of nodes. A node is a flat table with a `kind`, an optional `id`, its children and the properties of its kind. The function [`ui.mount`](#uimounttree-options) builds the tree, draws it over the app every frame and returns a [`Gui`](#gui). Sizes and positions are design units, the units of the design resolution in `app.json`. The metrics of the built-in themes suit the default design resolution of 1920 by 1080.

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

## Component kinds

Every kind is also a function of the module, so `ui.button{text = 'Play'}` builds a button node. The function [`ui.kinds()`](#uikinds) lists the 64 kinds. Every kind takes the [common properties](#common-properties) besides its own. Every kind reports [the events of every kind](#the-events-of-every-kind), and its section lists the events it adds.

| Kind | Group | What it is |
| --- | --- | --- |
| [`accordion`](#uiaccordionproperties) | [Containers](#containers) | Sections under headers that open and close. |
| [`alert`](#uialertproperties) | [Text](#text) | A message box with a colored bar on its start edge. |
| [`avatar`](#uiavatarproperties) | [Indicators](#indicators) | A round picture of a player, or the initials of a name. |
| [`badge`](#uibadgeproperties) | [Indicators](#indicators) | A small pill with short text, such as a count. |
| [`busyIndicator`](#uibusyindicatorproperties) | [Indicators](#indicators) | A spinning ring that shows work in progress. |
| [`button`](#uibuttonproperties) | [Buttons](#buttons) | A button that reports `click`, in six looks. |
| [`card`](#uicardproperties) | [Containers](#containers) | A column on a raised surface with a border. |
| [`carousel`](#uicarouselproperties) | [Containers](#containers) | Pages shown one at a time that slide sideways. |
| [`checkbox`](#uicheckboxproperties) | [Choices](#choices) | A box with a check mark and a label. |
| [`chip`](#uichipproperties) | [Buttons](#buttons) | A small rounded tag that toggles, and can be removable. |
| [`circularProgress`](#uicircularprogressproperties) | [Indicators](#indicators) | A ring that fills, or the cooldown shade over a picture. |
| [`collection`](#uicollectionproperties) | [Lists and grids](#lists-and-grids) | Any number of items of several types, with recycled cells. |
| [`colorField`](#uicolorfieldproperties) | [Inputs](#inputs) | A color swatch that opens a color picker. |
| [`column`](#uicolumnproperties) | [Containers](#containers) | Children from top to bottom. |
| [`combo`](#uicomboproperties) | [Choices](#choices) | A field that opens a list of choices. |
| [`contextMenu`](#uicontextmenuproperties) | [Overlays](#overlays) | A menu that opens over its child on a right click or a long press. |
| [`dialog`](#uidialogproperties) | [Overlays](#overlays) | A modal window with a message and answer buttons. |
| [`divider`](#uidividerproperties) | [Containers](#containers) | A thin line between parts of a layout. |
| [`emptyState`](#uiemptystateproperties) | [Text](#text) | A placeholder for a view with nothing to show yet. |
| [`filterField`](#uifilterfieldproperties) | [Inputs](#inputs) | A search field with a clear button. |
| [`formField`](#uiformfieldproperties) | [Containers](#containers) | A label above a control, with a help or error line. |
| [`grid`](#uigridproperties) | [Containers](#containers) | Children in cells of equal width. |
| [`icon`](#uiiconproperties) | [Indicators](#indicators) | A small square picture that a theme color can tint. |
| [`image`](#uiimageproperties) | [Indicators](#indicators) | A picture that fits its bounds. |
| [`imageButton`](#uiimagebuttonproperties) | [Buttons](#buttons) | A picture that works as a button. |
| [`keyCapture`](#uikeycaptureproperties) | [Inputs](#inputs) | A field that records the next key or button as a binding. |
| [`label`](#uilabelproperties) | [Text](#text) | Text in one of the theme fonts. |
| [`list`](#uilistproperties) | [Lists and grids](#lists-and-grids) | Rows of items, which the player can also drag. |
| [`menuButton`](#uimenubuttonproperties) | [Buttons](#buttons) | A button that opens a menu. |
| [`numberField`](#uinumberfieldproperties) | [Inputs](#inputs) | A number with minus and plus buttons. |
| [`pageHeader`](#uipageheaderproperties) | [Text](#text) | A page title with a caption, on a banner if you want. |
| [`panel`](#uipanelproperties) | [Containers](#containers) | A column on the panel color, without a border. |
| [`playArea`](#uiplayareaproperties) | [Play areas](#play-areas) | The part of a screen where the game shows and takes the focus. |
| [`popover`](#uipopoverproperties) | [Buttons](#buttons) | A button that opens a floating panel. |
| [`progress`](#uiprogressproperties) | [Indicators](#indicators) | A bar filled up to a value. |
| [`radioGroup`](#uiradiogroupproperties) | [Choices](#choices) | Options of which the player picks one. |
| [`rangeSlider`](#uirangesliderproperties) | [Inputs](#inputs) | A track with two knobs that pick a range. |
| [`richText`](#uirichtextproperties) | [Text](#text) | Text in markup, with links, effects and a typewriter reveal. |
| [`row`](#uirowproperties) | [Containers](#containers) | Children from left to right, in lines that can wrap. |
| [`safeArea`](#uisafeareaproperties) | [Containers](#containers) | Keeps its child inside the safe area of the screen. |
| [`scroll`](#uiscrollproperties) | [Containers](#containers) | One child in an area that scrolls. |
| [`secretField`](#uisecretfieldproperties) | [Inputs](#inputs) | A text field that hides what the player types. |
| [`sectionTitle`](#uisectiontitleproperties) | [Text](#text) | A heading that starts a section. |
| [`segmentedControl`](#uisegmentedcontrolproperties) | [Choices](#choices) | Joined segments of which one is selected. |
| [`settingsActions`](#uisettingsactionsproperties) | [Settings](#settings) | A row of buttons at the end of a form. |
| [`settingsForm`](#uisettingsformproperties) | [Settings](#settings) | A column of settings rows and section titles. |
| [`settingsRow`](#uisettingsrowproperties) | [Settings](#settings) | A setting with its label on one side and its control on the other. |
| [`slider`](#uisliderproperties) | [Inputs](#inputs) | A track with a knob that picks a number. |
| [`slotGrid`](#uislotgridproperties) | [Lists and grids](#lists-and-grids) | Square slots of an inventory, a chest or a hotbar. |
| [`spacer`](#uispacerproperties) | [Containers](#containers) | Empty room that pushes other nodes away. |
| [`splitter`](#uisplitterproperties) | [Containers](#containers) | Two children with a handle between them. |
| [`stack`](#uistackproperties) | [Containers](#containers) | Children on top of each other. |
| [`statusIndicator`](#uistatusindicatorproperties) | [Indicators](#indicators) | A colored dot with optional text. |
| [`stepper`](#uistepperproperties) | [Inputs](#inputs) | A value between two arrows that step it. |
| [`table`](#uitableproperties) | [Lists and grids](#lists-and-grids) | Rows of cells under a header. |
| [`tabs`](#uitabsproperties) | [Containers](#containers) | A strip of tabs over the child of the selected tab. |
| [`textArea`](#uitextareaproperties) | [Inputs](#inputs) | A text entry of several lines. |
| [`textField`](#uitextfieldproperties) | [Inputs](#inputs) | A text entry of one line. |
| [`toast`](#uitoastproperties) | [Overlays](#overlays) | Short notices at an edge of the screen. |
| [`toggle`](#uitoggleproperties) | [Choices](#choices) | A switch with a label. |
| [`touchButton`](#uitouchbuttonproperties) | [Touch controls](#touch-controls) | An on-screen button that drives a virtual button of the action map. |
| [`touchStick`](#uitouchstickproperties) | [Touch controls](#touch-controls) | An on-screen stick that drives a virtual stick of the action map. |
| [`tree`](#uitreeproperties) | [Lists and grids](#lists-and-grids) | Nested items that open and close. |
| [`window`](#uiwindowproperties) | [Overlays](#overlays) | A floating panel that the player drags by its title bar. |

## Containers

Containers hold other nodes and place them. The [layout](#layout) section lists the properties every node takes, such as `grow`, `align` and `margin`.

### ui.column(properties)

A column places its children from top to bottom, and it holds any number of them. Children with `grow` share the free height in proportion to their `grow`, each within its `minHeight` and `maxHeight`. The property `justify` places the children in the height they leave. Each child sits across the width by its own `align`, or by the `alignItems` of the column when it sets none. A stretched child stays within its `minWidth` and `maxWidth`. The `margin` of a child keeps space around it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `gap` | number from 0 to 10000 | theme `itemSpacing` | The space between children. |
| `padding` | insets | `0` | The space between the edges and the children. |
| `justify` | string | `'start'` | Where the free space goes. The values `'start'`, `'center'` and `'end'` keep the children together at that place. The value `'spaceBetween'` puts the space between the children, `'spaceAround'` gives every child an equal share on both sides, and `'spaceEvenly'` makes every space equal, the ends included. Children start at the nearest whole unit. |
| `alignItems` | string | none | The `align` of every child that sets none: `'start'`, `'center'`, `'end'` or `'stretch'`. Without it, each kind keeps its own default. |

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

A row places its children from left to right, or from right to left in a [right-to-left](#right-to-left-interfaces) node. It takes the properties of `column`. Children with `grow` share the width that the others leave, so wide content never pushes the row past its bounds. Each child sits across the height by its `align`, which is `'center'` by default in a row.

With `wrap = true`, the row breaks its children into lines. Each line takes as many children as fit. A child that grows counts at its `minWidth`, and every other child at its natural width. A child wider than the row takes a line of its own. Every line is at least as tall as its tallest child. A line places its children by `justify` and shares its free width among its children that grow. So a child that grows moves to a new line only when its `minWidth` does not fit. The lines share the height the row has beyond their own, so children with `align = 'stretch'` fill it. Lines taller than the row are cut at its edges.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `wrap` | boolean | `false` | Breaks the children into lines. |
| `lineGap` | number from 0 to 10000 | `gap` | The space between lines. |

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

local tags = {}
for index, name in ipairs({'Wood', 'Stone', 'Rope', 'Fish', 'Berries', 'Shells'}) do
    tags[index] = ui.chip{text = name}
end
ui.mount(ui.row{wrap = true, lineGap = 8, width = 500, children = tags})
```

### ui.grid(properties)

A grid places its children in cells of equal width, and it holds any number of them. It fills each row before it starts the next one. Every row is as tall as its tallest child. Each child sits inside its cell by its `align` in both directions, or by the `alignItems` of the grid when it sets none. A stretched child fills its cell within its size bounds.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | integer from 1 to 64 | `2` | The number of cells in each row. With `minColumnWidth`, it is the most cells a row holds. |
| `minColumnWidth` | number from 0 to 10000 | `0` | Above zero, each row holds as many cells as fit at this width or wider. So the number of columns follows the width of the grid. |
| `gap` | number from 0 to 10000 | theme `itemSpacing` | The space between cells. |
| `rowGap` | number from 0 to 10000 | `gap` | The space between rows. |
| `padding` | insets | `0` | The space between the edges and the cells. |
| `alignItems` | string | none | The `align` of every child that sets none. |

```lua
local ui = require('haylen.ui')

local slots = {}
for index = 1, 8 do
    slots[index] = ui.card{ui.label{text = 'Slot ' .. index}}
end
ui.mount(ui.grid{columns = 4, gap = 8, padding = 16, children = slots})
ui.mount(ui.grid{minColumnWidth = 240, columns = 6, children = {ui.card{ui.label{text = 'Map'}}, ui.card{ui.label{text = 'Log'}}}})
```

### ui.stack(properties)

A stack draws its children on top of each other, and later children draw above earlier ones. It holds any number of children. Each child sits by its own `align` in both directions, or by the `alignItems` of the stack when it sets none. The value `'stretch'` fills the whole stack within the size bounds of the child.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `padding` | insets | `0` | The space between the edges and the children. |
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

A scroll shows one child in an area that scrolls up and down, or sideways. The mouse wheel, the trackpad and the scroll bar scroll it, and on touch screens a finger drags the content. A drag along the scrolling direction takes the finger from the control it started on, so a list of buttons still scrolls. When the finger lets go, the content flings on for a while. Past an end, the content stretches and then springs back. A gesture across the axis goes to the area around the scroll, as the [UI guide](../ui/ui.md#scrolling-with-wheels-and-trackpads) explains.

The focus scrolls to the focused control. The command [`scrollTo`](#guicommandid-name-arguments) scrolls to an offset or to a node inside the scroll. A vertical scroll measures as tall as its content, so give it a `height` or a `maxHeight` to make it scroll. A horizontal scroll needs a `width`. While the child is longer than the area, the [scroll bar](#scroll-bars) takes a lane of its own beside it, and a horizontal scroll adds that lane to its height. The content draws at the place of the scroll in the tree. So the nodes after it, such as an anchored sheet, cover it, and so do the GUIs of higher layers.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `scrollbar` | boolean | `true` | Shows the scroll bar while the child is longer than the area. Without it, the child takes the whole area. |
| `axis` | string | `'vertical'` | The value `'vertical'` or `'horizontal'`. A horizontal scroll lays the child out at its full width and scrolls it sideways, with a sideways trackpad gesture, Shift and the mouse wheel, or a finger. |
| `snap` | boolean | `false` | Once the player lets go, settles on the start of the item of the child nearest to the scrolled place, such as a card of a row. |
| `fill` | boolean | `false` | Stretches a child shorter than the area to the whole length of the area. So the children of a column inside it that grow share the room, like a page that fills the screen and scrolls once its content is longer. |

Across the other axis, the child stretches over the scroll within its size bounds, so a column with a `maxWidth` keeps that width on a wide screen. A child that sets its `align`, such as `'center'`, sits there instead. Some kinds fill the width they get elsewhere: `list`, `tree`, `table`, `accordion`, `carousel` and `splitter`. Inside a horizontal scroll they take the width of their content, and grids and splitters give each child its own width. An axis other than `'vertical'` or `'horizontal'` raises `The property "axis" of a "scroll" must be "vertical" or "horizontal".`.

```lua
local ui = require('haylen.ui')

local lines = {}
for index = 1, 40 do
    lines[index] = ui.label{text = 'Quest log entry ' .. index}
end
local log = ui.mount(ui.scroll{id = 'log', height = 600, ui.column{gap = 8, children = lines}})
lines[#lines + 1] = ui.label{id = 'latest', text = 'Quest complete'}
log:replaceChildren('log', {ui.column{gap = 8, children = lines}})
log:command('log', 'scrollTo', {node = 'latest', align = 'end'})
log:command('log', 'scrollTo', {offset = 0, animated = false})

local cards = {}
for index = 1, 8 do
    cards[index] = ui.card{width = 360, ui.label{text = 'Level ' .. index}}
end
ui.mount(ui.scroll{axis = 'horizontal', snap = true, width = 1200, height = 240, align = 'end', ui.row{gap = 24, children = cards}})
```

### ui.card(properties)

A card is a column on a raised surface with a border. The theme `card` surface draws it when the theme has one. It takes the properties of `column`. When its `padding` is zero, the theme `panelPadding` metric pads the content. The `padding` of a theme surface image always adds on top, so content clears thick frame borders. A card keeps the pointer from reaching the app behind it. It cuts its children at its edges, so tall content never draws over what lies around it. Put content that may run long in a `scroll`, so the player can reach all of it.

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

A panel is a column on the `panel` color, without a border. The theme `panel` surface draws it when the theme has one. It takes the properties of `column` and pads its content like a card. Like a card, it keeps the pointer from reaching the app and cuts its children at its edges.

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

A spacer takes room and draws nothing. It has no properties of its own, so it uses `width`, `height` or `grow`. A spacer with `grow = 1` pushes the nodes after it to the far end of a row or a column.

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.label{text = 'Score'},
    ui.spacer{grow = 1},
    ui.label{text = '1200'},
})
```

### ui.divider(properties)

A divider draws a line through the middle of its bounds, as thick as the theme `borderWidth` metric. A vertical divider in a row needs a `height` or `align = 'stretch'`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `vertical` | boolean | `false` | Draws a vertical line instead of a horizontal one. |
| `color` | theme color | `'border'` | The color of the line. |

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

Tabs show a strip of tabs over the child of the selected tab. Give the node one child per item, in the order of the items. Only the child of the selected tab shows. Picking another tab reports `select`, with the item id as `item`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The tabs. Items use `id`, `text` and `enabled`. |
| `selected` | string | first item | The id of the selected tab. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.tabs{
    id = 'journal',
    items = {{id = 'quests', text = 'Quests'}, {id = 'map', text = 'Map'}},
    selected = 'quests',
    onSelect = function(event)
        print('Switched ' .. event.id .. ' to ' .. event.item .. '.')
    end,
    ui.label{text = 'Find the lighthouse'},
    ui.image{image = 'ui/map.png'},
})
```

### ui.accordion(properties)

An accordion shows sections under headers that open and close. Give the node one child per item, in the order of the items. A click, a tap or accept on a header opens or closes its section. Opening a section closes the others, unless `multiple` is set. The headers take the focus.

Each change reports `toggle`, with the item id as `item` and the new state as `expanded`. When opening a section closes others, each closed section first reports `toggle` with `expanded` set to `false`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The sections. Items use `id`, `text` and `enabled`. |
| `expanded` | list of strings | none | The ids of the open sections. The player opens and closes sections afterwards, and setting it again replaces the open sections. |
| `multiple` | boolean | `false` | Lets several sections stay open. |

A value of `expanded` that is not a list of strings raises `The property "expanded" of an "accordion" must be a list of item ids.`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.accordion{
    width = 800,
    items = {{id = 'controls', text = 'Controls'}, {id = 'goals', text = 'Goals'}},
    expanded = {'controls'},
    onToggle = function(event)
        print(event.item .. ' is now ' .. (event.expanded and 'open' or 'closed') .. '.')
    end,
    ui.label{text = 'Move with the left stick and chop with the south button.'},
    ui.label{text = 'Survive ten nights on the island.'},
})
```

### ui.carousel(properties)

A carousel shows pages one at a time that slide sideways, such as a tutorial or a level picker. Each child is one page. A swipe or a drag of the pages changes the page, and so do the arrow buttons and the page dots. The arrows draw above pages that fill the carousel. The page dots take the focus, and left and right turn the pages from there. The controls of the other pages stay out of navigation. A new page reports `change`, with the page number from 1 as `page`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `page` | integer from 1 | `1` | The page that shows. |
| `loop` | boolean | `false` | Goes on from the last page to the first, and back. |
| `indicators` | boolean | `true` | Shows the page dots under the pages. Without them, the whole carousel takes the focus. |
| `arrows` | boolean | `true` | Shows the arrow buttons on the sides. |
| `interval` | number from 0 to 3600 | `0` | The seconds before the next page turns on its own, looping. The value `0` turns pages only when the player does. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.carousel{
    width = 1000,
    height = 520,
    align = 'center',
    interval = 6,
    onChange = function(event)
        print('Page ' .. event.page .. '.')
    end,
    ui.image{image = 'tutorial/move.png'},
    ui.image{image = 'tutorial/chop.png'},
    ui.image{image = 'tutorial/build.png'},
})
```

### ui.formField(properties)

A form field shows a label above one child control, and a help line or an error line under it. The error replaces the help and uses the `dangerText` color.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `label` | text | none | The text above the control. |
| `help` | text | none | A hint under the control. |
| `error` | text | none | An error under the control, shown instead of `help`. |
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

A splitter shows two children beside each other, or one above the other, with a handle between them. The player drags the handle. Without a `height`, a side-by-side splitter is as tall as its taller child at its share of the width. A stacked splitter is as tall as both children. Releasing the handle reports `resize`, with the new `ratio`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ratio` | number from 0.05 to 0.95 | `0.5` | The share of the space the first child takes. |
| `vertical` | boolean | `false` | Stacks the children instead of placing them side by side. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.splitter{
    ratio = 0.3,
    onResize = function(event)
        print('Split at ' .. event.ratio .. '.')
    end,
    ui.list{items = {{id = 'a', text = 'Alpha'}, {id = 'b', text = 'Beta'}}},
    ui.label{text = 'Details'},
})
```

### ui.safeArea(properties)

A safe area keeps its one child inside the safe area of the screen, the part that notches and system bars leave free. The child fills the part of the safe area that the node covers. Use it in a GUI mounted with `placement = 'screen'`, which paints a background to the edges but holds controls that a notch must not hide. It has no properties of its own.

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/title.png', fit = 'cover', align = 'stretch'},
    ui.safeArea{ui.column{justify = 'end', ui.button{text = 'Start', align = 'end'}}},
}, {placement = 'screen'})
```

## Text

Text kinds show words in the fonts of the theme. Every text property takes a [text value](#value-types): a string, a number or a translation.

### ui.label(properties)

A label shows text in one of the theme fonts.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The text. |
| `font` | string | `'body'` | The font role: `'body'`, `'caption'`, `'button'`, `'heading'`, `'title'` or `'monospace'`. |
| `color` | theme color | `'text'` | The color of the text. |
| `textAlign` | string | `'start'` | The value `'start'`, `'center'` or `'end'` inside the bounds of the label. Start is the left of a left-to-right UI and the right of a right-to-left one. |
| `wrap` | boolean | `true` | Wraps long text onto more lines. Without wrapping, every line of the text stays on its own line, each one ended by a line break. A line that does not fit ends with an ellipsis, and the block of lines sits in the middle of the height of the label. |
| `outline` | color | none | Draws an outline of this color around the letters. |
| `outlineWidth` | number from 0 to 16 | `2` | The thickness of the outline. |
| `minScale` | number from 0.25 to 1 | `1` | A text that does not fit draws smaller, down to this share of its font size. So a long translation still shows whole in a fixed space. Wrapped text shrinks until its lines fit the height of the label. Text without wrapping shrinks until it fits the width, and ends with an ellipsis when it still does not fit. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.label{text = 'Chapter 1', font = 'title', color = 'accentText'},
    ui.label{text = {key = 'story.intro'}, textAlign = 'center'},
    ui.label{text = 'A very long hint that stays on one line', wrap = false, width = 300},
    ui.label{text = 'A title that shrinks to fit', wrap = false, width = 200, minScale = 0.6},
    ui.label{text = 'A long story that shrinks until all of its lines fit the box', width = 300, height = 90, minScale = 0.5},
    ui.label{text = 'Over the sea', outline = '#FF000000', outlineWidth = 3},
})
```

### ui.richText(properties)

Rich text shows text written in the BBCode markup of the [text guide](../graphics/text.md#markup). The markup gives bold, italic, colors, outlines, shadows, glows, lists, rules, tables, inline images and icons, links, hints, animated effects and a typewriter reveal. The 2D renderer draws the text inside the clip of the UI, from the font family of its theme font role. So a font registered as a family with [`ui.addFont`](#uiaddfontname-source) gives it real bold and italic faces and fallback fonts.

The tags `[font=name]` name fonts registered with `ui.addFont`. The tags `[img=path]` load images through the UI, like `ui.image`. The tags `[icon=name]` show icons registered with `graphics2d.registerTextIcon`. Links are items that the pointer, the keyboard and gamepads focus and activate, and hints show as tooltips. The [transforms](#uitransform) of the node and of the nodes around it scale, fade and tint its text like the rest of the UI.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The markup. Broken markup raises the error of `graphics2d.newRichText`. |
| `font` | string | `'body'` | The font role: `'body'`, `'caption'`, `'button'`, `'heading'`, `'title'` or `'monospace'`. |
| `color` | theme color | `'text'` | The color of text without a `[color]` tag. |
| `textAlign` | string | `'start'` | The alignment of paragraphs without their own: `'start'`, `'end'`, `'left'`, `'center'`, `'right'` or `'fill'`. Start and end follow the direction of the UI, while left and right name fixed sides. Every paragraph reads in the direction of its first strong letter, unless its markup sets `[p dir]`. |
| `wrap` | boolean | `true` | Wraps the paragraphs at the width of the node. Without wrapping, the text keeps its natural width and aligns as one block. |
| `revealSpeed` | number | `0` | The characters per second of the typewriter reveal. The reveal starts again whenever the text or the theme changes. The value `0` shows everything at once. |
| `visibleCharacters` | integer | `-1` | Shows only the first characters, and the reveal goes on from there. The value `-1` shows everything. |

| Event | Values | When |
| --- | --- | --- |
| `link` | `link` | The player clicked, tapped or activated a link with the focus. The value `link` is the payload of `[url=payload]`, or the text of `[url]text[/url]`. |
| `linkHover` | `link`, `hovered` | The pointer entered a link, with `hovered` set to `true`, or left it, with `hovered` set to `false`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{padding = 40, gap = 16,
    ui.richText{id = 'intro', text = '[b]Welcome![/b] Read the [url=rules]rules[/url] or [url=play]start playing[/url].\n[ul]Collect [color=gold]wood[/color]\nKeep the [wave]fire[/wave] burning[/ul]', revealSpeed = 40, onLink = function(event)
        print('Open ' .. event.link .. '.')
    end, onLinkHover = function(event)
        print(event.link, event.hovered)
    end},
    ui.richText{text = '[center]Press [icon=confirm] to continue[/center]', font = 'caption', textAlign = 'center'},
})
```

### ui.pageHeader(properties)

A page header shows a page title in the title font, with an optional caption under it. With `banner`, it sits on the theme `banner` surface, such as a ribbon, or on the accent color.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | text | none | The title. |
| `caption` | text | none | The text under the title. |
| `banner` | boolean | `false` | Draws the banner behind the text and writes the text in the `onAccent` color. |
| `textAlign` | string | `'start'` | The value `'start'`, `'center'` or `'end'`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.pageHeader{title = 'Settings', caption = 'Tune the island', banner = true, textAlign = 'center'},
})
```

### ui.sectionTitle(properties)

A section title is a heading in the heading font that starts a section.

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

An empty state is a placeholder for a view with nothing to show yet. It shows an optional picture, a title and a message, all centered. Its lines of text wrap at the theme `readingWidth`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture above the title. |
| `imageSize` | number from 0 to 4096 | `128` | The width and height of the picture. |
| `title` | text | none | The title, in the heading font. |
| `message` | text | none | The message, in the `textMuted` color. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.emptyState{
    image = 'ui/empty-chest.png',
    title = 'Nothing here',
    message = 'Chop some trees to fill your inventory.',
})
```

### ui.alert(properties)

An alert is a message box with a colored bar on its start edge.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `tone` | tone | `'information'` | The colors of the bar, the background and the title. |
| `title` | text | none | The title, in the button font. |
| `message` | text | none | The message. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.alert{tone = 'warning', title = 'Careful', message = 'Night is coming.'})
```

## Buttons

Buttons answer a press. They all take the focus.

### ui.button(properties)

A button reports `click` when the player presses it. A label longer than the width of the button wraps onto more lines, and the button grows taller to hold them, such as in a narrow panel.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The label of the button. |
| `icon` | image | none | A picture before the label, as large as the theme `iconSize` metric. |
| `iconColor` | theme color or `false` | none | The tint of the icon, such as `'text'` for an SVG icon drawn with `currentColor` on a light button. Without it, toolbar and icon buttons tint the icon like their label, and the other variants keep its colors. The value `false` keeps its colors in every variant. Any other value raises `The property "iconColor" of a "button" must name a theme color such as "accent" or "textMuted", or be "false".`. |
| `iconPlacement` | string | `'start'` | Where the icon stands: `'start'` before the label, or `'top'` above it. With `'top'`, the button is as tall as the icon and one line of text, like the buttons of a tab bar on a phone. |
| `justify` | string | `'center'` | Where the icon and the label sit across the button: `'start'`, `'center'` or `'end'`. At the start or the end, they keep the theme `controlPaddingX` from the edge, like the content of a search bar. |
| `variant` | variant | `'default'` | The look of the button, from the table below. |
| `checked` | boolean | `false` | Marks the button as active with the `selection` color, which suits toolbar toggles. |

| Variant | Look |
| --- | --- |
| `'default'` | A raised fill with a border, or the theme `button` surfaces. |
| `'primary'` | The accent fill, or the theme `buttonPrimary` surfaces. |
| `'destructive'` | The danger fill, or the theme `buttonDestructive` surfaces. |
| `'toolbar'` | No fill until hovered, with the icon tinted like the text. |
| `'icon'` | A square as tall as the theme `controlHeight` metric, with no fill until hovered. |
| `'link'` | Text only, in the `accentText` color, underlined while hovered. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.button{text = 'Play', variant = 'primary', onClick = function() print('Play.') end},
    ui.button{text = 'Delete save', variant = 'destructive'},
    ui.button{icon = 'ui/hammer.png', variant = 'toolbar', checked = true},
    ui.button{icon = 'ui/gear.png', variant = 'icon', tooltip = 'Settings'},
    ui.button{text = 'Credits', variant = 'link'},
})

ui.mount(ui.row{gap = 0,
    ui.button{text = 'Home', icon = 'ui/home.svg', iconPlacement = 'top', variant = 'toolbar', grow = 1},
    ui.button{text = 'Search', icon = 'ui/search.svg', iconPlacement = 'top', variant = 'toolbar', grow = 1},
}, {placement = 'safe'})

ui.mount(ui.button{text = 'Search dishes', icon = 'ui/search.svg', justify = 'start', width = 640})
```

### ui.imageButton(properties)

An image button is a picture that works as a button. It can have pictures for hover and press, and text on top in the `onAccent` color. It measures as large as its picture times `scale`. Without a hover picture, it darkens while hovered. Without a pressed picture, it moves down a little while held. It reports `click`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The normal picture. |
| `hoverImage` | image | none | The picture while hovered. |
| `pressedImage` | image | none | The picture while held. |
| `text` | text | none | Text drawn in the middle of the picture. |
| `scale` | number from 0 to 64 | `1` | The size of the button compared with the picture. |
| `tint` | color | `'#FFFFFFFF'` | A color multiplied with the pictures. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.imageButton{
    image = 'ui/wood-button.png',
    hoverImage = 'ui/wood-button-hover.png',
    pressedImage = 'ui/wood-button-pressed.png',
    text = 'Start',
    scale = 2,
    onClick = function() print('Start.') end,
})
```

### ui.chip(properties)

A chip is a small tag that turns on and off when pressed, such as a filter. Its corners round by the theme `chipRadius`, which makes a pill by default. A change reports `change`, with `selected`. A removable chip has its own cross button, which reports `remove`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The label of the chip. |
| `selected` | boolean | `false` | Whether the chip is on. |
| `removable` | boolean | `false` | Adds the remove button. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.chip{id = 'wood', text = 'Wood', selected = true, onChange = function(event)
        print('The wood filter is ' .. tostring(event.selected) .. '.')
    end},
    ui.chip{id = 'stone', text = 'Stone', removable = true, onRemove = function(event)
        event.gui:set('stone', {visible = false})
    end},
})
```

### ui.menuButton(properties)

A menu button opens a menu of items. Picking an item reports `select`, with the item id as `item`. It takes the properties of `button` except `checked`, plus `items`. Its menu opens and scrolls like the list of a [`combo`](#uicomboproperties).

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The menu entries. Items use `id`, `text`, `caption`, `image`, `iconColor` and `enabled`. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.menuButton{
    text = 'File',
    items = {{id = 'save', text = 'Save'}, {id = 'load', text = 'Load', enabled = false}},
    onSelect = function(event)
        print('Picked ' .. event.item .. '.')
    end,
})
```

### ui.popover(properties)

A popover is a button that opens a floating panel, which holds the one child of the node. It takes the properties of `button` except `checked`, plus `contentWidth`. Its panel opens and scrolls like the list of a [`combo`](#uicomboproperties).

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `contentWidth` | number from 0 to 10000 | `480` | The width of the floating panel. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.popover{
    text = 'Help',
    contentWidth = 520,
    ui.label{text = 'Drag the map to look around and tap a tile to walk there.'},
})
```

## Choices

Choices let the player pick a value. They all take the focus.

### ui.checkbox(properties)

A check box is a box with a check mark and a label. Pressing it flips `checked` and reports `change`, with `checked`. A label longer than the width of the check box wraps onto more lines like a `label`, centered beside the box.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The label after the box. |
| `checked` | boolean | `false` | Whether the box is checked. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.checkbox{text = 'Show hints', checked = true, onChange = function(event)
    print('Hints are ' .. tostring(event.checked) .. '.')
end})
```

### ui.toggle(properties)

A toggle is a switch with a label, which wraps like the label of a check box. It takes the properties of `checkbox` and reports `change`, with `checked`.

When it is off, its track takes the theme `track` color, with the `knob` at its start. When it is on, the `accent` color covers the track, with the knob at its end. A change slides the knob across over the theme `transitionDuration`. Meanwhile, the color of the on state fades in or out over the whole track, so the track never moves or changes its shape. Like a button, the track and its on color take the `hover` color under the pointer and the `pressed` color while held. The knob also takes the `pressed` color while held. The theme `track`, `trackFill` and `knob` surfaces draw the toggle when the theme has them, with `trackFill` inside the padding of `track`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.toggle{text = 'Vibration', onChange = function(event)
    print('Vibration is ' .. tostring(event.checked) .. '.')
end})
```

### ui.radioGroup(properties)

A radio group is a set of options of which the player picks one. The labels of a vertical group wrap at its width, like the label of a check box. Picking another option reports `change`, with the item id as `value`. The focus goes to the selected option. The theme `radio` and `radioSelected` surfaces draw the marks when the theme has them.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The options. Items use `id`, `text` and `enabled`. |
| `selected` | string | none | The id of the picked option. |
| `horizontal` | boolean | `false` | Places the options in a row instead of a column. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.radioGroup{
    items = {{id = 'easy', text = 'Easy'}, {id = 'hard', text = 'Hard'}},
    selected = 'easy',
    horizontal = true,
    onChange = function(event)
        print('Difficulty ' .. event.value .. '.')
    end,
})
```

### ui.combo(properties)

A combo is a field that shows the selected item and opens a list of the others. Picking another item reports `change`, with the item id as `value`. The list opens below the field. It opens above instead when the room below is too short for it and the room above is larger. It stays inside the screen, and a list taller than its room scrolls beside a [scroll bar](#scroll-bars). The menus of `menuButton` and `contextMenu` and the popups of `popover` and `colorField` open and scroll the same way.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The choices. Items use `id`, `text` and `enabled`, and the list also shows their `caption`, `image` and `iconColor`. |
| `selected` | string | none | The id of the selected item. |
| `placeholder` | text | none | The text shown while no item is selected. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.combo{
    width = 400,
    items = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}},
    placeholder = 'Language',
    onChange = function(event)
        print('Language ' .. event.value .. '.')
    end,
})
```

### ui.segmentedControl(properties)

A segmented control is a row of joined segments of which one is selected, such as a switch between views. A click or a tap picks a segment and reports `change`, with the item id as `value`. It takes the focus as a whole. Left and right move the selection and skip disabled segments. Accept moves the selection to the next segment and starts over after the last one. Without a selection, accept picks the first segment that can be picked.

The segments fit their labels into the room they get, so every label shows whole:

- With enough room, the segments share the width equally, with the theme `tabPaddingX` on both sides of each label.
- With less room, each segment takes the width of its label, and the padding shrinks down to the theme `contentSpacing`.
- With less room still, the segments share the room in proportion to their labels, which draw smaller, down to three quarters of their font size.
- Below that, the labels wrap between their words onto more lines, and the control grows taller to hold them. Every segment keeps room for the longest word of its label, which draws smaller still when the longest words do not fit side by side. No word breaks.

The segments sit inside the border of the control, or inside the padding of the theme `segment` surface, so the picked segment never covers the rim of the control.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The segments. Items use `id`, `text` and `enabled`. |
| `selected` | string | none | The id of the selected segment. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.segmentedControl{
    width = 600,
    items = {{id = 'daily', text = 'Daily'}, {id = 'weekly', text = 'Weekly'}, {id = 'all', text = 'All time'}},
    selected = 'daily',
    onChange = function(event)
        print('Leaderboard ' .. event.value .. '.')
    end,
})
```

## Inputs

Inputs let the player type or set a value. They all take the focus. The text kinds, `textField`, `secretField`, `textArea`, `filterField` and `numberField`, edit through the native text input of each platform, as the [text input guide](../ui/text-input.md) describes.

### ui.textField(properties)

A text field is a text entry of one line. Typing reports `change`, with the text as `value`. Enter, or the return key of the on-screen keyboard, reports `submit`, also with the text as `value`.

The focused field edits through the native text input of the platform. On phones, tablets, TVs and mobile browsers it opens the on-screen keyboard. Everywhere it brings input methods, autocorrection, dictation and native paste. The keyboard options below choose that keyboard. While an input method composes text, as Japanese and Chinese input do, the field underlines the text in progress. When the on-screen keyboard would cover the focused field, the whole UI moves up until the field shows above it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | string | `''` | The text. The text the player typed stays when `set` changes other properties. |
| `placeholder` | text | none | A hint shown while the field is empty. |
| `maxLength` | integer from 0 to 1048576 | `0` | The largest length in characters, which are Unicode code points. The value `0` sets no limit. |
| `keyboard` | string | `'text'` | The on-screen keyboard: `'text'` for letters, `'number'` for digits, `'decimal'` for digits with a decimal separator, `'phone'` for a phone pad, and `'email'`, `'url'` or `'search'` for letters with the keys of addresses, links or searches. Text areas and secret fields have keyboards of their own. |
| `returnKey` | string | `'default'` | The label of the return key of the on-screen keyboard: `'default'`, `'done'`, `'go'`, `'next'`, `'search'` or `'send'`. With `'next'`, the return key moves the focus to the next field, like Tab, instead of reporting `submit`. |
| `autocorrect` | boolean | `true` for `'text'`, `'search'` and text areas, `false` otherwise | Whether the keyboard corrects spelling and offers suggestions. |
| `autocapitalize` | string | `'sentences'` for `'text'` and text areas, `'none'` otherwise | Which letters the keyboard capitalizes on its own: `'none'`, `'sentences'`, `'words'` or `'characters'`. |

Escape brings back the text the field had when it took the focus, and lets the focus go. So does the escape key of a hardware keyboard on a phone. Closing the on-screen keyboard, or leaving the field, lets the focus go and keeps the text.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.textField{
        placeholder = 'Name your island',
        maxLength = 24,
        autocapitalize = 'words',
        returnKey = 'next',
        onChange = function(event) print('Typing ' .. event.value .. '.') end,
    },
    ui.textField{
        placeholder = 'Email',
        keyboard = 'email',
        returnKey = 'done',
        onSubmit = function(event) print('Email ' .. event.value .. '.') end,
    },
})
```

### ui.secretField(properties)

A secret field is a `textField` that hides what the player types, for passwords and codes. It takes the properties and reports the events of `textField`, except `keyboard`. It always uses the password keyboard, which neither corrects nor capitalizes by default and keeps away input methods that compose text.

```lua
local ui = require('haylen.ui')

ui.mount(ui.secretField{placeholder = 'Password', returnKey = 'go', onSubmit = function(event)
    print('The password has ' .. #event.value .. ' bytes.')
end})
```

### ui.textArea(properties)

A text area is a text entry of several lines. Enter, and the return key of the on-screen keyboard, start a new line. It takes the properties of `textField` except `keyboard`, plus `rows`. It reports `change` and never reports `submit`. It shows its `placeholder` while it is empty.

A line longer than the field wraps between its words onto the lines below, so the text never scrolls sideways. Text taller than the field scrolls with the wheel and with the caret. A [scroll bar](#scroll-bars) then shows inside the border of the field, from the frame after the text grew taller. The bar narrows the lines that wrap.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `rows` | integer from 1 to 200 | `4` | The height in lines of text. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.textArea{rows = 6, value = 'Dear diary,\n', onChange = function(event)
    print('The diary has ' .. #event.value .. ' bytes.')
end})
```

### ui.filterField(properties)

A filter field is a search field with a magnifier and a button that clears it. It takes the properties and reports the events of `textField`, except `keyboard`. It always uses the search keyboard. Clearing reports `change`, with an empty `value`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.filterField{placeholder = 'Search recipes', onChange = function(event)
    print('Filter ' .. event.value .. '.')
end})
```

### ui.numberField(properties)

A number field holds a number between a minimum and a maximum. Its minus and plus buttons change it, and the player can also type it in the middle. The signs of the buttons are strokes of one weight. A number that rounds to zero shows without a sign, as the values of sliders do. A change reports `change`, with the number as `value`.

A typed number inside the range reports at once. A number outside the range waits until the player submits or leaves the field, which brings it into the range. Text that is not a finite number, such as `nan`, changes nothing, and the field shows the number again once the editing ends.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number | `0` | The number, kept between `min` and `max`. |
| `min` | number | `0` | The smallest value. |
| `max` | number | `100` | The largest value. |
| `step` | number, at least 0 | `1` | The amount the buttons add or remove. |
| `decimals` | integer from 0 to 6 | `0` | The decimals shown. |
| `returnKey` | string | `'default'` | The label of the return key of the on-screen keyboard, as in `textField`. |

The on-screen keyboard shows digits, with a decimal separator when `decimals` is above zero. Numeric keypads have no minus sign, so a field whose `min` is negative types on the text keyboard. A minimum above the maximum raises `The property "min" of a "numberField" must not be greater than "max".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.numberField{value = 2, min = 1, max = 8, onChange = function(event)
    print('Players ' .. event.value .. '.')
end})
```

### ui.slider(properties)

A slider is a track with a knob that the player drags. A change reports `change`, with the number as `value`. Only the player changes the value, so a slider reports `change` only when the player moves it. A `value` given off its step stays as given until then.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number | `0` | The number, kept between `min` and `max`. |
| `min` | number from -1e15 to 1e15 | `0` | The value at the start of the track. |
| `max` | number from -1e15 to 1e15 | `1` | The value at the end of the track. |
| `step` | number, at least 0 | `0` | Snaps the value to multiples of the step once the player moves it, with the pointer or with left and right. The value `0` lets it move freely. |
| `showValue` | boolean | `false` | Shows the value after the track. The value takes the room of the wider of `min` and `max`, and at least the theme `sliderValueWidth`, so the tracks of the sliders of one column line up. |
| `decimals` | integer from 0 to 6 | `2` | The decimals of the shown value. |

A minimum that is not smaller than the maximum raises `The property "min" of a "slider" must be smaller than "max".`. An end outside its range raises `The property "min" of a "slider" must be from -1e+15 to 1e+15.`, or the same error for `max`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.slider{value = 0.8, showValue = true, onChange = function(event)
    print('Volume ' .. event.value .. '.')
end})
```

### ui.rangeSlider(properties)

A range slider is a track with two knobs that pick a range, such as a price filter. The pointer drags the knob nearer to where it presses, up to the other knob. It takes the focus as a whole. Left and right move the active knob, and accept switches to the other knob, which shows its own focus ring. A change reports `change`, with `low` and `high`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `low` | number | `0` | The start of the range. |
| `high` | number | `1` | The end of the range, at least `low`. |
| `min` | number from -1e15 to 1e15 | `0` | The value at the start of the track. |
| `max` | number from -1e15 to 1e15 | `1` | The value at the end of the track. |
| `step` | number, at least 0 | `0` | Snaps both ends to multiples of the step. The value `0` lets them move freely, and then left and right move by a twentieth of the track. |
| `showValue` | boolean | `false` | Shows the range after the track. It takes the room of the wider of `min` and `max` at both ends, and at least the theme `sliderValueWidth`, so its track lines up with the tracks of the sliders of its column. |
| `decimals` | integer from 0 to 6 | `2` | The decimals of the shown range. |

A minimum that is not smaller than the maximum raises `The property "min" of a "rangeSlider" must be smaller than "max".`. An end of the track outside its range raises `The property "min" of a "rangeSlider" must be from -1e+15 to 1e+15.`, or the same error for `max`. A `low` above `high` raises `The property "low" of a "rangeSlider" must not be greater than "high".`. A `set` that moves one end past the other end, where the player left it, takes that other end along.

```lua
local ui = require('haylen.ui')

ui.mount(ui.rangeSlider{min = 0, max = 500, low = 50, high = 300, step = 10, showValue = true, decimals = 0, onChange = function(event)
    print('From ' .. event.low .. ' to ' .. event.high .. '.')
end})
```

### ui.stepper(properties)

A stepper shows a value between two arrows that step it, the way console settings pick a difficulty. The value is a number or one of a list of options. The arrows are chevrons of two strokes with round ends, like the arrows of combos, carousels, accordions and trees. A click or a tap on the value moves to the next one. A change reports `change`, with the number or the item id as `value`. It takes the focus as a whole. Left and right step it, and accept moves to the next value, starting over after the last one.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number | `0` | The number of a stepper without items, kept between `min` and `max`. |
| `min` | number | `0` | The smallest number. |
| `max` | number | `10` | The largest number. |
| `step` | number above 0 | `1` | The amount one step adds or removes. |
| `decimals` | integer from 0 to 6 | `0` | The decimals shown. |
| `items` | items | empty | Options to step through instead of a number. The stepper skips disabled ones. Items use `id`, `text` and `enabled`. |
| `selected` | string | first item | The id of the selected option. |
| `wrap` | boolean | `false` | Lets the arrows, left and right go on from one end to the other. |

A minimum above the maximum raises `The property "min" of a "stepper" must not be greater than "max".`. A step of zero or less raises `The property "step" of a "stepper" must be greater than zero.`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.stepper{value = 2, min = 1, max = 4, onChange = function(event)
        print('Players ' .. event.value .. '.')
    end},
    ui.stepper{items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = 'normal', wrap = true, onChange = function(event)
        print('Difficulty ' .. event.value .. '.')
    end},
})
```

### ui.colorField(properties)

A color field shows a swatch and the color written as text, and opens a color picker. The text is `#AARRGGBB` while the player can change the opacity, and `#RRGGBB` without it. A change reports `change`, with the color as a `'#AARRGGBB'` string in `value`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | color | `'#FFFFFFFF'` | The color. |
| `alpha` | boolean | `true` | Lets the player change the opacity. Without it, every picked color is opaque. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.colorField{value = '#FF2E7D32', alpha = false, onChange = function(event)
    print('Flag color ' .. event.value .. '.')
end})
```

### ui.keyCapture(properties)

A key capture shows an [action map binding](input.md#action-map), for a controls screen. Once pressed, it listens for the next key, mouse button, gamepad button or gamepad axis, and takes it as its binding. A controls screen then hands the binding to `input.defineAction`. A new binding reports `change`, with the binding string as `value`, such as `'key:w'`, `'mouse:right'`, `'button:south'` or `'axis:leftX+'`. Sticks and triggers count once they travel well away from where they rested when listening started.

While it listens, it keeps every input from the rest of the UI. A binding of `cancelWith` stops it and reports `cancel`. It takes the focus, and accept starts listening.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | string | `''` | The binding. It shows in words for people, such as `Left Shift` for `'key:leftShift'`, `1` for `'key:digit1'` and `D-pad Up` for `'button:dpadUp'`. |
| `placeholder` | text | none | The text shown while there is no binding. |
| `prompt` | text | `'…'` | The text shown while it listens. |
| `sources` | list of strings | every source | The kinds of input it takes: `'key'`, `'mouse'`, `'button'` and `'axis'`. It ignores the others and goes on listening. |
| `cancelWith` | list of bindings | `{'key:escape'}` | The bindings that stop listening instead of becoming the value. |

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

Indicators show a state or a picture. They do not take the focus.

### ui.badge(properties)

A badge is a small pill with short text, such as a count or a state.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The text of the badge. |
| `tone` | tone | `'neutral'` | The colors of the badge. |
| `solid` | boolean | `false` | Uses the strong fill of the tone instead of its subtle background. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.badge{text = 'New', tone = 'accent', solid = true},
    ui.badge{text = 3, tone = 'danger'},
})
```

### ui.statusIndicator(properties)

A status indicator is a colored dot with optional text, such as an online state.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The text after the dot. |
| `tone` | tone | `'success'` | The color of the dot. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.statusIndicator{text = 'Server offline', tone = 'danger'})
```

### ui.busyIndicator(properties)

A busy indicator is a spinning ring that shows work in progress.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `size` | number from 4 to 1024 | `48` | The width and height. |
| `color` | theme color | `'accent'` | The color of the ring. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    align = 'center',
    ui.busyIndicator{size = 64, color = 'information'},
    ui.label{text = 'Loading'},
})
```

### ui.progress(properties)

A progress bar is filled up to a value between 0 and 1, which suits health and loading bars. The theme `track` and `trackFill` surfaces draw it when the theme has them.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number from 0 to 1 | `0` | The filled share of the bar. |
| `tone` | tone | `'accent'` | The color of the fill. The tone `'neutral'` draws like `'accent'`. |
| `text` | text | none | Text in the middle of the bar. Over the fill it takes the ink of the tone, and over the empty track the `text` color. So every letter keeps its contrast wherever the fill ends. |

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.progress{id = 'health', value = 1, tone = 'success', text = 'HP'})

local function damage(health)
    hud:set('health', {value = health, tone = health < 0.3 and 'danger' or 'success'})
end

damage(0.25)
```

### ui.circularProgress(properties)

A circular progress shows a value between 0 and 1 around a circle. The variant `'ring'` is a ring that fills clockwise from the top. The variant `'cooldown'` is a shade over a picture, which uncovers it clockwise as the value falls, like the cooldown of an ability.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `value` | number from 0 to 1 | `0` | The filled share of the ring, or the share of the picture still shaded. |
| `variant` | string | `'ring'` | The value `'ring'` or `'cooldown'`. |
| `size` | number from 0 to 2048 | `0` | The width and height. The value `0` uses the theme `circularProgressSize` metric. |
| `thickness` | number from 0 to 512 | `0` | The width of the ring. The value `0` uses the theme `circularProgressThickness` metric. |
| `tone` | tone | `'accent'` | The color of the ring, and of the circle behind a cooldown without a picture. |
| `text` | text | none | Text in the middle, such as the seconds left. A cooldown outlines it with the `window` color, so it reads over any picture. |
| `image` | image | none | The picture under the shade of a cooldown. |

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

An icon is a small square picture, such as an item or a resource. A theme color can tint it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture. |
| `size` | number from 0 to 1024 | `0` | The width and height. The value `0` uses the theme `iconSize` metric. |
| `color` | theme color | none | The tint of the picture. Without it, the picture keeps its colors. |

```lua
local ui = require('haylen.ui')

-- An SVG icon drawn with currentColor takes the color of the icon, sharp at any size.
ui.mount(ui.row{
    ui.icon{image = 'ui/coin.png'},
    ui.icon{image = 'icons/heart.svg', size = 48, color = 'danger'},
})
```

### ui.image(properties)

An image is a picture that measures as large as its natural size times `scale`, and fits into the bounds it gets. The natural size is the pixels of an image file or the size of an SVG document.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture. |
| `fit` | string | `'contain'` | The value `'contain'` shows the whole picture inside the bounds. The value `'cover'` fills the bounds and crops what spills over. The value `'fill'` stretches the picture to the bounds. |
| `scale` | number from 0 to 64 | `1` | The measured size compared with the picture. |
| `tint` | color | `'#FFFFFFFF'` | A color multiplied with the picture. |
| `radius` | number from 0 to 4096 | `0` | Rounds the corners of the picture by this radius, at most half its shorter side, such as a photo on a card. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.image{image = 'ui/logo.svg', scale = 4},
    ui.image{image = 'ui/banner.png', fit = 'cover', height = 200, align = 'stretch', tint = '#C0FFFFFF', radius = 16},
})
```

### ui.avatar(properties)

An avatar is a round picture of a player. Without a picture, it shows the initials of the first two words of the name.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | image | none | The picture. |
| `name` | text | none | The name whose initials show without a picture. |
| `size` | number from 8 to 1024 | `64` | The diameter. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.avatar{name = 'Ana Souza'},
    ui.avatar{image = 'ui/players/bia.png', size = 96},
})
```

## Lists and grids

Lists and grids show many entries and let the player pick them. The entries of lists, trees, slot grids and collections take the focus. The kinds `list` and `tree` take [items](#items).

### ui.list(properties)

A list shows rows of items, each as tall as the theme `listRowHeight` metric, with an optional picture and caption. Pressing a row selects it and reports `select`, with the item id as `item`. Pressing the selected row reports it again. The rows take the focus. Only some rows draw and take part in navigation: the rows in the view of the scroll around the list, one row on each side of them, and the focused row. So a list of thousands of rows costs what its view shows. The focus still walks past the view, and the view follows it.

A draggable list moves its rows the way a [slot grid](#uislotgridproperties) moves slots:

- The pointer drags a row onto a row or a slot of any list or slot grid.
- A finger picks a row up once it held it for the theme `longPressDuration`, since a finger that moves at once scrolls.
- A dragged row near an end of a `scroll` or a `collection` around it scrolls that area, faster the closer it gets, so it reaches any row.
- The keyboard, gamepads and remotes carry a row with accept. Accept on a row that is not selected selects it and reports `select`. Accept on the selected row picks it up. So players without a pointer can both choose rows and carry them.
- While a row is carried, accept on another row drops it there. Accept on the carried row, or `uiCancel`, puts it back.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The rows. Items use `id`, `text`, `caption`, `image`, `iconColor` and `enabled`. |
| `selected` | string | none | The id of the highlighted row. |
| `draggable` | boolean | `false` | Lets the player drag and carry rows, and drop items on them. |

| Event | Values | When |
| --- | --- | --- |
| `select` | `item` | The player pressed a row or chose it with accept. |
| `dragStart` | `item` | A draggable list: the player picked up or started to drag the row `item`. |
| `drop` | `item`, `source`, `sourceItem` | A draggable list: an item landed on the row `item`. The value `source` is the id of the node the item left, and `sourceItem` is its row or slot id there. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.list{
    items = {
        {id = 'slot1', text = 'Slot 1', caption = 'Day 12, 3 hours'},
        {id = 'slot2', text = 'Slot 2', caption = 'Empty'},
    },
    selected = 'slot1',
    onSelect = function(event)
        print('Picked the save slot ' .. event.item .. '.')
    end,
})
```

### ui.tree(properties)

A tree shows nested items that open and close. Pressing the arrow of an item with children opens or closes it, and reports `toggle` with the item id as `item` and the new state as `expanded`. Pressing the rest of a row selects it, and reports `select` with the item id as `item`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The items. Items use every item key, `children` included. |
| `selected` | string | none | The id of the highlighted item. |
| `expanded` | list of strings | none | The ids of the open items. The player opens and closes items afterwards, and setting it again replaces the open items. |

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
        print(event.item .. ' is now ' .. (event.expanded and 'open' or 'closed') .. '.')
    end,
})
```

### ui.table(properties)

A table shows rows of cells under a header. Columns with a `width` keep it, and the other columns share the rest. Pressing a row selects it and reports `select`, with the row id as `item`.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `columns` | list of columns | empty | The columns. |
| `rows` | list of rows | empty | The rows. |
| `selected` | string | none | The id of the highlighted row. |

A column is a table with these keys.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `text` | text | none | The header text. |
| `width` | number from 0 to 10000 | shared | A fixed width for the column. |
| `align` | string | `'start'` | The value `'start'`, `'center'` or `'end'` for the header and the cells. |

A row is a table with a unique `id` string and a `cells` list of texts, at most one per column. A row with fewer cells leaves its last columns empty. The table checks its rows and columns, and keeps what it had when they are wrong:

- A row with more cells than columns raises `The property "rows" of a "table" gives the row "<id>" <count> cells for <columns> columns.`. The same sentence names `columns` when only the columns change.
- A row without an id or cells raises `The property "rows" of a "table" must hold objects with an id and a list of cells.`.
- A repeated id raises `The property "rows" of a "table" uses the row id "<id>" more than once.`.
- An unknown column key raises `Unknown key "<key>" in "table.columns".`.
- A column width out of range raises `The property "columns" of a "table" has a width that is not a number from 0 to 10000.`.
- An unknown column alignment raises `The property "columns" of a "table" has an "align" other than "start", "center" or "end".`.

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

A slot grid is a grid of square slots that hold pictures and counts, such as an inventory, a chest or a hotbar. Every slot takes the focus. A click or a tap selects a slot and reports `select`, with the slot id as `item`. The player moves items between slots:

- The pointer drags the item of a slot onto another slot, or onto a row of any slot grid or draggable list, in the same GUI or another one.
- A finger picks an item up once it held it for the theme `longPressDuration`.
- A dragged item near an end of a `scroll` or a `collection` around it scrolls that area.
- The keyboard, gamepads and remotes carry an item. Accept picks the focused slot up, the focus moves, and accept drops it on another slot. Accept on the carried slot, or `uiCancel`, puts it back.

The grid only reports moves, so the app moves its items and sets the new slots. Picking up reports `dragStart`, with the slot id as `item`. A drop reports `drop` on the node it lands on. Its `item` is the slot or row it lands on. Its `source` is the id of the node the item left, and `sourceItem` is its slot or row there.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `slots` | list of slots | empty | The slots, in reading order. |
| `columns` | integer from 1 to 64 | `4` | The slots in each row. |
| `slotSize` | number from 0 to 1024 | `0` | The width and height of each slot. The value `0` uses the theme `slotSize` metric. |
| `gap` | number from 0 to 1024 | half the theme `itemSpacing` | The space between slots. |
| `selected` | string | none | The id of the highlighted slot. |
| `draggable` | boolean | `true` | Lets the player drag and carry items. Without it, accept selects a slot. |

A slot is a table with a unique `id` string and these optional keys: `image`, `count` and `enabled`. The `count` is text drawn on a dark pill in the bottom corner on the end side, in the theme colors `tooltip` and `onTooltip`, so it reads over any picture. Slots without an id raise `The property "slots" of a "slotGrid" must hold objects with an id.`. A repeated id raises `The property "slots" of a "slotGrid" uses the slot id "<id>" more than once.`. An unknown key raises `Unknown key "<key>" in "slotGrid.slots".`.

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

A collection shows any number of items of several types, in a vertical or horizontal list or in a grid that scrolls along one axis. It builds cells from the templates of its types and binds them to the items in view. As the player scrolls, it reuses the cells for other items. So its cost follows the visible items, not the item count, and a list of a hundred thousand items costs about what a list of a hundred does. The [long lists guide](../ui/ui.md#long-lists) explains how it works and what it costs.

The app gives a collection its items through the [`UiCollection`](#uicollection) that [`gui:collection(id)`](#guicollectionid) returns: as a Lua list with `setItems`, or in pages with `setPages`. The section [Collections](#collections) describes types, templates and bindings. The one child of the node, such as an `emptyState`, shows while the collection has no items.

A vertical collection is as long as its content within its size bounds, like `scroll`. So it needs a `height`, a `maxHeight` or `grow` to scroll, and it fills the width it gets. A horizontal collection needs a `width`, or `grow` along a row. It is as tall as its `height`, or as the tallest cell it has measured, which never shrinks while it lives.

The player scrolls a collection in these ways:

- The wheel and the trackpad scroll the collection under the pointer along its axis. A horizontal one scrolls with a sideways gesture, or with Shift and the mouse wheel. A gesture across its axis scrolls the area around it.
- A finger drags it along its axis and flings it when it lets go. Past its ends it stretches, with a resistance that grows the farther it goes.
- The [scroll bar](#scroll-bars), in its lane on the end edge, drags and pages it. The cells leave room for the lane from the frame after the items grew longer than the view. A horizontal collection adds the lane to its height.
- The focus scrolls the item that takes it into view.
- The mouse does not drag the content, as in the lists of desktop platforms.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `types` | object | required | The item types mapped to their definitions, as [types and templates](#types-and-templates) describes. |
| `axis` | string | `'vertical'` | The scroll axis, `'vertical'` or `'horizontal'`. |
| `layout` | string | `'linear'` | The value `'linear'` places one item per line. The value `'grid'` places cells in lanes across the axis. |
| `lanes` | integer from 1 to 64 | `2` | The cells across the axis of a grid: the columns of a vertical grid, or the rows of a horizontal one. |
| `minCellSize` | number from 0 to 10000 | `0` | Above zero, a grid fits as many lanes as hold cells at least this long across, instead of `lanes`. |
| `cellAspect` | number from 0 to 100 | `0` | Above zero, grid cells that span less than the whole line are this many times as long along the axis as across. So they need no measuring. |
| `gap` | number from 0 to 10000 | theme `itemSpacing` | The space between lines and between lanes. |
| `padding` | insets | `0` | The space between the edges of the view and the content. It scrolls with the content. |
| `placeholder` | string | none | The type that shows the items of pages that did not load yet. Without it, those items take their estimated length and draw nothing. |
| `stickToEnd` | boolean | `false` | Lays short content against the end edge. While the view is at the end, it stays at the end as items arrive, as a chat does. |
| `snap` | string | `'none'` | Once the content rests, `'item'` settles on the start of the nearest item, `'center'` centers the nearest item, and `'page'` settles on whole views. |
| `scrollbar` | boolean | `true` | Shows the scroll bar while the content is longer than the view. It fades while the content rests after touch input, and its lane stays. |
| `prefetch` | number from 0 to 4 | `0.5` | The views bound ahead in the direction of scrolling, so items show bound when they arrive. |
| `poolSize` | integer from 0 to 256 | `8` | The detached cells each type keeps for reuse between frames. |
| `selection` | string | `'none'` | The values `'single'` and `'multiple'` let presses select items. |
| `selected` | list of strings | empty | The ids of the selected items. The player changes the selection afterwards, and setting it again replaces it. |
| `selectionFollowsFocus` | boolean | `false` | With single selection, selects the item that takes the focus, as TV lists do. |
| `focusAlign` | string | `'nearest'` | Where an item that takes the focus scrolls to. The value `'nearest'` only brings it into view, and `'start'`, `'center'` and `'end'` align it with that part of the view. |
| `rememberFocus` | boolean | `false` | A move that enters the collection from outside lands on the item that had the focus there last, while that item is near the view, instead of on the nearest item. |
| `refreshable` | boolean | `false` | Lets a touch drag past the start of a vertical collection report `refresh`. |
| `refreshing` | boolean | `false` | Shows the busy indicator at the start. A pull sets it, and the app sets it back to `false` with `gui:set` when the refresh ends. |
| `endThreshold` | integer from 0 to 10000 | `5` | How many items from either end `endReached` and `startReached` report. |
| `animateChanges` | boolean | `true` | Slides the items that a change moves, fades in the items it inserts and fades out the items it removes, for the items in view. |
| `reorderable` | boolean | `false` | Lets the player move items to other places, as [reordering](#reordering) describes. |

The common properties apply too. The property `focusWrap` along the axis wraps the focus from the last item to the first and back, even when they do not show. The property `focusScope` keeps the focus inside the collection.

Indices count from 1 in every event.

| Event | Values | When |
| --- | --- | --- |
| `select` | `item`, `index`, and `selected` unless `selection` is `'none'` | The player pressed an interactive cell with the pointer, a tap or accept. In multiple selection, a press flips the selection of the item. A press on the selected item of a single selection reports it again. |
| `itemFocus` | `item`, `index` | The focus moved to another item of the collection. |
| `visibleChange` | `first`, `last` | The indices of the first and last items in view changed. It reports at most once per frame. |
| `scrollEnd` | `offset`, `item` | Scrolling by the player or a `scrollTo` came to rest, with the first item in view whole. |
| `endReached`, `startReached` | `count` | The items in view came within `endThreshold` items of the end or the start. It reports once per item count, so an app appends a page at a time. |
| `refresh` | none | A pull at the start went past the theme `refreshDistance` and let go. |
| `dragStart` | `item`, `index` | The player lifted an item of a reorderable collection with the pointer, or picked it up with accept. |
| `reorder` | `item`, `from`, `to` | The player dropped the item that was at `from` on the place `to` of a reorderable collection. The call `collection:move(event.from, event.to)` makes the move. |
| the event of a part | its values, `part`, `cell` | A part of a cell reported it, such as `click` of a button or `change` of a toggle. The value `part` is the name of the part, and `cell` is a table with the `item`, `index` and `type` of the item it shows. |

Every event of a part arrives on the collection. So a handler such as `onClick` on the collection answers the buttons of every cell, and tells them apart by `event.part` and `event.cell.item`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'quests', height = 640, selection = 'single', types = {
    quest = {template = ui.row{gap = 16,
        ui.label{part = 'name', bind = {text = 'name'}, grow = 1},
        ui.button{part = 'track', text = 'Track'},
    }},
}, onSelect = function(event)
    print('Opened ' .. event.item .. '.')
end, onClick = function(event)
    print('Tracking ' .. event.cell.item .. ' from the part ' .. event.part .. '.')
end, ui.emptyState{title = 'No quests yet'}})

gui:collection('quests'):setItems({{id = 'q1', name = 'Find the lighthouse'}, {id = 'q2', name = 'Build a raft'}})
```

## Settings

Settings kinds lay out a settings screen.

### ui.settingsForm(properties)

A settings form is a column of settings rows, section titles and actions. It adds room above every `sectionTitle` child except the first. It holds any number of children and has no properties of its own.

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

A settings row shows a setting, with its label and caption on the left and its one child control on the right. The label column is as wide as the theme `settingsLabelWidth` metric, and at most half the row.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `label` | text | none | The name of the setting. |
| `caption` | text | none | An explanation under the label, in the caption font. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.settingsRow{
    label = 'Language',
    caption = 'Changes every text at once',
    ui.combo{width = 360, items = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}}, selected = 'en'},
})
```

### ui.settingsActions(properties)

Settings actions are a row of buttons at the end of a form, such as cancel and save, lined up to the end edge. It holds any number of children and has no properties of its own.

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

Overlays float above the layout and take no room in it.

### ui.dialog(properties)

A dialog is a modal window over the whole screen, with a title, a message, optional children and answer buttons. A backdrop in the theme `overlay` color dims everything behind it. The dialog and its backdrop fade in together when it opens, and fade out together when it closes, over the theme `transitionDuration`. A closing dialog takes no more answers. The last button starts with the focus.

- Pressing a button closes the dialog and reports `answer`, with the button id as `button`.
- Escape or the east gamepad button closes a dismissible dialog and reports `dismiss`.
- A dialog that the player closed shows again when `set` sets `open = true`.
- A dialog stays inside the screen. When its title, message and children are taller than it, they scroll above the buttons, beside a [scroll bar](#scroll-bars) at the edge of the dialog. The padding of the dialog holds the lane of the bar.
- One dialog shows at a time. A dialog that opens while another one shows waits until that one closes.
- A dialog closes when its node stops drawing, because it, a node around it or its GUI was hidden. It shows again once it draws while `open` is still `true`. The popups of menu buttons, popovers, combos, color fields and context menus close the same way.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `open` | boolean | `false` | Shows the dialog. |
| `title` | text | none | The title, in the heading font. |
| `message` | text | none | The message under the title. |
| `dismissible` | boolean | `true` | Lets Escape and the east gamepad button close the dialog. |
| `buttons` | list of buttons | empty | The answer buttons, lined up at the bottom right in list order. |

A button is a table with a unique `id` string, a `text` and a `variant`. Buttons without an id raise `The property "buttons" of a "dialog" must hold objects with an id.`. A repeated id raises `The property "buttons" of a "dialog" uses the id "<id>" more than once.`. An unknown variant raises `The property "buttons" of a "dialog" has a variant other than "default", "primary", "destructive", "toolbar", "icon" or "link".`.

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
        onAnswer = function(event) print('The player chose ' .. event.button .. '.') end,
        onDismiss = function(event) print('The dialog was dismissed.') end,
        ui.checkbox{text = 'Do not ask again'},
    },
})
```

### ui.toast(properties)

A toast shows short notices at an edge of the safe area. The notice of the toast itself shows while `open` is `true`. The [commands](#guicommandid-name-arguments) `show`, `update` and `hide` add more notices to the same toast, change them and let them go.

A notice is a pill in the color of its tone. It can hold an indicator, a title, a text, an action and a close button. It slides in from the edge and fades in, stays for its duration, and then fades out while the notices around it move into its place. A toast takes no room in the layout. Its notices take the pointer over them. The action and the close button never take the focus, so a new notice never pulls the focus from the game or the screen.

The notices of every toast at the same position form one stack, so they never cover each other:

- In a `'list'`, the notices line up in the order they were asked to show. When the list already shows the theme `toastLimit` notices, the next ones wait their turn, and their time starts once they show.
- In a `'deck'`, the newest notice is in front at the edge. The older ones peek out behind it, smaller, each the theme `toastPeek` past the notice in front of it.
- An `expandable` deck opens into a list while the pointer or a finger is on it. It stays open while the pointer is anywhere in the area its notices take.
- A deck that is not expandable keeps its depth. Its notices behind the front leave every press to the front one. Each press or swipe takes the front notice away, and the next one comes forward.
- The time of the notices of a stack stops while the pointer or a finger is on it.
- When an edge of the safe area is too narrow for its start, middle and end stacks side by side, they form one stack. Every notice keeps its side in it.

A notice of a list is as wide as what it shows. It grows up to the theme `toastWidth` times the [text scale](#uisettextscalescale), within the safe area. The notices of a deck all take that width, so they line up.

The player lets a notice go in several ways. A press on a notice reports `click` and lets it go. The action reports `action` and lets the notice go. The close button lets it go. A press that drags the notice sideways past a third of its width swipes it away, while a shorter drag springs back. Every notice that leaves reports `dismiss` with the reason. The one exception is the notice of the properties when `open = true` is set again, which replaces it with a new one at the end of the stack.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `open` | boolean | `false` | Shows the notice of the toast. |
| `text` | text | none | The notice. |
| `title` | text | none | A bold line over the text. |
| `tone` | tone | `'neutral'` | The color of the notice, and the symbol of the indicator `'icon'`: a check for `'success'`, a cross for `'danger'`, an exclamation mark for `'warning'` and an "i" for the other tones. |
| `variant` | string | `'solid'` | How the notice paints its tone. The value `'solid'` fills it with the color of the tone. The value `'soft'` fills it with the pale color of the tone over the raised color, which keeps it opaque, with a border of the tone. The value `'outline'` fills it with the raised color of the theme, with a border of the tone. A neutral notice takes the colors of tooltips. A theme that paints toasts with an image keeps the image and gives the tone to the indicator. |
| `indicator` | string | `'none'` | What shows before the text: `'none'`, `'icon'` for the symbol of the tone, `'busy'` for a spinner, or `'progress'` for a ring that fills with `progress`. |
| `progress` | number from 0 to 1 | `0` | How full the ring of the indicator `'progress'` is. |
| `action` | text | none | The text of a button at the end of the notice, such as `'Undo'`. |
| `closable` | boolean | `false` | Shows a close button at the end of the notice. |
| `duration` | number from 0 to 3600 | `3` | The seconds on screen. The value `0` keeps the notice until it is closed, swiped away or hidden, or until `open` is set to `false`. |
| `position` | string | `'top'` | Where the stack sits. The values `'top'` and `'bottom'` are the middle of the edge. The values `'topStart'`, `'topEnd'`, `'bottomStart'` and `'bottomEnd'` are its sides, where start is the left of a left-to-right UI. |
| `arrangement` | string | `'list'` | How the notices of the toast stack: `'list'` or `'deck'`. |
| `expandable` | boolean | `true` | Opens a deck into a list while the pointer or a finger is on it. Without it, the deck keeps its depth. |

The commands of a toast take a table:

- The command `'show'` adds a notice. Its arguments are `notice`, the id of the notice, and `title`, `text`, `action`, `tone`, `variant`, `indicator`, `progress`, `closable` and `duration`, which work like the properties of the same names. A notice looks like the properties of the toast unless its arguments say otherwise, and it says only what its arguments say. Showing a notice with the id of a notice that shows changes it and starts its time again.
- The command `'update'` changes the notice with the id `notice`, with the same arguments, such as the progress of an upload. A `duration` among them starts its time again. A missing id raises `The "update" command of a "toast" needs the "notice" it changes.`.
- The command `'hide'` lets the notice with the id `notice` go. Without an id, it lets every notice of the toast go.

An id that no notice of the toast has raises `The toast "<id>" shows no notice "<notice>".`. An argument that is not a table raises `The "<command>" command of a "toast" takes a table.`. Unknown arguments raise `Unknown key "<key>" in the "<command>" command of a "toast".`. Wrong values raise the errors of the properties of the same names.

| Event | Values | When |
| --- | --- | --- |
| `noticeShow` | `notice` | A notice comes on screen. For a notice that waited in a full list, that is when its turn comes. |
| `click` | `notice` | The player pressed a notice and let go without swiping it, and the notice leaves. |
| `action` | `notice` | The player pressed the action of a notice, which then leaves. |
| `dismiss` | `reason`, `notice` | A notice left. The reason is `'timeout'` when its time ended, `'clicked'` after a press, `'closed'` after its close button, `'swiped'` after a swipe, `'action'` after its action, and `'hidden'` after `open = false` or the command `'hide'`. |

The value `notice` is the id the notice was shown with. It is absent for the notice of the properties.

```lua
local ui = require('haylen.ui')

local notices = ui.mount(ui.toast{id = 'saved', text = 'Progress saved', tone = 'success', indicator = 'icon', position = 'bottom',
    onAction = function(event)
        print('Undo ' .. event.notice .. '.')
    end,
    onDismiss = function(event)
        print((event.notice or 'saved') .. ' left: ' .. event.reason .. '.')
    end})

notices:set('saved', {open = true})
notices:command('saved', 'show', {notice = 'chest', text = 'Two new items in the chest', tone = 'information', duration = 5})
notices:command('saved', 'show', {notice = 'trash', text = 'The map was deleted', variant = 'outline', tone = 'danger', indicator = 'icon', action = 'Undo', closable = true})

-- A notice that follows a task changes as the task goes on.
notices:command('saved', 'show', {notice = 'upload', text = 'Uploading the save', indicator = 'progress', duration = 0})
notices:command('saved', 'update', {notice = 'upload', progress = 0.5})
notices:command('saved', 'update', {notice = 'upload', text = 'Save uploaded', tone = 'success', indicator = 'icon', duration = 2})

-- The newest notice in front, the older ones behind it.
local deck = ui.mount(ui.toast{id = 'feed', position = 'bottomEnd', arrangement = 'deck', duration = 6})
deck:command('feed', 'show', {text = 'A gull stole a fish'})
deck:command('feed', 'show', {text = 'The tide is rising', tone = 'information'})
```

### ui.window(properties)

A window is a floating panel with a title bar that the pointer drags around, such as an inventory or a map over the game. It holds a column of children and takes the properties of `column` for them. It takes no room in the layout and sizes itself to its children, unless it has a `width` or `height`. It pads its content like a panel. It draws with the theme `window` surface, on a shadow half again as large as the shadow of a card.

A window stays inside the safe area at every scale of the UI, wherever `x` and `y` place it. Only the part of its body below the title bar may leave the bottom. Its close button stands inside the padding of the theme `titleBar` surface, clear of the ends of the bar. Dragging the title bar reports `move`, with the new `x` and `y`, once the pointer lets go. Closing reports `close`. Moves of the focus reach the controls of the window from the rest of its GUI and bring it to the front. The action `uiCancel` closes a closable window while the focus is inside it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `title` | text | none | The title in the title bar. |
| `x`, `y` | number | the middle of the safe area | The position of the top left corner, from the top left of the screen. Setting them moves the window there. Setting only one keeps the other coordinate where the window is. |
| `open` | boolean | `true` | Shows the window. A window that the player closed shows again when `set` sets `open = true`. |
| `closable` | boolean | `false` | Adds the close button to the title bar and lets `uiCancel` close the window. |
| `movable` | boolean | `true` | Lets the pointer drag the title bar. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.window{id = 'map', title = 'Map', x = 120, y = 120, width = 640, closable = true,
        onClose = function(event) print('The map closed.') end,
        onMove = function(event) print('The map is at ' .. event.x .. ', ' .. event.y .. '.') end,
        ui.image{image = 'ui/map.png', fit = 'contain', height = 400},
        ui.button{text = 'Center on me', autofocus = true},
    },
}, {placement = 'screen'})
```

### ui.contextMenu(properties)

A context menu is a menu of items that opens over the one child of the node. It opens with a right click on the child, a long press on a touch screen, `uiMenu` while the focus is inside it, or the `open` command. Picking an item reports `select`, with the item id as `item`. The action `uiCancel`, or a click outside, closes the menu. The menu takes the focus while it is open and gives it back when it closes.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `items` | items | empty | The menu entries. Items use `id`, `text`, `caption`, `image`, `iconColor` and `enabled`. |

```lua
local ui = require('haylen.ui')

local slots = ui.mount(ui.contextMenu{
    id = 'slotMenu',
    items = {{id = 'rename', text = 'Rename'}, {id = 'delete', text = 'Delete'}},
    onSelect = function(event)
        print('Save slot: ' .. event.item .. '.')
    end,
    ui.button{text = 'Save slot 1', width = 480},
})
slots:command('slotMenu', 'open')
```

## Touch controls

Touch controls are on-screen controls that drive the virtual buttons and sticks of the action map. Actions of [`haylen.input`](input.md) read them through the bindings `virtual:<name>` and `virtualStick:<name>`. The [UI guide](../ui/ui.md#touch-controls) shows how to place them.

- Every control follows its own finger, so a stick and several buttons work at the same time. The mouse drives them when no finger is down.
- They keep the pointer from reaching the app behind them.
- A control that stops drawing lets go of the virtual button or stick it held, in that same frame. That happens when it, a container around it or its GUI is hidden or removed. A touch button that was pressed then reports `release`, unless its GUI was unmounted.
- A control that shows again under a finger that is still down waits for a new press.
- A control with `enabled = false` lets go the same way and ignores fingers and the mouse. Once enabled again, it waits for a new press.

### ui.touchStick(properties)

A touch stick is a virtual analog stick. While a finger drags it, it sets the virtual stick `action` to a vector of length 0 to 1. Several sticks work at once, each with its own finger. It reports only the events of every kind.

The `stickBase` and `stickKnob` surfaces of its theme or [style](#styles) draw it when they have images. Otherwise it draws flat in the theme colors: a ring around a guide ring where the knob turns, and a knob raised over a soft shadow and rimmed in the accent color.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `action` | string | required | The name of the virtual stick it drives. |
| `radius` | number from 8 to 2048 | `110` | The radius of the stick ring. |
| `deadZone` | number from 0 to 0.95 | `0.15` | The share of the radius near the center that counts as zero. |
| `mode` | string | `'fixed'` | Where the ring sits. The value `'fixed'` keeps it at the center of the control. The value `'floating'` centers it where the finger lands inside the control. The value `'following'` also moves it after a finger that leaves the ring, so the finger always sits on its rim, inside the control. Without a finger, the ring rests at the center of the control. |
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

A touch button holds the virtual button `action` down while a finger or the mouse presses it. It reports `press` and `release` for any finger, in place of the `press` and `release` of every kind. The theme `touchButton` and `touchButtonPressed` surfaces draw it when the theme has them.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `action` | string | required | The name of the virtual button it drives. |
| `text` | text | none | A label in the middle of the button. It keeps the theme `contentSpacing` from the ring. When it is wider than the room the ring leaves, it draws smaller, down to half its size. |
| `image` | image | none | A picture in the middle of the button. |
| `size` | number from 8 to 2048 | `120` | The width and height. |
| `touchOnly` | boolean | `false` | Shows the button only while the last input came from a touch screen. |
| `slideIn` | boolean | `false` | Takes a finger, or the held left mouse button, that moves onto the button after it went down elsewhere. It lets go when the finger moves off. So a thumb that slides along a row of buttons presses each one in turn. Without it, the button takes only a finger that lands on it. |

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
    ui.touchButton{action = 'jump', text = 'A', onPress = function() print('Pressed.') end, onRelease = function() print('Released.') end},
}, {placement = 'screen'})

scene.push({
    update = function(self, dt)
        if input.pressed('jump') then
            print('Jump.')
        end
    end,
})
```

## Play areas

A play area marks where the game shows inside a GUI, so the focus can go to the game. The [UI guide](../ui/ui.md#share-the-screen-with-the-game) shows how a screen shares the devices with the game, and the [input guide](../input/input.md#who-owns-the-keyboard-and-the-gamepad) explains the whole model.

### ui.playArea(properties)

A play area is the part of a screen where the game shows, as a place the focus can go, such as the space a level leaves between its controls. It draws nothing but the focus ring, and its rounded corners when it has a `radius`. It takes its size from `width`, `height` or `grow`.

- While it has the focus, the directions, accept and menu of the keyboard, gamepads and TV remotes reach the action map of the app. They do not move the focus or press a control.
- Cancel still reaches the `onCancel` handlers of the GUI. Tab and `uiFocus` move the focus to the controls of the GUI and back.
- It never takes the pointer. A click or a touch on it gives it the focus and reaches the game too, which reads `mouse:` bindings and touches as usual. A control drawn over it, such as a touch button, takes the press instead.
- It reports `focus` and `blur` like every node. The method `gui:bounds(id)` tells where it lies, so a camera can draw the world inside it.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `radius` | number from 0 to 10000 | `0` | Rounds the corners of the area and of its focus ring. The game draws below every GUI, so the area covers the corners of the game outside the curve with `cornerColor`. |
| `cornerColor` | color | the theme `window` color | The color around the area, which its rounded corners take, such as the `clearColor` of the app or the fill of the card the area sits in. |

```lua
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

input.defineAction({name = 'move', type = 'vector', up = {'key:up'}, down = {'key:down'}, left = {'key:left'}, right = {'key:right'}, bindings = {'stick:left'}})

scene.push({
    enter = function(self)
        self.gui = ui.mount(ui.row{
            ui.playArea{id = 'world', grow = 1, radius = 16, cornerColor = haylen.config.clearColor, autofocus = true, onFocus = function()
                print('The arrows move the hero.')
            end},
            ui.column{width = 360, ui.button{id = 'map', text = 'Map'}},
        }, {owner = self})
    end,
    update = function(self, dt)
        local dx, dy = input.vector('move')
        if dx ~= 0 or dy ~= 0 then
            print('The hero walks ' .. dx .. ', ' .. dy .. '.')
        end
    end,
})
```

## Layout

Layout decides where every node sits. The [containers](#containers) place their children, and every node takes the common properties below. This section also covers anchors, scroll bars, the scale of the UI, the safe area and right-to-left layouts. The [UI guide](../ui/ui.md#lay-out-the-nodes) teaches layout step by step.

### Common properties

Every kind takes these properties. A hidden node takes no room.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `visible` | boolean | `true` | A hidden node takes no room and does not draw. |
| `enabled` | boolean | `true` | A disabled node draws dimmed and ignores the player. |
| `tooltip` | text | none | Text that shows next to the pointer after it rests on the node for the theme `tooltipDelay`, half a second by default. |
| `grow` | number from 0 to 1000 | `0` | The share of the free space the node takes along a column or a row. A growing node starts from nothing rather than from its content. So two nodes with `grow = 1` split the free space in half, and a growing `scroll` scrolls inside its share. A column or row that takes the size of its content gives every growing node at least the room its content needs. Only `minWidth`, `maxWidth`, `minHeight` and `maxHeight` hold a growing node back. So a scroll with `grow = 1` and `height = 0` measures as nothing and fills the room the others leave. |
| `width` | length | `'auto'` | A fixed width. |
| `height` | length | `'auto'` | A fixed height. |
| `minWidth` | number | `0` | The smallest width. |
| `maxWidth` | number | unbounded | The largest width. |
| `minHeight` | number | `0` | The smallest height. |
| `maxHeight` | number | unbounded | The largest height. |
| `align` | string | depends on the kind | The value `'start'`, `'center'`, `'end'` or `'stretch'`. A column places the node across its width with it, a row across its height, and a grid inside its cell. A stack or the GUI root places it in both directions. Without `align`, rows center their children. Everywhere else, these kinds use `'start'`: labels, buttons, image buttons, menu buttons, popovers, chips, check boxes, toggles, radio groups, badges, status indicators, circular progress indicators, icons, images, avatars, rich text and touch controls. The busy indicator uses `'center'`, and every other kind stretches. |
| `anchor` | string | none | Takes the node out of the layout of its parent and places it against the safe area, the screen or its parent, as [anchors](#anchors) describes. The value `'none'` puts it back in the layout. |
| `anchorTo` | string | `'safe'` | The area an anchored node is placed in: `'safe'`, `'screen'` or `'parent'`. The value `'parent'` is the bounds of the parent, which for the root of a GUI is the area of the GUI. |
| `margin` | insets | `0` | The space around the node. Its parent places it that far from the nodes and edges around it, and an anchored node keeps that distance from the edges of its area. |
| `aspectRatio` | number from 0.01 to 100 | none | The width divided by the height, which the node keeps, such as `16 / 9` for a video frame. Its height follows its width, and its width follows a fixed `height`. Without a fixed size, it takes the whole width it is offered. A place of another shape shrinks around its center to the ratio. |
| `focusable` | boolean | `true` | The value `false` keeps the controls of the node, and of every node inside it, out of keyboard, gamepad and remote navigation. A click then presses them without moving the focus there, which suits HUD buttons that share keys with gameplay. |
| `autofocus` | boolean | `false` | Gives the node the focus when it appears, such as when its GUI mounts or its dialog opens. It does nothing when the focus is already on a control of the same GUI and window. |
| `focusScope` | boolean | `false` | Keeps the focus inside the node while it is there, and makes the node hear `cancel`. |
| `focusWrap` | string | `'none'` | The value `'horizontal'`, `'vertical'` or `'both'` wraps a move that would leave the node around to its other side. |
| `focusRing` | boolean | `true` | Shows the focus ring around the node and its children while they have the focus. A node that draws the focus its own way turns it off and reacts to its `focus` and `blur` events, such as a card that grows and glows. The focus still moves to it. |
| `focusLeft`, `focusRight`, `focusUp`, `focusDown` | string | none | The id of the node the focus moves to in that direction, instead of the nearest one. A node that names itself keeps the focus in that direction. |
| `direction` | string | `'inherit'` | The value `'leftToRight'` or `'rightToLeft'` lays out the node and every node inside it in that direction, whatever the direction of the UI. The value `'inherit'` takes the direction of the parent. [Right-to-left interfaces](#right-to-left-interfaces) lists what mirrors. |
| `language` | string | inherited | The BCP 47 tag of the language of the text of the node and of every node inside it, such as `'ar'` or `'hi'`. The shaper uses it to pick the letter forms a language prefers. It defaults to the current language of [`haylen.localization`](localization.md). |
| `theme` | string | inherited | The name of a registered theme that the node and every node inside it draw with, whatever the active theme, such as a light card in a dark screen. A name that is not registered stops the app with `The UI has no theme named "<name>".` when the node draws. |
| `style` | table | none | Values that replace those of the theme for the node and every node inside it, as [styles](#styles) describes. Setting it again replaces the whole style, and an empty table goes back to the theme. |
| `cursor` | string | none | The shape of the mouse cursor over the node: `'default'`, `'arrow'`, `'iBeam'`, `'crosshair'`, `'pointingHand'`, `'resizeHorizontal'`, `'resizeVertical'`, `'resizeDiagonalDown'`, `'resizeDiagonalUp'`, `'resizeAll'` or `'notAllowed'`. The innermost node under the pointer with a cursor wins. |
| `accessibilityLabel` | text | none | What screen readers say the node is, as [screen readers](#screen-readers) describes. |
| `accessibilityHint` | text | the tooltip | What screen readers say the node does, after a pause. |
| `accessibilityRole` | string | from the kind | What screen readers say the kind of the node is. |
| `accessibilityHidden` | boolean | `false` | Leaves the node and every node inside it out of what screen readers read. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    align = 'start',
    width = 900,
    ui.button{text = 'Back', tooltip = 'Return to the map', cursor = 'pointingHand'},
    ui.spacer{grow = 1},
    ui.image{image = 'ui/preview.png', aspectRatio = 16 / 9, width = 320, margin = {0, 16}},
    ui.button{text = 'Buy', enabled = false, minWidth = 200},
})
```

### Anchors

An anchored node leaves the layout of its parent. It takes no room there and draws over it. It sits against the safe area, the whole visible screen or the bounds of its parent. The GUI root can be anchored too, which places the whole GUI. The app draws under notches, dynamic islands, rounded corners and gesture bars on every platform, and anchors choose where each part of the interface sits.

| Anchor | Place |
| --- | --- |
| `'topLeft'`, `'top'`, `'topRight'` | The top corners and the middle of the top edge, at the measured size. |
| `'left'`, `'center'`, `'right'` | The middle of the left edge, the center and the middle of the right edge. |
| `'bottomLeft'`, `'bottom'`, `'bottomRight'` | The bottom corners and the middle of the bottom edge. |
| `'stretch'` | The whole area. |
| `'stretchTop'`, `'stretchBottom'` | The top or the bottom edge, as wide as the area. |
| `'stretchLeft'`, `'stretchRight'` | The left or the right edge, as tall as the area. |
| `'stretchHorizontal'`, `'stretchVertical'` | A band across the middle, as wide or as tall as the area. |

The property `margin` keeps the node away from the edges of its area. The properties `width` and `height` fix the size along the axes that do not stretch. Anchored nodes follow the safe area when it changes, such as when a phone turns. A node anchored to its parent moves and resizes with it, such as the touch controls in the corners of a play area beside a panel.

An unknown anchor raises `The property "anchor" of a "<kind>" must be "none" or an anchor name such as "topLeft", "center" or "stretchHorizontal".`. An `anchorTo` other than `'safe'`, `'screen'` or `'parent'` raises `The property "anchorTo" of a "<kind>" must be "safe", "screen" or "parent".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.stack{
    ui.image{image = 'ui/sky.png', fit = 'cover', anchor = 'stretch', anchorTo = 'screen'},
    ui.button{id = 'pause', icon = 'ui/pause.png', variant = 'icon', anchor = 'topRight', margin = 16},
    ui.panel{anchor = 'stretchBottom', height = 120, margin = {0, 24}, ui.label{text = 'Wood 12'}},
    ui.label{text = 'Day 3', font = 'heading', anchor = 'top', margin = {24, 0}},
}, {placement = 'screen'})

ui.mount(ui.stack{width = 1200, height = 700,
    ui.playArea{id = 'stage', align = 'stretch'},
    ui.touchButton{action = 'jump', text = 'Jump', touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = 24},
})
```

### Scroll bars

Every area that scrolls keeps its scroll bar in a lane of its own along its end edge. The bar never covers the content and never touches it. These areas scroll: `scroll`, `collection`, the text of `textArea`, the popups of `combo`, `menuButton`, `contextMenu`, `popover` and `colorField`, the body of `dialog`, and the windows of [`haylen.imgui`](imgui.md) with the debug overlay. For content that scrolls up and down, the end edge is the right edge, or the left one in a right-to-left interface. For content that scrolls sideways, it is the bottom edge.

| Metric | Default | Part of the lane |
| --- | --- | --- |
| `scrollbarGap` | `8` | The space between the content and the bar. It never spans fewer than 4 points of the screen, whatever the scale mode, the factor of `ui.setScale` or the scaling of the design area. So on a small window or with a small factor, it grows in UI units until it spans 4 points. With the built-in themes, it spans 6 points in the physical scale mode. |
| `scrollbarSize` | `10` | The thickness of the bar. |
| `scrollbarInset` | `4` | The space between the bar and the edge of its area. The ends of the bar keep it from the other edges, and from the rounded corners of dialogs, popups and text areas. |

The lane takes room from the content only while the content is longer than its area, so content that fits keeps the whole width. Padding on the side of the bar counts toward the lane. So the content of a dialog, or of a collection with enough padding, keeps its width when the bar shows. A popup grows by the part of the lane its padding leaves. A popup opens below the control that opened it, or above it when the room below is too short for its content and the room above is larger. It never leaves the display, so its whole bar shows. A horizontal `scroll` and a horizontal `collection` add the lane to their height, so their content keeps its height.

The layout never changes back and forth as the bar comes and goes. A scroll and a dialog decide from the size of their content at the whole width. A popup decides before it opens. A collection and a text area take the lane from the frame after their content grew longer than the view, and show the bar from then on.

A `scroll` and a `collection` also fade their content into the theme color `scrollFade` along each edge where the content continues past the area. The fade is as long as the theme metric `scrollFade`, and it shows whether the bar shows or not. It grows in as the content moves away from that end, and it is gone once the content rests at that end. A theme sets the metric to `0` to draw no fade.

The thumb runs in a groove along the whole bar, as wide as the thumb and with the same round ends. So the bar shows how long the content is and where the view lies in it. The colors `scrollbar` and `scrollbarHover` paint the thumb, and `scrollbarTrack` paints the groove, which a transparent color leaves out. The [surfaces](#theme-surfaces) `scrollbar`, `scrollbarHover` and `scrollbarTrack` paint the thumb and its groove with images in the same place. A `style` changes all of them for one node and the nodes inside it.

```lua
local ui = require('haylen.ui')

local lines = {}
for index = 1, 60 do
    lines[index] = ui.label{text = 'Entry ' .. index}
end
ui.mount(ui.scroll{height = 400, style = {metrics = {scrollbarGap = 12, scrollbarSize = 14, scrollFade = 0}}, ui.column{children = lines}})
```

### Scale

A GUI lays out in UI units. The scale mode decides how large a UI unit is, and the scale factor multiplies it. The [UI guide](../ui/ui.md#scale-and-screen-density) explains when to use each mode. Sizes, positions, `gui:bounds` and the values of events stay in design coordinates in both modes. Only the room the UI has changes.

### ui.setScaleMode(mode)

Sets how large the interface draws, from the next frame on. The `app.json` key `ui.scaleMode` sets it at start.

- The mode `'design'` is the default. A UI unit is a design unit, so the UI grows and shrinks with the screen like the rest of the app.
- The mode `'physical'` gives a UI unit the same size on every screen: three quarters of a point. So the default control height of 64 units is 48 points on a phone, a tablet and a desktop. The shorter side of the visible area never holds fewer than 480 UI units, so a layout always has room. A layout that grows with `grow`, `wrap` and `minColumnWidth` follows the room it gets.

A point is the unit that the platform reports through [`window.dpiScale()`](window.md). It is a point on Apple platforms, a density-independent pixel on Android and a CSS pixel on the web. An unknown mode raises `The UI scale mode "<mode>" is unknown. It is "design" or "physical".`.

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

Multiplies the size of the interface by a factor from 0.25 to 4, in both modes, such as a size setting of the player. The `app.json` key `ui.scale` sets it at start. A factor out of range raises `The UI scale must be from 0.25 to 4.`.

```lua
local ui = require('haylen.ui')

ui.setScale(1.25)
```

### ui.scale()

Returns three numbers about the last frame. The first is the factor that `ui.setScale` set. The second is the design units that one UI unit spans, which the physical mode takes from the density of the screen. The third is the points of the screen that one UI unit spans.

```lua
local ui = require('haylen.ui')

local factor, scale, points = ui.scale()
print('A control of 64 units spans ' .. 64 * scale .. ' design units and ' .. 64 * points .. ' points.')
```

### The safe area

The safe area is the part of the screen that notches, rounded corners and system bars leave free. A GUI mounted with the default `placement = 'safe'` lays out its root inside it. A GUI mounted with `placement = 'screen'` covers the whole visible screen, and a [`safeArea`](#uisafeareaproperties) node or an [anchor](#anchors) keeps its controls inside the safe area. The functions [`viewport.safeRect`](viewport.md) and [`viewport.setSafeAreaSimulation`](viewport.md#viewportsetsafeareasimulationvalue) read and simulate it.

### ui.setSafeAreaVisible(visible)

Shows or hides the debug view of the safe area. The view shades the screen outside the safe area and outlines it. It prints the insets by name in the widest shaded band that holds them, so they cover no interface. In the top or bottom band they take one line, and in a side band one line per edge. When no band holds them, they show on a dark pill at the bottom of the screen. The option `debug.showSafeArea` of `app.json` turns the view on at start. Together with [`viewport.setSafeAreaSimulation`](viewport.md#viewportsetsafeareasimulationvalue), it checks a layout against notches, rounded corners and gesture bars on a desktop.

```lua
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

viewport.setSafeAreaSimulation('iphoneDynamicIsland')
ui.setSafeAreaVisible(true)
```

### ui.safeAreaVisible()

Returns `true` while the debug view of the safe area shows.

```lua
local ui = require('haylen.ui')

print(ui.safeAreaVisible())
```

### Right-to-left interfaces

The text layout of the engine shapes every text of every component and orders it for display. So labels, buttons, fields, lists, tables, tooltips and rich text show Arabic, Hebrew, Persian, Urdu, Hindi, Thai and every other script that their font role covers. Each paragraph reads in the direction of its first strong letter. The [text guide](../graphics/text.md#scripts-and-directions) explains shaping and directions.

The direction of the layout is separate from the direction of each text. The call `ui.setDirection('rightToLeft')`, or a node with `direction = 'rightToLeft'`, mirrors the layout inside it:

- Rows place their first child at the right, and grids fill their rows from the right. The values `start` and `end` of `align`, `justify` and `textAlign` name the right and the left.
- Check boxes, radio buttons and toggles put their box on the right of their text, and buttons put their icon on the right. List rows, form fields, settings rows, tabs, accordions, trees, tables and alerts start from the right.
- Sliders, range sliders, progress bars and toggles fill from the right. The left and right arrow keys move sliders, steppers, segmented controls, carousels and splitters the way they point on screen.
- Steppers and carousels swap their ends, so the arrow that points left moves forward. The closed arrows of trees and accordions point left, and the arrow of a combo stands at its left. Menus, combo lists, popovers and context menus open from the right edge of their button.
- Text fields line their text up on the right. In every direction, the caret stands on the side of the letter it follows, and the left and right arrow keys move it the way they point on screen. A selection across a change of direction covers every run it spans.

Some things never mirror. Anchors, `padding` given as four sides and the properties `focusLeft` and `focusRight` name fixed sides. Images and icons draw as they are. A node with `direction = 'leftToRight'` inside a right-to-left UI keeps its own direction, for a phone number, a code or a game board.

```lua
local ui = require('haylen.ui')

ui.setDirection('rightToLeft')
ui.mount(ui.card{gap = 16,
    ui.row{gap = 12, ui.checkbox{text = 'تذكرني', checked = true}, ui.button{text = 'دخول', variant = 'primary'}},
    ui.slider{value = 0.3},
    ui.row{direction = 'leftToRight', gap = 8, ui.label{text = '+1 555 0100'}},
})
```

### ui.setDirection(direction)

Sets the direction of the whole UI, from the next frame on:

- The value `'leftToRight'` is the default.
- The value `'rightToLeft'` mirrors every GUI.
- The value `'auto'` follows the direction that the current language of [`haylen.localization`](localization.md#localizationdirectionlanguage) declares. So picking Arabic mirrors every GUI, and picking English again restores it.

An unknown name raises `bad argument #1 to 'setDirection' (unknown value '<name>')`.

```lua
local localization = require('haylen.localization')
local ui = require('haylen.ui')

ui.setDirection('auto')
localization.setLanguage('ar')
```

### ui.direction()

Returns two strings. The first is the direction set for the whole UI: `'leftToRight'`, `'rightToLeft'` or `'auto'`. The second is the direction the UI draws in: `'leftToRight'` or `'rightToLeft'`. An automatic direction takes it from the language at the start of every frame.

```lua
local ui = require('haylen.ui')

local set, drawn = ui.direction()
print(set, drawn)
```

## Themes

A theme holds every color, metric, font and surface the components use. The engine ships the themes `dark`, which is active at start, and `light`. Each has a high contrast theme, `darkHighContrast` and `lightHighContrast`, which the UI draws instead while the system asks for higher contrast. An app adds its own themes from JSON files or Lua tables. The active theme also styles the windows of [`haylen.imgui`](imgui.md). The [UI guide](../ui/ui.md#give-it-a-look) explains how to design a theme.

### ui.setTheme(name)

Switches every GUI and every [`haylen.imgui`](imgui.md) window to the registered theme `name`. An unknown name raises `The UI has no theme named "<name>".`.

```lua
local ui = require('haylen.ui')

ui.setTheme('light')
```

### ui.theme()

Returns the name of the theme that the app chose. It keeps returning it while the UI draws the high contrast theme of that theme.

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

Reads a [theme file](#theme-files) from the package assets and registers the theme under its name. The theme starts as a copy of the registered theme `base`, which is `'dark'` by default, so the file lists only what it changes. The function also registers the fonts that `fontFiles` lists. A theme with the name of a registered theme replaces it, and when that theme is active, the change shows at once. It returns the theme name and does not switch to it.

These errors can happen:

- An unknown base raises `The UI has no theme named "<base>" to start from.`.
- A font role that names an unknown font raises `The theme "<name>" uses the font "<font>", which is neither registered nor listed in "fontFiles".`.
- A missing file raises the error of the asset system, and an invalid file raises the errors of [theme files](#theme-files).

```lua
local ui = require('haylen.ui')

local name = ui.loadTheme('themes/wood.json', 'light')
ui.setTheme(name)
```

### ui.addTheme(definition, base)

Registers a theme from a Lua table in the [theme file](#theme-files) format, the way `ui.loadTheme` registers a file. The theme starts as a copy of the registered theme `base`, which is `'dark'` by default. It returns the theme name and does not switch to it. It raises the errors of `ui.loadTheme`, and a `definition` that is not a table raises an argument error.

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

### Theme files

A theme file is a JSON object in the package assets. It starts from a base theme and lists only what it changes. The same object, as a Lua table, goes to `ui.addTheme`.

| Key | Type | Meaning |
| --- | --- | --- |
| `name` | string | The name of the theme. It is required and not empty. |
| `colors` | object | [Color roles](#theme-colors) mapped to colors. |
| `metrics` | object | [Metrics](#theme-metrics) mapped to non-negative numbers in design units. |
| `fonts` | object | [Font roles](#theme-fonts) mapped to `{"font": name, "size": number, "bold": boolean, "italic": boolean, "ligatures": boolean}`, where every key is optional and the size is the size of the em square. |
| `fontFiles` | object | Font names mapped to TrueType or OpenType files in the package assets. The function `ui.loadTheme` registers them, unless a font with that name already exists. |
| `surfaces` | object | [Surfaces](#theme-surfaces) mapped to nine-slice images, or to `null` to go back to flat colors. |
| `imageFilter` | string | The texture filter of the pictures components show: `'nearest'`, the default, for pixel art, or `'linear'` for painted art that the screen scales. It covers the pictures of `image`, `icon`, `imageButton`, `avatar`, `circularProgress` and the touch controls. A switch to a theme with another filter loads the pictures again. |
| `highContrast` | string or `false` | The name of the theme the UI draws while the system asks for higher contrast, or `false` to keep this theme. A theme that leaves it out takes the one of its base. So themes built on `dark` and `light` switch to `darkHighContrast` and `lightHighContrast`. |

Unknown keys, roles, metrics and surfaces are errors, so a typo never goes unnoticed. A theme raises these errors:

- `A theme must be a JSON object.` and `Unknown key "<key>" in the theme.`
- `A theme needs a name.`
- `The image filter of the theme must be "nearest" or "linear".`
- `The high contrast theme of a theme must be the name of a theme or "false".`
- `The section "<name>" of the theme must be an object.`
- `The theme has no color role named "<name>".`, `The theme has no metric named "<name>".` and `The theme has no surface named "<name>".`
- `The theme color "<name>" must be a color such as "#FF2E7D32".` and `The theme metric "<name>" must be a non-negative number.`
- `The theme font file "<name>" must be a path.`
- `The theme font "<name>" must be a known role with an object value.` and `Unknown key "<key>" in the theme font "<name>".`
- `The theme font "<name>" must name its font with a string.`, `The size of the theme font "<name>" must be a non-negative number.`, `The theme font "<name>" must set "bold" to "true" or "false".` and the same for `italic` and `ligatures`.
- `A theme font needs a font name and a positive size.` for a role with an empty font name or a size of zero.
- The errors of [surface images](#theme-surfaces).

```json
{
    "name": "wood",
    "colors": {"accent": "#FF8B5A2B", "text": "#FF1B1E2B"},
    "metrics": {"controlHeight": 80, "panelPadding": 36},
    "fontFiles": {"pixel": "fonts/pixel.ttf"},
    "fonts": {"title": {"font": "pixel", "size": 72}, "body": {"size": 34}},
    "imageFilter": "linear",
    "highContrast": false,
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

### Styles

A style replaces values of the theme for one node and every node inside it. Any value a component reads can change for one node without a theme of its own: colors, metrics, fonts and surfaces. Set it with the [common property](#common-properties) `style`. It takes the sections of a [theme file](#theme-files):

- The section `colors` maps color roles to colors.
- The section `metrics` maps metrics to numbers.
- The section `fonts` maps font roles to tables, which set only the keys they list.
- The section `surfaces` maps surfaces to images, which load in the background the first time they draw, while the flat colors stand in. A surface set to `false` paints with flat colors, whatever the theme has.

Values that a style leaves out come from the style or theme around the node, so styles nest. A color of a style is either a color of its own, such as `'#FF2E9E62'`, or the name of a [theme color role](#theme-colors), such as `'accent'` or `'textMuted'`. A role takes its color each frame from the theme of the node, when the node gives a `theme`, or from the theme and styles around it. So a styled node follows `ui.setTheme`, higher contrast and reduced transparency without being built again. A role never comes from the colors of the same style, so `{accent = '#FFFF0000', border = 'accent'}` gives the border the accent of the theme around the node.

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
    -- A selected tab in the accent of whatever theme is active.
    ui.button{text = 'Home', variant = 'toolbar', style = {colors = {text = 'accentText'}}},
})
```

Each error of a style starts with the property that holds it:

- `The property "style" of a "<kind>" must be a table with "colors", "metrics", "fonts" or "surfaces".`
- `Unknown key "<key>" in the property "style" of a "<kind>".`
- `The property "style" of a "<kind>" must hold a table in "<section>".`
- `The property "style" of a "<kind>" has no color role named "<name>".`, and the same for metrics, font roles and surfaces.
- `The color "<name>" of the property "style" of a "<kind>" must be a color such as "#FF2E7D32".`, for a value that starts with `#` or is not a string.
- `The color "<name>" of the property "style" of a "<kind>" must name a theme color such as "accent" or "textMuted".`, for an unknown role.
- `The metric "<name>" of the property "style" of a "<kind>" must be a non-negative number.`
- `The font "<name>" of the property "style" of a "<kind>" must be a table.`, `... must name its font with a string.` and `... must set "bold" to "true" or "false".`, and the same for `italic` and `ligatures`.
- `The size of the font "<name>" of the property "style" of a "<kind>" must be positive.`
- `The image of the surface "<name>" of the property "style" of a "<kind>" must be a path.`

The other values of a surface image are checked once its image has loaded, with the errors of theme surfaces.

### Theme colors

Colors are `'#RRGGBB'` or `'#AARRGGBB'` strings. The accent and the four tones `success`, `warning`, `danger` and `information` each come in four colors. They are a fill, the ink written on that fill, a subtle background, and a text color that reads on the window. The table lists the colors of `dark` and `light`.

| Role | Dark | Light | Used for |
| --- | --- | --- | --- |
| `window` | `#FF1B1E2B` | `#FFF4F5F9` | The background of ImGui windows and the outline of progress text. |
| `panel` | `#FF232739` | `#FFFFFFFF` | The fill of panels and dialogs. |
| `raised` | `#FF2C3147` | `#FFFFFFFF` | The fill of cards, buttons, fields and chips. |
| `tooltip` | `#FF0F111A` | `#FF1B1E2B` | The background of tooltips, toasts and the counts of slots. |
| `overlay` | `#A0000000` | `#66000000` | The backdrop behind dialogs and the base of touch controls. |
| `shadow` | `#66000000` | `#291B1E2B` | The soft shadow under dialogs, windows, toasts, tooltips and the popups of combos, menus, popovers and color fields. |
| `hover` | `#14FFFFFF` | `#0F000000` | The layer over hovered controls and rows. |
| `pressed` | `#24FFFFFF` | `#1F000000` | The layer over pressed controls. |
| `selection` | `#404C7DFF` | `#334C7DFF` | Selected rows, checked buttons and selected text. |
| `focus` | `#FF7AA2FF` | `#FF3A66E0` | The border of focused fields and the focus ring of the navigation target. |
| `caret` | `#FF7AA2FF` | `#FF3A66E0` | The text cursor of fields. |
| `border` | `#FF3A4058` | `#FFD6D9E4` | Borders and dividers. |
| `borderStrong` | `#FF525A7A` | `#FFB3B8CC` | Strong borders, hovered fields and empty slider tracks. |
| `scrollbar` | `#FF3A4058` | `#FFC9CDDB` | The thumb of scroll bars. |
| `scrollbarHover` | `#FF525A7A` | `#FFA9AEC2` | The thumb of hovered or held scroll bars. |
| `scrollbarTrack` | `#4D000000` | `#14000000` | The groove that the thumb of a scroll bar runs in, a shade of the surface under it. |
| `track` | `#FF3A4058` | `#FFCDD1DD` | The track of a switch that is off, and the color a `scrollbarTrack` image with `colorize` takes. |
| `knob` | `#FFFFFFFF` | `#FFFFFFFF` | The knob of switches. |
| `text` | `#FFE8EAF2` | `#FF1B1E2B` | The main text. |
| `textMuted` | `#FFA3A8BF` | `#FF5C6380` | Captions, placeholders, help lines and inactive tabs. |
| `textDisabled` | `#FF6A7090` | `#FFA3A8BF` | The text of disabled ImGui items. |
| `onTooltip` | `#FFE8EAF2` | `#FFF4F5F9` | The text of tooltips, toasts and the counts of slots. |
| `accent` | `#FF4C7DFF` | `#FF3A66E0` | The accent fill of primary buttons, checked boxes, slider fills and the selected tab. |
| `accentHover` | `#FF6690FF` | `#FF4C7DFF` | The hovered accent fill and hovered links. |
| `accentStrong` | `#FF3A66E0` | `#FF2C52C0` | The pressed accent fill. |
| `onAccent` | `#FFFFFFFF` | `#FFFFFFFF` | Text and marks on the accent fill. |
| `accentBackground` | `#264C7DFF` | `#1F3A66E0` | The subtle accent background of selected chips and avatars. |
| `accentText` | `#FF8FB0FF` | `#FF2C52C0` | Accent text, such as links. |
| `success` | `#FF3DBE7A` | `#FF2E9E62` | The fill of the success tone. |
| `onSuccess` | `#FFFFFFFF` | `#FFFFFFFF` | The ink on the success fill. |
| `successBackground` | `#263DBE7A` | `#1F2E9E62` | The subtle background of the success tone. |
| `successText` | `#FF6FDCA0` | `#FF1F7A49` | The text of the success tone. |
| `warning` | `#FFF2B23A` | `#FFD9941C` | The fill of the warning tone. |
| `onWarning` | `#FF1B1E2B` | `#FFFFFFFF` | The ink on the warning fill. |
| `warningBackground` | `#26F2B23A` | `#1FD9941C` | The subtle background of the warning tone. |
| `warningText` | `#FFF7CB70` | `#FF9A6508` | The text of the warning tone. |
| `danger` | `#FFE5534B` | `#FFD0433B` | The fill of the danger tone and of destructive buttons. |
| `dangerHover` | `#FFEE6B64` | `#FFE5534B` | Hovered destructive buttons. |
| `dangerStrong` | `#FFC8423B` | `#FFB0352E` | Pressed destructive buttons. |
| `onDanger` | `#FFFFFFFF` | `#FFFFFFFF` | The ink on the danger fill. |
| `dangerBackground` | `#26E5534B` | `#1FD0433B` | The subtle background of the danger tone. |
| `dangerText` | `#FFFF8A84` | `#FFA8322B` | The text of the danger tone and of form errors. |
| `information` | `#FF3AA8E0` | `#FF2A8CC0` | The fill of the information tone. |
| `onInformation` | `#FFFFFFFF` | `#FFFFFFFF` | The ink on the information fill. |
| `informationBackground` | `#263AA8E0` | `#1F2A8CC0` | The subtle background of the information tone. |
| `informationText` | `#FF7FCBF2` | `#FF1D6A93` | The text of the information tone. |
| `scrollFade` | `#CC0F111A` | `#CCF4F5F9` | The color that the content of a `scroll` or a `collection` fades into along an edge where it continues past the area. |

### ui.themeColor(role)

Returns a [color role](#theme-colors) of the active theme as a `haylen.Color` of [`haylen.math`](math.md). It returns what the UI draws, so it follows higher contrast and reduced transparency. An unknown role raises `The theme has no color role named "<role>".`.

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

### Theme metrics

Metrics size the parts of the components. Both built-in themes share these values, in design units unless the table says seconds or a count.

| Metric | Default | Used for |
| --- | --- | --- |
| `controlHeight` | `64` | The height of buttons, fields, combos, sliders and the tab strip, and the least height of the rows of check boxes, radio buttons and toggles. So every control is a touch target of the same size, the size that the accessibility audits of the platforms ask for at the scales of phones. |
| `controlRadius` | `12` | The corner radius of controls, panels and windows. |
| `controlPaddingX` | `24` | The horizontal padding inside buttons, fields, alerts and toasts. |
| `controlPaddingY` | `12` | The vertical padding of settings rows, alerts, toasts and text areas. |
| `itemSpacing` | `16` | The default gap of columns, rows and grids, and the space between the parts of a component. |
| `panelPadding` | `28` | The default padding of cards, panels and dialogs, and the margin of toasts. |
| `borderWidth` | `2` | The width of borders and dividers. |
| `focusWidth` | `3` | The thickness of the focus ring and of the line under the selected tab. |
| `scrollbarSize` | `10` | The thickness of [scroll bars](#scroll-bars). |
| `scrollbarGap` | `8` | The space between scrolling content and its scroll bar, never fewer than 4 points of the screen. |
| `scrollbarInset` | `4` | The space between a scroll bar and the edges and rounded corners of its area. |
| `iconSize` | `36` | The size of button icons, row pictures and icons without a size, and the indent of tree levels. |
| `choiceSize` | `36` | The size of check boxes and radio marks. |
| `sliderTrackHeight` | `10` | The height of the slider track. |
| `sliderKnobSize` | `34` | The size of the slider knob. |
| `toggleWidth` | `72` | The width of the toggle switch. |
| `toggleHeight` | `38` | The height of the toggle switch. |
| `progressHeight` | `22` | The smallest height of progress bars. |
| `badgePaddingX` | `14` | The horizontal padding of badges. |
| `badgePaddingY` | `4` | The vertical padding of badges. |
| `tabPaddingX` | `24` | The horizontal padding of each tab, and of the labels of segmented controls with room. |
| `listRowHeight` | `64` | The height of list, tree and table rows, and the smallest height of settings rows. |
| `dialogWidth` | `760` | The width of dialogs. |
| `toastWidth` | `560` | The widest a toast grows at a text scale of 1, and the width of the toasts of a deck. |
| `tooltipWidth` | `520` | The width at which tooltips wrap. |
| `settingsLabelWidth` | `420` | The width of the label column of settings rows. |
| `caretWidth` | `2` | The width of the text cursor in fields and [`haylen.imgui`](imgui.md) inputs. |
| `circularProgressSize` | `72` | The size of circular progress indicators without a size. |
| `circularProgressThickness` | `8` | The width of the ring of circular progress indicators without a thickness. |
| `slotSize` | `96` | The size of the slots of slot grids without a slot size. |
| `windowTitleHeight` | `56` | The height of the title bar of windows. |
| `pageIndicatorSize` | `14` | The size of the page dots of carousels. |
| `toggleKnobInset` | `4` | The space between the knob of a switch and the edge of its track. |
| `transitionDuration` | `0.15` | The seconds the transitions of the UI take: the knob of a switch, the fades of dialogs and their backdrops, and the fades and moves of toasts. |
| `toastLimit` | `3` | The notices one list of toasts shows at once, while the others wait, and the notices a closed deck shows, while older ones hide behind them. The value `0` shows them all. |
| `toastRadius` | `999` | The corner radius of toasts, at most half the height of a toast of one line. So the default rounds every toast of one line into a pill. |
| `toastPeek` | `14` | How far each notice behind the front of a closed deck of toasts peeks out past the notice in front of it. |
| `toneBarWidth` | `6` | The width of the tone bar of alerts. |
| `panelRadius` | `16` | The corner radius of panels, cards, dialogs, windows, toasts and popups. The rows of a popup, the highlighted rows of a list or a collection in a card, and the title bar of a window follow it inside. |
| `contentSpacing` | `12` | The space between the box, the switch or the icon of a control and its text. |
| `focusGap` | `3` | The space between a control and its focus ring. |
| `disabledOpacity` | `0.5` | The opacity of disabled nodes. |
| `tooltipDelay` | `0.5` | The seconds the pointer rests on a node before its tooltip shows. |
| `longPressDuration` | `0.5` | The seconds a finger holds a context menu before it opens, and holds a row, a slot or an item before it lifts it. |
| `shadowSize` | `24` | How far the soft shadow of floating surfaces reaches past their edge. The shadow starts at half its color along the edge. A toast casts it at a share of this reach, nearer and lighter. The value `0` turns shadows off. |
| `shadowOffset` | `6` | How far below its surface a shadow falls. |
| `chipHeight` | `48` | The height of chips. |
| `chipRadius` | `999` | The corner radius of chips, at most half their height. So the default rounds every chip into a pill. |
| `strokeWidth` | `2` | The width of the lines of drawn icons, such as the cross of a removable chip and the lens of a filter field. |
| `rowPadding` | `16` | The padding at the start and end of the rows of lists, trees, tables, popups and accordions. |
| `checkRadius` | `8` | The corner radius of check boxes. |
| `menuPadding` | `8` | The space between the edge of the popup of a combo, a menu button or a context menu and its rows. |
| `splitterSize` | `10` | The thickness of the handle of splitters. |
| `scrollLine` | `64` | The distance that one step of a mouse wheel scrolls a `scroll`, a `collection`, a `textArea`, a `dialog` and a popup. A trackpad scrolls them by the points it moves instead. |
| `sliderValueWidth` | `96` | The least room a slider or a range slider keeps for the value it shows. So the tracks of the sliders of one column line up, whatever their ends. |
| `scrollFade` | `32` | The length of the fade along an edge of a `scroll` or a `collection` where the content continues past the area. The value `0` draws none. |
| `readingWidth` | `880` | The widest line of the title and the message of an empty state, which keeps a long message easy to read on a wide screen. |
| `cellRadius` | `12` | The corner radius of the backgrounds of interactive collection cells and of the focus ring around them. |
| `refreshDistance` | `120` | How far a pull at the start of a refreshable collection goes before it refreshes. |

### ui.themeMetric(name)

Returns a [metric](#theme-metrics) of the active theme. An unknown metric raises `The theme has no metric named "<name>".`.

```lua
local ui = require('haylen.ui')

local rowHeight = ui.themeMetric('listRowHeight')
ui.mount(ui.list{height = rowHeight * 5, items = {{id = 'first', text = 'First'}}})
```

### Theme fonts

A font role names a registered font and a size. Every role starts with the built-in font `default` in its regular style. The role `monospace` starts with the built-in font `monospace`, Space Mono, whose characters all take the same width. So code, tables of numbers and clocks line up in columns and keep their width as their digits change. Fonts are sized when drawn, so one registered font serves every role and size.

A role that sets `bold` or `italic` draws with that face of its font, when the font is a family that has it, and with its regular face otherwise. A [`richText`](#uirichtextproperties) of that role starts in that style, as if inside `[b]` or `[i]`.

A role joins letters such as f and i into the ligatures of its font, as prose takes them. A role with `ligatures = false` keeps a glyph for every letter. The `monospace` role does that, so code keeps its letters, and a `style` can do it for one node, such as a title that names a function or a list of keys.

The size of a role is the size of its em square in UI units. The em square is the square that the designer of a font draws every letter in, and it is what a font size means in every module of the engine and in design tools. So two fonts at one size draw their letters at one scale, whatever their ascents and descents, and a type scale works with any font without converting its sizes. A line of a role is as tall as its face from ascent to descent at that size, plus the line gap of the font. So a font with tall ascenders or deep descenders gets taller lines, not smaller letters. The function [`ui.themeFont`](#uithemefontrole) reports that height as `lineHeight`. The components and the `richText` of a role draw at that size, and the windows of [`haylen.imgui`](imgui.md) take the size of `body`.

| Role | Size | Used for |
| --- | --- | --- |
| `body` | `26` | Labels, fields, list rows and most text. |
| `caption` | `20` | Captions, badges, chips, tooltips and form field labels. |
| `button` | `26` | Button and tab labels, alert titles and avatar initials. |
| `heading` | `32` | Section titles, dialog titles and empty state titles. |
| `title` | `48` | Page header titles. |
| `monospace` | `18` | The text of color fields, code and columns of numbers. |

A font role with an empty font name or a size of zero raises `A theme font needs a font name and a positive size.`. A font name that is neither registered nor listed in `fontFiles` makes `ui.loadTheme` fail.

### ui.addFont(name, source)

Registers a font under `name`. The `source` is a TrueType or OpenType file of the package assets, or a `FontFamily` or a `Font` of [`haylen.graphics`](graphics.md). Theme font roles, `imgui.pushFont` and the `[font=name]` tags of `ui.richText` refer to fonts by this name. The built-in fonts are `default`, the default font of the engine, and `monospace`.

A family gives every text component of the roles that name it its faces and fallback fonts:

- Labels, buttons and the other components draw with its regular face. They draw with its bold, italic or bold italic face when the role asks for that style and the family has that face.
- Every character that a face lacks draws from the fallback fonts, such as Japanese text from a font of Chinese, Japanese and Korean characters. It draws at the size of the em square of the face.
- A `ui.richText` draws with every face and fallback of the family, mono faces included.
- A theme role can name a family whose regular face is a TrueType or OpenType font, and the components take the TrueType faces and fallbacks of the family.
- A bitmap font, or a family with a bitmap regular face, serves only `ui.richText` and `[font=name]` tags.

A name that is already registered raises `The UI already has a font named "<name>".`. A file that is not a font raises `The font "<name>" is not a TrueType or OpenType font.`.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

ui.addFont('serif', 'fonts/serif.ttf')
ui.addFont('story', graphics.newFontFamily({regular = assets.font('fonts/serif.ttf'), bold = assets.font('fonts/serif-bold.ttf'), fallbacks = {assets.font('fonts/cjk.ttf')}}))
ui.addFont('pixel', assets.font('fonts/pixel.fnt', {filter = 'nearest'}))

-- Labels draw Japanese from the fallback, and headings draw with the bold face.
ui.setTheme(ui.addTheme({name = 'story', fonts = {body = {font = 'story'}, heading = {font = 'story', bold = true}}}))
ui.mount(ui.column{ui.label{text = 'Chapter 1', font = 'heading'}, ui.label{text = '灯台守の物語'}})
```

### ui.themeFont(role)

Returns a [font role](#theme-fonts) of the active theme as a table. The field `font` is the registered font name, and `size` is the size of its em square in UI units. The field `lineHeight` is the height of a line of the role at that size. The fields `bold` and `italic` are the style of the face the role draws with, and `ligatures` tells whether its letters join into the ligatures of the font. An unknown role raises `The theme has no font role named "<role>".`.

```lua
local ui = require('haylen.ui')

local title = ui.themeFont('title')
print(title.font, title.size, title.lineHeight, title.bold, title.italic, title.ligatures)
```

### Theme surfaces

A surface paints a part of a component with a nine-slice image instead of flat colors, such as the wooden panels and ribbons of a textured game. A nine-slice image keeps its corners at their size times `scale`, while its edges and its center stretch or tile. When the bounds are too small for both borders, the borders shrink together. The built-in themes have no surfaces. A hover or pressed surface that a theme leaves out falls back to the normal surface of the same button.

| Surface | Used by |
| --- | --- |
| `panel` | `panel`. |
| `card` | `card`. |
| `dialog` | `dialog`. |
| `tooltip` | The tooltips of every node. The text stays inside the `padding` of the surface. |
| `toast` | `toast`. The text and the tone bar stay inside the `padding` of the surface, where the bar is a strip with round ends. |
| `banner` | `pageHeader` with `banner = true`. |
| `button`, `buttonHover`, `buttonPressed` | Default, toolbar and icon buttons, menu buttons, popovers, dialog buttons and the buttons of number fields. |
| `buttonPrimary`, `buttonPrimaryHover`, `buttonPrimaryPressed` | Primary buttons. |
| `buttonDestructive`, `buttonDestructiveHover`, `buttonDestructivePressed` | Destructive buttons. |
| `field`, `fieldFocused` | Text fields, secret fields, text areas, filter fields, number fields, combos, color fields, steppers and key captures. |
| `check`, `checkChecked` | Check boxes. |
| `radio`, `radioSelected` | The marks of radio groups, empty and selected. The selected mark takes the `accent` color with `colorize`. |
| `track`, `trackFill`, `knob` | Toggles, sliders, range sliders and progress bars. The fill of toggles, sliders and progress bars stays inside the `padding` of the `track` surface, so a framed bar keeps its frame. A toggle fills its groove as its knob slides on. |
| `stickBase`, `stickKnob` | The ring and the knob of touch sticks. |
| `touchButton`, `touchButtonPressed` | Touch buttons. |
| `tab`, `tabSelected` | Tabs. |
| `chip`, `chipSelected` | Chips. |
| `badge` | Badges. |
| `segment`, `segmentSelected` | The strip of segmented controls and their selected segment. |
| `menu` | The popups of combos, menu buttons, context menus, popovers and color fields. |
| `window` | Windows. With a `titleBar` image, the window starts behind the middle of its title bar, so the bar caps it and no corner of the window shows around the ends of the bar. |
| `titleBar` | The title bar across the top of windows, and the band behind the title of dialogs across the content of the dialog, which only an image draws. The title stays inside the `padding` of the surface. |
| `close`, `closeHover`, `closePressed` | The close button of windows and toasts, in place of the cross, drawn over the whole button. The `danger` color fills it with `colorize`. |
| `slot`, `slotHighlighted` | The slots of slot grids, and the selected or hovered slot. |
| `cell`, `cellHover`, `cellPressed`, `cellSelected` | The backgrounds of interactive collection cells: always, while hovered, while pressed and while selected. The state surfaces also mark the rows of lists, trees, tables, accordions and popups. Without images, the state surfaces paint the `hover`, `pressed` and `selection` colors, and `cell` paints nothing. |
| `scrollbar`, `scrollbarHover` | The thumb of [scroll bars](#scroll-bars), and the thumb while hovered or held, which falls back to `scrollbar`. |
| `scrollbarTrack` | The groove that the thumb of a scroll bar runs in, which the color `scrollbarTrack` paints without an image. |
| `focusRing` | The frame around the control that has the focus, in place of the line of the focus ring. It lies where the ring runs, `focusGap` and `focusWidth` away from the control. |

A surface image is an object with these keys. Insets of four numbers run clockwise from the top: top, right, bottom and left.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `image` | string | required | The texture path relative to the package `content/` folder, or an SVG document. The UI rasters an SVG document for the pixels its borders cover on the screen, at every scale and density, so its corners stay sharp. The regions of an SVG document are in its own units, `source` defaults to the whole document, and it draws with the `imageFilter` of the theme instead of `filter`. |
| `source` | `[x, y, width, height]` | whole texture | The region of the texture that holds the frame. |
| `slice` | insets | `0` | The border sizes cut from `source`, in texture pixels, as one, two or four numbers. Without it, the whole region is the center and stretches to the bounds, which suits round images. |
| `pieces` | list of nine `[x, y, width, height]` | none | The nine regions given one by one instead of `source` and `slice`, for art that ships as separate pieces. They go in reading order: top left, top, top right, left, center, right, bottom left, bottom and bottom right. |
| `scale` | number above 0 | `1` | The scale of the borders on screen. |
| `padding` | insets | `0` | The space in design units between the edge of the image and the content. It adds to the padding of the component. |
| `tint` | color | `'#FFFFFFFF'` | A color multiplied with the image. |
| `colorize` | boolean | `false` | Also multiplies the image with the color the component would fill the area with, such as the tone of a progress bar or the accent of a slider. So one light image serves every color. Without it, the image ignores that color. |
| `filter` | string | `'nearest'` | The texture filter, `'nearest'` or `'linear'`. |
| `fill` | string | `'stretch'` | The value `'stretch'` or `'tile'` for the edges and the center. A tiled edge or center that is not empty must be at least 1 design unit wide and tall at `scale`. |

A surface image raises these errors, where `<context>` is `the theme surface "<name>"`:

- `Unknown key "<key>" in <context>.`
- `The image of <context> must be a path.`
- `The filter of <context> must be "nearest" or "linear".` and `The fill of <context> must be "stretch" or "tile".`
- `The "colorize" of <context> must be "true" or "false".`
- `The pieces of <context> must hold nine rectangles.`, `Each piece of <context> must be four numbers: x, y, width and height.` and `The source of <context> must be four numbers: x, y, width and height.`
- `The slice of <context> must be one, two or four numbers.`, and the same for `padding`. Every number of them must be non-negative, as in `The slice of <context> must be a non-negative number.`.
- `The scale of <context> must be a non-negative number.` and `The scale of <context> must be positive.`
- `The tint of <context> must be a color such as "#FF2E7D32".`
- `The tiled edges and center of <context> must each be at least 1 unit wide and tall at its scale.`

Borders of an SVG surface that do not fit inside the document stop the app once the document has loaded, with `The borders of a nine-slice must fit inside its source rectangle, with the left and right borders at most its width and the top and bottom borders at most its height.`.

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

### ui.themeSurface(surface)

Returns the image that the active theme paints a [surface](#theme-surfaces) with, or `nil` when the theme paints it with flat colors. The table holds these fields:

- `slice`, a `haylen.NineSlice` for `graphics2d.drawNineSlice`, and `scale`.
- `padding` as `{top, right, bottom, left}`.
- `tint`, a `haylen.Color`, and `colorize`.
- `vector`, the path of the SVG document, for a surface drawn from one. Its `slice` and `scale` then come from the raster that covers its borders on the screen at this moment, and `slice` is absent while that raster loads.

An unknown surface raises `The theme has no surface named "<surface>".`.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    render = function(self)
        local panel = ui.themeSurface('panel')
        if panel then
            graphics2d.beginScreen()
            graphics2d.drawNineSlice(panel.slice, {100, 100, 400, 240}, panel.tint, {borderScale = panel.scale})
        end
    end,
})
```

### ui.themeImageFilter()

Returns the texture filter that the pictures of components load with in the active theme: `'nearest'` or `'linear'`. The key `imageFilter` of a [theme file](#theme-files) sets it.

```lua
local ui = require('haylen.ui')

ui.setTheme(ui.addTheme({name = 'painted', imageFilter = 'linear'}))
print(ui.themeImageFilter())
```

## Focus and navigation

The focus marks the control that the keyboard, gamepads and TV remotes act on. Buttons, choices, inputs, list rows, slots, play areas and the other interactive parts of a GUI take it. The [UI guide](../ui/ui.md#move-the-focus-with-every-device) explains how the player moves it with each device.

### Navigation actions

The UI moves the focus with these actions of [`haylen.input`](input.md#action-map). An app remaps one by defining an action with the same name in its action map, and the others keep their built-in bindings.

| Action | Built-in bindings | Use |
| --- | --- | --- |
| `uiAccept` | `key:enter`, `key:keypadEnter`, `key:space`, `button:south` | Presses the focused control. |
| `uiCancel` | `key:escape`, `button:east` | Goes back: closes the open popup or dialog, puts back a carried item, or sends `cancel`. |
| `uiLeft`, `uiRight`, `uiUp`, `uiDown` | the arrow keys, the directional pad and the left stick | Moves the focus. |
| `uiMenu` | `key:menu`, `button:north` | Opens the context menu around the focus. |
| `uiFocus` | `button:back` | Moves the focus from a play area to the control of its GUI that last had it, or to its first control, and back. |
| `uiPagePrevious`, `uiPageNext` | `key:pageUp` and `button:leftShoulder`, `key:pageDown` and `button:rightShoulder` | Scrolls the innermost collection around the focus by one view, and moves the focus to the first item of the new view. |
| `uiFirst`, `uiLast` | `key:home`, `key:end` | Moves the focus to the first or the last item of the innermost collection around the focus. |

### How the focus moves

- A direction moves the focus to the nearest control in that direction, and prefers controls in line with the focused one. A node that names a neighbor with `focusLeft`, `focusRight`, `focusUp` or `focusDown` sends it there instead.
- Some controls use left and right themselves while they have the focus: sliders, range sliders, steppers, segmented controls and the page dots of a carousel.
- Tab and Shift Tab walk the controls in the order they draw, also out of a text field being edited. A text field they reach starts editing. While nothing has the focus, Tab goes to the first control of the topmost GUI with controls, and Shift Tab to its last one. So a keyboard alone, as a screen reader user has, enters a GUI that focuses nothing by itself.
- The focus stays inside its GUI, or inside the GUIs that share it with [`sharedFocus`](#guisharedfocus). A dialog, a popover and a menu keep it until they close, and then it returns to where it was.
- A GUI that stops drawing keeps the control that had the focus. That happens to the GUI of a covered scene, or to a GUI with `visible` set to `false`. Once the GUI draws again, the control gets the focus back, unless the focus moved elsewhere meanwhile.
- A `window` belongs to the navigation of its GUI. A move reaches its controls from the rest of the GUI and brings it to the front. Closing it returns the focus to where it was.
- A node with `focusScope = true` keeps the focus while the focus is inside. A move never enters a scope from outside: only `autofocus`, the `focus` command or a click do.
- The property `focusWrap` wraps a move that would leave a node around to its other side, such as from the end of a row of cards back to its first card.
- A scroll brings the focused control into view, and so does a collection. A collection also keeps the focus on an item whose cell the wheel or a finger took out of view. The next direction or accept only brings that item back into view, and the one after it moves on or presses it.
- A collection binds the items around the focused one, so the focus reaches items that do not show yet. The property `focusWrap` of a collection, along its axis, wraps from its last item to its first, even when they do not show.
- The focused node reports `focus`, and the node that had the focus reports `blur`, as long as that node still draws.
- The action `uiCancel` closes the open popup or dialog, or puts back an item carried from a slot grid or a list. Otherwise it sends `cancel` to the innermost node with `focusScope` around the focus. Without one, it sends `cancel` to the root of the GUI that holds the focus, or of the topmost GUI when nothing has the focus. A screen goes back from the `onCancel` handler of its root, as in the example below.
- The first press of a direction while the focus ring is hidden only shows where the focus is. The first accept shows the ring and presses the control.
- A [`playArea`](#uiplayareaproperties) takes the focus like a control: from a click, a touch, Tab, `uiFocus`, a move or the `focus` command. While it has the focus, the directions, accept and menu belong to the game, so they never move the focus or press a control. Cancel, Tab and `uiFocus` stay with the UI.

### The focus ring

The ring shows around the focus once the player navigates, and a click or a touch hides it. On a device without a pointer, such as an Apple TV or an Android TV, it shows from the start. The theme colors it with `focus` and sizes it with the metrics `focusWidth` and `focusGap`. It follows the corners of the control. The theme surface `focusRing` replaces the line with an image, such as brackets at the corners or a glow. The property `focusRing = false` leaves it out of a node that draws the focus itself.

### Presses the UI keeps

A press that the UI answers itself never reaches the actions of the app. These presses read as up in the action map until they are released:

- Every navigation key and button, and the left stick, while a control has the focus.
- The keys and buttons of `uiCancel` and `uiFocus`, and Tab, while a play area has the focus.
- The keys and buttons of `uiCancel` that close a popup, a dialog, a closable window or a carried item, or that end the editing of a control.
- Every key while a text field edits, and every key and button while a `keyCapture` listens.

The functions [`input.keyCaptured`](input.md#inputkeycapturedkey), [`input.gamepadButtonCaptured`](input.md#inputgamepadbuttoncapturedbutton-index) and [`input.gamepadAxisCaptured`](input.md#inputgamepadaxiscapturedaxis-index) tell when a press belongs to the UI. A press that started in the game stays with the game until it is released, even once the focus moved to a control.

On an Apple TV and an Android TV, the player leaves the app by going back from its root screen, as Apple and Google ask. On other screens, [`window.setBackLeavesApp`](window.md#windowsetbackleavesappenabled) keeps that press inside the app, and an open popup or dialog always keeps it.

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

### ui.focused()

Returns a table that tells what holds the keyboard, gamepad and remote focus, or `nil` when no mounted GUI holds it. The field `gui` is the [`Gui`](#gui), and `id` is the id of the focused node, which is `nil` for a node without an id. Inside a cell of a [`collection`](#uicollectionproperties), `id` is the id of the collection and `item` is the id of the item. Then `part` is the part of the template that holds the focus, or `nil` for a cell that takes the focus as a whole.

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
        local focus = ui.focused()
        if focus and focus.gui == menu then
            menu:set('hint', {text = focus.id == 'quit' and 'Leave the island' or 'Start a new run'})
        end
    end,
})
```

### ui.focusOwner()

Returns what holds the keyboard, gamepad and remote focus. The value `'control'` means a control of a GUI. The value `'playArea'` means a [play area](#uiplayareaproperties), whose game gets the directions, accept and menu. The value `'none'` means nothing.

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
        print('The game moves by ' .. dx .. ', ' .. dy .. '.')
    end,
})
```

### ui.clearFocus()

Takes the focus away from every GUI. Call it when gameplay starts, so a key that plays the game never presses a menu control that kept the focus.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.button{id = 'pause', text = 'Pause', align = 'end'})
ui.clearFocus()
```

### ui.focusRingVisible()

Returns `true` while the focus ring shows. It shows once the player navigates with the keyboard, a gamepad or a remote, and always on a device without a pointer, such as a TV. A mouse click or a touch hides it again.

```lua
local ui = require('haylen.ui')

if ui.focusRingVisible() then
    print('The player navigates with keys, a gamepad or a remote.')
end
```

### ui.usingPointer()

Returns `true` while the pointer is over something the interface owns. That is an interactive component, an open dialog, menu or picker, a [`haylen.imgui`](imgui.md) window, or a card, a panel, a touch stick or a touch button. Empty space inside columns, rows and stacks lets the pointer through, even while a mouse button is held down on it. Check it before you treat a raw click or tap as an action in the world. The action map of [`haylen.input`](input.md) checks it by itself: while it is `true`, `mouse:` bindings read as released. So a click on a menu button never triggers an action bound to the same mouse button.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if input.mousePressed('left') and not ui.usingPointer() then
            print('The player clicked the world.')
        end
    end,
})
```

### ui.usingKeyboard()

Returns `true` while the interface uses the keyboard, such as while a text field has the focus. Check it before you treat key presses as gameplay input.

```lua
local input = require('haylen.input')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

scene.push({
    update = function(self, dt)
        if input.keyPressed('space') and not ui.usingKeyboard() then
            print('Jump.')
        end
    end,
})
```

## Accessibility

The UI works with the screen readers and the accessibility settings of each platform. The [UI guide](../ui/ui.md#accessibility) explains what it does on its own.

### Screen readers

The GUIs describe themselves to the screen readers of each platform. Every control, text and heading becomes an element with its role, label, value and state. The page of [`haylen.accessibility`](accessibility.md#screen-readers) lists what each kind says and where each platform reads it. These [common properties](#common-properties) say what a kind cannot know:

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `accessibilityLabel` | text | none | What screen readers say the node is, instead of what its kind describes, such as the name of an icon button. A node with a label is an element of the screen reader tree, even when its kind is not. |
| `accessibilityHint` | text | the tooltip | What screen readers say the node does, after a pause. |
| `accessibilityRole` | string | from the kind | What screen readers say the node is. It is one of the roles of [`accessibility.elements`](accessibility.md#accessibilityelements), such as `'heading'` for a label that titles a section. |
| `accessibilityHidden` | boolean | `false` | Leaves the node and every node inside it out of what screen readers read, such as decoration that repeats a text. |

```lua
local ui = require('haylen.ui')

ui.mount(ui.row{
    ui.label{text = 'Inventory', accessibilityRole = 'heading'},
    ui.button{icon = 'ui/gear.png', variant = 'icon', accessibilityLabel = 'Settings', accessibilityHint = 'Opens the settings of the game'},
    ui.image{image = 'ui/divider-vine.png', accessibilityHidden = true},
})
```

### Accessibility settings

The UI follows the [accessibility settings](accessibility.md) of the system without any code:

- While the system asks for higher contrast, the UI draws the theme that the `highContrast` key of the chosen theme names.
- While the system asks for reduced transparency, it draws the colors `window`, `panel`, `raised` and `tooltip` of the theme opaque.
- Its text follows the text size of the system, until the app sets a text scale of its own with `ui.setTextScale`.

### ui.setTextScale(scale)

Sets how much larger the text of every GUI draws than the font sizes of its theme and styles. The `scale` is a number from 0.5 to 4, or `'system'`. The value `'system'` follows the text size of the system, as [`accessibility.settings().textScale`](accessibility.md#accessibilitysettings) reports it, and it is how the UI starts. Controls grow with their text, so a layout made for the normal size keeps its text inside its controls. Any other value raises `The text scale of the UI is a number from 0.5 to 4, or "system" to follow the text size of the system.`.

```lua
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

ui.setTextScale(preferences.get('textScale', 'system'))
```

### ui.textScale()

Returns the text scale the UI draws with, and whether it follows the system.

```lua
local ui = require('haylen.ui')

local scale, followsSystem = ui.textScale()
print('Text draws at ' .. scale .. ' times its size.')
```

## Collections

A [`collection`](#uicollectionproperties) builds its cells from templates and binds them to the items of the app. This section describes the types, templates and bindings that the property `types` holds, how the player reorders items, and complete examples. The [`UiCollection`](#uicollection) gives a collection its items, and a [`UiCell`](#uicell) is one of its cells. The [long lists guide](../ui/ui.md#long-lists) explains the ideas behind them.

### Types and templates

Each entry of `types` names a type and holds its definition. Every item names its type in its `type` field. A collection of one type does not need it.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `template` | node | required | The tree that every cell of the type is built from, in the [screen format](#screen-format). |
| `estimatedSize` | number from 0 to 100000 | `0` | The length along the axis of an item of the type before it is measured. The value `0` learns it from the measured items of the type, starting from the theme `listRowHeight`. |
| `span` | integer from 1 to 64, or `'full'` | `1` | The lanes a cell of a grid takes. The value `'full'` takes the whole line. |
| `sticky` | boolean | `false` | Pins the current item of the type at the start of the view while the items after it scroll under it, until the next one pushes it out, as section headers do. |
| `interactive` | boolean | automatic | Makes the whole cell one focus target. It reports `select` when pressed and paints the cell surfaces of the theme behind it. By default, a cell is interactive when its template has no part that takes the focus. So a row of labels and pictures takes the focus as a whole, and a row with a toggle lets the toggle take it. |

A template is a node tree with two keys that only templates take:

- The key `part` names a node of the template, unique in it, so that binders, events and `cell:set` can refer to it. A template takes no `id`, since it is built many times. It takes no handlers either, since the events of every part arrive at the collection with the part name.
- The key `bind` maps properties of the node to fields of the item, such as `bind = {text = 'title', image = 'picture'}`. The pseudo fields `'$index'`, the index of the item from 1, and `'$selected'`, whether it is selected, bind what the item does not hold. When the item lacks a field, the property takes its value in the template. So give templates a value for every property that some items leave out, which keeps reuse cheap.

Templates do not hold a `scroll` or another `collection`, whose child windows and nested items a cell cannot reuse. The cells of a collection never count against the node limit of their GUI.

### Bindings

Bindings write back. When the player changes a bound property, such as the `checked` of a toggle bound to `on`, the collection stores the new value in that field of the item. It does so before the handlers of the event run. So the handlers read the item already changed, and the next bind of the item shows it.

Before a cell shows another item, every part takes the values of its template again. Properties that a binder or `cell:set` changed go back to their template values. A part that the player changed, or whose changed property the template leaves out, is built again from the template. Controls keep the state the UI keeps for them, such as the knob of a toggle, by the item they show, never by the cell. So no state passes from one item to the next.

A binder, which [`collection:setBinder`](#collectionsetbindertype-binder) registers, fills in what a binding cannot express. Binders and page loaders run before the GUIs draw, once per frame. Lua code may change the properties of nodes while a binder or a page loader runs, but not the items of the collection or the nodes of its GUI.

### Reordering

A collection with `reorderable = true` lets the player move its items. It reports each move as `reorder` and does not change the items itself. The app moves the item in its data, usually with `collection:move(event.from, event.to)`, and the other items slide to their new places.

- The mouse lifts an item that it drags past a few units. A finger lifts an item that it holds still for the theme `longPressDuration`, since a finger that moves at once scrolls.
- The lifted item follows the pointer above the other cells, along the axis of a list and freely in a grid. A bar in the gap marks where it lands.
- Near an end, the item scrolls the view, faster the closer it gets. It scrolls the `scroll` around a collection that shows all its items.
- The pointer starts a lift from any part of a cell, also from a control inside it, which then does not report its press.
- The keyboard, gamepads and remotes move the items of interactive cells. Accept on the item pressed last picks it up, so the first accept presses an item and the second one picks it up. The focus moves to another item, and accept there drops it. Accept on the carried item, or `uiCancel`, puts it back.

```lua
local ui = require('haylen.ui')

local tasks = {{id = 'wood', text = 'Gather wood'}, {id = 'fire', text = 'Light the fire'}, {id = 'fish', text = 'Catch a fish'}}
local gui = ui.mount(ui.collection{id = 'tasks', height = 400, reorderable = true, types = {
    task = {template = ui.label{part = 'text', bind = {text = 'text'}}},
}, onReorder = function(event)
    event.gui:collection('tasks'):move(event.from, event.to)
end})
gui:collection('tasks'):setItems(tasks)
```

### Template errors

The collection reports these errors when the GUI mounts or a `set` changes it:

- `A "collection" needs at least one type in "types".` and `The property "types" of a "collection" must map type names to type definitions.`
- `The type "<type>" of a "collection" needs a "template" node.` and `Unknown key "<key>" in "collection.types.<type>".`
- `The "estimatedSize" of the type "<type>" must be a number from 0 to 100000.`
- `The "span" of the type "<type>" must be a whole number from 1 to 64 or "full".`
- `The "sticky" of the type "<type>" must be "true" or "false".`, and the same for `interactive`.
- `A node inside a collection template names itself with "part", not "id".` and `The "part" of a "<kind>" in the template of the type "<type>" must be a non-empty string.`
- `A collection template takes no handlers. Handle the events of its parts on the collection, where they arrive with "part" and "cell".`
- `The part "<part>" is used more than once in the template of the type "<type>".`
- `The "bind" of a "<kind>" in the template of the type "<type>" must map property names to field names.`
- `The template of the type "<type>" cannot hold a "scroll" or a "collection".`
- `The template of the type "<type>" has a problem.`, followed by the error of the node, such as `The property "width" of a "label" must be a non-negative number or "auto".`
- `The property "placeholder" of a "collection" names the type "<type>", which it does not declare.`
- `The sticky type "<type>" of a grid collection must span the whole line.`
- `The property "selected" of a "collection" must be a list of item ids.`, and the usual property errors, such as `The property "axis" of a "collection" must be "vertical" or "horizontal".`

An item whose bound value a part does not accept stops the app when its cell binds. The message is `The item "<item>" gives the part "<part>" a value it does not accept.`, followed by the property error. For a node without a part, it names `a "<kind>" of its cell` instead.

### Examples

A chat of mixed message types with a binder:

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
        }, onSelect = function(event) print('Opened the message ' .. event.item .. '.') end},
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
        print(event.item .. (event.selected and ' was added to' or ' was removed from') .. ' the cart.')
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

## UiCollection

A `UiCollection`, named `haylen.UiCollection` in messages, gives a [`collection`](#uicollectionproperties) its items, and scrolls, focuses and reads it. The method [`gui:collection(id)`](#guicollectionid) returns it. Indices count from 1, as Lua lists do. A target is the id of an item or its index.

Every member raises `The GUI is not mounted.` after `unmount`. Every member raises `This "haylen.UiCollection" was already released.` once `replaceChildren` removed the node. Reading a member it does not have raises `The type "haylen.UiCollection" has no member "<name>".`. Writing a read-only property raises `The type "haylen.UiCollection" has no writable property "<name>".`.

### collection:setItems(items)

Shows the items of the Lua list `items`. The GUI keeps the list, and the collection reads it. Every item is a table with a non-empty string `id`, unique in the list. It has a `type`, unless the collection declares only one type, and the fields that its templates bind. The collection caches the ids and the types. It reads only the fields that a cell binds, with raw table access that runs no Lua code.

A second call, with the same list changed in place or with another list, compares the items by id. So the items that stay keep their measured sizes, their selection, the focus and their cells. A call after `setPages` drops the pages and shows the list.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}, {id = 'slot2', name = 'Slot 2'}})
print(saves.count)
```

### collection:setPages(options)

Shows a list too long to build at once, which a loader gives a page at a time. The loader runs before the GUIs draw, for the pages that the view and its prefetch need. The items of a page on its way show the `placeholder` type. At most `maxPages` pages stay, so the pages farthest from the view go. A second call with the same `load` keeps the pages it loaded, and another loader starts over.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `count` | integer, at least 0 | required | The number of items. |
| `load` | function | required | The function `load(first, count)`, which returns a list of `count` items from the index `first`, or a promise of one, such as the promise of `jobs.spawn`. |
| `pageSize` | integer, at least 1 | `50` | The items of a page. |
| `maxPages` | integer, at least 1 | `16` | The pages that stay loaded. |

An unknown option raises `Unknown option "<key>".`. A missing count raises `The option "count" is required.`, and a negative one `The option "count" must be at least 0.`. A page size or a page limit below 1 raises `The option "pageSize" must be at least 1.`, or the same for `maxPages`. A `load` that is not a function raises `The option "load" must be a function.`.

A loader that fails stops the app, like an error inside a binder:

- `The page loader of the collection "<id>" must return a list of items or a promise of one.`
- `The page loader of the collection "<id>" returned <count> items for a page of <length>.`
- `The page loader of the collection "<id>" failed.`, followed by the reason of a rejected promise.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'log', grow = 1, types = {line = {template = ui.label{part = 'text', bind = {text = 'text'}}}}})
gui:collection('log'):setPages{count = 5000, pageSize = 200, maxPages = 8, load = function(first, count)
    local page = {}
    for index = first, first + count - 1 do
        page[#page + 1] = {id = 'line-' .. index, text = 'Line ' .. index}
    end
    return page
end}
```

### collection:insert(index, items)

Inserts the items of the list `items` into the list of the collection at `index`, from 1 to the count plus 1, and shows them. The collection arranges its lines again only from the first item that changes, so appending a page to a long list costs the items of the page.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}})
saves:insert(1, {{id = 'auto', name = 'Autosave'}})
saves:insert(saves.count + 1, {{id = 'slot2', name = 'Slot 2'}, {id = 'slot3', name = 'Slot 3'}})
```

### collection:remove(index, count)

Removes `count` items from `index`. The `count` is `1` by default. A count below 1 raises `bad argument #2 to 'remove' (the count must be at least 1)`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}, {id = 'slot2', name = 'Slot 2'}, {id = 'slot3', name = 'Slot 3'}})
saves:remove(2)
saves:remove(1, saves.count)
```

### collection:move(from, to)

Moves one item from the index `from` to the index `to`. A move to the same index does nothing.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}, {id = 'slot2', name = 'Slot 2'}})
saves:move(2, 1)
```

### collection:replace(index, item)

Puts another item table at `index`. The cell stays when the id stays.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}})
saves:replace(1, {id = 'slot1', name = 'Slot 1, day 12'})
```

### collection:reload(target)

Binds again the items whose fields the app changed in place. The `target` is a target or a list of targets. It rebinds only those items and keeps their lines while their type stays.

```lua
local ui = require('haylen.ui')

local items = {{id = 'wood', name = 'Wood', count = 3}, {id = 'stone', name = 'Stone', count = 1}}
local gui = ui.mount(ui.collection{id = 'bag', height = 600, types = {entry = {template = ui.row{
    ui.label{part = 'name', bind = {text = 'name'}, grow = 1},
    ui.badge{part = 'count', bind = {text = 'count'}},
}}}})
local bag = gui:collection('bag')
bag:setItems(items)

items[1].count = 4
bag:reload('wood')
items[2].count = 2
bag:reload({2})
```

### collection:setBinder(type, binder)

Registers `binder(cell, item, index)` for the type `type`. It runs for every cell of the type that receives an item, after its bindings, with a [`UiCell`](#uicell), the item table and its index. It fills in what a binding cannot express with `cell:set`. The value `nil` removes it. An unknown type raises `The collection "<id>" has no type named "<type>".`.

An item that a jump brings into view without a cell, such as a drag of the scroll bar, gets one while the GUI draws, with its bindings. When its type has a binder, the item waits for the next frame instead and shows nothing for that one frame.

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

### collection:scrollTo(target, options)

Scrolls to an item. It keeps aiming every frame while the items on the way are measured, so it lands on the item even when the sizes change.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `align` | string | `'nearest'` | The value `'nearest'` scrolls the least that shows the whole item, and nothing when it already shows. The values `'start'`, `'center'` and `'end'` align that part of the item with that part of the view. |
| `offset` | number | `0` | How far inside the aligned edge the item stays. |
| `animated` | boolean | `true` | Follows the item with a spring, and jumps close to a far item first. |

An unknown option raises `Unknown option "<key>".`, and another alignment raises `The option "align" must be "nearest", "start", "center" or "end".`.

```lua
local ui = require('haylen.ui')

local items = {}
for index = 1, 500 do
    items[index] = {id = 'item-' .. index, name = 'Item ' .. index}
end
local gui = ui.mount(ui.collection{id = 'items', height = 600, types = {item = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local list = gui:collection('items')
list:setItems(items)
list:scrollTo(250, {align = 'center'})
list:scrollTo('item-400', {align = 'start', offset = 16, animated = false})
```

### collection:scrollBy(distance, options)

Scrolls by a distance in design units, forward with a positive one. The option `animated` is `true` by default. An unknown option raises `Unknown option "<key>".`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'news', height = 600, types = {post = {template = ui.label{part = 'title', bind = {text = 'title'}}}}})
local news = gui:collection('news')
news:setItems({{id = 'a', title = 'The tide is high'}, {id = 'b', title = 'A ship was seen'}})
news:scrollBy(300)
news:scrollBy(-300, {animated = false})
```

### collection:focus(target)

Gives the focus to an item, and scrolls it into view with the `focusAlign` of the collection.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'levels', height = 600, types = {level = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local levels = gui:collection('levels')
levels:setItems({{id = 'beach', name = 'Beach'}, {id = 'cave', name = 'Cave'}})
levels:focus('cave')
```

### collection:indexOf(id)

Returns the index of the item with the id, or `nil`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'levels', height = 600, types = {level = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local levels = gui:collection('levels')
levels:setItems({{id = 'beach', name = 'Beach'}, {id = 'cave', name = 'Cave'}})
print(levels:indexOf('cave'))
```

### collection:cellOf(target)

Returns the [`UiCell`](#uicell) that shows the item, or `nil` while the item has no cell.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'levels', height = 600, types = {level = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local levels = gui:collection('levels')
levels:setItems({{id = 'beach', name = 'Beach'}, {id = 'cave', name = 'Cave'}})

scene.push({
    update = function(self, dt)
        local cell = levels:cellOf('cave')
        if cell then
            cell:set('name', {color = 'warningText'})
        end
    end,
})
```

### collection:visibleRange()

Returns the indices of the first and the last item in view, or `nil` before the collection drew items.

```lua
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local items = {}
for index = 1, 100 do
    items[index] = {id = 'item-' .. index, name = 'Item ' .. index}
end
local gui = ui.mount(ui.collection{id = 'items', height = 600, types = {item = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local list = gui:collection('items')
list:setItems(items)

scene.push({
    update = function(self, dt)
        local first, last = list:visibleRange()
        if first then
            print('Showing ' .. first .. ' to ' .. last .. '.')
        end
    end,
})
```

### collection:saveState()

Returns where the collection is, so that `restoreState` brings it back later. The table holds three fields. The field `item` is the id of the first item in view, and `distance` is how far its start lies from the start of the view. The field `focused` is the item that has the focus. It keeps a place by item rather than by offset.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'items', height = 600, types = {item = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local list = gui:collection('items')
list:setItems({{id = 'a', name = 'First'}, {id = 'b', name = 'Second'}})
local saved = list:saveState()
print(saved.item, saved.distance, saved.focused)
```

### collection:restoreState(state)

Brings the saved item back to its saved distance, and gives the focus back to the saved item. It does so for the items that still exist, whatever happened to the other items meanwhile. An unknown key raises `Unknown option "<key>".`.

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
        }}}, onSelect = function(event) print('Picked ' .. event.item .. '.') end},
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

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `count` | integer | read | The number of items. |
| `scrollOffset` | number | read and write | The offset of the view from the start of the content, in design units. Writing it jumps there. |
| `contentLength` | number | read | The length of the content along the axis. It counts the estimated length of the items not measured yet. |
| `viewportLength` | number | read | The length of the view along the axis. |
| `focusedItem` | string | read | The id of the item that has the focus, or `nil`. |
| `selected` | list of strings | read | The ids of the selected items, in item order. |

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'items', height = 600, selection = 'multiple', selected = {'b'}, types = {item = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local list = gui:collection('items')
list:setItems({{id = 'a', name = 'First'}, {id = 'b', name = 'Second'}})
list.scrollOffset = 0
print(list.count, list.contentLength, list.viewportLength, list.focusedItem, #list.selected)
```

### Errors

The methods raise these errors besides the ones each method lists:

- `The item at index <index> of the collection "<id>" needs a non-empty string id.` and `The item at index <index> of the collection "<id>" must be a table.`
- `The items of the collection "<id>" use the id "<item>" more than once.`
- `The item "<item>" of the collection "<id>" has the type "<type>", which the collection does not declare.`, `The item "<item>" of the collection "<id>" needs a type, because the collection declares several.` and `The type of the item "<item>" of the collection "<id>" must be a string.`
- `The collection "<id>" has no item with the id "<item>".` and `The index <index> is outside the items of the collection "<id>", which holds <count>.`
- `The collection "<id>" pages its items, so it changes them with "setPages".`, for `insert`, `remove`, `move`, `replace` and `reload` of a paged collection.
- `The collection "<id>" has no list of items. Give it one with "setItems".`, for the same methods before `setItems`.
- `The collection "<id>" cannot change its items while it binds cells or loads them.`, for a change of the items from a binder or a page loader.
- `The GUI cannot replace nodes or unmount while its collections bind cells.`, for `replaceChildren` and `unmount` inside a binder, which stops the app.

## UiCell

A `UiCell`, named `haylen.UiCell` in messages, is a cell of a collection while it shows one item. A binder receives it, and [`collection:cellOf`](#collectioncelloftarget) returns it. Once the cell shows another item, `bound` is `false`, and every other member raises `This cell no longer shows the item "<item>".`.

### cell:set(part, properties)

Changes properties of the part `part` of this cell, checked like [`gui:set`](#guisetid-properties). The part takes its template values again before the cell shows another item. An unknown part raises `The template of the type "<type>" has no part named "<part>".`. A `set` with `kind`, `id`, `children`, `part` or `bind` raises `The "set" method of a cell changes properties only, so it takes an object without "kind", "id", "children", "part" or "bind".`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'scores', height = 600, types = {score = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local scores = gui:collection('scores')
scores:setBinder('score', function(cell, item)
    cell:set('name', {color = item.best and 'warningText' or 'text'})
end)
scores:setItems({{id = 'ana', name = 'Ana', best = true}, {id = 'leo', name = 'Leo'}})
```

### cell:transform(part)

Returns the [`UiTransform`](#uitransform) of the part `part`. A new transform replaces it before the cell shows another item, so a tween on it stops with the item.

```lua
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'news', height = 600, types = {post = {template = ui.label{part = 'title', bind = {text = 'title'}}}}})
local news = gui:collection('news')
news:setBinder('post', function(cell, item)
    if item.fresh then
        tween.to(cell:transform('title'), 0.5, {opacity = 0.4}, {repeatCount = -1, loopMode = 'yoyo'})
    end
end)
news:setItems({{id = 'a', title = 'A ship was seen', fresh = true}})
```

### cell:command(part, name, arguments)

Sends a [command](#guicommandid-name-arguments) to the part `part`, the way `gui:command` sends one to a node. For example, `'open'` opens a `contextMenu` of the cell, whose `select` then arrives on the collection with `part` and `cell`. A command that the part does not answer raises the error of `gui:command`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.contextMenu{part = 'menu',
    items = {{id = 'delete', text = 'Delete'}},
    ui.label{part = 'name', bind = {text = 'name'}},
}}}, onSelect = function(event)
    print(event.item .. ' on ' .. event.cell.item .. '.')
end})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}})
saves:setBinder('save', function(cell, item)
    if item.ask then
        cell:command('menu', 'open')
    end
end)
```

### cell:bounds(part)

Returns where the cell drew in the last frame, or where its part `part` drew when given. The result is a table with `x`, `y`, `width` and `height`, in the design coordinates of screen canvases, like `gui:bounds`. It is `nil` when the cell did not draw then, such as a cell bound ahead of the view. An app draws into a cell with it, such as a route under a row on a screen canvas.

```lua
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'routes', height = 600, types = {route = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local routes = gui:collection('routes')
routes:setItems({{id = 'north', name = 'North shore'}})

scene.push({
    render = function(self)
        local cell = routes:cellOf('north')
        local area = cell and cell:bounds('name')
        if area then
            graphics2d.beginScreen()
            graphics2d.drawRect({area.x, area.y + area.height - 4, area.width, 4}, '#FF3DBE7A')
        end
    end,
})
```

### Properties

| Property | Type | Access | Meaning |
| --- | --- | --- | --- |
| `item` | string | read | The id of the item the cell shows. |
| `index` | integer | read | The index of that item. |
| `type` | string | read | The type of that item. |
| `bound` | boolean | read | `true` while the cell shows the item it was handed out for. |

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'levels', height = 600, types = {level = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local levels = gui:collection('levels')
levels:setBinder('level', function(cell, item, index)
    print(cell.item, cell.index, cell.type, cell.bound)
end)
levels:setItems({{id = 'beach', name = 'Beach'}})
```

## GUIs

A GUI is a mounted tree of nodes. This section lists the functions that build and mount trees, the format of a tree, the values that properties take and how handlers receive events. The [UI guide](../ui/ui.md#mount-a-gui) explains how a GUI lives with its scene.

### ui.mount(tree, options)

Builds a GUI from the node table `tree`, shows it and returns its [`Gui`](#gui). Several GUIs can be mounted at once. The table `options` is optional.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `placement` | string | `'safe'` | The value `'safe'` lays out the root inside the safe area of the screen, away from notches and system bars. The value `'screen'` lays it out over the whole visible screen. |
| `layer` | integer | `0` | GUIs draw in ascending layer order. GUIs on the same layer draw in the order they were mounted, so a later GUI covers an earlier one with everything it holds, the content of its scrolls included. |
| `owner` | table or userdata | none | Unmounts the GUI when the owner is released, such as a scene when it unloads, or collected, like the other [owners of `haylen.events`](events.md#owners). |
| `sharedFocus` | boolean | `false` | Joins the navigation space of the other GUIs that share the focus, as [`gui.sharedFocus`](#guisharedfocus) describes. |

A scene of the stack as `owner` also makes the GUI part of the scene. The GUI draws only while the scene shows, so a scene pushed over it hides it, and it keeps its state until its scene shows again. During a transition, it goes into the image of its scene, so it never flashes over the other scene. A scene that owns its GUIs needs no `exit` to unmount them, and no `pause` and `resume` to hide them. A GUI without a scene as owner stays on screen above the scenes until `gui:unmount()`, even when scenes change, which suits the HUD of an autoload. Without an `owner`, a GUI that a module mounts at its top level in development belongs to that module, so a reload of the module unmounts it.

The root fills the whole area when its `align` is `'stretch'`, which is the default of containers. With `'start'`, `'center'` or `'end'`, the root keeps its measured size and sits at the top left, the center or the bottom right of the area. Mounting publishes the `guiMounted` event of [`haylen.events`](events.md) with the table `{gui}`.

An unknown option raises `Unknown option "<key>".`. A placement other than `safe` or `screen` raises `The "placement" option must be "safe" or "screen".`. An owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.`. A tree that breaks the [screen format](#screen-format), or has an invalid property, raises an error that names the problem, and nothing is mounted. Examples are `There is no UI component kind named "spaceship".` and `The property "width" of a "label" must be a non-negative number or "auto".`.

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

Returns a node table of the kind `kind`. It sets `kind` on the `properties` table and returns that same table, or a new table when `properties` is left out. It is the long form of `ui.<kind>{...}`, for when the kind comes from a variable. An unknown kind raises `There is no UI component kind named "<kind>".`.

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

Returns a sorted list of the names of every component kind: the 64 kinds of this page, and the kinds that C++ plugins register.

```lua
local ui = require('haylen.ui')

for _, kind in ipairs(ui.kinds()) do
    print(kind)
end
```

### ui.onEvent(listener, options)

Calls `listener(event)` for every event of every mounted GUI, with the same table that [handlers](#events-and-handlers) receive. It returns a `haylen.Connection`, whose `disconnect()` method stops the listener and whose `connected` property is `true` until then. Listeners run before the handlers of the node. Use it for screens loaded from JSON, for a sound on every click, or for analytics. The field `event.gui` is `nil` for GUIs that C++ code mounted.

The table `options` is optional and takes `owner`, a table or userdata. The listener ends when the owner is released, such as a scene when it unloads, or collected, like the other [owners of `haylen.events`](events.md#owners). An unknown key raises `Unknown option "<key>".`. An owner that is not a table or a userdata raises `An owner must be a table or a userdata, not <type>.`. An error inside a listener stops the app and shows the error screen with the message and its stack trace.

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
            print('Clicked ' .. event.id .. '.')
        end
    end, {owner = self})
end

scene.push(shop)
```

### Screen format

A node is a table with these keys, and the same shape works as JSON.

| Key | Type | Meaning |
| --- | --- | --- |
| `kind` | string | The component kind, one of the kinds of this page. It is required. |
| `id` | string | The optional name of the node, unique within the GUI and not empty. The methods `set`, `replaceChildren`, `get`, `bounds` and `command` find nodes by it. Ids that start with `#` are reserved for the engine. |
| `children` | list of nodes | The child nodes, for kinds that hold children. |
| any other key | depends on the kind | A property of the kind: a [common property](#common-properties) or a property of its own. |
| `onX` | function | In Lua only, a [handler](#events-and-handlers) for the event `x`. |

In Lua, the children can also go in the array part of the node, so trees read like `ui.column{ui.label{...}, ui.button{...}}`. A node takes its children from `children` or from its array part, never from both.

A GUI has at most 64 levels, the root included, and 20000 nodes. The count covers the whole GUI when `replaceChildren` adds children. A node table that holds itself, such as `c[1] = c`, raises the same error instead of nesting without end. Every property is checked when the tree is built, so a misspelled key or a wrong value raises an error instead of being ignored.

These errors come from invalid trees:

- `A UI node needs a kind.` and `There is no UI component kind named "<kind>".`
- `The id of a "<kind>" must be a non-empty string.`, which reads `of an "<kind>"` for a kind that starts with a vowel, and `The UI id "<id>" is used more than once.`
- `UI ids starting with "#" are reserved for nodes the engine names.`
- `The children of a "<kind>" must be a list.` and `The component kind "<kind>" takes at most <count> children.`
- `A UI node takes children either in its "children" list or in its array part, not both.`
- `UI node keys must be strings.` and `A UI node with handlers needs a string id.`
- `A GUI is limited to 64 levels and 20000 nodes.`
- `The property "<key>" of a "<kind>" does not exist.` for an unknown property, and the value errors that [value types](#value-types) lists.

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
menu:set('play', {onClick = function() print('Play.') end})
menu:set('quit', {onClick = function() print('Quit.') end})
```

Lua has no separate empty list, so an empty table converts to an empty JSON object. List properties accept an empty object as an empty list: `items`, `rows`, `columns`, `buttons`, `expanded`, the `children` of tree items and the `cells` of table rows. So `{}` works in Lua, and a JSON file with an empty list keeps working after it passes through Lua. Both `children = {}` and `replaceChildren(id, {})` work too.

### Value types

The tables of properties name these types.

| Type | Accepted values |
| --- | --- |
| text | A string, a number, or a translation such as `{key = 'menu.play', args = {n = 2}}`. [`haylen.localization`](localization.md) translates it every time the text draws, so a language change shows at once. Whole numbers show without decimals. |
| color | A string `'#RRGGBB'` or `'#AARRGGBB'`, such as `'#FF2E7D32'`. |
| theme color | The name of a [theme color role](#theme-colors), such as `'accent'` or `'textMuted'`. |
| length | A non-negative number, or `'auto'` to let the content decide. |
| insets | One number for every side, `{vertical, horizontal}`, or `{top, right, bottom, left}`. Every value is non-negative. |
| image | A path relative to the package `content/` folder, or the name of an image made at run time, which [`ui.setImage`](#uisetimagename-source) gives. |
| tone | `'neutral'`, `'accent'`, `'success'`, `'warning'`, `'danger'` or `'information'`. |
| variant | `'default'`, `'primary'`, `'destructive'`, `'toolbar'`, `'icon'` or `'link'`, as [button](#uibuttonproperties) describes. |
| items | A list of [items](#items). |

An image path names an image file, such as a `.png` or a `.jpg`, or an SVG document that ends in `.svg`. Images load in the background, and a component draws nothing in their place until they arrive. An image file measures its size in pixels. An SVG document measures the size that its view box, or its width and height, give it. The UI rasterizes an SVG document for the pixels it covers on the screen, so it stays sharp at every size, scale and density. The color `currentColor` paints white in an SVG document. So `ui.icon` tints the parts that use it with its `color`, and toolbar and icon buttons and the pictures of [items](#items) tint them like their text. The `iconColor` of a button or an item tints them with a theme color. An image that fails to load stops the app with `The UI image "<path>" could not be loaded.`, followed by the reason.

A value of the wrong type raises an error that names the key and the kind, such as `The property "<key>" of a "<kind>" must be "true" or "false".`. A kind that starts with a vowel reads as `of an "<kind>"`. The endings are these:

- `must be "true" or "false".`, `must be a string.`, `must be a number.` and `must be a whole number.`
- `must be at least <minimum>.` and `must be from <minimum> to <maximum>.`
- `must be a color such as "#FF2E7D32".` and `must name a theme color such as "accent" or "textMuted".`
- `must be <names>.` for a name outside its list, such as `must be "start", "center", "end" or "stretch".`
- `must be a non-negative number or "auto".` and `must be one, two or four non-negative numbers.`
- `must be text, a number or a translation such as "{key = 'menu.play'}".`

### Items

Kinds that offer choices take an `items` list: `tabs`, `accordion`, `menuButton`, `radioGroup`, `combo`, `segmentedControl`, `stepper`, `list`, `tree` and `contextMenu`. Every item is a table with a unique `id`. The section of each kind says which keys it uses.

| Key | Type | Default | Meaning |
| --- | --- | --- | --- |
| `id` | string | required | The name of the item, not empty. It is unique within the list, and within the whole tree for `tree`. Selection properties such as `selected` refer to it. |
| `text` | text | none | The text of the item. |
| `caption` | text | none | A second line under the text, which `list` and `tree` show. |
| `image` | image | none | A picture before the text, which `list`, `tree` and the menus of `menuButton`, `combo` and `contextMenu` show. |
| `iconColor` | theme color or `false` | the color of the text | The tint of the picture. Without it, the picture takes the color of the text of its item in every theme and state, so an SVG icon drawn with `currentColor` follows the theme. The value `false` keeps the colors of the picture, such as a painted icon or an image made at run time. |
| `enabled` | boolean | `true` | A disabled item cannot be picked. |
| `children` | list of items | none | Nested items, which only `tree` takes. |

Item errors name the property and the kind:

- A repeated id raises `The property "items" of a "<kind>" uses the item id "<id>" more than once.`.
- An item without an id raises `The property "items" of a "<kind>" needs a non-empty id for every item.`.
- An item that is not a table raises `The property "items" of a "<kind>" must hold objects with an id.`.
- Nested items outside a tree raise `The property "items" of a "<kind>" cannot nest items, which only a tree does.`.
- Tree children that are not a list raise `The property "items.children" of a "tree" must be a list of items.`.
- An `iconColor` that is neither a theme color nor `false` raises `The property "items.iconColor" of a "<kind>" must name a theme color such as "accent" or "textMuted", or be "false".`.

```lua
local ui = require('haylen.ui')

ui.mount(ui.list{
    items = {
        {id = 'wood', text = 'Wood', caption = 'Used for walls', image = 'ui/wood.png', iconColor = false},
        {id = 'stone', text = 'Stone', image = 'icons/stone.svg'},
        {id = 'gold', text = 'Gold', image = 'icons/coin.svg', iconColor = 'warning', enabled = false},
    },
})
```

### ui.setImage(name, source)

Gives an image made at run time a name, which every image value of the UI takes, in place of the image that had that name. An image, an icon, an avatar, an image button, the icon of a button or the picture of a list item then shows it like a file of the package. The `source` is one of these:

- A `Texture`.
- A `RenderTarget`, whose texture shows what the target renders every frame, such as a minimap or a portrait of the player. A render target holds colors premultiplied by their alpha, so its soft edges show darker unless its clear color is opaque.
- A `VectorImage`, which rasterizes for the pixels it covers, like an SVG file.
- An `Image`, which becomes a texture with the image filter of the theme.

Names start with `@`, which no path of the package does. Another name raises `A UI image made at run time has a name that starts with "@", such as "@minimap", not "<name>".`. A component that shows a name that nothing gives stops the app with `The UI image "@name" could not be loaded. No image of that name was set with "ui.setImage".`.

```lua
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local minimap = graphics.newRenderTarget(240, 160)
local camera = graphics2d.newCamera()
ui.setImage('@minimap', minimap)
ui.mount(ui.image{image = '@minimap', width = 240, height = 160, radius = 12})

scene.push({
    render = function(self)
        graphics2d.beginTarget(minimap, camera, {clear = '#FF1B5E20'})
        graphics2d.drawCircle(0, 0, 8, '#FFFFEB3B')
    end,
})
```

### ui.removeImage(name)

Takes the name away from the image made at run time that had it. A component that still shows the name then stops the app with the error of a missing name, as [`ui.setImage`](#uisetimagename-source) describes.

```lua
local graphics = require('haylen.graphics')
local ui = require('haylen.ui')

ui.setImage('@portrait', graphics.newRenderTarget(128, 128))
local hud = ui.mount(ui.image{id = 'portrait', image = '@portrait'})

hud:set('portrait', {visible = false})
ui.removeImage('@portrait')
```

### Events and handlers

Components report what the player does as events. A key of a node table is a handler when it starts with `on` followed by an upper-case letter and holds a function. It answers the event named by the rest of the key, with a lower-case first letter: `onClick` answers `click` and `onChange` answers `change`. A handler key that holds anything other than a function is read as a property, and raises `The property "<key>" of a "<kind>" does not exist.`. The engine does not check that the kind reports the event, so a handler for an event the kind never reports is never called.

A node with handlers and no `id` gets an id from the engine, such as `#1`, which handlers see as `event.id`. The method `gui:set` adds or replaces handlers, `gui:removeHandler` removes one, and the handlers of a node leave with it. A handler receives one table with the values of the event plus these fields:

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | string | The id of the node that reported the event. |
| `name` | string | The name of the event, such as `'click'`. |
| `gui` | Gui | The GUI that holds the node, so a handler can change it through `event.gui:set`. |

Event values never use the names `id`, `name` and `gui`. So the item of a `select` arrives as `event.item`, and the button of an `answer` as `event.button`, next to the node id in `event.id`. The section of each kind lists the events it reports. Every node also reports `focus` and `blur` as the focus comes and goes. A node with `focusScope` and the root of a GUI hear `cancel`.

The GUIs collect events while they draw. The handlers receive them once per frame, in the engine update of the next frame, before the scenes update. An error inside a handler stops the app and shows the error screen with the message and its stack trace.

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

### The events of every kind

Every node reports these events, each only while the node has a handler for it, since watching them costs a check every frame. They reach the listeners of `ui.onEvent` for those nodes too. A GUI mounted from C++ turns them on with `Gui::listen`.

| Event | Values | When |
| --- | --- | --- |
| `focus`, `blur` | none | The node took the focus or lost it. |
| `mount` | none | The node joined a mounted GUI, because its GUI mounted or `replaceChildren` added it. It reports in the update after the GUI next draws. |
| `unmount` | none | The node left its GUI, because the GUI unmounted or `replaceChildren` removed the node. It runs at once, before `unmount` or `replaceChildren` returns, since the handlers of a node end with it. A node reports it only after it reported `mount`. An error in the handler shows the error screen without stopping the change. |
| `show` | none | The node draws again after a frame in which it did not. That happens when it first draws, when it or a node around it becomes visible, when its tab or page shows, or when its GUI or scene shows again. |
| `hide` | none | The node stopped drawing, the opposite of `show`. A node that stops drawing while hovered or pressed first reports `hover` with `hovered = false`, and `release` with `inside = false`. |
| `hover` | `hovered` (boolean) | The pointer moved over the node or away from it. |
| `press` | `x`, `y` (numbers), `button` (`'left'`, `'right'` or `'middle'`) | A mouse button or a finger pressed the node. |
| `drag` | `x`, `y`, `deltaX`, `deltaY` (numbers), `button` | The pointer moved while the press that started on the node holds, wherever the pointer is. |
| `release` | `x`, `y` (numbers), `button`, `inside` (boolean) | The press that started on the node let go. The value `inside` tells whether the pointer was still over the node. |
| `scroll` | `deltaX`, `deltaY` (numbers) | The mouse wheel or a trackpad scrolled over the node, in UI units: the theme `scrollLine` for each step of a wheel, and the points a trackpad moved. A positive `deltaY` scrolls up. |

Positions are in the design coordinates of screen canvases, like `gui:bounds`. Every node under the pointer that listens reports the pointer events. So a press on a button inside a card reaches both when both listen. A disabled node reports none of them. The pointer events follow the visible part of the node, and stop while a window, a dialog or a popup above it takes the pointer.

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
        onScroll = function(event) print('Scrolled ' .. event.deltaY .. '.') end,
        onShow = function() print('The card shows.') end,
        onMount = function() print('The card joined the GUI.') end,
        onUnmount = function() print('The card left the GUI.') end,
    },
})
```

## Gui

A `Gui`, named `haylen.Gui` in messages, is the mounted tree that [`ui.mount`](#uimounttree-options) returns. Its methods find nodes by id. Reading a member it does not have raises `The type "haylen.Gui" has no member "<name>".`. Writing a read-only property raises `The type "haylen.Gui" has no writable property "<name>".`.

### gui:set(id, properties)

Changes the given properties of the node `id` and leaves the others as they are. The engine checks the merged properties before it changes anything, so an invalid value leaves the node and its handlers untouched. The `onX` keys add or replace [handlers](#events-and-handlers) of the node once the properties are accepted. Values that the player changed, such as typed text, a checked box or a selected item, stay as they are, unless `properties` sets that same property.

These errors can happen:

- `The GUI is not mounted.` after `unmount`.
- `The GUI has no node with the id "<id>".` for an unknown id.
- `The "set" method of a GUI changes properties only, so it takes an object without "kind", "id" or "children".` when `properties` holds `kind`, `id` or `children`.
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

Replaces every child of the node `id` with the nodes of the list `children`. An empty list removes them all. The ids of the new nodes are checked before anything changes, so a failed replace leaves the GUI and its handlers as they were. Once the new children are in place, the handlers of the removed nodes go. A new node that reuses the id of a removed one gets only the handlers of its own table.

These errors can happen:

- `The GUI is not mounted.` after `unmount`.
- `The GUI has no node with the id "<id>".` for an unknown id.
- `The component kind "<kind>" takes at most <count> children.` when the node cannot hold that many children.
- `The UI id "<id>" is used more than once.` when a new id is already in use outside the replaced children.
- `A GUI is limited to 64 levels and 20000 nodes.` when the GUI would grow past its limits.

```lua
local ui = require('haylen.ui')

local shop = ui.mount(ui.column{id = 'items'})

local function showItems(names)
    local rows = {}
    for index, name in ipairs(names) do
        rows[#rows + 1] = ui.button{id = 'buy-' .. index, text = name, onClick = function()
            print('Bought ' .. name .. '.')
        end}
    end
    shop:replaceChildren('items', rows)
end

showItems({'Axe', 'Rope', 'Lantern'})
```

### gui:get(id)

Returns a copy of the properties of the node `id`, as the tree created them and every `set` changed them, without `kind`, `id`, `children` and handlers. It returns `nil` for an unknown id. Changes of the player, such as typed text, are not part of it.

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

Returns the rectangle where the node was last drawn, as a `haylen.Rect` with `x`, `y`, `width` and `height`, in the design coordinates of screen canvases. A `window`, `dialog` or `toast` reports the frame it drew, which follows a window that the player drags. It returns `nil` for an unknown id, and for a node that has not drawn yet. That is the case until the GUI has drawn once, and for a window, dialog or toast that is closed.

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

Sends the command `name` to the node `id`. The table `arguments` is optional, and only `show`, `update`, `hide` and `scrollTo` take one.

- The command `'focus'` moves the keyboard, gamepad and remote focus to the node. The focus ring stays visible when the player was already navigating. It shows at once when the player last used a gamepad or has no pointer device. A mouse or touch player sees it only once they start navigating. The first accept also counts as navigating: it shows the ring and presses the focused node.
- The command `'open'` opens a `contextMenu` below its child, as a right click would.
- The command `'scrollTo'` scrolls a `scroll` to a place, as described below.
- The commands `'show'`, `'update'` and `'hide'` add, change and let go the notices of a `toast`, as [its section](#uitoastproperties) describes.

These kinds take the focus: `button`, `imageButton`, `menuButton`, `popover`, `chip`, `checkbox`, `toggle`, `radioGroup`, `combo`, `segmentedControl`, `textField`, `secretField`, `textArea`, `filterField`, `numberField`, `slider`, `rangeSlider`, `stepper`, `keyCapture`, `colorField`, `list`, `tree`, `slotGrid`, `accordion`, `carousel`, `playArea`, `collection` and a `richText` with links. Some of them choose an entry:

- A radio group, a list, a tree and a slot grid focus their selected entry. They focus their first entry that can be picked when none is selected, or when the selected one cannot take the focus, such as an item of a closed branch.
- An accordion focuses its first header.
- A collection keeps the focus on its focused item, or focuses its first item in view.
- Rich text focuses its first link. Rich text written as plain markup takes the focus as soon as it is mounted or set, and translated rich text once it has drawn.

The command `'scrollTo'` takes either `offset`, the distance from the start of the content in design units, or `node`, the id of a node inside the scroll, which the scroll follows while it moves. The argument `align` places the node. The default `'nearest'` scrolls the least that shows the whole node, and nothing when it already shows. The values `'start'`, `'center'` and `'end'` align that part of the node with that part of the view. The argument `animated`, `true` by default, moves there smoothly, and the wheel, a press or a drag of the player end the move.

These errors can happen:

- A kind without that command raises `The component kind "<kind>" does not answer the command "<name>".`.
- Arguments for `'focus'` or `'open'` raise `The "focus" command takes no arguments.` or `The "open" command takes no arguments.`.
- An unknown id raises `The GUI has no node with the id "<id>".`.
- A `'scrollTo'` without exactly one of `offset` and `node` raises `The "scrollTo" command of a "scroll" takes a table with either "node" or "offset".`. An unknown argument raises `Unknown key "<key>" in the "scrollTo" command of a "scroll".`. A node outside the scroll raises `The scroll "<id>" holds no node with the id "<node>".`. Wrong values raise the errors of the properties of the same names.

```lua
local ui = require('haylen.ui')

local form = ui.mount(ui.column{
    ui.textField{id = 'name', placeholder = 'Your name'},
})
form:command('name', 'focus')
```

### gui:removeHandler(id, event)

Removes the handler of the event named `event`, such as `'click'`, from the node `id`. It returns `true` when the node had one. It raises `The GUI is not mounted.` after `unmount`, and `The GUI has no node with the id "<id>".` for an unknown id.

```lua
local ui = require('haylen.ui')

local intro = ui.mount(ui.button{id = 'skip', text = 'Skip', onClick = function(event)
    print('Skipped once.')
    event.gui:removeHandler('skip', 'click')
end})
```

### gui:unmount()

Removes the GUI from the screen and forgets its handlers. It returns `true` when the GUI was mounted, and `false` when it was already unmounted. An unmounted GUI still answers `get`, `has` and `bounds`, while `set` and `replaceChildren` raise `The GUI is not mounted.`. Unmounting publishes the `guiUnmounted` event of [`haylen.events`](events.md) with the table `{gui}`. Then it ends every listener, timer and tween that has the GUI as its `owner`.

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

Returns the [`UiTransform`](#uitransform) of the node `id`, which moves, scales, fades and tints the node and its children. The GUI keeps one transform per node, so every call returns the same one while the node exists. An unknown id raises `The GUI has no node with the id "<id>".`.

```lua
local math2d = require('haylen.math')
local ui = require('haylen.ui')

local menu = ui.mount(ui.column{ui.label{id = 'title', text = 'Tiny Island', font = 'title'}})
menu:transform('title').offset = math2d.vec2(0, -40)
```

### gui:collection(id)

Returns the [`UiCollection`](#uicollection) of the `collection` node `id`, the same object every time while the node exists. An unknown id raises `The GUI has no node with the id "<id>".`. A node of another kind raises `The node "<id>" is a "<kind>", not a "collection".`. An unmounted GUI raises `The GUI is not mounted.`.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'saves', height = 600, types = {save = {template = ui.label{part = 'name', bind = {text = 'name'}}}}})
local saves = gui:collection('saves')
saves:setItems({{id = 'slot1', name = 'Slot 1'}, {id = 'slot2', name = 'Slot 2'}})
print(saves.count)
```

### gui.visible

A readable and writable boolean, `true` after `mount`. A hidden GUI does not draw and reports no events, except the `release` of a touch button that was pressed when it was hidden. It keeps its state for when it shows again.

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

A read-only boolean, `true` from `mount` until `unmount`.

```lua
local ui = require('haylen.ui')

local toastLayer = ui.mount(ui.toast{id = 'note', text = 'Saved'})
if toastLayer.mounted then
    toastLayer:set('note', {open = true})
end
```

### gui.sharedFocus

A readable and writable boolean, `false` unless `ui.mount` received `sharedFocus = true`. Every GUI is a navigation space of its own, where directions and Tab never leave it. The GUIs that share the focus form one space together. So a remote or a gamepad moves from the pages of an app to a tab bar or a player bar mounted as a GUI of its own, and back. Dialogs, popovers, menus and focus scopes still keep the focus inside them, and cancel still goes to the GUI that holds the focus.

```lua
local ui = require('haylen.ui')

-- The bar outlives the pages, which the scenes mount with the same option.
local bar = ui.mount(ui.row{anchor = 'stretchBottom', height = 96, ui.button{id = 'play', text = 'Play'}}, {layer = 1, sharedFocus = true})
local page = ui.mount(ui.column{ui.button{text = 'Album', autofocus = true}}, {sharedFocus = true})
```

### gui.placement

A read-only string, `'safe'` or `'screen'`, as `ui.mount` received it.

```lua
local ui = require('haylen.ui')

local hud = ui.mount(ui.label{text = 'HP 10'}, {placement = 'screen'})
print(hud.placement)
```

## UiTransform

A `UiTransform`, named `haylen.UiTransform` in messages, moves, scales, fades and tints a node and its children, on top of the place that its layout gives it. The method [`gui:transform(id)`](#guitransformid) returns the transform of a node, and [`cell:transform(part)`](#celltransformpart) the transform of a part of a cell. Changes show from the next frame.

| Property | Type | Default | Meaning |
| --- | --- | --- | --- |
| `offset` | Vec2 | `(0, 0)` | Moves the node, with the areas where it takes clicks and taps, in design units. |
| `scale` | Vec2 | `(1, 1)` | Scales what the node draws around its center. |
| `opacity` | number | `1` | Multiplies the alpha of what the node draws, from `0` to `1`. |
| `tint` | Color | white | Multiplies the colors of what the node draws. |

Scale, opacity and tint change only how the node looks, so its input areas keep their layout size. They reach everything the node draws in its GUI, the content of the scroll areas inside it included, with its pictures, shapes and text. Popups, tooltips, dialogs, toasts and windows float in windows of their own, so they keep their look whatever the transforms around them. The four properties are native properties, so [`haylen.tween`](tween.md#native-properties) animates them without running Lua every frame. Replacing the node with `replaceChildren` releases its transform. Reading it then raises `This "haylen.UiTransform" was already released.`, a tween on it stops, and `gui:transform` returns the transform of the new node.

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
title.tint = math2d.color(1, 0.9, 0.6, 1)
tween.to(title, 0.6, {opacity = 1, offset = math2d.vec2(0, 0)}, {ease = 'backOut'})
tween.to(menu:transform('play'), 0.4, {scale = math2d.vec2(1.1, 1.1)}, {repeatCount = -1, loopMode = 'yoyo'})
```
