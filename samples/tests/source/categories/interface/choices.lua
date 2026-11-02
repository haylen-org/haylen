-- Check boxes, toggles and radio groups, with the values they report shown in the status line.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Choices = haylen.class('Choices', Test)

Choices.code = [[
ui.checkbox{text = 'Show hints', checked = true, onChange = function(event) hints = event.checked end}
ui.toggle{text = 'Music', checked = true, onChange = function(event) music = event.checked end}
ui.radioGroup{items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = 'normal',
    onChange = function(event) difficulty = event.value end}
ui.radioGroup{items = {{id = 'left', text = 'Left hand'}, {id = 'right', text = 'Right hand'}}, selected = 'right', horizontal = true}]]

function Choices:init(entry)
    Choices.super.init(self, entry)
    self.values = {hints = true, camera = false, music = true, vibration = false, difficulty = 'normal', hand = 'right'}
end

function Choices:enter()
    local difficulties = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}, {id = 'nightmare', text = 'Nightmare', enabled = false}}
    self:frame{
        hint = 'Click or tap a choice, or focus it and press Enter, Space or the south button. In a radio group the arrows move between the options.',
        code = Choices.code,
        focus = 'hints',
        content = {layout.columns{
            layout.section('Component "checkbox"', {grow = 1, align = 'stretch',
                ui.column{gap = 0, align = 'stretch',
                    ui.checkbox{id = 'hints', text = 'Show hints', checked = true, onChange = self:change('hints')},
                    ui.checkbox{text = 'Invert the camera', onChange = self:change('camera')},
                    ui.checkbox{text = 'Remember the island I visited last between sessions', onChange = self:change('remember')},
                    ui.checkbox{text = 'Disabled', checked = true, enabled = false},
                },
                ui.spacer{grow = 1},
                Choices.note('A press flips "checked" and reports "change".'),
            }),
            layout.section('Component "toggle"', {grow = 1, align = 'stretch',
                ui.column{gap = 0, align = 'stretch',
                    ui.toggle{text = 'Music', checked = true, onChange = self:change('music')},
                    ui.toggle{text = 'Vibration', onChange = self:change('vibration')},
                    ui.toggle{text = 'Subtitles in a larger size for the dialogue', onChange = self:change('subtitles')},
                    ui.toggle{text = 'Disabled', enabled = false},
                },
                ui.spacer{grow = 1},
                Choices.note('The knob slides across, and "change" reports "checked".'),
            }),
            layout.section('Component "radioGroup"', {grow = 1, align = 'stretch',
                ui.radioGroup{items = difficulties, selected = 'normal', onChange = self:change('difficulty')},
                ui.divider{},
                ui.label{text = 'Horizontal', color = 'textMuted'},
                ui.radioGroup{items = {{id = 'left', text = 'Left hand'}, {id = 'right', text = 'Right hand'}}, selected = 'right', horizontal = true, onChange = self:change('hand')},
                ui.spacer{grow = 1},
                Choices.note('Picking an option reports "change" with its id as "value".'),
            }),
        }},
    }
end

-- A muted line at the foot of a card that says what its component reports.
function Choices.note(text)
    return ui.label{text = text, font = 'caption', color = 'textMuted', align = 'stretch'}
end

function Choices:change(key)
    return function(event)
        if event.checked ~= nil then
            self.values[key] = event.checked
        else
            self.values[key] = event.value
        end
        local values = self.values
        local switch = function(on) return on and 'on' or 'off' end
        self:set('status', {text = string.format('Hints are %s and music is %s, the difficulty is %s and the %s hand leads.', switch(values.hints), switch(values.music), values.difficulty, values.hand)})
    end
end

return Choices
