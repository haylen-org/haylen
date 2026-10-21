-- Pixel snap: at a zoom of 4 a view that moves slowly lands between the big pixels and makes pixel art shimmer, and `pixelSnap` rounds the drawn position to whole view pixels.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')

local PixelSnap = haylen.class('PixelSnap', Test)

PixelSnap.code = [[
local island = assets.texture('camera/pixel-island.png')  -- Pixel art keeps the nearest filter.
camera.zoom = {4, 4}
camera.pixelSnap = true  -- Rounds the drawn position to whole view pixels.
graphics2d.draw(island, 0, 0, {pivotX = 0, pivotY = 0})
local drawn = camera:renderPosition()]]

-- The knights of the island, by the art pixel they stand on.
PixelSnap.knights = {{300, 150}, {336, 162}, {372, 146}, {262, 176}, {420, 128}}

-- Returns the island and the knight of pixel art, whose two frames of 16 pixels sit side by side.
function PixelSnap.art()
    return assets.texture('camera/pixel-island.png'), assets.texture('camera/knight.png')
end

-- Draws the island and its knights, who step in place, into the active canvas, all at whole art pixels.
function PixelSnap.drawIsland(island, knight, time)
    graphics2d.draw(island, 0, 0, {pivotX = 0, pivotY = 0})
    for index, spot in ipairs(PixelSnap.knights) do
        local frame = math.floor(time * 3 + index) % 2
        graphics2d.draw(knight, spot[1], spot[2], {source = {frame * 16, 0, 16, 16}, pivotY = 1, flipHorizontal = index % 2 == 0, layer = 1})
    end
end

function PixelSnap:init(entry)
    PixelSnap.super.init(self, entry)
    self.island, self.knight = PixelSnap.art()
    self.cameras = {}
    for index = 1, 2 do
        local camera = graphics2d.newCamera()
        camera.zoom = {4, 4}
        camera.pixelSnap = index == 2
        self.cameras[index] = camera
    end
    self.speed = 6
    self.travel = 0
    self.time = 0
end

function PixelSnap:enter()
    self:frame{
        hint = 'Both halves drift at the same slow speed. The left one draws at fractions of a pixel, so the pixel art shimmers, and the right one snaps to whole pixels.',
        code = PixelSnap.code,
        controls = {ui.formField{label = 'Speed in art pixels per second', ui.slider{id = 'speed', min = 0, max = 40, value = self.speed, showValue = true, decimals = 1, onChange = function(event)
            self.speed = event.value
        end}}},
        focus = 'speed',
    }
end

-- Splits the play area between the two cameras and moves both to the same point, back and forth over the island.
function PixelSnap:place(stage)
    local span = 160
    local phase = self.travel % (span * 2)
    local x, y = 330 + (phase < span and phase or span * 2 - phase) - span / 2, 150 + math.sin(self.time * 0.5) * 12
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
    self.travel = self.travel + self.speed * dt
    self:place(self.stage)
    local position, drawn = self.cameras[1].position, self.cameras[2]:renderPosition()
    self:status(string.format('The view is at %.2f, %.2f, and the snapped view draws at %.2f, %.2f.', position.x, position.y, drawn.x, drawn.y))
end

function PixelSnap:render()
    if not self.stage then
        return
    end
    for _, camera in ipairs(self.cameras) do
        graphics2d.beginWorld(camera)
        PixelSnap.drawIsland(self.island, self.knight, self.time)
    end
end

-- The divider and the names of the halves stay still over the views.
function PixelSnap:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen()
    for index, camera in ipairs(self.cameras) do
        local view = camera.viewport
        CameraTest.tag(index == 1 and 'Without snapping' or 'Snapped to whole pixels', view.x + view.width / 2, view.y + 70)
    end
    local split = self.cameras[2].viewport
    graphics2d.drawRect({split.x - 4, split.y, 8, split.height}, '#FF101418')
end

return PixelSnap
