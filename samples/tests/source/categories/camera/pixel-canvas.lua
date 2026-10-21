-- Pixel canvas: pixel art rendered at a low resolution and scaled up to the play area. Without smoothing the view moves in whole big pixels and steps, and with smoothing the image shifts by the part of a pixel the camera lies between, so it glides while every pixel of the art stays whole.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local PixelSnap = require('categories.camera.pixel-snap')
local Test = require('harness.test')

local PixelCanvas = haylen.class('PixelCanvas', Test)

PixelCanvas.code = [[
local pixels = graphics2d.newPixelCanvas(220, 196, {smooth = true})
pixels:begin(lowCamera, {clear = '#FF101418'})  -- In render, then draw the art in canvas pixels.
graphics2d.draw(island, 0, 0, {pivotX = 0, pivotY = 0})
pixels:draw(half.x, half.y, half.width, half.height)  -- In renderUi, scaled up to its half of the play area.]]

-- The longer side of each canvas holds this many art pixels, so its pixels are big and its view stays on the island whatever the shape of the play area.
PixelCanvas.pixels = 220
PixelCanvas.gap = 8

function PixelCanvas:init(entry)
    PixelCanvas.super.init(self, entry)
    self.island, self.knight = PixelSnap.art()
    self.lowCamera = graphics2d.newCamera()
    self.speed = 6
    self.travel = 0
    self.time = 0
end

function PixelCanvas:enter()
    self:frame{
        hint = 'Both halves show the same low-resolution canvas drifting slowly. The left one moves in whole pixels of the canvas, the right one shifts by the part of a pixel the camera lies between.',
        code = PixelCanvas.code,
        controls = {ui.formField{label = 'Speed in canvas pixels per second', ui.slider{id = 'speed', min = 0, max = 40, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
            self.speed = event.value
        end}}},
        focus = 'speed',
    }
end

-- Makes the two canvases again whenever the play area changes its shape, each with the shape of its half.
function PixelCanvas:resize(area)
    local halfWidth = (area.width - PixelCanvas.gap) / 2
    local scale = math.max(halfWidth, area.height) / PixelCanvas.pixels
    self.canvasWidth, self.canvasHeight = math.max(1, math.floor(halfWidth / scale + 0.5)), math.max(1, math.floor(area.height / scale + 0.5))
    self.canvases = {graphics2d.newPixelCanvas(self.canvasWidth, self.canvasHeight, {smooth = false}), graphics2d.newPixelCanvas(self.canvasWidth, self.canvasHeight, {smooth = true})}
end

function PixelCanvas:update(dt)
    PixelCanvas.super.update(self, dt)
    if not self.canvases then
        return
    end
    self.time = self.time + dt
    self.travel = self.travel + self.speed * dt
    local span = 200
    local phase = self.travel % (span * 2)
    self.lowCamera.position = {340 + (phase < span and phase or span * 2 - phase) - span / 2, 160 + math.sin(self.time * 0.5) * 12}
    local x, y = self.canvases[2]:fraction()
    self:status(string.format('Each canvas is %d by %d pixels. The camera is at %.2f, %.2f, and the smooth canvas shifts by %.2f, %.2f of a pixel.', self.canvasWidth, self.canvasHeight, self.lowCamera.x, self.lowCamera.y, x, y))
end

function PixelCanvas:render()
    if not self.canvases then
        return
    end
    for _, canvas in ipairs(self.canvases) do
        canvas:begin(self.lowCamera, {clear = '#FF101418'})
        PixelSnap.drawIsland(self.island, self.knight, self.time)
    end
end

function PixelCanvas:renderUi()
    if not self.canvases or not self.stage then
        return
    end
    graphics2d.beginScreen()
    local stage = self.stage
    graphics2d.drawRect(stage, Test.stageColor, {layer = -1})
    local width = (stage.width - PixelCanvas.gap) / 2
    for index, canvas in ipairs(self.canvases) do
        local x = stage.x + (index - 1) * (width + PixelCanvas.gap)
        canvas:draw(x, stage.y, width, stage.height)
        CameraTest.tag(index == 1 and 'Whole pixels' or 'Smooth', x + width / 2, stage.y + 70)
    end
end

return PixelCanvas
