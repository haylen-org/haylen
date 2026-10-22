-- Many voices: bursts and a steady rain of quiet voices up to the limit of 128, where a new voice stops the oldest one that is not music, with the count of voices over time and the statistics of every bus.
local audio = require('haylen.audio')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Stress = haylen.class('Stress', AudioTest)

Stress.code = [[
for _ = 1, 160 do  -- Past 128 voices, a new voice stops the oldest one that is not music.
    audio.play(chop, {bus = 'sfx', volume = 0.12, pitchVariation = 0.2, pan = math.random() * 2 - 1})
end
print(audio.voiceCount(), audio.music() ~= nil)
audio.stopAll(0.2)]]

local kLimit = 128
local kBuses = {'sfx', 'ui', 'ambience'}
local kShort = {'audio/effects/chop.ogg', 'audio/effects/hit-1.ogg', 'audio/effects/footstep-1.ogg', 'audio/ui/click.ogg', 'audio/effects/wood-pickup.ogg'}
local kLong = {'audio/effects/explosion.ogg', 'audio/generated/pluck-loop.wav', 'audio/ambient/fire-loop.ogg'}

function Stress:enter()
    self.history = collections.newRingBuffer(360)
    self.rate, self.debt, self.started, self.long = 0, 0, 0, false
    audio.playMusic(sounds.track(sounds.tracks[1].path), {fade = 1, volume = 0.4})
    self:frame({
        hint = 'Start a burst or a rain of voices and watch the count stop at the limit while the music plays on.',
        code = Stress.code,
        focus = 'burst32',
        controls = {
            ui.grid{columns = 2, gap = 12, alignItems = 'stretch',
                ui.button{id = 'burst32', text = 'Burst of 32', variant = 'primary', onClick = function() self:burst(32) end},
                ui.button{id = 'burst160', text = 'Burst of 160', variant = 'primary', onClick = function() self:burst(160) end},
            },
            ui.formField{label = 'Voices per second', ui.slider{id = 'rate', min = 0, max = 240, step = 10, value = 0, showValue = true, decimals = 0, onChange = function(event) self.rate = event.value end}},
            ui.toggle{id = 'long', text = 'Long sounds that pile up', onChange = function(event) self.long = event.checked end},
            ui.button{id = 'stopAll', text = 'Stop every voice, music too', variant = 'destructive', onClick = function() audio.stopAll(0.2) end},
            ui.button{id = 'music', text = 'Start the music again', onClick = function() audio.playMusic(sounds.track(sounds.tracks[1].path), {fade = 1, volume = 0.4}) end},
            ui.label{text = 'At most 128 voices play at once. A new voice past the limit stops the oldest voice that is not music, and finished voices leave once per frame.', color = 'textMuted', font = 'caption'},
        },
    })
end

function Stress:exit()
    Stress.super.exit(self)
    audio.stopAll(0.2)
end

-- Starts one quiet voice on the next bus, of a short sound or of a long one that piles up.
function Stress:playOne()
    local paths = self.long and kLong or kShort
    self.started = self.started + 1
    audio.play(sounds.get(paths[math.random(#paths)]), {bus = kBuses[self.started % #kBuses + 1], volume = 0.12, pitchVariation = 0.2, pan = math.random() * 2 - 1})
end

function Stress:burst(count)
    for _ = 1, count do
        self:playOne()
    end
end

function Stress:update(dt)
    Stress.super.update(self, dt)
    self.debt = self.debt + self.rate * dt
    while self.debt >= 1 do
        self.debt = self.debt - 1
        self:playOne()
    end
    self.history:push(audio.voiceCount())
    self.stats = audio.busStats()
    local music = audio.music() and ', the music among them' or ', and the music is stopped'
    self:status(string.format('%d of the %d voices play now%s. This test started %d voices, and the last frame took %.1f ms.', audio.voiceCount(), kLimit, music, self.started, haylen.unscaledDelta() * 1000))
end

-- Says what a bus plays, such as "10 playing and 2 paused".
function Stress.describe(bus)
    local text = bus.voices == 0 and 'Silent' or bus.playing .. ' playing'
    if bus.paused > 0 then
        text = text .. ' and ' .. bus.paused .. ' paused'
    end
    return bus.processing and text or text .. ', held by the pause'
end

-- The chart takes the height that the statistics of the buses leave, which sit in two columns under it.
function Stress:draw(area)
    local stats = self.stats or {}
    local rows = math.max(1, math.ceil(#stats / 2))
    local rowHeight = 40
    local left, top, width = 70, 60, area.width - 110
    local height = math.max(120, area.height - top - 96 - rows * rowHeight)
    graphics2d.drawShape({left, top, width, height}, {radius = 10, color = Test.surface})
    for count = 0, 160, 32 do
        local y = top + height * (1 - count / 160)
        graphics2d.drawLine(left, y, left + width, y, 1, Test.line, {layer = 1})
        Test.caption(tostring(count), left - 10, y, {anchor = {1, 0.5}, size = 17})
    end
    local limitY = top + height * (1 - kLimit / 160)
    graphics2d.drawLine(left, limitY, left + width, limitY, 2, Test.red, {layer = 1})
    Test.caption('Limit 128', left + width - 8, limitY - 6, {anchor = {1, 1}, color = Test.red, size = 18})
    Test.caption('Voices over the last six seconds', left, top - 40, {size = 22})

    local values = self.history:values()
    local points = {}
    for index, count in ipairs(values) do
        points[#points + 1] = left + width * (index - 1) / 359
        points[#points + 1] = top + height * (1 - math.min(count, 160) / 160)
    end
    if #points >= 4 then
        graphics2d.drawStroke(points, {width = 3, color = Test.accent, join = 'round', layer = 2})
    end

    local y = top + height + 72
    Test.caption('Voices of every bus from "audio.busStats()", out of the limit of 128', left, y - 36, {size = 20})
    local column = (width + 40) / 2
    for index, bus in ipairs(stats) do
        local x, rowY = left + (index - 1) // rows * column, y + (index - 1) % rows * rowHeight
        Test.caption('Bus "' .. bus.name .. '"', x, rowY + 10, {color = Test.ink, size = 20, anchor = {0, 0.5}})
        AudioTest.bar(x + 150, rowY + 2, column * 0.34, 16, bus.voices / kLimit, bus.processing and Test.accent or Test.red)
        Test.caption(Stress.describe(bus), x + 166 + column * 0.34, rowY + 10, {size = 18, anchor = {0, 0.5}})
    end
end

return Stress
