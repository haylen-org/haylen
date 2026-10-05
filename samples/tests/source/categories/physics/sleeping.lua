-- The same pyramid of boxes in two worlds: with sleeping off every box stays awake and the solver works on the settled pile every step, and with sleeping on the pile falls asleep once it rests, costs almost nothing, and wakes where something touches it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Sleeping = haylen.class('Sleeping', PhysicsTest)

local kRows = 18
local kSize = 30
local kAsleep = '#FF5C6B8A'

function Sleeping:enter()
    self:frame{
        hint = 'Boxes turn gray while they sleep. Drag a box or drop a ball to wake the boxes it touches, or wake every box at once. R or the X button starts over.',
        controls = {
            ui.button{id = 'ball', text = 'Drop a ball on both piles', onClick = function() self:dropBall() end},
            ui.button{id = 'wake', text = 'Wake every box', onClick = function() self:wakeAll() end},
            ui.label{text = 'Speed under which boxes may sleep, in units per second', font = 'caption', color = 'textMuted'},
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
    self.sides = {}
    for index, sleeping in ipairs({false, true}) do
        local origin = index == 1 and -400 or 400
        local world = physics2d.newWorld({sleepEnabled = sleeping})
        local ground = world:createBody({type = 'static', x = origin, y = 400})
        parts.box(ground, 760, 40)
        local boxes = {}
        for row = 0, kRows - 1 do
            for column = 0, kRows - row - 1 do
                local x = origin + (column - (kRows - row) / 2 + 0.5) * (kSize + 1)
                local box = world:createBody({x = x, y = 380 - kSize / 2 - row * kSize, sleepThreshold = self.threshold})
                parts.box(box, kSize, kSize)
                boxes[#boxes + 1] = box
            end
        end
        self.sides[index] = {world = world, origin = origin, ground = ground, boxes = boxes, grab = Grab(world), balls = {}}
    end
end

function Sleeping:setThreshold(value)
    self.threshold = value
    for _, side in ipairs(self.sides) do
        for _, box in ipairs(side.boxes) do
            box.sleepThreshold = value
        end
    end
end

function Sleeping:dropBall()
    local offset = self.random:range(-200, 200)
    for _, side in ipairs(self.sides) do
        local ball = side.world:createBody({x = side.origin + offset, y = -400})
        parts.circle(ball, 24, {density = 4})
        parts.paint(ball, '#FFFFD54F')
        side.balls[#side.balls + 1] = ball
    end
end

function Sleeping:wakeAll()
    for _, side in ipairs(self.sides) do
        side.world:wakeAll()
    end
end

function Sleeping:update(dt)
    Sleeping.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    for _, side in ipairs(self.sides) do
        side.grab:update(self.pointer, self.camera)
    end
    local left, right = self.sides[1].world:stats(), self.sides[2].world:stats()
    self:status(string.format('Awake bodies %d of %d without sleeping and %d of %d with sleeping, solver %.3f ms and %.3f ms', left.awakeBodies, left.bodies, right.awakeBodies, right.bodies, left.solveMilliseconds, right.solveMilliseconds))
end

function Sleeping:fixedUpdate(step)
    for _, side in ipairs(self.sides) do
        self:simulate(side.world, step)
    end
end

function Sleeping:draw(area)
    graphics2d.drawLine(0, -430, 0, 430, 2, '#44FFFFFF')
    local titles = {'Sleeping off', 'Sleeping on'}
    for index, side in ipairs(self.sides) do
        local stats = side.world:stats()
        graphics2d.drawText(nil, titles[index], side.origin, -390, {size = 28, color = index == 1 and '#FFFF8A84' or '#FF6FDCA0', anchor = {0.5, 0.5}})
        graphics2d.drawText(nil, string.format('%d awake, step %.3f ms', stats.awakeBodies, stats.stepMilliseconds), side.origin, -350, {size = 22, color = '#CCFFFFFF', anchor = {0.5, 0.5}})
        parts.draw(side.ground)
        for _, box in ipairs(side.boxes) do
            parts.paint(box, box.awake and '#FF64B5F6' or kAsleep)
            parts.draw(box)
        end
        parts.drawAll(side.balls, {layer = 1})
        side.grab:draw()
    end
end

return Sleeping
