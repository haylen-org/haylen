-- Joints with a `breakForce` or `breakTorque` that the world destroys when the load passes it: shelves welded to a wall, a bridge of planks, and lamps hanging on chains, with sparks where `world.onJointBreak` reports each break.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local BreakingJoints = haylen.class('BreakingJoints', PhysicsTest)

local kSparkLife = 0.6
local kPlanks = 11
local kPitch = 64

function BreakingJoints:enter()
    self:frame{
        hint = 'Drop weights on the shelves and the bridge, or drag the lamps and pull. Each joint holds up to its break force in weights of its load. R or the X button rebuilds everything.',
        controls = {
            ui.button{id = 'drop', text = 'Drop a weight', onClick = function() self:drop() end},
            ui.label{text = 'Break force in weights of a crate', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'strength', value = 12, min = 2, max = 60, step = 1, showValue = true, decimals = 0, onChange = function(event)
                self.strength = event.value
                self:build()
            end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'drop',
    }
    self.random = m.random(41)
    self.strength = 12
    self:build()
end

function BreakingJoints:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.sparks, self.breaks = {}, {}, 0
    local wall = self.world:createBody({type = 'static'})
    parts.box(wall, 40, 600, {offsetX = -760, offsetY = 100})
    parts.box(wall, 1600, 40, {offsetY = 410})
    parts.box(wall, 120, 300, {offsetX = -60, offsetY = 260})
    parts.box(wall, 120, 300, {offsetX = 700, offsetY = 260})
    parts.box(wall, 400, 20, {offsetX = 350, offsetY = -400})
    self.wall = wall

    -- The weight of a crate of density 1 measures the load of every joint, so the slider reads in crates.
    local weight = 60 * 60 / self.world.pixelsPerMeter ^ 2 * self.world.gravity.y

    for row = 0, 2 do
        local shelf = self:add(-640, -150 + row * 140, function(body) parts.box(body, 200, 16) end)
        self.world:createJoint('weld', wall, shelf, {ax = -740, ay = -150 + row * 140, breakForce = weight * self.strength, breakTorque = weight * self.strength * 40})
    end

    -- The planks start on the curve they sag into, so the bridge carries only its weight from the first step.
    local previous, joints = wall, self:sag(640, kPlanks, kPitch)
    for plank = 1, kPlanks do
        local a, b = joints[plank], joints[plank + 1]
        local body = self:add((a[1] + b[1]) / 2, 110 + (a[2] + b[2]) / 2, function(body) parts.box(body, kPitch - 2, 14) end)
        body.rotation = math.atan(b[2] - a[2], b[1] - a[1])
        self.world:createJoint('revolute', previous, body, {ax = a[1], ay = 110 + a[2], breakForce = weight * self.strength})
        previous = body
    end
    self.world:createJoint('revolute', previous, wall, {ax = 640, ay = 110, breakForce = weight * self.strength})

    for lamp = 0, 1 do
        local x = 250 + lamp * 200
        local hook, link = wall, nil
        for index = 1, 5 do
            link = self:add(x, -390 + (index - 0.5) * 24, function(body) parts.box(body, 6, 24) end)
            self.world:createJoint('revolute', hook, link, {ax = x, ay = -390 + (index - 1) * 24, breakForce = weight * self.strength * 0.5})
            hook = link
        end
        local shade = self:add(x, -250, function(body) parts.polygon(body, {{-40, 20}, {-16, -20}, {16, -20}, {40, 20}}) end)
        parts.paint(shade, '#FFFFD54F')
        self.world:createJoint('revolute', hook, shade, {ax = x, ay = -270, breakForce = weight * self.strength * 0.5})
    end

    self.world.onJointBreak = function(joint, info)
        self.breaks = self.breaks + 1
        local body = info.bodyB or info.bodyA
        if body then
            self.sparks[#self.sparks + 1] = {x = body.x, y = body.y, age = 0, force = math.sqrt(info.forceX ^ 2 + info.forceY ^ 2) / weight}
        end
    end
end

-- Returns the joints of a chain of planks of equal pitch hanging across a span, with slopes that grow evenly toward the ends like a hanging chain, found by bisection.
function BreakingJoints:sag(span, count, pitch)
    local low, high = 0, 2
    for _ = 1, 40 do
        local slope = (low + high) / 2
        local width = 0
        for plank = 1, count do
            width = width + pitch * math.cos(math.atan(slope * (count / 2 - plank + 0.5)))
        end
        if width > span then
            low = slope
        else
            high = slope
        end
    end
    local points, x, y = {{0, 0}}, 0, 0
    for plank = 1, count do
        local angle = math.atan(low * (count / 2 - plank + 0.5))
        x, y = x + pitch * math.cos(angle), y + pitch * math.sin(angle)
        points[#points + 1] = {x, y}
    end
    return points
end

function BreakingJoints:add(x, y, shape)
    local body = self.world:createBody({x = x, y = y})
    shape(body)
    self.bodies[#self.bodies + 1] = body
    return body
end

function BreakingJoints:drop()
    local x = self.random:chance(0.5) and self.random:range(-700, -560) or self.random:range(60, 600)
    local crate = self:add(x, -380, function(body) parts.box(body, 60, 60, {density = self.random:range(1, 6)}) end)
    parts.paint(crate, '#FF78909C')
end

function BreakingJoints:update(dt)
    BreakingJoints.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    for index = #self.sparks, 1, -1 do
        local spark = self.sparks[index]
        spark.age = spark.age + dt
        if spark.age > kSparkLife then
            table.remove(self.sparks, index)
        end
    end
    self:status(string.format('Joints broken %d, break force of %d crate weights, step %.2f ms', self.breaks, self.strength, self:stepTime()))
end

function BreakingJoints:fixedUpdate(step)
    self:simulate(self.world, step)
end

function BreakingJoints:draw(area)
    parts.draw(self.wall)
    parts.drawAll(self.bodies, {layer = 1})
    for _, spark in ipairs(self.sparks) do
        local fade = 1 - spark.age / kSparkLife
        graphics2d.drawCircle(spark.x, spark.y, 10 + spark.age * 80, string.format('#%02XFFE082', math.floor(fade * 255)), {layer = 2})
        graphics2d.drawText(nil, string.format('%.0f crates', spark.force), spark.x, spark.y - 30 - spark.age * 40, {size = 20, color = '#FFFFFFFF', anchor = {0.5, 0.5}, layer = 3})
    end
    self.grab:draw()
end

return BreakingJoints
