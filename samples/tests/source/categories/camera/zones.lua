-- Zones and rails: a camera rig gives the view the limits, zoom and offset of the zone the walker is in and eases between them, and with the rail on the view follows the walker along a line through the meadow.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local Walker = require('categories.camera.walker')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Zones = haylen.class('Zones', Test)

Zones.zones = {
    {name = 'meadow', area = {-1800, -1000, 2000, 2000}, color = '#FF7CB342'},
    {name = 'pond', area = {200, -1000, 900, 1000}, zoom = 1.6, color = '#FF29B6F6'},
    {name = 'ridge', area = {200, 0, 1600, 1000}, zoom = 0.7, offset = {0, -120}, color = '#FFFFB74D'},
    {name = 'shrine', area = {600, 300, 400, 300}, limits = {500, 200, 600, 500}, zoom = 1.2, priority = 1, color = '#FFE57373'},
}
Zones.rail = {-1600, 0, -400, -300, 600, 200, 1700, 0}

function Zones:init(entry)
    Zones.super.init(self, entry)
    self.world = World()
    self.walker = Walker(-1000, 0, '#FFFFD040', 'move')
    self.camera = graphics2d.newCamera()
    self.camera.positionSmoothing = true
    self.camera.limitSmoothing = true
    self.rig = graphics2d.newCameraRig({limits = {-1800, -1000, 3600, 2000}, blendTime = 0.6})
    for _, zone in ipairs(Zones.zones) do
        self.rig:addZone({name = zone.name, area = zone.area, limits = zone.limits, zoom = zone.zoom, offset = zone.offset, priority = zone.priority})
    end
end

function Zones:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Walk through the meadow with WASD, the arrows, the left stick or by holding the mouse button or a finger. Each colored area is a zone with limits, zoom and offset of its own, and the shrine wins where it overlaps the ridge.',
        play = true,
        controls = {
            ui.toggle{id = 'rail', text = 'Follow along the rail', onChange = function(event)
                self.rig:setRail(event.checked and Zones.rail or nil)
            end},
        },
    }
end

function Zones:update(dt)
    Zones.super.update(self, dt)
    self.walker:update(dt, self.camera)
    self.rig:update(self.camera, self.walker.x, self.walker.y, dt)
    self.camera:update(dt)
    local zoom = self.camera.zoom
    self:status(string.format('Zone %s   Zoom %.2f   Zones %d', self.rig.zone or 'none', zoom.x, self.rig.zoneCount))
end

function Zones:draw(area)
    self.world:draw()
    for _, zone in ipairs(Zones.zones) do
        local box = zone.area
        graphics2d.drawRectOutline(box, 8, zone.color, {layer = 9})
        graphics2d.drawText(nil, zone.name:sub(1, 1):upper() .. zone.name:sub(2), box[1] + 24, box[2] + 24, {size = 36, color = zone.color, outlineWidth = 3, layer = 9})
    end
    graphics2d.drawStroke(Zones.rail, {width = 10, join = 'round', cap = 'round', color = '#A0FFFFFF', dashes = {40, 24}, layer = 8})
    self.walker:draw()
end

return Zones
