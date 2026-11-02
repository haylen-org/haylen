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
local terrain = physics2d.newTerrain(world, {columns = 481, rows = 161, cellSize = 5, x = -1200, y = -380})
terrain.samples = function(column, row) return (row * 5 - surface(column)) / 5 + 0.5 end
terrain:explode(x, y, 60, {impulse = 900})  -- Blows a crater and pushes the crates.
terrain:carve(x, y, 36)  -- Digs, and terrain:fill adds dirt the same way.
local rebuilt = terrain:update()  -- Only the chunks that changed.
local outlines = terrain:outlines()  -- One shape, with open caves and no seams.]]

Terrain.cellSize = 5
Terrain.columns, Terrain.rows = 481, 161
Terrain.origin = {x = -1200, y = -380}
Terrain.tools = {bomb = 60, dig = 36, fill = 36}
-- Pockets dug into the ground when it grows, as `{x, y, radius}`, which show the earth behind it.
Terrain.caves = {{-560, 250, 46}, {90, 310, 58}, {600, 270, 40}}
-- The earth behind the ground is the same soil in the shade.
Terrain.shade = '#FF8C6E5E'
Terrain.white = '#FFFFFFFF'

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
    self.groundMask, self.grassMask = {layer = -6}, {layer = -4}
    self.drawGround = function()
        graphics2d.drawPolygon(self.ground, Terrain.white)
    end
    -- The grass shows where the ground is and above the surface the hills grew with, so its band never hangs into a crater.
    self.drawGrassMask = function()
        graphics2d.drawPolygon(self.ground, Terrain.white)
        graphics2d.drawPolygon(self.above, Terrain.white)
    end
    self:build()
    timer.after(1, function() self:apply('bomb', -120, self:surfaceAt(-120)) end, {owner = self})
end

-- Grows new hills whose samples fade over one cell at the surface, so the outline follows the hills between the samples, and digs the caves.
function Terrain:build()
    self.world = physics2d.newWorld({owner = self})
    self.crates = {}
    self.terrain = physics2d.newTerrain(self.world, {columns = Terrain.columns, rows = Terrain.rows, cellSize = Terrain.cellSize, x = Terrain.origin.x, y = Terrain.origin.y})
    local noise = m.noise(self.random:integer(1, 1000))
    local size = Terrain.cellSize
    local surface, backfill, above, top = {}, {}, {}, {}
    for column = 0, Terrain.columns - 1 do
        surface[column] = 260 + noise:fractal(column / 70, 0.3, 4) * 140
        local x, y = Terrain.origin.x + column * size, Terrain.origin.y + surface[column]
        backfill[#backfill + 1] = {x, y + 6}
        above[#above + 1] = {x, y + 2}
        top[#top + 1] = {x, y}
    end
    local left, right = Terrain.origin.x, Terrain.origin.x + (Terrain.columns - 1) * size
    local bottom = Terrain.origin.y + Terrain.rows * size + 200
    backfill[#backfill + 1] = {right, bottom}
    backfill[#backfill + 1] = {left, bottom}
    above[#above + 1] = {right, Terrain.origin.y - 1000}
    above[#above + 1] = {left, Terrain.origin.y - 1000}
    self.surface, self.backfill, self.above = surface, backfill, above
    self.layers = Scenery.newLayers(top)

    self.terrain.samples = function(column, row)
        return (row * size - surface[column]) / size + 0.5
    end
    for _, cave in ipairs(Terrain.caves) do
        self.terrain:carve(cave[1], cave[2], cave[3])
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

-- Rebuilds the collision of the changed chunks and keeps the outlines that draw the ground and the grass along its top.
function Terrain:rebuild()
    local rebuilt = self.terrain:update()
    self.rebuilt = self.rebuilt + rebuilt
    self.ground = self.terrain:outlines()
    self:growGrass()
    return rebuilt
end

-- Returns whether the edge from `a` to `b` of a solid outline is a top face of the ground: it faces up, open air lies above it and it is no deeper than the surface the hills grew with, so craters and caves show bare dirt.
function Terrain:isLawn(a, b)
    local dx, dy = b.x - a.x, b.y - a.y
    if dx <= 0 or dx < math.abs(dy) * 0.6 then
        return false
    end
    local x, y = (a.x + b.x) / 2, (a.y + b.y) / 2
    if y > self:surfaceAt(x) + 3 then
        return false
    end
    local column = math.floor((x - Terrain.origin.x) / Terrain.cellSize + 0.5)
    local row = math.floor((y - Terrain.origin.y) / Terrain.cellSize) - 1
    return row < 0 or self.terrain:sample(column, row) < 0.5
end

-- Finds the runs of top faces along the solid outlines and joins the runs that meet at the edges of chunks, so each stretch of lawn draws as one band of grass.
function Terrain:growGrass()
    local runs = {}
    for _, outline in ipairs(self.ground) do
        local count = #outline
        if count >= 3 and m.polygonSignedArea(outline) > 0 then
            local first = 1
            while first <= count and self:isLawn(outline[(first - 2) % count + 1], outline[first]) do
                first = first + 1
            end
            local run
            for step = 0, count - 1 do
                local index = (first - 1 + step) % count + 1
                local a, b = outline[index], outline[index % count + 1]
                if self:isLawn(a, b) then
                    run = run or {{a.x, a.y}}
                    run[#run + 1] = {b.x, b.y}
                elseif run then
                    runs[#runs + 1] = run
                    run = nil
                end
            end
            if run then
                runs[#runs + 1] = run
            end
        end
    end
    table.sort(runs, function(a, b) return a[1][1] < b[1][1] end)

    local joined = {}
    for _, run in ipairs(runs) do
        local last = joined[#joined]
        local tail = last and last[#last]
        if tail and math.abs(tail[1] - run[1][1]) < 0.5 and math.abs(tail[2] - run[1][2]) < 0.5 then
            for index = 2, #run do
                last[#last + 1] = run[index]
            end
        else
            joined[#joined + 1] = run
        end
    end
    self.lawns = {}
    for _, run in ipairs(joined) do
        if run[#run][1] - run[1][1] > 8 then
            self.lawns[#self.lawns + 1] = Scenery.newGrass(run)
        end
    end
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

-- The earth behind the ground shows through every cave, crater and tunnel. The ground is the soil with the layers of the hills along the surface they grew with, which craters cut, and the grass grows along every top face at or above that surface.
function Terrain:draw(area)
    Scenery.sky(self.camera, 40)
    Scenery.drawSurface(self.backfill, 'soil', Terrain.shade, -7)
    graphics2d.pushMask(self.drawGround, self.groundMask)
    Scenery.drawPattern('soil', nil, 0)
    Scenery.drawLayers(self.layers, 1)
    graphics2d.popMask()
    graphics2d.pushMask(self.drawGrassMask, self.grassMask)
    for _, lawn in ipairs(self.lawns) do
        Scenery.drawGrass(lawn, 0)
    end
    graphics2d.popMask()
    Look.drawAll(self.crates, {layer = 3})
    self.blast:draw()
    local pointer = self.pointer
    local unit = Look.pixel
    graphics2d.drawRing(pointer.worldX, pointer.worldY, Terrain.tools[self.tool], 4 * unit, '#CC1E1B33', {layer = 40})
    graphics2d.drawRing(pointer.worldX, pointer.worldY, Terrain.tools[self.tool], 2 * unit, '#FFFFFFFF', {layer = 41})
end

return Terrain
