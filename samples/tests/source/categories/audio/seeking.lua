-- Seeking a long track: a streamed Ogg Vorbis track of two and a half minutes that starts near its end and jumps back to its start, and whose voice then jumps to its end, to its start, ten seconds either way and anywhere, by hand or back and forth every second, with how long each jump took until the sound played on. A seek decodes only from the page of the file just before the new place, so a jump back from the end is as quick as any other.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local AudioTest = require('categories.audio.audio-test')
local sounds = require('categories.audio.sounds')
local Test = require('harness.test')

local Seeking = haylen.class('Seeking', AudioTest)

local kTrack = 'audio/music/journey.ogg'
local kCode = [[
local journey = assets.load('audio/music/journey.ogg', 'sound', {stream = true})
local track = audio.playMusic(journey, {loop = true})
audio.setCursor(track, journey.duration - 8)  -- Near the end.
audio.setCursor(track, 2)                     -- Back to the start at once.
print(audio.cursor(track))]]
-- The readout keeps this many jumps, and the jumps back and forth come this many seconds apart.
local kHistory = 7
local kSwingSeconds = 1
-- The track starts this many seconds before its end and jumps back to its start this many seconds after the test opens, so the readout shows a jump at once.
local kStartBeforeEnd = 12
local kFirstJump = 1.5
-- The track has three sections of fifty seconds.
local kSections = {'Calm', 'Steady', 'Lively'}

function Seeking:enter()
    self.track = sounds.track(kTrack)
    self.clock, self.swingTime, self.swingEnd = 0, 0, false
    self.jumps = {}
    self.voice = audio.playMusic(self.track, {fade = 0.3, loop = true, startAt = self.track.duration - kStartBeforeEnd})
    timer.after(kFirstJump, function() self:jump(2) end, {owner = self})
    self:frame({
        code = kCode,
        hint = 'Jump to the end and back to the start, and watch how long the sound takes to play on.',
        focus = 'end',
        controls = {
            ui.button{id = 'end', text = 'Jump near the end', variant = 'primary', onClick = function() self:jump(self.track.duration - 8) end},
            ui.button{id = 'start', text = 'Jump near the start', variant = 'primary', onClick = function() self:jump(2) end},
            ui.grid{columns = 2, gap = 12, alignItems = 'stretch',
                ui.button{id = 'rewind', text = 'Back 10 s', onClick = function() self:jump(audio.cursor(self.voice) - 10) end},
                ui.button{id = 'forward', text = 'Ahead 10 s', onClick = function() self:jump(audio.cursor(self.voice) + 10) end},
            },
            ui.button{id = 'anywhere', text = 'Jump anywhere', onClick = function() self:jump(math.random() * self.track.duration) end},
            ui.toggle{id = 'swing', text = 'Jump between the end and the start every second', checked = false, onChange = function(event) self.swinging = event.checked end},
            ui.label{text = 'Each jump moves the voice of the stream, which decodes the track again only from the page of the file just before the new place. The time counts from the jump until the voice plays on, which includes a frame and the period of the output.', color = 'textMuted', font = 'caption'},
        },
    })
end

function Seeking:exit()
    Seeking.super.exit(self)
    audio.stopMusic(0.3)
end

-- Moves the voice and starts timing the jump, which ends when the cursor moves on from the place it landed on.
function Seeking:jump(seconds)
    if not audio.active(self.voice) then
        return
    end
    -- A jump that comes before the one before it played on leaves that one unmeasured.
    local previous = self.jumps[1]
    if previous and not previous.took then
        previous.skipped = true
    end
    local from = audio.cursor(self.voice)
    audio.setCursor(self.voice, math.max(0, math.min(seconds, self.track.duration - 0.5)))
    local landed = audio.cursor(self.voice)
    table.insert(self.jumps, 1, {from = from, to = landed, started = self.clock})
    self.jumps[kHistory + 1] = nil
end

function Seeking:update(dt)
    Seeking.super.update(self, dt)
    self.clock = self.clock + haylen.unscaledDelta()

    -- A browser that keeps the sound locked holds the voice too, so the time of a jump counts from when it lets the sound play.
    local latest = self.jumps[1]
    if latest and not latest.took and AudioTest.locked() then
        latest.started = self.clock
    elseif latest and not latest.took and audio.active(self.voice) and audio.cursor(self.voice) ~= latest.to then
        latest.took = self.clock - latest.started
    end

    if self.swinging then
        self.swingTime = self.swingTime + haylen.unscaledDelta()
        if self.swingTime >= kSwingSeconds then
            self.swingTime = 0
            self.swingEnd = not self.swingEnd
            self:jump(self.swingEnd and self.track.duration - 8 or 2)
        end
    end
    self:status(self:describe())
end

-- Says where the track plays and how long the last jump took.
function Seeking:describe()
    if not audio.active(self.voice) then
        return 'The track does not play.'
    end
    if AudioTest.locked() then
        return 'The track waits at its place, since the browser plays no sound until the first click, tap or key on the page.'
    end
    local place = string.format('The track plays at %s of %s', AudioTest.clock(audio.cursor(self.voice)), AudioTest.clock(self.track.duration))
    local latest = self.jumps[1]
    if not latest then
        return place .. '.'
    end
    if not latest.took then
        return place .. ', and the last jump waits for its sound.'
    end
    return string.format('%s, and the last jump played on after %d ms.', place, math.floor(latest.took * 1000 + 0.5))
end

function Seeking:draw(area)
    local left, width = 96, area.width - 192
    local duration = self.track.duration
    local cursor = audio.active(self.voice) and audio.cursor(self.voice) or 0
    Test.caption(string.format('A streamed Ogg Vorbis track of %s at %.1f kHz, %s', AudioTest.clock(duration), self.track.sampleRate / 1000, self.track.channels == 1 and 'mono' or 'stereo'), left, 40, {size = 22, color = Test.ink})
    AudioTest.lockNote(left + width, 26)

    -- The timeline of the track, with its sections and the place that plays.
    local top = 100
    AudioTest.bar(left, top, width, 24, cursor / duration, Test.accent)
    for index, name in ipairs(kSections) do
        local x = left + width * (index - 1) / #kSections
        if index > 1 then
            graphics2d.drawLine(x, top - 8, x, top + 32, 2, Test.line, {layer = 3})
        end
        Test.caption(name, x + 8, top + 40, {size = 18})
    end
    Test.caption(AudioTest.clock(cursor), left + width * cursor / duration, top - 12, {size = 20, color = Test.ink, anchor = {0.5, 1}})

    -- The last jump in large type, and the ones before it in a list.
    local latest = self.jumps[1]
    local headline = 'Jump to hear how fast the track moves.'
    if latest then
        headline = latest.took and string.format('Played on %d ms after the jump', math.floor(latest.took * 1000 + 0.5)) or 'Waiting for the sound after the jump'
    end
    Test.caption(headline, left, top + 96, {size = 34, color = latest and latest.took and Test.green or Test.ink})
    local rowTop = top + 160
    for index, jump in ipairs(self.jumps) do
        local y = rowTop + (index - 1) * 40
        if y + 30 > area.height then
            break
        end
        local took = jump.took and string.format('%d ms', math.floor(jump.took * 1000 + 0.5)) or jump.skipped and 'Jumped again first' or 'Waiting'
        Test.caption(string.format('From %s to %s', AudioTest.clock(jump.from), AudioTest.clock(jump.to)), left, y, {size = 20, color = index == 1 and Test.ink or Test.muted})
        Test.caption(took, left + 360, y, {size = 20, color = index == 1 and Test.ink or Test.muted})
    end
end

return Seeking
