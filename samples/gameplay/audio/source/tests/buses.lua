-- Buses: music, effects, footsteps, interface clicks and ambience play through the bus tree at once, with the volume, mute and process mode of every bus, the game pause and the live statistics of each bus.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local sample = require('sample')
local sounds = require('sounds')

local Buses = haylen.class('Buses', sample.Test)
-- The test keeps updating while the game is paused, so its statistics stay live.
Buses.processMode = 'always'

local kBuses = {
    {name = 'master', mode = 'inherit', column = 3, row = 1},
    {name = 'music', parent = 'master', mode = 'always', column = 1, row = 2},
    {name = 'sfx', parent = 'master', mode = 'pausable', column = 2.4, row = 2},
    {name = 'ui', parent = 'master', mode = 'always', column = 3.8, row = 2},
    {name = 'ambience', parent = 'master', mode = 'pausable', column = 5.2, row = 2},
    {name = 'footsteps', parent = 'sfx', mode = 'inherit', column = 2.4, row = 3},
}
local kByName = {}
for _, bus in ipairs(kBuses) do
    kByName[bus.name] = bus
end
local kModes = {{id = 'inherit', text = 'Mode "inherit"'}, {id = 'pausable', text = 'Mode "pausable"'}, {id = 'whenPaused', text = 'Mode "whenPaused"'}, {id = 'always', text = 'Mode "always"'}, {id = 'disabled', text = 'Mode "disabled"'}}
local kPulses = {
    {bus = 'sfx', paths = {'audio/effects/chop.ogg'}, every = 1.3},
    {bus = 'footsteps', paths = {'audio/effects/footstep_1.ogg', 'audio/effects/footstep_2.ogg', 'audio/effects/footstep_3.ogg', 'audio/effects/footstep_4.ogg'}, every = 0.4},
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
    self.fire = audio.play(sounds.get('audio/ambient/fire_loop.ogg'), {bus = 'ambience', loop = true, fadeIn = 1})
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
        controls[#controls + 1] = ui.sectionTitle{text = 'Bus "' .. bus.name .. '"' .. (bus.parent and ' under "' .. bus.parent .. '"' or '')}
        controls[#controls + 1] = ui.row{gap = 12,
            ui.slider{id = bus.name .. 'Volume', grow = 1, min = 0, max = 1, step = 0.05, value = audio.busVolume(bus.name), showValue = true, onChange = function(event) audio.setBusVolume(bus.name, event.value, 0.1) end},
            ui.checkbox{id = bus.name .. 'Muted', text = 'Mute', onChange = function(event) audio.setBusMuted(bus.name, event.checked) end},
        }
        controls[#controls + 1] = ui.combo{id = bus.name .. 'Mode', items = kModes, selected = audio.busProcessMode(bus.name), onChange = function(event) audio.setBusProcessMode(bus.name, event.value) end}
    end
    self:frame({hint = 'Pause the game, change the modes and listen to which buses stop.', focus = 'paused', controls = controls})
end

function Buses:exit()
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
    self:status(string.format('Paused %s   voices %d   buses %s', haylen.paused(), audio.voiceCount(), table.concat(audio.buses(), ', ')))
end

function Buses:node(bus, area)
    local width, height = 230, 180
    local x = area.width * (bus.column - 0.5) / 5.6 - width / 2 + 20
    local y = 40 + (bus.row - 1) * (height + 60)
    return x, y, width, height
end

function Buses:draw(area)
    for _, bus in ipairs(kBuses) do
        if bus.parent then
            local x, y, width = self:node(bus, area)
            local px, py, pw, ph = self:node(kByName[bus.parent], area)
            graphics2d.drawLine(px + pw / 2, py + ph, x + width / 2, y, 3, sample.line)
        end
    end

    for _, bus in ipairs(kBuses) do
        local x, y, width, height = self:node(bus, area)
        local stats = self.stats[bus.name]
        local muted = audio.busMuted(bus.name)
        graphics2d.drawRect({x, y, width, height}, sample.surface, {layer = 1})
        graphics2d.drawRectOutline({x, y, width, height}, 3, stats.processing and sample.green or sample.red, {layer = 2})
        sample.caption('Bus "' .. bus.name .. '"', x + 14, y + 10, {size = 22, color = sample.ink, layer = 3})
        sample.bar(x + 14, y + 52, width - 28, 12, audio.busVolume(bus.name), muted and sample.muted or sample.accent)
        sample.caption(string.format('Mode "%s"%s', audio.busProcessMode(bus.name), muted and ', muted' or ''), x + 14, y + 74, {size = 16, layer = 3})
        sample.caption(stats.processing and 'Processing' or 'Held by the pause', x + 14, y + 102, {size = 16, color = stats.processing and sample.green or sample.red, layer = 3})
        sample.caption(string.format('%d voices, %d playing, %d paused', stats.voices, stats.playing, stats.paused), x + 14, y + 132, {size = 15, layer = 3})
    end
end

return Buses
