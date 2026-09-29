-- Music: two streamed tracks that crossfade over the chosen time, looping or played once, a volume that playMusic changes on the track already playing, the pause and resume of every voice and a graph of the fades as the calls asked for them.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')
local sounds = require('sounds')

local Music = haylen.class('Music', sample.Test)

local kHistory = 14
local kColors = {inn = sample.accent, market = sample.warm}
local kCode = "audio.playMusic(track, {fade = 2, loop = true, volume = 0.8})\naudio.stopMusic(2)\naudio.pauseAll()\naudio.resumeAll()\naudio.music() == track"

function Music:enter()
    self.fade, self.loop, self.volume = 2, true, 0.8
    self.clock = 0
    self.segments = {}
    self.pauses = {}
    self:frame({
        hint = 'Start a track, then the other one to hear the crossfade.',
        focus = 'inn',
        controls = {
            ui.button{id = 'inn', text = 'Play ' .. sounds.tracks[1].text, variant = 'primary', onClick = function() self:play(sounds.tracks[1]) end},
            ui.button{id = 'market', text = 'Play ' .. sounds.tracks[2].text, variant = 'primary', onClick = function() self:play(sounds.tracks[2]) end},
            ui.formField{label = 'Crossfade and stop seconds', ui.slider{id = 'fade', min = 0, max = 5, step = 0.25, value = self.fade, showValue = true, onChange = function(event) self.fade = event.value end}},
            ui.toggle{id = 'loop', text = 'Loop the next track', checked = true, onChange = function(event) self.loop = event.checked end},
            ui.formField{label = 'Track volume', ui.slider{id = 'volume', min = 0, max = 1, step = 0.05, value = self.volume, showValue = true, onChange = function(event) self:setVolume(event.value) end}},
            ui.row{gap = 12,
                ui.button{id = 'pause', text = 'Pause', onClick = function() self:pause() end},
                ui.button{id = 'resume', text = 'Resume', onClick = function() self:resume() end},
                ui.button{id = 'stop', text = 'Stop', variant = 'destructive', onClick = function() self:stop() end},
            },
            ui.label{text = 'Asking for the track that already plays keeps it going and only changes its volume, which the volume slider uses. The music has no voice id, so Pause and Resume hold every voice with pauseAll and resumeAll.', color = 'textMuted', font = 'caption'},
            ui.label{text = kCode, font = 'monospace'},
        },
    })
end

function Music:exit()
    audio.resumeAll()
    audio.stopMusic(0.3)
end

function Music:current()
    local segment = self.segments[#self.segments]
    return segment and not segment.stop and segment or nil
end

-- Ends the segment of the graph that plays now, fading out over `fade` seconds.
function Music:endCurrent(fade)
    local current = self:current()
    if current then
        current.stop, current.fadeOut = self.clock, fade
    end
end

function Music:play(track)
    local current = self:current()
    audio.playMusic(sounds.track(track.path), {fade = self.fade, loop = self.loop, volume = self.volume})
    if current and current.track == track then
        return
    end
    self:endCurrent(self.fade)
    self.segments[#self.segments + 1] = {track = track, start = self.clock, fadeIn = self.fade, levels = {{self.clock, self.volume}}, loop = self.loop}
end

-- Playing the same track again changes only its volume.
function Music:setVolume(volume)
    self.volume = volume
    local current = self:current()
    if current then
        audio.playMusic(sounds.track(current.track.path), {fade = self.fade, loop = current.loop, volume = volume})
        current.levels[#current.levels + 1] = {self.clock, volume}
    end
end

function Music:pause()
    if not self.pausedAt then
        audio.pauseAll()
        self.pausedAt = self.clock
    end
end

function Music:resume()
    if self.pausedAt then
        audio.resumeAll()
        self.pauses[#self.pauses + 1] = {self.pausedAt, self.clock}
        self.pausedAt = nil
    end
end

function Music:stop()
    audio.stopMusic(self.fade)
    self:endCurrent(self.fade)
end

function Music:update(dt)
    Music.super.update(self, dt)
    self.clock = self.clock + haylen.unscaledDelta()

    -- A track played once ends by itself, and the music then reports no sound.
    local current = self:current()
    if current and not audio.music() and self.clock - current.start > 0.5 then
        self:endCurrent(0)
    end

    local playing = audio.music()
    local name = 'none'
    for _, track in ipairs(sounds.tracks) do
        if playing == sounds.track(track.path) then
            name = track.text
        end
    end
    self:status(string.format('music() %s   paused %s   voices %d   music bus %.2f', name, self.pausedAt ~= nil, audio.voiceCount(), audio.busVolume('music')))
end

-- The volume the calls asked for at a time: the fade in, the level set last and the fade out.
function Music:level(segment, time)
    if time < segment.start or segment.stop and time > segment.stop + segment.fadeOut then
        return nil
    end
    local volume = segment.levels[1][2]
    for _, level in ipairs(segment.levels) do
        if level[1] <= time then
            volume = level[2]
        end
    end
    local fadeIn = segment.fadeIn > 0 and math.min(1, (time - segment.start) / segment.fadeIn) or 1
    local fadeOut = 1
    if segment.stop and time > segment.stop then
        fadeOut = segment.fadeOut > 0 and math.max(0, 1 - (time - segment.stop) / segment.fadeOut) or 0
    end
    return volume * fadeIn * fadeOut
end

function Music:draw(area)
    local left, top, width, height = 60, 90, area.width - 100, area.height * 0.5
    local from = self.clock - kHistory
    graphics2d.drawRect({left, top, width, height}, sample.surface)
    for index = 0, kHistory, 2 do
        local x = left + width * index / kHistory
        graphics2d.drawLine(x, top, x, top + height, 1, sample.line, {layer = 1})
        sample.caption(string.format('%ds', index - kHistory), x, top + height + 8, {anchor = {0.5, 0}, size = 18})
    end
    sample.caption('Volume the calls asked for, over the last ' .. kHistory .. ' seconds', left, top - 40)

    local spans = {}
    for _, pause in ipairs(self.pauses) do
        spans[#spans + 1] = pause
    end
    if self.pausedAt then
        spans[#spans + 1] = {self.pausedAt, self.clock}
    end
    for _, span in ipairs(spans) do
        local a = math.max(0, (span[1] - from) / kHistory)
        local b = math.max(0, (span[2] - from) / kHistory)
        if b > a then
            graphics2d.drawRect({left + width * a, top, width * (b - a), height}, '#30FF8A84', {layer = 1})
        end
    end

    for _, segment in ipairs(self.segments) do
        local points = {}
        for step = 0, 140 do
            local time = from + kHistory * step / 140
            local volume = self:level(segment, time)
            if volume then
                points[#points + 1] = {left + width * step / 140, top + height * (1 - volume)}
            end
        end
        if #points > 1 then
            graphics2d.drawPolyline(points, 4, kColors[segment.track.id], false, {layer = 2})
        end
    end

    local y = top + height + 60
    for index, track in ipairs(sounds.tracks) do
        local sound = sounds.track(track.path)
        local playing = audio.music() == sound
        graphics2d.drawCircle(left + 12, y + 14 + (index - 1) * 44, 10, kColors[track.id], {layer = 1})
        sample.caption(string.format('%s   %.0f seconds, %d Hz, %d channels, streamed %s%s', track.text, sound.duration, sound.sampleRate, sound.channels, sound.streamed, playing and '   playing' or ''), left + 34, y + (index - 1) * 44, {color = playing and sample.ink or sample.muted})
    end
    sample.caption('Red spans: every voice paused with pauseAll', left, y + 100, {size = 18})
end

return Music
