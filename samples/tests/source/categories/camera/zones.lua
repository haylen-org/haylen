-- Zones and rails: a camera rig gives the view the limits, zoom and offset of the zone the walker is in and eases between them, and with the rail on the view follows the walker along a line through the island.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Zones = haylen.class('Zones', CameraTest)

Zones.code = [[
local rig = graphics2d.newCameraRig({limits = {-1800, -1000, 3600, 2000}, blendTime = 0.6})
rig:addZone({name = 'pond', area = {200, -1000, 900, 1000}, zoom = 1.4})
rig:addZone({name = 'shrine', area = {600, 300, 400, 300}, limits = {500, 160, 640, 560}, zoom = 1.3, priority = 1})
rig:setRail({-1600, 330, -400, 230, 800, 640, 1700, 420})
rig:update(camera, walker.x, walker.y, dt)]]

Zones.zones = {
    {name = 'meadow', title = 'Meadow', area = {-1800, -1000, 2000, 2000}, color = '#FF7CB342'},
    {name = 'pond', title = 'Pond', area = {200, -1000, 900, 1000}, zoom = 1.4, color = '#FF29B6F6'},
    {name = 'fields', title = 'Fields', area = {200, 0, 1600, 1000}, zoom = 0.75, offset = {0, -120}, color = '#FFFFB74D'},
    {name = 'shrine', title = 'Shrine', area = {600, 300, 400, 300}, limits = {500, 160, 640, 560}, zoom = 1.3, priority = 1, color = '#FFE57373'},
}
Zones.rail = {-1600, 330, -400, 230, 800, 640, 1700, 420}
-- Until a player steers, the walker tours every zone.
Zones.route = {{250, 500}, {820, 620, 1.2}, {1500, 560}, {1450, -60}, {1180, -180, 1.2}, {350, 60}, {-400, 230, 1.2}}

function Zones:init(entry)
    Zones.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-400, 230, {action = 'move', route = Zones.route})
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.limitSmoothing = true
    self.camera:snapTo(self.walker.x, self.walker.y)
    self.rig = graphics2d.newCameraRig({limits = {-1800, -1000, 3600, 2000}, blendTime = 0.6})
    for _, zone in ipairs(Zones.zones) do
        self.rig:addZone({name = zone.name, area = zone.area, limits = zone.limits, zoom = zone.zoom, offset = zone.offset, priority = zone.priority})
    end
    self.railPoints = Zones.dots(Zones.rail, 44)
    self.railed = false
end

-- Returns points every `step` units along a polyline of flat coordinates, which draw the rail as one call of round dots.
function Zones.dots(line, step)
    local points = {}
    for index = 1, #line - 3, 2 do
        local x1, y1, x2, y2 = line[index], line[index + 1], line[index + 2], line[index + 3]
        local length = math.sqrt((x2 - x1) ^ 2 + (y2 - y1) ^ 2)
        for distance = 0, length - 1, step do
            points[#points + 1] = x1 + (x2 - x1) * distance / length
            points[#points + 1] = y1 + (y2 - y1) * distance / length
        end
    end
    return points
end

function Zones:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk through the island with WASD, the arrows, the left stick or by holding the mouse button or a finger. Each colored area is a zone with limits, zoom and offset of its own, and the shrine wins where it overlaps the fields.',
        code = Zones.code,
        play = true,
        controls = {
            ui.toggle{id = 'rail', text = 'Follow along the rail', onChange = function(event)
                self.railed = event.checked
                self.rig:setRail(event.checked and Zones.rail or nil)
            end},
        },
    }
end

function Zones:update(dt)
    Zones.super.update(self, dt)
    self.walker:update(dt, self.world, self.camera)
    self.rig:update(self.camera, self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local zone
    for _, entry in ipairs(Zones.zones) do
        if entry.name == self.rig.zone then
            zone = entry
        end
    end
    local zoom = math.floor(self.camera.zoom.x * 100 + 0.5)
    if zone then
        self:status(string.format('The walker is in the %s zone, so the view eases to %d percent.', zone.title:lower(), zoom))
    else
        self:status(string.format('The walker is outside every zone, and the view keeps %d percent.', zoom))
    end
end

function Zones:draw(area)
    self.world:draw()
    self.walker:draw()
    local unit, view = graphics2d.canvasUnitSize(), graphics2d.canvasBounds()
    for _, zone in ipairs(Zones.zones) do
        local box, color = zone.area, m.color(zone.color)
        local active = zone.name == self.rig.zone
        graphics2d.drawShape(box, {radius = 24, color = color:withAlpha(active and 0.16 or 0.08), borderWidth = (active and 6 or 4) * unit, borderColor = color:withAlpha(active and 1 or 0.6), layer = 9})
        -- The name sits in the top left corner of the part of the zone in view, when that part is wide enough to hold it.
        local left, right = math.max(box[1], view.x), math.min(box[1] + box[3], view:right())
        if right - left > 240 * unit then
            local y = math.min(math.max(box[2], view.y) + 64 * unit, box[2] + box[4] - 20 * unit)
            CameraTest.tag(zone.title, left + 20 * unit, y, {color = zone.color, anchorX = 0})
        end
    end
    if self.railed then
        graphics2d.drawPoints(self.railPoints, {radius = 7 * unit, color = '#E0FFFFFF', layer = 8})
    end
end

return Zones
