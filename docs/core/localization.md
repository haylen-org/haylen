# 🌍 Localization

This guide explains how a Haylen app ships its texts in several languages. It covers the files of each language, the section of `app.json` that loads them, the form of language tags, how the engine picks the current language from the device or from the choice of the app, where each platform keeps the languages of the person, and how the app reacts when the language changes. The [`haylen.localization`](../lua-api/localization.md) reference lists every function, option and error.

## Texts per language

An app keeps its texts out of its code. Each language has one JSON file, and every file uses the same keys, such as `menu.play` or `hud.day`. The code asks for a key, and the engine answers with the text in the current language.

```text
content/
  locale/
    en.json
    pt-BR.json
    es.json
```

A file nests its keys in groups, and a key may hold a plural form or placeholders that the code fills. Here is `content/locale/en.json`:

```json
{
  "menu": {"play": "Play", "quit": "Quit"},
  "hud": {
    "day": "Day {day}",
    "wood": {"one": "{count} log", "other": "{count} logs"}
  }
}
```

And here is how the code shows those texts, in a GUI and on the screen:

```lua
local graphics2d = require('haylen.graphics2d')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

ui.button{text = {key = 'menu.play'}}
graphics2d.drawText(nil, localization.text('hud.wood', {count = 3}), 20, 20)
```

A GUI resolves a text written as `{key = ...}` each time it draws, so it never needs to be built again for another language. Text drawn by the code itself is asked for again on the next frame, so it follows the language too.

## Configuring the languages

The section `localization` of `app.json` loads the folder of the languages before any Lua runs. So the first scene already shows the right language, and an autoload can read texts in its `start` hook.

```json
{
    "localization": {
        "folder": "locale",
        "fallback": "en"
    }
}
```

- The `folder` is relative to `content/`. Every `.json` file in it becomes the language that its name tells.
- The `fallback` gives every text that another language lacks. Without it, the first file in the folder, in the order of the names, is the fallback.
- The `language` picks the language of the app from the start, such as `"pt-BR"`. Most apps leave it out, so they follow the device.

The engine checks the section when the app starts, so a misspelled tag, a fallback without its file or an empty folder shows on the error screen with a sentence that says what to fix. The [`app.json` reference](../lua-api/haylen.md#localization) lists the fields and their messages. A folder that the app loads later, such as the dialogue of an expansion, joins the same catalog through `localization.loadFolder`.

## Language tags

A language tag names a language, and maybe its script and its region: `en`, `en-US`, `pt-BR`, `zh-Hans-CN`. Platforms, files and people write tags in many ways, such as `pt_BR`, `PT-br` or `pt-BR`. The engine takes all of them and keeps one canonical form, the one of BCP 47:

| Written | Canonical |
| --- | --- |
| `en`, `EN` | `en` |
| `en_us`, `EN-us` | `en-US` |
| `pt_BR`, `pt-br` | `pt-BR` |
| `zh_hans_cn` | `zh-Hans-CN` |
| `es-419` | `es-419` |
| `en-US-u-mu-celsius` | `en-US` |

The last row shows an extension, the units that the person prefers on Android. Extensions do not change the language, so the canonical form drops them. A text that is no tag at all, such as `English` or `pt BR`, is an error. Every function of `haylen.localization` and every list of `haylen.system` gives tags back in the canonical form, so the code compares them as plain strings.

## Choosing the language

The engine keeps one current language. It comes from one of two places.

- **The device.** Every app starts by following the languages that the person prefers on the device, in their order. The first of them that the app can serve wins. If the person changes the languages while the app runs, the current language follows.
- **The app.** A call of `localization.setLanguage` picks a language, such as the one the player chose in the settings. From then on, the device no longer decides. A call of `localization.followSystem` gives the decision back to the device.

In both cases the wanted tag may not be one of the files of the app, so the engine looks for the closest one. Say the app has `en`, `pt-BR`, `es-MX` and `zh-Hans`.

1. The same tag wins. A device in `es-MX` gets `es-MX`.
2. Then a shorter tag, made by dropping the last part, again and again. A device in `zh-Hans-SG` gets `zh-Hans`.
3. Then another region of the same language, the one the app added first. A device in `pt-PT` gets `pt-BR`, and one in `es-ES` gets `es-MX`. Scripts must agree where both tags name one, so a device in `zh-Hant-TW` does not get `zh-Hans`.
4. Then the fallback language. A device in `de-DE` gets `en`.

While the app follows the device, the engine tries the steps for the first language of the person, then for the second, and so on, before it takes the fallback. So a person who prefers `de-DE` and then `es-ES` gets `es-MX`, not English.

The choice of the app stays when languages are added later. So an app can pick the stored choice of the player first and load an extra folder after, and the current language moves to the better match as soon as it is there.

```lua
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')

-- The player picked a language in a previous session, or the device decides.
local chosen = preferences.get('settings.language')
if chosen then
    localization.setLanguage(chosen)
else
    localization.followSystem()
end
```

## Where the languages come from

Each platform keeps the languages of the person in its own place. The engine reads them through the API of each platform, and [`system.languages()`](../lua-api/system.md#systemlanguages) gives the list to the app.

| Platform | Where the engine reads them | How a change reaches a running app |
| --- | --- | --- |
| macOS and Mac Catalyst | `NSLocale.preferredLanguages`, which holds the language order of the settings, the language that the settings give the app alone and the launch argument `-AppleLanguages`. | The engine reads the list again whenever the system posts `NSCurrentLocaleDidChangeNotification`. |
| iOS, iPadOS, tvOS and visionOS | `NSLocale.preferredLanguages`, as on macOS. | The system ends the app when the person changes the language of the device or of the app, so the app starts again with the new list. A change of the region posts `NSCurrentLocaleDidChangeNotification`, which the engine follows. |
| Android | The locales of the configuration of the activity, which are the languages of the system or, from Android 13, the language that the settings give the app alone. | The activity handles the change itself, because the manifest of the template lists `locale` in its `configChanges`, and reports the new list. |
| Web | `navigator.languages`, the languages of the settings of the browser. | The event `languagechange` of the page. |
| Windows | `GetUserPreferredUILanguages`, the display languages of the user. | The message `WM_SETTINGCHANGE` with `intl`, which Windows sends when the language settings change. |
| Linux | The variables of the environment, the way gettext reads them: the list of `LANGUAGE`, then the first of `LC_ALL`, `LC_MESSAGES` and `LANG` that is set. A locale of `C` or `POSIX` names no language, and then `LANGUAGE` is not read. | It never does. The session sets the environment of a process when the process starts, so a new language reaches the app at its next start. |

The settings of iOS show a language row for an app only when its `Info.plist` lists its languages in `CFBundleLocalizations`. The settings of Android 13 and later show one only when the manifest names a locale config with `android:localeConfig`. The engine reads the result either way.

To try another language without changing the device, start the app with the language of the platform:

| Platform | How |
| --- | --- |
| macOS | Start the player with `-AppleLanguages '(pt-BR)'` after its arguments. |
| iOS simulator | Run `xcrun simctl launch <device> <identifier> -AppleLanguages '(pt-BR)'`. |
| Android emulator | Run `adb shell cmd locale set-app-locales <package> --locales pt-BR` on Android 13 and later, and `--locales ''` to undo it. |
| Web | Start the browser with `--lang=pt-BR`, as Chrome takes it. |
| Linux | Start the app with `LANG=pt_BR.UTF-8`. |

## Reacting to a change

A GUI shows a new language on its next frame by itself. Code that keeps a text, such as one drawn into a texture or a list of names that it built once, listens to the event `languageChanged`. It arrives at the start of the frame after the change, whatever caused it: the app, the device or a language added later. The event `systemLanguagesChanged` of `haylen.system` tells of a change of the list of the device, even when the app chose its own language and the current language stays.

```lua
local events = require('haylen.events')
local localization = require('haylen.localization')

local title = localization.text('menu.title')
events.on('languageChanged', function(change)
    title = localization.text('menu.title')
    print('The texts are now in ' .. change.language .. '.')
end)
```

A settings screen usually offers the languages of the app and one more entry that follows the device. The entry calls `localization.followSystem`, and the others call `localization.setLanguage` and keep the choice in [`haylen.preferences`](../lua-api/preferences.md).

## Plurals and placeholders

A placeholder such as `{name}` takes the value of the argument of that name. A key whose value lists plural categories, such as `one` and `other`, picks its form from the argument `count`, by the rules of the language that holds the text. Russian, Arabic and Polish have more forms than English, and the rules of the engine know them. The [text section](../lua-api/localization.md#localizationtextkey-arguments) of the reference shows every case.

## Numbers, dates and lists

The functions `formatNumber`, `formatDate` and `formatList` write values with the marks and the words of the current language, such as `1.234,5` in German and `1,234.5` in English. They also take a `language` option for a value in another language. The [reference](../lua-api/localization.md#plurals-numbers-dates-and-lists) lists the languages they know.

## Assets per language

Voiced lines and pictures with words can come in each language too. The function `localization.assetPath` looks for the asset under `i18n/<language>/` of the current language, then under its shorter tags, and gives the common path when no language has its own copy.

```lua
local assets = require('haylen.assets')
local localization = require('haylen.localization')

local intro = assets.load(localization.assetPath('voice/intro.ogg'))
```

## Testing the translations

The function `localization.setPseudo(true)` turns every text of the catalog into an accented, longer text between guillemets. Labels that cut their text, buttons that outgrow their space and fonts that lack a letter show at once, before the first translation exists. A text without the guillemets never went through the catalog. The tests of the category `LOC` of the test project show every part of this guide working on each platform.
