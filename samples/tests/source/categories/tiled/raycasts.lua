-- Ray casts against a map without a physics world: `map:raycastTiles` walks the cells of the walls layer and `map:raycastObjects` hits the shapes of the obstacles layer, and the nearer hit stops the ray.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local profiler = require('haylen.debug')
local tiled = require('haylen.tiled')
local ui = require('haylen.ui')

local MapTest = require('categories.tiled.map-test')
local outlines = require('categories.tiled.outlines')

local Raycasts = haylen.class('Raycasts', MapTest)

Raycasts.code = [[
local tileHit = map:raycastTiles('walls', x1, y1, x2, y2, function(gid)
    return map:tileInfo(gid).type ~= 'water'  -- Rays pass over water.
end)
local objectHit = map:raycastObjects('obstacles', x1, y1, x2, y2)
if tileHit then print(tileHit.column, tileHit.row, tileHit.gid, tileHit.distance) end]]

local kFanRays = 48
local kLength = 1200

function Raycasts:enter()
    self.map = MapTest.loadMap('tiled/maps/raycasts.tmj')
    local bounds = self.map.pixelBounds
    self:frame{
        hint = 'The ray aims at the pointer, and tapping or clicking moves the eye. Tile hits show the cell and its tile, object hits the name of the object. R or the X button puts the eye back.',
        code = Raycasts.code,
        controls = {
            ui.radioGroup{id = 'mode', items = {{id = 'single', text = 'One ray'}, {id = 'fan', text = 'A fan of rays'}}, selected = 'single', onChange = function(event) self.mode = event.value end},
            ui.checkbox{id = 'water', text = 'Water stops rays', onChange = function(event) self.waterBlocks = event.checked end},
            ui.checkbox{id = 'fences', text = 'Fences stop rays', checked = true, onChange = function(event) self.fencesBlock = event.checked end},
        },
        view = {bounds.width + 64, bounds.height + 64},
        focus = 'mode',
    }
    self.mode, self.waterBlocks, self.fencesBlock = 'single', false, true
    self.rope = assets.texture('sprites/images/rope.png', {filter = 'linear', wrap = 'repeat'})
    self.eye = {bounds.width * 0.42, bounds.height * 0.55}
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

function Raycasts:update(dt)
    Raycasts.super.update(self, dt)
    if input.pressed('reset') then
        self.eye = {self.map.pixelBounds.width * 0.42, self.map.pixelBounds.height * 0.55}
    end
    if self.pointer.pressed then
        self.eye = {self.pointer.worldX, self.pointer.worldY}
    end
    local dx, dy = self.pointer.worldX - self.eye[1], self.pointer.worldY - self.eye[2]
    local aim = math.atan(dy, dx)
    self.rays = {}
    profiler.beginScope('tiledCasts')
    local count = self.mode == 'fan' and kFanRays or 1
    for index = 1, count do
        local angle = count == 1 and aim or aim + (index - 1) / count * math.pi * 2
        local x2, y2 = self.eye[1] + math.cos(angle) * kLength, self.eye[2] + math.sin(angle) * kLength
        local hit, source = self:cast(x2, y2)
        self.rays[index] = {x2 = x2, y2 = y2, hit = hit, source = source}
    end
    profiler.endScope()

    local first = self.rays[1]
    local summary = 'The ray hits nothing'
    if first.source == 'tile' then
        summary = string.format('The ray hits the tile %d of the class "%s" at the cell %d, %d', tiled.tileId(first.hit.gid), self.map:tileInfo(first.hit.gid).type, first.hit.column, first.hit.row)
    elseif first.source == 'object' then
        summary = string.format('The ray hits the object "%s" of the class "%s"', first.hit.name, first.hit.type)
    end
    self:status(string.format('%s, %.0f units away. %d rays took %.3f ms.', summary, first.hit and first.hit.distance or 0, count, self:timing('tiledCasts')))
end

-- A ray glows from the eye to where it stops, with a bright point and an arrow along the normal of a hit.
function Raycasts:drawRay(ray, single)
    local x1, y1 = self.eye[1], self.eye[2]
    local x2, y2 = ray.x2, ray.y2
    if ray.hit then
        x2, y2 = ray.hit.x, ray.hit.y
    end
    local width = single and 4 or 2.5
    graphics2d.drawStroke({x1, y1, x2, y2}, {width = width * 3.5, color = '#40FF8A84', cap = 'round', layer = 6})
    graphics2d.drawStroke({x1, y1, x2, y2}, {width = width, color = '#FFFFD0C8', cap = 'round', layer = 7})
    if not ray.hit then
        return
    end
    graphics2d.drawShape({x2 - 9, y2 - 9, 18, 18}, {kind = 'ellipse', color = '#80FF8A84', softness = 8, layer = 7})
    graphics2d.drawShape({x2 - 4, y2 - 4, 8, 8}, {kind = 'ellipse', color = '#FFFFFFFF', layer = 8})
    if single then
        local nx, ny = ray.hit.normalX, ray.hit.normalY
        local tipX, tipY = x2 + nx * 34, y2 + ny * 34
        graphics2d.drawStroke({x2, y2, tipX - nx * 8, tipY - ny * 8}, {width = 4, color = '#FF8FE3FF', cap = 'round', layer = 8})
        graphics2d.drawPolygon({{tipX, tipY}, {tipX - nx * 12 - ny * 7, tipY - ny * 12 + nx * 7}, {tipX - nx * 12 + ny * 7, tipY - ny * 12 - nx * 7}}, '#FF8FE3FF', {layer = 8})
    end
end

-- The eye looks toward the pointer.
function Raycasts:drawEye()
    local x, y = self.eye[1], self.eye[2]
    local dx, dy = self.pointer.worldX - x, self.pointer.worldY - y
    local length = math.max(1, math.sqrt(dx * dx + dy * dy))
    graphics2d.drawShape({x - 17, y - 12, 34, 24}, {kind = 'ellipse', color = '#FFF2F3FF', borderWidth = 3, borderColor = MapTest.ink, layer = 9})
    local px, py = x + dx / length * 6, y + dy / length * 4
    graphics2d.drawShape({px - 7, py - 7, 14, 14}, {kind = 'ellipse', color = '#FF2F86F6', borderWidth = 2, borderColor = MapTest.ink, layer = 10})
    graphics2d.drawShape({px - 2.5, py - 2.5, 5, 5}, {kind = 'ellipse', color = MapTest.ink, layer = 11})
end

function Raycasts:draw(area)
    local map = self.map
    map:draw(self.camera)
    MapTest.frameMap(map.pixelBounds)
    local unit = graphics2d.canvasUnitSize()
    for _, object in ipairs(self.objects) do
        local points, closed = outlines.of(map, object)
        if object.shape == 'polyline' then
            local flat = {}
            for _, point in ipairs(points) do
                flat[#flat + 1] = point[1]
                flat[#flat + 1] = point[2]
            end
            graphics2d.drawStroke(flat, {width = 12, texture = self.rope, textureMode = 'tile', join = 'round', cap = 'round', layer = 4})
        else
            graphics2d.drawPolyline(points, 2 * unit, '#99FFC93C', closed, {layer = 5})
        end
    end
    local single = self.mode == 'single'
    for _, ray in ipairs(self.rays) do
        self:drawRay(ray, single)
        if ray.source == 'tile' and single then
            local x, y = map:cellToWorld(ray.hit.column, ray.hit.row)
            graphics2d.drawShape({x, y, map.tileWidth, map.tileHeight}, {radius = 4, color = '#33FF8A84', borderWidth = 3 * unit, borderColor = '#FFFF8A84', layer = 6})
        end
    end
    self:drawEye()
end

return Raycasts
