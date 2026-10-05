-- Every setting the tests of the category keep next to its default, with the changed ones marked, and the two ways back: one group with `preferences.remove` and the engine defaults, or every group of these tests, which leaves the preferences of the rest of the project alone.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local haylen = require('haylen')
local input = require('haylen.input')
local localization = require('haylen.localization')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')

local Test = require('harness.test')
local settings = require('categories.preferences.settings')

local Reset = haylen.class('Reset', Test)

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
    return value == nil and 'None' or tostring(value)
end

function Reset:init(entry)
    Reset.super.init(self, entry)
    self.defaultJump = jumpKey(assets.json(settings.actions))
end

-- The test starts the way an app does, with the stored settings, and leaves the engine as it found it.
function Reset:enter()
    self.snapshot = settings.snapshot()
    settings.start(self)
    self:frame{
        hint = 'Change some values to see them marked, then reset the audio group or every setting of these tests. The table reads the stored preferences every time.',
        focus = 'change',
        content = {ui.row{grow = 1, gap = 24,
            ui.panel{width = 520, align = 'stretch', gap = 12,
                ui.button{id = 'change', text = 'Change some values', align = 'stretch', onClick = function()
                    self:change()
                end},
                ui.button{id = 'audio', text = 'Reset the audio group', align = 'stretch', onClick = function()
                    self:resetAudio()
                end},
                ui.button{id = 'everything', text = 'Reset every setting', variant = 'destructive', align = 'stretch', onClick = function(event)
                    event.gui:set('confirm', {open = true})
                end},
                ui.label{id = 'result', text = '', color = 'accentText'},
            },
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.label{id = 'summary', text = '', color = 'textMuted'},
                ui.scroll{grow = 1, ui.table{id = 'values', columns = {{text = 'Setting'}, {text = 'Stored', width = 240}, {text = 'Default', width = 240}, {text = '', width = 150}}, rows = {}}},
            },
            ui.dialog{id = 'confirm', title = 'Reset every setting?', message = 'Every setting of these tests goes back to its default and is saved at once. The other preferences of the project stay.', buttons = {{id = 'cancel', text = 'Cancel'}, {id = 'reset', text = 'Reset', variant = 'destructive'}}, onAnswer = function(event)
                if event.button == 'reset' then
                    settings.reset(self)
                    self:refresh('The groups "' .. table.concat(settings.groups, '", "') .. '" went back to their defaults, which were applied and saved.')
                end
            end},
        }},
    }
    self:refresh('')
end

function Reset:exit()
    settings.restore(self.snapshot)
    Reset.super.exit(self)
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
        rows[index] = {id = entry.key, cells = {entry.key, format(entry.stored), format(entry.default), differs and 'Changed' or ''}}
    end
    self:set('values', {rows = rows})
    self:set('summary', {text = string.format('%d of %d settings differ from their defaults.', changed, #rows)})
    self:set('result', {text = result})
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
    self:refresh('Changed the volumes, the mute, the jump key, the difficulty, the language and the name, and saved them.')
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
    self:refresh("The call \"preferences.remove('audio')\" dropped the group, and capturing the default mix stored it again.")
end

return Reset
