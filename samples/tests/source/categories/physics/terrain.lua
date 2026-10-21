-- Destructible ground from `physics2d.newTerrain`: bombs blow craters and push crates, the shovel digs and the trowel adds dirt, and only the chunks that changed rebuild their collision.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Terrain = haylen.class('Terrain', PhysicsTest)

Terrain.code = [[
local terrain = physics2d.newTerrain(world, {columns = 361, rows = 161, cellSize = 5, x = -900, y = -380, chunkSize = 32})
terrain.samples = function(column, row) return row * 5 > surface(column) and 1 or 0 end
terrain:explode(x, y, 60, {impulse = 900})  -- Blows a crater and pushes the crates.
terrain:carve(x, y, 36)  terrain:fill(x, y, 36)
local rebuilt = terrain:update()  -- Only the chunks that changed.
for _, outline in ipairs(terrain:outlines()) do graphics2d.drawPolygon(outline, dirt) end]]

Terrain.cellSize = 5
Terrain.columns, Terrain.rows = 361, 161
Terrain.origin = {x = -900, y = -380}
Terrain.tools = {bomb = 60, dig = 36, fill = 36}
Terrain.dirt = '#FFA0582A'
Terrain.grass = '#FF6CBF3E'
Terrain.cave = '#FF4A2A1A'
Terrain.grassDepth = 16

function Terrain:enter()
    self:frame{
        code = Terrain.code,
        hint = 'Tap, click or hold on the ground with the selected tool. Bombs also push the crates away. R or the X button grows new hills.',
        controls = {
            ui.radioGroup{id = 'tool', items = {{id = 'bomb', text = 'Bomb'}, {id = 'dig', text = 'Dig with a shovel'}, {id = 'fill', text = 'Fill with a trowel'}}, selected = 'bomb', onChange = function(event) self.tool = event.value end},
            ui.button{id = 'crates', text = 'Drop crates', onClick = function() self:dropCrates() end},
            ui.button{id = 'bombs', text = 'Bomb at random', onClick = function() self:bombAtRandom() end},
            ui.button{id = 'reset', text = 'New hills', onClick = function() self:build() end},
        },
        focus = 'tool',
    }
    self.random = m.random(17)
    self.tool = 'bomb'
    self.rebuilt = 0
    self.blast = Burst('explosions/explosion-small', {count = 4, scale = 0.6, layer = 30})
    self.order = {layer = 0}
    self:build()
    timer.after(1, function() self:apply('bomb', -120, self:surfaceAt(-120)) end, {owner = self})
end

function Terrain:build()
    self.world = physics2d.newWorld()
    self.crates = {}
    self.terrain = physics2d.newTerrain(self.world, {columns = Terrain.columns, rows = Terrain.rows, cellSize = Terrain.cellSize, x = Terrain.origin.x, y = Terrain.origin.y, chunkSize = 32})
    local noise = m.noise(self.random:integer(1, 1000))
    self.surface = {}
    self.backfill = {}
    for column = 0, Terrain.columns - 1 do
        self.surface[column] = 260 + noise:fractal(column / 70, 0.3, 4) * 140
        self.backfill[#self.backfill + 1] = {Terrain.origin.x + column * Terrain.cellSize, Terrain.origin.y + self.surface[column] + 6}
    end
    local bottom = Terrain.origin.y + Terrain.rows * Terrain.cellSize + 200
    self.backfill[#self.backfill + 1] = {Terrain.origin.x + (Terrain.columns - 1) * Terrain.cellSize, bottom}
    self.backfill[#self.backfill + 1] = {Terrain.origin.x, bottom}
    self.terrain.samples = function(column, row)
        local cave = noise:fractal(column / 30, row / 30 + 5, 2) > 0.45 and row * Terrain.cellSize > self.surface[column] + 60
        return (row * Terrain.cellSize > self.surface[column] and not cave) and 1 or 0
    end
    self:rebuild()
    self:dropCrates()
end

-- Returns the height of the first ground below the top at `x`, from the outline the terrain was made with.
function Terrain:surfaceAt(x)
    local column = math.max(0, math.min(Terrain.columns - 1, math.floor((x - Terrain.origin.x) / Terrain.cellSize + 0.5)))
    return Terrain.origin.y + self.surface[column]
end

function Terrain:dropCrates()
    for index = 1, 6 do
        local crate = self.world:createBody({x = -600 + index * 170, y = -360})
        crate:addBox(48, 48)
        Look.dress(crate, 'woodCrate')
        self.crates[#self.crates + 1] = crate
    end
end

function Terrain:bombAtRandom()
    local x = self.random:range(-650, 650)
    self:apply('bomb', x, self:surfaceAt(x) + self.random:range(0, 60))
end

function Terrain:apply(tool, x, y)
    local radius = Terrain.tools[tool]
    if tool == 'bomb' then
        self.terrain:explode(x, y, radius, {impulse = 900})
        self.blast:fire(x, y)
    elseif tool == 'dig' then
        self.terrain:carve(x, y, radius)
    else
        self.terrain:fill(x, y, radius)
    end
end

-- Rebuilds the collision of the changed chunks and keeps the outlines that draw the ground, solid ones and holes apart.
function Terrain:rebuild()
    local rebuilt = self.terrain:update()
    self.rebuilt = self.rebuilt + rebuilt
    self.solids, self.holes, self.lawns = {}, {}, {}
    for _, outline in ipairs(self.terrain:outlines()) do
        local list = m.polygonSignedArea(outline) >= 0 and self.solids or self.holes
        list[#list + 1] = outline
        if list == self.solids then
            local lawn = {}
            for index, point in ipairs(outline) do
                lawn[index] = {point.x, point.y - Terrain.grassDepth}
            end
            self.lawns[#self.lawns + 1] = lawn
        end
    end
    return rebuilt
end

function Terrain:update(dt)
    Terrain.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local pointer = self.pointer
    if pointer.pressed or (pointer.down and self.tool ~= 'bomb') then
        self:apply(self.tool, pointer.worldX, pointer.worldY)
    end
    if self.terrain.dirtyChunkCount > 0 then
        self.lastRebuilt = self:rebuild()
    end
    self.blast:update(dt)
    self:status(string.format('The ground lies in %d chunks. The last change rebuilt %d of them, and %d in all.', self.terrain.chunkCount, self.lastRebuilt or 0, self.rebuilt))
end

function Terrain:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The grass is the ground drawn once more a little higher under the dirt, so it shows along every face that looks up above the hills the ground grew with, and the earth behind the ground covers it below them and darkens every cave, crater and tunnel.
function Terrain:draw(area)
    local order = self.order
    Scenery.sky(self.camera, 40)
    order.layer = -7
    for _, lawn in ipairs(self.lawns) do
        graphics2d.drawPolygon(lawn, Terrain.grass, order)
    end
    order.layer = -6
    graphics2d.drawPolygon(self.backfill, Terrain.cave, order)
    order.layer = -5
    for _, outline in ipairs(self.solids) do
        graphics2d.drawPolygon(outline, Terrain.dirt, order)
    end
    order.layer = -4
    for _, outline in ipairs(self.holes) do
        graphics2d.drawPolygon(outline, Terrain.cave, order)
    end
    Look.drawAll(self.crates, {layer = 3})
    self.blast:draw()
    local pointer = self.pointer
    local unit = Look.pixel
    graphics2d.drawRing(pointer.worldX, pointer.worldY, Terrain.tools[self.tool], 4 * unit, '#CC1E1B33', {layer = 40})
    graphics2d.drawRing(pointer.worldX, pointer.worldY, Terrain.tools[self.tool], 2 * unit, '#FFFFFFFF', {layer = 41})
end

return Terrain
