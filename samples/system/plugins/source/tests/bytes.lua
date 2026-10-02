-- Binary payloads: bytes cross to the native part and back as byte buffers next to the JSON of the call, never as text, and the native part draws an image and returns it as the bytes of a PNG file, which becomes a texture. Swift draws it with CoreGraphics, Kotlin with a Bitmap, JavaScript with a canvas and C with a PNG encoder of its own.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local demo = require('native-demo')
local sample = require('sample')

local Bytes = haylen.class('Bytes', sample.Test)

local kImageWidth = 384
local kImageHeight = 216
local kPngSignature = '\137PNG\r\n\26\n'

-- Every byte value, with the zeros and the bytes that are no UTF-8, which text could never carry.
local function everyByte()
    local values = {}
    for value = 0, 255 do
        values[#values + 1] = string.char(value)
    end
    return table.concat(values) .. string.rep('\0', 16)
end

function Bytes:enter()
    self:frame({
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The function "platform.bytes" marks a string that crosses as bytes, and every byte buffer of an answer arrives as a Lua string in its place. Swift reads and answers "Data", Kotlin "ByteArray", JavaScript "Uint8Array" and C arrays of "HaylenNativeBuffer".', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local echoed = demo.echoBytes(data):await()\nprint(echoed.data == data)\nlocal image = demo.generatedImage(384, 216):await()\nlocal texture = graphics.newTexture(image.png)"},
        },
    })
    self:run()
end

function Bytes:run()
    self:act(function()
        self.results:clear()
        self:echo()
        self:image()
    end)
end

function Bytes:echo()
    local name = 'The call "echoBytes" returns the bytes it received'
    local data = everyByte()
    self.results:set('echo', 'waiting', name, string.format('Sending %d bytes', #data))
    local echoed, err = demo.echoBytes(data):await()
    if err then
        self.results:failure('echo', name, err)
        return
    end
    local same = echoed.data == data and echoed.size == #data
    self.results:set('echo', same and 'pass' or 'fail', name, string.format('%s received %d bytes as a buffer and sent %d bytes back, %s, every value from 0 to 255 and zeros included.', echoed.language, echoed.size, #echoed.data, same and 'the same ones' or 'but other ones'))
end

function Bytes:image()
    local name = 'The call "generatedImage" returns a PNG drawn natively'
    self.results:set('image', 'waiting', name, 'Drawing the image')
    local image, err = demo.generatedImage(kImageWidth, kImageHeight):await()
    if err then
        self.results:failure('image', name, err)
        return
    end
    local decoded, texture = pcall(graphics.newTexture, image.png, {filter = 'linear'})
    if not decoded then
        self.results:set('image', 'fail', name, 'The PNG of the native part did not decode: ' .. tostring(texture))
        return
    end
    self.texture = texture
    local valid = image.png:sub(1, 8) == kPngSignature and texture.width == kImageWidth and texture.height == kImageHeight
    self.results:set('image', valid and 'pass' or 'fail', name, string.format('%s drew the image with %s and returned %d bytes of PNG, which "graphics.newTexture" decoded into the texture of %d by %d pixels below.', image.language, image.drawnWith, #image.png, texture.width, texture.height))
end

function Bytes:draw(area)
    if not self.texture then
        return
    end
    local width = math.min(kImageWidth * 1.5, area.width - 48)
    local height = width * kImageHeight / kImageWidth
    graphics2d.draw(self.texture, 24, area.height - height - 64, {width = width, height = height, pivotX = 0, pivotY = 0})
    sample.caption('The image of the native part, drawn from its PNG.', 24, area.height - 52)
end

return Bytes
