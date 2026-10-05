-- The simulated camera and microphone of an SDK: the native part pushes the frames of the camera into the video stream "camera", whose texture the app draws, takes photos as JPEG bytes, which become a texture, and pushes the samples of the microphone into the audio stream "microphone" with its level. Swift captures them with AVFoundation, Kotlin with Camera2 and "AudioRecord" and JavaScript with "getUserMedia", after the permission of the person.
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local Test = require('harness.test')
local demo = require('native-demo')

local Camera = haylen.class('Camera', DemoTest)

Camera.framesToPass = 30
Camera.cameraName = 'The frames of the camera reach the texture'
Camera.photoName = 'A photo arrives as JPEG bytes'
Camera.microphoneName = 'The microphone fills its stream and reports its level'

function Camera:enter()
    self.samples = collections.newFloatBuffer(1024)
    self:frame{
        hint = 'Start the camera, take a photo and start the microphone, then speak.',
        focus = 'camera',
        controls = {
            ui.button{id = 'camera', text = 'Start the back camera', variant = 'primary', onClick = function() self:toggleCamera('back') end},
            ui.button{id = 'front', text = 'Start the front camera', onClick = function() self:toggleCamera('front') end},
            ui.button{id = 'photo', text = 'Take a photo', onClick = function() self:takePhoto() end},
            ui.button{id = 'microphone', text = 'Start the microphone', onClick = function() self:toggleMicrophone() end},
            ui.label{text = 'AVFoundation on iOS, iPadOS, Mac Catalyst and macOS, Camera2 and "AudioRecord" on Android, and "getUserMedia" on the web. The system asks the person for the camera and the microphone the first time.', color = 'textMuted', font = 'caption'},
        },
        code = "demo.startCamera('back'):await()\ngraphics2d.draw(demo.cameraStream().texture, x, y)\nlocal photo = demo.takePhoto():await()\nlocal texture = graphics.newTexture(photo.jpeg)\ndemo.startMicrophone():await()",
    }
end

function Camera:exit()
    self:stopCamera()
    self:stopMicrophone()
    Camera.super.exit(self)
end

function Camera:toggleCamera(facing)
    if self.cameraFeed then
        self:stopCamera()
        return
    end
    self:act(function()
        self.results:set('camera', 'waiting', Camera.cameraName, 'The camera starts.')
        local started, err = demo.startCamera(facing):await()
        if err then
            self.results:failure('camera', Camera.cameraName, err)
            return
        end
        self.cameraFeed = demo.cameraStream()
        self.cameraFrames = 0
        self.frames = self.cameraFeed:on('frame', function()
            self.cameraFrames = self.cameraFrames + 1
            local state = self.cameraFrames >= Camera.framesToPass and 'pass' or 'waiting'
            self.results:set('camera', state, Camera.cameraName, string.format('%d frames of the %s camera of %d by %d pixels reached the texture through %s.', self.cameraFrames, started.facing, self.cameraFeed.width, self.cameraFeed.height, started.language))
        end)
        self:set('camera', {text = 'Stop the camera'})
    end)
end

function Camera:stopCamera()
    if not self.cameraFeed then
        return
    end
    self.frames:disconnect()
    self.cameraFeed = nil
    demo.stopCamera()
    self:set('camera', {text = 'Start the back camera'})
end

function Camera:takePhoto()
    self:act(function()
        local photo, err = demo.takePhoto():await()
        if err and err.code == 'cameraOff' then
            self.results:set('photo', 'info', Camera.photoName, 'The camera is off. Start it before taking a photo.')
            return
        end
        if err then
            self.results:failure('photo', Camera.photoName, err)
            return
        end
        local decoded, texture = pcall(graphics.newTexture, photo.jpeg, {filter = 'linear'})
        if not decoded then
            self.results:set('photo', 'fail', Camera.photoName, 'The JPEG of the photo did not decode: ' .. tostring(texture))
            return
        end
        self.photo = texture
        local jpeg = photo.jpeg:sub(1, 2) == '\255\216'
        self.results:set('photo', jpeg and 'pass' or 'fail', Camera.photoName, string.format('%s returned %d bytes of JPEG, which decoded into a texture of %d by %d pixels.', photo.language, #photo.jpeg, texture.width, texture.height))
    end)
end

function Camera:toggleMicrophone()
    if self.microphoneFeed then
        self:stopMicrophone()
        return
    end
    self:act(function()
        self.results:set('microphone', 'waiting', Camera.microphoneName, 'The microphone starts.')
        local started, err = demo.startMicrophone():await()
        if err then
            self.results:failure('microphone', Camera.microphoneName, err)
            return
        end
        self.microphoneFeed = demo.microphoneStream()
        self.levels = 0
        self.levelConnection = demo.onMicrophoneLevel(function(level)
            self.levels = self.levels + 1
            self.level = level.level
            local state = self.levels >= 5 and 'pass' or 'waiting'
            self.results:set('microphone', state, Camera.microphoneName, string.format('%s records %d Hz, and %d level reports arrived, the last at %.3f.', started.language, started.sampleRate, self.levels, level.level))
        end)
        self:set('microphone', {text = 'Stop the microphone'})
    end)
end

function Camera:stopMicrophone()
    if not self.microphoneFeed then
        return
    end
    self.levelConnection:disconnect()
    self.microphoneFeed = nil
    self.level = nil
    demo.stopMicrophone()
    self:set('microphone', {text = 'Start the microphone'})
end

function Camera:update(dt)
    Camera.super.update(self, dt)
    if self.microphoneFeed then
        self.count = self.microphoneFeed:read(self.samples)
    end
end

function Camera:draw(area)
    Camera.super.draw(self, area)
    local width = math.min(480, area.width * 0.4)
    if self.cameraFeed then
        local texture = self.cameraFeed.texture
        graphics2d.draw(texture, 24, area.height - width * texture.height / texture.width - 64, {width = width, height = width * texture.height / texture.width, pivotX = 0, pivotY = 0})
        Test.caption('The texture of the camera stream.', 24, area.height - 52)
    end
    if self.photo then
        local left = width + 48
        graphics2d.draw(self.photo, left, area.height - width * self.photo.height / self.photo.width - 64, {width = width, height = width * self.photo.height / self.photo.width, pivotX = 0, pivotY = 0})
        Test.caption('The last photo.', left, area.height - 52)
    end
    if self.level then
        local left = area.width - 264
        graphics2d.drawRect({left, area.height - 40, 240, 12}, Test.line)
        graphics2d.drawRect({left, area.height - 40, 240 * math.min(1, self.level * 4), 12}, Test.green, {layer = 1})
        Test.caption(string.format('Microphone level %.3f, %d samples read.', self.level, self.count or 0), left, area.height - 72)
    end
end

return Camera
