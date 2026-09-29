-- Engine settings: preferences.capture stores the volume and mute of every audio bus, fullscreen and the action map under fixed keys, and preferences.apply puts stored values back into the engine, so a settings screen changes the engine directly and saves once.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local haylen = require('haylen')
local input = require('haylen.input')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')
local settings = require('settings')

local Engine = haylen.class('Engine', sample.Test)

Engine.hints = 'Move the sliders and toggles, rebind jump and press it. Capture and save stores the engine state, Mix it up changes it without saving and Apply brings the stored state back.'
Engine.focus = 'volume-master'

local kClickInterval = 0.12
local kKeys = {'key:j', 'key:k', 'key:up', 'key:enter'}

function Engine:init(entry)
    Engine.super.init(self, entry)
    self.music = assets.load('audio/music_loop.wav')
    self.click = assets.load('audio/click.wav')
    self.clickTime = 0
end

function Engine:content()
    local rows = {}
    for _, bus in ipairs(settings.buses) do
        rows[#rows + 1] = ui.row{gap = 16,
            ui.label{text = bus, width = 170},
            ui.slider{id = 'volume-' .. bus, value = audio.busVolume(bus), showValue = true, grow = 1, onChange = function(event)
                audio.setBusVolume(bus, event.value)
                self:tick(bus)
            end},
            ui.toggle{id = 'muted-' .. bus, text = 'Muted', checked = audio.busMuted(bus), onChange = function(event)
                audio.setBusMuted(bus, event.checked)
            end},
        }
    end
    return {
        ui.panel{grow = 1, align = 'stretch', gap = 16,
            ui.sectionTitle{text = 'Audio buses'},
            ui.column{gap = 12, children = rows},
            ui.sectionTitle{text = 'Window and controls'},
            ui.row{gap = 16,
                ui.label{text = 'Fullscreen', width = 170},
                ui.toggle{id = 'fullscreen', checked = window.fullscreen(), onChange = function(event)
                    window.setFullscreen(event.checked)
                end},
            },
            ui.row{gap = 16,
                ui.label{text = 'Jump', width = 170},
                ui.keyCapture{id = 'jump', value = self:binding(), prompt = 'Press a key or a button', width = 420, onChange = function(event)
                    self:rebind(event.value)
                end},
                ui.badge{id = 'jumping', text = 'Jumping', tone = 'success', solid = true, visible = false},
            },
        },
        ui.panel{width = 700, align = 'stretch', gap = 12,
            ui.grid{columns = 2, gap = 12,
                ui.button{id = 'capture', text = 'Capture and save', variant = 'primary', align = 'stretch', onClick = function()
                    preferences.capture()
                    preferences.save()
                    self:report('preferences.capture() and preferences.save() stored the engine state')
                end},
                ui.button{id = 'apply', text = 'Apply', align = 'stretch', onClick = function()
                    local ok, failure = pcall(preferences.apply)
                    self:sync()
                    self:report(ok and 'preferences.apply() put the stored state back' or 'preferences.apply() raised: ' .. failure)
                end},
                ui.button{id = 'mix', text = 'Mix it up', align = 'stretch', onClick = function()
                    self:mix()
                end},
                ui.button{id = 'reload', text = 'Load from disk', align = 'stretch', onClick = function()
                    preferences.load()
                    preferences.apply()
                    self:sync()
                    self:report('preferences.load() and preferences.apply() read the file again')
                end},
            },
            ui.label{id = 'result', text = 'Nothing captured in this visit yet.', color = 'accentText'},
            ui.sectionTitle{text = 'Stored engine keys'},
            ui.scroll{height = 0, grow = 1, ui.label{id = 'stored', text = '', font = 'monospace'}},
        },
    }
end

function Engine:enter()
    Engine.super.enter(self)
    audio.playMusic(self.music, {fade = 0.5})
    self:refresh()
end

function Engine:exit()
    audio.stopMusic(0.3)
    Engine.super.exit(self)
end

function Engine:update(dt)
    self.clickTime = self.clickTime + dt
    local jumping = input.down('jump')
    if jumping ~= self.jumping then
        self.jumping = jumping
        self:show('jumping', {visible = jumping})
    end
end

-- Plays a click through the bus a slider changed, a few times per second at most, so the new volume can be heard.
function Engine:tick(bus)
    if self.clickTime >= kClickInterval and bus ~= 'music' then
        self.clickTime = 0
        audio.play(self.click, {bus = bus == 'master' and 'sfx' or bus})
    end
end

function Engine:binding()
    return input.actionDefinition('jump').bindings[1]
end

-- Replaces the first binding of jump and keeps the others, such as the gamepad button.
function Engine:rebind(binding)
    local jump = input.actionDefinition('jump')
    jump.bindings[1] = binding
    input.defineAction(jump)
    self:report('Jump is now ' .. binding .. '. Capture and save to keep it.')
end

-- Changes the engine without touching the preferences, so Apply has something to undo.
function Engine:mix()
    for _, bus in ipairs(settings.buses) do
        audio.setBusVolume(bus, math.random(0, 20) / 20)
    end
    local bus = settings.buses[math.random(#settings.buses)]
    audio.setBusMuted(bus, not audio.busMuted(bus))
    local jump = input.actionDefinition('jump')
    jump.bindings[1] = kKeys[math.random(#kKeys)]
    input.defineAction(jump)
    self:sync()
    self:report('Changed the volumes, the mute of ' .. bus .. ' and the jump key without saving.')
end

-- Moves every control to the state of the engine.
function Engine:sync()
    for _, bus in ipairs(settings.buses) do
        self:show('volume-' .. bus, {value = audio.busVolume(bus)})
        self:show('muted-' .. bus, {checked = audio.busMuted(bus)})
    end
    self:show('fullscreen', {checked = window.fullscreen()})
    self:show('jump', {value = self:binding()})
end

function Engine:report(text)
    self:show('result', {text = text})
    self:refresh()
end

function Engine:refresh()
    local stored = {audio = preferences.get('audio'), window = preferences.get('window'), input = preferences.get('input')}
    self:show('stored', {text = sample.json(stored)})
end

return Engine
