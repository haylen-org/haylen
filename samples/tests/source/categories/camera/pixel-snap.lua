-- Pixel snap: at a zoom of 4 a view that moves slowly lands between the big pixels and makes pixel art shimmer, and `pixelSnap` rounds the drawn position to whole view pixels.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')

local PixelSnap = haylen.class('PixelSnap', Test)

function PixelSnap:init(entry)
    PixelSnap.super.init(self, entry)
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
    self.cameras = {}
    for index = 1, 2 do
        local camera = graphics2d.newCamera()
        camera.zoom = {4, 4}
        camera.pixelSnap = index == 2
        self.cameras[index] = camera
    end
    self.speed = 6
    self.time = 0
end

function PixelSnap:enter()
    self:frame{
        hint = 'Both halves drift at the same slow speed. The left one draws at fractions of a pixel, the right one snaps to whole pixels.',
        controls = {ui.formField{label = 'Speed in world pixels per second', ui.slider{id = 'speed', min = 0, max = 40, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
            self.speed = event.value
        end}}},
        focus = 'speed',
    }
end

-- Splits the play area between the two cameras and moves both to the same point.
function PixelSnap:place(stage)
    local x, y = self.time * self.speed, math.sin(self.time * 0.5) * 12
    for index, camera in ipairs(self.cameras) do
        camera.viewport = {stage.x + (index - 1) * stage.width / 2, stage.y, stage.width / 2, stage.height}
        camera.position = {x, y}
    end
end

function PixelSnap:update(dt)
    PixelSnap.super.update(self, dt)
    if not self.stage then
        return
    end
    self.time = self.time + dt
    self:place(self.stage)
    local position, drawn = self.cameras[1].position, self.cameras[2]:renderPosition()
    self:status(string.format('Position %.3f, %.3f   Snapped to %.3f, %.3f', position.x, position.y, drawn.x, drawn.y))
end

function PixelSnap:render()
    if not self.stage then
        return
    end
    for _, camera in ipairs(self.cameras) do
        graphics2d.beginWorld(camera)
        graphics2d.drawStatic(self.ground)
        for index = 0, 6 do
            graphics2d.draw(self.knight, index * 40 - 100, (index % 2) * 30 - 10, {layer = 1})
        end
    end
end

function PixelSnap:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen()
    for index, camera in ipairs(self.cameras) do
        local view = camera.viewport
        local text = index == 1 and 'With "pixelSnap = false"' or 'With "pixelSnap = true"'
        graphics2d.drawText(nil, text, view.x + view.width / 2, view:bottom() - 80, {size = 40, anchor = {0.5, 0}, outlineWidth = 4})
    end
    local split = self.cameras[2].viewport
    graphics2d.drawRect({split.x - 4, split.y, 8, split.height}, '#FF101418')
end

return PixelSnap
