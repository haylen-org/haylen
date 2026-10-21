-- The base of the camera tests: it preloads the art of the island while the transition covers the screen, releases it when the test unloads, and draws the world of the test sorted by the y it stands on, so the walkers pass behind trees and in front of them.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local World = require('categories.camera.world')

local CameraTest = haylen.class('CameraTest', Test)

assets.defineGroups({groups = {camera = {
    {path = 'tiled/tiles/terrain.png', options = World.textureOptions},
    {path = 'tiled/tiles/transitions.png', options = World.textureOptions},
    {path = 'tiled/tiles/props.png', options = World.textureOptions},
    {path = 'camera/props.png', options = World.textureOptions},
    {path = 'sprites/sheets/walker.png', options = World.textureOptions},
}}})

function CameraTest:load(context)
    context:preload('camera'):await()
end

function CameraTest:unload()
    assets.unloadGroup('camera')
end

-- Draws a label for people on a dark pill whose bottom middle sits at `x`, `y`, as large on the screen whatever the zoom of the canvas. The table `options` takes the `color` of the rim, the `size` of the text, the `layer`, and `anchorX`, the share of the width of the pill at `x`, from 0 for its left edge to 1 for its right edge.
function CameraTest.tag(text, x, y, options)
    options = options or {}
    local unit = graphics2d.canvasUnitSize()
    local size = (options.size or 26) * unit
    local width, height = graphics2d.measureText(nil, text, {size = size})
    local padX, padY = 16 * unit, 7 * unit
    local box = {x - (width + padX * 2) * (options.anchorX or 0.5), y - height - padY * 2, width + padX * 2, height + padY * 2}
    local layer = options.layer or 20
    graphics2d.drawShape(box, {radius = box[4] / 2, color = '#E6151A2A', borderWidth = 2 * unit, borderColor = options.color or Test.accent, layer = layer})
    graphics2d.drawText(nil, text, box[1] + box[3] / 2, y - padY, {size = size, color = Test.ink, anchor = {0.5, 1}, layer = layer + 1})
end

-- The main view draws the ground and the details of the world, and leaves out the map that only minimaps draw.
function CameraTest:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera, {sort = 'y', visibilityMask = World.view})
    graphics2d.drawRect(graphics2d.canvasBounds(), Test.stageColor, {layer = -1000})
    self:draw(self.area)
end

return CameraTest
