-- The school of fish. Every fish swims after its own point on an ellipse around the bait, and the ellipses of the two halves of the school turn opposite ways, so the school swirls as a cross. A fish beats its tail faster the farther it is from its point. The simulation runs in plain Lua arrays, the simulation mode decides how many fish move each frame, and the hand-over mode decides how the sprites reach the renderer.
local collections = require('haylen.collections')
local debugging = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')

local Swarm = {}
Swarm.__index = Swarm

-- The fish strip holds three species of eight frames in one row, each frame a cell of 128 by 80 pixels.
Swarm.cellWidth = 128
Swarm.cellHeight = 80
Swarm.frames = 8
Swarm.species = 3
Swarm.size = {26, 16}
Swarm.fields = {'x', 'y', 'rotation', 'sourceX'}
Swarm.stride = #Swarm.fields
Swarm.follow = 2.4
Swarm.spin = 0.4
Swarm.beat = 6
Swarm.beatPerDistance = 0.05
Swarm.wide = {1.4, 0.7}
Swarm.tall = {0.75, 1.3}
Swarm.order = {layer = 2}

function Swarm.new(texture, random)
    local self = setmetatable({}, Swarm)
    self.texture = texture
    self.random = random
    self.simulation = 'full'
    self.handOverMode = 'buffer'
    self.count = 0
    self.angle = 0
    self.frame = 0
    -- The sprite fields of every fish in a row, x, y, rotation and the column of its frame, which is also where the simulation keeps the position.
    self.sprites = {}
    self.arms = {}
    self.phases = {}
    self.columns = {}
    self.tables = {}
    self.sources = {}
    for cell = 0, Swarm.frames * Swarm.species - 1 do
        self.sources[cell] = {cell * Swarm.cellWidth, 0, Swarm.cellWidth, Swarm.cellHeight}
    end
    self.template = {width = Swarm.size[1], height = Swarm.size[2], source = self.sources[0]}
    self.layout = {fields = Swarm.fields, width = Swarm.size[1], height = Swarm.size[2], source = self.sources[0]}
    self.batch = graphics2d.newSpriteBatch(texture)
    self.buffer = collections.newFloatBuffer(0)
    return self
end

-- Adds fish around the bait at `x`, `y`, each one at its own distance within `reach`.
function Swarm:add(amount, x, y, reach)
    local random = self.random
    local sprites, arms = self.sprites, self.arms
    for index = self.count + 1, self.count + amount do
        local angle = random:range(0, math.pi * 2)
        local radius = reach * math.sqrt(random:range(0.01, 1))
        local armX, armY = math.cos(angle) * radius, math.sin(angle) * radius
        local slot = index * Swarm.stride - Swarm.stride
        sprites[slot + 1], sprites[slot + 2] = x + armX * 1.6, y + armY * 1.6
        sprites[slot + 3], sprites[slot + 4] = 0, 0
        arms[index * 2 - 1], arms[index * 2] = armX, armY
        self.phases[index] = random:range(0, Swarm.frames)
        self.columns[index] = random:integer(0, Swarm.species - 1) * Swarm.frames * Swarm.cellWidth
        self.tables[index] = {x = 0, y = 0, rotation = 0, width = Swarm.size[1], height = Swarm.size[2], source = self.sources[0]}
    end
    self:resize(self.count + amount)
end

function Swarm:remove(amount)
    local count = math.max(0, self.count - amount)
    for index = count + 1, self.count do
        local slot = index * Swarm.stride - Swarm.stride
        for field = 1, Swarm.stride do
            self.sprites[slot + field] = nil
        end
        self.arms[index * 2 - 1], self.arms[index * 2] = nil, nil
        self.phases[index], self.columns[index], self.tables[index] = nil, nil, nil
    end
    self:resize(count)
end

-- A float buffer has a fixed size, so it is made again whenever the count changes, and the batch takes the same count.
function Swarm:resize(count)
    self.count = count
    self.buffer = collections.newFloatBuffer(count * Swarm.stride)
    self.batch:resize(count, self.template)
    if count > 0 then
        self:handOver('batch')
    end
end

function Swarm:setSimulation(mode)
    self.simulation = mode
end

-- A frozen school draws whatever the last hand-over gave the renderer, so a new mode gets the current fish at once.
function Swarm:setHandOver(mode)
    self.handOverMode = mode
    if self.count > 0 then
        self:handOver(mode)
    end
end

-- Moves every second fish from `first` on toward its point on the ellipse around the bait that the turn `cos`, `sin` and the stretch `stretchX`, `stretchY` give.
function Swarm:move(first, dt, baitX, baitY, cos, sin, stretchX, stretchY)
    local sprites, arms, phases, columns = self.sprites, self.arms, self.phases, self.columns
    local follow = m.dampFactor(Swarm.follow, dt)
    local beat, beatPerDistance = Swarm.beat * dt, Swarm.beatPerDistance * dt
    local frames, cellWidth = Swarm.frames, Swarm.cellWidth
    local atan = math.atan
    for index = first, self.count, 2 do
        local pair = index * 2
        local armX, armY = arms[pair - 1], arms[pair]
        local slot = index * 4 - 3
        local x, y = sprites[slot], sprites[slot + 1]
        local dx = baitX + (armX * cos - armY * sin) * stretchX - x
        local dy = baitY + (armX * sin + armY * cos) * stretchY - y
        local phase = phases[index] + beat + ((dx < 0 and -dx or dx) + (dy < 0 and -dy or dy)) * beatPerDistance
        phases[index] = phase
        sprites[slot], sprites[slot + 1] = x + dx * follow, y + dy * follow
        sprites[slot + 2] = atan(dy, dx)
        sprites[slot + 3] = columns[index] + phase // 1 % frames * cellWidth
    end
end

-- Moves the school one frame. The full simulation moves every fish, and the staggered one moves the odd fish in one frame and the even fish in the next, each by the time of two frames.
function Swarm:simulate(dt, baitX, baitY)
    self.angle = self.angle + Swarm.spin * dt
    self.frame = self.frame + 1
    local cos, sin = math.cos(self.angle), math.sin(self.angle)
    local wide, tall = Swarm.wide, Swarm.tall
    if self.simulation == 'full' then
        self:move(1, dt, baitX, baitY, cos, sin, wide[1], wide[2])
        self:move(2, dt, baitX, baitY, cos, -sin, tall[1], tall[2])
    elseif self.frame % 2 == 0 then
        self:move(1, dt * 2, baitX, baitY, cos, sin, wide[1], wide[2])
    else
        self:move(2, dt * 2, baitX, baitY, cos, -sin, tall[1], tall[2])
    end
end

-- Hands the sprite fields to the renderer the way `mode` does.
function Swarm:handOver(mode)
    if mode == 'buffer' then
        self.buffer:set(1, self.sprites)
    elseif mode == 'batch' then
        self.buffer:set(1, self.sprites)
        self.batch:writeFields(self.buffer, Swarm.fields)
    else
        local sprites, tables, sources = self.sprites, self.tables, self.sources
        local cellWidth = Swarm.cellWidth
        for index = 1, self.count do
            local slot = index * 4 - 3
            local sprite = tables[index]
            sprite.x, sprite.y, sprite.rotation = sprites[slot], sprites[slot + 1], sprites[slot + 2]
            sprite.source = sources[sprites[slot + 3] // cellWidth]
        end
    end
end

function Swarm:update(dt, baitX, baitY)
    if self.simulation == 'frozen' or self.count == 0 then
        return
    end
    debugging.beginScope('swarm')
    self:simulate(dt, baitX, baitY)
    debugging.endScope()
    debugging.beginScope('handOver')
    self:handOver(self.handOverMode)
    debugging.endScope()
end

function Swarm:draw()
    if self.count == 0 then
        return
    end
    if self.handOverMode == 'batch' then
        self.batch:draw(Swarm.order)
    elseif self.handOverMode == 'tables' then
        graphics2d.drawBatch(self.texture, self.tables, Swarm.order)
    else
        graphics2d.drawBatch(self.texture, self.buffer, self.layout, Swarm.order)
    end
end

return Swarm
