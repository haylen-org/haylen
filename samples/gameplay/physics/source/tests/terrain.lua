-- Destructible ground from physics2d.newTerrain: bombs carve craters and push crates, the shovel digs and the trowel adds dirt, and only the chunks that changed rebuild their collision.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('parts')
local sample = require('sample')

local Terrain = haylen.class('Terrain', sample.Test)

local kCellSize = 5
local kColumns, kRows = 321, 141
local kOrigin = {x = -800, y = -300}
local kTools = {bomb = 60, dig = 36, fill = 36}
local kGround = '#FF6D4C41'
local kSky = '#FF1A2029'

function Terrain:enter()
    Terrain.super.enter(self, {
        hint = 'Tap, click or hold on the ground with the selected tool. Bombs also push the crates away. R or X grows new hills.',
        controls = {
            ui.radioGroup{id = 'tool', items = {{id = 'bomb', text = 'Bomb'}, {id = 'dig', text = 'Dig'}, {id = 'fill', text = 'Fill'}}, selected = 'bomb', onChange = function(event) self.tool = event.value end},
            ui.button{id = 'crates', text = 'Drop crates', onClick = function() self:dropCrates() end},
            ui.button{id = 'bombs', text = 'Bomb at random', onClick = function() self:apply('bomb', self.random:range(-700, 700), self.random:range(-100, 300)) end},
            ui.button{id = 'reset', text = 'New hills', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'tool',
    })
    self.random = m.random(17)
    self.tool = 'bomb'
    self.rebuilt = 0
    self:build()
end

function Terrain:build()
    self.world = physics2d.newWorld()
    self.crates = {}
    self.terrain = physics2d.newTerrain(self.world, {columns = kColumns, rows = kRows, cellSize = kCellSize, x = kOrigin.x, y = kOrigin.y, chunkSize = 32})
    local noise = m.noise(self.random:integer(1, 1000))
    self.terrain.samples = function(column, row)
        local surface = 180 + noise:fractal(column / 70, 0.3, 4) * 140
        local cave = noise:fractal(column / 30, row / 30 + 5, 2) > 0.45
        return (row * kCellSize > surface and not cave) and 1 or 0
    end
    self:rebuild()
    self:dropCrates()
end

function Terrain:dropCrates()
    for index = 1, 6 do
        local crate = self.world:createBody({x = -600 + index * 170, y = -380})
        parts.box(crate, 44, 44)
        self.crates[#self.crates + 1] = crate
    end
end

function Terrain:apply(tool, x, y)
    local radius = kTools[tool]
    if tool == 'bomb' then
        self.terrain:explode(x, y, radius, {impulse = 900})
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
    self.solids, self.holes = {}, {}
    for _, outline in ipairs(self.terrain:outlines()) do
        local list = m.polygonSignedArea(outline) >= 0 and self.solids or self.holes
        list[#list + 1] = outline
    end
    return rebuilt
end

function Terrain:exit()
    Terrain.super.exit(self)
    self.world, self.terrain, self.crates = nil, nil, nil
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
    self:showStats(string.format('chunks %d\nlast rebuild %d chunks\ntotal rebuilt %d\nstep %.2f ms', self.terrain.chunkCount, self.lastRebuilt or 0, self.rebuilt, sample.milliseconds('physics step')))
end

function Terrain:fixedUpdate(step)
    sample.step(self.world, step)
end

function Terrain:render()
    self:beginWorld()
    for _, outline in ipairs(self.solids) do
        graphics2d.drawPolygon(outline, kGround)
    end
    for _, outline in ipairs(self.holes) do
        graphics2d.drawPolygon(outline, kSky, {layer = 1})
    end
    parts.drawAll(self.crates, {layer = 3})
    local pointer = self.pointer
    graphics2d.drawRing(pointer.worldX, pointer.worldY, kTools[self.tool], 2, '#88FFFFFF', {layer = 4})
end

return Terrain
