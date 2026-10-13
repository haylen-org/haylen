# haylen.accessibility

`haylen.accessibility` follows the accessibility settings of the system: how large the user wants text, whether they want less motion, higher contrast or less transparency, and whether a screen reader runs. Use it to calm the motion of an app, to pick a theme with stronger colors and to describe more aloud when a screen reader runs.

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

`platform::Accessibility`, which `engine.getAccessibility()` returns, holds the settings as `platform::AccessibilitySettings` with `getSettings()` and publishes `accessibilitySettingsChanged`. The platform reports the settings through `SokolHost::getSystemState()` from any thread, and the host hands the engine the last report once per frame through `Host::getAccessibilitySettings()`.

```cpp
const haylen::platform::AccessibilitySettings& settings = engine.getAccessibility().getSettings();
if (settings.reducedMotion) {
    skipIntroAnimation();
}
```
