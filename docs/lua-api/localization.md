# haylen.localization

Translated text by key, with placeholders, plural forms by the rules of each language, a fallback language, and numbers, dates and lists written the way each language writes them. Use it for every piece of text the player reads, so the app can ship in several languages and switch between them at runtime.

```lua
local localization = require('haylen.localization')
```

## Language tables

A language table maps keys to texts. Nested tables become dotted keys, so the table below defines `menu.play`, `menu.quit`, `hud.day` and `hud.wood`. A table whose keys are only the plural categories `zero`, `one`, `two`, `few`, `many` and `other`, and always `other`, is a plural form instead of a group. Any other value raises `The localization entry "<key>" must be text, a plural form or a group of entries.`.

```json
{
  "menu": {"play": "Play", "quit": "Quit"},
  "hud": {
    "day": "Day {day}",
    "wood": {"zero": "No wood", "one": "{count} log", "other": "{count} logs"}
  }
}
```

A language that reads right to left declares it with `"@direction": "rightToLeft"` at the top of its table, and `"leftToRight"` is the default. The key holds no text, `localization.direction()` returns it, and a UI that follows the language with [`ui.setDirection('auto')`](ui.md#uisetdirectiondirection) mirrors its layout when the language becomes current. Any other value raises `The "@direction" of the localization table of "<language>" must be "leftToRight" or "rightToLeft".`.

```json
{
  "@direction": "rightToLeft",
  "menu": {"play": "العب", "quit": "خروج"}
}
```

The usual layout keeps one JSON file per language in a folder of the assets, named after its language tag, such as `i18n/en.json` and `i18n/pt-BR.json`.

The first language added becomes both the current language and the fallback language until others are chosen. Keys missing from the current language come from the fallback language, and keys missing from both come back unchanged, so a missing translation shows its key on screen.

GUIs from [`haylen.ui`](ui.md) translate any text given as `{key = 'menu.play'}` or `{key = 'hud.day', args = {day = 3}}` through this module.

## Functions

### localization.loadFolder(folder)

Adds every `.json` file under a folder of the assets, in subfolders too, as the language its file is named after, and returns the list of languages in file order. Other files are ignored. A file that is not valid JSON raises `The localization file "<path>" is not valid JSON.`.

```lua
local localization = require('haylen.localization')

local languages = localization.loadFolder('i18n')
print(table.concat(languages, ', '))
```

### localization.add(language, table)

Adds a language table, merging it into the language when it already exists. An empty language raises `A localization table needs a language.`, and a table that is a list raises `The localization table of "<language>" must be a JSON object.`.

```lua
local localization = require('haylen.localization')

localization.add('en', {menu = {play = 'Play', quit = 'Quit'}})
localization.add('fr', {menu = {play = 'Jouer', quit = 'Quitter'}})
localization.add('en', {menu = {settings = 'Settings'}})
```

### localization.setLanguage(language)

Makes a language current. A language that was never added raises `No localization table was added for "<language>".`.

```lua
local localization = require('haylen.localization')

localization.loadFolder('i18n')
localization.setLanguage('pt-BR')
```

### localization.language()

Returns the current language, or an empty string before any language is added.

```lua
local localization = require('haylen.localization')

localization.loadFolder('i18n')
print(localization.language())
```

### localization.direction(language)

Returns `'rightToLeft'` when a language declares that it reads right to left with `@direction`, and `'leftToRight'` otherwise, for the current language when `language` is omitted. Text laid out in the language takes its direction from its own letters, and the direction of a language decides the layout of a UI around it. A language that was never added raises `No localization table was added for "<language>".`, and so does a call without `language` before any language is added, which names the empty current language.

```lua
local localization = require('haylen.localization')

localization.add('en', {hello = 'Hello'})
localization.add('ar', {['@direction'] = 'rightToLeft', hello = 'مرحبا'})
print(localization.direction(), localization.direction('ar'))
```

### localization.setFallback(language)

Makes a language the fallback for keys the current language lacks. A language that was never added raises `No localization table was added for "<language>".`.

```lua
local localization = require('haylen.localization')

localization.loadFolder('i18n')
localization.setFallback('en')
```

### localization.fallback()

Returns the fallback language, or an empty string before any language is added.

```lua
local localization = require('haylen.localization')

localization.loadFolder('i18n')
print(localization.fallback())
```

### localization.languages()

Returns a sorted list of every language added.

```lua
local localization = require('haylen.localization')

localization.loadFolder('i18n')
for _, language in ipairs(localization.languages()) do
    print(language)
end
```

### localization.has(key)

Returns `true` when the current or the fallback language has a text or plural form for the key. Groups such as `menu` are not keys.

```lua
local localization = require('haylen.localization')

localization.add('en', {tutorial = {jump = 'Press {button} to jump'}})
if localization.has('tutorial.jump') then
    print(localization.text('tutorial.jump', {button = 'Space'}))
end
```

### localization.text(key, arguments)

Returns the translated text of the key. The optional arguments table fills `{name}` placeholders with its values, and `{{` and `}}` produce single braces. A placeholder without a matching argument stays in the text as it is. Whole numbers print without decimals, other numbers print as JSON numbers such as `2.5`, and booleans print as `true` or `false`.

For a plural form, the `count` argument picks the text by the plural rules of the language that holds the form, as [`localization.pluralCategory()`](#localizationpluralcategorycount-options) returns them, so Russian picks `one` for 21, `few` for 3 and `many` for 5. A `zero` text takes a count of 0 whatever the rules say, a category the form lacks falls back to `other`, and so does a missing `count`. The argument `ordinal = true` picks the text by the ordinal rules instead, for ranks such as `1st`, `2nd` and `3rd`. Arguments given as a list raise `Localization arguments must be a JSON object.`.

```lua
local localization = require('haylen.localization')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')

localization.add('en', {
    hud = {day = 'Day {day}', wood = {zero = 'No wood', one = '{count} log', other = '{count} logs'}},
    hint = 'Use {{ and }} for braces',
})

scene.push({
    enter = function(self)
        self.day = 3
        self.wood = 1
    end,
    render = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, localization.text('hud.day', {day = self.day}), 20, 20)
        graphics2d.drawText(nil, localization.text('hud.wood', {count = self.wood}), 20, 60)
        graphics2d.drawText(nil, localization.text('hint'), 20, 100)
    end,
})
```

### localization.findBestMatch(tag)

Returns the added language that best matches a BCP 47 language tag, or `nil` when none matches. The match ignores case and treats `_` like `-`. An exact match wins, and otherwise a language with the same base language matches, preferring the plain base language, so `pt-BR` matches `pt` before `pt-PT`.

The device language comes from the `locale` of `system.info()` in [`haylen.system`](system.md#systeminfo), a tag such as `pt-BR`, and its `languages` list the other languages the player prefers.

```lua
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')
local system = require('haylen.system')

localization.loadFolder('i18n')
localization.setFallback('en')

local chosen = preferences.get('game.language')
if chosen then
    localization.setLanguage(chosen)
else
    local tag = system.info().locale
    local language = tag and localization.findBestMatch(tag)
    if language then
        localization.setLanguage(language)
    end
end
```

### localization.pluralCategory(count, options)

Returns the plural category of a count, `'zero'`, `'one'`, `'two'`, `'few'`, `'many'` or `'other'`, by the rules of the current language or of the option `language`, and by the ordinal rules with `ordinal = true`. The rules cover English, the Germanic, Romance, Slavic and Baltic languages of Europe, Greek, Hungarian, Turkish, Romanian, Arabic, Hebrew, Hindi, Bengali, Welsh and Irish, and every other language, such as Japanese, Chinese and Korean, uses `other`. Ordinal rules cover English, French, Italian, Swedish, Catalan, Hungarian, Romanian, Vietnamese and Malay.

```lua
local localization = require('haylen.localization')

print(localization.pluralCategory(3, {language = 'ru'}))
print(localization.pluralCategory(22, {ordinal = true, language = 'en'}))
```

### localization.formatNumber(value, options)

Writes a number with the decimal mark, the grouping and the signs of the current language or of the option `language`.

| Option | Default | Meaning |
| --- | --- | --- |
| `style` | `'decimal'` | `'decimal'`, `'percent'` for a ratio, so 0.25 is 25 percent, `'currency'` for an amount, or `'compact'` for a short form such as `1.2K`, `1,2 Mio.` or `1.2万`. |
| `minimumFractionDigits`, `maximumFractionDigits` | 0 and 3 for decimals, 0 for percentages, the digits of the currency for amounts, one decimal below ten for compact numbers | From 0 to 20. |
| `currency` | `'USD'` | The ISO code of the currency, whose symbol the language places, or the code itself for a currency the engine does not know. |
| `grouping` | `true` | Whether thousands are grouped. |
| `language` | the current language | The language whose marks to use. |

A number that is not finite raises `A number to format must be finite.`, another style raises `The style of a number must be "decimal", "percent", "currency" or "compact".` and fraction digits out of range raise `Fraction digits run from 0 to 20, with the minimum not above the maximum.`.

```lua
local localization = require('haylen.localization')

print(localization.formatNumber(1234567.5, {language = 'de'}))
print(localization.formatNumber(0.25, {style = 'percent'}))
print(localization.formatNumber(9.99, {style = 'currency', currency = 'EUR', language = 'fr'}))
print(localization.formatNumber(1500000, {style = 'compact'}))
```

### localization.formatDate(seconds, options)

Writes a moment in Unix seconds, such as `os.time()`, as a date, a time or both. The option `date` is `'short'`, `'medium'` by default, `'long'`, `'full'` with the day of the week, or `false` for the time alone, `time = true` adds the time in the clock of the language, `timeZoneOffset` gives a fixed offset from UTC in minutes instead of the time zone of the device, and `language` another language. English, Spanish, Portuguese, French, German, Italian, Dutch, Swedish, Danish, Norwegian, Finnish, Polish, Czech, Russian, Ukrainian, Turkish, Japanese, Chinese and Korean write the names of months and days, and the other languages write every style with numbers. Another date style raises `The date of a date format must be "short", "medium", "long", "full" or false.` and an offset beyond 18 hours raises `A time zone offset runs from -1080 to 1080 minutes.`.

```lua
local localization = require('haylen.localization')

print(localization.formatDate(os.time(), {date = 'long', time = true}))
print(localization.formatDate(os.time(), {date = 'short', language = 'en-GB'}))
```

### localization.formatList(items, options)

Joins a list of strings with the separators of the language, such as `apples, pears, and plums` in English, `apples, pears and plums` in British English or `apples、pears、plums` in Japanese. The option `type` is `'and'` by default or `'or'`, and another type raises `The type of a list must be "and" or "or".`.

```lua
local localization = require('haylen.localization')

print(localization.formatList({'Ada', 'Grace', 'Hedy'}))
print(localization.formatList({'red', 'blue'}, {type = 'or', language = 'es'}))
```

### localization.assetPath(path)

Returns `i18n/<language>/<path>` when the package holds that asset for the current language, then for its base language, such as `i18n/pt/` for `pt-BR`, and the path itself otherwise, so voiced lines and pictures with text come in each language with a common fallback.

```lua
local assets = require('haylen.assets')
local localization = require('haylen.localization')

local intro = assets.load(localization.assetPath('voice/intro.ogg'))
```

## Errors

| Message | Cause |
| --- | --- |
| `A localization table needs a language.` | The function `localization.add()` received an empty language. |
| `The localization table of "<language>" must be a JSON object.` | The function `localization.add()` received a list instead of a table with string keys. |
| `The localization entry "<key>" must be text, a plural form or a group of entries.` | A language table holds a number, a boolean or a list. |
| `A number to format must be finite.` | The function `localization.formatNumber()` received NaN or an infinity. |
| `The style of a number must be "decimal", "percent", "currency" or "compact".` | The function `localization.formatNumber()` received another style. |
| `Fraction digits run from 0 to 20, with the minimum not above the maximum.` | The fraction digits of `localization.formatNumber()` are out of range. |
| `The date of a date format must be "short", "medium", "long", "full" or false.` | The function `localization.formatDate()` received another date style. |
| `A time zone offset runs from -1080 to 1080 minutes.` | The function `localization.formatDate()` received an offset beyond 18 hours. |
| `The type of a list must be "and" or "or".` | The function `localization.formatList()` received another type. |
| `Unknown option "<key>".` | An options table has a key the function does not accept. |
| `The localization file "<path>" is not valid JSON.` | The function `localization.loadFolder()` found a broken file. |
| `The "@direction" of the localization table of "<language>" must be "leftToRight" or "rightToLeft".` | A language table declares a direction other than `leftToRight` or `rightToLeft`. |
| `No localization table was added for "<language>".` | The function `localization.setLanguage()` or `localization.setFallback()` named a language that was never added. |
| `Localization arguments must be a JSON object.` | The function `localization.text()` received a list as arguments. |
