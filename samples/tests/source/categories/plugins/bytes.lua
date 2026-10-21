-- Bytes cross to the native part and back as byte buffers next to the JSON of the call, never as text, and the native part paints a picture and returns it as the bytes of a PNG file, which becomes a texture that the UI shows. Swift paints it with CoreGraphics, Kotlin with a Bitmap, JavaScript with a canvas and C pixel by pixel with a PNG encoder of its own.
local graphics = require('haylen.graphics')
local haylen = require('haylen')
local ui = require('haylen.ui')

local DemoTest = require('categories.plugins.demo-test')
local demo = require('native-demo')

local Bytes = haylen.class('Bytes', DemoTest)

Bytes.imageWidth = 384
Bytes.imageHeight = 216
Bytes.pngSignature = '\137PNG\r\n\26\n'
Bytes.picture = '@native-picture'
Bytes.shown = 12

-- Every byte value, with the zeros and the bytes that are no UTF-8, which text could never carry.
local function everyByte()
    local values = {}
    for value = 0, 255 do
        values[#values + 1] = string.char(value)
    end
    return table.concat(values) .. string.rep('\0', 16)
end

-- The first and the last bytes of some data in hexadecimal, around the count of the bytes between them.
local function strip(data)
    local function hex(first, last)
        return (data:sub(first, last):gsub('.', function(byte) return string.format('%02X ', byte:byte()) end))
    end
    local half = Bytes.shown // 2
    return string.format('%s… %s', hex(1, half), hex(#data - half + 1, #data)):gsub(' $', '')
end

function Bytes:enter()
    self:frame{
        hint = 'Run the checks again to see that they hold.',
        focus = 'run',
        controls = {
            ui.button{id = 'run', text = 'Run the checks again', variant = 'primary', onClick = function() self:run() end},
            ui.label{text = 'The function "platform.bytes" marks a string that crosses as bytes, and every byte buffer of an answer arrives as a Lua string. Swift reads and answers "Data", Kotlin "ByteArray", JavaScript "Uint8Array" and C arrays of "HaylenNativeBuffer".', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{grow = 1, align = 'stretch', gap = 12,
                ui.label{text = 'Painted by the native part', font = 'heading'},
                ui.stack{grow = 1, align = 'stretch',
                    ui.label{id = 'picturePending', text = 'The picture shows here once its PNG bytes arrive.', color = 'textMuted', textAlign = 'center', align = 'center'},
                    ui.image{id = 'picture', radius = 16, align = 'stretch', visible = false},
                },
                ui.label{id = 'pictureInfo', text = '', font = 'caption', color = 'textMuted', textAlign = 'center', align = 'stretch'},
            },
            ui.card{align = 'stretch', gap = 8,
                ui.label{text = 'The bytes that crossed', font = 'heading'},
                ui.row{gap = 16, ui.label{text = 'Sent', width = 120, color = 'textMuted'}, ui.label{id = 'sent', text = 'Nothing yet', font = 'monospace', grow = 1}},
                ui.row{gap = 16, ui.label{text = 'Back', width = 120, color = 'textMuted'}, ui.label{id = 'received', text = 'Nothing yet', font = 'monospace', grow = 1}},
            },
        },
        code = "local echoed = demo.echoBytes(data):await()\nprint(echoed.data == data)\nlocal image = demo.generatedImage(384, 216):await()\nui.setImage('@native-picture', graphics.newTexture(image.png))",
    }
    self:run()
end

function Bytes:exit()
    if self.texture then
        ui.removeImage(Bytes.picture)
    end
    Bytes.super.exit(self)
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
    self.results:set('echo', 'waiting', name, string.format('Sending %d bytes.', #data))
    self:set('sent', {text = strip(data)})
    local echoed, err = demo.echoBytes(data):await()
    if err then
        self.results:failure('echo', name, err)
        return
    end
    local same = echoed.data == data and echoed.size == #data
    self:set('received', {text = strip(echoed.data)})
    self.results:set('echo', same and 'pass' or 'fail', name, string.format('%s received %d bytes as a buffer and sent %d bytes back, %s, from 0 to 255 with zeros.', echoed.language, echoed.size, #echoed.data, same and 'the same ones' or 'but other ones'))
end

function Bytes:image()
    local name = 'The call "generatedImage" returns a PNG painted natively'
    self.results:set('image', 'waiting', name, 'Painting the picture.')
    local image, err = demo.generatedImage(Bytes.imageWidth, Bytes.imageHeight):await()
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
    ui.setImage(Bytes.picture, texture)
    self:set('picture', {image = Bytes.picture, visible = true})
    self:set('picturePending', {visible = false})
    self:set('pictureInfo', {text = string.format('%d bytes of PNG from %s, decoded into %d by %d pixels.', #image.png, image.drawnWith, texture.width, texture.height)})
    local valid = image.png:sub(1, 8) == Bytes.pngSignature and texture.width == Bytes.imageWidth and texture.height == Bytes.imageHeight
    self.results:set('image', valid and 'pass' or 'fail', name, string.format('%s painted it with %s and returned %d bytes of PNG, which "graphics.newTexture" decoded.', image.language, image.drawnWith, #image.png))
end

return Bytes
