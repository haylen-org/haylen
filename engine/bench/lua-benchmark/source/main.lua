-- A bunnymark for Lua: sprites fall, bounce off the edges of the screen and draw every frame. Each phase keeps them in a different way and logs the average CPU milliseconds of the process spent updating them, drawing them and on the whole frame. CPU time stays steady when other programs load the machine, while wall time does not.
--   tables  one Lua table for each sprite, drawn with graphics2d.drawBatch(texture, tables)
--   buffer  positions in a FloatBuffer written one value at a time, drawn with graphics2d.drawBatch(texture, buffer, layout)
--   bulk    positions in a plain Lua array copied into the FloatBuffer with one buffer:set call, drawn the same way
--   batch   the bulk update feeding a persistent SpriteBatch through batch:writeFields
local haylen = require('haylen')
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local collections = require('haylen.collections')
local scene = require('haylen.scene')

local width, height, size, gravity, step = 1920, 1080, 4, 980, 1 / 60
local counts = {10000, 100000, 1000000}
local modes = {'tables', 'buffer', 'bulk', 'batch'}
local measured = {[10000] = 120, [100000] = 30, [1000000] = 6}
local warmup = 3
local texture = graphics.whiteTexture()
local fields = {'x', 'y'}
local layout = {fields = fields, width = size, height = size}

local phases = {}
for _, count in ipairs(counts) do
    for _, mode in ipairs(modes) do
        phases[#phases + 1] = {count = count, mode = mode}
    end
end

local function setup(phase)
    math.randomseed(7)
    local random = math.random
    local state = {count = phase.count, vx = {}, vy = {}}
    for index = 1, phase.count do
        state.vx[index] = random() * 400 - 200
        state.vy[index] = random() * 400 - 200
    end

    if phase.mode == 'tables' then
        state.sprites = {}
        for index = 1, phase.count do
            state.sprites[index] = {x = random() * width, y = random() * height, width = size, height = size}
        end
        return state
    end

    state.positions = {}
    for index = 1, phase.count do
        state.positions[index * 2 - 1] = random() * width
        state.positions[index * 2] = random() * height
    end
    state.buffer = collections.newFloatBuffer(phase.count * 2)
    state.buffer:set(1, state.positions)
    if phase.mode == 'batch' then
        state.batch = graphics2d.newSpriteBatch(texture)
        state.batch:resize(phase.count, {width = size, height = size})
    end
    return state
end

-- Every mode runs the same motion, so only where the positions live changes.
local function move(x, y, index, vx, vy)
    local speedX, speedY = vx[index], vy[index] + gravity * step
    x, y = x + speedX * step, y + speedY * step
    if x < 0 or x > width then
        vx[index] = -speedX
        x = x < 0 and 0 or width
    end
    if y > height then
        speedY = -speedY * 0.85
        y = height
    end
    vy[index] = speedY
    return x, y
end

local updates = {}

function updates.tables(state)
    local sprites, vx, vy = state.sprites, state.vx, state.vy
    for index = 1, state.count do
        local sprite = sprites[index]
        sprite.x, sprite.y = move(sprite.x, sprite.y, index, vx, vy)
    end
end

function updates.buffer(state)
    local buffer, vx, vy = state.buffer, state.vx, state.vy
    for index = 1, state.count do
        local slot = index * 2
        buffer[slot - 1], buffer[slot] = move(buffer[slot - 1], buffer[slot], index, vx, vy)
    end
end

function updates.bulk(state)
    local positions, vx, vy = state.positions, state.vx, state.vy
    for index = 1, state.count do
        local slot = index * 2
        positions[slot - 1], positions[slot] = move(positions[slot - 1], positions[slot], index, vx, vy)
    end
    state.buffer:set(1, positions)
end

updates.batch = updates.bulk

local draws = {}

function draws.tables(state)
    graphics2d.drawBatch(texture, state.sprites)
end

function draws.buffer(state)
    graphics2d.drawBatch(texture, state.buffer, layout)
end

draws.bulk = draws.buffer

function draws.batch(state)
    state.batch:writeFields(state.buffer, fields)
    state.batch:draw()
end

local bench = {}

function bench:enter()
    print(string.format('%9s %-7s %10s %10s %10s', 'sprites', 'mode', 'update ms', 'draw ms', 'frame ms'))
    self.index = 0
    self:next()
end

function bench:next()
    self.index = self.index + 1
    self.phase = phases[self.index]
    self.state = nil
    collectgarbage()
    collectgarbage()
    if self.phase then
        self.state = setup(self.phase)
        self.frames = 0
        self.totals = {update = 0, draw = 0, frame = 0}
    end
end

-- A frame lasts from one update to the next, and a phase adds up the frames after its warmup and reports once it has enough of them.
function bench:update()
    local now = os.clock()
    if not self.phase then
        haylen.quit()
        return
    end

    local count, mode = self.phase.count, self.phase.mode
    if self.frames > warmup then
        self.totals.frame = self.totals.frame + (now - self.frameStart)
    end
    if self.frames == warmup + measured[count] then
        local frames = measured[count] / 1000
        print(string.format('%9d %-7s %10.2f %10.2f %10.2f', count, mode, self.totals.update / frames, self.totals.draw / frames, self.totals.frame / frames))
        self:next()
        return
    end

    self.frames = self.frames + 1
    self.frameStart = now
    debugging.beginScope('bunnies update')
    updates[mode](self.state)
    debugging.endScope()
    if self.frames > warmup then
        self.totals.update = self.totals.update + (os.clock() - now)
    end
end

function bench:render()
    if not self.state then
        return
    end
    graphics2d.beginScreen()
    local started = os.clock()
    debugging.beginScope('bunnies draw')
    draws[self.phase.mode](self.state)
    debugging.endScope()
    if self.frames > warmup then
        self.totals.draw = self.totals.draw + (os.clock() - started)
    end
end

scene.push(bench)
