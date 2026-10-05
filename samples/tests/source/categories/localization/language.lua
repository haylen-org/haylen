-- The languages of the tests and the font they draw with: the default font in a family that falls back to fonts of Japanese, Arabic and Devanagari, so every text component draws every language alike. Arabic declares that it reads right to left, and the UI follows the language while a test shows, so the whole interface mirrors in Arabic.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local language = {}

language.tags = {'en', 'pt-BR', 'es', 'ja', 'ar', 'hi'}
language.folder = 'localization/locale'

-- Loads the translations, gives every theme font role the family, lets the UI take the direction of the language and starts in English. Returns what the app used before, which `restore` puts back.
function language.setup()
    local previous = {language = localization.language(), fallback = localization.fallback(), theme = ui.theme(), direction = ui.direction()}
    localization.loadFolder(language.folder)
    localization.setFallback('en')
    if not language.family then
        local japanese = assets.font('fonts/mplus_1p_regular.ttf')
        local arabic = assets.font('fonts/noto_sans_arabic_regular.ttf')
        local devanagari = assets.font('fonts/noto_sans_devanagari_regular.ttf')
        language.family = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallbacks = {japanese, arabic, devanagari}})
        ui.addFont('text', language.family)
    end
    local role = {font = 'text'}
    ui.setTheme(ui.addTheme({name = 'localized', fonts = {body = role, caption = role, button = role, heading = role, title = role, monospace = role}}, 'dark'))
    ui.setDirection('auto')
    localization.setLanguage('en')
    return previous
end

function language.restore(previous)
    ui.setDirection(previous.direction)
    ui.setTheme(previous.theme)
    localization.setLanguage(previous.language ~= '' and previous.language or 'en')
    localization.setFallback(previous.fallback ~= '' and previous.fallback or 'en')
end

-- Moves to the next language of the list, or the previous one with a step of -1.
function language.step(step)
    local current = 1
    for index, tag in ipairs(language.tags) do
        if tag == localization.language() then
            current = index
        end
    end
    localization.setLanguage(language.tags[(current + step - 1) % #language.tags + 1])
end

-- The items of a language picker, each named in the current language.
function language.items()
    local items = {}
    for index, tag in ipairs(language.tags) do
        items[index] = {id = tag, text = {key = 'language.' .. tag}}
    end
    return items
end

-- Reads the language file of a tag, to explain where a text comes from.
function language.file(tag)
    return assets.json(language.folder .. '/' .. tag .. '.json')
end

return language
