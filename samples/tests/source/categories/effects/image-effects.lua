-- Image effects on the CPU: a picture and a sprite changed by the effects of `haylen.graphics` images, applied to a copy on the task pool with `image:applyAsync` whenever a control changes, so the frame never waits for them.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')

local ImageEffects = haylen.class('ImageEffects', Test)

ImageEffects.code = [[
local picture = assets.load('interface/images/landscape.png', 'image')
local copy = picture:applyAsync({{effect = 'blur', radius = 4}, {effect = 'adjust', brightness = 0.1, contrast = 1.2, saturation = 1.3, hue = 0}, {effect = 'pixelate', size = 6}}):await()
local outlined = sprite:applyAsync({{effect = 'outline', color = '#FF1E2A44', thickness = 4}}):await()
texture = graphics.newTexture(copy, {filter = 'linear'})]]

ImageEffects.view = {1600, 860}
ImageEffects.sliders = {
    {'blur', 'Blur radius', 0, 16, 0, 1},
    {'sharpen', 'Sharpen', 0, 3, 0, 0.1},
    {'brightness', 'Brightness', -0.5, 0.5, 0, 0.05},
    {'contrast', 'Contrast', 0, 2, 1, 0.05},
    {'saturation', 'Saturation', 0, 2, 1, 0.05},
    {'hue', 'Hue turns', -0.5, 0.5, 0, 0.01},
    {'pixelate', 'Pixel size', 1, 24, 1, 1},
    {'outline', 'Outline of the sprite', 0, 12, 4, 1},
}

function ImageEffects:enter()
    self.values = {invert = false}
    local controls = {}
    for _, spec in ipairs(ImageEffects.sliders) do
        local key = spec[1]
        self.values[key] = spec[5]
        controls[#controls + 1] = ui.formField{label = spec[2], ui.slider{id = key, min = spec[3], max = spec[4], step = spec[6], value = spec[5], showValue = true, decimals = spec[6] < 1 and 2 or 0, onChange = function(event)
            self.values[key] = event.value
            self:apply()
        end}}
    end
    controls[#controls + 1] = ui.toggle{id = 'invert', text = 'Invert the colors', checked = false, onChange = function(event)
        self.values.invert = event.checked
        self:apply()
    end}
    self:frame{
        hint = 'Every control changes a copy of the picture and of the sprite on the task pool, so the frame keeps running while the effects work.',
        code = ImageEffects.code,
        controls = controls,
        view = ImageEffects.view,
        focus = 'blur',
    }
    self.picture = assets.load('interface/images/landscape.png', 'image')
    self.sprite = assets.load('sprites/images/hero.png', 'image')
    self.original = graphics.newTexture(self.picture, {filter = 'linear'})
    self.waiting = 0
    self:apply()
end

-- The newest request wins: a request that ends after a newer one started leaves its result unused.
function ImageEffects:apply()
    self.request = (self.request or 0) + 1
    local request = self.request
    local values = self.values
    local effects = {
        {effect = 'blur', radius = values.blur},
        {effect = 'sharpen', amount = values.sharpen},
        {effect = 'adjust', brightness = values.brightness, contrast = values.contrast, saturation = values.saturation, hue = values.hue},
        {effect = 'pixelate', size = values.pixelate},
    }
    if values.invert then
        effects[#effects + 1] = {effect = 'invert'}
    end
    self.waiting = self.waiting + 1
    self:spawn(function()
        local picture = self.picture:applyAsync(effects):await()
        local sprite = self.sprite:applyAsync({{effect = 'outline', color = '#FF1E2A44', thickness = values.outline}, {effect = 'outline', color = '#FFFFD54F', thickness = values.outline > 0 and 1 or 0}}):await()
        self.waiting = self.waiting - 1
        if request == self.request then
            self.result = graphics.newTexture(picture, {filter = 'linear'})
            self.outlined = graphics.newTexture(sprite, {filter = 'linear'})
        end
    end)
end

function ImageEffects:update(dt)
    ImageEffects.super.update(self, dt)
    self:status(string.format('Effects working %d   Request %d', self.waiting, self.request))
end

function ImageEffects:draw(area)
    local width, height = 620, 349
    ImageEffects.caption('The picture', -760, -400)
    graphics2d.draw(self.original, -760, -370, {width = width, height = height, pivotX = 0, pivotY = 0})
    ImageEffects.caption('Its copy with the effects', -100, -400)
    if self.result then
        graphics2d.draw(self.result, -100, -370, {width = width, height = height, pivotX = 0, pivotY = 0})
    end
    ImageEffects.caption('A sprite with outlines drawn into its image', -760, 30)
    if self.outlined then
        graphics2d.draw(self.outlined, -560, 240, {width = self.sprite.width * 2, height = self.sprite.height * 2})
    end
end

return ImageEffects
