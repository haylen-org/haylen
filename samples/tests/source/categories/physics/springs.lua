-- Things that bounce: a trampoline whose bed keeps the speed of what lands on it by its restitution, platforms on prismatic joints with a spring and jump pads whose sensors launch crates and balls.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Springs = haylen.class('Springs', PhysicsTest)

Springs.code = [[
trampoline:addBox(280, 14, {restitution = 1, friction = 0.4})  -- Keeps the speed of what lands on it.
local spring = world:createJoint('prismatic', base, platform, {ax = -150, ay = 210, axisY = 1, enableSpring = true, hertz = 2.5, enableLimit = true, lower = -40, upper = 100})
pad:addBox(110, 30, {offsetY = -22, sensor = true})
world.onSensorBegin = function(sensor, visitor) visitor.velocity = {-420, -1150} end  -- A jump pad.
spring.hertz = 4  -- Stiffens the spring while it runs.]]

Springs.ground = 360
Springs.spawnInterval = 0.7
Springs.maxBodies = 20
Springs.flashLife = 0.35
Springs.springs = {{-150, 1}, {150, 1.6}}
Springs.pads = {{430, -420, -1150}, {640, -250, -1250}}
Springs.trampoline = {-540, 250}

function Springs:enter()
    self:frame{
        code = Springs.code,
        hint = 'Crates and balls rain on a trampoline, two spring platforms and two jump pads. Drag them to throw them again. R or the X button clears them.',
        controls = {
            ui.toggle{id = 'rain', text = 'Drop bodies', checked = true, onChange = function(event) self.raining = event.checked end},
            ui.button{id = 'crate', text = 'Drop a crate', onClick = function() self:drop('crate') end},
            ui.button{id = 'ball', text = 'Drop a ball', onClick = function() self:drop('ball') end},
            ui.label{text = 'Bounciness of the trampoline', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'restitution', value = 1, min = 0.5, max = 1.1, step = 0.05, showValue = true, onChange = function(event) self.bed.restitution = event.value end},
            ui.label{text = 'Stiffness of the springs', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'hertz', value = 2.5, min = 1, max = 6, step = 0.5, showValue = true, decimals = 1, onChange = function(event) self:setStiffness(event.value) end},
            ui.button{id = 'reset', text = 'Clear the bodies', onClick = function() self:clear() end},
        },
        focus = 'rain',
    }
    self.random = m.random(37)
    self.raining = true
    self.boosts = Burst('movement/spring-bounce', {count = 4, scale = 0.6, layer = 30})
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.springs, self.pads = {}, {}, {}
    self.bounces, self.launches, self.clock, self.next = 0, 0, 0, 'crate'

    local ground = Springs.ground
    local room = self.world:createBody({type = 'static'})
    room:addBox(8000, 60, {offsetY = ground + 30})
    room:addBox(24, 760, {offsetX = -790, offsetY = ground - 380})
    room:addBox(24, 760, {offsetX = 790, offsetY = ground - 380})
    local spot = Springs.trampoline
    local trampoline = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
    self.bed = trampoline:addBox(280, 14, {restitution = 1, friction = 0.4})
    trampoline.data = {bed = true}

    for _, spec in ipairs(Springs.springs) do
        local base = self.world:createBody({type = 'static', x = spec[1], y = ground - 15})
        base:addBox(100, 30)
        Look.dress(base, 'iron')
        local platform = self.world:createBody({x = spec[1], y = ground - 150})
        platform:addBox(160, 20, {friction = 0.8})
        Look.dress(platform, 'steel')
        local joint = self.world:createJoint('prismatic', base, platform, {ax = spec[1], ay = ground - 150, axisX = 0, axisY = 1, enableSpring = true, hertz = 2.5 * spec[2], dampingRatio = 0.1, enableLimit = true, lower = -40, upper = 100})
        self.springs[#self.springs + 1] = {base = base, platform = platform, joint = joint, factor = spec[2]}
    end

    for _, spec in ipairs(Springs.pads) do
        local pad = self.world:createBody({type = 'static', x = spec[1], y = ground - 8})
        pad:addBox(120, 16)
        pad:addBox(110, 30, {offsetY = -22, sensor = true})
        pad.data = {launch = {spec[2], spec[3]}}
        self.pads[#self.pads + 1] = {body = pad, flash = 0}
    end

    self.world.onHit = function(a, b, contact)
        if a.data and a.data.bed or b.data and b.data.bed then
            self.bounces = self.bounces + 1
        end
    end
    -- The pads are the only sensors, and each one throws what enters it along its own launch velocity.
    self.world.onSensorBegin = function(sensor, visitor, shapes)
        visitor.velocity = sensor.data.launch
        self.launches = self.launches + 1
        for _, pad in ipairs(self.pads) do
            if pad.body == sensor then
                pad.flash = Springs.flashLife
                self.boosts:fire(pad.body.x, pad.body.y - 20)
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
    local body = self.world:createBody({x = self.random:range(-700, 700), y = -420, rotation = self.random:range(-0.4, 0.4)})
    if kind == 'crate' then
        body:addBox(54, 54, {friction = 0.6, restitution = 0.1})
        Look.dress(body, 'woodCrate')
    else
        body:addCircle(26, {restitution = 0.5})
        Look.dress(body, self.random:chance(0.5) and 'red' or 'blue', {spin = true})
    end
    self.bodies[#self.bodies + 1] = body
    if #self.bodies > Springs.maxBodies then
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
    self.boosts:update(dt)
    for _, pad in ipairs(self.pads) do
        pad.flash = math.max(0, pad.flash - dt)
    end
    local first, second = self.springs[1].joint.translation, self.springs[2].joint.translation
    self:status(string.format('%d bounces on the trampoline and %d launches from the pads. The springs sink %d and %d units under their loads.', self.bounces, self.launches, math.floor(math.max(0, first) + 0.5), math.floor(math.max(0, second) + 0.5)))
end

function Springs:fixedUpdate(step)
    self.clock = self.clock + step
    if self.raining and self.clock >= Springs.spawnInterval then
        self.clock = 0
        self:drop(self.next)
        self.next = self.next == 'crate' and 'ball' or 'crate'
    end
    self:simulate(self.world, step)
end

-- The trampoline is a round frame on legs with a dark bed held by springs along its edge.
function Springs:drawTrampoline()
    local x, y = Springs.trampoline[1], Springs.trampoline[2]
    local unit = Look.pixel
    for _, side in ipairs({-1, 1}) do
        Look.block(x + side * 118, (y + Springs.ground) / 2 + 4, 14, Springs.ground - y - 8, 'steel', 0, side * 0.12)
    end
    Look.block(x, y + 6, 300, 16, 'steel', 1)
    graphics2d.drawShape({x - 134, y - 9, 268, 14}, {radius = 7, color = '#FF2F3352', borderWidth = 3 * unit, borderColor = Look.ink, layer = 2})
    for spring = -5, 5 do
        local sx = x + spring * 24
        Look.line(sx - 6, y + 4, sx + 6, y + 4, 2 * unit, '#FFE9F2FB', 2)
    end
end

-- A jump pad is a plate with chevrons that glow when it throws something up.
function Springs:drawPad(pad)
    local body = pad.body
    local unit = Look.pixel
    Look.block(body.x, body.y, 120, 16, 'iron', 1)
    local glow = pad.flash / Springs.flashLife
    local color = {1, 0.79 + glow * 0.2, 0.24 + glow * 0.6, 1}
    for index = 0, 2 do
        local y = body.y - 18 - index * 16
        Look.line(body.x - 18, y + 10, body.x, y, 6 * unit, color, 2)
        Look.line(body.x, y, body.x + 18, y + 10, 6 * unit, color, 2)
    end
end

function Springs:draw(area)
    Scenery.sky(self.camera, Springs.ground)
    Scenery.ground(Springs.ground)
    for _, x in ipairs({-790, 790}) do
        Look.block(x, Springs.ground - 380, 24, 760, 'stone', 0)
    end
    self:drawTrampoline()
    for _, spring in ipairs(self.springs) do
        local platform = spring.platform
        Look.spring(platform.x, Springs.ground - 30, platform.x, platform.y + 10, 46, 6, '#FFA9BCD3', 0)
        Look.draw(spring.base, {layer = 1})
        Look.draw(platform, {layer = 1})
    end
    for _, pad in ipairs(self.pads) do
        self:drawPad(pad)
    end
    Look.drawAll(self.bodies, {layer = 3})
    self.boosts:draw()
    self.grab:draw()
end

return Springs
