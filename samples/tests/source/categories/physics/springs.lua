-- Things that bounce: a trampoline whose bed keeps the speed of what lands on it by its restitution, platforms on prismatic joints with a spring and jump pads whose sensors launch crates and balls.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Springs = haylen.class('Springs', PhysicsTest)

local kSpawnInterval = 0.7
local kMaxBodies = 20
local kFlashLife = 0.3
local kSprings = {{-150, 1}, {150, 1.6}}
local kPads = {{430, -420, -1150}, {640, -250, -1250}}

function Springs:enter()
    self:frame{
        hint = 'Crates and balls rain on a trampoline, two spring platforms and two jump pads. Drag them to throw them again. R or the X button clears them.',
        controls = {
            ui.toggle{id = 'rain', text = 'Drop bodies', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:drop('crate') end},
            ui.button{id = 'ball', text = 'Drop a ball', onClick = function() self:drop('ball') end},
            ui.label{text = 'Trampoline restitution', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'restitution', value = 1, min = 0.5, max = 1.1, step = 0.05, showValue = true, onChange = function(event) self.bed.restitution = event.value end},
            ui.label{text = 'Stiffness of the springs in hertz', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'hertz', value = 2.5, min = 1, max = 6, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setStiffness(event.value) end},
            ui.button{id = 'reset', text = 'Clear the bodies', onClick = function() self:clear() end},
        },
        focus = 'rain',
    }
    self.random = m.random(37)
    self.raining = true
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.springs, self.pads = {}, {}, {}
    self.bounces, self.launches, self.clock, self.next = 0, 0, 0, 'crate'

    local room = self.world:createBody({type = 'static'})
    parts.box(room, 1600, 40, {offsetY = 410})
    parts.box(room, 20, 860, {offsetX = -790})
    parts.box(room, 20, 860, {offsetX = 790})
    local trampoline = self.world:createBody({type = 'static', x = -540, y = 300})
    self.bed = parts.box(trampoline, 280, 14, {restitution = 1, friction = 0.4})
    parts.box(trampoline, 12, 90, {offsetX = -120, offsetY = 50, mask = 0})
    parts.box(trampoline, 12, 90, {offsetX = 120, offsetY = 50, mask = 0})
    parts.paint(trampoline, '#FF4DD0E1')
    trampoline.data.bed = true
    self.statics = {room, trampoline}

    for _, spec in ipairs(kSprings) do
        local base = self.world:createBody({type = 'static', x = spec[1], y = 375})
        parts.box(base, 100, 30)
        local platform = self.world:createBody({x = spec[1], y = 250})
        parts.box(platform, 160, 18, {friction = 0.8})
        parts.paint(platform, '#FFFFB74D')
        local joint = self.world:createJoint('prismatic', base, platform, {ax = spec[1], ay = 250, axisX = 0, axisY = 1, enableSpring = true, hertz = 2.5 * spec[2], dampingRatio = 0.1, enableLimit = true, lower = -40, upper = 100})
        self.statics[#self.statics + 1] = base
        self.springs[#self.springs + 1] = {platform = platform, joint = joint, factor = spec[2]}
    end

    for _, spec in ipairs(kPads) do
        local pad = self.world:createBody({type = 'static', x = spec[1], y = 382})
        parts.box(pad, 120, 16)
        pad:addBox(110, 30, {offsetY = -22, sensor = true})
        parts.paint(pad, '#FFBA68C8')
        pad.data.launch = {spec[2], spec[3]}
        self.pads[#self.pads + 1] = {body = pad, flash = 0}
        self.statics[#self.statics + 1] = pad
    end

    self.world.onHit = function(a, b, contact)
        if a.data.bed or b.data.bed then
            self.bounces = self.bounces + 1
        end
    end
    -- The pads are the only sensors, and each one throws what enters it along its own launch velocity.
    self.world.onSensorBegin = function(sensor, visitor, shapes)
        visitor.velocity = sensor.data.launch
        self.launches = self.launches + 1
        for _, pad in ipairs(self.pads) do
            if pad.body == sensor then
                pad.flash = kFlashLife
            end
        end
    end
end

function Springs:setStiffness(hertz)
    for _, spring in ipairs(self.springs) do
        spring.joint.hertz = hertz * spring.factor
    end
end

function Springs:drop(kind)
    local body = self.world:createBody({x = self.random:range(-700, 700), y = -400, rotation = self.random:range(-0.4, 0.4)})
    if kind == 'crate' then
        parts.box(body, 54, 54, {friction = 0.6, restitution = 0.1})
    else
        parts.circle(body, 26, {restitution = 0.5})
    end
    self.bodies[#self.bodies + 1] = body
    if #self.bodies > kMaxBodies then
        table.remove(self.bodies, 1):destroy()
    end
end

function Springs:clear()
    for _, body in ipairs(self.bodies) do
        body:destroy()
    end
    self.bodies = {}
end

function Springs:update(dt)
    Springs.super.update(self, dt)
    if input.pressed('reset') then
        self:clear()
    end
    self.grab:update(self.pointer, self.camera)
    for _, pad in ipairs(self.pads) do
        pad.flash = math.max(0, pad.flash - dt)
    end
    local first, second = self.springs[1].joint.translation, self.springs[2].joint.translation
    self:status(string.format('Bodies %d, trampoline bounces %d, pad launches %d, springs pressed %.0f and %.0f units, step %.2f ms', #self.bodies, self.bounces, self.launches, first, second, self:stepTime()))
end

function Springs:fixedUpdate(step)
    self.clock = self.clock + step
    if self.raining and self.clock >= kSpawnInterval then
        self.clock = 0
        self:drop(self.next)
        self.next = self.next == 'crate' and 'ball' or 'crate'
    end
    self:simulate(self.world, step)
end

-- Draws a coil between the base and the platform, which squeezes as the platform comes down.
function Springs:drawCoil(spring)
    local platform = spring.platform
    local top, bottom = platform.y + 9, 360
    local points = {}
    for index = 0, 12 do
        local y = top + (bottom - top) * index / 12
        local offset = (index == 0 or index == 12) and 0 or (index % 2 == 0 and -26 or 26)
        points[#points + 1] = {platform.x + offset, y}
    end
    graphics2d.drawPolyline(points, 5, '#FFB0BEC5', false, {layer = -1})
end

function Springs:draw(area)
    parts.drawAll(self.statics)
    for _, spring in ipairs(self.springs) do
        self:drawCoil(spring)
        parts.draw(spring.platform)
    end
    for _, pad in ipairs(self.pads) do
        local body = pad.body
        graphics2d.drawPolygon({{body.x - 20, body.y - 12}, {body.x + 20, body.y - 12}, {body.x, body.y - 36}}, '#AAFFFFFF', {layer = 1})
        if pad.flash > 0 then
            graphics2d.drawCircle(body.x, body.y - 30, 70 * (1 - pad.flash / kFlashLife) + 20, m.color(0.8, 0.5, 1, pad.flash / kFlashLife), {layer = 1, blend = 'additive'})
        end
    end
    parts.drawAll(self.bodies, {layer = 2})
    self.grab:draw()
end

return Springs
