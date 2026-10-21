# 📖 `haylen.localization`

The module `haylen.localization` translates the texts of an app, so it can ship in several languages and switch between them while it runs. It also writes numbers, dates and lists the way each language writes them. Use it for every text that the player reads.

```lua
local localization = require('haylen.localization')
```

## How translation works

The module keeps a catalog of texts. Each language adds a table that maps keys, such as `menu.play`, to texts. The app asks for a key with `localization.text`, and the catalog answers with the text of the current language.

The first language added becomes both the current language and the fallback language, until you choose others. A key that the current language lacks comes from the fallback language. A key that both lack comes back unchanged, so a missing translation shows its key on the screen.

GUIs of [`haylen.ui`](ui.md) translate any text given as `{key = 'menu.play'}` or `{key = 'hud.day', args = {day = 3}}` through this module, every time they draw it. So a change of language shows at once.

## Language tables

A language table maps keys to texts. Nested tables become dotted keys, so the table below defines `menu.play`, `menu.quit`, `hud.day` and `hud.wood`. A text may hold placeholders such as `{day}`, which [`localization.text`](#localizationtextkey-arguments) fills.

```json
{
  "menu": {"play": "Play", "quit": "Quit"},
  "hud": {
    "day": "Day {day}",
    "wood": {"zero": "No wood", "one": "{count} log", "other": "{count} logs"}
  }
}
```

A table is a plural form, such as `hud.wood` above, when its keys are only plural categories and its values are all texts. The categories are `zero`, `one`, `two`, `few`, `many` and `other`, and `other` must be there. Any other table is a group of entries. Any value that is not a text, a plural form or a group, such as a number, a boolean or a list, raises `The localization entry "<key>" must be text, a plural form or a group of entries.`.

A language that reads right to left declares it with `"@direction": "rightToLeft"` at the top of its table. The default is `"leftToRight"`. This key holds no text. The function [`localization.direction`](#localizationdirectionlanguage) returns it, and a UI that follows the language with [`ui.setDirection('auto')`](ui.md#uisetdirectiondirection) mirrors its layout when the language becomes current. Any other value raises `The "@direction" of the localization table of "<language>" must be "leftToRight" or "rightToLeft".`.

```json
{
  "@direction": "rightToLeft",
  "menu": {"play": "العب", "quit": "خروج"}
}
```

The usual layout keeps one JSON file per language in a folder of the content, named after its language tag, such as `i18n/en.json` and `i18n/pt-BR.json`. The function [`localization.loadFolder`](#localizationloadfolderfolder) adds them all.

## Languages

### localization.loadFolder(folder)

Adds every `.json` file under `folder` of the content, subfolders included, as the language that its file is named after. So `i18n/pt-BR.json` adds the language `pt-BR`. Returns the list of languages, in the order of the sorted file paths. Other files are ignored.

A file that is not valid JSON raises `The localization file "<path>" is not valid JSON.`. A table with a wrong entry raises the errors of [`localization.add`](#localizationaddlanguage-table).

In development, a saved change to a file of a loaded folder adds the file again, so `localization.text` returns its new texts at once. A text that the file no longer has stays until the app restarts. A file that fails while an editor still writes it waits for the next save, with a warning in the log. The [Lua guide](../core/lua.md#hot-reload) explains hot reload.

```lua
local localization = require('haylen.localization')

local languages = localization.loadFolder('i18n')
print('Loaded ' .. table.concat(languages, ', ') .. '.')
```

### localization.add(language, table)

Adds a language table for `language`. When the language already exists, the table merges into it: its keys replace the same keys and add the new ones. A later table without `@direction` keeps the direction that the language already declared. A table with a wrong entry changes nothing.

- An empty language raises `A localization table needs a language.`.
- A table that is not a table with string keys, such as a list, raises `The localization table of "<language>" must be a JSON object.`.

```lua
local localization = require('haylen.localization')

localization.add('en', {menu = {play = 'Play', quit = 'Quit'}})
localization.add('fr', {menu = {play = 'Jouer', quit = 'Quitter'}})
localization.add('en', {menu = {settings = 'Settings'}})
```

### localization.setLanguage(language)

Makes `language` the current language. A language that was never added raises `No localization table was added for "<language>".`.

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
print('The language is ' .. localization.language() .. '.')
```

### localization.setFallback(language)

Makes `language` the fallback language, which gives the keys that the current language lacks. A language that was never added raises `No localization table was added for "<language>".`.

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
print('The fallback is ' .. localization.fallback() .. '.')
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

### localization.findBestMatch(tag)

Returns the added language that best matches `tag`, a BCP 47 language tag such as `'pt-BR'`, or `nil` when none matches. The match ignores case and treats `_` like `-`.

- An exact match wins.
- Otherwise a language with the same base language matches, such as `pt` or `pt-PT` for `pt-BR`.
- Among those, the plain base language wins, so `pt-BR` finds `pt` before `pt-PT`. Without it, the first such language in sorted order wins.

The language of the device comes from the field `locale` of [`system.info`](system.md#systeminfo), a tag such as `'pt-BR'`. Its field `languages` lists every language that the player prefers, most preferred first.

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
    -- The first preferred language of the player that the app has wins.
    for _, tag in ipairs(system.info().languages) do
        local language = localization.findBestMatch(tag)
        if language then
            localization.setLanguage(language)
            break
        end
    end
end
```

### localization.direction(language)

Returns `'rightToLeft'` when `language` declares with `@direction` that it reads right to left, and `'leftToRight'` otherwise. Without `language`, it answers for the current language.

The letters of a text decide the direction of the text itself. The direction of a language decides the layout of a UI around the text. A language that was never added raises `No localization table was added for "<language>".`. A call without `language` before any language is added raises it too, with the empty current language as the name.

```lua
local localization = require('haylen.localization')

localization.add('en', {hello = 'Hello'})
localization.add('ar', {['@direction'] = 'rightToLeft', hello = 'مرحبا'})
print(localization.direction(), localization.direction('ar')) -- leftToRight rightToLeft
```

## Texts

### localization.text(key, arguments)

Returns the translated text of `key`, or `key` itself when neither the current nor the fallback language has it. The table `arguments` is optional, and arguments given as a list raise `Localization arguments must be a JSON object.`.

Placeholders work this way:

- A placeholder `{name}` takes the value of the argument `name`.
- A placeholder without a matching argument stays in the text as it is.
- The pairs `{{` and `}}` give single braces.
- Strings print as they are, and whole numbers print without decimals.
- Other numbers print as JSON numbers, such as `2.5`, and booleans print as `true` or `false`.
- A table prints as JSON text.

For a plural form, the argument `count` picks the text. The rules of plurals of the language that holds the form decide the category, as [`localization.pluralCategory`](#localizationpluralcategorycount-options) returns it. So Russian picks `one` for 21, `few` for 3 and `many` for 5. A `zero` text takes a count of 0, whatever the rules say. A category that the form lacks falls back to `other`, and so does a missing `count`. The argument `ordinal = true` picks the text by the ordinal rules instead, for ranks such as `1st`, `2nd` and `3rd`, and then a `zero` text gets no special case.

```lua
local graphics2d = require('haylen.graphics2d')
local localization = require('haylen.localization')
local scene = require('haylen.scene')

localization.add('en', {
    hud = {day = 'Day {day}', wood = {zero = 'No wood', one = '{count} log', other = '{count} logs'}},
    place = {one = '{count}st', two = '{count}nd', few = '{count}rd', other = '{count}th'},
    hint = 'Use {{ and }} for braces.',
})

print(localization.text('place', {count = 22, ordinal = true})) -- 22nd

scene.push({
    enter = function(self)
        self.day = 3
        self.wood = 1
    end,
    renderUi = function(self)
        graphics2d.beginScreen()
        graphics2d.drawText(nil, localization.text('hud.day', {day = self.day}), 20, 20)
        graphics2d.drawText(nil, localization.text('hud.wood', {count = self.wood}), 20, 60)
        graphics2d.drawText(nil, localization.text('hint'), 20, 100)
    end,
})
```

### localization.has(key)

Returns `true` when the current or the fallback language has a text or a plural form for `key`. A group, such as `menu`, is not a key.

```lua
local localization = require('haylen.localization')

localization.add('en', {tutorial = {jump = 'Press {button} to jump.'}})
if localization.has('tutorial.jump') then
    print(localization.text('tutorial.jump', {button = 'Space'}))
end
```

### localization.format(text, arguments)

Fills the placeholders of `text`, a text that is not a key of the catalog. Use it for a text written in the code or in a data file before it is translated. It fills the placeholders the same way as [`localization.text`](#localizationtextkey-arguments). The text never goes through the catalog, so it shows without the pseudo translation. Arguments given as a list raise `Localization arguments must be a JSON object.`.

```lua
local localization = require('haylen.localization')

print(localization.format('Wave {wave} of {waves}.', {wave = 2, waves = 5})) -- Wave 2 of 5.
```

### localization.setPseudo(enabled)

Turns every text of the catalog into a pseudo translation while `enabled` is `true`. Use it to test the layout of the app before you translate it.

- The letters take accents, such as `á` for `a`.
- The text grows by a third with dots and sits between guillemets, such as `« Pláý .. »`.
- Placeholders such as `{name}` and the tags of rich text stay as they are.

So a label that cuts its text, a button that grows past its space and a font that lacks an accented letter show at once. A text without guillemets never went through the catalog. Every GUI shows the change on its next frame.

```lua
local localization = require('haylen.localization')

localization.add('en', {menu = {play = 'Play'}})
localization.setPseudo(true)
print(localization.text('menu.play')) -- « Pláý .. »
```

### localization.pseudo()

Returns `true` while the catalog gives pseudo translations.

```lua
local localization = require('haylen.localization')

if localization.pseudo() then
    print('The texts are pseudo translations.')
end
```

### localization.assetPath(path)

Returns the path of the asset `path` for the current language. So voiced lines and pictures with text can come in each language, with a common fallback. It tries these paths in order and returns the first that the package holds:

1. The path `i18n/<language>/<path>` of the current language, such as `i18n/pt-BR/voice/intro.ogg`.
2. The same path for the base language, such as `i18n/pt/voice/intro.ogg` for `pt-BR`.
3. The path itself.

```lua
local assets = require('haylen.assets')
local localization = require('haylen.localization')

local intro = assets.load(localization.assetPath('voice/intro.ogg'))
```

## Plurals, numbers, dates and lists

These functions follow the current language, or the language of the option `language`. An unknown option raises `Unknown option "<key>".`.

Numbers, dates and lists know the marks and the words of these languages: `en`, `en-GB`, `es`, `pt`, `fr`, `de`, `it`, `nl`, `sv`, `da`, `nb`, `fi`, `pl`, `cs`, `ru`, `uk`, `tr`, `ja`, `zh`, `ko`, `ar`, `he`, `hi`, `id`, `vi`, `th`, `el`, `ro` and `hu`. The tags match without regard to case, with `-` or `_`. Another tag takes the data of its base language, so `pt-BR` uses `pt`. A language outside the list, or no language at all, uses English.

### localization.pluralCategory(count, options)

Returns the plural category of `count`: `'zero'`, `'one'`, `'two'`, `'few'`, `'many'` or `'other'`. The rules of plurals of the language pick it. The option `ordinal = true` picks it by the ordinal rules instead, for ranks such as `1st` and `2nd`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `ordinal` | boolean | `false` | Whether to use the ordinal rules. |
| `language` | string | the current language | The language whose rules to use. |

The rules of plurals cover `en`, `de`, `nl`, `sv`, `nb`, `no`, `nn`, `fi`, `et`, `it`, `ca`, `es`, `pt`, `fr`, `da`, `el`, `hu`, `tr`, `bg`, `hi`, `bn`, `ru`, `uk`, `be`, `pl`, `cs`, `sk`, `hr`, `sr`, `bs`, `sl`, `ro`, `lt`, `lv`, `ar`, `he`, `cy` and `ga`. Portuguese of Portugal, `pt-PT`, gives `one` to 1 alone, while other Portuguese gives it to 0 and 1. The ordinal rules cover `en`, `fr`, `it`, `sv`, `ca`, `hu`, `ro`, `vi` and `ms`. Every other language, such as Japanese, Chinese and Korean, uses `other` for every count.

```lua
local localization = require('haylen.localization')

print(localization.pluralCategory(3, {language = 'ru'})) -- few
print(localization.pluralCategory(22, {ordinal = true, language = 'en'})) -- two
```

### localization.formatNumber(value, options)

Writes the number `value` with the decimal mark, the grouping and the signs of the language. A negative number starts with `-`, unless it rounds to zero. Hindi groups by the Indian system, such as `1,23,45,678`.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `style` | string | `'decimal'` | The kind of number. The style `'decimal'` writes a plain number. The style `'percent'` writes a ratio as a percentage, so `0.25` is `25%`. The style `'currency'` writes an amount of the currency `currency`. The style `'compact'` writes a short form, such as `1.2K`, `2,5 Mio.` or `1.2万`. |
| `minimumFractionDigits` | integer | by style | The fewest digits after the decimal mark, from `0` to `20`. |
| `maximumFractionDigits` | integer | by style | The most digits after the decimal mark, from `0` to `20`. |
| `currency` | string | `'USD'` | The ISO code of the currency. The language places its symbol. |
| `grouping` | boolean | `true` | Whether to group the thousands. |
| `language` | string | the current language | The language whose marks to use. |

The fraction digits default by style:

- A decimal number shows from 0 to 3 digits.
- A percentage shows none.
- An amount shows the digits of its currency: none for `JPY` and `KRW`, and 2 for every other code.
- A compact number shows one digit below ten and none from ten up.
- When you give only one of the two options, the other moves so that the minimum never passes the maximum.

The language also places the percent sign and the currency symbol, such as `25 %` and `1.234,50 €` in German or `%25` in Turkish. The currency symbols that the engine knows are those of `USD`, `EUR`, `GBP`, `JPY`, `CNY`, `KRW`, `BRL`, `INR`, `RUB`, `CAD`, `AUD`, `MXN`, `CHF`, `SEK`, `NOK`, `DKK`, `PLN`, `CZK`, `TRY`, `ILS` and `UAH`. Any other code prints itself, such as `XYZ10.00`.

A compact number takes the largest scale of the language that it reaches, such as thousands or millions. A number below the first scale prints whole, such as `950`. A number that rounds up to the next scale moves to it, so `999950` prints `1M`.

A number that is not finite raises `A number to format must be finite.`. Another style raises `The style of a number must be "decimal", "percent", "currency" or "compact".`. Fraction digits out of range raise `Fraction digits run from 0 to 20, with the minimum not above the maximum.`.

```lua
local localization = require('haylen.localization')

print(localization.formatNumber(1234567.5, {language = 'de'})) -- 1.234.567,5
print(localization.formatNumber(0.25, {style = 'percent', language = 'en'})) -- 25%
print(localization.formatNumber(9.99, {style = 'currency', currency = 'EUR', language = 'fr'})) -- 9,99 €
print(localization.formatNumber(1500000, {style = 'compact', language = 'en'})) -- 1.5M
```

### localization.formatDate(seconds, options)

Writes a moment in Unix seconds, such as the value of `os.time()`, as a date, a time or both. A fraction of a second is dropped.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `date` | string or `false` | `'medium'` | The style of the date: `'short'`, `'medium'`, `'long'`, or `'full'` with the day of the week. The value `false` leaves the date out. |
| `time` | boolean | `false` | Whether to add the time after the date, in the clock of the language. |
| `timeZoneOffset` | integer | the time zone of the device | A fixed offset from UTC in minutes, from `-1080` to `1080`. |
| `language` | string | the current language | The language whose patterns and names to use. |

The languages `en`, `en-GB`, `es`, `pt`, `fr`, `de`, `it`, `nl`, `sv`, `da`, `nb`, `fi`, `pl`, `cs`, `ru`, `uk`, `tr`, `ja`, `zh` and `ko` write the names of months and days. The other languages write every style with numbers. The clock of a language has 12 or 24 hours, such as `2:05 PM` in English and `14:05` in British English. With `date = false` and no `time`, the result is an empty string.

Another date style raises `The date of a date format must be "short", "medium", "long", "full" or false.`. An offset beyond 18 hours raises `A time zone offset runs from -1080 to 1080 minutes.`.

```lua
local localization = require('haylen.localization')

print(localization.formatDate(1791900300, {date = 'long', time = true, timeZoneOffset = 0, language = 'en'})) -- October 13, 2026 2:05 PM
print(localization.formatDate(os.time(), {date = 'short', language = 'en-GB'}))
print(localization.formatDate(os.time(), {date = false, time = true}))
```

### localization.formatList(items, options)

Joins the list of strings `items` with the words and separators of the language. Two items join with the word, such as `red or blue`. Longer lists put separators between the items and the word before the last one. English writes a comma before the word, as in `apples, pears, and plums`, while British English writes `apples, pears and plums` and Japanese writes `apples、pears、plums`. One item comes back as it is, and an empty list gives an empty string.

| Option | Type | Default | Meaning |
| --- | --- | --- | --- |
| `type` | string | `'and'` | The word that joins the items: `'and'` or `'or'`. |
| `language` | string | the current language | The language whose words to use. |

Another type raises `The type of a list must be "and" or "or".`.

```lua
local localization = require('haylen.localization')

print(localization.formatList({'Ada', 'Grace', 'Hedy'}, {language = 'en'})) -- Ada, Grace, and Hedy
print(localization.formatList({'red', 'blue'}, {type = 'or', language = 'es'})) -- red o blue
```

## Errors

| Message | Cause |
| --- | --- |
| `A localization table needs a language.` | The function `localization.add` received an empty language. |
| `The localization table of "<language>" must be a JSON object.` | The function `localization.add` received a list or another value instead of a table with string keys. |
| `The localization entry "<key>" must be text, a plural form or a group of entries.` | A language table holds a number, a boolean or a list. |
| `The "@direction" of the localization table of "<language>" must be "leftToRight" or "rightToLeft".` | A language table declares another direction. |
| `The localization file "<path>" is not valid JSON.` | The function `localization.loadFolder` found a broken file. |
| `No localization table was added for "<language>".` | The function `localization.setLanguage`, `localization.setFallback` or `localization.direction` named a language that was never added. |
| `Localization arguments must be a JSON object.` | The function `localization.text` or `localization.format` received a list as arguments. |
| `A <type> cannot be converted to JSON.` | A language table or the arguments hold a function, a thread or a userdata. |
| `A <type> key cannot be converted to JSON.` | A language table or the arguments hold a table with a key that is not a string or a number. |
| `A number to format must be finite.` | The function `localization.formatNumber` received NaN or an infinity. |
| `The style of a number must be "decimal", "percent", "currency" or "compact".` | The function `localization.formatNumber` received another style. |
| `Fraction digits run from 0 to 20, with the minimum not above the maximum.` | The fraction digits of `localization.formatNumber` are out of range. |
| `The date of a date format must be "short", "medium", "long", "full" or false.` | The function `localization.formatDate` received another date style. |
| `A time zone offset runs from -1080 to 1080 minutes.` | The function `localization.formatDate` received an offset beyond 18 hours. |
| `The type of a list must be "and" or "or".` | The function `localization.formatList` received another type. |
| `Unknown option "<key>".` | An options table holds a key that the function does not take. |
