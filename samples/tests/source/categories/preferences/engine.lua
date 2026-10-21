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

local Landscape = require('categories.sprites.landscape')
local Test = require('harness.test')
local settings = require('categories.preferences.settings')

local Engine = haylen.class('Engine', Test)

Engine.code = [[
audio.setBusVolume('music', 0.4)
audio.setBusMuted('sfx', false)
window.setFullscreen(true)
input.defineAction({name = 'jump', type = 'button', bindings = {'key:k'}})
preferences.capture()  -- Stores the volumes, the mutes, fullscreen and the actions.
preferences.save()
preferences.apply()  -- At startup, after the buses and the default action map.]]

Engine.clickInterval = 0.12
Engine.keys = {'key:j', 'key:k', 'key:up', 'key:enter'}

-- The meters of the buses on a card over the landscape, and the hop of the bunny that jump plays.
Engine.meter = {width = 440, row = 52, bar = 14}
Engine.hopSeconds = 0.5
Engine.hopHeight = 150
Engine.bunnyScale = 1.6
Engine.busNames = {master = 'Master', music = 'Music', sfx = 'Sound effects', ui = 'Interface', ambience = 'Ambience'}

function Engine:init(entry)
    Engine.super.init(self, entry)
    self.music = assets.load('preferences/audio/music-loop.wav')
    self.click = assets.load('preferences/audio/click.wav')
    self.clickTime = 0
    self.hop = nil
end

-- The test starts the way an app does, with the stored settings, and leaves the engine as it found it.
function Engine:enter()
    self.snapshot = settings.snapshot()
    settings.start(self)
    self.landscape = self.landscape or Landscape()
    self.bunny = self.bunny or assets.texture('sprites/images/bunny.png', {filter = 'linear'})
    local rows = {}
    for _, bus in ipairs(settings.buses) do
        rows[#rows + 1] = ui.row{gap = 16,
            ui.label{text = 'Bus "' .. bus .. '"', width = 200},
            ui.slider{id = 'volume-' .. bus, value = audio.busVolume(bus), showValue = true, grow = 1, onChange = function(event)
                audio.setBusVolume(bus, event.value)
                self:tick(bus)
            end},
            ui.toggle{id = 'muted-' .. bus, text = 'Mute', checked = audio.busMuted(bus), onChange = function(event)
                audio.setBusMuted(bus, event.checked)
            end},
        }
    end
    self:frame{
        hint = 'Move the sliders and toggles, rebind jump and press it on the play area. Capture and save stores the engine state, Mix it up changes it without saving and Apply brings the stored state back.',
        code = Engine.code,
        focus = 'volume-master',
        panelWidth = 820,
        controls = {
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
            ui.sectionTitle{text = 'Audio buses'},
            ui.column{gap = 12, children = rows},
            ui.sectionTitle{text = 'Window and controls'},
            ui.row{gap = 16,
                ui.label{text = 'Fullscreen', width = 200},
                ui.toggle{id = 'fullscreen', checked = window.fullscreen(), onChange = function(event)
                    window.setFullscreen(event.checked)
                end},
            },
            ui.row{gap = 16,
                ui.label{text = 'Jump', width = 200},
                ui.keyCapture{id = 'jump', value = self:binding(), prompt = 'Press a key or a button', width = 420, onChange = function(event)
                    self:rebind(event.value)
                end},
            },
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
    if input.pressed('jump') and not self.hop then
        self.hop = 0
    end
    if self.hop then
        self.hop = self.hop + dt
        if self.hop >= Engine.hopSeconds then
            self.hop = nil
        end
    end
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

-- The play area shows a landscape with a card of the volume of every bus, the muted ones grey, and a bunny that hops each time jump is pressed.
function Engine:draw(area)
    self.landscape:draw(area, area.height * 0.84, 0.75, haylen.elapsed())

    local meter = Engine.meter
    local height = 64 + #settings.buses * meter.row
    graphics2d.drawShape({24, 24, meter.width, height}, {radius = 16, color = '#D91B1E2B', borderWidth = 2, borderColor = Test.line, layer = 10})
    graphics2d.drawText(nil, 'The buses of the mixer', 48, 44, {size = 22, color = Test.ink, layer = 11})
    for index, bus in ipairs(settings.buses) do
        local y = 84 + (index - 1) * meter.row
        local muted = audio.busMuted(bus)
        graphics2d.drawText(nil, Engine.busNames[bus] or bus, 48, y, {size = 19, color = muted and Test.muted or Test.ink, layer = 11})
        if muted then
            graphics2d.drawText(nil, 'Muted', meter.width, y, {size = 17, color = Test.red, anchor = {1, 0}, layer = 11})
        end
        local width = meter.width - 48
        graphics2d.drawShape({48, y + 24, width, meter.bar}, {radius = meter.bar / 2, color = Test.line, layer = 11})
        local filled = width * audio.busVolume(bus)
        if filled > 0 then
            graphics2d.drawShape({48, y + 24, math.max(meter.bar, filled), meter.bar}, {radius = meter.bar / 2, color = muted and Test.muted or Test.accent, layer = 12})
        end
    end

    local ground = area.height * 0.84
    local x = area.width * 0.68
    local rise = 0
    if self.hop then
        local t = self.hop / Engine.hopSeconds
        rise = 4 * t * (1 - t) * Engine.hopHeight
    end
    local shadow = 1 - rise / Engine.hopHeight * 0.5
    graphics2d.drawEllipse(x, ground, 46 * shadow, 12 * shadow, '#55101418', {layer = 20})
    graphics2d.draw(self.bunny, x, ground - rise, {pivotX = 0.5, pivotY = 1, scaleX = Engine.bunnyScale, scaleY = Engine.bunnyScale, layer = 21})
    local binding = (self:binding() or ''):gsub('^key:', ''):gsub('^button:', 'the gamepad button ')
    local text = self.hop and 'Hop!' or 'Press "' .. binding .. '" here to jump'
    local labelWidth = graphics2d.measureText(nil, text, {size = 22}) + 36
    graphics2d.drawShape({x - labelWidth / 2, ground + 20, labelWidth, 40}, {radius = 20, color = '#D91B1E2B', layer = 22})
    graphics2d.drawText(nil, text, x, ground + 40, {size = 22, color = Test.ink, anchor = {0.5, 0.5}, layer = 23})
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
