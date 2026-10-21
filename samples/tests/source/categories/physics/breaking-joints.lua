-- Joints with a `breakForce` or `breakTorque` that the world destroys when the load passes it: shelves welded to a wall, a bridge of planks between two stone piers, and lamps hanging on chains, with sparks and the force where `world.onJointBreak` reports each break. Weights fall on the shelves and the bridge by themselves when the test opens.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local BreakingJoints = haylen.class('BreakingJoints', PhysicsTest)

BreakingJoints.code = [[
local weight = crate.mass * 980  -- The weight of one crate.
world:createJoint('weld', wall, shelf, {ax = -740, ay = -150, breakForce = weight * 12, breakTorque = weight * 480})
world:createJoint('revolute', plank, nextPlank, {ax = x, ay = 110, breakForce = weight * 12})
world.onJointBreak = function(joint, info) spark(info.bodyB, math.sqrt(info.forceX ^ 2 + info.forceY ^ 2)) end]]

BreakingJoints.labelLife = 1.6
BreakingJoints.planks = 11
BreakingJoints.pitch = 64
BreakingJoints.ground = 360
BreakingJoints.deck = 110
BreakingJoints.ceiling = -330

function BreakingJoints:enter()
    self:frame{
        code = BreakingJoints.code,
        hint = 'Drop weights on the shelves and the bridge, or drag the lamps and pull. Each joint holds up to its break force in weights of a crate. R or the X button rebuilds everything.',
        controls = {
            ui.button{id = 'drop', text = 'Drop a weight', onClick = function() self:drop() end},
            ui.label{text = 'Break force, in weights of a crate', font = 'caption', color = 'textMuted'},
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
    self.sparks = Burst('combat/bullet-impact', {count = 6, scale = 0.7, layer = 30})
    local glow = graphics.newImage(128, 128)
    glow:fillGradient({kind = 'radial', stops = {{0, '#C0FFE9A0'}, {0.5, '#50FFD060'}, {1, '#00FFB040'}}})
    self.glow = graphics.newTexture(glow, {filter = 'linear'})
    self.chain = {}
    self:build()
    timer.after(0.8, function() self:drop(-640, 6) end, {owner = self})
    timer.after(1.3, function() self:drop(330, 6) end, {owner = self})
end

function BreakingJoints:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.labels, self.lamps, self.breaks = {}, {}, {}, 0
    local ground = BreakingJoints.ground
    local wall = self.world:createBody({type = 'static'})
    wall:addBox(40, 560, {offsetX = -760, offsetY = ground - 280})
    wall:addBox(8000, 60, {offsetY = ground + 30})
    wall:addBox(120, ground - BreakingJoints.deck, {offsetX = -60, offsetY = (ground + BreakingJoints.deck) / 2})
    wall:addBox(120, ground - BreakingJoints.deck, {offsetX = 700, offsetY = (ground + BreakingJoints.deck) / 2})
    wall:addBox(440, 20, {offsetX = 350, offsetY = BreakingJoints.ceiling})
    self.wall = wall

    -- The weight of a crate of density 1 measures the load of every joint, so the slider reads in crates.
    local weight = 60 * 60 / self.world.pixelsPerMeter ^ 2 * self.world.gravity.y
    self.weight = weight

    self.shelves = {}
    for row = 0, 2 do
        local y = -170 + row * 140
        local shelf = self:add(-640, y, function(body) body:addBox(200, 18) end, 'wood')
        self.world:createJoint('weld', wall, shelf, {ax = -740, ay = y, breakForce = weight * self.strength, breakTorque = weight * self.strength * 40})
        self.shelves[#self.shelves + 1] = shelf
    end

    -- The planks start on the curve they sag into, so the bridge carries only its weight from the first step.
    local deck, pitch = BreakingJoints.deck, BreakingJoints.pitch
    local previous, joints = wall, self:sag(640, BreakingJoints.planks, pitch)
    for plank = 1, BreakingJoints.planks do
        local a, b = joints[plank], joints[plank + 1]
        local body = self:add((a[1] + b[1]) / 2, deck + (a[2] + b[2]) / 2, function(body) body:addBox(pitch - 2, 16) end, 'wood')
        body.rotation = math.atan(b[2] - a[2], b[1] - a[1])
        self.world:createJoint('revolute', previous, body, {ax = a[1], ay = deck + a[2], breakForce = weight * self.strength})
        previous = body
    end
    self.world:createJoint('revolute', previous, wall, {ax = 640, ay = deck, breakForce = weight * self.strength})

    for lamp = 0, 1 do
        local x = 250 + lamp * 200
        local hook, link = wall, nil
        local links = {}
        for index = 1, 5 do
            local top = BreakingJoints.ceiling + 10
            link = self:add(x, top + (index - 0.5) * 24, function(body) body:addBox(6, 24) end)
            self.world:createJoint('revolute', hook, link, {ax = x, ay = top + (index - 1) * 24, breakForce = weight * self.strength * 0.5})
            links[index] = link
            hook = link
        end
        local shade = self:add(x, BreakingJoints.ceiling + 150, function(body) body:addPolygon({{-40, 20}, {-16, -20}, {16, -20}, {40, 20}}) end, 'gold')
        self.world:createJoint('revolute', hook, shade, {ax = x, ay = BreakingJoints.ceiling + 130, breakForce = weight * self.strength * 0.5})
        self.lamps[#self.lamps + 1] = {links = links, shade = shade}
    end

    self.world.onJointBreak = function(joint, info)
        self.breaks = self.breaks + 1
        local body = info.bodyB or info.bodyA
        if body then
            self.sparks:fire(body.x, body.y)
            self.labels[#self.labels + 1] = {x = body.x, y = body.y, age = 0, force = math.sqrt(info.forceX ^ 2 + info.forceY ^ 2) / weight}
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

function BreakingJoints:add(x, y, shape, material)
    local body = self.world:createBody({x = x, y = y})
    shape(body)
    if material then
        Look.dress(body, material)
        self.bodies[#self.bodies + 1] = body
    end
    return body
end

function BreakingJoints:drop(x, density)
    x = x or (self.random:chance(0.5) and self.random:range(-700, -560) or self.random:range(60, 600))
    self:add(x, -380, function(body) body:addBox(60, 60, {density = density or self.random:range(1, 6)}) end, 'iron')
end

function BreakingJoints:update(dt)
    BreakingJoints.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    self.sparks:update(dt)
    for index = #self.labels, 1, -1 do
        local label = self.labels[index]
        label.age = label.age + dt
        if label.age > BreakingJoints.labelLife then
            table.remove(self.labels, index)
        end
    end
    self:status(string.format('%d joints broke so far, each holding up to %d crate weights.', self.breaks, self.strength))
end

function BreakingJoints:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The wall, the piers and the ceiling draw in stone, the shelves rest on brackets, and the lamps hang on chains of links and shine down.
function BreakingJoints:drawScenery()
    local ground = BreakingJoints.ground
    Look.block(-760, ground - 280, 40, 560, 'stone', 0)
    for _, x in ipairs({-60, 700}) do
        Look.block(x, (ground + BreakingJoints.deck) / 2, 120, ground - BreakingJoints.deck, 'stone', 0)
    end
    Look.block(350, BreakingJoints.ceiling, 440, 20, 'darkWood', 0)
    for row = 0, 2 do
        local y = -170 + row * 140
        Look.block(-712, y + 30, 70, 10, 'iron', -1, 0.75)
    end
    local unit = Look.pixel
    for _, lamp in ipairs(self.lamps) do
        local chain = self.chain
        local count = 0
        for _, link in ipairs(lamp.links) do
            if link.valid then
                count = count + 1
                chain[count] = link
            end
        end
        for index = 1, count do
            local link = chain[index]
            local cos, sin = math.sin(link.rotation), -math.cos(link.rotation)
            Look.line(link.x - cos * 11, link.y - sin * 11, link.x + cos * 11, link.y + sin * 11, (index % 2 == 0 and 7 or 4) * unit, '#FF9AA5BE', 1)
        end
        local shade = lamp.shade
        if shade.valid then
            local cos, sin = math.cos(shade.rotation), math.sin(shade.rotation)
            graphics2d.draw(self.glow, shade.x - sin * 90, shade.y + cos * 90, {width = 240, height = 240, rotation = shade.rotation, blend = 'additive', layer = 0})
        end
    end
end

function BreakingJoints:draw(area)
    Scenery.sky(self.camera, BreakingJoints.ground)
    Scenery.ground(BreakingJoints.ground)
    self:drawScenery()
    Look.drawAll(self.bodies, {layer = 1})
    self.sparks:draw()
    for _, label in ipairs(self.labels) do
        self:label(string.format('Broke at %d crates', math.floor(label.force + 0.5)), label.x, label.y - 40 - label.age * 30, {size = 18, plate = PhysicsTest.problemPlate})
    end
    self.grab:draw()
end

return BreakingJoints
