-- Debug drawing: `camera:drawDebug` draws the view as the screen shows it, the limits, the box where the target moves freely and a cross on the target, here from a zoomed-out overview in the panel beside the view, so it never covers what the camera follows.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Debug = haylen.class('Debug', CameraTest)

Debug.code = [[
local overview = graphics2d.newCamera()
overview.zoom, overview.position = {0.1, 0.1}, {0, 0}
graphics2d.beginTarget(map, overview, {clear = '#FF2AA6BC', visibilityMask = 4})
camera:drawDebug({layer = 10, visibility = 4})  -- The view, the limits, the free box and the target of the main camera.
ui.setImage('@overview', map)  -- The panel shows the target of the overview.]]

-- The overview renders the island at a tenth of its size into a target that the panel shows.
Debug.zoom = 0.1
Debug.image = '@overview'
-- The height that gives the overview the shape of the island across the panel.
Debug.imageHeight = 236
-- The colors `camera:drawDebug` draws with, which the legend names.
Debug.legend = {{'View', '#FFFFFFFF'}, {'Limits', '#FFFF5A4A'}, {'Free box', '#FFFFD84A'}, {'Target', '#FF4AD8FF'}}
Debug.limits = m.rect(-1800, -1000, 3600, 2000)
Debug.route = {{-400, 230, 0.6}, {800, 640}, {1500, 560, 0.6}, {800, 640}, {-1000, 60, 0.6}, {-1700, 330, 0.6}}

function Debug:init(entry)
    Debug.super.init(self, entry)
    self.world = World()
    self.world:chart()
    self.walker = Walker(-1000, 60, {action = 'move', route = Debug.route})
    self.camera = graphics2d.newCamera()
    self.camera.limits = Debug.limits
    self.camera.positionSmoothing = true
    self.camera.dragHorizontal = true
    self.camera.dragVertical = true
    self.camera.dragMargins = {0.4, 0.3, 0.4, 0.3}
    self.camera.lookAheadTime = 0.2
    self.camera:snapTo(self.walker.x, self.walker.y)
    local bounds = World.bounds
    self.target = graphics.newRenderTarget(bounds.width * Debug.zoom, bounds.height * Debug.zoom, {filter = 'linear'})
    self.overview = graphics2d.newCamera()
    self.overview.zoom = {Debug.zoom, Debug.zoom}
    self.overview.position = {bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}
    self.onMain = false
end

function Debug:enter()
    ui.setImage(Debug.image, self.target)
    local legend = {}
    for index, entry in ipairs(Debug.legend) do
        legend[index] = ui.statusIndicator{text = entry[1], tone = 'accent', style = {colors = {accent = entry[2]}}}
    end
    self:loadActions(actions)
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or by holding the mouse button or a finger. The overview beside the view shows what the main camera does, in the colors of its legend.',
        code = Debug.code,
        play = true,
        controls = {
            ui.toggle{id = 'main', text = 'Draw on the main view too', onChange = function(event)
                self.onMain = event.checked
            end},
            ui.sectionTitle{text = 'Overview of the island'},
            ui.image{image = Debug.image, height = Debug.imageHeight, fit = 'contain', radius = 12, align = 'stretch'},
            ui.grid{columns = 2, gap = 8, children = legend},
        },
    }
end

function Debug:exit()
    Debug.super.exit(self)
    ui.removeImage(Debug.image)
end

function Debug:update(dt)
    Debug.super.update(self, dt)
    if not self.stage then
        return
    end
    self.walker:update(dt, self.world, self.camera)
    self.camera:follow(self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local view = self.camera:visibleBounds()
    self:status(string.format('The view shows %d by %d units of the island around %d, %d.', math.floor(view.width + 0.5), math.floor(view.height + 0.5), math.floor(self.camera.x + 0.5), math.floor(self.camera.y + 0.5)))
end

function Debug:draw(area)
    self.world:draw()
    self.walker:draw()
    if self.onMain then
        self.camera:drawDebug({layer = 10})
    end
end

-- The overview draws the map of the island with the land outside the limits shaded, the walker and the debug drawing of the main camera into its target.
function Debug:render()
    Debug.super.render(self)
    if not self.stage then
        return
    end
    graphics2d.beginTarget(self.target, self.overview, {clear = World.seaColor, sort = 'y', visibilityMask = World.map})
    self.world:draw()
    local unit = graphics2d.canvasUnitSize()
    local limits, outside = Debug.limits, World.bounds:expanded(World.sea)
    local shade, order = '#59101828', {layer = 9, visibility = World.map}
    graphics2d.drawRect({outside.x, outside.y, outside.width, limits.y - outside.y}, shade, order)
    graphics2d.drawRect({outside.x, limits:bottom(), outside.width, outside:bottom() - limits:bottom()}, shade, order)
    graphics2d.drawRect({outside.x, limits.y, limits.x - outside.x, limits.height}, shade, order)
    graphics2d.drawRect({limits:right(), limits.y, outside:right() - limits:right(), limits.height}, shade, order)
    graphics2d.drawCircle(self.walker.x, self.walker.y, 7 * unit, '#FFFFD040', {layer = 31, visibility = World.map})
    graphics2d.drawRing(self.walker.x, self.walker.y, 7 * unit, 2 * unit, '#FF1E1B33', {layer = 31, visibility = World.map})
    self.camera:drawDebug({layer = 10, visibility = World.map})
end

return Debug
