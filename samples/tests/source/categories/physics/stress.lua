-- Hundreds of crates and coins in one pile between two stone pillars, drawn with two sprite batches, with the body count and the time of the physics steps on a readout over the stage, which the status repeats with the same numbers.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Stress = haylen.class('Stress', PhysicsTest)

Stress.code = [[
local world = physics2d.newWorld({subSteps = 4})
local body = world:createBody({x = x, y = -420, rotation = angle})
body:addBox(22, 22, {friction = 0.5})
sprite.x, sprite.y, sprite.rotation = body.x, body.y, body.rotation  -- One sprite of a batch for each body.
graphics2d.drawBatch(crate, sprites, {layer = 1})
print(world.bodyCount, world:stats().stepMilliseconds)]]

Stress.box = 22
Stress.coin = 12
-- New bodies arrive in rows this often, each falling far enough before the next row that none starts inside another.
Stress.rowSize = 40
Stress.rowInterval = 0.1
Stress.dropSpeed = 400
Stress.ground = 360
Stress.left, Stress.right = -760, 760
-- The pillars reach this high, and the physics walls reach far above them, so nothing leaves the pile over the top.
Stress.pillarTop = -290
-- The readout and the status show the same numbers, taken this often.
Stress.sampleInterval = 0.5
-- The crate and the coin cover this share of their pictures.
Stress.crateCover = 152 / 160
Stress.coinCover = 88 / 96

function Stress:enter()
    self:frame{
        code = Stress.code,
        hint = 'Add bodies and watch the step time grow. Every body is drawn from one of two sprite batches. R or the X button clears the pile.',
        controls = {
            ui.button{id = 'boxes', text = 'Add 250 crates', onClick = function() self:queue('box', 250) end},
            ui.button{id = 'balls', text = 'Add 250 coins', onClick = function() self:queue('ball', 250) end},
            ui.button{id = 'many', text = 'Add 1000 of both', onClick = function() self:queue('mixed', 1000) end},
            ui.button{id = 'reset', text = 'Clear', onClick = function() self:build() end},
        },
        focus = 'boxes',
    }
    self.random = m.random(37)
    self.rowClock = 0
    self.sampleClock = Stress.sampleInterval
    self.crate = assets.texture('physics/crate.png', {filter = 'linear'})
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    self:build()
    self:queue('mixed', 600)
end

function Stress:build()
    self.world = physics2d.newWorld({owner = self, subSteps = 4})
    self.boxes, self.balls = {}, {}
    self.boxSprites, self.ballSprites = {}, {}
    self.pending = {}
    local walls = self.world:createBody({type = 'static'})
    walls:addBox(8000, 60, {offsetY = Stress.ground + 30})
    walls:addBox(40, 2000, {offsetX = Stress.left - 20, offsetY = Stress.ground - 1000})
    walls:addBox(40, 2000, {offsetX = Stress.right + 20, offsetY = Stress.ground - 1000})
    walls:addPolygon({{-200, Stress.ground}, {0, Stress.ground - 140}, {200, Stress.ground}})
    self.wedge = Look.newShape({{-200, Stress.ground}, {0, Stress.ground - 140}, {200, Stress.ground}}, 'stone')
end

function Stress:queue(kind, count)
    for _ = 1, count do
        self.pending[#self.pending + 1] = kind == 'mixed' and (self.random:chance(0.5) and 'box' or 'ball') or kind
    end
end

-- New bodies arrive in rows above the pile, thrown down so each row is clear of the stage top before the next one comes.
function Stress:spawnPending(dt)
    self.rowClock = self.rowClock + dt
    if self.rowClock < Stress.rowInterval or #self.pending == 0 then
        return
    end
    self.rowClock = 0
    for slot = 1, math.min(Stress.rowSize, #self.pending) do
        local kind = table.remove(self.pending)
        local x = Stress.left + 40 + slot * 36 + self.random:range(-4, 4)
        local body = self.world:createBody({x = x, y = -420, vy = Stress.dropSpeed, rotation = self.random:range(0, math.pi)})
        if kind == 'box' then
            body:addBox(Stress.box, Stress.box, {friction = 0.5})
            self.boxes[#self.boxes + 1] = body
            local size = Stress.box / Stress.crateCover
            self.boxSprites[#self.boxSprites + 1] = {width = size, height = size}
        else
            body:addCircle(Stress.coin, {friction = 0.5})
            self.balls[#self.balls + 1] = body
            local size = Stress.coin * 2 / Stress.coinCover
            self.ballSprites[#self.ballSprites + 1] = {width = size, height = size}
        end
    end
end

-- Removes the bodies that a full pile pushed out over a pillar.
function Stress:dropEscaped(bodies, sprites)
    for index = #bodies, 1, -1 do
        local x = bodies[index].x
        if x < Stress.left - 40 or x > Stress.right + 40 then
            bodies[index]:destroy()
            table.remove(bodies, index)
            table.remove(sprites, index)
        end
    end
end

-- The readout and the status take the same sample of the counts and the times, so they always agree. The work of a frame is the time the engine spends on it, which is much less than the time between the frames the display shows.
function Stress:sample()
    local frame = profiler.frame()
    local bodies, step = self.world.bodyCount - 1, self:stepTime()
    local work = string.format('Engine work %.1f ms a frame', frame.average)
    if frame.fps > 0 then
        work = string.format('Engine work %.1f ms of each %d ms frame', frame.average, math.floor(1000 / frame.fps + 0.5))
    end
    self.readout = string.format('%d bodies, %d waiting\nPhysics step %.1f ms\n%s', bodies, #self.pending, step, work)
    self:status(string.format('%d bodies in the pile, simulated in %.1f ms a step.', bodies, step))
end

function Stress:update(dt)
    Stress.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:spawnPending(dt)
    self:dropEscaped(self.boxes, self.boxSprites)
    self:dropEscaped(self.balls, self.ballSprites)
    self.sampleClock = self.sampleClock + dt
    if self.sampleClock >= Stress.sampleInterval then
        self.sampleClock = 0
        self:sample()
    end
end

function Stress:fixedUpdate(step)
    self:simulate(self.world, step)
end

local function place(bodies, sprites)
    for index, body in ipairs(bodies) do
        local sprite = sprites[index]
        sprite.x, sprite.y, sprite.rotation = body.x, body.y, body.rotation
    end
end

function Stress:draw(area)
    Scenery.sky(self.camera, Stress.ground)
    Scenery.ground(Stress.ground)
    for _, x in ipairs({Stress.left - 20, Stress.right + 20}) do
        Look.block(x, (Stress.pillarTop + Stress.ground) / 2, 40, Stress.ground - Stress.pillarTop, 'stone', 0)
        Look.block(x, Stress.pillarTop - 10, 64, 24, 'stone', 0)
    end

    Look.drawShape(self.wedge, 0)
    place(self.boxes, self.boxSprites)
    place(self.balls, self.ballSprites)
    graphics2d.drawBatch(self.crate, self.boxSprites, {layer = 1})
    graphics2d.drawBatch(self.coin, self.ballSprites, {layer = 1})
    if self.readout then
        self:label(self.readout, Stress.left + 30, -330, {anchor = {0, 0}, size = 20})
    end
end

return Stress
