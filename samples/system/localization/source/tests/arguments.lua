-- Arguments: {name} placeholders take the values of the arguments table, whole numbers print without decimals, other numbers as JSON numbers, booleans as true or false, {{ and }} write braces, and a placeholder without an argument stays as it is. Without arguments a text shows its template.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Arguments = haylen.class('Arguments', sample.Test)

Arguments.hints = 'Change the name, the day, the score, the accuracy and the sound, and watch every result follow. The grey line of each key is its template.'
Arguments.focus = 'day'

local kKeys = {'hud.greeting', 'hud.score', 'hud.accuracy', 'hud.sound', 'hud.braces', 'hud.missing'}

function Arguments:init(entry)
    Arguments.super.init(self, entry)
    self.values = {name = 'Ana', day = 3, score = 1250, ratio = 0.75, sound = true}
end

function Arguments:content()
    local lines = {}
    for _, key in ipairs(kKeys) do
        lines[#lines + 1] = ui.label{text = key, font = 'caption', color = 'accentText'}
        lines[#lines + 1] = ui.label{text = {key = key}, font = 'monospace', color = 'textMuted'}
        lines[#lines + 1] = ui.label{id = key, text = ''}
    end
    local values = self.values
    return {
        ui.panel{width = 620, align = 'stretch', gap = 12,
            ui.formField{label = 'name', ui.textField{id = 'name', value = values.name, maxLength = 20, autocapitalize = 'words', returnKey = 'done', onChange = function(event)
                self:change('name', event.value)
            end}},
            ui.formField{label = 'day', ui.stepper{id = 'day', value = values.day, min = 1, max = 30, onChange = function(event)
                self:change('day', event.value)
            end}},
            ui.formField{label = 'score', ui.numberField{id = 'score', value = values.score, min = 0, max = 99999, step = 250, onChange = function(event)
                self:change('score', event.value)
            end}},
            ui.formField{label = 'ratio', ui.slider{id = 'ratio', value = values.ratio, showValue = true, onChange = function(event)
                self:change('ratio', math.floor(event.value * 100 + 0.5) / 100)
            end}},
            ui.row{gap = 16,
                ui.label{text = 'sound', grow = 1},
                ui.toggle{id = 'sound', checked = values.sound, onChange = function(event)
                    self:change('sound', event.checked)
                end},
            },
        },
        ui.panel{grow = 1, align = 'stretch', gap = 4,
            ui.scroll{height = 0, grow = 1, ui.column{gap = 6, padding = {0, 24, 0, 0}, children = lines}},
        },
    }
end

function Arguments:enter()
    Arguments.super.enter(self)
    self:results()
end

function Arguments:change(name, value)
    self.values[name] = value
    self:results()
end

-- Every result is a translation with the current arguments, so it follows the language on its own and only needs setting when an argument changes.
function Arguments:results()
    for _, key in ipairs(kKeys) do
        self:show(key, {text = {key = key, args = self.values}})
    end
end

return Arguments
