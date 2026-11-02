-- Music: two streamed tracks that crossfade over the chosen time, looping or played once, started partway and repeated from a loop start after their intro, a volume that `playMusic` changes on the track already playing, the pause, resume and cursor of the music through its voice and a graph of the fades as the calls asked for them.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Music = haylen.class('Music', AudioTest)

local kHistory = 14
local kColors = {calm = Test.accent, lively = Test.warm}
local kPaused = '#30FF8A84'
-- The plot of the volume sits this far inside its card, so the lines at the edges of the scale stay clear of the rounded corners.
local kInset = 16
-- The pieces of the curve where a stop cuts a fade in short and the two fades multiply.
local kCurveSteps = 8
local kCode = "local music = audio.playMusic(track, {fade = 2, loop = true, volume = 0.8, startAt = 0, loopStart = 4})\naudio.pause(music)\naudio.resume(music)\naudio.setCursor(music, audio.cursor(music) + 10)\naudio.stopMusic(2)\naudio.music() == track"

function Music:enter()
    self.fade, self.loop, self.volume, self.startAt, self.loopStart = 2, true, 0.8, 0, 0
    self.clock = 0
    self.stroke = {width = 4, color = Test.accent, join = 'round', cap = 'round', layer = 2}
    self.segments = {}
    self.pauses = {}
    self:frame({
        code = kCode,
        hint = 'Start a track, then the other one to hear the crossfade.',
        focus = 'calm',
        controls = {
            ui.button{id = 'calm', text = 'Play the ' .. sounds.tracks[1].text:lower(), variant = 'primary', onClick = function() self:play(sounds.tracks[1]) end},
            ui.button{id = 'lively', text = 'Play the ' .. sounds.tracks[2].text:lower(), variant = 'primary', onClick = function() self:play(sounds.tracks[2]) end},
            ui.grid{columns = 2, gap = 12, alignItems = 'stretch',
                ui.button{id = 'pause', text = 'Pause', onClick = function() self:pause() end},
                ui.button{id = 'resume', text = 'Resume', onClick = function() self:resume() end},
                ui.button{id = 'rewind', text = 'Back 10 s', onClick = function() self:seek(-10) end},
                ui.button{id = 'forward', text = 'Ahead 10 s', onClick = function() self:seek(10) end},
            },
            ui.button{id = 'stop', text = 'Stop', variant = 'destructive', onClick = function() self:stop() end},
            ui.formField{label = 'Crossfade and stop seconds', ui.slider{id = 'fade', min = 0, max = 5, step = 0.25, value = self.fade, showValue = true, onChange = function(event) self.fade = event.value end}},
            ui.toggle{id = 'loop', text = 'Loop the next track', checked = true, onChange = function(event) self.loop = event.checked end},
            ui.formField{label = 'Repeat the next track from seconds', ui.slider{id = 'loopStart', min = 0, max = 20, step = 1, decimals = 0, value = self.loopStart, showValue = true, onChange = function(event) self.loopStart = event.value end}},
            ui.formField{label = 'Start the next track at seconds', ui.slider{id = 'startAt', min = 0, max = 20, step = 1, decimals = 0, value = self.startAt, showValue = true, onChange = function(event) self.startAt = event.value end}},
            ui.formField{label = 'Track volume', ui.slider{id = 'volume', min = 0, max = 1, step = 0.05, value = self.volume, showValue = true, onChange = function(event) self:setVolume(event.value) end}},
            ui.label{text = 'Asking for the track that already plays keeps it going on the same voice and only changes its volume, which the volume slider uses. Pause and Resume hold only the music, through the voice that "playMusic" returns.', color = 'textMuted', font = 'caption'},
        },
    })
    -- The calm track fades in as the test opens, so the graph shows a fade at once.
    self:play(sounds.tracks[1])
end

function Music:exit()
    Music.super.exit(self)
    audio.stopMusic(0.3)
end

function Music:current()
    local segment = self.segments[#self.segments]
    return segment and not segment.stop and segment or nil
end

-- Ends the segment of the graph that plays now, fading out over `fade` seconds, or at once when the track was paused, since a paused track fades out in silence.
function Music:endCurrent(fade)
    local current = self:current()
    if current then
        current.stop, current.fadeOut = self.clock, self.pausedAt and 0 or fade
    end
    self:endPause()
end

-- Closes the red span of a pause.
function Music:endPause()
    if self.pausedAt then
        self.pauses[#self.pauses + 1] = {self.pausedAt, self.clock}
        self.pausedAt = nil
    end
end

function Music:play(track)
    local current = self:current()
    -- The loop starts inside the track, a second before its end at the latest.
    local sound = sounds.track(track.path)
    self.voice = audio.playMusic(sound, {fade = self.fade, loop = self.loop, volume = self.volume, startAt = self.startAt, loopStart = math.min(self.loopStart, math.max(0, sound.duration - 1))})
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
    if self:current() and not self.pausedAt then
        audio.pause(self.voice)
        self.pausedAt = self.clock
    end
end

function Music:resume()
    if self.pausedAt then
        audio.resume(self.voice)
        self:endPause()
    end
end

-- Moves the music through its voice, which keeps playing from the new place, or waits there while it is paused.
function Music:seek(seconds)
    if self:current() then
        audio.setCursor(self.voice, audio.cursor(self.voice) + seconds)
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

    self:status(self:describe())
end

-- Says what the music does now in a sentence: the track, its place in it and its state.
function Music:describe()
    local current = self:current()
    if not current or not self.voice or not audio.active(self.voice) then
        return 'No track plays. Start one with a button of the panel.'
    end
    local name = 'The ' .. current.track.text:lower()
    if AudioTest.locked() then
        return name .. ' waits at its start, since the browser plays no sound until the first click, tap or key on the page.'
    end
    local duration = sounds.track(current.track.path).duration
    local place = string.format('%s of %s', AudioTest.clock(audio.cursor(self.voice) % duration), AudioTest.clock(duration))
    if audio.paused(self.voice) then
        return string.format('%s is paused at %s.', name, place)
    end
    local before = self.segments[#self.segments - 1]
    local fading = before and before.stop and self.clock < before.stop + before.fadeOut
    return string.format('%s plays at %s%s.', name, place, fading and ', while the track before it fades out' or '')
end

-- The volume the calls asked for at a time: the fade in, the level set last and the fade out. With `before`, a level set or a stop at that very time does not count yet, which gives the volume just before a step.
function Music:level(segment, time, before)
    if time < segment.start or segment.stop and time > segment.stop + segment.fadeOut then
        return nil
    end
    local volume = segment.levels[1][2]
    for _, level in ipairs(segment.levels) do
        if level[1] < time or level[1] == time and not before then
            volume = level[2]
        end
    end
    local fadeIn = segment.fadeIn > 0 and math.min(1, (time - segment.start) / segment.fadeIn) or 1
    local fadeOut = 1
    if segment.stop and (time > segment.stop or time == segment.stop and not before) then
        fadeOut = segment.fadeOut > 0 and math.max(0, 1 - (time - segment.stop) / segment.fadeOut) or 0
    end
    return volume * fadeIn * fadeOut
end

-- The times from `first` to `last` where the volume of a segment turns: the end of its fade in, each level set, its stop and the end of its fade out. The volume runs straight between them, except while a stop cuts its fade in short, where the two fades multiply into a curve that gets times of its own.
function Music.turns(segment, first, last)
    local times = {first, segment.start + segment.fadeIn}
    for _, level in ipairs(segment.levels) do
        times[#times + 1] = level[1]
    end
    if segment.stop then
        times[#times + 1] = segment.stop
        times[#times + 1] = segment.stop + segment.fadeOut
        local fadeInEnd = segment.start + segment.fadeIn
        if segment.stop < fadeInEnd then
            local curveEnd = math.min(fadeInEnd, segment.stop + segment.fadeOut)
            for step = 1, kCurveSteps - 1 do
                times[#times + 1] = segment.stop + (curveEnd - segment.stop) * step / kCurveSteps
            end
        end
    end
    times[#times + 1] = last
    table.sort(times)

    local kept = {}
    for _, time in ipairs(times) do
        if time >= first and time <= last and time ~= kept[#kept] then
            kept[#kept + 1] = time
        end
    end
    return kept
end

-- The line of a segment in the plot, from the exact times where its volume turns, so its corners and its ends stay where they are as the history scrolls instead of jumping between samples. A step of the volume becomes a vertical piece.
function Music:curve(segment, from, plot)
    local first = math.max(from, segment.start)
    local last = segment.stop and math.min(self.clock, segment.stop + segment.fadeOut) or self.clock
    if last <= first then
        return nil
    end
    local points = {}
    local function add(time, volume)
        points[#points + 1] = plot.x + plot.width * (time - from) / kHistory
        points[#points + 1] = plot.y + plot.height * (1 - volume)
    end
    for _, time in ipairs(Music.turns(segment, first, last)) do
        local before = time > first and self:level(segment, time, true)
        local volume = self:level(segment, time)
        if before and before ~= volume then
            add(time, before)
        end
        add(time, volume)
    end
    return #points >= 4 and points or nil
end

function Music:draw(area)
    local left, top, width, height = 96, 90, area.width - 136, area.height * 0.5
    local plot = {x = left + kInset, y = top + kInset, width = width - kInset * 2, height = height - kInset * 2}
    local from = self.clock - kHistory
    Test.caption(string.format('Volume the calls asked for, over the last %d seconds', kHistory), left, top - 40, {size = 22})
    AudioTest.lockNote(left + width, top - 54)
    graphics2d.drawShape({left, top, width, height}, {radius = 12, color = Test.surface})
    for volume = 0, 1, 0.25 do
        local y = plot.y + plot.height * (1 - volume)
        graphics2d.drawLine(plot.x, y, plot.x + plot.width, y, 1, Test.line, {layer = 1})
        Test.caption(string.format('%g', volume), left - 14, y, {anchor = {1, 0.5}, size = 18})
    end
    for index = 0, kHistory, 2 do
        local x = plot.x + plot.width * index / kHistory
        graphics2d.drawLine(x, plot.y, x, plot.y + plot.height, 1, Test.line, {layer = 1})
        Test.caption(string.format('%d s', index - kHistory), x, top + height + 8, {anchor = {0.5, 0}, size = 18})
    end

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
            graphics2d.drawRect({plot.x + plot.width * a, plot.y, plot.width * (b - a), plot.height}, kPaused, {layer = 1})
        end
    end

    local stroke = self.stroke
    for _, segment in ipairs(self.segments) do
        local points = self:curve(segment, from, plot)
        if points then
            stroke.color = kColors[segment.track.id]
            graphics2d.drawStroke(points, stroke)
        end
    end

    -- The legend names each track with what it is, marks the one that plays and shows what a red span means.
    local y = top + height + 54
    local current = self:current()
    for index, track in ipairs(sounds.tracks) do
        local sound = sounds.track(track.path)
        local rowY = y + (index - 1) * 44
        local playing = current ~= nil and current.track == track
        local text = string.format('%s: a %s %s of %d seconds at %.1f kHz', track.text, sound.channels == 1 and 'mono' or 'stereo', sound.streamed and 'stream' or 'decoded sound', math.floor(sound.duration + 0.5), sound.sampleRate / 1000)
        graphics2d.drawCircle(left + 12, rowY + 14, 10, kColors[track.id], {layer = 1})
        Test.caption(text, left + 34, rowY, {size = 22, color = playing and Test.ink or Test.muted})
        if playing then
            local textWidth = graphics2d.measureText(nil, text, {size = 22})
            graphics2d.drawShape({left + 50 + textWidth, rowY - 2, 120, 32}, {radius = 16, color = '#FF2C3A66', layer = 1})
            Test.caption('Now playing', left + 110 + textWidth, rowY + 14, {size = 18, color = Test.accent, anchor = {0.5, 0.5}})
        end
    end
    local spanY = y + 2 * 44 + 10
    graphics2d.drawShape({left + 2, spanY + 2, 20, 20}, {radius = 4, color = kPaused, borderWidth = 2, borderColor = Test.red, layer = 1})
    Test.caption('A red span marks a pause of the music through its voice', left + 34, spanY, {size = 19})
end

return Music
