-- Pixel canvas: pixel art rendered at a low resolution and scaled up to the play area. Without smoothing the view moves in whole big pixels and steps, and with smoothing the image shifts by the part of a pixel the camera lies between, so it glides while every pixel of the art stays whole.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')

local PixelCanvas = haylen.class('PixelCanvas', Test)

PixelCanvas.width = 240
PixelCanvas.height = 135

function PixelCanvas:init(entry)
    PixelCanvas.super.init(self, entry)
    local tiles = assets.texture('camera/tiles.png')
    self.knight = assets.texture('camera/knight.png')
    local random = m.random(4)
    local batch = graphics2d.newSpriteBatch(tiles)
    for row = -20, 20 do
        for column = -40, 40 do
            local kind = random:weightedIndex({8, 1, 1, 0.3})
            batch:add({x = column * 16, y = row * 16, pivotX = 0, pivotY = 0, source = {(kind - 1) * 16, 0, 16, 16}})
        end
    end
    self.ground = batch:bake()
    self.canvases = {graphics2d.newPixelCanvas(PixelCanvas.width, PixelCanvas.height, {smooth = false}), graphics2d.newPixelCanvas(PixelCanvas.width, PixelCanvas.height, {smooth = true})}
    self.lowCamera = graphics2d.newCamera()
    self.speed = 6
    self.time = 0
end

function PixelCanvas:enter()
    self:frame{
        hint = 'Both halves show the same low-resolution canvas drifting slowly. The left one moves in whole pixels of the canvas, the right one shifts by the part of a pixel the camera lies between.',
        controls = {ui.formField{label = 'Speed in canvas pixels per second', ui.slider{id = 'speed', min = 0, max = 40, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
            self.speed = event.value
        end}}},
        focus = 'speed',
    }
end

function PixelCanvas:update(dt)
    PixelCanvas.super.update(self, dt)
    self.time = self.time + dt
    self.lowCamera.position = {self.time * self.speed, math.sin(self.time * 0.5) * 12}
    local x, y = self.canvases[2]:fraction()
    self:status(string.format('Canvas %d by %d   Camera %.3f, %.3f   Shifted by %.2f, %.2f pixels', PixelCanvas.width, PixelCanvas.height, self.lowCamera.x, self.lowCamera.y, x, y))
end

function PixelCanvas:render()
    if not self.stage then
        return
    end
    for _, canvas in ipairs(self.canvases) do
        canvas:begin(self.lowCamera, {clear = '#FF101418'})
        graphics2d.drawStatic(self.ground)
        for index = 0, 6 do
            graphics2d.draw(self.knight, index * 40 - 100, (index % 2) * 30 - 10, {layer = 1})
        end
    end
end

function PixelCanvas:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen()
    local stage = self.stage
    local width = stage.width / 2
    local height = math.min(stage.height, width * PixelCanvas.height / PixelCanvas.width)
    for index, canvas in ipairs(self.canvases) do
        local x = stage.x + (index - 1) * width
        local y = stage.y + (stage.height - height) / 2
        canvas:draw(x, y, width, height)
        local text = index == 1 and 'Whole pixels' or 'Smooth'
        graphics2d.drawText(nil, text, x + width / 2, y + height - 60, {size = 40, anchor = {0.5, 0}, outlineWidth = 4})
    end
    graphics2d.drawRect({stage.x + width - 4, stage.y, 8, stage.height}, '#FF101418')
end

return PixelCanvas
