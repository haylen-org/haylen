-- Buses: music, effects, footsteps, interface clicks and ambience play through the bus tree at once, with the volume, mute and process mode of every bus, the game pause and the live statistics of each bus.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Buses = haylen.class('Buses', AudioTest)

Buses.code = [[
audio.createBus('footsteps', 'sfx')
audio.setBusVolume('music', 0.6, 0.1)
audio.setBusMuted('ambience', true)
audio.setBusProcessMode('ui', 'always')  -- Plays on while the game is paused.
audio.play(step, {bus = 'footsteps', volume = 0.7, pitchVariation = 0.05})
haylen.setPaused(true)  -- Holds the voices of the pausable buses in place.
for _, stats in ipairs(audio.busStats()) do print(stats.name, stats.processing, stats.voices) end]]

-- The test keeps updating while the game is paused, so its statistics stay live.
Buses.processMode = 'always'

-- The nodes of the tree sit in a world that the play area fits, by column from -1.5 to 1.5 and by row from the master down.
local kBuses = {
    {name = 'master', mode = 'inherit', column = 0, row = 1},
    {name = 'music', parent = 'master', mode = 'always', column = -1.5, row = 2},
    {name = 'sfx', parent = 'master', mode = 'pausable', column = -0.5, row = 2},
    {name = 'ui', parent = 'master', mode = 'always', column = 0.5, row = 2},
    {name = 'ambience', parent = 'master', mode = 'pausable', column = 1.5, row = 2},
    {name = 'footsteps', parent = 'sfx', mode = 'inherit', column = -0.5, row = 3},
}
local kNode = {width = 300, height = 214, columnStep = 340, rowStep = 284, top = -412}
local kByName = {}
for _, bus in ipairs(kBuses) do
    kByName[bus.name] = bus
end
local kModes = {{id = 'inherit', text = 'Mode "inherit"'}, {id = 'pausable', text = 'Mode "pausable"'}, {id = 'whenPaused', text = 'Mode "whenPaused"'}, {id = 'always', text = 'Mode "always"'}, {id = 'disabled', text = 'Mode "disabled"'}}
local kPulses = {
    {bus = 'sfx', paths = {'audio/effects/chop.ogg'}, every = 1.3},
    {bus = 'footsteps', paths = {'audio/effects/footstep-1.ogg', 'audio/effects/footstep-2.ogg', 'audio/effects/footstep-3.ogg', 'audio/effects/footstep-4.ogg'}, every = 0.4},
    {bus = 'ui', paths = {'audio/ui/click.ogg'}, every = 1.1},
}

function Buses:enter()
    local existing = {}
    for _, name in ipairs(audio.buses()) do
        existing[name] = true
    end
    if not existing.footsteps then
        audio.createBus('footsteps', 'sfx')
    end
    self.stats = {}
    self:readStats()

    audio.playMusic(sounds.track(sounds.tracks[2].path), {fade = 1, volume = 0.6})
    self.fire = audio.play(sounds.get('audio/ambient/fire-loop.ogg'), {bus = 'ambience', loop = true, fadeIn = 1})
    -- A pulse plays only while its bus processes, so the pause does not pile up voices that start held.
    for _, pulse in ipairs(kPulses) do
        local count = 0
        timer.every(pulse.every, function()
            if self.stats[pulse.bus].processing then
                count = count + 1
                audio.play(sounds.get(pulse.paths[(count - 1) % #pulse.paths + 1]), {bus = pulse.bus, volume = 0.7, pitchVariation = 0.05})
            end
        end, {owner = self})
    end

    local controls = {
        ui.toggle{id = 'paused', text = 'Pause the game', onChange = function(event) haylen.setPaused(event.checked) end},
        ui.label{text = 'The pause stops the buses whose mode does not run while paused. Their voices keep their place and go on when the pause ends.', color = 'textMuted', font = 'caption'},
    }
    for _, bus in ipairs(kBuses) do
        controls[#controls + 1] = ui.formField{label = 'Bus "' .. bus.name .. '"' .. (bus.parent and ' under "' .. bus.parent .. '"' or ''), ui.column{gap = 8,
            ui.row{gap = 12,
                ui.slider{id = bus.name .. 'Volume', grow = 1, min = 0, max = 1, step = 0.05, value = audio.busVolume(bus.name), showValue = true, onChange = function(event) audio.setBusVolume(bus.name, event.value, 0.1) end},
                ui.checkbox{id = bus.name .. 'Muted', text = 'Mute', onChange = function(event) audio.setBusMuted(bus.name, event.checked) end},
            },
            ui.combo{id = bus.name .. 'Mode', items = kModes, selected = audio.busProcessMode(bus.name), onChange = function(event) audio.setBusProcessMode(bus.name, event.value) end},
        }}
    end
    self:frame({hint = 'Pause the game, change the modes and listen to which buses stop. A green dot marks a bus that processes, and a red one a bus that the pause holds.', code = Buses.code, focus = 'paused', controls = controls, view = {1400, 860}})
end

function Buses:exit()
    Buses.super.exit(self)
    haylen.setPaused(false)
    for _, bus in ipairs(kBuses) do
        audio.setBusVolume(bus.name, 1)
        audio.setBusMuted(bus.name, false)
        audio.setBusProcessMode(bus.name, bus.mode)
    end
    audio.stop(self.fire, 0.3)
    audio.stopMusic(0.3)
end

function Buses:readStats()
    for _, stats in ipairs(audio.busStats()) do
        self.stats[stats.name] = stats
    end
end

function Buses:update(dt)
    Buses.super.update(self, dt)
    self:readStats()
    self:status(string.format('The game is %s, with %d voices on the buses "%s"', haylen.paused() and 'paused' or 'running', audio.voiceCount(), table.concat(audio.buses(), '", "')))
end

function Buses:node(bus)
    return bus.column * kNode.columnStep - kNode.width / 2, kNode.top + (bus.row - 1) * kNode.rowStep, kNode.width, kNode.height
end

function Buses:draw(area)
    for _, bus in ipairs(kBuses) do
        if bus.parent then
            local x, y, width = self:node(bus)
            local px, py, pw, ph = self:node(kByName[bus.parent])
            local fromX, fromY, toX, toY = px + pw / 2, py + ph, x + width / 2, y
            local middle = (fromY + toY) / 2
            graphics2d.drawStroke({fromX, fromY, fromX, middle, toX, middle, toX, toY}, {width = 4, color = '#FF3A4360', join = 'round', cap = 'round'})
        end
    end

    for _, bus in ipairs(kBuses) do
        local x, y, width, height = self:node(bus)
        local stats = self.stats[bus.name]
        local muted = audio.busMuted(bus.name)
        local state = stats.processing and Test.green or Test.red
        graphics2d.drawShape({x, y, width, height}, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = '#FF3A4360', layer = 1})
        graphics2d.drawCircle(x + 30, y + 36, 10, state, {layer = 2})
        Test.caption('Bus "' .. bus.name .. '"', x + 52, y + 16, {size = 32, color = Test.ink, layer = 3})
        AudioTest.bar(x + 22, y + 70, width - 44, 14, audio.busVolume(bus.name), muted and Test.muted or Test.accent)
        Test.caption(string.format('Mode "%s"%s', audio.busProcessMode(bus.name), muted and ', muted' or ''), x + 22, y + 98, {size = 24, layer = 3})
        Test.caption(stats.processing and 'Processing' or 'Held by the pause', x + 22, y + 132, {size = 24, color = state, layer = 3})
        Test.caption(string.format('%d voices, %d playing', stats.voices, stats.playing), x + 22, y + 166, {size = 24, layer = 3})
    end
end

return Buses
