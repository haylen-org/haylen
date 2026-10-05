-- Traps that throw ragdolls and crates around: spinning blades that are kinematic bodies turned with `body:moveTo`, a crusher on a prismatic joint whose motor slams it down and lifts it again, and a boulder rolling down a slope.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Traps = haylen.class('Traps', PhysicsTest)

local kBlades = {{x = -60, y = 380, radius = 80, speed = 6}, {x = 180, y = 20, radius = 70, speed = -4}}
local kCrusher = {x = 500, top = -100, depth = 430, down = 700, up = 160, slam = 1, cycle = 3}
local kBoulderStart = {-730, -140}
local kBoulderTime = 8
local kHardHit = 500
local kMaxDolls, kMaxCrates = 6, 10
local kDollHeight = 140
local kDollColors = {'#FFFFCC80', '#FF80DEEA', '#FFCE93D8', '#FFA5D6A7'}
local kBones = {
    {'head', 'chest'}, {'chest', 'hips'},
    {'chest', 'upperArmLeft'}, {'upperArmLeft', 'lowerArmLeft'},
    {'chest', 'upperArmRight'}, {'upperArmRight', 'lowerArmRight'},
    {'hips', 'upperLegLeft'}, {'upperLegLeft', 'lowerLegLeft'},
    {'hips', 'upperLegRight'}, {'upperLegRight', 'lowerLegRight'},
}

function Traps:enter()
    self:frame{
        hint = 'Drop ragdolls and crates into the traps and drag them around. The blades turn by kinematic moves, the crusher rides a prismatic motor and the boulder comes back to the top every few seconds. R or the X button clears everything.',
        controls = {
            ui.button{id = 'ragdoll', text = 'Drop a ragdoll', onClick = function() self:dropRagdoll() end},
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:dropCrate() end},
            ui.label{text = 'Blade speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'blades', value = 1, min = 0, max = 2, step = 0.1, showValue = true, onChange = function(event) self.bladeScale = event.value end},
            ui.button{id = 'reset', text = 'Clear', onClick = function() self:build() end},
        },
        focus = 'ragdoll',
    }
    self.random = m.random(62)
    self.bladeScale = 1
    self:build()
end

function Traps:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.dolls, self.crates, self.groups, self.hardHits, self.boulderTime, self.crushTime = {}, {}, 0, 0, 0, 0

    local ground = self.world:createBody({type = 'static'})
    parts.box(ground, 1600, 40, {offsetY = 410})
    parts.box(ground, 20, 860, {offsetX = -790})
    parts.box(ground, 20, 860, {offsetX = 790})
    parts.polygon(ground, {{-780, -60}, {-300, 390}, {-780, 390}})
    parts.box(ground, 240, 20, {offsetX = kCrusher.x, offsetY = kCrusher.top - 290})
    self.statics = {ground}

    self.blades = {}
    for _, spec in ipairs(kBlades) do
        local blade = self.world:createBody({type = 'kinematic', x = spec.x, y = spec.y})
        parts.circle(blade, spec.radius * 0.5)
        for tooth = 0, 5 do
            local angle = tooth * math.pi / 3
            parts.polygon(blade, {{math.cos(angle - 0.3) * spec.radius * 0.45, math.sin(angle - 0.3) * spec.radius * 0.45}, {math.cos(angle) * spec.radius, math.sin(angle) * spec.radius}, {math.cos(angle + 0.3) * spec.radius * 0.45, math.sin(angle + 0.3) * spec.radius * 0.45}})
        end
        parts.paint(blade, '#FFB0BEC5')
        self.blades[#self.blades + 1] = {body = blade, spec = spec}
    end

    self.crusher = self.world:createBody({x = kCrusher.x, y = kCrusher.top})
    parts.box(self.crusher, 160, 60, {density = 5})
    parts.paint(self.crusher, '#FF78909C')
    self.piston = self.world:createJoint('prismatic', ground, self.crusher, {ax = kCrusher.x, ay = kCrusher.top, axisX = 0, axisY = 1, enableLimit = true, lower = 0, upper = kCrusher.depth, enableMotor = true, motorSpeed = kCrusher.down, maxMotorForce = 4e4})

    self.boulder = self.world:createBody({x = kBoulderStart[1], y = kBoulderStart[2]})
    parts.circle(self.boulder, 50, {density = 3, friction = 0.8})
    parts.paint(self.boulder, '#FF8D6E63')

    self.world.onHit = function(a, b, contact)
        if contact.speed > kHardHit then
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
    local doll = physics2d.newRagdoll(self.world, {x = x or self.random:range(-500, 650), y = -330, height = kDollHeight, group = -self.groups, stiffness = 0.3})
    self.dolls[#self.dolls + 1] = {doll = doll, color = kDollColors[self.groups % #kDollColors + 1]}
    if #self.dolls > kMaxDolls then
        table.remove(self.dolls, 1).doll:destroy()
    end
end

function Traps:dropCrate()
    local crate = self.world:createBody({x = self.random:range(-500, 650), y = -330, rotation = self.random:range(-0.5, 0.5)})
    parts.box(crate, 56, 56)
    parts.paint(crate, '#FFBC8F5A')
    self.crates[#self.crates + 1] = crate
    if #self.crates > kMaxCrates then
        table.remove(self.crates, 1):destroy()
    end
end

function Traps:update(dt)
    Traps.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local velocity = self.boulder.velocity
    self:status(string.format('Ragdolls %d, crates %d, blades at %.1f radians per second, crusher down %.0f, boulder %.0f units per second, hard hits %d, step %.2f ms', #self.dolls, #self.crates, kBlades[1].speed * self.bladeScale, self.piston.translation, math.sqrt(velocity.x ^ 2 + velocity.y ^ 2), self.hardHits, self:stepTime()))
end

-- The blades turn by a kinematic move each step, the crusher goes back up at the end of its travel or when something stops it, and the boulder starts again at the top after a while.
function Traps:fixedUpdate(step)
    for _, blade in ipairs(self.blades) do
        local body = blade.body
        body:moveTo(blade.spec.x, blade.spec.y, body.rotation + blade.spec.speed * self.bladeScale * step)
    end
    local travel = self.piston.translation
    local falling = self.piston.motorSpeed > 0
    self.crushTime = self.crushTime + step
    if falling and (travel >= kCrusher.depth - 2 or self.crushTime > kCrusher.slam) then
        self.piston.motorSpeed, self.crushTime = -kCrusher.up, 0
    elseif not falling and travel <= 2 and self.crushTime > kCrusher.cycle then
        self.piston.motorSpeed, self.crushTime = kCrusher.down, 0
    end
    self.boulderTime = self.boulderTime + step
    if self.boulderTime > kBoulderTime then
        self.boulderTime = 0
        self.boulder:setTransform(kBoulderStart[1], kBoulderStart[2])
        self.boulder.velocity = {0, 0}
        self.boulder.angularVelocity = 0
    end
    self:simulate(self.world, step)
end

function Traps:drawDoll(doll, color)
    local bodies = doll:bodies()
    local order = {layer = 1}
    local thickness = kDollHeight / 14
    for _, bone in ipairs(kBones) do
        local a, b = bodies[bone[1]], bodies[bone[2]]
        graphics2d.drawLine(a.x, a.y, b.x, b.y, thickness, color, order)
        graphics2d.drawCircle(b.x, b.y, thickness / 2, color, order)
    end
    graphics2d.drawCircle(bodies.head.x, bodies.head.y, kDollHeight / 11, color, {layer = 2})
end

function Traps:draw(area)
    parts.drawAll(self.statics)
    graphics2d.drawLine(kBlades[2].x, -430, kBlades[2].x, kBlades[2].y, 10, '#FF546E7A', {layer = -1})
    graphics2d.drawLine(kCrusher.x, kCrusher.top - 290, self.crusher.x, self.crusher.y, 18, '#FF546E7A', {layer = -1})
    for _, blade in ipairs(self.blades) do
        parts.draw(blade.body, {layer = 3})
    end
    parts.draw(self.crusher, {layer = 3})
    parts.draw(self.boulder, {layer = 2})
    parts.drawAll(self.crates, {layer = 1})
    for _, entry in ipairs(self.dolls) do
        self:drawDoll(entry.doll, entry.color)
    end
    self.grab:draw()
end

return Traps
