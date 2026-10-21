-- A key the current language lacks comes from the fallback language, and a key that neither has comes back unchanged, so a missing translation shows its key. The Spanish and Japanese files leave out some extras on purpose, and every text shows where it came from.
local haylen = require('haylen')
local localization = require('haylen.localization')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Fallback = haylen.class('Fallback', LanguageTest)

Fallback.code = [[
localization.setFallback('en')  -- Keys the language lacks come from it.
localization.setLanguage('ja')
localization.text('menu.quit')
localization.has('menu.quit')
localization.fallback()]]

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
        rows[index] = ui.card{align = 'stretch', padding = {14, 20},
            ui.row{gap = 20,
                ui.column{grow = 1, gap = 4,
                    ui.label{text = key, font = 'monospace', color = 'textMuted'},
                    ui.label{text = {key = key}},
                },
                ui.badge{id = key .. '-source', text = '', tone = 'neutral'},
            },
        }
    end
    return {
        ui.panel{width = 520, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12,
                ui.sectionTitle{text = 'Fills the gaps'},
                ui.label{text = 'The language whose texts stand in for the keys the current language lacks.', color = 'textMuted'},
                ui.radioGroup{id = 'fallback', items = language.items(), selected = localization.fallback(), onChange = function(event)
                    localization.setFallback(event.value)
                    self:languageChanged()
                end},
            }},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.row{gap = 12, ui.sectionTitle{text = 'Where each text comes from', grow = 1}, ui.label{id = 'state', text = '', color = 'textMuted'}},
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = rows}},
        },
    }
end

-- Explains where each text comes from by looking at the language files themselves.
function Fallback:languageChanged()
    local current, fallback = localization.language(), localization.fallback()
    for _, key in ipairs(Fallback.keys) do
        local text, tone = 'Nowhere, so the key shows', 'danger'
        if defines(current, key) then
            text, tone = 'The current language, "' .. current .. '"', 'success'
        elseif defines(fallback, key) then
            text, tone = 'The fallback, "' .. fallback .. '"', 'warning'
        end
        self:set(key .. '-source', {text = text, tone = tone})
    end
    self:set('state', {text = string.format('Current "%s", fallback "%s"', current, fallback)})
end

return Fallback
