-- The native part paints an animated picture 30 times per second into a video stream, whose texture the UI shows and which stays current by itself, and synthesizes a tone into an audio stream, which the app plays as a voice and reads for a wave and a level meter. Swift pushes both from dispatch queues on Apple platforms, Kotlin from threads of its own on Android, C from threads of its library on the desktops and JavaScript from timers of the page on the web.
local audio = require('haylen.audio')
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local Test = require('harness.test')
local demo = require('native-demo')

local Streams = haylen.class('Streams', DemoTest)

Streams.framesToPass = 60
Streams.toneSeconds = 2
Streams.levelToPass = 0.1
Streams.videoName = 'The video stream reaches the texture'
Streams.toneName = 'The audio stream plays as a voice'
Streams.video = '@native-video'
Streams.wave = '@native-wave'
Streams.waveSize = {960, 200}

function Streams:enter()
    self.samples = collections.newFloatBuffer(1024)
    self.waveTarget = graphics.newRenderTarget(Streams.waveSize[1], Streams.waveSize[2], {filter = 'linear'})
    self.waveCamera = graphics2d.newCamera()
    self.waveCamera.anchor = 'topLeft'
    ui.setImage(Streams.wave, self.waveTarget)
    self:frame{
        hint = 'The video starts when the test opens. Start the tone to hear it, and stop either one.',
        focus = 'video',
        checksEmpty = 'Start the video or the tone to check them.',
        controls = {
            ui.button{id = 'video', text = 'Start the video', variant = 'primary', onClick = function() self:toggleVideo() end},
            ui.button{id = 'tone', text = 'Start the tone', onClick = function() self:toggleTone() end},
            ui.label{text = 'The native part pushes frames and samples from any thread. The engine keeps only the newest frame and uploads it once per frame at most, and the voice reads the ring of the tone, resampled to the mixer and silent where samples are missing.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12,
                ui.label{text = 'Video stream', font = 'heading'},
                ui.stack{grow = 1, align = 'stretch',
                    DemoTest.placeholder('videoWaiting', 'plugins/video.svg', 'The frames that the native part paints show here once the video starts.'),
                    ui.image{id = 'videoFrame', radius = 16, align = 'stretch', visible = false},
                },
            },
            ui.card{align = 'stretch', gap = 12,
                ui.row{gap = 12, ui.label{text = 'Audio stream', font = 'heading', grow = 1}, ui.badge{id = 'toneState', text = 'Silent', tone = 'neutral'}},
                ui.image{image = Streams.wave, fit = 'fill', height = 140, radius = 12, align = 'stretch'},
                ui.progress{id = 'level', value = 0, text = 'Level 0.00'},
            },
        },
        code = "demo.startVideo():await()\nui.setImage('@native-video', demo.videoStream().texture)\ndemo.startTone(440):await()\nlocal voice = demo.audioStream():play({volume = 0.5})\nlocal count = demo.audioStream():read(samples)",
    }
    self:toggleVideo()
end

function Streams:exit()
    self:stopVideo()
    self:stopTone()
    ui.removeImage(Streams.wave)
    Streams.super.exit(self)
end

function Streams:toggleVideo()
    if self.videoFeed then
        self:stopVideo()
        return
    end
    self:act(function()
        local started, err = demo.startVideo():await()
        if err then
            self.results:failure('video', Streams.videoName, err)
            return
        end
        self.videoFeed = demo.videoStream()
        self.videoInfo = started
        self.videoFrames = 0
        self.videoStarted = haylen.elapsed()
        self.frames = self.videoFeed:on('frame', function() self:videoFrame() end)
        ui.setImage(Streams.video, self.videoFeed.texture)
        self:set('videoFrame', {image = Streams.video, visible = true})
        self:set('videoWaiting', {visible = false})
        self:set('video', {text = 'Stop the video'})
        self.results:set('video', 'waiting', Streams.videoName, string.format('%s pushes %d by %d "%s" frames %d times per second from %s.', started.language, started.width, started.height, started.format, started.fps, started.thread))
    end)
end

function Streams:videoFrame()
    self.videoFrames = self.videoFrames + 1
    local seconds = haylen.elapsed() - self.videoStarted
    local sized = self.videoFeed.width == self.videoInfo.width and self.videoFeed.height == self.videoInfo.height
    local state = self.videoFrames >= Streams.framesToPass and sized and 'pass' or 'waiting'
    self.results:set('video', state, Streams.videoName, string.format('%d frames of %d by %d pixels reached the texture, %.1f per second, and it shows the newest one.', self.videoFrames, self.videoFeed.width, self.videoFeed.height, seconds > 0 and self.videoFrames / seconds or 0))
end

function Streams:stopVideo()
    if not self.videoFeed then
        return
    end
    self.frames:disconnect()
    self.videoFeed = nil
    demo.stopVideo()
    self:set('videoFrame', {visible = false})
    self:set('videoWaiting', {visible = true})
    self:set('videoWaiting-text', {text = 'The video stopped. Start it again to see its newest frames.'})
    self:set('video', {text = 'Start the video'})
    ui.removeImage(Streams.video)
end

function Streams:toggleTone()
    if self.tone then
        self:stopTone()
        return
    end
    self:act(function()
        local started, err = demo.startTone(440):await()
        if err then
            self.results:failure('tone', Streams.toneName, err)
            return
        end
        self.tone = demo.audioStream()
        self.voice = self.tone:play({volume = 0.5})
        self.toneStarted = haylen.elapsed()
        self:set('tone', {text = 'Stop the tone'})
        self:set('toneState', {text = string.format('%s Hz from %s', tostring(started.frequency), started.language), tone = 'success'})
        self.results:set('tone', 'waiting', Streams.toneName, string.format('%s synthesizes %s Hz as "%s" samples at %d Hz.', started.language, tostring(started.frequency), started.format, started.sampleRate))
    end)
end

function Streams:stopTone()
    if not self.tone then
        return
    end
    audio.stop(self.voice, 0.1)
    self.tone = nil
    self.level = nil
    self.count = 0
    demo.stopTone()
    self:set('tone', {text = 'Start the tone'})
    self:set('toneState', {text = 'Silent', tone = 'neutral'})
    self:set('level', {value = 0, text = 'Level 0.00'})
end

-- The level is the root mean square of the newest samples, which a steady sine of amplitude 0.3 holds near 0.21.
function Streams:update(dt)
    Streams.super.update(self, dt)
    if not self.tone then
        return
    end
    self.count = self.tone:read(self.samples)
    local sum = 0
    for index = 1, self.count do
        sum = sum + self.samples[index] * self.samples[index]
    end
    self.level = self.count > 0 and math.sqrt(sum / self.count) or 0
    self:set('level', {value = math.min(1, self.level * 3), text = string.format('Level %.2f', self.level)})
    local playing = audio.active(self.voice)
    local state = haylen.elapsed() - self.toneStarted >= Streams.toneSeconds and self.level > Streams.levelToPass and playing and 'pass' or 'waiting'
    self.results:set('tone', state, Streams.toneName, string.format('The voice plays the stream resampled from %d Hz to %d Hz at a level of %.2f, and %d reads found the ring empty.', self.tone.sampleRate, audio.sampleRate(), self.level, self.tone.underruns))
end

-- Paints the newest samples of the tone into the target that the audio card shows, a flat line while it is silent.
function Streams:render()
    Streams.super.render(self)
    local width, height = Streams.waveSize[1], Streams.waveSize[2]
    graphics2d.beginTarget(self.waveTarget, self.waveCamera, {clear = ui.themeColor('window')})
    graphics2d.drawShape({24, height / 2 - 1, width - 48, 2}, {radius = 1, color = Test.line})
    local count = self.tone and self.count or 0
    if count < 2 then
        return
    end
    local points = {}
    local step = math.max(1, count // 240)
    for index = 1, count, step do
        points[#points + 1] = {24 + (width - 48) * (index - 1) / (count - 1), height / 2 - self.samples[index] * height * 1.2}
    end
    graphics2d.drawPolyline(points, 5, Test.accent)
end

return Streams
