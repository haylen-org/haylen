-- Reset to defaults: every setting the sample keeps next to its default, with the changed ones marked, and the two ways back: one group with preferences.remove and the engine defaults, or everything with preferences.clear.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local haylen = require('haylen')
local input = require('haylen.input')
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local sample = require('sample')
local settings = require('settings')

local Reset = haylen.class('Reset', sample.Test)

Reset.hints = 'Change some values to see them marked, then reset the audio group or everything. The table reads the stored preferences every time.'
Reset.focus = 'change'

-- Reads the first binding of jump from an action map document, stored or default.
local function jumpKey(document)
    for _, action in ipairs(document and document.actions or {}) do
        if action.name == 'jump' then
            return action.bindings[1]
        end
    end
    return nil
end

local function format(value)
    if type(value) == 'number' then
        return string.format('%.2f', value)
    end
    return value == nil and 'none' or tostring(value)
end

function Reset:init(entry)
    Reset.super.init(self, entry)
    self.defaultJump = jumpKey(assets.json(settings.actions))
end

function Reset:content()
    return {
        ui.panel{width = 520, align = 'stretch', gap = 12,
            ui.button{id = 'change', text = 'Change some values', align = 'stretch', onClick = function()
                self:change()
            end},
            ui.button{id = 'audio', text = 'Reset the audio group', align = 'stretch', onClick = function()
                self:resetAudio()
            end},
            ui.button{id = 'everything', text = 'Reset everything', variant = 'destructive', align = 'stretch', onClick = function(event)
                event.document:set('confirm', {open = true})
            end},
            ui.label{id = 'result', text = '', color = 'accentText'},
        },
        ui.panel{grow = 1, align = 'stretch', gap = 12,
            ui.label{id = 'summary', text = '', color = 'textMuted'},
            ui.scroll{height = 0, grow = 1, ui.table{id = 'values', columns = {{text = 'Setting'}, {text = 'Stored', width = 240}, {text = 'Default', width = 240}, {text = '', width = 150}}, rows = {}}},
        },
        ui.dialog{id = 'confirm', title = 'Reset everything?', message = 'Every preference goes back to its default and is saved at once.', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'reset', text = 'Reset', variant = 'destructive'}}, onAnswer = function(event)
            if event.button == 'reset' then
                settings.reset()
                self:refresh('settings.reset() cleared the preferences, applied the defaults and saved them.')
            end
        end},
    }
end

function Reset:enter()
    Reset.super.enter(self)
    self:refresh('')
end

-- The settings the table shows, each with its stored value and its default.
function Reset:entries()
    local entries = {}
    for _, bus in ipairs({'master', 'music', 'sfx'}) do
        entries[#entries + 1] = {key = 'audio.volume.' .. bus, stored = preferences.get('audio.volume.' .. bus), default = settings.volumes[bus]}
    end
    entries[#entries + 1] = {key = 'audio.muted.master', stored = preferences.get('audio.muted.master'), default = false}
    entries[#entries + 1] = {key = 'window.fullscreen', stored = preferences.get('window.fullscreen'), default = false}
    entries[#entries + 1] = {key = 'input.actions jump', stored = jumpKey(preferences.get('input.actions')), default = self.defaultJump}
    for _, key in ipairs({'game.difficulty', 'game.language', 'game.subtitles', 'game.textSize', 'game.playerName', 'controls.invertY'}) do
        entries[#entries + 1] = {key = key, stored = preferences.get(key), default = settings.game[key]}
    end
    return entries
end

function Reset:refresh(result)
    local rows = {}
    local changed = 0
    for index, entry in ipairs(self:entries()) do
        local differs = format(entry.stored) ~= format(entry.default)
        changed = changed + (differs and 1 or 0)
        rows[index] = {id = entry.key, cells = {entry.key, format(entry.stored), format(entry.default), differs and 'changed' or ''}}
    end
    self:show('values', {rows = rows})
    self:show('summary', {text = string.format('%d of %d settings differ from their defaults.', changed, #rows)})
    self:show('result', {text = result})
end

-- Changes engine and game settings the way a player would, and saves them.
function Reset:change()
    audio.setBusVolume('master', 0.35)
    audio.setBusVolume('music', 0.1)
    audio.setBusMuted('master', true)
    local jump = input.actionDefinition('jump')
    jump.bindings[1] = 'key:j'
    input.defineAction(jump)
    preferences.capture()
    preferences.set('game.difficulty', 'hard')
    preferences.set('game.language', 'es')
    preferences.set('game.playerName', 'Zed')
    localization.setLanguage('es')
    preferences.save()
    self:refresh('Changed the volumes, the mute, the jump key, the difficulty, the language and the name, and saved.')
end

-- Removes the audio group and puts the default mix back into the engine before capturing it again.
function Reset:resetAudio()
    preferences.remove('audio')
    for bus, volume in pairs(settings.volumes) do
        audio.setBusVolume(bus, volume)
        audio.setBusMuted(bus, false)
    end
    preferences.capture()
    preferences.save()
    self:refresh("preferences.remove('audio') dropped the group, and capturing the default mix stored it again.")
end

return Reset
