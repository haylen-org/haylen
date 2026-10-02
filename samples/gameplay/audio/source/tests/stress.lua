-- Many voices: bursts and a steady rain of quiet voices up to the limit of 128, where a new voice stops the oldest one that is not music, with the count of voices over time and the statistics of every bus.
local audio = require('haylen.audio')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')
local sounds = require('sounds')

local Stress = haylen.class('Stress', sample.Test)

local kLimit = 128
local kBuses = {'sfx', 'ui', 'ambience'}
local kShort = {'audio/effects/chop.ogg', 'audio/effects/hit_1.ogg', 'audio/effects/footstep_1.ogg', 'audio/ui/click.ogg', 'audio/effects/wood_pickup.ogg'}
local kLong = {'audio/effects/explosion.ogg', 'audio/generated/pluck_loop.wav', 'audio/ambient/fire_loop.ogg'}

function Stress:enter()
    self.history = collections.newRingBuffer(360)
    self.rate, self.debt, self.started, self.long = 0, 0, 0, false
    audio.playMusic(sounds.track(sounds.tracks[1].path), {fade = 1, volume = 0.4})
    self:frame({
        hint = 'Start a burst or a rain of voices and watch the count stop at the limit while the music plays on.',
        focus = 'burst32',
        controls = {
            ui.row{gap = 12,
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
    audio.stopAll(0.2)
end

function Stress:spawn()
    local paths = self.long and kLong or kShort
    self.started = self.started + 1
    audio.play(sounds.get(paths[math.random(#paths)]), {bus = kBuses[self.started % #kBuses + 1], volume = 0.12, pitchVariation = 0.2, pan = math.random() * 2 - 1})
end

function Stress:burst(count)
    for _ = 1, count do
        self:spawn()
    end
end

function Stress:update(dt)
    Stress.super.update(self, dt)
    self.debt = self.debt + self.rate * dt
    while self.debt >= 1 do
        self.debt = self.debt - 1
        self:spawn()
    end
    self.history:push(audio.voiceCount())
    self.stats = audio.busStats()
    self:status(string.format('Voices %d of %d   started %d   music %s   frame %.1f ms', audio.voiceCount(), kLimit, self.started, audio.music() and 'playing' or 'stopped', haylen.unscaledDelta() * 1000))
end

function Stress:draw(area)
    local left, top, width, height = 70, 60, area.width - 110, area.height * 0.46
    graphics2d.drawRect({left, top, width, height}, sample.surface)
    local limitY = top + height * (1 - kLimit / 160)
    graphics2d.drawLine(left, limitY, left + width, limitY, 2, sample.red, {layer = 1})
    sample.caption('Limit 128', left + width - 8, limitY - 6, {anchor = {1, 1}, color = sample.red, size = 18})
    sample.caption('Voices over the last six seconds', left, top - 40)

    local values = self.history:values()
    local points = {}
    for index, count in ipairs(values) do
        points[index] = {left + width * (index - 1) / 359, top + height * (1 - math.min(count, 160) / 160)}
    end
    if #points > 1 then
        graphics2d.drawPolyline(points, 3, sample.accent, false, {layer = 2})
    end

    local y = top + height + 60
    sample.caption('Bus statistics from "audio.busStats()"', left, y - 36)
    for index, bus in ipairs(self.stats or {}) do
        local rowY = y + (index - 1) * 40
        sample.caption(bus.name, left, rowY, {color = sample.ink, size = 20})
        sample.bar(left + 160, rowY + 4, width * 0.45, 16, bus.voices / kLimit, bus.processing and sample.accent or sample.red)
        sample.caption(string.format('%d voices, %d playing, %d paused, processing %s', bus.voices, bus.playing, bus.paused, bus.processing), left + 180 + width * 0.45, rowY, {size = 18})
    end
end

return Stress
