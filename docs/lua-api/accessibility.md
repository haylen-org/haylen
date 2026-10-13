# haylen.accessibility

`haylen.accessibility` follows the accessibility settings of the system, how large the user wants text, whether they want less motion, higher contrast or less transparency and whether a screen reader runs, and opens the UI to screen readers, which read the elements of every GUI aloud and act on them. Use it to calm the motion of an app, to pick a theme with stronger colors, to announce what changed and to check what a screen reader reads.

```lua
local accessibility = require('haylen.accessibility')
```

The platform reports the settings when the app starts and whenever the user changes them, and the engine publishes each change once, at the start of the next frame, as the `accessibilitySettingsChanged` [event](#events).

## Functions

### accessibility.settings()

Returns a new table with the settings as the app last heard of them at the start of a frame. A platform without a setting reports its default, as [Platforms](#platforms) lists.

| Field | Type | Meaning |
| --- | --- | --- |
| `textScale` | number | How much larger the user wants text than its normal size, where `1` is the normal size, such as `1.3` for the larger text sizes of a phone. |
| `reducedMotion` | boolean | Whether the user wants less motion, such as no sliding, zooming or parallax. |
| `highContrast` | boolean | Whether the user wants higher contrast between colors. |
| `reducedTransparency` | boolean | Whether the user wants opaque surfaces instead of translucent ones. |
| `screenReader` | boolean | Whether a screen reader runs, which reads the interface aloud and moves through it by its own gestures. |

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

Asks the screen reader to say a text, such as a change that shows nowhere a reader moves to, after what it says now. The option `interrupt`, `false` by default, makes it stop what it says and say the text at once. Where no screen reader runs, nothing is said. An empty text raises `An announcement needs a text.` and an unknown option raises `Unknown option "<key>".`.

```lua
local accessibility = require('haylen.accessibility')

accessibility.announce('Level 3 is ready.')
accessibility.announce('Game over.', {interrupt = true})
```

### accessibility.elements()

Returns the elements of the [screen reader tree](#screen-readers) of the last frame, in the order a screen reader moves through them, as a list of tables. The UI builds the tree only while a screen reader asks for it or `accessibility.setTreeEnabled(true)` turned it on, and the list is empty before the first frame that built it.

| Field | Type | Meaning |
| --- | --- | --- |
| `id` | integer | The element, which stays the same while its node stays where it is. |
| `parent` | integer | The element that holds it, or `0` at the top. |
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

Returns the id of the element that holds the focus in the last tree, or `nil`.

### accessibility.perform(id, action)

Asks an element for an action the way a screen reader does: `'focus'` moves the focus of the UI to it, `'activate'` presses it as the accept button does, and `'increment'` and `'decrement'` raise or lower a slider, a stepper or a segmented control. The UI performs it at its next frame, and an element that is no longer there ignores it. Tests use it to drive the UI as a screen reader would. An unknown action raises `The accessibility action "<action>" is unknown. It is "focus", "activate", "increment" or "decrement".`.

```lua
local accessibility = require('haylen.accessibility')

for _, element in ipairs(accessibility.elements()) do
    if element.label == 'Play' then
        accessibility.perform(element.id, 'activate')
    end
end
```

### accessibility.setTreeEnabled(enabled) and accessibility.treeEnabled()

Turns on the building of the tree in every frame, also while no screen reader asks for it, such as for tests and tools, and tells whether it is on. It starts off.

## Screen readers

The UI of [`haylen.ui`](ui.md) draws itself, so the engine describes it to screen readers: while a screen reader asks for it, the GUIs build a tree of elements as they draw, and the engine hands it to the platform whenever it changes, so a menu that stays as it is costs nothing beyond the drawing.

- Every kind of component describes itself with its role, label, value and state: a `button` is a button labeled by its text, a `toggle` a switch that is checked or not, a `slider` a slider with its value, a `textField` a text field with its text, a `secretField` one with a bullet for each character, a `progress` a progress bar with its percentage, a `label` and a `richText` text without their markup, a `pageHeader` and a `sectionTitle` headings, an `alert` and a `toast` alerts, a `dialog` and a `window` dialogs, and a `list`, a `table`, a `tree` and a `slotGrid` lists. The options of a `radioGroup`, the tabs of `tabs`, the rows of a `list`, the headers of an `accordion` and the buttons of a `dialog` are elements of their own.
- Containers are no elements, so their children join the element around them, and an `image` or an `icon` is decorative until it has a label.
- The [common properties](ui.md#common-properties) `accessibilityLabel`, `accessibilityHint`, `accessibilityRole` and `accessibilityHidden` give a node the label, the hint and the role screen readers say instead of what its kind describes, or leave it and its children out. A node with a label or a role of its own is an element even when its kind is not, such as a `card` that stands for one thing. A disabled node is disabled for screen readers too, and a tooltip is its hint unless it has one of its own.
- The focus stays in step: the element of the control that holds the focus of the UI holds the focus of the screen reader, and a screen reader that moves its focus to an element, presses it or adjusts it moves the focus of the UI there, so the player who listens and the one who looks share one focus.

| Platform | Screen readers |
| --- | --- |
| macOS | VoiceOver and every assistive technology of AppKit, such as Accessibility Inspector, through a view over the view of the app that serves the elements as `NSAccessibilityElement` objects with the roles, values and actions of AppKit. The engine builds the tree once an assistive technology asked the view for its elements or VoiceOver runs, and announcements go through `NSAccessibilityAnnouncementRequestedNotification`. |
| iOS, iPadOS, tvOS and Mac Catalyst | VoiceOver, Switch Control and the accessibility audits of Xcode, through a view over the view of the app whose accessibility elements are `UIAccessibilityElement` objects with the traits of UIKit, where sliders and steppers are adjustable and a play area allows direct interaction, so a game takes its touches while VoiceOver runs. The engine builds the tree once an assistive technology asked the view for its elements, VoiceOver or Switch Control runs, and announcements go through `UIAccessibilityAnnouncementNotification`. |
| Android | TalkBack, Switch Access and every accessibility service, through the virtual views that an `ExploreByTouchHelper` of AndroidX serves for the surface of the app, with the classes of the Android widgets for the roles, so services explore by touch, click, scroll sliders and move their accessibility focus, which moves the focus of the UI. The engine builds the tree while an accessibility service runs or once one asked for the views, and announcements go through `View.announceForAccessibility`, which has no way to interrupt. |
| Windows | Not supported yet. Windows reads apps through UI Automation, whose providers are COM objects that a window serves for every element, and the engine does not serve them yet. |
| Linux | Not supported yet. Linux reads apps through AT-SPI, a protocol of the accessibility bus of D-Bus that an app serves for every element, and the engine does not serve it yet. |

## What follows the settings

The engine follows the settings on its own where it can, and leaves the rest to the app:

- The UI scales its text with `textScale` until the app sets a scale with [`ui.setTextScale`](ui.md#uisettextscalescale), draws the high contrast theme of its theme with `highContrast` and draws the surfaces of its theme opaque with `reducedTransparency`, as [accessibility settings](../ui/ui.md#accessibility-settings) of the UI guide describes.
- Tweens and scene transitions that opt in with their `reducedMotion` option shorten or skip their motion with `reducedMotion`, as the options of [tweens](tween.md#options) and [transitions](scene.md) describe, while the others keep their motion, since only the app knows which motion carries meaning.

```lua
local scene = require('haylen.scene')
local tween = require('haylen.tween')

tween.to(logo, 0.6, {y = 200}, {ease = 'outBack', reducedMotion = 'skip'})
scene.replace(require('scenes.menu'), {duration = 0.5, effect = 'slideIn', reducedMotion = 'shorten'})
```

## Events

| Event | When | Data |
| --- | --- | --- |
| `accessibilitySettingsChanged` | One or more of the settings changed. | The table `accessibility.settings()` returns. |

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
| iOS, iPadOS and Mac Catalyst | The size of body text at the preferred content size of the app over its size at the default size. | Reduce motion. | Increase contrast. | Reduce transparency. | VoiceOver. |
| tvOS | As iOS. | Reduce motion. | Increase contrast. | Reduce transparency. | VoiceOver. |
| Android | The font scale of the configuration, which the activity handles itself because the manifest of the template lists `fontScale` in its `configChanges`. | The animator duration scale of the global settings, whose `0` means the user removed animations. | The contrast of the UI mode on Android 14 and later, and `false` before. | Always `false`, since Android has no such setting. | Touch exploration, which TalkBack and other screen readers turn on. |
| Web | The default font size of the browser over its usual 16 pixels. | The `prefers-reduced-motion` media query. | The `prefers-contrast: more` media query. | The `prefers-reduced-transparency` media query where the browser has it, and `false` elsewhere. | Always `false`, since browsers never tell it. |
| Windows | The text size of the accessibility settings. | The animation effects of the accessibility settings. | The contrast themes. | The transparency effects of the personalization settings. | The screen reader flag that screen readers set. |
| Linux | The text scaling factor of the GNOME interface settings through the settings portal. | The reduced motion of the appearance settings of the portal, which recent portals have. | The contrast of the appearance settings of the portal. | Always `false`, since Linux has no such setting. | Always `false`, since the portal never tells it. |

Every platform reports every change while the app runs. On Linux the engine reaches the settings portal through GIO, which it loads at run time, and without GIO or the portal the settings keep their defaults.

## C++

`platform::Accessibility`, which `engine.getAccessibility()` returns, holds the settings as `platform::AccessibilitySettings` with `getSettings()` and publishes `accessibilitySettingsChanged`. The platform reports the settings through `SokolHost::getSystemState()` from any thread, and the host hands the engine the last report once per frame through `Host::getAccessibilitySettings()`. It also holds the last tree of `platform::AccessibilityElement` records with `getElements()` and `getFocused()`, says announcements with `announce(text, interrupt)`, queues the actions of screen readers with `perform(request)` from any thread and turns the tree on with `setTreeEnabled`. The UI builds the tree in `ui::AccessibilityTree`, which components join through `Component::describe` and parts of components through `addPart`, and the platform receives it through `Host::updateAccessibility` and answers through `AccessibilityRelay::perform`.

```cpp
const haylen::platform::AccessibilitySettings& settings = engine.getAccessibility().getSettings();
if (settings.reducedMotion) {
    skipIntroAnimation();
}
```
