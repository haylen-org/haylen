-- Pixel snap: at a zoom of 4 a view that moves slowly lands between the big pixels and makes pixel art shimmer, and pixelSnap rounds the drawn position to whole view pixels.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local PixelSnap = haylen.class('PixelSnap', sample.Test)

PixelSnap.hints = 'Both halves drift at the same slow speed. The left one draws at fractions of a pixel, the right one snaps to whole pixels.'

function PixelSnap:init(entry)
    PixelSnap.super.init(self, entry)
    local tiles = assets.texture('images/tiles.png')
    self.knight = assets.texture('images/knight.png')
    local random = m.random(4)
    local batch = graphics2d.newSpriteBatch(tiles)
    for row = -20, 20 do
        for column = -40, 40 do
            local kind = random:weightedIndex({8, 1, 1, 0.3})
            batch:add({x = column * 16, y = row * 16, pivotX = 0, pivotY = 0, source = {(kind - 1) * 16, 0, 16, 16}})
        end
    end
    self.ground = batch:bake()
    self.cameras = {}
    for index = 1, 2 do
        local camera = graphics2d.newCamera()
        camera.zoom = {4, 4}
        camera.pixelSnap = index == 2
        self.cameras[index] = camera
    end
    self.speed = 6
    self.time = 0
    self:place()
end

-- Splits the visible area between the two cameras and moves both to the same point.
function PixelSnap:place()
    local area = viewport.visibleRect()
    local x, y = self.time * self.speed, math.sin(self.time * 0.5) * 12
    for index, camera in ipairs(self.cameras) do
        camera.viewport = {area.x + (index - 1) * area.width / 2, area.y, area.width / 2, area.height}
        camera.position = {x, y}
    end
end

function PixelSnap:controls()
    return {ui.formField{label = 'Speed in world pixels per second', ui.slider{min = 0, max = 40, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
        self.speed = event.value
    end}}}
end

function PixelSnap:update(dt)
    self.time = self.time + dt
    self:place()
    local position, drawn = self.cameras[1].position, self.cameras[2]:renderPosition()
    self:setStatus(string.format('position %.3f, %.3f, snapped to %.3f, %.3f', position.x, position.y, drawn.x, drawn.y))
end

function PixelSnap:render()
    for _, camera in ipairs(self.cameras) do
        graphics2d.beginWorld(camera)
        graphics2d.drawStatic(self.ground)
        for index = 0, 6 do
            graphics2d.draw(self.knight, index * 40 - 100, (index % 2) * 30 - 10, {layer = 1})
        end
    end
end

function PixelSnap:renderUi()
    graphics2d.beginScreen()
    for index, camera in ipairs(self.cameras) do
        local view = camera.viewport
        graphics2d.drawText(nil, index == 1 and 'pixelSnap = false' or 'pixelSnap = true', view.x + view.width / 2, view:bottom() - 170, {size = 40, anchor = {0.5, 0}, outlineWidth = 4})
    end
    local split = self.cameras[2].viewport
    graphics2d.drawRect({split.x - 4, split.y, 8, split.height}, '#FF101418')
end

return PixelSnap
