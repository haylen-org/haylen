-- Tile layers: an endless world of ground tiles kept by chunks in a `TileLayer`, where chunks near the view are generated from noise as the camera travels and far ones are emptied away, each chunk bakes into a static batch only when its cells change, and tapping paints stone paths.
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')

local SpriteTest = require('categories.sprites.sprite-test')
local StaticBatches = require('categories.sprites.static-batches')

local TileLayers = haylen.class('TileLayers', SpriteTest)

TileLayers.tile = 48
TileLayers.chunk = 16
TileLayers.speed = 420
TileLayers.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left'}},
}}

TileLayers.code = [[
local layer = graphics2d.newTileLayer(tiles, {cellWidth = 48, cellHeight = 48, chunkSize = 16})
layer:defineFrame(1, {source = {0, 0, 32, 32}, width = 48, height = 48})
layer:setCells(column, row, 16, ids)  -- A float buffer of ids, one call per chunk.
layer:draw()  -- Bakes changed chunks and draws the ones in view.]]

function TileLayers:enter()
    local size = TileLayers.tile
    self.layer = graphics2d.newTileLayer(SpriteTest.texture('images/tiles.png'), {cellWidth = size, cellHeight = size, chunkSize = TileLayers.chunk})
    for tile = 0, 7 do
        -- The frames draw the 32 pixel tiles at the cell size, so each one covers its cell.
        self.layer:defineFrame(tile + 1, {source = {(tile % 4) * 32, (tile // 4) * 32, 32, 32}, width = size, height = size})
    end
    self.noise = m.noise(3)
    self.ids = collections.newFloatBuffer(TileLayers.chunk * TileLayers.chunk)
    self.empty = collections.newFloatBuffer(TileLayers.chunk * TileLayers.chunk)
    self.chunks = {}
    self.generated, self.dropped, self.painted = 0, 0, 0
    self.time = 0
    self.camera.position = {0, 0}
    self:loadActions(TileLayers.actions)
    self:frame{
        code = TileLayers.code,
        hint = 'Move with the arrow keys, WASD or the left stick, or let the camera wander, and tap or click to paint stone paths.',
        play = true,
    }
end

-- Writes the cells of one chunk from noise with one call, or empties it, which removes the chunk.
function TileLayers:writeChunk(chunkX, chunkY, keep)
    local size = TileLayers.chunk
    if not keep then
        self.layer:setCells(chunkX * size, chunkY * size, size, self.empty)
        return
    end
    local ids, index = self.ids, 1
    for row = chunkY * size, chunkY * size + size - 1 do
        for column = chunkX * size, chunkX * size + size - 1 do
            ids[index] = StaticBatches.tileAt(self.noise, column, row) + 1
            index = index + 1
        end
    end
    self.layer:setCells(chunkX * size, chunkY * size, size, ids)
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
            self:writeChunk(chunk[1], chunk[2], false)
            self.dropped = self.dropped + 1
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
        local column, row = math.floor(worldX / TileLayers.tile), math.floor(worldY / TileLayers.tile)
        if self.layer:cell(column, row) ~= 0 and self.layer:cell(column, row) ~= 5 then
            self.layer:setCell(column, row, 5)
            self.painted = self.painted + 1
        end
    end

    local stats = graphics2d.stats()
    self:status(string.format('Chunks %d with %d batches, generated %d, dropped %d, painted %d   Draw calls %d   Sprites %d   Bytes uploaded %d', self.layer.chunkCount, self.layer.batchCount, self.generated, self.dropped, self.painted, stats.drawCalls, stats.sprites, stats.uploadedBytes))
end

function TileLayers:draw(area)
    self.layer:draw()
end

return TileLayers
