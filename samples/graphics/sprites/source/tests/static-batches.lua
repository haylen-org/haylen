-- Static batches: a tile map and a row of trees baked once to the GPU, seen through a camera that pans, where the trees take an offset each frame to move faster for parallax, compared with drawing the same tiles as a sprite batch.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')

local StaticBatches = haylen.class('StaticBatches', sample.Test)

local kTile = 48
local kColumns, kRows = 70, 30
local kCode = [[
local ground = graphics2d.newSpriteBatch(tiles)  -- one add per tile of the map
local baked = ground:bake()  -- copies the sprites to the GPU once
graphics2d.drawStatic(baked)  -- draws every frame without uploading
graphics2d.drawStatic(trees, -camera.x * 0.4, 0, {layer = 1})  -- an offset moves the whole batch without rebaking]]

-- Picks the tile of a map cell from noise: water, sand, grass with flowers and tufts, and stone paths.
local function tileAt(noise, column, row)
    local value = noise:fractal(column * 0.08, row * 0.08, 3, 2, 0.5)
    if value < -0.25 then
        return 5
    elseif value < -0.15 then
        return 6
    elseif value > 0.35 then
        return 4
    end
    return (column * 7 + row * 13) % 5 == 0 and 2 or ((column + row) % 11 == 0 and 1 or 0)
end

function StaticBatches:enter()
    local tiles = sample.texture('images/tiles.png')
    local noise = m.noise(3)
    self.groundBatch = graphics2d.newSpriteBatch(tiles)
    self.groundBatch:reserve(kColumns * kRows)
    for row = 0, kRows - 1 do
        for column = 0, kColumns - 1 do
            local tile = tileAt(noise, column, row)
            self.groundBatch:add({x = column * kTile, y = row * kTile, width = kTile, height = kTile, pivotX = 0, pivotY = 0, source = {(tile % 4) * 32, (tile // 4) * 32, 32, 32}})
        end
    end
    self.ground = self.groundBatch:bake()

    local trees = graphics2d.newSpriteBatch(sample.texture('images/tree.png'))
    for index = 0, 40 do
        trees:add({x = index * 190 + (index * 37) % 90, y = kRows * kTile * 0.5 + (index * 53) % 400, pivotY = 1})
    end
    self.trees = trees:bake()
    self.useBatch = false
    self.time = 0
    self:frame({
        hint = 'Compare the uploaded bytes of the baked map with the same map drawn as a sprite batch every frame.',
        code = kCode,
        controls = {ui.toggle{id = 'batch', text = 'Draw as a sprite batch', onChange = function(event) self.useBatch = event.checked end}},
        focus = 'batch',
    })
end

function StaticBatches:update(dt)
    StaticBatches.super.update(self, dt)
    self.time = self.time + dt
    if self.area then
        local rangeX, rangeY = kColumns * kTile - self.area.width, kRows * kTile - self.area.height
        self.camera.position = {(math.sin(self.time * 0.2) * 0.5 + 0.5) * rangeX, (math.sin(self.time * 0.13) * 0.5 + 0.5) * rangeY}
    end
    local stats = graphics2d.stats()
    local bounds = self.ground:bounds()
    self:status(string.format('%d tiles in a %.0f x %.0f batch   %d trees   %d draw calls   %d bytes uploaded', self.ground:size(), bounds.width, bounds.height, self.trees:size(), stats.drawCalls, stats.uploadedBytes))
end

function StaticBatches:draw(area)
    if self.useBatch then
        self.groundBatch:draw()
    else
        graphics2d.drawStatic(self.ground)
    end
    graphics2d.drawStatic(self.trees, -self.camera.x * 0.4, 0, {layer = 1})
end

return StaticBatches
