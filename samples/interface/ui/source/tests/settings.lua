-- Settings: a settings screen from a settings form with section titles, rows with captions and actions that save or reset the values.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Settings = haylen.class('Settings', sample.Test)

Settings.hints = 'Change the settings and press Save to keep them, Cancel to go back to the saved ones or Defaults to start over. The arrows move between rows, and left and right change sliders and steppers.'
Settings.focus = 'subtitles'

Settings.defaults = {subtitles = true, music = 0.7, effects = 0.9, language = 'en', difficulty = 'normal'}

function Settings:init(entry)
    Settings.super.init(self, entry)
    self.saved = {}
    self.values = {}
    for key, value in pairs(Settings.defaults) do
        self.saved[key] = value
        self.values[key] = value
    end
end

function Settings:change(key)
    return function(event)
        if event.checked ~= nil then
            self.values[key] = event.checked
        else
            self.values[key] = event.value
        end
        self:setStatus('Unsaved change to ' .. key)
    end
end

-- Sets every control from a table of values, which moves them back to the saved or default state.
function Settings:show(values)
    local document = self.document
    document:set('subtitles', {checked = values.subtitles})
    document:set('music', {value = values.music})
    document:set('effects', {value = values.effects})
    document:set('language', {selected = values.language})
    document:set('difficulty', {selected = values.difficulty})
    for key, value in pairs(values) do
        self.values[key] = value
    end
end

function Settings:content()
    local values = self.values
    return ui.row{
        justify = 'center',
        ui.panel{width = 1100, align = 'start',
            ui.settingsForm{
                ui.sectionTitle{text = 'Game'},
                ui.settingsRow{label = 'Subtitles', caption = 'Show what the characters say', ui.toggle{id = 'subtitles', checked = values.subtitles, onChange = self:change('subtitles')}},
                ui.settingsRow{label = 'Difficulty', caption = 'Enemies hit harder on hard', ui.stepper{id = 'difficulty', width = 420, items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = values.difficulty, onChange = self:change('difficulty')}},
                ui.sectionTitle{text = 'Audio and language'},
                ui.settingsRow{label = 'Music', ui.slider{id = 'music', value = values.music, width = 420, onChange = self:change('music')}},
                ui.settingsRow{label = 'Effects', ui.slider{id = 'effects', value = values.effects, width = 420, onChange = self:change('effects')}},
                ui.settingsRow{label = 'Language', caption = 'Changes every text at once', ui.combo{id = 'language', width = 420, items = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}}, selected = values.language, onChange = self:change('language')}},
                ui.settingsActions{
                    ui.button{id = 'defaults', text = 'Defaults', variant = 'link', onClick = function()
                        self:show(Settings.defaults)
                        self:setStatus('Back to the defaults, not saved yet')
                    end},
                    ui.button{id = 'cancel', text = 'Cancel', onClick = function()
                        self:show(self.saved)
                        self:setStatus('Back to the saved settings')
                    end},
                    ui.button{id = 'save', text = 'Save', variant = 'primary', onClick = function(event)
                        for key, value in pairs(self.values) do
                            self.saved[key] = value
                        end
                        event.document:set('saved', {open = true})
                        self:setStatus(string.format('Saved: music %.2f, effects %.2f, %s, %s', self.values.music, self.values.effects, self.values.language, self.values.difficulty))
                    end},
                },
            },
        },
        ui.toast{id = 'saved', text = 'Settings saved', tone = 'success', position = 'bottom'},
    }
end

return Settings
