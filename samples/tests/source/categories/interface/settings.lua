-- A settings screen from a settings form with section titles, rows with captions and actions that save or reset the values.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Settings = haylen.class('Settings', Test)

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

function Settings:enter()
    self:frame{
        hint = 'Change the settings and press Save to keep them, Cancel to go back to the saved ones or Defaults to start over. The arrows move between rows, and left and right change sliders and steppers.',
        focus = 'subtitles',
        content = {self:form()},
    }
end

function Settings:report(text)
    self:set('status', {text = text})
end

function Settings:change(key)
    return function(event)
        if event.checked ~= nil then
            self.values[key] = event.checked
        else
            self.values[key] = event.value
        end
        self:report('The setting "' .. key .. '" changed and is not saved yet.')
    end
end

-- Sets every control from a table of values, which moves them back to the saved or default state.
function Settings:show(values)
    local gui = self.gui
    gui:set('subtitles', {checked = values.subtitles})
    gui:set('music', {value = values.music})
    gui:set('effects', {value = values.effects})
    gui:set('language', {selected = values.language})
    gui:set('difficulty', {selected = values.difficulty})
    for key, value in pairs(values) do
        self.values[key] = value
    end
end

function Settings:form()
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
                        self:report('Back to the defaults, which are not saved yet.')
                    end},
                    ui.button{id = 'cancel', text = 'Cancel', onClick = function()
                        self:show(self.saved)
                        self:report('Back to the saved settings.')
                    end},
                    ui.button{id = 'save', text = 'Save', variant = 'primary', onClick = function(event)
                        for key, value in pairs(self.values) do
                            self.saved[key] = value
                        end
                        event.gui:set('saved', {open = true})
                        local values = self.values
                        self:report(string.format('Saved music %.2f, effects %.2f, language "%s" and difficulty "%s".', values.music, values.effects, values.language, values.difficulty))
                    end},
                },
            },
        },
        ui.toast{id = 'saved', text = 'Settings saved', tone = 'success', position = 'bottom'},
    }
end

return Settings
