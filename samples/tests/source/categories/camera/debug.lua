-- Debug drawing: `camera:drawDebug` draws the view as the screen shows it, the limits, the box where the target moves freely and a cross on the target, here from a zoomed-out overview.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Debug = haylen.class('Debug', CameraTest)

Debug.code = [[
local overview = graphics2d.newCamera()
overview.zoom = {0.1, 0.1}
overview.viewport = {1380, 760, 480, 280}  -- A corner of the screen.
graphics2d.beginWorld(overview, {order = 1})
camera:drawDebug({layer = 10})  -- The view, the limits, the free box and the target of the main camera.]]

Debug.zoom = 0.1
Debug.margin = 20
-- The colors `camera:drawDebug` draws with, which the legend names.
Debug.legend = {{'View', '#FFFFFFFF'}, {'Limits', '#FFFF5A4A'}, {'Free box', '#FFFFD84A'}, {'Target', '#FF4AD8FF'}}
Debug.route = {{-400, 230, 0.6}, {800, 640}, {1500, 560, 0.6}, {800, 640}, {-1000, 60, 0.6}, {-1700, 330, 0.6}}

function Debug:init(entry)
    Debug.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-1000, 60, {action = 'move', route = Debug.route})
    self.camera = graphics2d.newCamera()
    self.camera.limits = {-1800, -1000, 3600, 2000}
    self.camera.positionSmoothing = true
    self.camera.dragHorizontal = true
    self.camera.dragVertical = true
    self.camera.dragMargins = {0.4, 0.3, 0.4, 0.3}
    self.camera.lookAheadTime = 0.2
    self.camera:snapTo(self.walker.x, self.walker.y)
    self.overview = graphics2d.newCamera()
    self.overview.zoom = {Debug.zoom, Debug.zoom}
    self.onMain = false
end

function Debug:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The overview in the corner shows what the main camera does, in the colors of its legend.',
        code = Debug.code,
        play = true,
        controls = {ui.toggle{id = 'main', text = 'Draw on the main view too', onChange = function(event)
            self.onMain = event.checked
        end}},
    }
end

function Debug:update(dt)
    Debug.super.update(self, dt)
    if not self.stage then
        return
    end
    local width, height = World.bounds.width * Debug.zoom, World.bounds.height * Debug.zoom
    self.overview.viewport = {self.stage:right() - width - Debug.margin, self.stage:bottom() - height - Debug.margin, width, height}
    self.walker:update(dt, self.world, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local view = self.camera:visibleBounds()
    self:status(string.format('The view shows %d by %d units of the island around %d, %d.', math.floor(view.width + 0.5), math.floor(view.height + 0.5), math.floor(self.camera.x + 0.5), math.floor(self.camera.y + 0.5)))
end

-- The main view draws a soft shadow under the corner of the screen the overview covers.
function Debug:draw(area)
    self.world:draw()
    self.walker:draw()
    if self.onMain then
        self.camera:drawDebug({layer = 10})
    end
    local view = self.overview.viewport
    local left, top = self.camera:screenToWorld(view.x, view.y)
    local right, bottom = self.camera:screenToWorld(view:right(), view:bottom())
    graphics2d.drawShape({left - 4, top + 4, right - left + 8, bottom - top + 8}, {radius = 18, color = '#90000000', softness = 24, layer = 50})
end

-- The overview draws the map of the island, the walker and the debug drawing of the main camera.
function Debug:render()
    Debug.super.render(self)
    if not self.stage then
        return
    end
    graphics2d.beginWorld(self.overview, {order = 1, sort = 'y', visibilityMask = World.map})
    self.world:draw()
    local unit = graphics2d.canvasUnitSize()
    graphics2d.drawCircle(self.walker.x, self.walker.y, 7 * unit, '#FFFFD040', {layer = 31, visibility = World.map})
    graphics2d.drawRing(self.walker.x, self.walker.y, 7 * unit, 2 * unit, '#FF1E1B33', {layer = 31, visibility = World.map})
    self.camera:drawDebug({layer = 10, visibility = World.map})
end

-- The frame and the legend of the overview draw over every canvas of the frame.
function Debug:renderUi()
    if not self.stage then
        return
    end
    graphics2d.beginScreen({order = 2})
    local view = self.overview.viewport
    graphics2d.drawShape({view.x - 4, view.y - 4, view.width + 8, view.height + 8}, {radius = 12, color = '#00000000', borderWidth = 6, borderColor = '#FF2C3147'})
    local x = view.x
    for _, entry in ipairs(Debug.legend) do
        local width = graphics2d.measureText(nil, entry[1], {size = 22}) + 44
        graphics2d.drawShape({x, view.y - 52, width, 38}, {radius = 19, color = '#E6151A2A', borderWidth = 2, borderColor = Test.line})
        graphics2d.drawCircle(x + 19, view.y - 33, 7, entry[2])
        graphics2d.drawText(nil, entry[1], x + 33, view.y - 33, {size = 22, color = Test.ink, anchor = {0, 0.5}})
        x = x + width + 8
    end
end

return Debug
