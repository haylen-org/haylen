-- The languages of the sample and the fonts they draw with. The Latin languages use the default font in a family that falls back to the Japanese font, so rich text mixes scripts, and Japanese switches the whole interface to the Japanese font, since widgets draw with the regular face of a family only.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local language = {}

language.tags = {'en', 'pt-BR', 'es', 'ja'}

-- The UI theme of each language, whose font roles name the font that has its letters.
language.themes = {['en'] = 'latin', ['pt-BR'] = 'latin', ['es'] = 'latin', ['ja'] = 'japanese'}

local function theme(font)
    local role = {font = font}
    return {name = font, fonts = {body = role, caption = role, button = role, heading = role, title = role}}
end

-- Loads the translations, registers both fonts and their themes and starts in English.
function language.setup()
    localization.loadFolder('locale')
    localization.setFallback('en')
    language.japanese = assets.font('fonts/mplus_1p_regular.ttf')
    language.latin = graphics.newFontFamily({regular = graphics2d.defaultFont(), fallback = {language.japanese}})
    ui.addFont('latin', language.latin)
    ui.addFont('japanese', language.japanese)
    ui.addTheme(theme('latin'), 'dark')
    ui.addTheme(theme('japanese'), 'dark')
    language.use('en')
end

-- Makes a language current together with the theme of its font.
function language.use(tag)
    localization.setLanguage(tag)
    ui.setTheme(language.themes[tag])
end

-- Moves to the next language of the list, or the previous one with a step of -1.
function language.step(step)
    local current = 1
    for index, tag in ipairs(language.tags) do
        if tag == localization.language() then
            current = index
        end
    end
    language.use(language.tags[(current + step - 1) % #language.tags + 1])
end

-- The items of a language picker, each named in the current language.
function language.items()
    local items = {}
    for index, tag in ipairs(language.tags) do
        items[index] = {id = tag, text = {key = 'language.' .. tag}}
    end
    return items
end

return language
