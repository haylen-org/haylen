-- A settings screen: every control changes the engine or a game setting at once and stores it in the preferences, Save writes them, and leaving the screen saves what is still unsaved, so the choices come back the next time the screen opens, after a restart too. The labels are translations, so picking a language relabels the screen live.
local audio = require('haylen.audio')
local haylen = require('haylen')
local input = require('haylen.input')
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local settings = require('categories.preferences.settings')

local SettingsScreen = haylen.class('SettingsScreen', Test)

SettingsScreen.languages = {{id = 'en', text = 'English'}, {id = 'pt-BR', text = 'Português'}, {id = 'es', text = 'Español'}}
SettingsScreen.textFonts = {small = 'caption', medium = 'body', large = 'heading'}
SettingsScreen.windowed = {macos = true, windows = true, linux = true, web = true}
SettingsScreen.controlWidth = 520

local function text(key, args)
    return {key = 'settings.' .. key, args = args}
end

-- Returns a row with a label and one control.
local function row(key, control, caption)
    return ui.settingsRow{label = text(key), caption = caption and text(caption), control}
end

-- The screen starts the way an app does, with the stored settings, and leaves the engine as it found it.
function SettingsScreen:enter()
    self.snapshot = settings.snapshot()
    settings.start(self)
    self:frame{
        hint = 'Change anything, then Save, or just leave: unsaved changes are saved on the way out. Close the app and open this test again to see every value come back.',
        focus = 'master',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.scroll{grow = 1, ui.column{padding = {0, 24, 0, 0}, ui.settingsForm{children = self:form()}}},
                ui.row{gap = 16,
                    ui.label{id = 'saveState', text = '', color = 'textMuted', grow = 1},
                    ui.button{id = 'save', text = text('save'), variant = 'primary', onClick = function()
                        self:save()
                    end},
                },
            },
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Subtitle preview'},
                ui.label{id = 'subtitle', text = '', outline = '#FF000000', outlineWidth = 3},
            },
        }},
    }
    self:preview()
end

-- Leaving the screen saves the changes that were not saved yet.
function SettingsScreen:exit()
    if preferences.dirty() then
        self:save()
    end
    settings.restore(self.snapshot)
    SettingsScreen.super.exit(self)
end

function SettingsScreen:form()
    local width = SettingsScreen.controlWidth
    local form = {
        ui.sectionTitle{text = text('audio')},
        row('master', ui.slider{id = 'master', value = audio.busVolume('master'), showValue = true, width = width, onChange = function(event)
            self:engine(audio.setBusVolume, 'master', event.value)
        end}),
        row('music', ui.slider{id = 'music', value = audio.busVolume('music'), showValue = true, width = width, onChange = function(event)
            self:engine(audio.setBusVolume, 'music', event.value)
        end}),
        row('effects', ui.slider{id = 'effects', value = audio.busVolume('sfx'), showValue = true, width = width, onChange = function(event)
            self:engine(audio.setBusVolume, 'sfx', event.value)
        end}),
        row('mute', ui.toggle{id = 'mute', checked = audio.busMuted('master'), onChange = function(event)
            self:engine(audio.setBusMuted, 'master', event.checked)
        end}),
    }
    if SettingsScreen.windowed[haylen.platform] then
        form[#form + 1] = ui.sectionTitle{text = text('video')}
        form[#form + 1] = row('fullscreen', ui.toggle{id = 'fullscreen', checked = window.fullscreen(), onChange = function(event)
            self:engine(window.setFullscreen, event.checked)
        end})
    end
    local rest = {
        ui.sectionTitle{text = text('game')},
        row('difficulty', ui.segmentedControl{id = 'difficulty', width = width, selected = settings.get('game.difficulty'), items = {{id = 'easy', text = text('easy')}, {id = 'normal', text = text('normal')}, {id = 'hard', text = text('hard')}}, onChange = function(event)
            preferences.set('game.difficulty', event.value)
        end}),
        row('language', ui.combo{id = 'language', width = width, selected = settings.get('game.language'), items = SettingsScreen.languages, onChange = function(event)
            preferences.set('game.language', event.value)
            localization.setLanguage(event.value)
        end}, 'languageCaption'),
        row('subtitles', ui.toggle{id = 'subtitles', checked = settings.get('game.subtitles'), onChange = function(event)
            preferences.set('game.subtitles', event.checked)
            self:preview()
        end}),
        row('textSize', ui.stepper{id = 'textSize', width = width, selected = settings.get('game.textSize'), items = {{id = 'small', text = text('small')}, {id = 'medium', text = text('medium')}, {id = 'large', text = text('large')}}, onChange = function(event)
            preferences.set('game.textSize', event.value)
            self:preview()
        end}),
        row('name', ui.textField{id = 'name', width = width, value = settings.get('game.playerName'), maxLength = 24, autocorrect = false, autocapitalize = 'words', returnKey = 'done', onChange = function(event)
            preferences.set('game.playerName', event.value)
            self:preview()
        end}),
        ui.sectionTitle{text = text('controls')},
        row('jump', ui.keyCapture{id = 'jump', width = width, value = input.actionDefinition('jump').bindings[1], prompt = '…', onChange = function(event)
            local jump = input.actionDefinition('jump')
            jump.bindings[1] = event.value
            self:engine(input.defineAction, jump)
        end}),
        row('invertY', ui.toggle{id = 'invertY', checked = settings.get('controls.invertY'), onChange = function(event)
            preferences.set('controls.invertY', event.checked)
        end}),
    }
    table.move(rest, 1, #rest, #form + 1, form)
    return form
end

-- Changes the engine and stores its new state in the preferences, which marks them unsaved.
function SettingsScreen:engine(change, ...)
    change(...)
    preferences.capture()
end

function SettingsScreen:save()
    preferences.set('settings.savedAt', os.time())
    preferences.save()
end

function SettingsScreen:update(dt)
    SettingsScreen.super.update(self, dt)
    local state = preferences.dirty() and 'unsaved' or (preferences.has('settings.savedAt') and 'saved' or 'never')
    local savedAt = preferences.get('settings.savedAt')
    if state ~= self.saveState or savedAt ~= self.savedAt then
        self.saveState, self.savedAt = state, savedAt
        self:set('saveState', {text = text(state, {time = savedAt and os.date('%H:%M:%S', savedAt)}), color = state == 'unsaved' and 'warningText' or 'textMuted'})
    end
end

-- Shows a line of dialogue the way the subtitle settings would, with the name and the size the player picked.
function SettingsScreen:preview()
    if not settings.get('game.subtitles') then
        self:set('subtitle', {text = 'Subtitles are off.', font = 'body', color = 'textMuted'})
        return
    end
    local line = string.format('%s: The storm reaches the island tonight. Get to the lighthouse!', settings.get('game.playerName'))
    self:set('subtitle', {text = line, font = SettingsScreen.textFonts[settings.get('game.textSize')], color = 'text'})
end

return SettingsScreen
