-- The simulated camera and microphone of an SDK: the native part pushes the frames of the camera into the video stream "camera", whose texture the UI shows, takes photos as JPEG bytes, which become a texture, and pushes the samples of the microphone into the audio stream "microphone" with its level. Swift captures them with AVFoundation, Kotlin with Camera2 and "AudioRecord" and JavaScript with "getUserMedia", after the permission of the person.
local collections = require('haylen.collections')
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Camera = haylen.class('Camera', DemoTest)

Camera.framesToPass = 30
Camera.cameraName = 'The frames of the camera reach the texture'
Camera.photoName = 'A photo arrives as JPEG bytes'
Camera.microphoneName = 'The microphone fills its stream and reports its level'
Camera.viewfinder = '@native-camera'
Camera.snapshot = '@native-photo'

function Camera:enter()
    self.samples = collections.newFloatBuffer(1024)
    self:frame{
        hint = 'Start the camera, take a photo and start the microphone, then speak.',
        focus = 'camera',
        checksEmpty = 'Start the camera or the microphone to check them.',
        controls = {
            ui.button{id = 'camera', text = 'Start the back camera', variant = 'primary', onClick = function() self:toggleCamera('back') end},
            ui.button{id = 'front', text = 'Start the front camera', onClick = function() self:toggleCamera('front') end},
            ui.button{id = 'photo', text = 'Take a photo', enabled = false, onClick = function() self:takePhoto() end},
            ui.button{id = 'microphone', text = 'Start the microphone', onClick = function() self:toggleMicrophone() end},
            ui.label{text = 'AVFoundation on iOS, iPadOS, Mac Catalyst and macOS, Camera2 and "AudioRecord" on Android, and "getUserMedia" on the web. The system asks the person for the camera and the microphone the first time.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.row{grow = 1, gap = 16, align = 'stretch',
                ui.card{grow = 2, align = 'stretch', gap = 12,
                    ui.label{text = 'Viewfinder', font = 'heading'},
                    ui.stack{grow = 1, align = 'stretch',
                        DemoTest.placeholder('cameraOff', 'plugins/camera.svg', 'The camera is off.'),
                        ui.image{id = 'cameraFrame', radius = 16, align = 'stretch', visible = false},
                    },
                },
                ui.card{grow = 1, align = 'stretch', gap = 12,
                    ui.label{text = 'Last photo', font = 'heading'},
                    ui.stack{grow = 1, align = 'stretch',
                        DemoTest.placeholder('noPhoto', 'plugins/camera.svg', 'No photo yet.'),
                        ui.image{id = 'photoFrame', radius = 16, align = 'stretch', visible = false},
                    },
                },
            },
            ui.card{align = 'stretch', gap = 12,
                ui.row{gap = 16, ui.icon{id = 'microphoneIcon', image = 'plugins/microphone.svg', size = 48, color = 'textMuted'}, ui.label{text = 'Microphone', font = 'heading', grow = 1}, ui.badge{id = 'microphoneState', text = 'Off', tone = 'neutral'}},
                ui.progress{id = 'level', value = 0, tone = 'success', text = 'Level 0.000'},
            },
        },
        code = "demo.startCamera('back'):await()\nui.setImage('@native-camera', demo.cameraStream().texture)\nlocal photo = demo.takePhoto():await()\nlocal texture = graphics.newTexture(photo.jpeg)\ndemo.startMicrophone():await()",
    }
end

function Camera:exit()
    self:stopCamera()
    self:stopMicrophone()
    if self.photo then
        ui.removeImage(Camera.snapshot)
    end
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
        ui.setImage(Camera.viewfinder, self.cameraFeed.texture)
        self:set('cameraFrame', {image = Camera.viewfinder, visible = true})
        self:set('cameraOff', {visible = false})
        self:set('camera', {text = 'Stop the camera'})
        self:set('photo', {enabled = true})
    end)
end

function Camera:stopCamera()
    if not self.cameraFeed then
        return
    end
    self.frames:disconnect()
    self.cameraFeed = nil
    demo.stopCamera()
    self:set('cameraFrame', {visible = false})
    self:set('cameraOff', {visible = true})
    self:set('camera', {text = 'Start the back camera'})
    self:set('photo', {enabled = false})
    ui.removeImage(Camera.viewfinder)
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
        ui.setImage(Camera.snapshot, texture)
        self:set('photoFrame', {image = Camera.snapshot, visible = true})
        self:set('noPhoto', {visible = false})
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
            self:set('level', {value = math.min(1, level.level * 4), text = string.format('Level %.3f', level.level)})
            local state = self.levels >= 5 and 'pass' or 'waiting'
            self.results:set('microphone', state, Camera.microphoneName, string.format('%s records %d Hz, the app read %d samples of the stream in the last frame, and %d level reports arrived.', started.language, started.sampleRate, self.count or 0, self.levels))
        end)
        self:set('microphone', {text = 'Stop the microphone'})
        self:set('microphoneState', {text = string.format('Recording %d Hz', started.sampleRate), tone = 'success'})
        self:set('microphoneIcon', {color = 'successText'})
    end)
end

function Camera:stopMicrophone()
    if not self.microphoneFeed then
        return
    end
    self.levelConnection:disconnect()
    self.microphoneFeed = nil
    demo.stopMicrophone()
    self:set('microphone', {text = 'Start the microphone'})
    self:set('microphoneState', {text = 'Off', tone = 'neutral'})
    self:set('microphoneIcon', {color = 'textMuted'})
    self:set('level', {value = 0, text = 'Level 0.000'})
end

function Camera:update(dt)
    Camera.super.update(self, dt)
    if self.microphoneFeed then
        self.count = self.microphoneFeed:read(self.samples)
    end
end

return Camera
