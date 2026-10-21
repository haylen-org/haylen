-- A moon lander with two thrusters that push with `body:applyForce` at points off its center, so one thruster turns it and both lift it, with fuel, a landing speed limit and a sensor over the landing pad. An autopilot flies it to the pad until the player fires a thruster.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Thrusters = haylen.class('Thrusters', PhysicsTest)

Thrusters.code = [[
local lander = world:createBody({x = -560, y = -300, angularDamping = 4})
lander:applyForce(math.sin(lander.rotation) * force, -math.cos(lander.rotation) * force, nozzleX, nozzleY)  -- Off center, so one thruster also turns it.
pad:addBox(200, 60, {offsetY = -38, sensor = true})
world.onSensorBegin = function(sensor, visitor) if visitor == lander then over = over + 1 end end
world.onHit = function(a, b, contact) if contact.speed > 150 then crashed = true end end]]

Thrusters.actions = {
    {name = 'leftThruster', type = 'button', bindings = {'key:a', 'key:left', 'axis:leftTrigger+', 'button:dpadLeft', 'virtual:leftThruster'}},
    {name = 'rightThruster', type = 'button', bindings = {'key:d', 'key:right', 'axis:rightTrigger+', 'button:dpadRight', 'virtual:rightThruster'}},
    {name = 'bothThrusters', type = 'button', bindings = {'key:w', 'key:up', 'key:space', 'button:south', 'virtual:bothThrusters'}},
}

Thrusters.gravity = 300
Thrusters.start = {-560, -260}
Thrusters.nozzles = {{-18, 22}, {18, 22}}
Thrusters.fuelTank = 100
Thrusters.burn = 6
Thrusters.crashSpeed = 150
Thrusters.landSpeed = 12
Thrusters.landTime = 1
Thrusters.pad = {430, 242, 200}
Thrusters.terrain = {{-800, -430}, {-800, 200}, {-650, 260}, {-500, 180}, {-380, 300}, {-200, 330}, {-50, 220}, {100, 290}, {250, 250}, {330, 250}, {530, 250}, {600, 150}, {700, 220}, {800, 120}, {800, -430}}
Thrusters.hull = {{-34, -50}, {34, -50}, {44, 10}, {-44, 10}}
-- The picture of the lander, its size and how far its middle sits above the center of the body, so its feet stand where the legs of the body end.
Thrusters.picture = {path = 'physics/lander.png', width = 118, height = 110, lift = 6}
-- The autopilot cruises at this height and speed, and settles onto the pad at this speed.
Thrusters.cruise = {height = -60, speed = 200, descent = 60}
-- The craters of the moon ground, by their middle and their half width and height.
Thrusters.craters = {{-600, 300, 50, 13}, {-250, 365, 70, 18}, {80, 345, 40, 11}, {650, 262, 46, 12}, {-720, 270, 30, 8}, {300, 330, 34, 9}}
Thrusters.moon = Look.tint('stone', '#FFA59BC8', '#FF7A6FA3', '#FFD3CCEA')

function Thrusters:enter()
    self:frame{
        code = Thrusters.code,
        hint = 'A, the left arrow or the left trigger fires the left thruster, which turns the lander right, D, the right arrow or the right trigger fires the right one, and W, Space or the south button fires both. Land slowly and upright on the pad.',
        controls = {
            ui.label{text = 'Thruster acceleration', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'thrust', value = 300, min = 200, max = 500, step = 10, showValue = true, decimals = 0, onChange = function(event) self.thrust = event.value end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = Thrusters.actions,
        overlay = {
            ui.touchButton{action = 'leftThruster', text = 'Left', size = 140, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}},
            ui.touchButton{action = 'bothThrusters', text = 'Both', size = 140, touchOnly = true, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 184}},
            ui.touchButton{action = 'rightThruster', text = 'Right', size = 140, touchOnly = true, anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}},
        },
    }
    self.thrust = 300
    self.random = m.random(56)
    self.picture = assets.texture(Thrusters.picture.path, {filter = 'linear'})
    self.earth = assets.texture('physics/planet-grass.png', {filter = 'linear'})
    local fill = {{-1600, 200}}
    for index = 2, #Thrusters.terrain - 1 do
        fill[#fill + 1] = Thrusters.terrain[index]
    end
    fill[#fill + 1] = {1600, 120}
    fill[#fill + 1] = {1600, 1200}
    fill[#fill + 1] = {-1600, 1200}
    self.ground = Look.newShape(fill, Thrusters.moon)
    self.flame = {kind = 'ellipse', rotation = 0, color = '#FFFFFFFF', layer = 2}
    self.flameRect = {0, 0, 0, 0}
    self:build()
end

function Thrusters:build()
    self.world = physics2d.newWorld({gravity = {0, Thrusters.gravity}})
    self.fuel, self.state, self.onPad, self.still, self.touchdown = Thrusters.fuelTank, 'flying', 0, 0, 0
    self.throttles = {0, 0}
    self.autopilot = true

    local ground = self.world:createBody({type = 'static'})
    ground:addChain(Thrusters.terrain, false, {friction = 0.8})
    local pad = Thrusters.pad
    self.padBody = self.world:createBody({type = 'static', x = pad[1], y = pad[2]})
    self.padBody:addBox(pad[3], 16, {friction = 0.9})
    self.padBody:addBox(pad[3], 60, {offsetY = -38, sensor = true})

    local start = Thrusters.start
    self.lander = self.world:createBody({x = start[1], y = start[2], angularDamping = 4})
    self.lander:addPolygon(Thrusters.hull)
    self.lander:addCapsule(-30, 10, -50, 40, 5)
    self.lander:addCapsule(30, 10, 50, 40, 5)

    -- Every shape of the lander enters and leaves the sensor on its own, so a count tells whether any part is over the pad.
    self.world.onSensorBegin = function(sensor, visitor)
        if visitor == self.lander then
            self.onPad = self.onPad + 1
        end
    end
    self.world.onSensorEnd = function(sensor, visitor)
        if visitor == self.lander then
            self.onPad = self.onPad - 1
        end
    end
    self.world.onHit = function(a, b, contact)
        if (a == self.lander or b == self.lander) and self.state ~= 'crashed' then
            self.touchdown = math.max(self.touchdown, contact.speed)
            if contact.speed > Thrusters.crashSpeed then
                self.state = 'crashed'
            end
        end
    end
end

function Thrusters:exit()
    Thrusters.super.exit(self)
    input.clearVirtual()
end

-- The autopilot holds the cruise height while it tilts toward the pad, then sinks onto it slowly and upright, and cuts the thrusters once the lander has landed.
function Thrusters:steer()
    local lander, pad = self.lander, Thrusters.pad
    if self.state ~= 'flying' then
        return 0, 0
    end
    local velocity = lander.velocity
    local cruise = Thrusters.cruise
    local offset = pad[1] - lander.x
    local wantedX = m.clamp(offset * 0.5, -cruise.speed, cruise.speed)
    local tilt = m.clamp((wantedX - velocity.x) * 0.005, -0.4, 0.4)
    local wantedY
    if math.abs(offset) > 40 or math.abs(velocity.x) > 30 then
        wantedY = m.clamp((cruise.height - lander.y) * 1.2, -150, 150)
    else
        wantedY = m.clamp((pad[2] - 60 - lander.y) * 0.8, 20, cruise.descent)
        tilt = m.clamp(tilt, -0.15, 0.15)
    end
    local lift = (Thrusters.gravity - (wantedY - velocity.y) * 3) / math.max(0.5, math.cos(lander.rotation))
    local base = lift / (2 * self.thrust)
    local turn = (tilt - lander.rotation) * 4 - lander.angularVelocity * 1.2
    return m.clamp(base + turn, 0, 1), m.clamp(base - turn, 0, 1)
end

function Thrusters:update(dt)
    Thrusters.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local both = input.value('bothThrusters')
    local left, right = math.max(input.value('leftThruster'), both), math.max(input.value('rightThruster'), both)
    if left > 0 or right > 0 then
        self.autopilot = false
    end
    if self.autopilot then
        left, right = self:steer()
    end
    local active = self.state ~= 'crashed' and self.fuel > 0
    self.throttles[1] = active and left or 0
    self.throttles[2] = active and right or 0

    local speed = math.floor(self.lander.velocity:length() + 0.5)
    local fuel = math.floor(self.fuel / Thrusters.fuelTank * 100 + 0.5)
    if self.state == 'crashed' then
        self:status(string.format('The lander crashed at %d units per second, faster than the safe %d. Start over to fly again.', math.floor(self.touchdown + 0.5), Thrusters.crashSpeed))
    elseif self.state == 'landed' then
        self:status(string.format('The lander stands on the pad, set down at %d units per second with %d percent of its fuel left.', math.floor(self.touchdown + 0.5), fuel))
    elseif self.autopilot then
        self:status(string.format('The autopilot flies the lander to the pad at %d units per second until you fire a thruster, with %d percent of the fuel left.', speed, fuel))
    else
        self:status(string.format('The lander flies at %d units per second, tilted %d degrees, with %d percent of the fuel left.', speed, PhysicsTest.degrees(self.lander.rotation), fuel))
    end
end

-- Each thruster pushes along the up direction of the lander at its own point, so a single one also turns it.
function Thrusters:fixedUpdate(step)
    local lander = self.lander
    local cos, sin = math.cos(lander.rotation), math.sin(lander.rotation)
    for index, point in ipairs(Thrusters.nozzles) do
        local throttle = self.throttles[index]
        if throttle > 0 then
            local force = lander.mass * self.thrust * throttle
            lander:applyForce(sin * force, -cos * force, lander.x + point[1] * cos - point[2] * sin, lander.y + point[1] * sin + point[2] * cos)
            self.fuel = math.max(0, self.fuel - throttle * Thrusters.burn * step)
        end
    end
    self:simulate(self.world, step)

    local velocity = lander.velocity
    local resting = velocity:length() < Thrusters.landSpeed and math.abs(lander.rotation) < 0.25
    self.still = resting and self.onPad > 0 and self.still + step or 0
    if self.state == 'flying' and self.still > Thrusters.landTime then
        self.state = 'landed'
    elseif self.state == 'landed' and self.onPad == 0 then
        self.state = 'flying'
    end
end

-- The pad is a steel deck on two legs with lamps at its ends, which blink while the lander flies and glow green once it has landed.
function Thrusters:drawPad()
    local pad = Thrusters.pad
    local x, y, width = pad[1], pad[2], pad[3]
    for _, side in ipairs({-1, 1}) do
        Look.block(x + side * (width / 2 - 24), y + 30, 14, 50, 'iron', 0)
    end
    Look.block(x, y, width, 16, 'steel', 1)
    local on = self.state == 'landed' or math.sin(haylen.elapsed() * 6) > 0
    local lamp = on and (self.state == 'landed' and 'green' or 'yellow') or 'slate'
    for _, side in ipairs({-1, 1}) do
        Look.disc(x + side * (width / 2 - 10), y - 14, 7, lamp, 2)
    end
end

-- The lander is its painted picture, with a flickering flame under every nozzle that fires.
function Thrusters:drawLander()
    local lander, picture = self.lander, Thrusters.picture
    local cos, sin = math.cos(lander.rotation), math.sin(lander.rotation)
    local color = self.state == 'crashed' and '#FFFF9A92' or '#FFFFFFFF'
    graphics2d.draw(self.picture, lander.x + picture.lift * sin, lander.y - picture.lift * cos, {width = picture.width, height = picture.height, rotation = lander.rotation, color = color, layer = 3})
    local shape, rect = self.flame, self.flameRect
    for index, point in ipairs(Thrusters.nozzles) do
        local throttle = self.throttles[index]
        if throttle > 0.02 then
            local length = (26 + self.random:range(0, 12)) * throttle + 8
            local x, y = lander.x + point[1] * cos - point[2] * sin, lander.y + point[1] * sin + point[2] * cos
            local cx, cy = x - sin * length / 2, y + cos * length / 2
            shape.rotation = lander.rotation
            rect[1], rect[2], rect[3], rect[4] = cx - 9, cy - length / 2, 18, length
            shape.color = '#E6FF9A3C'
            graphics2d.drawShape(rect, shape)
            rect[1], rect[2], rect[3], rect[4] = cx - 5, cy - length / 2 + 2, 10, length * 0.6
            shape.color = '#FFFFF0A0'
            graphics2d.drawShape(rect, shape)
        end
    end
end

-- The fuel gauge is a plate in the corner with a bar that turns red when the tank runs low.
function Thrusters:drawFuel()
    local unit = Look.pixel
    local share = self.fuel / Thrusters.fuelTank
    graphics2d.drawShape({-780, -370, 300, 76}, {radius = 14, color = '#EE2F3352', borderWidth = 2 * unit, borderColor = '#FF15172A', layer = 40})
    self:tag(string.format('Fuel %d%%', math.floor(share * 100 + 0.5)), -630, -346, {size = 22, layer = 41})
    graphics2d.drawShape({-764, -326, 268, 18}, {radius = 9, color = '#FF15172A', layer = 41})
    if share > 0 then
        graphics2d.drawShape({-764, -326, 268 * share, 18}, {radius = 9, color = share > 0.2 and '#FF7BC74D' or '#FFF0473E', layer = 41})
    end
end

function Thrusters:draw(area)
    Scenery.space()
    graphics2d.draw(self.earth, 560, -250, {width = 150, height = 150, layer = -800})
    Look.drawShape(self.ground, -10)
    for _, crater in ipairs(Thrusters.craters) do
        local x, y, width, height = crater[1], crater[2], crater[3], crater[4]
        graphics2d.drawShape({x - width, y - height, width * 2, height * 2}, {kind = 'ellipse', color = '#FF7A6FA3', layer = -9})
        graphics2d.drawShape({x - width * 0.8, y - height * 0.2, width * 1.6, height * 1.1}, {kind = 'ellipse', color = '#FFD3CCEA', layer = -9})
        graphics2d.drawShape({x - width * 0.85, y - height * 0.75, width * 1.7, height * 1.5}, {kind = 'ellipse', color = '#FF8C82B5', layer = -9})
    end
    self:drawPad()
    self:drawLander()
    self:drawFuel()
end

return Thrusters
