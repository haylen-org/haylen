-- A top-down puzzle in a world without gravity: a worker that is a dynamic body driven by a limited force pushes heavy crates, slowed by their damping, across a tiled floor between brick walls onto star pads that are sensors.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local CratePushing = haylen.class('CratePushing', PhysicsTest)

CratePushing.code = [[
local world = physics2d.newWorld({gravity = {0, 0}})
local crate = world:createBody({x = 150, y = -150, linearDamping = 4, angularDamping = 8})
local hero = world:createBody({x = -350, y = -50, fixedRotation = true, sleepEnabled = false})
hero:applyForce(fx, fy)  -- Steers toward the stick with a limited force.
local goal = pad:addBox(70, 70, {sensor = true})
local filled = #goal:overlaps() > 0]]

CratePushing.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
}

-- Walls are `#`, crates `$`, goal pads `.` and the character `@`.
CratePushing.level = {
    '##############',
    '#   .    #   #',
    '# $  ##  $ . #',
    '#  @   $     #',
    '#   ##   ##  #',
    '# .   $   .  #',
    '#        #   #',
    '##############',
}
CratePushing.cell = 100
CratePushing.left, CratePushing.top = -700, -400
CratePushing.speed = 380
CratePushing.response = 10
CratePushing.strength = 3000
CratePushing.onGoal = 22
-- The four neighbors of a cell, which the outline of the walls checks.
CratePushing.sides = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}

function CratePushing:enter()
    self:frame{
        code = CratePushing.code,
        hint = 'Walk with WASD, the arrows, the left stick or the touch stick and push the four crates onto the pads. The crates are heavy and slow down by their damping, and the character pushes with a limited force.',
        controls = {
            ui.label{text = 'Crate damping', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'damping', value = 4, min = 0.5, max = 10, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setDamping(event.value) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = CratePushing.actions,
        view = {1420, 820},
        overlay = {
            ui.touchStick{action = 'move', radius = 110, mode = 'floating', touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 360, height = 300},
        },
    }
    self.damping = 4
    self.facing, self.stride = 0, 0
    self.glow = Scenery.glow()
    self.star = {}
    for index = 1, 10 do
        self.star[index] = {0, 0}
    end
    self:build()
end

function CratePushing:cellCenter(column, row)
    return CratePushing.left + (column - 0.5) * CratePushing.cell, CratePushing.top + (row - 0.5) * CratePushing.cell
end

function CratePushing:isWall(column, row)
    local line = CratePushing.level[row]
    return line ~= nil and line:sub(column, column) == '#'
end

function CratePushing:inside(column, row)
    local line = CratePushing.level[row]
    return line ~= nil and column >= 1 and column <= #line
end

function CratePushing:build()
    self.world = physics2d.newWorld({gravity = {0, 0}})
    self.crates, self.goals, self.moveX, self.moveY = {}, {}, 0, 0
    self.walls = self.world:createBody({type = 'static'})

    -- Runs of wall cells in a row become one box each, so crates slide along long walls without catching on seams.
    for row, line in ipairs(CratePushing.level) do
        local run
        for column = 1, #line + 1 do
            local cell = line:sub(column, column)
            local x, y = self:cellCenter(column, row)
            if cell == '#' then
                run = run or column
            elseif run then
                local first = self:cellCenter(run, row)
                self.walls:addBox((column - run) * CratePushing.cell, CratePushing.cell, {offsetX = (first + x - CratePushing.cell) / 2, offsetY = y})
                run = nil
            end
            if cell == '$' then
                local crate = self.world:createBody({x = x, y = y, linearDamping = self.damping, angularDamping = 8})
                crate:addBox(84, 84, {density = 2, friction = 0.6})
                Look.dress(crate, 'woodCrate')
                self.crates[#self.crates + 1] = crate
            elseif cell == '.' then
                local pad = self.world:createBody({type = 'static', x = x, y = y})
                self.goals[#self.goals + 1] = {x = x, y = y, sensor = pad:addBox(70, 70, {sensor = true}), filled = false}
            elseif cell == '@' then
                self.hero = self.world:createBody({x = x, y = y, fixedRotation = true, sleepEnabled = false})
                self.hero:addCircle(30, {friction = 0.6})
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
            if body ~= self.hero and (body.x - goal.x) ^ 2 + (body.y - goal.y) ^ 2 < CratePushing.onGoal ^ 2 then
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
    local speed = self.hero.velocity:length()
    self.stride = self.stride + dt * speed / 18
    if filled == #self.goals then
        self:status(string.format('All %d crates sit on their pads, and the puzzle is solved.', filled))
    else
        local doing = speed < 15 and 'stands still' or self:isPushing() and 'pushes a crate' or 'walks'
        self:status(string.format('The worker %s, and %d of the %d crates sit on their pads.', doing, filled, #self.goals))
    end
end

-- The worker pushes while it touches a crate and steers toward the stick.
function CratePushing:isPushing()
    if self.moveX == 0 and self.moveY == 0 then
        return false
    end
    for _, contact in ipairs(self.hero:contacts()) do
        if contact.other.type == 'dynamic' then
            return true
        end
    end
    return false
end

-- The character steers its velocity toward the stick with a force it cannot exceed, so heavy crates slow it down.
function CratePushing:fixedUpdate(step)
    local hero = self.hero
    local velocity = hero.velocity
    local fx = hero.mass * (self.moveX * CratePushing.speed - velocity.x) * CratePushing.response
    local fy = hero.mass * (self.moveY * CratePushing.speed - velocity.y) * CratePushing.response
    local length = math.sqrt(fx * fx + fy * fy)
    local limit = hero.mass * CratePushing.strength
    if length > limit then
        fx, fy = fx / length * limit, fy / length * limit
    end
    hero:applyForce(fx, fy)
    self:simulate(self.world, step)
end

-- Draws the floor as worn stone tiles and the walls as one continuous block of brick, whose outline and front face show only where a wall meets the floor.
function CratePushing:drawRoom()
    local unit = Look.pixel
    local cell = CratePushing.cell
    for row, line in ipairs(CratePushing.level) do
        for column = 1, #line do
            local x, y = self:cellCenter(column, row)
            if self:isWall(column, row) then
                graphics2d.drawRect({x - cell / 2, y - cell / 2, cell, cell}, '#FFB4553A', {layer = 0})
                graphics2d.drawRect({x - cell / 2 + 6, y - cell / 2 + 4, cell - 12, cell / 2 - 8}, '#FFE57A55', {layer = 0})
                graphics2d.drawRect({x - cell / 2 + 6, y + 4, cell / 2 - 10, cell / 2 - 10}, '#FFE57A55', {layer = 0})
                graphics2d.drawRect({x + 4, y + 4, cell / 2 - 10, cell / 2 - 10}, '#FFE57A55', {layer = 0})
                if not self:isWall(column, row + 1) and row < #CratePushing.level then
                    graphics2d.drawRect({x - cell / 2, y + cell / 2 - 18, cell, 18}, '#FF7E3524', {layer = 1})
                end
                for _, side in ipairs(CratePushing.sides) do
                    if not self:isWall(column + side[1], row + side[2]) and self:inside(column + side[1], row + side[2]) then
                        local ex, ey = x + side[1] * cell / 2, y + side[2] * cell / 2
                        local hx, hy = side[2] * cell / 2, side[1] * cell / 2
                        graphics2d.drawLine(ex - hx, ey - hy, ex + hx, ey + hy, 4 * unit, Look.ink, {layer = 1})
                    end
                end
            else
                local light = (column + row) % 2 == 0
                graphics2d.drawRect({x - cell / 2, y - cell / 2, cell, cell}, light and '#FFD9CFC0' or '#FFCBC0AF', {layer = -1})
                graphics2d.drawRectOutline({x - cell / 2, y - cell / 2, cell, cell}, 2 * unit, '#33806A50', {layer = -1})
            end
        end
    end
end

-- A goal is a round pad with a star that lights up green once a crate sits on it.
function CratePushing:drawGoal(goal)
    local unit = Look.pixel
    local color = goal.filled and '#FF7BC74D' or '#FFFFC93C'
    graphics2d.drawShape({goal.x - 42, goal.y - 42, 84, 84}, {kind = 'ellipse', color = goal.filled and '#664E9A32' or '#40D99A12', borderWidth = 5, borderColor = color, layer = -1})
    local points = self.star
    for index = 1, 10 do
        local angle = -math.pi / 2 + (index - 1) * math.pi / 5
        local reach = index % 2 == 1 and 26 or 11
        points[index][1], points[index][2] = goal.x + math.cos(angle) * reach, goal.y + math.sin(angle) * reach
    end
    graphics2d.drawPolygon(points, color, {layer = -1})
    if goal.filled then
        graphics2d.draw(self.glow, goal.x, goal.y, {width = 200, height = 200, color = '#807BC74D', blend = 'additive', layer = 2})
    end
end

-- The worker is seen from above: a shadow, shoulders in a blue shirt that swing their arms while walking, and a head with brown hair that faces the way it walks.
function CratePushing:drawHero()
    local unit = Look.pixel
    local hero = self.hero
    local velocity = hero.velocity
    if velocity:length() > 20 then
        self.facing = math.atan(velocity.y, velocity.x)
    end
    local facing = self.facing
    local cos, sin = math.cos(facing), math.sin(facing)
    local swing = math.sin(self.stride) * 12
    graphics2d.drawShape({hero.x - 30, hero.y - 22, 60, 48}, {kind = 'ellipse', color = '#33000000', layer = 3})
    for _, side in ipairs({-1, 1}) do
        local ax, ay = hero.x - sin * side * 30 + cos * swing * side, hero.y + cos * side * 30 + sin * swing * side
        Look.disc(ax, ay, 10, 'skin', 3)
    end
    graphics2d.drawShape({hero.x - 20, hero.y - 32, 40, 64}, {kind = 'ellipse', rotation = facing, color = '#FF1E5FB8', borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = 3})
    graphics2d.drawShape({hero.x - 16, hero.y - 28, 30, 54}, {kind = 'ellipse', rotation = facing, color = '#FF2F86F6', layer = 3})
    Look.disc(hero.x + cos * 4, hero.y + sin * 4, 17, 'skin', 3)
    graphics2d.drawShape({hero.x - 17 - cos * 2, hero.y - 17 - sin * 2, 34, 34}, {kind = 'ellipse', rotation = facing, startAngle = math.pi * 0.55, sweep = math.pi * 0.9, color = '#FF5A3A22', layer = 3})
end

function CratePushing:draw(area)
    graphics2d.drawRect(graphics2d.canvasBounds(), '#FF2B2440', {layer = -30})
    self:drawRoom()
    for _, goal in ipairs(self.goals) do
        self:drawGoal(goal)
    end
    Look.drawAll(self.crates, {layer = 2})
    self:drawHero()
end

return CratePushing
