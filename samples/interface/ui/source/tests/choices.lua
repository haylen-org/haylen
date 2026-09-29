-- Choices: check boxes, toggles and radio groups, with the values they report shown in the status line.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Choices = haylen.class('Choices', sample.Test)

Choices.hints = 'Click or tap a choice, or focus it and press Enter, Space or the south button. In a radio group the arrows move between the options.'
Choices.focus = 'hints'

function Choices:init(entry)
    Choices.super.init(self, entry)
    self.values = {hints = true, camera = false, music = true, vibration = false, difficulty = 'normal', hand = 'right'}
end

function Choices:change(key)
    return function(event)
        if event.checked ~= nil then
            self.values[key] = event.checked
        else
            self.values[key] = event.value
        end
        local values = self.values
        self:setStatus(string.format('hints %s, inverted camera %s, music %s, vibration %s, difficulty %s, hand %s', values.hints, values.camera, values.music, values.vibration, values.difficulty, values.hand))
    end
end

function Choices:content()
    local difficulties = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}, {id = 'nightmare', text = 'Nightmare', enabled = false}}
    return sample.columns{
        sample.section('checkbox', {grow = 1,
            ui.checkbox{id = 'hints', text = 'Show hints', checked = true, align = 'stretch', onChange = self:change('hints')},
            ui.checkbox{text = 'Invert the camera', align = 'stretch', onChange = self:change('camera')},
            ui.checkbox{text = 'Disabled', checked = true, enabled = false, align = 'stretch'},
        }),
        sample.section('toggle', {grow = 1,
            ui.toggle{text = 'Music', checked = true, align = 'stretch', onChange = self:change('music')},
            ui.toggle{text = 'Vibration', align = 'stretch', onChange = self:change('vibration')},
            ui.toggle{text = 'Disabled', enabled = false, align = 'stretch'},
        }),
        sample.section('radioGroup', {grow = 1,
            ui.radioGroup{items = difficulties, selected = 'normal', align = 'stretch', onChange = self:change('difficulty')},
            ui.divider{},
            ui.label{text = 'Horizontal', color = 'textMuted'},
            ui.radioGroup{items = {{id = 'left', text = 'Left hand'}, {id = 'right', text = 'Right hand'}}, selected = 'right', horizontal = true, onChange = self:change('hand')},
        }),
    }
end

return Choices
