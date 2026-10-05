-- Debug drawing: `camera:drawDebug` draws the view as the screen shows it, the limits, the box where the target moves freely and a cross on the target, here from a zoomed-out overview.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Debug = haylen.class('Debug', Test)

function Debug:init(entry)
    Debug.super.init(self, entry)
    self.world = World()
    self.walker = Walker(0, 0, '#FFFFD040', 'move')
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
end

function Debug:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The overview in the corner shows what the main camera does.',
        play = true,
        controls = {ui.toggle{id = 'main', text = 'Draw on the main view', onChange = function(event)
            self.onMain = event.checked
        end}},
    }
end

function Debug:update(dt)
    Debug.super.update(self, dt)
    if not self.stage then
        return
    end
    self.overview.viewport = {self.stage:right() - 540, self.stage:bottom() - 320, 520, 300}
    self.walker:update(dt, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local view = self.camera:visibleBounds()
    self:status(string.format('View %.0f by %.0f at %.0f, %.0f', view.width, view.height, self.camera.x, self.camera.y))
end

function Debug:draw(area)
    self.world:draw()
    self.walker:draw()
    if self.onMain then
        self.camera:drawDebug({layer = 10})
    end
end

function Debug:render()
    Debug.super.render(self)
    if not self.stage then
        return
    end
    graphics2d.beginWorld(self.overview, {order = 1})
    graphics2d.drawRect(graphics2d.canvasBounds(), Test.stageColor, {layer = -1000})
    self.world:draw()
    self.walker:draw()
    self.camera:drawDebug({layer = 10})
end

function Debug:renderUi()
    if self.stage then
        graphics2d.beginScreen()
        graphics2d.drawRectOutline(self.overview.viewport, 6, '#FF101418')
    end
end

return Debug
