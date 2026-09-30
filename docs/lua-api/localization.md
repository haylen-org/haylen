# haylen.localization

Translated text by key, with placeholders, plural forms and a fallback language. Use it for every piece of text the player reads, so the app can ship in several languages and switch between them at runtime.

```lua
local localization = require('haylen.localization')
```

## Language tables

A language table maps keys to texts. Nested tables become dotted keys, so the table below defines `menu.play`, `menu.quit`, `hud.day` and `hud.wood`. A table with only `zero`, `one` and `other` texts, and always `other`, is a plural form instead of a group. Any other value raises `The localization entry "<key>" must be text, a plural form or a group of entries.`.

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

UI documents from [`haylen.ui`](ui.md) translate any text given as `{key = 'menu.play'}` or `{key = 'hud.day', args = {day = 3}}` through this module.

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

Returns `'rightToLeft'` when a language declares that it reads right to left with `@direction`, and `'leftToRight'` otherwise, for the current language when `language` is omitted. Text laid out in the language takes its direction from its own letters, and the direction of a language decides the layout of a UI around it.

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

For a plural form, the `count` argument picks the text: `zero` for 0 when the form has it, `one` for 1 when the form has it, and `other` for anything else or when `count` is missing. Arguments given as a list raise `Localization arguments must be a JSON object.`.

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

## Errors

| Message | Cause |
| --- | --- |
| `A localization table needs a language.` | The function `localization.add()` received an empty language. |
| `The localization table of "<language>" must be a JSON object.` | The function `localization.add()` received a list instead of a table with string keys. |
| `The localization entry "<key>" must be text, a plural form or a group of entries.` | A language table holds a number, a boolean or a list. |
| `The localization file "<path>" is not valid JSON.` | The function `localization.loadFolder()` found a broken file. |
| `The "@direction" of the localization table of "<language>" must be "leftToRight" or "rightToLeft".` | A language table declares a direction other than `leftToRight` or `rightToLeft`. |
| `No localization table was added for "<language>".` | The function `localization.setLanguage()` or `localization.setFallback()` named a language that was never added. |
| `Localization arguments must be a JSON object.` | The function `localization.text()` received a list as arguments. |
