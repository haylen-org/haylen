-- Debug drawing: `camera:drawDebug` draws the view as the screen shows it, the limits, the box where the target moves freely and a cross on the target, here from a zoomed-out overview.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Player = require('player')
local World = require('world')
local sample = require('sample')

local Debug = haylen.class('Debug', sample.Test)

Debug.hints = 'Walk with WASD, the left stick or by holding the mouse button or a finger. The overview in the corner shows what the main camera does.'

function Debug:init(entry)
    Debug.super.init(self, entry)
    self.world = World()
    self.player = Player(0, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.limits = {-1800, -1000, 3600, 2000}
    self.camera.positionSmoothing = true
    self.camera.dragHorizontal = true
    self.camera.dragVertical = true
    self.camera.dragMargins = {0.4, 0.3, 0.4, 0.3}
    self.camera.lookAheadTime = 0.2
    self.overview = graphics2d.newCamera()
    self.overview.zoom = {0.11, 0.11}
    self.onMain = false
    self:layout()
end

-- Keeps the overview in the corner of the safe area, which moves when the window is resized.
function Debug:layout()
    local safe = viewport.safeRect()
    self.overview.viewport = {safe:right() - 560, safe:bottom() - 380, 520, 300}
end

function Debug:controls()
    return {ui.toggle{text = 'Draw on the main view', onChange = function(event)
        self.onMain = event.checked
    end}}
end

function Debug:update(dt)
    self:layout()
    self.player:update(dt, self.camera)
    self.camera:follow(self.player.x, self.player.y, dt)
    self.camera:update(dt)
    local view = self.camera:visibleBounds()
    self:setStatus(string.format('View %.0f by %.0f at %.0f, %.0f', view.width, view.height, self.camera.x, self.camera.y))
end

function Debug:render()
    graphics2d.beginWorld(self.camera)
    self.world:draw()
    self.player:draw()
    if self.onMain then
        self.camera:drawDebug({layer = 10})
    end

    graphics2d.beginWorld(self.overview, {order = 1})
    self.world:draw()
    self.player:draw()
    self.camera:drawDebug({layer = 10})
end

function Debug:renderUi()
    graphics2d.beginScreen()
    graphics2d.drawRectOutline(self.overview.viewport, 6, '#FF101418')
end

return Debug
