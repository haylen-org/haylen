-- A settings screen from a settings form with section titles, rows with captions and actions that save or reset the values.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Settings = haylen.class('Settings', Test)

Settings.code = [[
ui.settingsForm{
    ui.sectionTitle{text = 'Audio'},
    ui.settingsRow{label = 'Music', caption = 'The volume of the songs', ui.slider{id = 'music', value = 70, max = 100, showValue = true, decimals = 0}},
    ui.settingsActions{ui.button{text = 'Cancel'}, ui.button{text = 'Save', variant = 'primary', onClick = save}},
}
gui:set('music', {value = 70})  -- Puts a control back to its saved value.]]

Settings.defaults = {subtitles = true, music = 70, effects = 90, language = 'en', difficulty = 'normal'}

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
        code = Settings.code,
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

-- The game settings and the audio settings stand side by side in one panel, with the actions at the end of the audio settings, so the whole screen shows at once.
function Settings:form()
    local values = self.values
    return ui.row{justify = 'center',
        ui.panel{width = 1600, align = 'start',
            ui.row{gap = 48, alignItems = 'start',
                ui.settingsForm{grow = 1,
                    ui.sectionTitle{text = 'Game'},
                    ui.settingsRow{label = 'Subtitles', caption = 'Show what the characters say', ui.toggle{id = 'subtitles', checked = values.subtitles, onChange = self:change('subtitles')}},
                    ui.settingsRow{label = 'Difficulty', caption = 'Enemies hit harder on hard', ui.stepper{id = 'difficulty', width = 360, items = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}, selected = values.difficulty, onChange = self:change('difficulty')}},
                    ui.settingsRow{label = 'Language', caption = 'Changes every text at once', ui.combo{id = 'language', width = 360, items = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}}, selected = values.language, onChange = self:change('language')}},
                },
                ui.settingsForm{grow = 1,
                    ui.sectionTitle{text = 'Audio'},
                    ui.settingsRow{label = 'Music', caption = 'The volume of the songs', ui.slider{id = 'music', value = values.music, max = 100, showValue = true, decimals = 0, width = 360, onChange = self:change('music')}},
                    ui.settingsRow{label = 'Effects', caption = 'The volume of steps, waves and hits', ui.slider{id = 'effects', value = values.effects, max = 100, showValue = true, decimals = 0, width = 360, onChange = self:change('effects')}},
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
                            local saved = self.values
                            self:report(string.format('Saved the music at %.0f and the effects at %.0f percent, the language "%s" and the difficulty "%s".', saved.music, saved.effects, saved.language, saved.difficulty))
                        end},
                    },
                },
            },
        },
        ui.toast{id = 'saved', text = 'Settings saved', tone = 'success', position = 'bottom'},
    }
end

return Settings
