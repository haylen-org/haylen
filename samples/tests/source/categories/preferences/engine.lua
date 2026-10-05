-- The function `preferences.capture` stores the volume and mute of every audio bus, fullscreen and the action map under fixed keys, and `preferences.apply` puts stored values back into the engine, so a settings screen changes the engine directly and saves once.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local json = require('json')
local preferences = require('haylen.preferences')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local settings = require('categories.preferences.settings')

local Engine = haylen.class('Engine', Test)

Engine.clickInterval = 0.12
Engine.keys = {'key:j', 'key:k', 'key:up', 'key:enter'}

function Engine:init(entry)
    Engine.super.init(self, entry)
    self.music = assets.load('preferences/audio/music_loop.wav')
    self.click = assets.load('preferences/audio/click.wav')
    self.clickTime = 0
end

-- The test starts the way an app does, with the stored settings, and leaves the engine as it found it.
function Engine:enter()
    self.snapshot = settings.snapshot()
    settings.start(self)
    local rows = {}
    for _, bus in ipairs(settings.buses) do
        rows[#rows + 1] = ui.row{gap = 16,
            ui.label{text = 'Bus "' .. bus .. '"', width = 220},
            ui.slider{id = 'volume-' .. bus, value = audio.busVolume(bus), showValue = true, grow = 1, onChange = function(event)
                audio.setBusVolume(bus, event.value)
                self:tick(bus)
            end},
            ui.toggle{id = 'muted-' .. bus, text = 'Muted', checked = audio.busMuted(bus), onChange = function(event)
                audio.setBusMuted(bus, event.checked)
            end},
        }
    end
    self:frame{
        hint = 'Move the sliders and toggles, rebind jump and press it on the play area. Capture and save stores the engine state, Mix it up changes it without saving and Apply brings the stored state back.',
        focus = 'volume-master',
        panelWidth = 1100,
        controls = {
            ui.sectionTitle{text = 'Audio buses'},
            ui.column{gap = 12, children = rows},
            ui.sectionTitle{text = 'Window and controls'},
            ui.row{gap = 16,
                ui.label{text = 'Fullscreen', width = 220},
                ui.toggle{id = 'fullscreen', checked = window.fullscreen(), onChange = function(event)
                    window.setFullscreen(event.checked)
                end},
            },
            ui.row{gap = 16,
                ui.label{text = 'Jump', width = 220},
                ui.keyCapture{id = 'jump', value = self:binding(), prompt = 'Press a key or a button', width = 420, onChange = function(event)
                    self:rebind(event.value)
                end},
            },
            ui.sectionTitle{text = 'Preferences'},
            ui.grid{columns = 2, gap = 12,
                ui.button{id = 'capture', text = 'Capture and save', variant = 'primary', align = 'stretch', onClick = function()
                    preferences.capture()
                    preferences.save()
                    self:report('The functions "preferences.capture()" and "preferences.save()" stored the engine state.')
                end},
                ui.button{id = 'apply', text = 'Apply', align = 'stretch', onClick = function()
                    local ok, failure = pcall(preferences.apply)
                    self:sync()
                    self:report(ok and 'The function "preferences.apply()" put the stored state back.' or 'The function "preferences.apply()" raised: ' .. failure)
                end},
                ui.button{id = 'mix', text = 'Mix it up', align = 'stretch', onClick = function()
                    self:mix()
                end},
                ui.button{id = 'reload', text = 'Load from disk', align = 'stretch', onClick = function()
                    preferences.load()
                    preferences.apply()
                    self:sync()
                    self:report('The functions "preferences.load()" and "preferences.apply()" read the file again.')
                end},
            },
            ui.label{id = 'result', text = 'Nothing captured in this visit yet.', color = 'accentText'},
            ui.sectionTitle{text = 'Stored engine keys'},
            ui.label{id = 'stored', text = '', font = 'monospace'},
        },
    }
    audio.playMusic(self.music, {fade = 0.5})
    self:refresh()
end

function Engine:exit()
    audio.stopMusic(0.3)
    settings.restore(self.snapshot)
    Engine.super.exit(self)
end

function Engine:update(dt)
    Engine.super.update(self, dt)
    self.clickTime = self.clickTime + dt
end

-- Plays a click through the bus a slider changed, a few times per second at most, so the new volume can be heard.
function Engine:tick(bus)
    if self.clickTime >= Engine.clickInterval and bus ~= 'music' then
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
    self:report('Jump is now "' .. binding .. '". Capture and save to keep it.')
end

-- Changes the engine without touching the preferences, so Apply has something to undo.
function Engine:mix()
    for _, bus in ipairs(settings.buses) do
        audio.setBusVolume(bus, math.random(0, 20) / 20)
    end
    local bus = settings.buses[math.random(#settings.buses)]
    audio.setBusMuted(bus, not audio.busMuted(bus))
    local jump = input.actionDefinition('jump')
    jump.bindings[1] = Engine.keys[math.random(#Engine.keys)]
    input.defineAction(jump)
    self:sync()
    self:report('Changed the volumes, the mute of "' .. bus .. '" and the jump key without saving.')
end

-- Moves every control to the state of the engine.
function Engine:sync()
    for _, bus in ipairs(settings.buses) do
        self:set('volume-' .. bus, {value = audio.busVolume(bus)})
        self:set('muted-' .. bus, {checked = audio.busMuted(bus)})
    end
    self:set('fullscreen', {checked = window.fullscreen()})
    self:set('jump', {value = self:binding()})
end

-- The play area shows the volume of every bus, which buses are muted and a lamp that lights while jump is down.
function Engine:draw(area)
    local width = area.width - 300
    for index, bus in ipairs(settings.buses) do
        local y = 40 + (index - 1) * 70
        local muted = audio.busMuted(bus)
        Test.caption(string.format('Bus "%s"%s', bus, muted and ', muted' or ''), 32, y, {size = 22, color = muted and Test.muted or Test.ink})
        graphics2d.drawRect({32, y + 32, width, 14}, Test.surface)
        graphics2d.drawRect({32, y + 32, width * audio.busVolume(bus), 14}, muted and Test.line or Test.accent)
    end
    local down = input.down('jump')
    local y = 60 + #settings.buses * 70
    graphics2d.drawCircle(72, y + 30, 36, down and Test.green or Test.surface)
    graphics2d.drawRing(72, y + 30, 36, 4, down and Test.ink or Test.line)
    Test.caption(down and 'Jumping' or 'Press jump here', 128, y + 14, {size = 26, color = Test.ink})
end

function Engine:report(text)
    self:set('result', {text = text})
    self:refresh()
end

function Engine:refresh()
    local stored = {audio = preferences.get('audio'), window = preferences.get('window'), input = preferences.get('input')}
    self:set('stored', {text = json.encode(stored, {pretty = true})})
end

return Engine
