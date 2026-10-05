-- The chunks of the world around the camera. Every frame it wants the chunks the camera sees and a ring around them, builds the missing ones nearest first, a few at a time, and unloads the chunks that fell far behind. A build scatters the decorations on a worker thread with "scatterAsync", makes the sprite fields in a job of "haylen.jobs" and bakes them into two static batches, one for the tiles and one for the decorations, so a loaded chunk draws with two draw calls and uploads nothing.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local jobs = require('haylen.jobs')
local m = require('haylen.math')
local procedural = require('haylen.procedural2d')
local scene = require('haylen.scene')

local Terrain = require('terrain')

local Chunks = {}
Chunks.__index = Chunks

-- The world spans this many chunks on each side of the origin, 16,384 tiles across in all.
Chunks.extent = 128
Chunks.preload = 1
Chunks.keep = 3
Chunks.maximumBuilds = 8
Chunks.tileOrder = {layer = 0}
Chunks.decorationOrder = {layer = 1}

function Chunks.new(owner, texture, seed)
    local self = setmetatable({}, Chunks)
    self.owner = owner
    self.loaded = {}
    self.building = {}
    self.buildCount = 0
    self.loadedCount = 0
    self.bakedSprites = 0
    self.generation = 0
    self.waiting = 0
    local tiles = Terrain.chunkTiles * Terrain.chunkTiles
    self.tileBatch = graphics2d.newSpriteBatch(texture)
    self.tileBatch:resize(tiles, {width = Terrain.tileSize, height = Terrain.tileSize, source = {0, Terrain.tileInset, Terrain.tilePixels, Terrain.tilePixels}})
    self.tileBuffer = collections.newFloatBuffer(tiles * #Terrain.tileFields)
    self.decorationBatch = graphics2d.newSpriteBatch(texture)
    self.decorationTemplate = {pivotY = 0.92, source = {0, Terrain.decorationTop, Terrain.decorationPixels[1], Terrain.decorationPixels[2]}}
    self:reseed(seed)
    return self
end

local function key(chunkX, chunkY)
    return chunkX * 65536 + chunkY
end

-- Starts a new world: every chunk unloads, and the builds still running are dropped when they finish.
function Chunks:reseed(seed)
    self.terrain = Terrain.new(seed)
    self.generation = self.generation + 1
    self.loaded = {}
    self.building = {}
    self.loadedCount = 0
    self.bakedSprites = 0
end

-- The range of chunks that `bounds`, a rectangle of the world, touches, widened by `margin` chunks and kept inside the world.
function Chunks.range(bounds, margin)
    local size = Terrain.chunkSize
    local extent = Chunks.extent
    return math.max(-extent, math.floor(bounds.x / size) - margin),
        math.max(-extent, math.floor(bounds.y / size) - margin),
        math.min(extent - 1, math.floor((bounds.x + bounds.width) / size) + margin),
        math.min(extent - 1, math.floor((bounds.y + bounds.height) / size) + margin)
end

-- Unloads the chunks outside the kept range and starts the builds of the missing chunks nearest to the middle of the view.
function Chunks:stream(bounds)
    local left, top, right, bottom = Chunks.range(bounds, Chunks.keep)
    for chunkKey, chunk in pairs(self.loaded) do
        if chunk.x < left or chunk.x > right or chunk.y < top or chunk.y > bottom then
            self.loaded[chunkKey] = nil
            self.loadedCount = self.loadedCount - 1
            self.bakedSprites = self.bakedSprites - chunk.sprites
        end
    end

    left, top, right, bottom = Chunks.range(bounds, Chunks.preload)
    local centerX = (bounds.x + bounds.width / 2) / Terrain.chunkSize - 0.5
    local centerY = (bounds.y + bounds.height / 2) / Terrain.chunkSize - 0.5
    local missing = {}
    for chunkY = top, bottom do
        for chunkX = left, right do
            local chunkKey = key(chunkX, chunkY)
            if not self.loaded[chunkKey] and not self.building[chunkKey] then
                missing[#missing + 1] = {x = chunkX, y = chunkY, distance = (chunkX - centerX) ^ 2 + (chunkY - centerY) ^ 2}
            end
        end
    end
    self.waiting = #missing
    if #missing == 0 or self.buildCount >= Chunks.maximumBuilds then
        return
    end
    table.sort(missing, function(a, b) return a.distance < b.distance end)
    for index = 1, math.min(#missing, Chunks.maximumBuilds - self.buildCount) do
        self:build(missing[index].x, missing[index].y)
    end
end

-- Builds one chunk in a task the owner holds, so nothing of it runs once the owner is gone.
function Chunks:build(chunkX, chunkY)
    local chunkKey = key(chunkX, chunkY)
    local terrain, generation = self.terrain, self.generation
    self.building[chunkKey] = true
    self.buildCount = self.buildCount + 1
    scene.spawn(self.owner, function()
        local points, failure = procedural.scatterAsync(terrain:scatterOptions(chunkX, chunkY)):await()
        if not points then
            error(failure, 0)
        end
        local fields, problem = jobs.spawn(terrain.build, terrain, chunkX, chunkY, points):await()
        if not fields then
            error(problem, 0)
        end
        self.buildCount = self.buildCount - 1
        if generation == self.generation then
            self.building[chunkKey] = nil
            self:bake(chunkKey, chunkX, chunkY, fields)
        end
    end)
end

function Chunks:bake(chunkKey, chunkX, chunkY, fields)
    self.tileBuffer:set(1, fields.tiles)
    self.tileBatch:writeFields(self.tileBuffer, Terrain.tileFields)
    local chunk = {x = chunkX, y = chunkY, tiles = self.tileBatch:bake(), sprites = Terrain.chunkTiles * Terrain.chunkTiles}
    local decorations = #fields.decorations // #Terrain.decorationFields
    if decorations > 0 then
        local buffer = collections.newFloatBuffer(#fields.decorations)
        buffer:set(1, fields.decorations)
        self.decorationBatch:resize(decorations, self.decorationTemplate)
        self.decorationBatch:writeFields(buffer, Terrain.decorationFields)
        chunk.decorations = self.decorationBatch:bake()
        chunk.sprites = chunk.sprites + decorations
    end
    self.loaded[chunkKey] = chunk
    self.loadedCount = self.loadedCount + 1
    self.bakedSprites = self.bakedSprites + chunk.sprites
end

-- Draws the loaded chunks that `bounds` touches, the tiles first and the decorations above them row by row, and returns the tiles and decorations it drew. The chunks below the view draw too, since their trees reach up into it.
function Chunks:draw(bounds)
    local left, top, right, bottom = Chunks.range(bounds, 0)
    bottom = math.min(Chunks.extent - 1, math.floor((bounds.y + bounds.height + Terrain.decorationSize[2]) / Terrain.chunkSize))
    local tiles, decorations = 0, 0
    for chunkY = top, bottom do
        for chunkX = left, right do
            local chunk = self.loaded[key(chunkX, chunkY)]
            if chunk then
                graphics2d.drawStatic(chunk.tiles, 0, 0, Chunks.tileOrder)
                tiles = tiles + Terrain.chunkTiles * Terrain.chunkTiles
                if chunk.decorations then
                    graphics2d.drawStatic(chunk.decorations, 0, 0, Chunks.decorationOrder)
                    decorations = decorations + chunk.decorations:size()
                end
            end
        end
    end
    return tiles, decorations
end

-- Outlines the chunks around the view: the loaded ones faintly and the ones that build in amber, which shows the streaming at work once the view zooms out.
function Chunks:drawOutlines(bounds, time)
    local size = Terrain.chunkSize
    local thickness = 2 * graphics2d.canvasUnitSize()
    local left, top, right, bottom = Chunks.range(bounds, Chunks.keep)
    local pulse = 0.35 + 0.25 * math.sin(time * 6)
    for chunkY = top, bottom do
        for chunkX = left, right do
            local chunkKey = key(chunkX, chunkY)
            local rect = {chunkX * size, chunkY * size, size, size}
            if self.building[chunkKey] then
                graphics2d.drawRect(rect, m.color(1, 0.7, 0.2, pulse), {layer = 2})
                graphics2d.drawRectOutline(rect, thickness * 2, '#FFFFB43C', {layer = 3})
            elseif self.loaded[chunkKey] then
                graphics2d.drawRectOutline(rect, thickness, '#59FFFFFF', {layer = 3})
            end
        end
    end
end

return Chunks
