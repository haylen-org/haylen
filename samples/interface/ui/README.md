# Haylen UI

Haylen UI is a Lua sample of `haylen.ui`, the retained interface of the engine: every component kind, the themes with a textured theme of nine-slice surfaces, focus navigation for keyboards, gamepads and TV remotes, native tweens of node transforms and text entry through the hidden native field of each platform. A menu lists one test per group of components, each test is a scene with a Back button, and Escape, the east or Back button of a gamepad, the Menu button of an Apple TV remote or the Back button of an Android device returns to the menu once no popup, dialog or focus scope takes the press.

| Test | What it shows |
| --- | --- |
| Containers | `row`, `column`, `grid`, `stack`, `scroll`, `card`, `panel`, `spacer`, `divider`, `tabs`, `formField` with validation and `splitter`. `safeArea` is shown in the `interface/safe-area` sample. |
| Text | `label` in every font role and theme color, alignment, wrapping and ellipsis, outlines, `pageHeader` with a banner, `sectionTitle`, `emptyState` and `alert` in every tone. |
| Buttons | Every `button` variant with icons, checked and disabled states, `imageButton` with hover and pressed pictures, `chip` that toggles or removes itself and `menuButton`. |
| Choices | `checkbox`, `toggle` and `radioGroup`, vertical and horizontal, with disabled options. |
| Text fields | `textField` with every keyboard (`text`, `number`, `decimal`, `phone`, `email`, `url`, `search`) and every return key label, length limits and capitalization, `secretField`, `textArea` and `filterField`. |
| Pickers | `combo` with placeholders and disabled items, `colorField` with and without opacity, `numberField` and `slider`. |
| Indicators | `badge`, `statusIndicator`, `busyIndicator`, `progress`, `circularProgress` as a ring and as ability cooldowns, `icon`, `image` with every fit and `avatar`. |
| Collections | `list` with pictures and captions, a draggable `list` the player reorders, `tree` and `table`. |
| Settings | A settings screen from `settingsForm`, `settingsRow` and `settingsActions` with save, cancel and defaults. |
| Overlays | `dialog` with children, dismissible or not, `toast` at the top and bottom of the safe area, tooltips, `popover` and `contextMenu`. |
| Game controls | `stepper` of numbers and options, `segmentedControl`, `rangeSlider` and `keyCapture` fields that rebind actions of the action map. |
| Windows and pages | A draggable `window` holding an `accordion` with several open sections, an `accordion`, a `carousel` and a horizontal `scroll` that snaps to its cards. |
| Slot grid | An inventory `slotGrid`, a hotbar in a second document and a draggable chest `list` trading items by drag and drop, with the pointer and by carrying items with keys, gamepads and remotes. |
| Rich text | `richText` with styles, colors, outlines, shadows, glows, links, hints, inline images and icons, lists, rules, tables, effects, fill alignment and a typewriter dialogue. |
| Themes | The built-in `dark` and `light` themes and the `parchment` theme of this sample, loaded from `content/themes/parchment.json`, whose surfaces cut one atlas into nine-slices. |
| Focus navigation | The nearest control in a direction, explicit neighbours, a focus scope, a wrapping row and the way back, with the bindings of keyboards, gamepads and TV remotes and the state of the focus ring. |
| UI tweens | `document:transform` animated by `haylen.tween`: an entrance with a stagger, a shake, a fade, a tint and a pulse that never ends. |
| Text input | The hidden native field of each platform, the events of the on-screen keyboard, the UI moving above it and the plain keyboard of `window.setKeyboardVisible`. |
| Touch controls | `touchStick` and `touchButton` driving actions that keys and a gamepad drive too. |

## Controls

The mouse, touch, the keyboard, gamepads and TV remotes reach every control. The arrow keys, the directional pad, the left stick and the Siri Remote move the focus, Enter, Space, the south button and a remote click press the focused control, and Tab walks the controls in drawing order. The hint line at the bottom of each test lists what it adds.

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run interface/ui` |
| macOS app | `python3 make.py run interface/ui --platform macos` |
| iPhone and iPad simulator | `python3 make.py run interface/ui --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run interface/ui --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run interface/ui --platform android` |
| Browser | `python3 make.py run interface/ui --platform web` |

## Package layout

```text
ui/
  app.json               Window, design resolution of 1920 by 1080 and identifier.
  source/
    main.lua             Adds the Back button of gamepads to uiCancel, loads the parchment theme and opens the menu.
    tests.lua            The tests in menu order.
    sample.lua           The frame of every test with its Back button and hints, and the way back to the menu.
    scenes/menu.lua      The menu.
    tests/               One scene per test.
  content/
    themes/              The parchment theme and its atlas.
    icons/, images/      Item icons, landscapes, avatars and the wooden sign.
    fonts/               The Kenney Future fonts of the parchment theme.
  tools/                 The generator of the art, which is not part of the package.
```

`python3 samples/interface/ui/tools/generate_content.py` draws the theme atlas, the icons and the pictures again and writes the theme file.
