-- Ray casts against a map without a physics world: `map:raycastTiles` walks the cells of the walls layer and `map:raycastObjects` hits the shapes of the obstacles layer, and the nearer hit stops the ray.
local haylen = require('haylen')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local profiler = require('haylen.debug')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local outlines = require('outlines')
local sample = require('sample')

local Raycasts = haylen.class('Raycasts', sample.Test)

local kFanRays = 48
local kLength = 1200

function Raycasts:enter()
    self.map = tiled.newMapRenderer(assets.load('maps/raycasts.tmj'))
    local bounds = self.map.pixelBounds
    Raycasts.super.enter(self, {
        hint = 'The ray aims at the pointer, and tapping or clicking moves the eye. Tile hits show the cell and its tile, object hits the name of the object.',
        controls = {
            ui.radioGroup{id = 'mode', items = {{id = 'single', text = 'One ray'}, {id = 'fan', text = 'A fan of rays'}}, selected = 'single', onChange = function(event) self.mode = event.value end},
            ui.checkbox{id = 'water', text = 'Water stops rays', onChange = function(event) self.waterBlocks = event.checked end},
            ui.checkbox{id = 'fences', text = 'Fences stop rays', checked = true, onChange = function(event) self.fencesBlock = event.checked end},
        },
        stats = true,
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'mode',
    })
    self.mode, self.waterBlocks, self.fencesBlock = 'single', false, true
    self.eye = {bounds.width * 0.3, bounds.height * 0.55}
    self.objects = self.map:objects('obstacles')
    self.camera:snapTo(bounds.width / 2, bounds.height / 2)
end

-- Casts one ray over the tiles and the objects and returns the nearer hit with where it came from.
function Raycasts:cast(x2, y2)
    local map, x1, y1 = self.map, self.eye[1], self.eye[2]
    local tileHit = map:raycastTiles('walls', x1, y1, x2, y2, function(gid)
        local kind = map:tileInfo(gid).type
        return (kind ~= 'water' or self.waterBlocks) and (kind ~= 'fence' or self.fencesBlock)
    end)
    local objectHit = map:raycastObjects('obstacles', x1, y1, x2, y2)
    if tileHit and (objectHit == nil or tileHit.distance <= objectHit.distance) then
        return tileHit, 'tile'
    end
    return objectHit, objectHit and 'object'
end

function Raycasts:exit()
    Raycasts.super.exit(self)
    self.map, self.objects, self.rays = nil, nil, nil
end

function Raycasts:update(dt)
    Raycasts.super.update(self, dt)
    if input.pressed('reset') then
        self.eye = {self.map.pixelBounds.width * 0.3, self.map.pixelBounds.height * 0.55}
    end
    if self.pointer.pressed then
        self.eye = {self.pointer.worldX, self.pointer.worldY}
    end
    local dx, dy = self.pointer.worldX - self.eye[1], self.pointer.worldY - self.eye[2]
    local aim = math.atan(dy, dx)
    self.rays = {}
    profiler.beginScope('tiled casts')
    local count = self.mode == 'fan' and kFanRays or 1
    for index = 1, count do
        local angle = count == 1 and aim or aim + (index - 1) / count * math.pi * 2
        local x2, y2 = self.eye[1] + math.cos(angle) * kLength, self.eye[2] + math.sin(angle) * kLength
        local hit, source = self:cast(x2, y2)
        self.rays[index] = {x2 = x2, y2 = y2, hit = hit, source = source}
    end
    profiler.endScope()

    local first = self.rays[1]
    local summary = 'No hit'
    if first.source == 'tile' then
        summary = string.format('Tile %d at cell %d, %d\nClass %s', tiled.tileId(first.hit.gid), first.hit.column, first.hit.row, self.map:tileInfo(first.hit.gid).type)
    elseif first.source == 'object' then
        summary = string.format('Object "%s" of class "%s"', first.hit.name, first.hit.type)
    end
    self:showStats(string.format('%s\nDistance %.0f\n%d rays in %.3f ms', summary, first.hit and first.hit.distance or 0, count, self:timing('tiled casts')))
end

function Raycasts:render()
    self:beginWorld()
    local map = self.map
    map:draw(self.camera)
    local unit = graphics2d.canvasUnitSize()
    for _, object in ipairs(self.objects) do
        outlines.draw(map, object, '#AAFFD54F', 2 * unit, {layer = 5})
    end
    for _, ray in ipairs(self.rays) do
        physics2d.drawRay(self.eye[1], self.eye[2], ray.x2, ray.y2, ray.hit, {layer = 6})
        if ray.source == 'tile' and self.mode == 'single' then
            local x, y = map:cellToWorld(ray.hit.column, ray.hit.row)
            graphics2d.drawRectOutline({x, y, map.tileWidth, map.tileHeight}, 3 * unit, '#FFFF5252', {layer = 6})
        end
    end
    graphics2d.drawCircle(self.eye[1], self.eye[2], 10, '#FFFFFFFF', {layer = 7})
end

return Raycasts
