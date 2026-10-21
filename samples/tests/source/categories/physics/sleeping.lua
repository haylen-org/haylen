-- The same pyramid of blocks in two worlds: with sleeping off every block stays awake and the solver works on the settled pile every step, and with sleeping on the pile falls asleep once it rests, costs almost nothing, and wakes where something touches it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Sleeping = haylen.class('Sleeping', PhysicsTest)

Sleeping.code = [[
local world = physics2d.newWorld({sleepEnabled = true})
local block = world:createBody({x = 0, y = 345, sleepThreshold = 3.2})
local asleep = not block.awake
world:wakeAll()
local stats = world:stats()
print(stats.awakeBodies, stats.solveMilliseconds)]]

Sleeping.rows = 16
Sleeping.size = 32
Sleeping.ground = 360
Sleeping.awake = Look.material('orange')
Sleeping.asleep = Look.tint('orange', '#FF8E9AC0', '#FF6A7398', '#FFB4BEDC')

function Sleeping:enter()
    self:frame{
        code = Sleeping.code,
        hint = 'Blocks turn gray while they sleep. Drag a block or drop a ball to wake the blocks it touches, or wake every block at once. R or the X button starts over.',
        controls = {
            ui.button{id = 'ball', text = 'Drop a ball on both piles', onClick = function() self:dropBall() end},
            ui.button{id = 'wake', text = 'Wake every block', onClick = function() self:wakeAll() end},
            ui.label{text = 'Blocks sleep below this speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'threshold', value = 3.2, min = 0.5, max = 40, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setThreshold(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        focus = 'ball',
    }
    self.random = m.random(31)
    self.threshold = 3.2
    self:build()
end

function Sleeping:build()
    self.halves = {}
    local size = Sleeping.size
    for index, sleeping in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({sleepEnabled = sleeping})
        local ground = world:createBody({type = 'static', x = origin, y = Sleeping.ground + 30})
        ground:addBox(780, 60)
        local blocks = {}
        for row = 0, Sleeping.rows - 1 do
            for column = 0, Sleeping.rows - row - 1 do
                local x = origin + (column - (Sleeping.rows - row) / 2 + 0.5) * (size + 1)
                local block = world:createBody({x = x, y = Sleeping.ground - size / 2 - row * size, sleepThreshold = self.threshold})
                block:addBox(size, size)
                Look.dress(block, Sleeping.awake)
                blocks[#blocks + 1] = {body = block, look = block.data.look}
            end
        end
        self.halves[index] = {world = world, origin = origin, blocks = blocks, grab = Grab(world), balls = {}}
    end
end

function Sleeping:setThreshold(value)
    self.threshold = value
    for _, half in ipairs(self.halves) do
        for _, block in ipairs(half.blocks) do
            block.body.sleepThreshold = value
        end
    end
end

function Sleeping:dropBall()
    local offset = self.random:range(-200, 200)
    for _, half in ipairs(self.halves) do
        local ball = half.world:createBody({x = half.origin + offset, y = -400})
        ball:addCircle(26, {density = 4})
        Look.dress(ball, 'red', {spin = true})
        half.balls[#half.balls + 1] = ball
    end
end

function Sleeping:wakeAll()
    for _, half in ipairs(self.halves) do
        half.world:wakeAll()
    end
end

function Sleeping:update(dt)
    Sleeping.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, half in ipairs(self.halves) do
        half.grab:update(self.pointer, self.camera)
        half.stats = half.world:stats()
    end
    local left, right = self.halves[1].stats, self.halves[2].stats
    self:status(string.format('The solver works on %d awake bodies in %.2f ms without sleeping, and on %d in %.2f ms with it.', left.awakeBodies, left.solveMilliseconds, right.awakeBodies, right.solveMilliseconds))
end

function Sleeping:fixedUpdate(step)
    for _, half in ipairs(self.halves) do
        self:simulate(half.world, step)
    end
end

function Sleeping:draw(area)
    Scenery.sky(self.camera, Sleeping.ground)
    Scenery.ground(Sleeping.ground)
    self:compare('Sleeping off', 'Sleeping on', 0, -340)
    local time = haylen.elapsed()
    for _, half in ipairs(self.halves) do
        local stats = half.stats or half.world:stats()
        self:label(string.format('%d of %d awake', stats.awakeBodies, #half.blocks + #half.balls), half.origin, -290, {size = 18})
        local sleepers = 0
        for _, block in ipairs(half.blocks) do
            local awake = block.body.awake
            block.look.material = awake and Sleeping.awake or Sleeping.asleep
            sleepers = sleepers + (awake and 0 or 1)
            Look.draw(block.body)
        end
        Look.drawAll(half.balls, {layer = 1})
        if sleepers > #half.blocks / 2 then
            for index = 0, 2 do
                local rise = (time * 0.6 + index / 3) % 1
                self:tag('z', half.origin + 30 + rise * 60 + index * 6, -170 - rise * 90, {size = 20 + index * 6, color = {1, 1, 1, 1 - rise}})
            end
        end
        half.grab:draw()
    end
end

return Sleeping
