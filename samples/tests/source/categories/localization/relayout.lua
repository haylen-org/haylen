-- Buttons measure their words, labels wrap at the width of their card and rows share what is left, so a new language lays the card out again without any code. The panel on the right reads the bounds of the nodes every frame.
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')
local language = require('categories.localization.language')

local Relayout = haylen.class('Relayout', LanguageTest)

Relayout.hint = 'Pick another language, or turn on the cycle, and watch the buttons and the paragraph take their new sizes.'
Relayout.measured = {'save', 'cancel', 'options', 'intro', 'hard'}
Relayout.cycleSeconds = 1.5

function Relayout:content()
    return {
        ui.card{width = 900, align = 'start', gap = 20,
            ui.label{text = {key = 'menu.title'}, font = 'heading'},
            ui.label{id = 'intro', text = {key = 'layout.intro'}},
            ui.row{gap = 12,
                ui.button{id = 'save', text = {key = 'layout.save'}, variant = 'primary'},
                ui.button{id = 'cancel', text = {key = 'layout.cancel'}},
                ui.button{id = 'options', text = {key = 'layout.options'}},
            },
            ui.row{gap = 12,
                ui.label{text = {key = 'layout.difficulty'}},
                ui.badge{id = 'hard', text = {key = 'layout.hard'}, tone = 'danger'},
                ui.spacer{grow = 1},
                ui.label{text = {key = 'extras.credits'}, color = 'textMuted'},
            },
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Sizes from "document:bounds"'},
            ui.label{id = 'sizes', text = '', font = 'monospace'},
            ui.row{gap = 16,
                ui.label{text = 'Cycle the languages', grow = 1},
                ui.toggle{id = 'cycle', onChange = function(event)
                    self:cycle(event.checked)
                end},
            },
        },
    }
end

-- Shows the size every measured node was last drawn with, touching the label only when a size changed.
function Relayout:update(dt)
    Relayout.super.update(self, dt)
    local lines = {}
    for _, id in ipairs(Relayout.measured) do
        local area = self.document:bounds(id)
        if area then
            lines[#lines + 1] = string.format('Node %-10s %4.0f x %3.0f', '"' .. id .. '"', area.width, area.height)
        end
    end
    local text = table.concat(lines, '\n')
    if text ~= self.sizes then
        self.sizes = text
        self:set('sizes', {text = text})
    end
end

function Relayout:cycle(enabled)
    if enabled then
        self.timer = timer.every(Relayout.cycleSeconds, function()
            language.step(1)
        end, {owner = self})
    else
        timer.cancel(self.timer)
    end
end

return Relayout
