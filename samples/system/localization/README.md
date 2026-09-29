# Haylen Localization

A Lua sample with one scene per feature of [haylen.localization](../../../docs/lua-api/localization.md) in English, Portuguese, Spanish and Japanese. The menu lists the tests, each test opens as its own scene with a Back button and a language picker, and Escape, the east gamepad button or the Menu button of a TV remote return to the menu. Every text of the tests is a translation such as `{key = 'menu.play'}`, which [haylen.ui](../../../docs/lua-api/ui.md) resolves every time it draws, so a new language shows at once.

| Test | What it shows |
| --- | --- |
| Switching languages | A title screen whose every text follows `localization.setLanguage` at once, with an argument filled from another translation and a timer that cycles the languages. |
| Arguments | `{name}` placeholders filled with a name, whole and fractional numbers and a boolean, `{{` and `}}` writing braces, a placeholder without an argument staying as it is, and the template of every key next to its result. |
| Plurals | The `zero`, `one` and `other` forms picked by a count, with Japanese forms that have no `one` text, in live labels and in a table of several counts. |
| Nested keys | The groups of a language file as a tree, read with dotted keys such as `story.chapters.first.title`, and `localization.has` telling keys from groups. |
| Fallback language | Keys the current language lacks taken from the fallback language, which the test changes with `localization.setFallback`, and a key no language has showing itself. |
| Best match | The language of the device from the `system.locale` call of [haylen.platform](../../../docs/lua-api/platform.md), matched with `localization.bestMatch`, and a table of other tags with the reason of each match. |
| Fonts per language | The interface switching to a theme whose fonts are M PLUS 1p for Japanese, rich text drawn from a [font family](../../../docs/lua-api/graphics.md#graphicsnewfontfamilyfaces) whose fallback font fills in the Japanese letters the default font lacks, and the same line without the fallback. |
| Layout follows the text | Buttons, a wrapped paragraph and a row that take new sizes when the language changes, with the bounds of each node read live. |

## Languages and fonts

`source/language.lua` loads the files of `content/locale` with `localization.loadFolder`, makes English the fallback and registers two UI fonts. The Latin languages use a family with the default font as its regular face and M PLUS 1p as its fallback, and Japanese uses M PLUS 1p itself. Widgets such as labels and buttons draw with the regular face of their theme font only, while rich text uses the whole family, so picking Japanese also switches the theme to the one whose fonts have Japanese letters. The names of the languages in the picker are translations too, so each one is written in the current language and its font.

The sample has no right-to-left language. The text layout of the engine has no bidirectional reordering or shaping, so Arabic or Hebrew would draw their letters unjoined and from left to right, and the sample leaves them out rather than show them wrong.

The Japanese font is M PLUS 1p under the SIL Open Font License, listed in [content/CREDITS.md](content/CREDITS.md).

## Running it

| Where | Command |
| --- | --- |
| Desktop player with hot reload | `python3 make.py run samples/system/localization` |
| macOS app | `python3 make.py run samples/system/localization --platform macos` |
| iPhone and iPad simulator | `python3 make.py run samples/system/localization --platform ios-simulator` |
| Apple TV simulator | `python3 make.py run samples/system/localization --platform tvos-simulator` |
| Android device or emulator | `python3 make.py run samples/system/localization --platform android --device <serial>` |
| Browser | `python3 make.py run samples/system/localization --platform web` |

## Controls

| Action | Keyboard and mouse | Gamepad | Touch | TV remote |
| --- | --- | --- | --- | --- |
| Pick a test or a control | Arrows and Enter, or click | Directional pad and south button | Tap | Swipe and select |
| Next and previous language | L and K, or the picker | Right and left shoulders, or the picker | The picker | The picker, with left and right |
| Back to the menu | Escape or the Back button | East button | Back button | Menu |
