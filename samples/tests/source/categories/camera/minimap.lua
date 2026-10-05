-- Minimap: a zoomed-out camera with a viewport in a corner draws the world again, and its visibility mask leaves the details out.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Minimap = haylen.class('Minimap', Test)

function Minimap:init(entry)
    Minimap.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.minimap = graphics2d.newCamera()
    self.minimap.zoom = {0.1, 0.1}
    self.details = false
end

function Minimap:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The frame on the minimap is what the main camera sees.',
        play = true,
        controls = {ui.toggle{id = 'details', text = 'Details on the minimap', onChange = function(event)
            self.details = event.checked
        end}},
    }
end

function Minimap:mask()
    return self.details and World.terrain | World.details or World.terrain
end

function Minimap:update(dt)
    Minimap.super.update(self, dt)
    if not self.stage then
        return
    end
    self.minimap.viewport = {self.stage:right() - 500, self.stage:bottom() - 310, 480, 290}
    self.walker:update(dt, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    self:status(string.format('Minimap zoom %.2f   Property "visibilityMask" is %d', self.minimap.zoom.x, self:mask()))
end

function Minimap:render()
    Minimap.super.render(self)
    if not self.stage then
        return
    end
    graphics2d.beginWorld(self.minimap, {order = 1, visibilityMask = self:mask()})
    graphics2d.drawRect(graphics2d.canvasBounds(), Test.stageColor, {layer = -1000})
    self.world:draw()
    local unit = graphics2d.canvasUnitSize()
    graphics2d.drawRectOutline(self.camera:visibleBounds(), 3 * unit, '#FFFFFFFF', {layer = 5})
    graphics2d.drawCircle(self.walker.x, self.walker.y, 10 * unit, '#FFFFD040', {layer = 6})
end

function Minimap:draw(area)
    self.world:draw()
    self.walker:draw()
end

function Minimap:renderUi()
    if self.stage then
        graphics2d.beginScreen()
        graphics2d.drawRectOutline(self.minimap.viewport, 6, '#FF101418')
    end
end

return Minimap
