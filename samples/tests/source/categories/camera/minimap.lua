-- Minimap: a zoomed-out camera with a viewport in a corner draws the world again, and its visibility mask leaves the details out.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Minimap = haylen.class('Minimap', CameraTest)

Minimap.code = [[
minimap.zoom = {0.1, 0.1}
minimap.viewport = {1400, 750, 480, 280}  -- A corner of the screen.
graphics2d.beginWorld(minimap, {order = 1, visibilityMask = 4})
graphics2d.drawStatic(ground, 0, 0, {visibility = 1})  -- The painted ground is left out.
graphics2d.drawPoints(water, {radius = 75, color = '#FF2FB3C4', visibility = 4})  -- The flat map shows.
props:draw({layer = 1, visibility = 2})  -- The props show once the mask takes 2.]]

-- The minimap shows the whole island, so its viewport has the shape of the world.
Minimap.zoom = 0.1
Minimap.margin = 20
Minimap.route = {{-400, 230, 0.6}, {250, 500}, {1300, 570, 0.6}, {250, 500}, {-1000, 60, 0.6}}

function Minimap:init(entry)
    Minimap.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-1000, 60, {action = 'move', route = Minimap.route})
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera:snapTo(self.walker.x, self.walker.y)
    self.minimap = graphics2d.newCamera()
    self.minimap.zoom = {Minimap.zoom, Minimap.zoom}
    self.details = false
end

function Minimap:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The white frame on the map is what the main camera sees.',
        code = Minimap.code,
        play = true,
        controls = {ui.toggle{id = 'details', text = 'Show the props on the map', onChange = function(event)
            self.details = event.checked
        end}},
    }
end

function Minimap:mask()
    return self.details and World.map | World.details or World.map
end

function Minimap:update(dt)
    Minimap.super.update(self, dt)
    if not self.stage then
        return
    end
    local width, height = World.bounds.width * Minimap.zoom, World.bounds.height * Minimap.zoom
    self.minimap.viewport = {self.stage:right() - width - Minimap.margin, self.stage:bottom() - height - Minimap.margin, width, height}
    self.walker:update(dt, self.world, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    self:status(self.details and 'The mask of the map takes the bits of its flat colors and of the props.' or 'The mask of the map takes only the bit of its flat colors, so it leaves the painted ground and the props out.')
end

-- The main view draws a soft shadow under the corner of the screen the map covers.
function Minimap:draw(area)
    self.world:draw()
    self.walker:draw()
    local view = self.minimap.viewport
    local left, top = self.camera:screenToWorld(view.x, view.y)
    local right, bottom = self.camera:screenToWorld(view:right(), view:bottom())
    graphics2d.drawShape({left - 4, top + 4, right - left + 8, bottom - top + 8}, {radius = 18, color = '#90000000', softness = 24, layer = 50})
end

function Minimap:render()
    Minimap.super.render(self)
    if not self.stage then
        return
    end
    graphics2d.beginWorld(self.minimap, {order = 1, sort = 'y', visibilityMask = self:mask()})
    self.world:draw()
    local unit = graphics2d.canvasUnitSize()
    graphics2d.drawShape(self.camera:visibleBounds(), {radius = 6 * unit, color = '#18FFFFFF', borderWidth = 2.5 * unit, borderColor = '#FFFFFFFF', layer = 30, visibility = World.map})
    graphics2d.drawCircle(self.walker.x, self.walker.y, 7 * unit, '#FFFFD040', {layer = 31, visibility = World.map})
    graphics2d.drawRing(self.walker.x, self.walker.y, 7 * unit, 2 * unit, '#FF1E1B33', {layer = 31, visibility = World.map})
end

-- The frame of the map draws over every canvas of the frame.
function Minimap:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen({order = 2})
    local view = self.minimap.viewport
    graphics2d.drawShape({view.x - 4, view.y - 4, view.width + 8, view.height + 8}, {radius = 12, color = '#00000000', borderWidth = 6, borderColor = '#FF2C3147'})
    CameraTest.tag('Island map', view.x + view.width / 2, view.y - 10, {color = Test.line})
end

return Minimap
