-- Hundreds of boxes and balls in one pile, drawn with two sprite batches, with the body count and the time of every physics step.
local haylen = require('haylen')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local parts = require('parts')
local sample = require('sample')

local Stress = haylen.class('Stress', sample.Test)

local kBox = 18
local kBall = 10
local kSpawnPerFrame = 40
local kPalette = {'#FFE57373', '#FF64B5F6', '#FF81C784', '#FFFFD54F', '#FFBA68C8', '#FF4DD0E1'}
local kDiscSize = 64

-- A white disc with a soft edge that tints into every ball of the batch.
local function discTexture()
    local bytes = {}
    local center = (kDiscSize - 1) / 2
    for y = 0, kDiscSize - 1 do
        for x = 0, kDiscSize - 1 do
            local distance = math.sqrt((x - center) ^ 2 + (y - center) ^ 2)
            local alpha = math.floor(m.saturate(center - distance + 0.5) * 255)
            bytes[#bytes + 1] = string.char(255, 255, 255, alpha)
        end
    end
    return graphics.newTexture(kDiscSize, kDiscSize, {pixels = table.concat(bytes), filter = 'linear'})
end

function Stress:enter()
    Stress.super.enter(self, {
        hint = 'Add bodies and watch the step time grow. Every body is drawn from one of two sprite batches. R or X clears the pile.',
        controls = {
            ui.button{id = 'boxes', text = 'Add 250 boxes', onClick = function() self:queue('box', 250) end},
            ui.button{id = 'balls', text = 'Add 250 balls', onClick = function() self:queue('ball', 250) end},
            ui.button{id = 'many', text = 'Add 1000 of both', onClick = function() self:queue('mixed', 1000) end},
            ui.button{id = 'reset', text = 'Clear', onClick = function() self:build() end},
        },
        stats = true,
        focus = 'boxes',
    })
    self.random = m.random(37)
    self.disc = discTexture()
    self.white = graphics.whiteTexture()
    self:build()
    self:queue('mixed', 600)
end

function Stress:build()
    self.world = physics2d.newWorld({subSteps = 4})
    self.boxes, self.balls = {}, {}
    self.boxSprites, self.ballSprites = {}, {}
    self.pending = {}
    local walls = self.world:createBody({type = 'static'})
    parts.box(walls, 1600, 40, {offsetY = 410})
    parts.box(walls, 40, 900, {offsetX = -780})
    parts.box(walls, 40, 900, {offsetX = 780})
    parts.polygon(walls, {{-200, 390}, {0, 250}, {200, 390}})
    self.statics = {walls}
end

function Stress:queue(kind, count)
    for _ = 1, count do
        self.pending[#self.pending + 1] = kind == 'mixed' and (self.random:chance(0.5) and 'box' or 'ball') or kind
    end
end

-- New bodies arrive a few per frame in a row above the pile, so they never start inside each other.
function Stress:spawnPending()
    for slot = 1, math.min(kSpawnPerFrame, #self.pending) do
        local kind = table.remove(self.pending)
        local x = -720 + slot * 36 + self.random:range(-4, 4)
        local body = self.world:createBody({x = x, y = -420, rotation = self.random:range(0, math.pi)})
        local color = kPalette[self.random:integer(1, #kPalette)]
        if kind == 'box' then
            body:addBox(kBox, kBox, {friction = 0.5})
            self.boxes[#self.boxes + 1] = body
            self.boxSprites[#self.boxSprites + 1] = {width = kBox, height = kBox, color = color}
        else
            body:addCircle(kBall, {friction = 0.5})
            self.balls[#self.balls + 1] = body
            self.ballSprites[#self.ballSprites + 1] = {width = kBall * 2, height = kBall * 2, color = color}
        end
    end
end

function Stress:exit()
    Stress.super.exit(self)
    self.world, self.boxes, self.balls, self.boxSprites, self.ballSprites, self.statics, self.disc = nil, nil, nil, nil, nil, nil, nil
end

function Stress:update(dt)
    Stress.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:spawnPending()
    local frame = profiler.frame()
    self:showStats(string.format('Bodies %d\nWaiting %d\nStep %.2f ms\nFrame %.2f ms\nFPS %.0f', self.world.bodyCount, #self.pending, sample.milliseconds('physics step'), frame.milliseconds, frame.fps))
end

function Stress:fixedUpdate(step)
    sample.step(self.world, step)
end

local function place(bodies, sprites)
    for index, body in ipairs(bodies) do
        local sprite = sprites[index]
        sprite.x, sprite.y, sprite.rotation = body.x, body.y, body.rotation
    end
end

function Stress:render()
    self:beginWorld()
    parts.drawAll(self.statics)
    place(self.boxes, self.boxSprites)
    place(self.balls, self.ballSprites)
    graphics2d.drawBatch(self.white, self.boxSprites, {layer = 1})
    graphics2d.drawBatch(self.disc, self.ballSprites, {layer = 1})
end

return Stress
