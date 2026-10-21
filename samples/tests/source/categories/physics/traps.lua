-- Traps that throw ragdolls and crates around: saw blades that are kinematic bodies turned with `body:moveTo`, one rising from a slot in the floor and one hanging from the girder, a crusher on a prismatic joint whose motor slams it down and lifts it again, and a boulder rolling down a hill.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Figure = require('categories.physics.figure')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Traps = haylen.class('Traps', PhysicsTest)

Traps.code = [[
blade:moveTo(-60, 380, blade.rotation + 6 * step)  -- A kinematic blade turned each fixed step.
local piston = world:createJoint('prismatic', ground, crusher, {ax = 500, ay = -100, axisX = 0, axisY = 1, enableLimit = true, lower = 0, upper = 430, enableMotor = true, motorSpeed = 700, maxMotorForce = 4e4})
piston.motorSpeed = -160  -- Lifts the crusher again.
physics2d.newRagdoll(world, {x = 20, y = -330, height = 140, group = -1, stiffness = 0.3})
boulder:addCircle(50, {density = 3, friction = 0.8})]]

Traps.ground = 390
Traps.girder = -390
Traps.blades = {{x = -60, y = 380, radius = 80, speed = 6}, {x = 180, y = 20, radius = 70, speed = -4}}
Traps.teeth = 6
Traps.crusherPath = {x = 500, top = -100, depth = 430, down = 700, up = 160, slam = 1, cycle = 3}
Traps.boulderStart = {-730, -140}
Traps.boulderTime = 8
Traps.hardHit = 500
Traps.maxDolls, Traps.maxCrates = 6, 10
Traps.dollHeight = 140
Traps.hill = {{-1600, -60}, {-780, -60}, {-300, 390}, {1600, 390}}
Traps.saw = Look.tint('steel', '#FFC9D3E2', '#FF8090A8', '#FFF2F6FB')

function Traps:enter()
    self:frame{
        code = Traps.code,
        hint = 'Drop ragdolls and crates into the traps and drag them around. The blades turn by kinematic moves, the crusher rides a prismatic motor and the boulder comes back to the top every few seconds. R or the X button clears everything.',
        controls = {
            ui.button{id = 'ragdoll', text = 'Drop a ragdoll', onClick = function() self:dropRagdoll() end},
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:dropCrate() end},
            ui.label{text = 'Blade speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'blades', value = 1, min = 0, max = 2, step = 0.1, showValue = true, onChange = function(event) self.bladeScale = event.value end},
            ui.button{id = 'reset', text = 'Clear', onClick = function() self:build() end},
        },
        focus = 'ragdoll',
        view = {1700, 840},
    }
    self.random = m.random(62)
    self.bladeScale = 1
    self.terrain = Scenery.newTerrain(Traps.hill)
    self.sawPoints, self.sawLit, self.sawOutline = {}, {}, {}
    for index = 1, Traps.teeth * 3 do
        self.sawPoints[index], self.sawLit[index] = {0, 0}, {0, 0}
        self.sawOutline[index * 2 - 1], self.sawOutline[index * 2] = 0, 0
    end
    self.sawStroke = {width = 1, color = Look.ink, join = 'round', closed = true, layer = 0}
    self.sawOrder = {layer = 0}
    self:build()
end

function Traps:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.dolls, self.crates, self.groups, self.hardHits, self.boulderTime, self.crushTime = {}, {}, 0, 0, 0, 0

    local ground = self.world:createBody({type = 'static'})
    ground:addBox(4000, 40, {offsetY = Traps.ground + 20})
    ground:addBox(20, 1000, {offsetX = -790})
    ground:addBox(20, 1000, {offsetX = 790})
    ground:addPolygon({{-780, -60}, {-300, 390}, {-780, 390}})
    ground:addBox(1580, 30, {offsetY = Traps.girder})

    self.saws = {}
    for _, spec in ipairs(Traps.blades) do
        local blade = self.world:createBody({type = 'kinematic', x = spec.x, y = spec.y})
        blade:addCircle(spec.radius * 0.5)
        for tooth = 0, Traps.teeth - 1 do
            local angle = tooth * math.pi * 2 / Traps.teeth
            blade:addPolygon({{math.cos(angle - 0.3) * spec.radius * 0.45, math.sin(angle - 0.3) * spec.radius * 0.45}, {math.cos(angle) * spec.radius, math.sin(angle) * spec.radius}, {math.cos(angle + 0.3) * spec.radius * 0.45, math.sin(angle + 0.3) * spec.radius * 0.45}})
        end
        self.saws[#self.saws + 1] = {body = blade, spec = spec}
    end

    local path = Traps.crusherPath
    self.crusher = self.world:createBody({x = path.x, y = path.top})
    self.crusher:addBox(160, 60, {density = 5})
    self.piston = self.world:createJoint('prismatic', ground, self.crusher, {ax = path.x, ay = path.top, axisX = 0, axisY = 1, enableLimit = true, lower = 0, upper = path.depth, enableMotor = true, motorSpeed = path.down, maxMotorForce = 4e4})

    self.boulder = self.world:createBody({x = Traps.boulderStart[1], y = Traps.boulderStart[2]})
    self.boulder:addCircle(50, {density = 3, friction = 0.8})
    Look.dress(self.boulder, 'stone', {spin = true})

    self.world.onHit = function(a, b, contact)
        if contact.speed > Traps.hardHit then
            self.hardHits = self.hardHits + 1
        end
    end
    for _, x in ipairs({-480, 20, 380}) do
        self:dropRagdoll(x)
    end
end

-- Every ragdoll gets its own negative group, so its parts ignore each other but collide with the other ragdolls.
function Traps:dropRagdoll(x)
    self.groups = self.groups + 1
    local doll = physics2d.newRagdoll(self.world, {x = x or self.random:range(-500, 650), y = -300, height = Traps.dollHeight, group = -self.groups, stiffness = 0.3})
    self.dolls[#self.dolls + 1] = {doll = doll, outfit = Figure.dress(doll, self.groups)}
    if #self.dolls > Traps.maxDolls then
        table.remove(self.dolls, 1).doll:destroy()
    end
end

function Traps:dropCrate()
    local crate = self.world:createBody({x = self.random:range(-500, 650), y = -300, rotation = self.random:range(-0.5, 0.5)})
    crate:addBox(56, 56)
    Look.dress(crate, 'woodCrate')
    self.crates[#self.crates + 1] = crate
    if #self.crates > Traps.maxCrates then
        table.remove(self.crates, 1):destroy()
    end
end

function Traps:update(dt)
    Traps.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local crusher = self.piston.motorSpeed > 0 and 'slams down' or 'rises'
    self:status(string.format('%d ragdolls and %d crates ride the traps, the blades turn at %.1f radians a second, the crusher %s, and %d hard hits so far.', #self.dolls, #self.crates, Traps.blades[1].speed * self.bladeScale, crusher, self.hardHits))
end

-- The blades turn by a kinematic move each step, the crusher goes back up at the end of its travel or when something stops it, and the boulder starts again at the top after a while.
function Traps:fixedUpdate(step)
    for _, saw in ipairs(self.saws) do
        local body = saw.body
        body:moveTo(saw.spec.x, saw.spec.y, body.rotation + saw.spec.speed * self.bladeScale * step)
    end
    local path = Traps.crusherPath
    local travel = self.piston.translation
    local falling = self.piston.motorSpeed > 0
    self.crushTime = self.crushTime + step
    if falling and (travel >= path.depth - 2 or self.crushTime > path.slam) then
        self.piston.motorSpeed, self.crushTime = -path.up, 0
    elseif not falling and travel <= 2 and self.crushTime > path.cycle then
        self.piston.motorSpeed, self.crushTime = path.down, 0
    end
    self.boulderTime = self.boulderTime + step
    if self.boulderTime > Traps.boulderTime then
        self.boulderTime = 0
        self.boulder:setTransform(Traps.boulderStart[1], Traps.boulderStart[2])
        self.boulder.velocity = {0, 0}
        self.boulder.angularVelocity = 0
    end
    self:simulate(self.world, step)
end

-- A saw is one star of teeth in polished steel with the bold outline, its lit side toward the light and a dark hub with bolts, turning with its body.
function Traps:drawSaw(saw, layer)
    local body, spec = saw.body, saw.spec
    local radius, rotation = spec.radius, body.rotation
    local count = Traps.teeth
    for tooth = 0, count - 1 do
        local angle = rotation + tooth * math.pi * 2 / count
        for corner = 0, 2 do
            local index = tooth * 3 + corner + 1
            local a = angle + (corner - 1) * 0.42
            local reach = corner == 1 and radius or radius * 0.55
            local point, lit = self.sawPoints[index], self.sawLit[index]
            point[1], point[2] = body.x + math.cos(a) * reach, body.y + math.sin(a) * reach
            lit[1], lit[2] = body.x + math.cos(a) * reach * 0.8 - 3, body.y + math.sin(a) * reach * 0.8 - 3
            self.sawOutline[index * 2 - 1], self.sawOutline[index * 2] = point[1], point[2]
        end
    end
    local stroke, order = self.sawStroke, self.sawOrder
    stroke.width, stroke.layer, order.layer = Look.outlinePixels * Look.pixel * 2, layer, layer
    graphics2d.drawStroke(self.sawOutline, stroke)
    graphics2d.drawPolygon(self.sawPoints, Traps.saw.shade, order)
    graphics2d.drawPolygon(self.sawLit, Traps.saw.fill, order)
    Look.disc(body.x, body.y, radius * 0.28, 'iron', layer, rotation)
    Look.pin(body.x, body.y, radius * 0.1, layer)
end

-- The crusher is a heavy iron slab with yellow and black stripes on a steel piston from the girder.
function Traps:drawCrusher()
    local crusher = self.crusher
    Look.line(crusher.x, Traps.girder, crusher.x, crusher.y - 30, 24, '#FFA9BCD3', 1)
    Look.block(crusher.x, crusher.y, 160, 60, 'iron', 2)
    for stripe = -3, 3 do
        local x = crusher.x + stripe * 22
        graphics2d.drawShape({x - 6, crusher.y + 14, 12, 10}, {rotation = 0.5, color = stripe % 2 == 0 and '#FFFFC93C' or '#FF2F2B45', layer = 2})
    end
end

function Traps:draw(area)
    local bounds = graphics2d.canvasBounds()
    Scenery.sky(self.camera, Traps.ground)
    Scenery.drawTerrain(self.terrain)
    for _, x in ipairs({-800, 800}) do
        Look.block(x, (bounds.y + Traps.ground) / 2, 40, Traps.ground - bounds.y + 20, 'stone', -12)
    end
    Look.block(0, Traps.girder, 1620, 30, 'iron', 1)

    -- The floor blade turns behind the ground and shows through a slot, and the hanging blade hangs on a rod from the girder.
    local floor, hanging = self.saws[1], self.saws[2]
    self:drawSaw(floor, -11)
    graphics2d.drawShape({floor.spec.x - floor.spec.radius - 10, Traps.ground - 8, (floor.spec.radius + 10) * 2, 16}, {radius = 8, color = '#FF2F2B45', borderWidth = 3 * Look.pixel, borderColor = Look.ink, layer = -8})
    Look.line(hanging.spec.x, Traps.girder, hanging.spec.x, hanging.spec.y, 10, '#FF7088A6', 1)
    self:drawSaw(hanging, 2)
    self:drawCrusher()

    Look.draw(self.boulder, {layer = 2})
    Look.drawAll(self.crates, {layer = 1})
    for index, entry in ipairs(self.dolls) do
        Figure.draw(entry.doll, entry.outfit, 3 + index)
    end
    self.grab:draw()
end

return Traps
