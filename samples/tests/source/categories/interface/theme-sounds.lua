-- A theme that gives the interactions of the UI their sounds: presses, focus moves, choices that change, slider steps and typing, with styles that silence or replace the sound of one node and a switch that mutes them all.
local audio = require('haylen.audio')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local ThemeSounds = haylen.class('ThemeSounds', Test)

ThemeSounds.code = [[
ui.setTheme(ui.addTheme({name = 'sounding', sounds = {
    press = {sound = 'audio/ui/click.ogg', volume = 0.8}, focus = {sound = 'audio/ui/focus.ogg', interval = 0.08},
    toggle = {sound = 'audio/ui/toggle.ogg'}, step = {sound = 'audio/ui/step.ogg', volume = 0.6}, type = {sound = 'audio/ui/key.ogg', volume = 0.7},
}}, 'dark'))
ui.button{text = 'Silent', style = {sounds = {press = false}}}
ui.button{text = 'Confirm', style = {sounds = {press = {sound = 'audio/ui/confirm.ogg'}}}}
audio.setBusMuted('ui', true)  -- Every theme sound plays through the bus "ui" unless it names another.]]

ThemeSounds.sounds = {
    press = {sound = 'audio/ui/click.ogg', volume = 0.8},
    focus = {sound = 'audio/ui/focus.ogg', interval = 0.08},
    toggle = {sound = 'audio/ui/toggle.ogg'},
    step = {sound = 'audio/ui/step.ogg', volume = 0.6},
    type = {sound = 'audio/ui/key.ogg', volume = 0.7},
}

function ThemeSounds:enter()
    self.started = {theme = ui.theme(), muted = audio.busMuted('ui')}
    ui.setTheme(ui.addTheme({name = 'sounding', sounds = ThemeSounds.sounds}, self.started.theme))
    audio.setBusMuted('ui', false)
    self:frame{
        hint = 'Press the buttons, flip the choices, drag the slider and type in the field. Move the focus with the arrows, Tab or a gamepad to hear its tick, and hold a direction to hear the ticks stay apart. The switch mutes the bus "ui".',
        code = ThemeSounds.code,
        focus = 'play',
        content = {self:nodes()},
    }
    self:describe('Every control of the test plays the sound of its theme.')
end

function ThemeSounds:exit()
    ui.setTheme(self.started.theme)
    audio.setBusMuted('ui', self.started.muted)
    ThemeSounds.super.exit(self)
end

function ThemeSounds:describe(text)
    local state = audio.busMuted('ui') and 'muted' or 'playing'
    self:set('status', {text = string.format('%s The bus "ui" is %s.', text, state)})
end

-- A handler that tells which control the player used.
function ThemeSounds:heardFrom(name)
    return function()
        self:describe(string.format('The control "%s" played its sound.', name))
    end
end

function ThemeSounds:nodes()
    return layout.columns{
        layout.section('Presses', {grow = 1, align = 'stretch',
            ui.button{id = 'play', text = 'Play', variant = 'primary', onClick = self:heardFrom('Play')},
            ui.button{text = 'Options', onClick = self:heardFrom('Options')},
            ui.button{text = 'Silent', style = {sounds = {press = false}}, onClick = function() self:describe('The button "Silent" played nothing, since its style silences its press.') end},
            ui.button{text = 'Confirm', style = {sounds = {press = {sound = 'audio/ui/confirm.ogg'}}}, onClick = self:heardFrom('Confirm')},
            ui.tabs{items = {{id = 'bag', text = 'Bag'}, {id = 'map', text = 'Map'}}, selected = 'bag', onSelect = self:heardFrom('Tabs')},
        }),
        layout.section('Choices', {grow = 1, align = 'stretch',
            ui.toggle{id = 'sounds', text = 'Sounds', checked = true, onChange = function(event)
                audio.setBusMuted('ui', not event.checked)
                self:describe(event.checked and 'The sounds play again.' or 'The sounds are muted.')
            end},
            ui.checkbox{text = 'Show hints', checked = true, onChange = self:heardFrom('Show hints')},
            ui.radioGroup{items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = 'normal', onChange = self:heardFrom('Difficulty')},
            ui.segmentedControl{items = {{id = 'day', text = 'Day'}, {id = 'night', text = 'Night'}}, selected = 'day', onChange = self:heardFrom('Time of day')},
        }),
        layout.section('Steps and typing', {grow = 1, align = 'stretch',
            ui.slider{value = 5, min = 0, max = 10, step = 1, showValue = true, onChange = self:heardFrom('Volume')},
            ui.stepper{items = {{id = 'low', text = 'Low'}, {id = 'medium', text = 'Medium'}, {id = 'high', text = 'High'}}, selected = 'medium', onChange = self:heardFrom('Quality')},
            ui.textField{placeholder = 'Type a name', onChange = self:heardFrom('Name')},
        }),
    }
end

return ThemeSounds
