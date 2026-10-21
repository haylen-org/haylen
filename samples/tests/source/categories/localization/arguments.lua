-- The `{name}` placeholders take the values of the arguments table, whole numbers print without decimals, other numbers as JSON numbers, booleans as `true` or `false`, `{{` and `}}` write braces, and a placeholder without an argument stays as it is. Without arguments a text shows its template.
local haylen = require('haylen')
local ui = require('haylen.ui')

local LanguageTest = require('categories.localization.language-test')

local Arguments = haylen.class('Arguments', LanguageTest)

Arguments.code = [[
localization.text('hud.greeting', {name = 'Ana', day = 3})
localization.text('hud.score', {score = 1250})
localization.text('hud.accuracy', {ratio = 0.75})
localization.text('hud.sound', {sound = true})
ui.label{id = 'hud.braces', text = {key = 'hud.braces'}}  -- Escaped braces stay braces.]]

Arguments.hint = 'Change the name, the day, the score, the accuracy and the sound, and watch every text follow. The grey line of each key is its template.'
Arguments.focus = 'day'
Arguments.keys = {'hud.greeting', 'hud.score', 'hud.accuracy', 'hud.sound', 'hud.braces', 'hud.missing'}

function Arguments:init(entry)
    Arguments.super.init(self, entry)
    self.values = {name = 'Ana', day = 3, score = 1250, ratio = 0.75, sound = true}
end

function Arguments:content()
    local cards = {}
    for _, key in ipairs(Arguments.keys) do
        cards[#cards + 1] = ui.card{align = 'stretch', gap = 6, padding = {14, 20},
            ui.label{text = 'Key "' .. key .. '"', font = 'caption', color = 'accentText'},
            ui.label{text = {key = key}, font = 'monospace', color = 'textMuted'},
            ui.label{id = key, text = '', font = 'heading'},
        }
    end
    local values = self.values
    return {
        ui.panel{width = 620, align = 'stretch',
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12,
                ui.formField{label = 'Argument "name"', ui.textField{id = 'name', value = values.name, maxLength = 20, autocapitalize = 'words', returnKey = 'done', onChange = function(event)
                    self:change('name', event.value)
                end}},
                ui.formField{label = 'Argument "day"', ui.stepper{id = 'day', value = values.day, min = 1, max = 30, onChange = function(event)
                    self:change('day', event.value)
                end}},
                ui.formField{label = 'Argument "score"', ui.numberField{id = 'score', value = values.score, min = 0, max = 99999, step = 250, onChange = function(event)
                    self:change('score', event.value)
                end}},
                ui.formField{label = 'Argument "ratio"', ui.slider{id = 'ratio', value = values.ratio, showValue = true, onChange = function(event)
                    self:change('ratio', math.floor(event.value * 100 + 0.5) / 100)
                end}},
                ui.formField{label = 'Argument "sound"', ui.toggle{id = 'sound', checked = values.sound, onChange = function(event)
                    self:change('sound', event.checked)
                end}},
            }},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.sectionTitle{text = 'Each key, its template and its text'},
            ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = cards}},
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
    for _, key in ipairs(Arguments.keys) do
        self:set(key, {text = {key = key, args = self.values}})
    end
end

return Arguments
