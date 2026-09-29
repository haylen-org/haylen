-- Minimap: a zoomed-out camera with a viewport in a corner draws the world again, and its visibility mask leaves the details out.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Minimap = haylen.class('Minimap', sample.Test)

Minimap.hints = 'Walk with WASD, the left stick or by holding the mouse button or a finger. The frame on the minimap is what the main camera sees.'

function Minimap:init(entry)
    Minimap.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.minimap = graphics2d.newCamera()
    self.minimap.zoom = {0.1, 0.1}
    self.details = false
    self:layout()
end

-- Keeps the minimap in the corner of the safe area, which moves when the window is resized.
function Minimap:layout()
    local safe = viewport.safeRect()
    self.minimap.viewport = {safe:right() - 520, safe:bottom() - 370, 480, 290}
end

function Minimap:controls()
    return {ui.toggle{text = 'Details on the minimap', onChange = function(event)
        self.details = event.checked
    end}}
end

function Minimap:update(dt)
    self:layout()
    self.player:update(dt, self.camera)
    self.camera:follow(self.player.x, self.player.y, dt)
    self.camera:update(dt)
    self:setStatus(string.format('minimap zoom %.2f, visibilityMask %d', self.minimap.zoom.x, self.details and World.terrain | World.details or World.terrain))
end

function Minimap:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()

    graphics2d.beginWorld(self.minimap, {order = 1, visibilityMask = self.details and World.terrain | World.details or World.terrain})
    self.world:draw()
    local unit = graphics2d.canvasUnitSize()
    graphics2d.drawRectOutline(self.camera:visibleBounds(), 3 * unit, '#FFFFFFFF', {layer = 5})
    graphics2d.drawCircle(self.player.x, self.player.y, 10 * unit, '#FFFFD040', {layer = 6})
end

function Minimap:renderUi()
    graphics2d.beginScreen()
    graphics2d.drawRectOutline(self.minimap.viewport, 6, '#FF101418')
end

return Minimap
