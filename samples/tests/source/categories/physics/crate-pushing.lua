-- A top-down puzzle in a world without gravity: a character that is a dynamic body driven by a limited force pushes heavy crates, slowed by their damping, onto goal pads that are sensors.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local CratePushing = haylen.class('CratePushing', PhysicsTest)

CratePushing.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
}

-- Walls are `#`, crates `$`, goal pads `.` and the character `@`.
local kLevel = {
    '##############',
    '#   .    #   #',
    '# $  ##  $ . #',
    '#  @   $     #',
    '#   ##   ##  #',
    '# .   $   .  #',
    '#        #   #',
    '##############',
}
local kCell = 100
local kLeft, kTop = -700, -400
local kSpeed = 380
local kResponse = 10
local kStrength = 3000
local kOnGoal = 22

function CratePushing:enter()
    self:frame{
        hint = 'Walk with WASD, the arrows, the left stick or the touch stick and push the four crates onto the pads. The crates are heavy and slow down by their damping, and the character pushes with a limited force.',
        controls = {
            ui.label{text = 'Crate damping', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'damping', value = 4, min = 0.5, max = 10, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setDamping(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = CratePushing.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
        },
    }
    self.damping = 4
    self:build()
end

function CratePushing:cellCenter(column, row)
    return kLeft + (column - 0.5) * kCell, kTop + (row - 0.5) * kCell
end

function CratePushing:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.crates, self.goals, self.push, self.moveX, self.moveY = {}, {}, 0, 0, 0
    self.walls = self.world:createBody({type = 'static'})

    -- Runs of wall cells in a row become one box each, so crates slide along long walls without catching on seams.
    for row, line in ipairs(kLevel) do
        local run
        for column = 1, #line + 1 do
            local cell = line:sub(column, column)
            local x, y = self:cellCenter(column, row)
            if cell == '#' then
                run = run or column
            elseif run then
                local first = self:cellCenter(run, row)
                parts.box(self.walls, (column - run) * kCell, kCell, {offsetX = (first + x - kCell) / 2, offsetY = y})
                run = nil
            end
            if cell == '$' then
                local crate = self.world:createBody({x = x, y = y, linearDamping = self.damping, angularDamping = 8})
                parts.box(crate, 84, 84, {density = 2, friction = 0.6})
                parts.paint(crate, '#FFBC8F5A')
                self.crates[#self.crates + 1] = crate
            elseif cell == '.' then
                local pad = self.world:createBody({type = 'static', x = x, y = y})
                self.goals[#self.goals + 1] = {x = x, y = y, sensor = pad:addBox(70, 70, {sensor = true}), filled = false}
            elseif cell == '@' then
                self.hero = self.world:createBody({x = x, y = y, fixedRotation = true, sleepEnabled = false})
                parts.circle(self.hero, 30, {friction = 0.6})
                parts.paint(self.hero, '#FF4DD0E1')
            end
        end
    end
end

function CratePushing:setDamping(damping)
    self.damping = damping
    for _, crate in ipairs(self.crates) do
        crate.linearDamping = damping
    end
end

function CratePushing:exit()
    CratePushing.super.exit(self)
    input.clearVirtual()
end

-- A pad counts a crate whose center sits near its own among the shapes inside the sensor.
function CratePushing:countGoals()
    local filled = 0
    for _, goal in ipairs(self.goals) do
        goal.filled = false
        for _, shape in ipairs(goal.sensor:overlaps()) do
            local body = shape.body
            if body ~= self.hero and (body.x - goal.x) ^ 2 + (body.y - goal.y) ^ 2 < kOnGoal ^ 2 then
                goal.filled = true
            end
        end
        filled = filled + (goal.filled and 1 or 0)
    end
    return filled
end

function CratePushing:update(dt)
    CratePushing.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.moveX, self.moveY = input.vector('move')
    local filled = self:countGoals()
    local velocity = self.hero.velocity
    self:status(string.format('%s, crates on pads %d of %d, speed %.0f, pushing force %.0f of %.0f, step %.2f ms', filled == #self.goals and 'Solved' or 'Pushing', filled, #self.goals, math.sqrt(velocity.x ^ 2 + velocity.y ^ 2), self.push, self.hero.mass * kStrength, self:stepTime()))
end

-- The character steers its velocity toward the stick with a force it cannot exceed, so heavy crates slow it down.
function CratePushing:fixedUpdate(step)
    local hero = self.hero
    local velocity = hero.velocity
    local fx = hero.mass * (self.moveX * kSpeed - velocity.x) * kResponse
    local fy = hero.mass * (self.moveY * kSpeed - velocity.y) * kResponse
    local length = math.sqrt(fx * fx + fy * fy)
    local limit = hero.mass * kStrength
    if length > limit then
        fx, fy = fx / length * limit, fy / length * limit
    end
    self.push = math.min(length, limit)
    hero:applyForce(fx, fy)
    self:simulate(self.world, step)
end

function CratePushing:draw(area)
    for _, goal in ipairs(self.goals) do
        graphics2d.drawRect({goal.x - 40, goal.y - 40, 80, 80}, goal.filled and '#6066BB6A' or '#30FFFFFF', {layer = -1})
        graphics2d.drawRectOutline({goal.x - 40, goal.y - 40, 80, 80}, 4, goal.filled and '#FF66BB6A' or '#88FFFFFF', {layer = -1})
    end
    parts.draw(self.walls)
    parts.drawAll(self.crates, {layer = 1})
    for _, crate in ipairs(self.crates) do
        local cos, sin = math.cos(crate.rotation), math.sin(crate.rotation)
        for _, corner in ipairs({{-1, -1}, {1, -1}}) do
            local x, y = corner[1] * 30, corner[2] * 30
            graphics2d.drawLine(crate.x + x * cos - y * sin, crate.y + x * sin + y * cos, crate.x - x * cos + y * sin, crate.y - x * sin - y * cos, 6, '#88704A2C', {layer = 2})
        end
    end
    parts.draw(self.hero, {layer = 3})
end

return CratePushing
