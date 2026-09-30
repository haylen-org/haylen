-- Streams: the native part draws an animated pattern 30 times per second into a video stream, whose texture the app draws and which stays current by itself, and synthesizes a tone into an audio stream, which the app plays as a voice and reads for a level meter. Swift pushes both from dispatch queues on Apple platforms, Kotlin from threads of its own on Android, C from threads of its library on the desktops and JavaScript from timers of the page on the web.
local audio = require('haylen.audio')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Streams = haylen.class('Streams', sample.Test)

local kFramesToPass = 60
local kToneSeconds = 2
local kLevelToPass = 0.1
local kVideoName = 'the video stream reaches the texture'
local kToneName = 'the audio stream plays as a voice'

function Streams:enter()
    self.samples = collections.newFloatBuffer(1024)
    self:frame({
        hint = 'Start the video and the tone, watch and listen, and stop them.',
        focus = 'video',
        controls = {
            ui.button{id = 'video', text = 'Start the video', variant = 'primary', onClick = function() self:toggleVideo() end},
            ui.button{id = 'tone', text = 'Start the tone', onClick = function() self:toggleTone() end},
            ui.label{text = 'The native part pushes frames and samples from any thread. The engine keeps only the newest frame and uploads it once per frame at most, and the voice reads the ring of the tone, resampled to the mixer and silent where samples are missing.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "demo.startVideo():await()\nlocal video = demo.videoStream()\ngraphics2d.draw(video.texture, x, y)\n\ndemo.startTone(440):await()\nlocal tone = demo.audioStream()\nlocal voice = tone:play({volume = 0.5})\ntone:read(buffer)"},
        },
    })
end

function Streams:exit()
    self:stopVideo()
    self:stopTone()
end

function Streams:toggleVideo()
    if self.video then
        self:stopVideo()
        return
    end
    self:act(function()
        local started, err = demo.startVideo():await()
        if err then
            self.results:failure('video', kVideoName, err)
            return
        end
        self.video = demo.videoStream()
        self.videoInfo = started
        self.videoFrames = 0
        self.videoStarted = haylen.elapsed()
        self.frames = self.video:on('frame', function() self:videoFrame() end)
        self:set('video', {text = 'Stop the video'})
        self.results:set('video', 'waiting', kVideoName, string.format('%s pushes %d by %d %s frames %d times per second from %s.', started.language, started.width, started.height, started.format, started.fps, started.thread))
    end)
end

function Streams:videoFrame()
    self.videoFrames = self.videoFrames + 1
    local seconds = haylen.elapsed() - self.videoStarted
    local sized = self.video.width == self.videoInfo.width and self.video.height == self.videoInfo.height
    local state = self.videoFrames >= kFramesToPass and sized and 'pass' or 'waiting'
    self.results:set('video', state, kVideoName, string.format('%d frames of %d by %d pixels reached the texture, %.1f per second, and it shows the newest one.', self.videoFrames, self.video.width, self.video.height, seconds > 0 and self.videoFrames / seconds or 0))
end

function Streams:stopVideo()
    if not self.video then
        return
    end
    self.frames:disconnect()
    self.video = nil
    demo.stopVideo()
    self:set('video', {text = 'Start the video'})
end

function Streams:toggleTone()
    if self.tone then
        self:stopTone()
        return
    end
    self:act(function()
        local started, err = demo.startTone(440):await()
        if err then
            self.results:failure('tone', kToneName, err)
            return
        end
        self.tone = demo.audioStream()
        self.voice = self.tone:play({volume = 0.5})
        self.toneStarted = haylen.elapsed()
        self:set('tone', {text = 'Stop the tone'})
        self.results:set('tone', 'waiting', kToneName, string.format('%s synthesizes %s Hz as %s samples at %d Hz.', started.language, tostring(started.frequency), started.format, started.sampleRate))
    end)
end

function Streams:stopTone()
    if not self.tone then
        return
    end
    audio.stop(self.voice, 0.1)
    self.tone = nil
    self.level = nil
    demo.stopTone()
    self:set('tone', {text = 'Start the tone'})
end

-- The level is the root mean square of the newest samples, which a steady sine of amplitude 0.3 holds near 0.21.
function Streams:update(dt)
    sample.Test.update(self, dt)
    if not self.tone then
        return
    end
    self.count = self.tone:read(self.samples)
    local sum = 0
    for index = 1, self.count do
        sum = sum + self.samples[index] * self.samples[index]
    end
    self.level = self.count > 0 and math.sqrt(sum / self.count) or 0
    local playing = audio.active(self.voice)
    local state = haylen.elapsed() - self.toneStarted >= kToneSeconds and self.level > kLevelToPass and playing and 'pass' or 'waiting'
    self.results:set('tone', state, kToneName, string.format('The voice plays the stream resampled from %d Hz to %d Hz, the newest samples have a level of %.2f, and %d reads of the voice found the ring empty.', self.tone.sampleRate, audio.sampleRate(), self.level, self.tone.underruns))
end

function Streams:draw(area)
    if self.video then
        local texture = self.video.texture
        local width = math.min(640, area.width * 0.5)
        local height = width * texture.height / texture.width
        graphics2d.draw(texture, 24, area.height - height - 64, {width = width, height = height, pivotX = 0, pivotY = 0})
        sample.caption('The texture of the video stream.', 24, area.height - 52)
    end
    if self.level then
        local left = area.width * 0.55
        local width = area.width - left - 24
        local middle = area.height - 160
        local points = {}
        for index = 1, self.count, 4 do
            points[#points + 1] = {left + width * (index - 1) / self.count, middle - self.samples[index] * 200}
        end
        if #points >= 2 then
            graphics2d.drawPolyline(points, 2, sample.accent)
        end
        graphics2d.drawRect({left, area.height - 40, width * math.min(1, self.level * 3), 12}, sample.green)
        sample.caption(string.format('The newest samples of the tone, at a level of %.2f.', self.level), left, area.height - 72)
    end
end

return Streams
