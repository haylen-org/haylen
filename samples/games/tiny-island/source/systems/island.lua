-- The island map with its physics walls, walkable grid and the gameplay points placed in Tiled.
local assets = require('haylen.assets')
local mathx = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local physics2d = require('haylen.physics2d')
local tiled = require('haylen.tiled')

local config = require('config')

local island = {}
island.__index = island

local groundLayers = {'foam', 'ground', 'shadow', 'cliffs', 'plateau'}

function island.new()
    local self = setmetatable({}, island)
    self.map = tiled.newMapRenderer(assets.load('maps/island.tmj'))
    self.world = physics2d.newWorld({gravity = {0, 0}, pixelsPerMeter = 64})
    self.map:buildCollision(self.world)
    self.bounds = self.map.pixelBounds

    -- Cells under a collision rectangle are water, cliff or plateau, so paths go around them.
    self.grid = navigation2d.newGrid(self.map.width, self.map.height)
    for _, wall in ipairs(self.map:objects('collision')) do
        local left, top = self.map:worldToCell(wall.x, wall.y)
        local right, bottom = self.map:worldToCell(wall.x + wall.width - 1, wall.y + wall.height - 1)
        for row = top, bottom do
            for column = left, right do
                self.grid:setWalkable(column, row, false)
            end
        end
    end

    self.spawns = {}
    self.treeRegions = {}
    for _, object in ipairs(self.map:objects('gameplay')) do
        if object.type == 'campfire' then
            self.fire = {x = object.x, y = object.y}
        elseif object.type == 'player_start' then
            self.start = {x = object.x, y = object.y}
        elseif object.type == 'enemy_spawn' then
            self.spawns[#self.spawns + 1] = {x = object.x, y = object.y}
        elseif object.type == 'tree_region' then
            self.treeRegions[#self.treeRegions + 1] = {x = object.x, y = object.y, width = object.width, height = object.height}
        end
    end
    return self
end

function island:walkable(x, y)
    local column, row = self.map:worldToCell(x, y)
    return self.grid:contains(column, row) and self.grid:walkable(column, row)
end

-- Trees grow on walkable land inside the regions of the map, away from the fire so the first day already means a walk.
function island:treeSpots()
    local random = mathx.random(7)
    local clearance = config.trees.fireClearance
    local spots = {}
    for _, region in ipairs(self.treeRegions) do
        local points = mathx.poissonDisk({area = mathx.rect(region.x, region.y, region.width, region.height), minimumDistance = config.trees.spacing, random = random, accept = function(point)
            local dx, dy = point.x - self.fire.x, point.y - self.fire.y
            return self:walkable(point.x, point.y) and self:walkable(point.x, point.y + 40) and dx * dx + dy * dy > clearance * clearance
        end})
        for _, point in ipairs(points) do
            spots[#spots + 1] = {x = point.x, y = point.y, variant = random:integer(1, 4)}
        end
    end
    return spots
end

-- Returns the waypoints from one point to another as world positions, or nil when the goal cannot be reached.
function island:path(fromX, fromY, toX, toY)
    local startColumn, startRow = self.map:worldToCell(fromX, fromY)
    local goalColumn, goalRow = self.map:worldToCell(toX, toY)
    local cells = self.grid:findPath(startColumn, startRow, goalColumn, goalRow, {diagonal = true, smooth = true})
    if not cells then
        return nil
    end
    local points = {}
    for index = 2, #cells do
        local x, y = self.map:cellToWorld(cells[index].x, cells[index].y)
        points[#points + 1] = {x = x + config.tile / 2, y = y + config.tile / 2}
    end
    points[#points + 1] = {x = toX, y = toY}
    return points
end

function island:update(dt)
    self.map:update(dt)
end

function island:drawGround(camera)
    for index, name in ipairs(groundLayers) do
        self.map:drawLayer(name, camera, {layer = config.layer.ground, depth = index})
    end
    self.map:drawLayer('decorations', camera, {layer = config.layer.ground, depth = #groundLayers + 1})
end

function island:drawClouds(camera)
    self.map:drawLayer('clouds', camera, {layer = config.layer.clouds})
end

return island
