-- Loudness and output: thirty explosions at once with the limiter of the master bus on and off and the gain it takes away, the threshold of the limiter, and the state and the latency of the output with the event that tells when a browser lets the sound play.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Output = haylen.class('Output', AudioTest)

local kExplosions = 30
local kCode = [[
local limiter = audio.busEffects('master')[1]
limiter.threshold = 0.9
print(limiter.reduction)  -- The share of the gain it takes away now.
print(audio.outputState(), audio.outputLatency())
events.on('audioUnlocked', function() print('The browser lets the sound play.') end, {owner = self})]]
-- The graph keeps this many seconds of the gain taken away, and its scale goes down to this many decibels.
local kHistory = 6
local kSamples = 180
local kFloor = -12

function Output:enter()
    self.limiter = audio.busEffects('master')[1]
    self.threshold = self.limiter and self.limiter.threshold or 0.9
    self.limited = self.limiter ~= nil
    self.reduction, self.unlocks = 0, 0
    self.history, self.sampleTime = {}, 0
    for index = 1, kSamples do
        self.history[index] = 0
    end
    self:listen('audioUnlocked', function() self.unlocks = self.unlocks + 1 end)
    -- A first burst goes off a moment after the test opens, so the graph shows what the limiter takes away.
    timer.after(0.8, function() self:explode() end, {owner = self})
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
    self.sampleTime = self.sampleTime + dt
    while self.sampleTime >= kHistory / kSamples do
        self.sampleTime = self.sampleTime - kHistory / kSamples
        table.remove(self.history, 1)
        self.history[kSamples] = reduction
    end
    self:status(string.format('The output is "%s" with a latency of %.1f ms, unlocked %d times, %d voices', audio.outputState(), audio.outputLatency() * 1000, self.unlocks, audio.voiceCount()))
end

-- The gain a reduction leaves, in decibels, within the floor of the scale.
function Output.decibels(reduction)
    return math.max(kFloor, 20 * math.log(math.max(1 - reduction, 1e-4), 10))
end

function Output:draw(area)
    -- The graph of the gain the limiter takes away, on a scale of decibels.
    local left, top = 96, 64
    local width, height = area.width - left - 360, area.height - top - 64
    Test.caption('Gain the limiter takes away, over the last 6 seconds', left, 20, {size = 24, color = Test.ink})
    graphics2d.drawShape({left, top, width, height}, {radius = 12, color = Test.surface, borderWidth = 1, borderColor = Test.line})
    for decibels = 0, kFloor, -3 do
        local y = top + height * decibels / kFloor
        graphics2d.drawLine(left, y, left + width, y, 1, Test.line, {layer = 1})
        Test.caption(string.format('%d dB', decibels), left - 14, y, {size = 18, anchor = {1, 0.5}})
    end
    local points = {}
    for index, reduction in ipairs(self.history) do
        points[#points + 1] = left + width * (index - 1) / (kSamples - 1)
        points[#points + 1] = top + height * Output.decibels(reduction) / kFloor
    end
    graphics2d.drawStroke(points, {width = 4, color = Test.red, join = 'round', cap = 'round', layer = 2})
    Test.caption(self.limited and string.format('Limiter on, threshold %.2f', self.threshold) or 'Limiter off, the sum may clip', left, top + height + 14, {size = 20, color = self.limited and Test.green or Test.red})

    -- The state of the output, with a padlock while the browser holds the sound back.
    local cardX = area.width - 320
    local state = audio.outputState()
    local color = state == 'running' and Test.green or (state == 'locked' and Test.warm or Test.muted)
    graphics2d.drawShape({cardX, top, 296, 220}, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = color})
    if state == 'locked' then
        graphics2d.drawArc(cardX + 60, top + 62, 18, 7, math.pi, math.pi * 2, color, {layer = 1})
        graphics2d.drawShape({cardX + 34, top + 60, 52, 42}, {radius = 8, color = color, layer = 1})
        graphics2d.drawCircle(cardX + 60, top + 79, 5, Test.surface, {layer = 2})
    else
        graphics2d.drawCircle(cardX + 60, top + 72, 16, color, {layer = 1})
    end
    Test.caption(state == 'locked' and 'Locked' or 'Output ' .. state, cardX + 104, top + 52, {size = 28, color = Test.ink, layer = 1})
    Test.caption(state == 'locked' and 'The first tap, click or key on the page lets the sound play.' or string.format('Latency %.1f ms, unlocked %d times.', audio.outputLatency() * 1000, self.unlocks), cardX + 28, top + 124, {size = 19, maxWidth = 240, layer = 1})
end

return Output
