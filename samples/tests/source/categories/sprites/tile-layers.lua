-- Tile layers: an endless painted world kept by chunks in tile layers, one for the grass and one for each kind of edge over it. Chunks near the view are generated from noise as the camera travels and far ones are emptied away, each chunk bakes into static batches only when its cells change, and the pointer paints dirt paths whose edges follow the corners it paints.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')

local SpriteTest = require('categories.sprites.sprite-test')
local StaticBatches = require('categories.sprites.static-batches')
local Terrain = require('categories.sprites.terrain')

local TileLayers = haylen.class('TileLayers', SpriteTest)

TileLayers.tile = 64
TileLayers.chunk = 8
TileLayers.speed = 420
-- A chunk that was generated or baked again glows for this many seconds.
TileLayers.glow = 0.9
TileLayers.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
}}

TileLayers.code = [[
local grass = graphics2d.newTileLayer({terrain, transitions}, {cellWidth = 64, cellHeight = 64, chunkSize = 8})
grass:defineFrame(1, {texture = 1, source = {2, 2, 128, 128}, width = 64, height = 64})
grass:setCells(column, row, 8, ids)  -- A float buffer of ids, one call per chunk.
paths:setCell(column, row, id)  -- Bakes only the chunk of the cell again.
grass:draw({layer = 0})  -- Bakes changed chunks and draws the ones in view.]]

-- The frame of the grass tile of a cell, and of the corner tile of a kind of edge with a mask.
function TileLayers.grassFrame(tile)
    return tile + 1
end

function TileLayers.edgeFrame(kind, mask)
    return 2 + (kind - 1) * 15 + mask
end

function TileLayers:enter()
    local size = TileLayers.tile
    local terrain, transitions = Terrain.textures()
    self.layers = {}
    for kind = Terrain.grass, Terrain.water do
        local layer = graphics2d.newTileLayer({terrain, transitions}, {cellWidth = size, cellHeight = size, chunkSize = TileLayers.chunk})
        for tile = 0, 1 do
            layer:defineFrame(TileLayers.grassFrame(tile), {texture = 1, source = Terrain.source(tile, 0), width = size, height = size})
        end
        for edge = Terrain.sand, Terrain.water do
            for mask = 1, 15 do
                layer:defineFrame(TileLayers.edgeFrame(edge, mask), {texture = 2, source = Terrain.source(mask, Terrain.rows[edge]), width = size, height = size})
            end
        end
        self.layers[kind] = layer
    end
    self.lakes, self.roads = m.noise(3), m.noise(11)
    self.painted = {}
    local cells = TileLayers.chunk * TileLayers.chunk
    self.buffers = {}
    for kind = Terrain.grass, Terrain.water do
        self.buffers[kind] = collections.newFloatBuffer(cells)
    end
    self.empty = collections.newFloatBuffer(cells)
    self.chunks, self.glows = {}, {}
    self.generated, self.dropped, self.paintedCount = 0, 0, 0
    self.time = 0
    self.camera.position = {0, 0}
    self:loadActions(TileLayers.actions)
    self:frame{
        code = TileLayers.code,
        hint = 'Move with the arrow keys, WASD or the left stick, or let the camera wander, and hold the pointer on the ground to paint dirt paths. Each square is a chunk, which glows when it is generated or baked again.',
        play = true,
    }
end

-- Returns the kind of ground at a corner: a painted path, or the noise of the map.
function TileLayers:kindAt(column, row)
    local painted = self.painted[column .. ',' .. row]
    if painted then
        return painted
    end
    return StaticBatches.kindAt(self.lakes, self.roads, column, row)
end

-- Writes the tile of each layer for one cell into the buffers at an index, 0 where a layer has nothing.
function TileLayers:writeCell(column, row, index)
    local buffers = self.buffers
    for kind = Terrain.grass, Terrain.water do
        buffers[kind][index] = 0
    end
    buffers[Terrain.grass][index] = TileLayers.grassFrame(Terrain.grassAt(column, row))
    local topLeft, topRight = self:kindAt(column, row), self:kindAt(column + 1, row)
    local bottomRight, bottomLeft = self:kindAt(column + 1, row + 1), self:kindAt(column, row + 1)
    for kind = Terrain.sand, Terrain.water do
        local mask = Terrain.mask(kind, topLeft, topRight, bottomRight, bottomLeft)
        if mask > 0 then
            buffers[kind][index] = TileLayers.edgeFrame(kind, mask)
        end
    end
end

-- Writes the cells of one chunk with one call per layer, or empties it, which removes the chunk.
function TileLayers:writeChunk(chunkX, chunkY, keep)
    local size = TileLayers.chunk
    local column, row = chunkX * size, chunkY * size
    if keep then
        local index = 1
        for y = row, row + size - 1 do
            for x = column, column + size - 1 do
                self:writeCell(x, y, index)
                index = index + 1
            end
        end
    end
    for kind = Terrain.grass, Terrain.water do
        self.layers[kind]:setCells(column, row, size, keep and self.buffers[kind] or self.empty)
    end
    if keep then
        self.glows[chunkX .. ',' .. chunkY] = self.time
    end
end

-- Keeps the chunks around the view generated and drops those two chunks past it, only when the view enters another chunk.
function TileLayers:stream()
    local span = TileLayers.tile * TileLayers.chunk
    local left, top = math.floor(self.camera.x / span) - 1, math.floor(self.camera.y / span) - 1
    local right, bottom = math.floor((self.camera.x + self.area.width) / span) + 1, math.floor((self.camera.y + self.area.height) / span) + 1
    local key = left .. ',' .. top .. ',' .. right .. ',' .. bottom
    if key == self.streamed then
        return
    end
    self.streamed = key
    for chunkY = top, bottom do
        for chunkX = left, right do
            local name = chunkX .. ',' .. chunkY
            if not self.chunks[name] then
                self.chunks[name] = {chunkX, chunkY}
                self:writeChunk(chunkX, chunkY, true)
                self.generated = self.generated + 1
            end
        end
    end
    for name, chunk in pairs(self.chunks) do
        if chunk[1] < left - 1 or chunk[1] > right + 1 or chunk[2] < top - 1 or chunk[2] > bottom + 1 then
            self.chunks[name] = nil
            self.glows[name] = nil
            self:writeChunk(chunk[1], chunk[2], false)
            self.dropped = self.dropped + 1
        end
    end
end

-- Paints the corner nearest a point of the world with dirt and writes again the four cells around it, whose chunks bake again.
function TileLayers:paint(x, y)
    local size = TileLayers.tile
    local column, row = math.floor(x / size + 0.5), math.floor(y / size + 0.5)
    local current = self:kindAt(column, row)
    if current == Terrain.path or current == Terrain.water then
        return
    end
    self.painted[column .. ',' .. row] = Terrain.path
    self.paintedCount = self.paintedCount + 1
    for cellY = row - 1, row do
        for cellX = column - 1, column do
            self:writeCell(cellX, cellY, 1)
            for kind = Terrain.grass, Terrain.water do
                self.layers[kind]:setCell(cellX, cellY, self.buffers[kind][1])
            end
            self.glows[math.floor(cellX / TileLayers.chunk) .. ',' .. math.floor(cellY / TileLayers.chunk)] = self.time
        end
    end
end

function TileLayers:update(dt)
    TileLayers.super.update(self, dt)
    if not self.area then
        return
    end
    self.time = self.time + dt
    local dx, dy = input.vector('move')
    if dx == 0 and dy == 0 then
        dx, dy = math.cos(self.time * 0.3), math.sin(self.time * 0.21)
    end
    self.camera.position = {self.camera.x + dx * TileLayers.speed * dt, self.camera.y + dy * TileLayers.speed * dt}
    self:stream()

    local worldX, worldY, _, held = self:pointer()
    if held then
        self:paint(worldX, worldY)
    end
    local layer = self.layers[Terrain.grass]
    self:status(string.format('%d chunks hold the world near the view: %d generated and %d dropped so far, with %d corners of paths painted.', layer.chunkCount, self.generated, self.dropped, self.paintedCount))
end

function TileLayers:draw(area)
    for kind = Terrain.grass, Terrain.water do
        self.layers[kind]:draw({layer = kind})
    end
    -- Every chunk shows its square, and one that was just generated or baked again glows.
    local span = TileLayers.tile * TileLayers.chunk
    local unit = graphics2d.canvasUnitSize()
    for name, chunk in pairs(self.chunks) do
        local box = {chunk[1] * span, chunk[2] * span, span, span}
        local glow = self.glows[name]
        local fade = glow and math.max(0, 1 - (self.time - glow) / TileLayers.glow) or 0
        graphics2d.drawShape(box, {radius = 12, color = m.color(1, 0.88, 0.54, 0.22 * fade), borderWidth = 3 * unit, borderColor = m.color(1, 1, 1, 0.35 + 0.5 * fade), layer = 10})
    end
end

return TileLayers
