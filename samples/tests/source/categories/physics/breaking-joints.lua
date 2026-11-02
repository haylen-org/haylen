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
local shelf = world:createJoint('weld', wall, board, {ax = -740, ay = -170})
shelf.breakForce, shelf.breakTorque = weight * 12, weight * 480
local hinge = world:createJoint('revolute', plank, nextPlank, {ax = x, ay = 110})
hinge.breakForce = weight * 12
world.onJointBreak = function(joint, info) spark(info.bodyB, info.forceX, info.forceY) end]]

BreakingJoints.labelLife = 1.6
-- A break within this distance of a label that still shows, across and up or down, joins that label.
BreakingJoints.labelReach = {360, 160}
-- The cone of light of a lamp, in the space of its bulb with the shade pointing up: the two corners at the mouth of the shade and the two where it fades out.
BreakingJoints.beamCorners = {{-34, 0}, {34, 0}, {150, 330}, {-150, 330}}
BreakingJoints.planks = 11
BreakingJoints.pitch = 64
BreakingJoints.ground = 360
BreakingJoints.deck = 110
-- The lamps hang from the arm of a gantry, whose post stands at the right end of the arm.
BreakingJoints.gantry = {left = -510, right = -170, y = -300, post = -182}
BreakingJoints.lamps = {-430, -270}
BreakingJoints.links = 5
BreakingJoints.link = 24

function BreakingJoints:enter()
    self:frame{
        code = BreakingJoints.code,
        hint = 'Drop weights on the shelves and the bridge, or drag the lamps and pull. Each joint holds up to its break force in weights of a crate, and a falling weight hits it far harder than it weighs. R or the X button rebuilds everything.',
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
    self.beam = {{x = 0, y = 0, color = '#70FFE9A0'}, {x = 0, y = 0, color = '#70FFE9A0'}, {x = 0, y = 0, color = '#00FFD070'}, {x = 0, y = 0, color = '#00FFD070'}}
    self.beamIndices = {1, 2, 3, 1, 3, 4}
    self.beamOrder = {layer = 0, blend = 'additive'}
    self:build()
    timer.after(0.8, function() self:drop(-640, 6) end, {owner = self})
    timer.after(1.3, function() self:drop(330, 6) end, {owner = self})
end

function BreakingJoints:build()
    self.world = physics2d.newWorld({owner = self})
    self.grab = Grab(self.world)
    self.bodies, self.labels, self.lamps, self.breaks = {}, {}, {}, 0
    local ground = BreakingJoints.ground
    local wall = self.world:createBody({type = 'static'})
    wall:addBox(40, 560, {offsetX = -760, offsetY = ground - 280})
    wall:addBox(8000, 60, {offsetY = ground + 30})
    wall:addBox(120, ground - BreakingJoints.deck, {offsetX = -60, offsetY = (ground + BreakingJoints.deck) / 2})
    wall:addBox(120, ground - BreakingJoints.deck, {offsetX = 700, offsetY = (ground + BreakingJoints.deck) / 2})
    local gantry = BreakingJoints.gantry
    wall:addBox(gantry.right - gantry.left, 20, {offsetX = (gantry.left + gantry.right) / 2, offsetY = gantry.y})
    wall:addBox(24, ground - gantry.y, {offsetX = gantry.post, offsetY = (gantry.y + ground) / 2})
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

    for _, x in ipairs(BreakingJoints.lamps) do
        local hook, link = wall, nil
        local links = {}
        local top, length = BreakingJoints.gantry.y + 10, BreakingJoints.link
        for index = 1, BreakingJoints.links do
            link = self:add(x, top + (index - 0.5) * length, function(body) body:addBox(6, length) end)
            self.world:createJoint('revolute', hook, link, {ax = x, ay = top + (index - 1) * length, breakForce = weight * self.strength * 0.5})
            links[index] = link
            hook = link
        end
        local bottom = top + BreakingJoints.links * length
        local shade = self:add(x, bottom + 20, function(body) body:addPolygon({{-40, 20}, {-16, -20}, {16, -20}, {40, 20}}) end, 'gold')
        self.world:createJoint('revolute', hook, shade, {ax = x, ay = bottom, breakForce = weight * self.strength * 0.5})
        self.lamps[#self.lamps + 1] = {links = links, shade = shade}
    end

    self.world.onJointBreak = function(joint, info)
        self.breaks = self.breaks + 1
        local body = info.bodyB or info.bodyA
        if body then
            self.sparks:fire(body.x, body.y)
            self:note(body.x, body.y, math.sqrt(info.forceX ^ 2 + info.forceY ^ 2) / weight)
        end
    end
end

-- Shows the force of a break where it happened. Breaks close to a label that still shows join it, such as the planks of the bridge that give way together, so the labels never pile on each other.
function BreakingJoints:note(x, y, force)
    for _, label in ipairs(self.labels) do
        if math.abs(label.x - x) < BreakingJoints.labelReach[1] and math.abs(label.y - y) < BreakingJoints.labelReach[2] then
            label.count, label.force, label.age = label.count + 1, math.max(label.force, force), 0
            return
        end
    end
    self.labels[#self.labels + 1] = {x = x, y = y, age = 0, count = 1, force = force, limit = self.strength}
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

-- The wall and the piers draw in stone, the shelves rest on brackets, and the lamps hang on chains of links from a wooden gantry.
function BreakingJoints:drawScenery()
    local ground = BreakingJoints.ground
    Look.block(-760, ground - 280, 40, 560, 'stone', 0)
    for _, x in ipairs({-60, 700}) do
        Look.block(x, (ground + BreakingJoints.deck) / 2, 120, ground - BreakingJoints.deck, 'stone', 0)
    end
    for row = 0, 2 do
        local y = -170 + row * 140
        Look.block(-712, y + 30, 70, 10, 'iron', -1, 0.75)
    end
    local gantry = BreakingJoints.gantry
    Look.block(gantry.post, (gantry.y + ground) / 2, 24, ground - gantry.y, 'darkWood', -1)
    Look.block(gantry.post, ground - 8, 90, 16, 'darkWood', -1)
    Look.block(gantry.post - 30, gantry.y + 30, 14, 80, 'darkWood', -1, -math.pi / 4)
    Look.block((gantry.left + gantry.right) / 2, gantry.y, gantry.right - gantry.left + 20, 20, 'darkWood', 0)
    for _, lamp in ipairs(self.lamps) do
        self:drawChain(lamp.links)
    end
end

function BreakingJoints:drawChain(links)
    local unit = Look.pixel
    local chain = self.chain
    local count = 0
    for _, link in ipairs(links) do
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
end

-- A lamp shines from a warm bulb in the mouth of its shade, as a cone of light that fades with distance and a glow around the bulb, both turning with the shade.
function BreakingJoints:drawLamps()
    for _, lamp in ipairs(self.lamps) do
        local shade = lamp.shade
        if shade.valid then
            local cos, sin = math.cos(shade.rotation), math.sin(shade.rotation)
            local bulbX, bulbY = shade.x - sin * 20, shade.y + cos * 20
            local beam = self.beam
            for index, corner in ipairs(BreakingJoints.beamCorners) do
                beam[index].x = bulbX + corner[1] * cos - corner[2] * sin
                beam[index].y = bulbY + corner[1] * sin + corner[2] * cos
            end
            graphics2d.drawMesh(nil, beam, self.beamIndices, self.beamOrder)
            graphics2d.draw(self.glow, bulbX, bulbY, {width = 150, height = 150, blend = 'additive', layer = 0})
            graphics2d.drawShape({bulbX - 13, bulbY - 13, 26, 26}, {kind = 'ellipse', rotation = shade.rotation, startAngle = 0, sweep = math.pi, color = '#FFFFF4C8', borderWidth = 3 * Look.pixel, borderColor = Look.ink, layer = 2})
        end
    end
end

function BreakingJoints:draw(area)
    Scenery.sky(self.camera, BreakingJoints.ground)
    Scenery.ground(BreakingJoints.ground)
    self:drawScenery()
    self:drawLamps()
    Look.drawAll(self.bodies, {layer = 1})
    self.sparks:draw()
    for _, label in ipairs(self.labels) do
        local force = math.floor(label.force + 0.5)
        local text = label.count == 1 and string.format('Broke under %d crates, limit %d', force, label.limit) or string.format('%d joints broke, under up to %d crates', label.count, force)
        self:label(text, label.x, label.y - 40 - label.age * 30, {size = 18, plate = PhysicsTest.problemPlate})
    end
    self.grab:draw()
end

return BreakingJoints
