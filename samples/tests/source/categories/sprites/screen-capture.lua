-- Screen capture: the button captures the next frame with everything the screen shows, the GPU copies it without the frame thread waiting, and the test shows the image, writes it as a PNG file to the private storage of the app and reads it back.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local ScreenCapture = haylen.class('ScreenCapture', SpriteTest)

ScreenCapture.path = 'tests/screen-capture.png'

ScreenCapture.code = [[
local shot = graphics.captureScreen():await()
local png = shot:encodePngAsync():await()
storage.writeTextAsync('tests/screen-capture.png', png):await()]]

function ScreenCapture:enter()
    self.time = 0
    self.message = 'Capture the screen to see it here.'
    self:frame{
        code = ScreenCapture.code,
        hint = 'Capture the screen, which comes back as an image a frame later, shown small on the right and saved as a PNG file.',
        controls = {ui.button{id = 'capture', text = 'Capture the screen', variant = 'primary', onClick = function() self:capture() end}},
        focus = 'capture',
    }
end

function ScreenCapture:capture()
    scene.spawn(self, function()
        local started = haylen.frameIndex()
        local shot = graphics.captureScreen():await()
        local frames = haylen.frameIndex() - started
        local png = shot:encodePngAsync():await()
        storage.writeTextAsync(ScreenCapture.path, png):await()
        local back = graphics.decodeImage(storage.readText(ScreenCapture.path))
        self.shot = graphics.newTexture(shot, {filter = 'linear'})
        self.message = string.format('Captured %d by %d pixels %d frames later, saved %d bytes and read back %d by %d', shot.width, shot.height, frames, #png, back.width, back.height)
    end)
end

function ScreenCapture:update(dt)
    ScreenCapture.super.update(self, dt)
    self.time = self.time + dt
    self:status(self.message)
end

function ScreenCapture:draw(area)
    -- Something that moves, so captures differ.
    for index = 0, 11 do
        local angle = self.time + index * math.pi / 6
        graphics2d.drawCircle(area.width * 0.3 + math.cos(angle) * 140, area.height * 0.5 + math.sin(angle) * 140, 24, index % 2 == 0 and '#FFFF7043' or '#FF29B6F6')
    end
    if self.shot then
        local width = area.width * 0.4
        local height = width * self.shot.height / self.shot.width
        graphics2d.drawRect({area.width * 0.55 - 4, area.height * 0.5 - height / 2 - 4, width + 8, height + 8}, '#FFFFFFFF', {layer = 1})
        graphics2d.draw(self.shot, area.width * 0.55, area.height * 0.5 - height / 2, {width = width, height = height, pivotX = 0, pivotY = 0, layer = 2})
    end
end

return ScreenCapture
