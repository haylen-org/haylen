-- Loudness and output: thirty explosions at once with the limiter of the master bus on and off and the gain it takes away, the threshold of the limiter, and the state and the latency of the output with the event that tells when a browser lets the sound play.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Output = haylen.class('Output', AudioTest)

local kExplosions = 30
local kCode = "audio.busEffects('master')[1].threshold = 0.9\naudio.outputState()\naudio.outputLatency()\nevents.on('audioUnlocked', fn)"

function Output:enter()
    self.limiter = audio.busEffects('master')[1]
    self.threshold = self.limiter and self.limiter.threshold or 0.9
    self.limited = self.limiter ~= nil
    self.reduction, self.unlocks = 0, 0
    self:listen('audioUnlocked', function() self.unlocks = self.unlocks + 1 end)
    self:frame({
        code = kCode,
        hint = 'Play thirty explosions at once with the limiter on and off, and listen for the crackle it removes.',
        controls = {
            ui.button{id = 'boom', text = 'Thirty explosions at once', variant = 'primary', onClick = function() self:explode() end},
            ui.toggle{id = 'limited', text = 'Limiter on the master bus', checked = self.limited, onChange = function(event) self:setLimited(event.checked) end},
            ui.formField{label = 'Threshold', ui.slider{id = 'threshold', min = 0.3, max = 1, step = 0.05, value = self.threshold, showValue = true, onChange = function(event)
                self.threshold = event.value
                if self.limiter then
                    self.limiter.threshold = event.value
                end
            end}},
        },
    })
end

function Output:exit()
    Output.super.exit(self)
    self:setLimited(true)
    if self.limiter then
        self.limiter.threshold = 0.9
    end
end

function Output:setLimited(enabled)
    self.limited = enabled and self.limiter ~= nil
    if not self.limiter then
        return
    end
    if enabled and not self.limiter.attached then
        audio.addBusEffect('master', self.limiter)
    elseif not enabled and self.limiter.attached then
        audio.removeBusEffect('master', self.limiter)
    end
end

function Output:explode()
    local boom = sounds.get('audio/effects/explosion.ogg')
    for _ = 1, kExplosions do
        audio.play(boom, {pitchVariation = 0.05, priority = 200})
    end
end

function Output:update(dt)
    Output.super.update(self, dt)
    local reduction = self.limiter and self.limiter.attached and self.limiter.reduction or 0
    self.reduction = math.max(reduction, self.reduction - dt)
    self:status(string.format('Output "%s", latency %.1f ms, unlocked %d times, voices %d', audio.outputState(), audio.outputLatency() * 1000, self.unlocks, audio.voiceCount()))
end

function Output:draw(area)
    Test.caption('Gain the limiter takes away', 40, 40, {size = 24, color = Test.ink})
    AudioTest.bar(40, 80, area.width - 80, 28, self.reduction, Test.red)
    Test.caption(self.limited and string.format('Limiter on, threshold %.2f', self.threshold) or 'Limiter off, the sum may clip', 40, 120, {color = self.limited and Test.green or Test.red})
    local state = audio.outputState()
    graphics2d.drawCircle(70, 220, 24, state == 'running' and Test.green or (state == 'locked' and Test.warm or AudioTest.idle), {layer = 1})
    Test.caption(state == 'locked' and 'Locked until the first tap, click or key' or 'Output ' .. state, 110, 208, {color = Test.ink})
end

return Output
