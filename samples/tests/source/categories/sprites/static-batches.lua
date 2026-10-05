-- Static batches: a tile map and a row of trees baked once to the GPU, seen through a camera that pans, where the trees take an offset each frame to move faster for parallax, compared with drawing the same tiles as a sprite batch.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local StaticBatches = haylen.class('StaticBatches', SpriteTest)

StaticBatches.tile = 48
StaticBatches.columns = 70
StaticBatches.rows = 30

StaticBatches.code = [[
local ground = graphics2d.newSpriteBatch(tiles)  -- One add per tile of the map.
local baked = ground:bake()  -- Copies the sprites to the GPU once.
graphics2d.drawStatic(baked)  -- Draws every frame without uploading.
graphics2d.drawStatic(trees, -camera.x * 0.4, 0, {layer = 1})  -- An offset moves the whole batch without baking again.]]

-- Picks the tile of a map cell from noise: water, sand, grass with flowers and tufts, and stone paths.
function StaticBatches.tileAt(noise, column, row)
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
    local size, columns, rows = StaticBatches.tile, StaticBatches.columns, StaticBatches.rows
    local noise = m.noise(3)
    self.groundBatch = graphics2d.newSpriteBatch(SpriteTest.texture('images/tiles.png'))
    self.groundBatch:reserve(columns * rows)
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            local tile = StaticBatches.tileAt(noise, column, row)
            self.groundBatch:add({x = column * size, y = row * size, width = size, height = size, pivotX = 0, pivotY = 0, source = {(tile % 4) * 32, (tile // 4) * 32, 32, 32}})
        end
    end
    self.ground = self.groundBatch:bake()

    local trees = graphics2d.newSpriteBatch(SpriteTest.texture('images/tree.png'))
    for index = 0, 40 do
        trees:add({x = index * 190 + (index * 37) % 90, y = rows * size * 0.5 + (index * 53) % 400, pivotY = 1})
    end
    self.trees = trees:bake()
    self.useBatch = false
    self.time = 0
    self:frame{
        code = StaticBatches.code,
        hint = 'Compare the uploaded bytes of the baked map with the same map drawn as a sprite batch every frame.',
        controls = {ui.toggle{id = 'batch', text = 'Draw as a sprite batch', onChange = function(event) self.useBatch = event.checked end}},
        focus = 'batch',
    }
end

function StaticBatches:update(dt)
    StaticBatches.super.update(self, dt)
    self.time = self.time + dt
    if self.area then
        local rangeX = StaticBatches.columns * StaticBatches.tile - self.area.width
        local rangeY = StaticBatches.rows * StaticBatches.tile - self.area.height
        self.camera.position = {(math.sin(self.time * 0.2) * 0.5 + 0.5) * rangeX, (math.sin(self.time * 0.13) * 0.5 + 0.5) * rangeY}
    end
    local stats = graphics2d.stats()
    local bounds = self.ground:bounds()
    self:status(string.format('Tiles %d in a %.0f x %.0f batch   Trees %d   Draw calls %d   Bytes uploaded %d', self.ground:size(), bounds.width, bounds.height, self.trees:size(), stats.drawCalls, stats.uploadedBytes))
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
