-- Untrusted images: the bytes of image files that arrive from outside the app decode on the task pool with the longest side the app accepts, and two damaged files, one whose header declares a huge image and one whose data inflates past its image, fail with their reasons before they take the memory of what they claim.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local UntrustedImages = haylen.class('UntrustedImages', SpriteTest)

UntrustedImages.sizes = {{id = '128', text = '128'}, {id = '256', text = '256'}, {id = '1024', text = '1024'}}
UntrustedImages.layouts = {
    landscape = {view = {1700, 800}, cards = {{-840, -380, 550, 760}, {-275, -380, 550, 760}, {290, -380, 550, 760}}},
    portrait = {view = {900, 1700}, cards = {{-430, -830, 860, 540}, {-430, -270, 860, 540}, {-430, 290, 860, 540}}},
}
UntrustedImages.titles = {'A picture as sent', 'A header that lies', 'Data that inflates'}

UntrustedImages.code = [[
local bytes = assets.bytes('sprites/images/hero.png')
local image, failure = graphics.decodeImageAsync(bytes, {maxSize = 256}):await()
print(image and image.width or failure, graphics.maxImageSize())]]

-- Returns the bytes of a PNG file with the width and height of its header replaced, which sit after the signature and the header of the first chunk.
function UntrustedImages.withSize(png, width, height)
    return png:sub(1, 16) .. string.pack('>I4>I4', width, height) .. png:sub(25)
end

function UntrustedImages:enter()
    self.layout = UntrustedImages.layouts.landscape
    self.maxSize = 256
    self.results = {}
    self.message = 'Decoding the three files.'
    self.picture = assets.bytes('sprites/images/hero.png')
    self.lying = UntrustedImages.withSize(self.picture, graphics.maxImageSize() + 1, 16000)
    self:frame{
        code = UntrustedImages.code,
        view = self.layout.view,
        hint = 'Pick the longest side the app accepts. The picture decodes only while it fits, and the damaged files fail at once with their reasons.',
        controls = {
            ui.formField{label = 'Longest side in pixels', ui.segmentedControl{id = 'size', items = UntrustedImages.sizes, selected = '256', onChange = function(event)
                self.maxSize = tonumber(event.value)
                self:decodeAll()
            end}},
        },
        focus = 'size',
    }
    scene.spawn(self, function()
        -- The pixels of a large transparent image inflate far past the one pixel that the header then declares.
        local large = graphics.newImage(1024, 1024):encodePngAsync():await()
        self.inflating = UntrustedImages.withSize(large, 1, 1)
        self:decodeAll()
    end)
end

function UntrustedImages:resize(area)
    self.layout = Panels.layout(UntrustedImages.layouts, area)
    self.view = self.layout.view
end

-- Decodes the three files on the task pool, each with the longest side that the control picks, and keeps the texture or the reason of each.
function UntrustedImages:decodeAll()
    local files = {self.picture, self.lying, self.inflating}
    local maxSize = self.maxSize
    for index, bytes in ipairs(files) do
        scene.spawn(self, function()
            local image, failure = graphics.decodeImageAsync(bytes, {maxSize = maxSize}):await()
            if maxSize ~= self.maxSize then
                return
            end
            self.results[index] = image and {texture = graphics.newTexture(image, {filter = 'linear'}), width = image.width, height = image.height} or {failure = tostring(failure)}
            self:report()
        end)
    end
end

function UntrustedImages:report()
    local picture = self.results[1]
    if not picture or not self.results[2] or not self.results[3] then
        return
    end
    local refused = (self.results[2].failure and 1 or 0) + (self.results[3].failure and 1 or 0)
    local first = picture.texture and string.format('Decoded the picture of %d by %d pixels within %d pixels on a side.', picture.width, picture.height, self.maxSize) or string.format('Refused the picture, which is larger than %d pixels on a side.', self.maxSize)
    self.message = string.format('%s Refused %d damaged %s before %s took memory.', first, refused, refused == 1 and 'file' or 'files', refused == 1 and 'it' or 'they')
end

function UntrustedImages:update(dt)
    UntrustedImages.super.update(self, dt)
    self:status(self.message)
end

function UntrustedImages:drawCard(rect, title, result)
    local inner = Panels.card(rect, title)
    local x, y = inner[1] + inner[3] / 2, inner[2] + inner[4] / 2
    if not result then
        Panels.caption('Decoding', x, y, {color = Test.muted})
        return
    end
    if result.failure then
        graphics2d.drawShape({inner[1], inner[2], inner[3], inner[4]}, {radius = 14, color = '#FF2A1E26', borderWidth = 2, borderColor = '#FF8C3B4A'})
        graphics2d.drawText(nil, result.failure, x, y, {size = 24, color = Test.ink, maxWidth = inner[3] - 40, align = 'center', anchor = {0.5, 0.5}, layer = 1})
        return
    end
    local side = math.min(inner[3], inner[4] - 50)
    graphics2d.draw(result.texture, x, inner[2] + side / 2, {width = side, height = side, layer = 1})
    Panels.caption(string.format('%d by %d pixels', result.width, result.height), x, inner[2] + side + 10)
end

function UntrustedImages:draw(area)
    for index, rect in ipairs(self.layout.cards) do
        self:drawCard(rect, UntrustedImages.titles[index], self.results[index])
    end
end

return UntrustedImages
