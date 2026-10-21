# 📖 `haylen.accessibility`

The module `haylen.accessibility` follows the accessibility settings of the system: how large the user wants text, whether they want less motion, higher contrast or less transparency, and whether a screen reader runs. It also opens the UI to screen readers, which read the elements of every GUI aloud and act on them. Use it to calm the motion of an app, to pick a theme with stronger colors, to announce what changed and to check what a screen reader reads. The [UI guide](../ui/ui.md#accessibility-settings) explains how the UI follows these settings on its own.

```lua
local accessibility = require('haylen.accessibility')
```

The platform reports the settings when the app starts and whenever the user changes them. The engine publishes each change once, at the start of the next frame, as the [event](#events) `accessibilitySettingsChanged`.

## Functions

### accessibility.settings()

Returns a new table with the settings, as the app last heard of them at the start of a frame. A platform without a setting reports its default, as the [platforms](#platforms) section lists.

| Field | Type | Meaning |
| --- | --- | --- |
| `textScale` | number | How much larger the user wants text than its normal size. The value `1` is the normal size, and the larger text sizes of a phone give values such as `1.3`. |
| `reducedMotion` | boolean | Whether the user wants less motion, such as no sliding, zooming or parallax. |
| `highContrast` | boolean | Whether the user wants higher contrast between colors. |
| `reducedTransparency` | boolean | Whether the user wants opaque surfaces instead of translucent ones. |
| `screenReader` | boolean | Whether a screen reader runs. A screen reader reads the interface aloud and moves through it with its own gestures. |

```lua
local accessibility = require('haylen.accessibility')
local tween = require('haylen.tween')

local function slideIn(panel)
    if accessibility.settings().reducedMotion then
        panel.x = 0
        return
    end
    tween.to(panel, 0.4, {x = 0}, {ease = 'outCubic'})
end
```

### accessibility.announce(text, options)

Asks the screen reader to say `text`, after what it says now. Use it for a change that shows nowhere a reader moves to. Where no screen reader runs, nothing is said. The table `options` is optional:

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `interrupt` | boolean | `false` | Stops what the screen reader says, and says the text at once. |

An empty text raises `An announcement needs a text.`, and an unknown option raises `Unknown option "<key>".`.

```lua
local accessibility = require('haylen.accessibility')

accessibility.announce('Level 3 is ready.')
accessibility.announce('Game over.', {interrupt = true})
```

### accessibility.elements()

Returns the elements of the [screen reader tree](#screen-readers) of the last frame, as a list of tables in the order a screen reader moves through them. The UI builds the tree only while a screen reader asks for it or `accessibility.setTreeEnabled(true)` turned it on. The list is empty before the first frame that built it.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The id of the element. It stays the same while its node stays where it is. |
| `parent` | integer | The id of the element that holds it, or `0` at the top. |
| `role` | string | What the element is: `'group'`, `'text'`, `'heading'`, `'image'`, `'button'`, `'checkBox'`, `'switch'`, `'radioGroup'`, `'radioButton'`, `'tabList'`, `'tab'`, `'slider'`, `'textField'`, `'comboBox'`, `'progressBar'`, `'list'`, `'dialog'`, `'alert'` or `'playArea'`. |
| `label` | string | What the screen reader says the element is. |
| `value` | string | What the element holds, such as the number of a slider or the text of a field. |
| `hint` | string | What the element does, which screen readers say after a pause. |
| `bounds` | `Rect` | Where the element lies, in design units. |
| `disabled`, `checked`, `selected`, `focused`, `expanded`, `busy` | boolean | The states of the element. |
| `actions` | list of strings | What a screen reader can do with it: `'focus'`, `'activate'`, `'increment'` and `'decrement'`. |

```lua
local accessibility = require('haylen.accessibility')

accessibility.setTreeEnabled(true)
for _, element in ipairs(accessibility.elements()) do
    print(element.role, element.label, element.value)
end
```

### accessibility.focused()

Returns the id of the element that holds the focus in the last tree, or `nil` when none does.

```lua
local accessibility = require('haylen.accessibility')

local focused = accessibility.focused()
for _, element in ipairs(accessibility.elements()) do
    if element.id == focused then
        print('The focus is on ' .. element.label .. '.')
    end
end
```

### accessibility.perform(id, action)

Asks the element `id` for an action, the way a screen reader does. The UI performs it at its next frame, and an element that is no longer there ignores it. Tests use it to drive the UI as a screen reader would. An unknown action raises `The accessibility action "<action>" is unknown. It is "focus", "activate", "increment" or "decrement".`.

| Action | What it does |
| --- | --- |
| `'focus'` | Moves the focus of the UI to the element. |
| `'activate'` | Presses the element, as the accept button does. |
| `'increment'`, `'decrement'` | Raises or lowers a slider, a stepper or a segmented control. |

```lua
local accessibility = require('haylen.accessibility')

for _, element in ipairs(accessibility.elements()) do
    if element.label == 'Play' then
        accessibility.perform(element.id, 'activate')
    end
end
```

### accessibility.setTreeEnabled(enabled)

Turns on the building of the tree in every frame, also while no screen reader asks for it, such as for tests and tools. It starts off.

```lua
local accessibility = require('haylen.accessibility')

accessibility.setTreeEnabled(true)
```

### accessibility.treeEnabled()

Returns whether `accessibility.setTreeEnabled` turned the tree on.

```lua
local accessibility = require('haylen.accessibility')

print('The tree is on:', accessibility.treeEnabled())
```

## Screen readers

The UI of [`haylen.ui`](ui.md) draws itself, so the engine describes it to screen readers. While a screen reader asks for it, the GUIs build a tree of elements as they draw. The engine hands the tree to the platform only when it changed, so a menu that stays as it is costs nothing beyond the drawing.

- **Kinds.** Every kind of component describes itself with its role, label, value and state. A `button` is a button labeled by its text. A `toggle` is a switch that is checked or not. A `slider` is a slider with its value. A `textField` is a text field with its text, and a `secretField` one with a bullet for each character. A `progress` is a progress bar with its percentage. A `label` and a `richText` are text without their markup. A `pageHeader` and a `sectionTitle` are headings. An `alert` and a `toast` are alerts, a `dialog` and a `window` are dialogs, and a `list`, a `table`, a `tree` and a `slotGrid` are lists.
- **Parts.** The options of a `radioGroup`, the tabs of `tabs`, the rows of a `list`, the headers of an `accordion` and the buttons of a `dialog` are elements of their own.
- **Containers.** Containers are no elements, so their children join the element around them. An `image` or an `icon` is decorative until it has a label.
- **Properties.** The [common properties](ui.md#common-properties) `accessibilityLabel`, `accessibilityHint`, `accessibilityRole` and `accessibilityHidden` give a node the label, the hint and the role that screen readers say instead of what its kind describes, or leave the node and its children out. A node with a label or a role of its own is an element even when its kind is not, such as a `card` that stands for one thing. A disabled node is disabled for screen readers too. A tooltip is the hint of its node, unless the node has a hint of its own.
- **Focus.** The focus stays in step. The element of the control that holds the focus of the UI holds the focus of the screen reader. A screen reader that moves its focus to an element, presses it or adjusts it moves the focus of the UI there. So the player who listens and the player who looks share one focus.

```lua
local ui = require('haylen.ui')

-- The icon button has no text, so its label tells screen readers what it does.
ui.mount(ui.column{
    ui.button{text = 'Play', autofocus = true},
    ui.button{icon = 'ui/gear.png', variant = 'icon', accessibilityLabel = 'Settings', accessibilityHint = 'Opens the options of the game.'},
})
```

| Platform | Screen readers |
| --- | --- |
| macOS | VoiceOver and every assistive technology of AppKit, such as Accessibility Inspector. A view over the view of the app serves the elements as `NSAccessibilityElement` objects, with the roles, values and actions of AppKit. The engine builds the tree once an assistive technology asked the view for its elements, or while VoiceOver runs. Announcements go through `NSAccessibilityAnnouncementRequestedNotification`. |
| iOS, iPadOS, tvOS and Mac Catalyst | VoiceOver, Switch Control and the accessibility audits of Xcode. A view over the view of the app serves its elements as `UIAccessibilityElement` objects with the traits of UIKit. Sliders and steppers are adjustable, and a play area allows direct interaction, so a game takes its touches while VoiceOver runs. The engine builds the tree once an assistive technology asked the view for its elements, or while VoiceOver or Switch Control runs. Announcements go through `UIAccessibilityAnnouncementNotification`. |
| Android | TalkBack, Switch Access and every accessibility service. An `ExploreByTouchHelper` of AndroidX serves virtual views for the surface of the app, with the classes of the Android widgets for the roles. So services explore by touch, click, scroll sliders and move their accessibility focus, which moves the focus of the UI. The engine builds the tree while an accessibility service runs, or once one asked for the views. Announcements go through a virtual view that is a live region, polite, or assertive when the announcement interrupts. |
| Web | Every screen reader of the browser. The page keeps a mirror of the UI over the canvas without showing it, where every element is an element of the page with its ARIA role, name, value and states. Clicking, focusing or pressing an arrow key on an element of the mirror performs the action in the UI. A focus that moves in the UI moves the focus of the page along while that focus is inside the mirror. Browsers never tell whether a screen reader runs, so the engine builds the tree all the time. Announcements go through two live regions, polite and assertive. |
| Windows | Narrator and every client of UI Automation, such as Inspect. The window of the app serves providers when UI Automation asks it for its root: a root provider for the window, whose own provider gives the name and the place, and one provider for each element, which keeps its id across frames. Each provider has the control type of UI Automation for its role and the patterns of its kind. Buttons invoke, and check boxes and switches toggle. Sliders and progress bars give their number through the range value pattern, where a new value moves them a step toward it. Fields give their text through the value pattern, read only, since the native field edits it. Tabs and radio buttons are selection items, and combo boxes expand and collapse. The engine raises the events of the focus, of the structure and of the names, values and states that change. It builds the tree while the screen reader flag of the system is on, or once a client asked the window for its elements. Announcements go through the notifications of UI Automation, after what the screen reader says, or at once when the announcement interrupts. |
| Linux | Orca and every assistive technology of AT-SPI, the protocol of the accessibility bus of D-Bus. The engine serves it with the GDBus of GIO, which it loads at run time. The app is an application of the registry of the bus with one frame, its window, whose children are the elements. Each element has the role, the name, the description and the states of AT-SPI, and the interfaces that its kind takes. Every element is a component that tells its place on the screen, takes the focus and finds the element under a point. The elements with actions click, increment and decrement through the action interface. Sliders and progress bars give their number and its text through the value interface, where setting a new value moves them a step toward it. The value of another element, such as the text of a field, comes before its hint in its description. The engine announces the elements that join or leave, the names, descriptions, values and states that change and every move of the focus, as the events of AT-SPI. It builds the tree while the bus reports an assistive technology or a screen reader, or once one asked for the elements of the window. Announcements go through the announcement event of the window, polite, or assertive when the announcement interrupts. Without GIO or an accessibility bus nothing happens. |

## What follows the settings

The engine follows the settings on its own where it can, and leaves the rest to the app.

- The UI scales its text with `textScale`, until the app sets a scale with [`ui.setTextScale`](ui.md#uisettextscalescale). It draws the high contrast theme of its theme with `highContrast`, and draws the surfaces of its theme opaque with `reducedTransparency`, as the [UI guide](../ui/ui.md#accessibility-settings) describes.
- Tweens and scene transitions that opt in with their option `reducedMotion` shorten or skip their motion while `reducedMotion` is on, as the options of [tweens](tween.md#options) and [transitions](scene.md) describe. The others keep their motion, since only the app knows which motion carries meaning.
- The post-processing of world canvases shows how players with a deficiency of color vision see the app, or corrects for it, with the options `colorVision` and `correctColorVision` of [`haylen.graphics2d`](graphics2d.md#graphics2dbeginworldcamera-options). An app offers that as a setting of its own.

```lua
local scene = require('haylen.scene')
local tween = require('haylen.tween')

local logo = {y = 0}
tween.to(logo, 0.6, {y = 200}, {ease = 'outBack', reducedMotion = 'skip'})
scene.replace(require('scenes.menu'), {duration = 0.5, effect = 'slideIn', reducedMotion = 'shorten'})
```

## Events

| Event | When | Data |
| --- | --- | --- |
| `accessibilitySettingsChanged` | One or more of the settings changed. | The table that `accessibility.settings()` returns. |

```lua
local events = require('haylen.events')

events.on('accessibilitySettingsChanged', function(settings)
    print('Text scale', settings.textScale, 'screen reader', settings.screenReader)
end)
```

## Platforms

| Platform | Text scale | Reduced motion | High contrast | Reduced transparency | Screen reader |
| --- | --- | --- | --- | --- | --- |
| macOS | Always `1`, since macOS has no text size for apps. | The display options of the accessibility settings. | Increase contrast. | Reduce transparency. | VoiceOver. |
| iOS, iPadOS and Mac Catalyst | The size of body text at the preferred content size of the app, over its size at the default size. | Reduce motion. | Increase contrast. | Reduce transparency. | VoiceOver. |
| tvOS | As on iOS. | Reduce motion. | Increase contrast. | Reduce transparency. | VoiceOver. |
| Android | The font scale of the configuration. The activity handles its changes itself, because the manifest of the template lists `fontScale` in its `configChanges`. | The animator duration scale of the global settings, whose `0` means the user removed animations. | The contrast of the UI mode on Android 14 and later, and `false` before. | Always `false`, since Android has no such setting. | Touch exploration, which TalkBack and other screen readers turn on. |
| Web | The default font size of the browser, over its usual 16 pixels. | The media query `prefers-reduced-motion`. | The media query `prefers-contrast: more`. | The media query `prefers-reduced-transparency` where the browser has it, and `false` elsewhere. | Always `false`, since browsers never tell it. |
| Windows | The text size of the accessibility settings. | The animation effects of the accessibility settings. | The contrast themes. | The transparency effects of the personalization settings. | The screen reader flag that screen readers set. |
| Linux | The text scaling factor of the GNOME interface settings, through the settings portal. | The reduced motion of the appearance settings of the portal, which recent portals have. | The contrast of the appearance settings of the portal. | Always `false`, since Linux has no such setting. | The status `ScreenReaderEnabled` of the accessibility bus, which screen readers such as Orca turn on. |

Every platform reports every change while the app runs. On Linux the engine reaches the settings portal and the accessibility bus through GIO, which it loads at run time. Without GIO, the portal or the bus the settings keep their defaults.

## C++

The class `platform::Accessibility`, which `engine.getAccessibility()` returns, holds the settings and the tree.

| Member | Meaning |
| --- | --- |
| `getSettings()` | The settings, as `platform::AccessibilitySettings`. The class publishes `accessibilitySettingsChanged` when they change. |
| `getElements()`, `getFocused()` | The last tree of `platform::AccessibilityElement` records, and the id of the element with the focus. |
| `announce(text, interrupt)` | Says an announcement. |
| `perform(request)` | Queues an action of a screen reader, from any thread. |
| `setTreeEnabled(enabled)` | Turns the building of the tree on or off. |

The platform reports the settings through `NativeHost::getSystemState()` from any thread, and the host hands the engine the last report once per frame through `Host::getAccessibilitySettings()`. The UI builds the tree in `ui::AccessibilityTree`. Components join it through `Component::describe`, and parts of components through `addPart`. The platform receives the tree through `Host::updateAccessibility` and answers through `AccessibilityRelay::perform`.

```cpp
const haylen::platform::AccessibilitySettings& settings = engine.getAccessibility().getSettings();
if (settings.reducedMotion) {
    skipIntroAnimation();
}
```
