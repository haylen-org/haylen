-- Switching languages: every text of the title screen is a translation such as `{key = 'menu.play'}`, which the UI resolves every time it draws, so `localization.setLanguage` changes the whole screen at once without rebuilding it.
local haylen = require('haylen')
local localization = require('haylen.localization')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local language = require('language')
local sample = require('sample')

local Switch = haylen.class('Switch', sample.Test)

Switch.hints = 'Pick a language above, or turn on the cycle to watch every text change on its own.'

local kCycleSeconds = 2

function Switch:content()
    local buttons = {}
    for index, key in ipairs({'play', 'continue', 'settings', 'quit'}) do
        buttons[index] = ui.button{id = key, text = {key = 'menu.' .. key}, variant = index == 1 and 'primary' or 'default', align = 'stretch'}
    end
    return {
        ui.card{grow = 1, align = 'stretch', gap = 24, justify = 'center',
            ui.label{text = {key = 'menu.title'}, font = 'title', textAlign = 'center', align = 'stretch'},
            ui.label{text = {key = 'menu.subtitle'}, color = 'textMuted', textAlign = 'center', align = 'stretch'},
            ui.column{width = 520, gap = 12, align = 'center', children = buttons},
            ui.label{id = 'current', text = '', color = 'accentText', textAlign = 'center', align = 'stretch'},
        },
        ui.panel{width = 600, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'The languages'},
            ui.label{text = 'The function "localization.languages()" returns "' .. table.concat(localization.languages(), '", "') .. '", the files of "content/locale".'},
            ui.label{id = 'state', text = '', font = 'monospace'},
            ui.row{gap = 16,
                ui.label{text = 'Cycle every two seconds', grow = 1},
                ui.toggle{id = 'cycle', onChange = function(event)
                    self:cycle(event.checked)
                end},
            },
        },
    }
end

-- The name of the language is an argument filled with another translation, so it is set again whenever the language changes.
function Switch:languageChanged()
    local tag = localization.language()
    self:show('current', {text = {key = 'menu.current', args = {name = localization.text('language.' .. tag)}}})
    self:show('state', {text = string.format('The call "localization.language()" returns "%s"\nThe call "localization.fallback()" returns "%s"', tag, localization.fallback())})
end

-- The timer belongs to the scene, so it also stops when the player leaves.
function Switch:cycle(enabled)
    if enabled then
        self.timer = timer.every(kCycleSeconds, function()
            language.step(1)
        end, {owner = self})
    else
        timer.cancel(self.timer)
    end
end

return Switch
