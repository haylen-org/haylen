# UI

Haylen has two interface modules, and both draw over the app in design units with the active theme. The module [`haylen.ui`](lua-api/ui.md) builds the interface players see, such as menus, HUDs, settings screens, dialogs and on-screen touch controls, as retained trees of themed components. The module [`haylen.imgui`](lua-api/imgui.md) exposes Dear ImGui immediate mode windows for debug panels, cheat menus and tools. This guide explains how the retained UI is organized, how themes work and when to reach for each module. The reference pages, indexed in the [Lua API reference](lua-api.md), list every property, event and error.

The worked example throughout is the Tiny Island sample: its theme in `samples/games/tiny-island/content/ui/theme.json`, its screens in `samples/games/tiny-island/source/scenes/`, the HUD in `samples/games/tiny-island/source/ui/hud.lua` and the shared building blocks in `samples/games/tiny-island/source/ui/widgets.lua`.

## GUIs

A GUI is a tree of nodes. Every node is a flat Lua table with a `kind`, an optional `id`, its children and the properties of its kind. Every kind is also a function of the module, so `ui.button{text = 'Play'}` builds a button node and `ui.node('button', {text = 'Play'})` does the same when the kind comes from a variable. Children go in the array part of a node or in a `children` list, never in both.

The function `ui.mount(tree, options)` builds the components, draws them over the app every frame and returns a `Gui`. A GUI mounted with `owner = self` in a scene belongs to the scene: it unmounts when the scene unloads, it draws only while the scene shows, so a scene pushed over it hides it and keeps its state until the scene shows again, and it goes through scene transitions with its scene, into the image of the scene that leaves or the scene that arrives, so it never flashes over the other scene. A GUI without a scene stays on screen above the scenes until `gui:unmount()`, even when scenes change, which suits a HUD of an autoload.

```lua
local ui = require('haylen.ui')

local pause = {}
pause.__index = pause

function pause:enter()
    self.gui = ui.mount(ui.column{
        justify = 'center',
        padding = 64,
        ui.panel{
            width = 640,
            align = 'center',
            ui.label{text = 'Paused', font = 'title', textAlign = 'center', align = 'center'},
            ui.button{id = 'resume', text = 'Resume', variant = 'primary', align = 'stretch', onClick = function()
                require('haylen.scene').pop()
            end},
        },
    }, {owner = self})
    self.gui:command('resume', 'focus')
end
```

Every property is checked when the tree is built, so a misspelled key or a wrong value raises an error that names the kind and the key, such as `The property "width" of a "label" must be a non-negative number or "auto".`, and nothing is mounted. A GUI holds at most 64 levels, the root included, and 20000 nodes, and `gui:replaceChildren` keeps it within them. A node table that holds itself, such as `c[1] = c`, raises the same error instead of nesting without end.

### Screens in JSON

The node format is plain data, so screens can also live in JSON files in the package assets. Handlers cannot be written in JSON, so the app attaches them with `set` after mounting.

```lua
local assets = require('haylen.assets')
local ui = require('haylen.ui')

local menu = ui.mount(assets.json('ui/main-menu.json'))
menu:set('play', {onClick = function() print('play') end})
```

An empty Lua table converts to an empty JSON object, and list properties such as `items`, `rows`, `columns`, `buttons` and `expanded` read an empty object as an empty list, so `items = {}` works, and so does a JSON screen with an empty list once it passes through Lua. An empty `children` table works too, because the engine builds the children list itself.

### Changing a mounted GUI

A mounted GUI is changed by node id instead of being rebuilt.

| Method | Use |
| --- | --- |
| `gui:set(id, properties)` | Changes some properties of a node and keeps the others. The merged result is validated first, so a bad value leaves the node untouched. The `onX` keys add or replace handlers. |
| `gui:replaceChildren(id, children)` | Replaces every child of a node, such as the rows of a shop list. |
| `gui:get(id)` and `gui:has(id)` | The method `get` returns a copy of the properties the tree and later `set` calls gave a node, and `has` tells whether the id exists. |
| `gui:bounds(id)` | Returns where the node was last drawn, which suits tutorials that point at a button. |
| `gui:command(id, 'focus')` | Moves the keyboard and gamepad focus to a focusable node. |

Values the player changed, such as typed text or a moved slider, stay in the component unless a `set` call names that same property.

The Tiny Island HUD checks a dozen values every frame and only calls `set` when one of them changed. Translations are compared by the plain value they show, so the day label is only set again when the day number changes.

```lua
function hud:show(id, key, value, compared)
    local shownKey = id .. '.' .. key
    compared = compared or value
    if self.shown[shownKey] ~= compared then
        self.shown[shownKey] = compared
        self.gui:set(id, {[key] = value})
    end
end

self:show('health', 'value', health)
self:show('health', 'tone', health < 0.3 and 'danger' or 'success')
self:show('day', 'text', widgets.text('hud.day', {day = game.cycle.day}), game.cycle.day)
```

## Scale and screen density

A GUI lays out in UI units. With the default scale mode `'design'`, a UI unit is a design unit, so the interface grows and shrinks with the screen like the rest of the app, which suits games drawn for one screen shape and televisions. With `ui.setScaleMode('physical')` or `"ui": {"scaleMode": "physical"}` in `app.json`, a UI unit keeps the same physical size on every screen, from the density the platform reports, so buttons and text are as large on a phone as on a desktop monitor, and the shorter side of the visible area always holds at least 480 units. Then the room the UI has changes with the screen, and layouts that grow, wrap and fit their columns follow it. The factor of `ui.setScale`, such as a text size setting, multiplies both modes, and `ui.scale()` reports the factor, the design units a UI unit spans and the points of the screen it spans.

## Placement and the safe area

Sizes and positions are design units of the `design` resolution in `app.json`, which is 1920 by 1080 in Tiny Island and by default. The metrics of the built-in themes suit that resolution. See [Rendering](rendering.md) for the scaling policies that map design units to the screen. The functions [`viewport.setScaling`](lua-api/viewport.md#viewportsetscalingpolicy) and [`viewport.setDesignSize`](lua-api/viewport.md#viewportsetdesignsizewidth-height) change them while the app runs, and every GUI lays out in the new visible and safe areas from the next frame on.

The function `ui.mount` takes three options.

| Option | Default | Meaning |
| --- | --- | --- |
| `placement` | `'safe'` | The value `'safe'` lays the root out inside the safe area, away from notches, rounded corners and system bars. The value `'screen'` lays it out over the whole visible screen. |
| `layer` | `0` | GUIs draw in ascending layer order, and GUIs on the same layer draw in mount order, so a later GUI covers an earlier one with everything it holds, the content of its scrolls included. |
| `owner` | none | A table or userdata, such as a scene, that owns the GUI. The GUI unmounts when the owner is released, as a scene is when it unloads, so a scene that mounts with `owner = self` needs no `unmount` of its own. |

The root fills the whole area when its `align` is `stretch`, which is the default of containers. With `start`, `center` or `end` the root keeps its measured size and sits at the top left, the center or the bottom right of the area.

Inside a GUI, nodes draw in the order of the tree, and a scroll draws its content at the place of its node, so the nodes after it, such as an anchored bottom sheet, cover what scrolls.

A GUI that paints a background to the screen edges uses `placement = 'screen'` and wraps its controls in `ui.safeArea`, which keeps its one child inside the safe area. Toasts always appear at the top or bottom edge of the safe area, stacked so they never cover each other. Every Tiny Island screen, the HUD included, uses the default safe placement.

### Anchors

The app draws over the whole screen on every platform, under notches, dynamic islands, rounded corners and gesture bars, and the interface chooses where each part of it sits. Any node, the GUI root included, can leave the layout of its parent with an [anchor](lua-api/ui.md#anchors) and sit against the safe area or the whole screen: at a corner, at the middle of an edge, at the center, or stretched along an edge, a band or the whole area. The property `margin` keeps it away from the edges, and anchored nodes follow the safe area when it changes, such as when a phone turns.

```lua
ui.mount(ui.stack{
    ui.image{image = 'ui/sky.png', fit = 'cover', anchor = 'stretch', anchorTo = 'screen'},
    ui.button{id = 'pause', icon = 'ui/pause.png', variant = 'icon', anchor = 'topRight', margin = 16},
    ui.panel{anchor = 'stretchBottom', height = 120, margin = {0, 24}, ui.label{text = 'Wood 12'}},
}, {placement = 'screen'})
```

### Testing the safe area on a desktop

A desktop window has no notch, so the safe area can be simulated. The option `debug.safeArea` in `app.json`, or [`viewport.setSafeAreaSimulation`](lua-api/viewport.md#viewportsetsafeareasimulationvalue) while the app runs, takes a device such as `'iphoneNotch'`, `'iphoneDynamicIsland'`, `'ipad'`, `'androidGestureBar'` or `'television'`, whose insets follow the window as it turns between landscape and portrait, or insets in window points. The option `debug.showSafeArea`, or [`ui.setSafeAreaVisible`](lua-api/ui.md#uisetsafeareavisiblevisible), shades what lies outside the safe area and prints its insets over everything.

```json
{
    "name": "Notch Test",
    "debug": {"safeArea": "iphoneDynamicIsland", "showSafeArea": true}
}
```

## Layout

Layout comes from a small set of containers and a few common properties that every kind accepts.

| Container | Layout |
| --- | --- |
| `column` | Children from top to bottom. |
| `row` | Children from left to right, or from right to left in a [right-to-left](#right-to-left-interfaces) interface, in lines that wrap with `wrap = true`. |
| `grid` | Cells of equal width, `columns` per row or as many as fit at `minColumnWidth`, each row as tall as its tallest child. |
| `stack` | Children on top of each other, the later ones above. |
| `scroll` | One child in a vertically scrolling area. It measures as tall as its content, so it needs a `height` or `maxHeight` to scroll. |
| `card`, `panel` | Columns on a themed surface that keep the pointer from reaching the app. |

Columns and rows share these properties.

- The property `gap` is the space between children and defaults to the theme `itemSpacing` metric.
- The property `padding` is one number for every side, `{vertical, horizontal}` or `{top, right, bottom, left}`.
- The property `grow` on a child takes a share of the free space along the main axis, within its minimum and maximum sizes, and a child that reaches one of them leaves the rest to the others. A growing child starts from nothing rather than from its content, so two children with `grow = 1` split the space the others leave in half, long content never pushes a row or a column past its bounds, and a `scroll` with `grow = 1` scrolls inside the room its parent leaves. A row measures its growing children at the width they get, so text that wraps there reports every line and the node below starts after it. A column or row that takes the size of its content still gives each growing child at least the room its content needs.
- The property `justify` places the children along the main axis in the space growing children leave: `start`, `center`, `end`, `spaceBetween`, `spaceAround` or `spaceEvenly`.
- The property `align` on a child places it across the other axis: `start`, `center`, `end` or `stretch`. In a grid it places the child inside its cell, and in a stack or at the root it applies in both directions. The property `alignItems` of a column, row, grid or stack gives the alignment of every child that sets none. Without either, rows center their children, labels, buttons, icons, images, badges, touch controls and the other small kinds use `start`, the busy indicator uses `center` and every other kind stretches.
- The property `margin` keeps space around a node in the layout of its parent, on top of the `gap`, and `padding` keeps space inside a container.
- The properties `width`, `height`, `minWidth`, `maxWidth`, `minHeight` and `maxHeight` fix or bound the size, also of a node that stretches or grows. The value `'auto'` lets the content decide.
- The property `aspectRatio` keeps the width divided by the height of a node, such as `16 / 9` for a video frame.

A row with `wrap = true` breaks its children into lines, as a row of tags or of shop items does, with `lineGap` between the lines, and a grid with `minColumnWidth` fits as many columns as the width allows, so both follow the size of the screen.

### Scroll bars

Every area that scrolls shows its scroll bar in a lane of its own beside the content, never over it and never touching it: a `scroll` in both directions, a `collection`, the text of a `textArea`, the popups of `combo`, `menuButton`, `contextMenu`, `popover` and `colorField`, the body of a `dialog`, and the windows of [`haylen.imgui`](lua-api/imgui.md) with the debug overlay. The lane runs along the end edge of the area, the right edge or the left one of a [right-to-left](#complex-scripts-and-right-to-left-interfaces) interface for content that scrolls up and down, and the bottom edge for content that scrolls sideways. It holds the gap `scrollbarGap` between the content and the bar, the bar `scrollbarSize` thick, and the inset `scrollbarInset` between the bar and the edge of its area, and the ends of the bar keep the inset from the other edges and from the rounded corners of a dialog, a popup or a text area. A theme or the `style` of a node changes the three metrics, and the images of the surfaces `scrollbar`, `scrollbarHover` and `scrollbarTrack` draw in the same place as the flat bar.

The gap never spans fewer than 4 points of the screen, whatever the scale mode, the factor of `ui.setScale` or the scaling of the design area, so on a small window or with a small factor it grows in UI units until it spans 4 points. With the built-in themes it is 8 units, 6 points in the physical mode.

The lane takes room from the content only while the content is longer than its area, so content that fits keeps the whole width. Padding on the side of the bar counts toward the lane, so the content of a dialog or of a collection with enough padding keeps its width when the bar shows, and a popup grows by the part of the lane its padding leaves. A popup opens below the control that opened it, or above it when the room below is too short for its content and the room above is larger, and it never leaves the display, so its whole bar shows. The layout never changes back and forth as the bar comes and goes: a scroll and a dialog decide from the size of their content at the whole width, a popup decides before it opens, and a collection and a text area take the lane from the frame after their content grew longer than the view and show the bar from then on. A horizontal `scroll` and a horizontal `collection` add the lane to their height, so their content keeps its height.

A `spacer` takes room and draws nothing, so `ui.spacer{grow = 1}` pushes the nodes after it to the far end of a row or column.

Text measures in whole design units, so a label, button or choice placed at its measured size always shows its whole text, whatever the scaling of the screen, and ends with an ellipsis only when its space is really too small.

Cards and panels add padding from two sources. Their own `padding` applies when it is set, and otherwise the theme `panelPadding` metric applies. The `padding` of their theme surface image is always added on top, so content clears thick frame borders.

The Tiny Island HUD shows the typical shape of a game HUD. A padded root column holds a top row with the survivor panel and its health, food, special and wood meters on the left, a growing spacer and the day and fire panel and pause button on the right. A toast sits under it, a growing spacer pushes the touch controls to the bottom, and the touch row stretches across the screen.

```lua
ui.mount(ui.column{
    padding = 32,
    ui.row{
        align = 'stretch',
        gap = 24,
        ui.panel{width = 620, align = 'start', gap = 10, --[[ portrait, health, food, special, wood ]]},
        ui.spacer{grow = 1},
        ui.panel{width = 420, align = 'start', gap = 8, --[[ day, phase, fire, raiders ]]},
        ui.button{id = 'pause', icon = art.icon('pause'), variant = 'icon', align = 'start'},
    },
    ui.toast{id = 'notice', duration = 4},
    ui.spacer{grow = 1},
    touchControls(class),
})
```

## Component catalog

The engine ships 64 component kinds, and `ui.kinds()` lists them. They are grouped by purpose below, and each group links to its section of the reference.

| Purpose | Kinds |
| --- | --- |
| [Containers](lua-api/ui.md#containers) | `column`, `row`, `grid`, `stack`, `scroll` (vertical or horizontal, with snapping), `card`, `panel`, `spacer`, `divider`, `tabs`, `accordion`, `carousel`, `formField`, `splitter`, `safeArea` |
| [Text](lua-api/ui.md#text) | `label`, `richText` (BBCode markup with links, effects and a typewriter reveal), `pageHeader`, `sectionTitle`, `emptyState`, `alert` |
| [Buttons](lua-api/ui.md#buttons) | `button` (variants `default`, `primary`, `destructive`, `toolbar`, `icon` and `link`), `imageButton`, `chip`, `menuButton`, `popover` |
| [Choices](lua-api/ui.md#choices) | `checkbox`, `toggle`, `radioGroup`, `combo`, `segmentedControl` |
| [Inputs](lua-api/ui.md#inputs) | `textField`, `secretField`, `textArea`, `filterField`, `numberField`, `slider`, `rangeSlider`, `stepper`, `colorField`, `keyCapture` |
| [Indicators](lua-api/ui.md#indicators) | `badge`, `statusIndicator`, `busyIndicator`, `progress`, `circularProgress`, `icon`, `image`, `avatar` |
| [Collections](lua-api/ui.md#collections) | `list` (with draggable rows), `tree`, `table`, `slotGrid`, `collection` (any number of items of several types in lists and grids with recycled cells, described in [Long lists](#long-lists)) |
| [Settings](lua-api/ui.md#settings) | `settingsForm`, `settingsRow`, `settingsActions` |
| [Overlays](lua-api/ui.md#overlays) | `dialog`, `toast`, `window`, `contextMenu` |
| [Touch controls](lua-api/ui.md#touch-controls) | `touchStick`, `touchButton` |

Pictures, such as the `image` of `ui.image`, `ui.icon`, `ui.avatar` and list items and the `icon` of buttons, are image files or SVG documents in the package. The UI rasterizes an SVG document on the job system for the pixels it covers on the screen, in steps a quarter of an octave apart, and draws with the nearest raster it has until the new one arrives, so icons and illustrations stay sharp on every density, at every UI scale and while they animate, from files of a few hundred bytes. The color `currentColor` paints white, so `ui.icon` tints an icon drawn with it through its `color`, toolbar and icon buttons tint it like their text, and the `iconColor` of a button tints it in every variant. The rasters that no frame drew lately go once they pass a memory budget, `ImageLibrary::kRasterBudget` in `engine/src/ui/ImageLibrary.hpp`, so a long list of pictures keeps the memory of the ones in view, and an SVG changed during hot reload shows once the app restarts. The property `radius` of `ui.image` rounds the corners of a picture, such as a photo on a card.

Every kind also accepts the [common properties](lua-api/ui.md#common-properties): `visible`, `enabled`, `tooltip`, `grow`, the size bounds, `align`, the anchor, the focus properties, `direction` and `language`. A hidden node takes no room.

Games reach for a few of them often. The kind `circularProgress` with `variant = 'cooldown'` shades an ability icon while it recharges. The kinds `stepper` and `segmentedControl` pick settings with left and right, the way console menus do. The kind `slotGrid` holds inventories and hotbars and moves items between slots, draggable lists and other GUIs, with the pointer and by carrying them with a gamepad or a remote. The kind `keyCapture` reads the next key, mouse button, gamepad button or stick for a controls screen and returns a binding the action map takes. The kind `window` floats a draggable panel over the game, and `contextMenu` opens actions on a right click, a long press or `uiMenu`.

The Tiny Island screens use a small part of the catalog. The menu is the logo as an `image` over a column of buttons. The class selection screen uses a `pageHeader` with `banner = true`, `imageButton` portraits whose `tint` dims the classes that are not chosen, and `progress` bars for the stats. The settings sheet is a `panel` holding a `settingsForm` of `settingsRow` nodes with sliders, a combo and toggles. The HUD uses panels, icons, labels, progress bars, an icon button, a toast and the touch controls.

## Events and handlers

A node table key that starts with `on` followed by an upper-case letter and holds a function is a handler. The handler `onClick` answers the `click` event and `onChange` answers `change`. The handler receives one table with the values of the event plus `id`, `name` and `gui`, so a handler can change its own GUI through `event.gui:set`.

```lua
ui.settingsRow{label = widgets.text('settings.music'), ui.slider{value = preferences.get('music'), width = 440, onChange = function(event)
    preferences.set('music', event.value)
end}}
```

Events are collected while the GUI draws and handed to the handlers once per frame, in the engine update of the next frame, before the scenes update. An error inside a handler stops the app and shows the error screen with the message and its Lua stack trace. A node with handlers and no `id` gets an engine id such as `#1`. The method `set` adds or replaces handlers, and handlers leave with their node.

The [event table](lua-api/ui.md#events-and-handlers) of the reference lists which kind reports which event. Event values never use the names `id`, `name` and `gui`, so the item of a `select` arrives as `event.item` and the button of an `answer` as `event.button`, next to the node id in `event.id`. Every node also reports `focus` and `blur` as the focus comes and goes, and focus scopes and GUI roots hear `cancel`. Every node can also report its lifecycle and the pointer: `mount` and `unmount` when it joins and leaves its GUI, `show` and `hide` when it starts and stops drawing, and `hover`, `press`, `drag`, `release` and `scroll` with positions and wheel steps, which a custom control or a draggable card builds on. Each of these reports only while the node has a handler for it, since watching them costs a check every frame, and `unmount` runs at once, before the change that removes the node returns, as [the events of every kind](lua-api/ui.md#the-events-of-every-kind) describes.

### Pointer, keyboard and gamepad

The function `ui.usingPointer()` returns `true` while the pointer is over something the interface owns: an interactive component, an open dialog, menu or picker, an ImGui window, or a card, panel or touch control. Empty space inside columns, rows and stacks lets the pointer through. The function `ui.usingKeyboard()` returns `true` while a text field has the focus. Gameplay that reacts to raw clicks or key presses checks them first.

A press the interface answers itself never reaches the actions of the app, like a click on a button. While a control has the focus, every navigation key, button and the left stick belong to the interface, and while a [play area](lua-api/ui.md#uiplayareaproperties) has the focus, the directions, accept and menu belong to the game and only cancel, Tab and `uiFocus` stay with the interface. Escape, the east button and the Menu button of a TV remote that close a popup, a combo list, a dialog, a closable window or a carried item, or that end the editing of a control, every key while a text field edits, and every key and button while a `keyCapture` listens stay with the interface too, until they are released, so an action bound to the same key, such as a `pause` bound to Escape, fires only when the interface answers nothing. A screen goes back with the `onCancel` handler of its GUI root, which hears `uiCancel` only when nothing else in the interface answers it. The keys and buttons are those of the navigation actions, remapped or built in. The [input guide](input.md#who-owns-the-keyboard-and-the-gamepad) explains how this relates to the action map.

While a text field edits, the keys it types belong to the field alone and never navigate, so Space types a space and Enter starts a new line in a `textArea` instead of pressing the field. Enter still submits a single line and Escape still cancels, as keys of the field, and the gamepad buttons of `uiAccept` and `uiCancel` still end the editing.

### Focus, navigation and TV remotes

Buttons, choices, inputs, rows, slots, play areas and the other interactive parts of a GUI take the keyboard, gamepad and remote focus, and the [navigation actions](lua-api/ui.md#focus-and-navigation) move it: `uiUp`, `uiDown`, `uiLeft` and `uiRight` on the arrow keys, the directional pad and the left stick, `uiAccept` on Enter, Space and the south button, `uiCancel` on Escape and the east button, `uiMenu` on the menu key and the north button, `uiFocus` on the View button, which moves the focus between a play area and the controls of its GUI, and `uiPagePrevious`, `uiPageNext`, `uiFirst` and `uiLast` on Page Up, Page Down, Home, End and the shoulder buttons, which page through collections. An app remaps any of them by defining an action with the same name in its action map, and the others keep their built-in bindings.

- A direction moves the focus to the nearest control that way, unless the node names its neighbor with `focusLeft`, `focusRight`, `focusUp` or `focusDown`. Sliders, range sliders, steppers, segmented controls and carousel dots use left and right themselves. Tab walks the controls in drawing order, also from one text field on to the next.
- A screen gives the first focus with `autofocus = true` on a node, or with `gui:command(id, 'focus')`, which every Tiny Island screen does so a gamepad player can start at once. A screen where the game plays next to controls gives the first focus to its play area, so the keys play the game and Tab, `uiFocus`, a click or a tap reach the controls. Setting `focusable = false` keeps a HUD button out of navigation, and `ui.clearFocus()` drops the focus.
- The focus stays inside its GUI: dialogs, popovers and menus keep it until they close and then give it back to where it was, and a node with `focusScope = true` keeps it the same way. A GUI that stops drawing, such as the GUI of a scene that another scene covers, keeps the control that had the focus and gives it the focus again once it draws, unless the focus moved elsewhere meanwhile. A `window` belongs to the navigation of its GUI, so moves reach it and bring it to the front. The property `focusWrap` wraps it around the ends of a row or a column, and a scroll brings the focused control into view.
- The action `uiCancel` closes the open popup or dialog, puts back an item carried from a slot grid, and otherwise sends `cancel` to the innermost focus scope or to the GUI root, where a screen goes back with an `onCancel` handler.
- The focused node shows a ring in the `focus` color of the theme, `focusWidth` thick and following the corners of the control, once the player navigates with the keyboard, a gamepad or a remote. A scripted focus keeps the ring as the player last saw it and shows it at once when a gamepad was the last device used, so a mouse or touch player never sees a ring appear on its own. The first direction while the ring is hidden only shows it, and the first accept shows it and presses the focused node. Dialogs focus their last button.

On an Apple TV and an Android TV there is no pointer, so the ring shows from the start. The Siri Remote moves the focus with swipes on its touch surface and the clicks of its edges, presses with a click, goes back with Menu and reports play and pause as the pause key and the west button. The remote of an Android TV moves the focus with its directional pad, presses with select and goes back with Back. On the root screen of an app, Menu and Back leave the app as Apple and Google ask, and elsewhere the app keeps them with [`window.setBackLeavesApp`](lua-api/window.md#windowsetbackleavesappenabled) set to `false`, while an open popup or dialog always keeps them.

### Text entry

The text components (`textField`, `secretField`, `textArea`, `filterField` and `numberField`) edit through the native text input of the platform. The focused field hands its text, selection, place on screen and keyboard options to a hidden native field of the platform, which opens the on-screen keyboard on phones, tablets, TVs and mobile browsers and brings input methods, autocorrection, dictation, the emoji picker and native paste, copy and undo. What the user types comes back to the field, text the input method still composes is underlined, and return, tab and escape submit, move to the next field and cancel. The properties `keyboard`, `returnKey`, `autocorrect`, `autocapitalize` and `maxLength` choose the keyboard, as the [reference](lua-api/ui.md#inputs) lists.

When the on-screen keyboard would cover the focused field, every mounted GUI moves up together until the field shows above the keyboard, and moves back when the keyboard closes. The events `keyboardShown` and `keyboardHidden` of [`haylen.events`](lua-api/events.md) tell the app where the keyboard is. The [text input guide](text-input.md) describes what each platform does.

```lua
local ui = require('haylen.ui')

ui.mount(ui.column{
    ui.textField{placeholder = 'Email', keyboard = 'email', returnKey = 'next'},
    ui.secretField{placeholder = 'Password', returnKey = 'go', onSubmit = function(event)
        print('signing in')
    end},
})
```

## Long lists

The kind [`collection`](lua-api/ui.md#uicollectionproperties) shows any number of items of several types in a vertical or horizontal list or in a grid, the way the native lists of phones do. It builds a cell from the template of a type only when no cell of that type is free, binds it to the item it shows, and gives it to another item of the same type once it leaves the view, so a list of a hundred thousand items keeps a few dozen cells and costs about what a list of a hundred does. The [reference](lua-api/ui.md#uicollectionproperties) lists every property, the template keys, the events and the errors, and [`UiCollection`](lua-api/ui.md#uicollection) and [`UiCell`](lua-api/ui.md#uicell) the classes Lua drives it with.

```lua
local ui = require('haylen.ui')

local gui = ui.mount(ui.collection{id = 'quests', height = 640, selection = 'single', types = {
    chapter = {template = ui.sectionTitle{part = 'title', bind = {text = 'title'}}, sticky = true, interactive = false},
    quest = {template = ui.row{gap = 16,
        ui.label{part = 'name', bind = {text = 'name'}, grow = 1},
        ui.toggle{part = 'done', bind = {checked = 'done'}},
    }},
}, onSelect = function(event) print('opened ' .. event.item) end})

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

Items are Lua tables that the app keeps. The function `setItems` gives the collection a list of them, every item with a unique `id` and a `type`, and the collection caches the ids and the types and reads only the fields a cell binds, with raw table access that runs no Lua code. The methods `insert`, `remove`, `move` and `replace` change the list and the collection together, `reload` binds items again after the app changed their fields in place, and a second `setItems`, with the same list changed or another list, compares the items by id, so the items that stay keep their measured sizes, their selection, the focus and their cells. A list too long to build at once loads in pages with `setPages`, whose loader returns a page or a promise of one, while the items of pages on their way show the `placeholder` type and the collection keeps at most `maxPages` pages.

The template of a type is a node tree in the screen format whose nodes name themselves with `part` and map their properties to fields of the item with `bind`. A binder, `setBinder(type, binder)` in Lua and `Collection::setBinder` in C++, fills what a binding cannot express with `cell:set`. Binders and page loaders run before the GUIs draw, in a preparation that runs once per frame before the first GUI draws, which is where Lua may run during the UI, and an item that a jump, such as a drag of the scroll bar, brings into view without a cell gets one while the GUI draws, with its bindings, unless its type has a binder, in which case it waits for the next preparation and shows nothing for that one frame. A cell returns to its template before it shows another item, the values the player changes in a bound control write back to the item before the handlers run, and the state the UI keeps for a control, such as the knob of a toggle, its focus and its hover, follows the item rather than the cell, because the ImGui ids of the components of a cell come from the item and from their place in the template.

### Layout, sizes and anchoring

A list places one item per line, and a grid fills `lanes` cells across its axis, or as many as fit at `minCellSize`, where a type with a `span` takes several lanes or the whole line. Every line is as long as its longest cell. An item that was never laid out takes the `estimatedSize` of its type, which the measured items of the type teach when the type declares none, and the lines keep their lengths in a Fenwick tree, so the line at an offset and the offset of a line cost a logarithm of the line count. With `cellAspect`, grid cells take their length from their width and need no measuring.

The view holds what the player sees while the items or their sizes change: before a change it keeps the focused item, or the first measured item in view, and after the change it puts that item back where it was, so items inserted above, items measured above for the first time and a reordered list never move what shows, and a collection with `stickToEnd` that showed its end keeps showing it. A `scrollTo` aims again every frame while the items on the way are measured, with an alignment of `'nearest'`, `'start'`, `'center'` or `'end'`, and `saveState` and `restoreState` keep a place by item rather than by offset.

### Scrolling and the focus

The wheel scrolls the collection under the pointer, a finger drags it along its axis and flings it, stretching past the ends and springing back, the scroll bar in its lane on the end edge drags and pages it, `snap` settles it on items or whole views, and a pull at the start of a `refreshable` list reports `refresh`. The keyboard, gamepads and TV remotes move the focus between items with the directions, and the actions `uiPagePrevious`, `uiPageNext`, `uiFirst` and `uiLast`, on Page Up, Page Down, Home, End and the shoulder buttons, page through the innermost collection around the focus. The collection binds the items around the focused one, so the focus moves on to items that do not show yet and scrolls them in with `focusAlign`, which `'center'` keeps in the middle of the view as TV lists do. An item whose cell the wheel or a finger took out of view keeps the focus, and the next direction brings it back first. With `rememberFocus` the focus that enters the collection lands on its last focused item, and `focusWrap` along the axis wraps from the last item to the first.

### What a long list costs

The command `python3 haylen.py bench --suite ui` runs the UI benchmark, `engine/bench/UiBenchmark.cpp`, on the headless host: a list and a grid of a hundred thousand items of three types, a sticky header type, a plain type and a type with a binder, take a fling every 45 frames for 900 frames and then rest for 300 frames. It prints the average and the worst CPU time of a frame, the items bound, the binder calls and the cells created in each phase, and the cells alive at its end. These are the numbers of an Apple M5 Pro in Release:

| Layout | Phase | Frame ms | Worst ms | Binds | Binder calls | Cells created | Cells alive |
| --- | --- | --- | --- | --- | --- | --- | --- |
| List | Fling | 0.063 | 0.365 | 1174 | 385 | 33 | 37 |
| List | Rest | 0.046 | 0.247 | 0 | 0 | 0 | 37 |
| Grid | Fling | 0.088 | 0.481 | 817 | 683 | 93 | 37 |
| Grid | Rest | 0.067 | 0.318 | 0 | 0 | 0 | 35 |

A frame at rest binds nothing, calls no binder and creates no cell, and the engine tests check that it calls no Lua and that the collection allocates nothing in it. A frame while flinging binds only the items that enter the window, and the cells stay bounded by the window, its prefetch and the pools of the types. The components inside the cells keep their own costs, since every visible container measures its children every frame, so cells of few parts keep long lists cheapest.

## Text and translations

Every text property takes a string, a number or a translation such as `{key = 'hud.day', args = {day = 3}}`. A translation is resolved through [`haylen.localization`](lua-api/localization.md) every time the text is drawn, so switching the language changes every mounted GUI at once without rebuilding anything. Plural forms pick their `zero`, `one` or `other` text from the `count` argument.

Tiny Island wraps this in `widgets.text`, and its settings screen switches the language live through `localization.setLanguage`.

```lua
function widgets.text(key, arguments)
    return {key = key, args = arguments}
end

widgets.caption('best', widgets.text('menu.best', {count = best and best.days or 0}))
```

Labels choose one of the theme font roles `body`, `caption`, `button`, `heading`, `title` and `monospace`, and a theme color role such as `text`, `textMuted` or `onAccent`. A role that names a font family registered with `ui.addFont(name, family)` draws the characters its face lacks from the fallback fonts of the family, so one theme serves every language, Japanese included, and a role with `bold` or `italic` draws with those faces of the family. Text drawn straight over the app needs an outline to stay readable, and the Tiny Island titles and captions use `color = 'onAccent'` with a dark `outline` for that reason.

Styled text goes in a `richText` node, which reads the BBCode markup of the [text guide](text.md): bold and italic, colors, outlines and shadows, lists and tables, inline images and input prompt icons, links the player can focus and activate, and animated effects with a typewriter reveal for dialogue. It reports `link` and `linkHover` events.

```lua
ui.richText{id = 'npc', text = 'The [b]old sailor[/b] says: [i]"Mind the [color=#FF6A6A]crabs[/color]."[/i] [url=more]Ask more[/url]', revealSpeed = 30, onLink = function(event)
    showMore(event.link)
end}
```

## Complex scripts and right-to-left interfaces

ImGui lays out and draws the widgets, and the text layout of the engine sets their text. Every component measures its text with the shaped layout of the font family of its role, and draws it through the 2D renderer from a callback of the ImGui draw list, at its place among the ImGui draws and inside their clip, moved, scaled and tinted by the transforms around it. Labels, buttons, fields, lists, tables, tooltips, dialogs and rich text therefore show Arabic, Hebrew, Persian, Urdu, Hindi, Thai and every other script the fonts of the family cover, with joined letters, conjuncts, marks and the bidirectional order of mixed text, as the [text guide](text.md#scripts-and-directions) describes. Layouts are cached by text and style, so a label that stays the same shapes once. The fonts of ImGui itself only serve the text editing engine of fields, which draws nothing, and the debug windows of `haylen.imgui`.

Every paragraph of the UI reads in the direction of its first strong letter, so an Arabic label reads from the right and an English one from the left in any interface. The direction of the layout is separate, and it is left to right unless the app changes it. The call `ui.setDirection('rightToLeft')` mirrors every GUI: rows start from the right, start and end alignments name the right and the left, check boxes and toggles put their box on the right, sliders and progress bars fill from the right, steppers and carousels swap their ends, menus open from the right edge and the arrow keys move sliders and steppers the way they point. A node with `direction = 'rightToLeft'` or `'leftToRight'` sets the direction of its own subtree, and `language` sets the language its text is shaped for, which otherwise is the current language. The [reference](lua-api/ui.md#right-to-left-interfaces) lists everything that mirrors.

A language declares its direction in its catalog with `"@direction": "rightToLeft"`, as [`haylen.localization`](lua-api/localization.md) describes, and `ui.setDirection('auto')` makes the UI follow it, so switching to Arabic or Hebrew mirrors every mounted GUI from the next frame and switching back restores it. Apps opt in, because a game may prefer to keep its HUD in one layout whatever the language. Anchors and four-sided padding stay where they are, and images are never flipped.

```lua
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

ui.addFont('world', graphics.newFontFamily({regular = assets.font('fonts/body.ttf'), fallbacks = {assets.font('fonts/noto_sans_arabic_regular.ttf'), assets.font('fonts/noto_sans_devanagari_regular.ttf')}}))
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

Text fields show complex scripts and right-to-left text the same way, and the caret moves over whole characters in the order they show, as the [text input guide](text-input.md#complex-scripts-and-right-to-left-text) explains.

## Themes

A theme holds every color, metric, font and surface the components use, so no component draws a literal color. The engine ships `dark`, which is active at start, and `light`. The function `ui.setTheme(name)` switches every GUI and every ImGui window at once, `ui.theme()` returns the active name and `ui.themes()` lists the registered ones.

An app adds its own theme with a JSON file loaded by `ui.loadTheme(path, base)`. The theme starts as a copy of the registered theme `base`, which defaults to `dark`, so the file only lists what it changes. The function `loadTheme` registers the fonts of `fontFiles`, loads the surface images and returns the theme name without switching to it. Loading a file whose `name` matches a registered theme replaces that theme, and the change shows at once when it is active. Tiny Island loads its theme at startup in `source/main.lua`.

```lua
ui.setTheme(ui.loadTheme('ui/theme.json', 'dark'))
```

A theme file has these keys.

| Key | Content |
| --- | --- |
| `name` | Name of the theme. It is required. |
| `colors` | [Color roles](lua-api/ui.md#theme-colors) mapped to `'#RRGGBB'` or `'#AARRGGBB'` strings. |
| `metrics` | [Metrics](lua-api/ui.md#theme-metrics) mapped to non-negative numbers in design units, such as `controlHeight` or `panelPadding`. |
| `fonts` | [Font roles](lua-api/ui.md#theme-fonts) mapped to `{"font": name, "size": number, "bold": boolean, "italic": boolean}`, where every key is optional. |
| `fontFiles` | Font names mapped to TrueType or OpenType files in the package assets, registered unless a font with that name already exists. The function `ui.addFont(name, path)` registers fonts from Lua too, and `ui.addFont(name, family)` registers a font family, whose faces and fallback fonts every text component of the roles that name it uses. |
| `surfaces` | [Surfaces](lua-api/ui.md#theme-surfaces) mapped to nine-slice images, or to `null` to go back to flat colors. |
| `imageFilter` | The filter that the pictures of `image`, `icon`, `imageButton`, `avatar`, `circularProgress` and the touch controls load with, `'nearest'` (the default) for pixel art or `'linear'` for painted art that the screen scales. Switching to a theme with another filter loads the pictures again. |

Unknown keys, roles, metrics and surfaces are errors, so a typo in a theme file never goes unnoticed.

### Colors, metrics and fonts

Color roles come in families. The roles `window`, `panel`, `raised` and `tooltip` fill surfaces, `overlay` dims the app behind modal dialogs, `hover`, `pressed`, `selection` and `focus` mark states, `border` and `borderStrong` draw lines, `scrollbar` and `scrollbarHover` draw the thumbs of scroll bars, and `text`, `textMuted`, `textDisabled` and `onTooltip` write text. The accent and the four tones `success`, `warning`, `danger` and `information` each come as a fill, the ink written on that fill (such as `onAccent`), a subtle background (such as `accentBackground`) and a text color readable on the window (such as `accentText`). A `progress` bar with `tone = 'danger'` draws with the `danger` fill, and a `badge` with the same tone uses `dangerBackground` and `dangerText`, or `danger` and `onDanger` when it is `solid`.

Metrics size the controls. The metric `controlHeight` sets the height of buttons, fields and sliders, `itemSpacing` is the default gap, `panelPadding` pads cards, panels and dialogs, and more specific metrics such as `progressHeight`, `sliderTrackHeight`, `toggleWidth` and `iconSize` size single parts. Fonts are sized when drawn, so one registered font serves every role and size. A role that names a font family takes its bold and italic faces with `"bold": true` and `"italic": true`, and every face draws the characters it lacks from the fallback fonts of the family at the size of its own em square, so symbols and Japanese text match the letters around them.

### Surfaces

A surface paints part of a component with a nine-slice image instead of flat colors, such as the paper panels and wooden dialogs of a textured game. The built-in themes have none. The reference lists [which component uses which surface](lua-api/ui.md#theme-surfaces), from `panel`, `card`, `dialog`, `toast` and `banner` to the three states of each button variant, `field`, `check`, `track`, `trackFill`, `knob`, `tab`, `chip`, `badge` and the touch control surfaces `stickBase`, `stickKnob`, `touchButton` and `touchButtonPressed`.

A surface image is an object with these keys.

| Key | Meaning |
| --- | --- |
| `image` | Texture path relative to the package `content/` folder. It is required. |
| `source` | The `[x, y, width, height]` region of the texture that holds the frame. It defaults to the whole texture. |
| `slice` | Border sizes cut from `source`, in texture pixels, as one, two or four numbers. Without it the whole region is the center and stretches to the bounds, which suits round images. |
| `pieces` | The nine regions given one by one instead of `source` and `slice`, in reading order, for art that ships as separate corner and edge pieces. |
| `scale` | Scale of the borders on screen. It defaults to 1. |
| `padding` | Space in design units between the edge of the image and the content, one, two or four numbers. |
| `tint` | Color multiplied with the image. |
| `colorize` | Also multiplies the image with the color the component would fill the area with, such as the tone of a progress bar or the accent of a slider. It defaults to `false`. |
| `filter` | `'nearest'` (the default) or `'linear'`. |
| `fill` | Either `'stretch'` (the default) or `'tile'` for the edges and the center. A tiled edge or center that is not empty must be at least 1 design unit wide and tall at `scale`, and a smaller one raises an error that names the surface. |

The corners keep their texture size times `scale`, and the edges and center stretch or tile. When the bounds are too small for both borders, the borders shrink together.

A theme may leave out the hover or pressed surface of a button, and the normal surface of the same button stands in for it. Tiny Island gives its buttons hover states with the same image and a different `tint`.

Without `colorize`, a surface image ignores the color the component would have used, so every tone of a progress bar and every state of a touch stick look alike. With `colorize = true`, one light image serves every color. Tiny Island draws the fill of every HUD bar from one light image, `track_fill.png`, and the tone of each `progress` node turns it green for health, orange for food and fuel and red when one runs low.

The fill of toggles, sliders and progress bars stays inside the `padding` of the `track` surface, so a framed bar image keeps its frame visible around the fill. Tiny Island pads its groove by 5 units so the fill sits inside its outline. A toggle draws its `track` in every state, and its `trackFill` fills the groove and fades in as the `knob` slides on, so the track never moves, an empty groove means off and a full one means on.

### One node in another look

Every node takes the common properties `theme` and `style`, which reach the node and every node inside it. The property `theme` draws the subtree with another registered theme, such as a light card in a dark screen, and `style` replaces single values of the theme, with the sections `colors`, `metrics`, `fonts` and `surfaces` of a theme file, such as one button that is rounder and greener than the others. Styles nest, so a value a style leaves out comes from the style or theme around the node. The property `cursor` sets the mouse cursor shape over a node, and the innermost node under the pointer with a cursor wins.

```lua
ui.row{style = {colors = {accent = '#FF2E9E62'}, metrics = {controlRadius = 40}},
    ui.button{text = 'Rounder and greener', variant = 'primary', cursor = 'pointingHand'},
}
```

Every part a component draws reads its theme: the shadows of floating surfaces (`shadow`, `shadowSize`, `shadowOffset`), the caret of fields (`caret`), the corner radius of panels and popups (`panelRadius`), which the rows of a popup and the title bar of a window follow inside, the space between a box and its text (`contentSpacing`), the focus ring gap (`focusGap`), the opacity of disabled nodes (`disabledOpacity`) and the delays of tooltips and context menus (`tooltipDelay`, `longPressDuration`).

### The Tiny Island theme

The file `samples/games/tiny-island/content/ui/theme.json` turns the `dark` theme into the casual fantasy look of the game, with art painted for it in high definition. It is a good model for a textured theme.

- The key `imageFilter` is `linear`, so the icons, portraits and logo of the screens stay smooth when the screen scales them, and every surface sets `"filter": "linear"` for the same reason.
- The key `fontFiles` registers the rounded display font of the game, and `fonts` gives it to every role but `monospace`.
- The key `colors` sets a slate indigo palette with white text, a royal blue accent, green, orange and red tones for the HUD bars and a yellow `focus` color that rings the navigation target and marks the chosen class name.
- The key `metrics` makes controls larger (`controlHeight` 84), lowers `panelPadding` to 10 because the panel image brings its own padding, and sizes bars, sliders, check boxes and the switch to the art.
- The surfaces `panel`, `card`, `dialog`, `toast` and `menu` use one panel image at different scales and paddings, and `banner` uses the blue ribbon with only left and right borders, so the ribbon stretches in the middle and keeps its folded ends.
- The surfaces of the three button variants use slate, blue and red buttons. The hover states tint the normal image, and the pressed states use the pressed art, whose face sits lower on a thinner bevel.
- The surface `track` is a dark groove with `padding` for its outline, `trackFill` is a light pill with `colorize` that the accent, a tone or the toggle state colors, and `knob` is a light knob with `colorize`, so a held knob darkens with the `pressed` color.
- The surfaces `stickBase`, `stickKnob`, `touchButton` and `touchButtonPressed` give the touch controls a ring, a knob and a round button with its pressed state.
- The keys `check`, `checkChecked`, `field`, `fieldFocused`, `tab` and `tabSelected` are the remaining surfaces. Chips and badges keep the flat colors of the palette.

The switch of the settings screen shows how the surfaces work together. Off, it is the empty groove with the knob at its start. On, the blue fill covers the groove and the knob sits at its end. Held, the knob darkens, and focused, the yellow ring surrounds it, while the same three images draw every state.

```json
{
    "imageFilter": "linear",
    "surfaces": {
        "track": {"image": "ui/track.png", "slice": 28, "scale": 0.93, "padding": 5, "filter": "linear"},
        "trackFill": {"image": "ui/track_fill.png", "slice": 16, "scale": 1.3, "colorize": true, "filter": "linear"},
        "knob": {"image": "ui/knob.png", "colorize": true, "filter": "linear"},
        "touchButtonPressed": {"image": "ui/touch_button_pressed.png", "filter": "linear"}
    }
}
```

Four-number insets such as `slice` and `padding` run clockwise from the top: top, right, bottom and left. The key `slice` is measured in texture pixels and drawn at `scale`, while `padding` is in design units.

## Touch controls

The kinds `touchStick` and `touchButton` put on-screen controls in a GUI and drive the virtual sticks and buttons that the [action map](input.md#the-action-map) reads. A stick with `action = 'move'` feeds every action bound to `virtualStick:move`, and a button with `action = 'attack'` feeds every action bound to `virtual:attack`, so gameplay code reads `input.vector('move')` and `input.pressed('attack')` the same way for keyboards, gamepads and touch.

- Every control follows its own finger, so a stick and several buttons work at the same time. The mouse drives them when no finger is down.
- A stick reports a vector of length 0 to 1 with its `deadZone` removed. A stick with `mode = 'floating'` centers where the finger lands inside its area, and one with `mode = 'following'` also moves after a finger that leaves its ring, so give them a generous `width` and `height`. The default `'fixed'` stick stays in place.
- Setting `touchOnly = true` shows a control only while the last input came from a touch screen, as `input.lastDevice()` reports it. Keyboard and gamepad players never see them.
- Controls keep the pointer from reaching the app behind them.
- A control that stops drawing, because it, its parent or its GUI was hidden or removed, releases what it held. Values reach the actions at the start of the next frame.
- A `touchButton` also reports `press` and `release` events for feedback such as sounds.

The Tiny Island HUD puts a floating stick in a large area at the bottom left and three buttons at the bottom right, all touch-only, inside a row whose visibility follows the player's touch controls setting. The buttons show the icons of the attack and the special of the chosen class.

```lua
ui.row{
    id = 'touch',
    align = 'stretch',
    visible = preferences.get('touch'),
    ui.touchStick{action = 'move', radius = 140, mode = 'floating', touchOnly = true, width = 760, height = 420, align = 'end'},
    ui.spacer{grow = 1},
    ui.column{
        gap = 20,
        align = 'end',
        ui.row{
            gap = 24,
            ui.touchButton{action = 'interact', image = art.icon('log'), size = 120, touchOnly = true},
            ui.touchButton{action = 'special', image = art.icon(class.specialIcon), size = 140, touchOnly = true},
        },
        ui.touchButton{action = 'attack', image = art.icon(class.attackIcon), size = 190, align = 'end', touchOnly = true},
    },
}
```

The theme surfaces `stickBase`, `stickKnob`, `touchButton` and `touchButtonPressed` give the controls their art, and a `style` gives it to one control. Without them they draw as translucent circles in the theme colors.

```lua
local art = {
    stickBase = {image = 'ui/stick_base.png', filter = 'linear'},
    stickKnob = {image = 'ui/stick_knob.png', filter = 'linear'},
}
ui.touchStick{action = 'move', mode = 'following', radius = 130, width = 600, height = 420, style = {surfaces = art}}
```

## Immediate mode UI with haylen.imgui

The module [`haylen.imgui`](lua-api/imgui.md) is Dear ImGui for Lua. The app rebuilds its windows every frame from its own state, usually in the `renderUi` callback of a scene, and widgets that edit a value return whether it changed and the new value.

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

ImGui windows use the colors, metrics and body font of the active theme, and pad the thumb of their scroll bars by the [gap](#scroll-bars) on every side, so their content keeps it too, and `ui.usingPointer()` returns `true` while the pointer is over one. Calls outside a running frame, such as at the top level of `source/main.lua`, raise an error. The engine's own debug overlay, the full mode of the statistics that F3 cycles through and [`haylen.debug`](lua-api/debug.md) controls, is built with it.

## A GUI of your own in Lua

An app can also draw its own GUI from Lua, every frame or retained, on the same APIs the built-in UI uses. Drawing takes rectangles, circles, lines, polygons, meshes, nine-slices, textures and text from [`haylen.graphics2d`](lua-api/graphics2d.md), with `measureText` to lay text out, `pushClip` to keep a list inside its box, and render targets and shaders for masks and effects. Input takes the mouse, the fingers, the keys, typed characters and gamepads from [`haylen.input`](lua-api/input.md), and [`input.editText`](lua-api/input.md#text-fields-of-the-app) edits a field through the native text input with the on-screen keyboard and input method composition. The cursor comes from `window.setCursor`, the safe area from `viewport.safeRect` and the scale from `ui.scale`. The test `GUI-027` of the test project is such a GUI, with buttons, a check box, a slider that holds the pointer while it drags, a text field, a clipped scrolling list and focus that keys, gamepads and remotes move.

## Choosing between them

| Need | Module |
| --- | --- |
| Menus, HUDs, settings, dialogs and anything else players see | `haylen.ui` |
| Textured art, translations, safe area placement and touch controls | `haylen.ui` |
| Gamepad and keyboard navigation with focus | `haylen.ui` |
| Screens stored as data in JSON | `haylen.ui` |
| Debug panels, cheat menus, live tuning and inspectors | `haylen.imgui` |
| Windows whose content changes shape every frame, such as entity lists | `haylen.imgui` |
| Plots, drag values and quick tools that never ship to players | `haylen.imgui` |

A retained GUI is built once and then changed by id, which keeps per-frame Lua work low and lets the engine validate every property. Immediate mode code is shorter for tools but runs Lua for every widget every frame and follows the ImGui look rather than the app's components.

## From C++

The same system is available to C++ code through `haylen::plugins::UiPlugin` (`haylen/plugins/UiPlugin.hpp`), reached with `engine.getPlugin<plugins::UiPlugin>()`. It builds `haylen::ui::Gui` trees from JSON with `createGui` and shows them with `mount`, which publishes `guiMounted` on the event bus of the engine, like `unmount` publishes `guiUnmounted`. It also loads and switches themes (`haylen::ui::Theme` in `haylen/ui/Theme.hpp`) and publishes every `haylen::ui::Event` of a GUI through its `events` signal. The method `UiPlugin::getComponents()` returns the `haylen::ui::ComponentRegistry`, where a C++ plugin registers its own component kinds, subclasses of `haylen::ui::Component` that read their properties with a `PropertyReader` and draw through the `haylen::ui::Context` they receive, which Lua then builds with `ui.<kind>{...}` like the built-in ones. The method `UiPlugin::getBackend()` returns the `haylen::ui::Backend` that owns the Dear ImGui context, for C++ code that draws immediate mode windows, whose `addRenderCallback` lets a component draw with the 2D renderer at its place among the ImGui draws, as `richText` does, and whose `beginChild` begins a child window that draws at its place among the draws of its parent, as `scroll` does, so the nodes after it cover it. The method `UiPlugin::addFontFamily` registers a `haylen::text::FontFamily` under a font name, which reaches the backend as `Backend::FontFiles`, its TrueType faces and fallbacks. Long lists are `haylen::ui::Collection` nodes (`haylen/ui/Collection.hpp`), which `Gui::getCollection(id)` finds: a C++ app gives one its items through a `haylen::ui::CollectionSource`, such as the ready `haylen::ui::CollectionItems`, binds cells with `Collection::setBinder` and a `haylen::ui::CollectionCell`, and places items its own way with a `haylen::ui::CollectionLayout`. The method `UiPlugin::getFocus()` returns the `haylen::ui::FocusNavigator`, which tells which GUI and node hold the focus and whether the ring shows, and `UiPlugin::getNavigation()` the `haylen::ui::NavigationInput` with the navigation actions. A C++ component makes an ImGui item a focus target with `FocusNavigator::addTarget` while it draws, and keeps a direction for itself by overriding `Component::usesFocusDirection`. The methods `core::Engine::setSafeAreaSimulation` and `setBackLeavesApp` are the C++ side of the safe area simulation and the back button, and `setScaling` and `setDesignSize` change the mapping of design space the UI lays out in while the app runs. Text fields reach the platform through `haylen::platform::TextInput` (`haylen/platform/TextInput.hpp`), which `Window::getTextInput()` returns and which the [text input guide](text-input.md) describes. See [Architecture](architecture.md) for plugins and [Lua](lua.md) for the scripting model.
