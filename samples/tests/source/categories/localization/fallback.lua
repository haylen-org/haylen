-- A key the current language lacks comes from the fallback language, and a key that neither has comes back unchanged, so a missing translation shows its key. The Spanish and Japanese files leave out some extras on purpose.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Fallback = haylen.class('Fallback', LanguageTest)

Fallback.hint = 'Pick the current language above and the fallback language on the left. Spanish and Japanese lack the tip, only English has the pirate and only Portuguese has the secret.'
Fallback.focus = 'fallback'
Fallback.keys = {'extras.credits', 'extras.tip', 'extras.pirate', 'extras.secret', 'extras.nowhere'}

-- Tells whether the file of a language defines a dotted key.
local function defines(tag, key)
    local value = language.file(tag)
    for part in key:gmatch('[^.]+') do
        value = type(value) == 'table' and value[part] or nil
    end
    return value ~= nil
end

function Fallback:content()
    local rows = {}
    for index, key in ipairs(Fallback.keys) do
        rows[index] = {id = key, cells = {key, {key = key}, ''}}
    end
    return {
        ui.panel{width = 560, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Fallback language'},
            ui.radioGroup{id = 'fallback', items = language.items(), selected = localization.fallback(), onChange = function(event)
                localization.setFallback(event.value)
                self:languageChanged()
            end},
            ui.label{id = 'state', text = '', font = 'monospace'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.table{id = 'table', columns = {{text = 'Key', width = 280}, {text = 'Text'}, {text = 'Comes from', width = 380}}, rows = rows},
        },
    }
end

-- Explains where each text comes from by looking at the language files themselves.
function Fallback:languageChanged()
    local current, fallback = localization.language(), localization.fallback()
    local rows = {}
    for index, key in ipairs(Fallback.keys) do
        local source = 'Nowhere, so the key shows'
        if defines(current, key) then
            source = 'The current language, "' .. current .. '"'
        elseif defines(fallback, key) then
            source = 'The fallback language, "' .. fallback .. '"'
        end
        rows[index] = {id = key, cells = {key, {key = key}, source}}
    end
    self:set('table', {rows = rows})
    self:set('state', {text = string.format('The call "localization.language()" returns "%s".\nThe call "localization.fallback()" returns "%s".', current, fallback)})
end

return Fallback
