-- Effects: one effect of any kind, a filter, the delay or the reverb, on the music bus or on a looping voice, with a slider per parameter, a tween that sweeps its main parameter and a drawing of what it does: the response of a filter, the repeats of the delay or the decay of the reverb.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local response = require('categories.audio.response')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Effects = haylen.class('Effects', AudioTest)

local kKinds = {
    {id = 'lowpass', text = 'Lowpass filter', params = {'cutoff', 'q'}, sweep = {'cutoff', 300, 8000}},
    {id = 'highpass', text = 'Highpass filter', params = {'cutoff', 'q'}, sweep = {'cutoff', 100, 3000}},
    {id = 'bandpass', text = 'Bandpass filter', params = {'cutoff', 'q'}, sweep = {'cutoff', 300, 5000}},
    {id = 'notch', text = 'Notch filter', params = {'cutoff', 'q'}, sweep = {'cutoff', 300, 5000}},
    {id = 'peak', text = 'Peak equalizer', params = {'cutoff', 'q', 'gain'}, sweep = {'cutoff', 200, 6000}},
    {id = 'lowShelf', text = 'Low shelf', params = {'cutoff', 'q', 'gain'}, sweep = {'gain', -18, 18}},
    {id = 'highShelf', text = 'High shelf', params = {'cutoff', 'q', 'gain'}, sweep = {'gain', -18, 18}},
    {id = 'delay', text = 'Delay and echo', params = {'time', 'feedback', 'wet', 'dry'}, sweep = {'time', 0.12, 0.6}},
    {id = 'reverb', text = 'Reverb', params = {'roomSize', 'damping', 'width', 'wet', 'dry'}, sweep = {'roomSize', 0.2, 0.95}},
}
local kParams = {
    cutoff = {label = 'Cutoff', min = 0, max = 1, step = 0.01},
    q = {label = 'Q', min = 0.2, max = 10, step = 0.1},
    gain = {label = 'Gain in decibels', min = -24, max = 24, step = 1},
    time = {label = 'Time in seconds', min = 0.02, max = 1, step = 0.01},
    feedback = {label = 'Feedback', min = 0, max = 0.95, step = 0.05},
    wet = {label = 'Wet', min = 0, max = 1, step = 0.05},
    dry = {label = 'Dry', min = 0, max = 1, step = 0.05},
    roomSize = {label = 'Room size', min = 0, max = 1, step = 0.05},
    damping = {label = 'Damping', min = 0, max = 1, step = 0.05},
    width = {label = 'Width', min = 0, max = 1, step = 0.05},
}
local kTargets = {{id = 'bus', text = 'Music bus'}, {id = 'voice', text = 'Looping voice'}}
local kCode = "local muffle = audio.newEffect('lowpass', {cutoff = 800})\naudio.addBusEffect('music', muffle)\naudio.play(loop, {loop = true, effects = {echo}})\ntween.to(muffle, 2, {cutoff = 8000}, {repeatCount = -1, loopMode = 'yoyo'})"

function Effects:enter()
    self.kind = kKinds[1]
    self.target = 'bus'
    self:start()
    self:build()
    self:frame({
        hint = 'Pick an effect and a target, move the sliders or sweep the main parameter.',
        focus = 'kind',
        controls = {
            ui.formField{label = 'Effect', ui.combo{id = 'kind', items = sounds.items(kKinds), selected = self.kind.id, onChange = function(event) self:choose(event.value) end}},
            ui.formField{label = 'On', ui.segmentedControl{id = 'target', items = kTargets, selected = self.target, onChange = function(event) self:retarget(event.value) end}},
            ui.toggle{id = 'sweep', text = 'Sweep the main parameter', onChange = function(event) self:setSweep(event.checked) end},
            ui.column{id = 'params', gap = 12},
            ui.label{text = kCode, font = 'monospace'},
        },
    })
    self:showParams()
end

function Effects:exit()
    Effects.super.exit(self)
    self:detach()
    self:stopVoice()
    audio.stopMusic(0.3)
end

function Effects:stopVoice()
    if self.voice then
        audio.stop(self.voice, 0.2)
        self.voice = nil
    end
end

-- The music bus plays a track, and the voice plays a plucked loop on the effects bus.
function Effects:start()
    if self.target == 'bus' then
        self:stopVoice()
        audio.playMusic(sounds.track(sounds.tracks[2].path), {fade = 0.5, volume = 0.8})
    else
        audio.stopMusic(0.5)
        self.voice = audio.play(sounds.get('audio/generated/pluck_loop.wav'), {loop = true, volume = 0.8})
    end
end

function Effects:attach()
    if self.target == 'bus' then
        audio.addBusEffect('music', self.effect)
    else
        audio.addEffect(self.voice, self.effect)
    end
end

function Effects:detach()
    if self.target == 'bus' then
        audio.removeBusEffect('music', self.effect)
    else
        audio.removeEffect(self.voice, self.effect)
    end
end

function Effects:choose(id)
    self.kind = sounds.find(kKinds, id)
    self:build()
    self:showParams()
end

-- A new effect of the chosen kind replaces the current one on the target.
function Effects:build()
    if self.effect then
        self:detach()
    end
    self.effect = audio.newEffect(self.kind.id)
    self:attach()
    if self.sweeping then
        self:setSweep(true)
    end
end

-- Moving the effect to the other target creates it again with the same values, since an effect processes one bus or voice at a time.
function Effects:retarget(target)
    local values = {}
    for _, param in ipairs(self.kind.params) do
        values[param] = self.effect[param]
    end
    self:detach()
    self.target = target
    self:start()
    self.effect = audio.newEffect(self.kind.id, values)
    self:attach()
    if self.sweeping then
        self:setSweep(true)
    end
end

function Effects:setSweep(on)
    self.sweeping = on
    if self.sweepTween then
        self.sweepTween:kill()
        self.sweepTween = nil
    end
    if on then
        local param, from, to = table.unpack(self.kind.sweep)
        self.effect[param] = from
        self.sweepTween = tween.to(self.effect, 2.5, {[param] = to}, {repeatCount = -1, loopMode = 'yoyo', ease = 'sineInOut', owner = self})
    end
end

function Effects:showParams()
    local nodes = {}
    for _, param in ipairs(self.kind.params) do
        local info = kParams[param]
        local value = param == 'cutoff' and response.position(self.effect.cutoff) or self.effect[param]
        nodes[#nodes + 1] = ui.formField{label = info.label, ui.slider{id = param, min = info.min, max = info.max, step = info.step, value = value, showValue = param ~= 'cutoff', onChange = function(event)
            self.effect[param] = param == 'cutoff' and response.frequency(event.value) or event.value
        end}}
    end
    self.document:replaceChildren('params', nodes)
end

function Effects:update(dt)
    Effects.super.update(self, dt)
    local chain = self.target == 'bus' and audio.busEffects('music') or audio.effects(self.voice)
    local where = self.target == 'bus' and 'the music bus' or 'voice ' .. self.voice
    self:status(string.format('Effect "%s" on %s, %s, tail %.2f s, chain of %d with this effect %s', self.kind.id, where, self.effect.attached and 'attached' or 'detached', self.effect.tail, #chain, chain[1] == self.effect and 'first' or 'not first'))
end

function Effects:drawFilter(left, top, width, height)
    local low, high = -30, 24
    local function y(decibels)
        return top + height * (high - math.max(low, math.min(high, decibels))) / (high - low)
    end
    for _, decibels in ipairs({-24, -12, 0, 12}) do
        graphics2d.drawLine(left, y(decibels), left + width, y(decibels), decibels == 0 and 2 or 1, Test.line, {layer = 1})
        Test.caption(decibels .. ' dB', left - 10, y(decibels), {anchor = {1, 0.5}, size = 16})
    end
    for _, frequency in ipairs({50, 100, 500, 1000, 5000, 10000}) do
        local x = left + width * response.position(frequency)
        graphics2d.drawLine(x, top, x, top + height, 1, Test.line, {layer = 1})
        Test.caption(frequency >= 1000 and (frequency // 1000) .. ' kHz' or frequency .. ' Hz', x, top + height + 8, {anchor = {0.5, 0}, size = 16})
    end

    local effect = self.effect
    local gain = (effect.kind == 'peak' or effect.kind == 'lowShelf' or effect.kind == 'highShelf') and effect.gain or 0
    local points = {}
    for step = 0, 160 do
        local frequency = response.frequency(step / 160)
        points[#points + 1] = {left + width * step / 160, y(response.decibels(effect.kind, effect.cutoff, effect.q, gain, frequency))}
    end
    graphics2d.drawPolyline(points, 4, Test.accent, false, {layer = 2})
    local cutoffX = left + width * response.position(effect.cutoff)
    graphics2d.drawLine(cutoffX, top, cutoffX, top + height, 2, Test.warm, {layer = 2})
    Test.caption(string.format('%.0f Hz', effect.cutoff), cutoffX + 8, top + 8, {color = Test.warm})
end

function Effects:drawDelay(left, top, width, height)
    local effect = self.effect
    local seconds = 2.5
    local base = top + height
    graphics2d.drawLine(left, base, left + width, base, 2, Test.line, {layer = 1})
    graphics2d.drawRect({left, base - height * math.min(1, effect.dry), 10, height * math.min(1, effect.dry)}, Test.green, {layer = 2})
    local level = effect.wet
    local time = effect.time
    while time <= seconds and level > 0.01 do
        local x = left + width * time / seconds
        graphics2d.drawRect({x, base - height * math.min(1, level), 10, height * math.min(1, level)}, Test.accent, {layer = 2})
        level = level * effect.feedback
        time = time + effect.time
    end
    Test.caption('Dry input in green, then each repeat after ' .. string.format('%.2f', effect.time) .. ' seconds, over 2.5 seconds', left, top - 36)
end

function Effects:drawReverb(left, top, width, height)
    local effect = self.effect
    local seconds = math.max(1, effect.tail * 1.2)
    local points = {{left, top + height}}
    for step = 0, 100 do
        local time = seconds * step / 100
        local level = effect.wet * 10 ^ (-3 * time / math.max(effect.tail, 0.01))
        points[#points + 1] = {left + width * step / 100, top + height * (1 - math.min(1, level))}
    end
    points[#points + 1] = {left + width, top + height}
    graphics2d.drawPolygon(points, '#608FB0FF', {layer = 1})
    graphics2d.drawRect({left, top + height * (1 - math.min(1, effect.dry)), 10, height * math.min(1, effect.dry)}, Test.green, {layer = 2})
    Test.caption(string.format('Decay to -60 dB in %.2f seconds, drawn over %.1f seconds', effect.tail, seconds), left, top - 36)
end

function Effects:draw(area)
    local left, top, width, height = 90, 90, area.width - 140, area.height * 0.6
    graphics2d.drawRect({left, top, width, height}, Test.surface)
    Test.caption(self.kind.text .. ' on the ' .. (self.target == 'bus' and 'music bus' or 'looping voice'), left, 24, {size = 28, color = Test.ink})
    if self.kind.id == 'delay' then
        self:drawDelay(left, top, width, height)
    elseif self.kind.id == 'reverb' then
        self:drawReverb(left, top, width, height)
    else
        self:drawFilter(left, top, width, height)
    end

    local lines = {}
    for _, param in ipairs(self.kind.params) do
        lines[#lines + 1] = string.format('"%s" %.2f', param, self.effect[param])
    end
    Test.caption('Parameters ' .. table.concat(lines, ', '), left, top + height + 50, {size = 22, color = Test.ink})
end

return Effects
